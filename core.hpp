#include "r0utils.hpp"
#include "r3utils.hpp"
#include <stdio.h>

typedef struct _CALLBACK_PARAMS {
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
} CALLBACK_PARAMS, * PCALLBACK_PARAMS;

EXTERN_C FLT_POSTOP_CALLBACK_STATUS PreCreateCallback(PFLT_CALLBACK_DATA Data, PFLT_RELATED_OBJECTS FltObjects, PVOID* CompletionContext, PCALLBACK_PARAMS params);
VOID InstallCreateFileCallback(PVOID krnl_base, get_system_routine_t get_kroutine);
VOID ModifyConfigByMjFunc(PVOID krnl_base, get_system_routine_t get_kroutine);
VOID ModifyConfigByWhiteList(PVOID krnl_base, get_system_routine_t get_kroutine);
VOID ModifyConfigByWhiteListEx(PVOID krnl_base, get_system_routine_t get_kroutine);