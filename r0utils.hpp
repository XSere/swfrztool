#include "struct.hpp"

#define InitFunctionByName(name) name = (name##_T)get_kroutine(krnl_base, #name);

extern CHAR logBuf[4096];
extern CHAR* logBufPtr;

VOID Logger(LPCSTR fmt, ...);
BOOLEAN GetDriverBaseAndSize(const char* targetModuleName, PVOID* pDriverBase, SIZE_T* pDriverSize);
DWORD64 ScanPatternByMask(PVOID base, SIZE_T limit, BYTE* pattern, SIZE_T patternSize, BYTE* mask);
DWORD64 ScanPattern(PVOID base, SIZE_T limit, BYTE* pattern, SIZE_T patternSize);
BOOLEAN InitObReferenceObjectByName();
PVOID GetFunctionByExportDir(PVOID FltMgrBase, const char* FunctionName);
VOID InitFunction(PVOID krnl_base, get_system_routine_t get_kroutine);
VOID InitFunctionForFileFilter(PVOID krnl_base, get_system_routine_t get_kroutine);
PDRIVER_OBJECT GetDriverObjectByName(WCHAR* DriverName);