#include "r0utils.hpp"
#include "r3utils.hpp"
#include <stdio.h>

typedef struct _FLT_CALLBACK_PARAMS {
    FltGetFileNameInformation_T FltGetFileNameInformation;
    FltParseFileNameInformation_T FltParseFileNameInformation;
    FltReleaseFileNameInformation_T FltReleaseFileNameInformation;
    RtlCompareUnicodeString_T RtlCompareUnicodeString;
    RtlCopyUnicodeString_T RtlCopyUnicodeString;
    ExAllocatePool_T ExAllocatePool;
    ExFreePool_T ExFreePool;
    BOOLEAN isRedirect;
    WCHAR configPath[MAX_PATH];
    WCHAR redirectPath[MAX_PATH];
} FLT_CALLBACK_PARAMS, * PFLT_CALLBACK_PARAMS;

EXTERN_C FLT_PREOP_CALLBACK_STATUS PreCreateCallback(PFLT_CALLBACK_DATA Data, PFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext, PFLT_CALLBACK_PARAMS params);
BOOLEAN InstallCreateFileCallback(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN SetMjFunc(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN ModifyWhiteListByRanges(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN ModifyWhiteListByBuffer(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN GetWhiteListBitmap(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN ModifyVolumeProtectionStatus(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);
BOOLEAN GetFreezeInfo(PVOID krnl_base, get_system_routine_t get_kroutine, PR0_PARAMS r0params);