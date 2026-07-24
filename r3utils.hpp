#include "struct.hpp"
#include "raw_dll.hpp"
#include "md5.hpp"

extern PVOLUME_INFO_R3 volumeInfoTable[26];
extern FILTER_INSTALLATION_STATUS filterInfo;
extern BYTE config[1024];

VOID GetMd5(BYTE* data, DWORD size, BYTE* md5_out);
BOOLEAN InitVolumesInfoTable();
BOOLEAN IsDriverLoaded(const wchar_t* driverName);
BOOLEAN IsFilterDriverLoaded(const wchar_t* filterName);
BOOLEAN ReadConfigFile(const wchar_t* filePath, BYTE* config);
BOOLEAN GenerateFreezeConfig(DWORD volumeProtected);
int CompareRange(const void* a, const void* b);
DWORD64* GetFileClusters(HANDLE hFile, DWORD* pCount);
BOOLEAN GetFileSectorList(PWCHAR filePath, PSECTOR_RANGE* pSectorList, SIZE_T* length);
BOOLEAN CreateAndWriteFile(PWCHAR filepath, PVOID context, SIZE_T size);
BOOLEAN CreateAndReadFile(PWCHAR filepath, PVOID buf, SIZE_T size);
BOOLEAN WriteConfigFile(BOOLEAN bypass);
BOOLEAN InitRedirectFile();
BOOLEAN InitDllFile(DWORD volume);
BOOLEAN DeleteDllFile();
VOID PrintVolumeInfo();