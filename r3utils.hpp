#include "struct.hpp"
#include "raw_dll.hpp"
#include "md5.hpp"

enum VOLUME_PROTECTION_STATUS {
	UNKNOWN,
	UNPROTECTED,
	PROTECTED,
	BYPASS
};

enum FILTER_INSTALLATION_STATUS {
	NOT_INSTALLED,
	INSTALLED,
	DISABLED,
};

typedef struct _VOLUME_INFO_R3 {
	CHAR name;
	VOLUME_PROTECTION_STATUS volumeProtectType;
	DWORD64 reservedBlockBytes;
	DWORD64 physicalStartingOffset;
	DWORD64 volumeTotalBytes;
	DWORD64 extendCluster;
	DWORD64 volumeSectorCount;
} VOLUME_INFO_R3, * PVOLUME_INFO_R3;

extern PVOLUME_INFO_R3 volumeInfoTable[26];
extern FILTER_INSTALLATION_STATUS filterInfo;
extern BYTE config[1024];
extern DWORD64 startSector;
extern DWORD64 byteOffset;

VOID GetMd5(BYTE* data, DWORD size, BYTE* md5_out);
BOOLEAN InitVolumesInfoTable();
BOOLEAN IsDriverLoaded(const wchar_t* driverName);
BOOLEAN IsFilterDriverLoaded(const wchar_t* filterName);
BOOLEAN ReadConfigFile(const wchar_t* filePath, BYTE* config);
BOOLEAN GenerateFreezeConfig(DWORD volumeProtected);
BOOLEAN GetConfigFileSectorInfo();
BOOLEAN WriteConfigFile(BOOLEAN bypass);
BOOLEAN InitRedirectFile();
BOOLEAN InitDllFile(DWORD volume);
BOOLEAN DeleteDllFile();
VOID PrintVolumeInfo();