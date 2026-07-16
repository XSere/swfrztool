#include "core.hpp"

BYTE ret1[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };
BYTE mov_r9[] = { 0x49, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE jmp[] = { 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0 };
WCHAR configFilePathNt[] = L"\\Device\\HarddiskVolume3\\ProgramData\\SeewoFreezeKernelConfig\\VolumeInfo.config";
WCHAR redirectFilePathNt[] = L"\\Device\\HarddiskVolume3\\ProgramData\\SeewoFreezeKernelConfig\\redirect.config";
WCHAR swfreezeDriverName[] = L"\\Driver\\SWFreeze";
WCHAR seewoKeLiteLadyDriverName[] = L"\\FileSystem\\SeewoKeLiteLady";

PDRIVER_OBJECT pSWFreezeDriverObject;
PDRIVER_OBJECT pDiskDriverObject;
PDRIVER_OBJECT pSeewoKeLiteLadyDriverObject;
ULONG64 PUnhookedReadFuncAddress = 0;
ULONG64 PUnhookedWriteFuncAddress = 0;
ULONG64 PUnhookedIoControlFuncAddress = 0;
ULONG64 HookedReadFunction = 0;
ULONG64 HookedWriteFunction = 0;
ULONG64 HookedIoControlFunction = 0;

// shellcode
__declspec(noinline) EXTERN_C FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
	PFLT_CALLBACK_DATA Data,
	PFLT_RELATED_OBJECTS FltObjects,
	PVOID* CompletionContext,
	PCALLBACK_PARAMS params
)
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION nameInfo;

	if (!params->isRedirect
		|| Data == NULL
		|| Data->Iopb == NULL
		|| Data->RequestorMode == KernelMode
		|| Data->Iopb->MajorFunction != 0x0) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	status = params->FltGetFileNameInformation(Data, 0x102, &nameInfo);
	if (!NT_SUCCESS(status)) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	params->FltParseFileNameInformation(nameInfo);
	if (nameInfo->Name.Length != sizeof(configFilePathNt) - sizeof(WCHAR)) {
		params->FltReleaseFileNameInformation(nameInfo);
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	for (ULONG i = 0; i < nameInfo->Name.Length / sizeof(WCHAR); i++) {
		if (nameInfo->Name.Buffer[i] != params->configPath[i]) {
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
	}
	params->FltReleaseFileNameInformation(nameInfo);

	ULONG createOptions = Data->Iopb->Options & 0x00FFFFFF;
	if (createOptions & FILE_SEQUENTIAL_ONLY) {
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	REPARSE_DATA_BUFFER* repBuffer = (REPARSE_DATA_BUFFER*)params->ExAllocatePool(0, sizeof(REPARSE_DATA_BUFFER) + sizeof(redirectFilePathNt));
	if (!repBuffer) {
		Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		return FLT_PREOP_COMPLETE;
	}

	for (int i = 0; i < sizeof(REPARSE_DATA_BUFFER) + sizeof(redirectFilePathNt); i++) {
		((BYTE*)repBuffer)[i] = 0;
	}

	repBuffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;
	// FIELD_OFFSET(SYMBOLIC_LINK_REPARSE_BUFFER, PathBuffer) -> 12
	repBuffer->ReparseDataLength = (USHORT)(12 + sizeof(redirectFilePathNt) - sizeof(WCHAR));
	repBuffer->Reserved = 0;
	repBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
	repBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength = (USHORT)(sizeof(redirectFilePathNt) - sizeof(WCHAR));
	repBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;
	repBuffer->SymbolicLinkReparseBuffer.PrintNameLength = 0;
	repBuffer->SymbolicLinkReparseBuffer.Flags = 0;

	for (int i = 0; i < sizeof(redirectFilePathNt) / sizeof(WCHAR); i++) {
		repBuffer->SymbolicLinkReparseBuffer.PathBuffer[i] = params->redirectPath[i];
	}

	Data->TagData = (PVOID)repBuffer;

	Data->IoStatus.Status = STATUS_REPARSE;
	Data->IoStatus.Information = IO_REPARSE_TAG_SYMLINK;

	return FLT_PREOP_COMPLETE;
}
__declspec(noinline) EXTERN_C int PreCreateCallbackEnd() {
	return 0;
}

VOID InstallCreateFileCallback(PVOID krnl_base, get_system_routine_t get_kroutine) {
	ULONG_PTR preCreateFuncStartAddr = (ULONG_PTR)PreCreateCallback;
	ULONG_PTR preCreateFuncEndAddr = (ULONG_PTR)PreCreateCallbackEnd;
	int preCreateFuncSize = preCreateFuncEndAddr - preCreateFuncStartAddr;

	InitFunction(krnl_base, get_kroutine);
	InitFunctionForFileFilter(krnl_base, get_kroutine);

	pSWFreezeDriverObject = GetDriverObjectByName(swfreezeDriverName);
	if (!pSWFreezeDriverObject) {
		Logger("[-] Failed to get SWFreeze driver object by name!\n");
		return;
	}
	pSeewoKeLiteLadyDriverObject = GetDriverObjectByName(seewoKeLiteLadyDriverName);
	if (!pSeewoKeLiteLadyDriverObject) {
		Logger("[-] Failed to get SeewoKeLiteLady driver object by name!\n");
		return;
	}

	DWORD64 seewoSetInfoFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xFF\x4C\x89", 11) - 0x30;
	if (!(seewoSetInfoFltFunctionAddress + 0x30) || !MmIsAddressValid((PVOID)seewoSetInfoFltFunctionAddress)) {
		Logger("[-] Failed to find SeewoKeLiteLady set information file filter function pattern address or address is invalid...\n");
		return;
	}
	Logger("[+] Found SeewoKeLiteLady set information file filter function pattern address -> %016llx\n", seewoSetInfoFltFunctionAddress);

	DWORD64 seewoCreateFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x4D\x8B\xF8\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xED\x4C\x89", 14) - 0x2A;
	if (!(seewoCreateFltFunctionAddress + 0x2A) || !MmIsAddressValid((PVOID)seewoCreateFltFunctionAddress)) {
		Logger("[-] Failed to find SeewoKeLiteLady file filter function pattern address or address is invalid...\n");
		return;
	}
	Logger("[+] Found SeewoKeLiteLady file filter function pattern address -> %016llx\n", seewoCreateFltFunctionAddress);

	if (RtlCompareMemory((PVOID)seewoCreateFltFunctionAddress, mov_r9, 2) == 2) {
		PCALLBACK_PARAMS params = *(PCALLBACK_PARAMS*)(seewoCreateFltFunctionAddress + 2);

		if (((PFREEZE_CONFIG)config)->volumeProtected == -1) {
			params->isRedirect = FALSE;
			Logger("[+] SeewoKeLiteLady file filter function is disabled\n");
			return;
		}

		params->isRedirect = TRUE;
		Logger("[*] SeewoKeLiteLady create file filter function is already modified, skipping...\n");
		return;
	}
	else if (((PFREEZE_CONFIG)config)->volumeProtected == -1) {
		Logger("[-] File filter not installed...\n");
		return;
	}

	PCALLBACK_PARAMS params = (PCALLBACK_PARAMS)ExAllocatePool(0, sizeof(CALLBACK_PARAMS));
	params->FltGetFileNameInformation = FltGetFileNameInformation;
	params->FltParseFileNameInformation = FltParseFileNameInformation;
	params->FltReleaseFileNameInformation = FltReleaseFileNameInformation;
	params->RtlCompareUnicodeString = RtlCompareUnicodeString;
	params->RtlCopyUnicodeString = RtlCopyUnicodeString;
	params->ExAllocatePool = ExAllocatePool;
	params->ExFreePool = ExFreePool;
	params->isRedirect = TRUE;
	RtlCopyMemory(params->configPath, configFilePathNt, sizeof(configFilePathNt));
	RtlCopyMemory(params->redirectPath, redirectFilePathNt, sizeof(redirectFilePathNt));
	*(PCALLBACK_PARAMS*)(mov_r9 + 2) = params;

	PVOID prefunc = ExAllocatePool(0, preCreateFuncSize);
	if (!prefunc) {
		Logger("[-] Fail to allocate memory for shellcode!\n");
		return;
	}

	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	*(PVOID*)(jmp + 2) = prefunc;
	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	Logger("[+] Allocated shellcode for file filter function at -> 0x%p\n", prefunc);

	PMDL pMdl = NULL;
	PVOID writableAddress = InitRWmemForShellcode((PVOID)seewoCreateFltFunctionAddress, sizeof(mov_r9) + sizeof(jmp), &pMdl);
	if (!writableAddress) {
		Logger("[-] Failed to get writable address for file filter function!\n");
		return;
	}

	RtlCopyMemory(writableAddress, mov_r9, sizeof(mov_r9));
	writableAddress = (BYTE*)writableAddress + sizeof(mov_r9);
	RtlCopyMemory(writableAddress, jmp, sizeof(jmp));
	ReleaseRwmem(pMdl, writableAddress);
	Logger("[+] Successfully wrote to filter function!\n");

	pMdl = NULL;
	writableAddress = InitRWmemForShellcode((PVOID)seewoSetInfoFltFunctionAddress, sizeof(ret1), &pMdl);
	if (!writableAddress) {
		Logger("[-] Failed to get writable address for file filter function!\n");
		return;
	}

	RtlCopyMemory(writableAddress, ret1, sizeof(ret1));
	ReleaseRwmem(pMdl, writableAddress);
	Logger("[+] Successfully wrote to set information filter function!\n");

	return;
}

VOID ModifyConfigByMjFunc(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	DWORD64 Address = 0;
	WCHAR swfreezeDriverName[] = L"\\Driver\\SWFreeze";
	WCHAR diskDriverName[] = L"\\Driver\\Disk";

	InitFunction(krnl_base, get_kroutine);

	Logger("[*] Starting to recover major function...\n");

	pSWFreezeDriverObject = GetDriverObjectByName(swfreezeDriverName);
	if (!MmIsAddressValid(pSWFreezeDriverObject)) {
		Logger("[-] Failed to get SWFreeze driver object by name or driver object is invalid!\n");
		return;
	}

	pDiskDriverObject = GetDriverObjectByName(diskDriverName);
	if (!MmIsAddressValid(pDiskDriverObject)) {
		Logger("[-] Failed to get disk driver object by name or driver object is invalid!\n");
		return;
	}

	Logger("[*] SWFreeze driver object -> 0x%p\n", pSWFreezeDriverObject);
	Logger("[*] Disk driver object -> 0x%p\n", pDiskDriverObject);

	HookedReadFunction = (ULONG64)pDiskDriverObject->MajorFunction[3];
	HookedWriteFunction = (ULONG64)pDiskDriverObject->MajorFunction[4];
	HookedIoControlFunction = (ULONG64)pDiskDriverObject->MajorFunction[14];

	DriverBase = pSWFreezeDriverObject->DriverStart;
	DriverSize = pSWFreezeDriverObject->DriverSize;
	Logger("[*] SWFreeze driver base -> 0x%016llx, SWFreeze driver size -> 0x%016llx\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x48\x8B\xC1\x48\x87", 5);
	PUnhookedReadFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0xD) + 0x11 : 0;
	PUnhookedWriteFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0x26) + 0x2A : 0;
	PUnhookedIoControlFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0x3B) + 0x3F : 0;

	if (!PUnhookedReadFuncAddress or !MmIsAddressValid((PVOID)PUnhookedReadFuncAddress)) {
		Logger("[-] Failed to find unhooked read dispatch function pattern address or address is invalid...\n");
		return;
	}

	if (!PUnhookedWriteFuncAddress or !MmIsAddressValid((PVOID)PUnhookedWriteFuncAddress)) {
		Logger("[-] Failed to find unhooked write dispatch function pattern address or address is invalid...\n");
		return;
	}

	if (!PUnhookedIoControlFuncAddress or !MmIsAddressValid((PVOID)PUnhookedIoControlFuncAddress)) {
		Logger("[-] Failed to find unhooked io control dispatch function pattern address or address is invalid...\n");
		return;
	}

	Logger("[+] Found unhooked read dispatch function pattern address -> %016llx\n", PUnhookedReadFuncAddress);
	Logger("[+] Found unhooked write dispatch function pattern address -> %016llx\n", PUnhookedWriteFuncAddress);
	Logger("[+] Found unhooked io control dispatch function pattern address -> %016llx\n", PUnhookedIoControlFuncAddress);

	if (*(PVOID*)PUnhookedWriteFuncAddress) {
		pDiskDriverObject->MajorFunction[3] = *(PVOID*)PUnhookedReadFuncAddress;
		pDiskDriverObject->MajorFunction[4] = *(PVOID*)PUnhookedWriteFuncAddress;
		pDiskDriverObject->MajorFunction[14] = *(PVOID*)PUnhookedIoControlFuncAddress;
		Logger("[+] Successfully restored original dispatch functions for read/write/io control!\n");
	}
	else {
		Logger("[+] No recovery required.\n");
	}
}

VOID ModifyConfigByWhiteList(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	Logger("[*] Starting to recover major function...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return;
	}
	Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		Logger("[-] Invalid volume info table address.\n");
		return;
	}

	VOLUME_INFO_R0 volumeCInfo = pVolumeInfo[2];
	volumeCInfo.whiteListItemCount = 1;
	volumeCInfo.itemStartSectorList[0] = startSector;
	volumeCInfo.itemSectorOffsetList[0] = byteOffset;

	Logger("[*] Start sector: %llu\n", volumeCInfo.itemStartSectorList[0]);
	Logger("[*] Sector count: %llu\n", volumeCInfo.itemSectorOffsetList[0]);

	Logger("[+] Successfully modified white list in memory!\n");
}

VOID ModifyConfigByWhiteListEx(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	Logger("[*] Starting to recover major function...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return;
	}
	Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);
	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		Logger("[-] Invalid volume info table address.\n");
		return;
	}

	for (int i = 0; i < 26; i++) {
		if (pVolumeInfo[i].isProtected == 1) {
			pVolumeInfo[i].isProtected = 0;
			Logger("[*] Disable drive %c protection.\n", pVolumeInfo[i].name);
		}
	}

	Logger("[+] Successfully disabled all protections!\n");
}

VOID GetFreezeInfo(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return;
	}
	Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		Logger("[-] Invalid volume info table address.\n");
		return;
	}

	for (int i = 0; i < 26; i++) {
		PVOLUME_INFO_R3 pVolumeInfoR3 = volumeInfoTable[i];
		if (!pVolumeInfoR3) continue;

		VOLUME_INFO_R0 volumeInfoR0 = pVolumeInfo[i];
		if (!volumeInfoR0.isProtected && !volumeInfoR0.isProtected2) {
			pVolumeInfoR3->volumeProtectType = UNPROTECTED;
		}
		else if (volumeInfoR0.isProtected && volumeInfoR0.isProtected2) {
			pVolumeInfoR3->volumeProtectType = PROTECTED;
		}
		else if (!volumeInfoR0.isProtected && volumeInfoR0.isProtected2) {
			pVolumeInfoR3->volumeProtectType = BYPASS;
		}
		else {
			pVolumeInfoR3->volumeProtectType = UNKNOWN;
		}
		pVolumeInfoR3->reservedBlockBytes = volumeInfoR0.reservedBlockBytes;
		pVolumeInfoR3->physicalStartingOffset = volumeInfoR0.physicalStartingOffset;
		pVolumeInfoR3->volumeTotalBytes = volumeInfoR0.volumeTotalBytes;
		pVolumeInfoR3->volumeSectorCount = volumeInfoR0.volumeSectorCount;
	}

	pSeewoKeLiteLadyDriverObject = GetDriverObjectByName(seewoKeLiteLadyDriverName);
	if (pSeewoKeLiteLadyDriverObject) {
		DWORD64 seewoCreateFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x4D\x8B\xF8\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xED\x4C\x89", 14) - 0x2A;
		if (!(seewoCreateFltFunctionAddress + 0x2A) || !MmIsAddressValid((PVOID)seewoCreateFltFunctionAddress)) {
			Logger("[-] Failed to find SeewoKeLiteLady file filter function pattern address or address is invalid...\n");
			return;
		}
		if (RtlCompareMemory((PVOID)seewoCreateFltFunctionAddress, mov_r9, 2) == 2) {
			PCALLBACK_PARAMS params = *(PCALLBACK_PARAMS*)(seewoCreateFltFunctionAddress + 2);
			if (params->isRedirect) filterInfo = INSTALLED;
			else filterInfo = DISABLED;
		}
		else filterInfo = NOT_INSTALLED;
	}
}