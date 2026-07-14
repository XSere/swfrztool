#include "struct.hpp"
#include "raw_dll.hpp"
#include "md5.hpp"

extern BYTE config[1024];
extern DWORD64 startSector;
extern DWORD64 byteOffset;

VOID GetMd5(BYTE* data, DWORD size, BYTE* md5_out);
BOOLEAN GenerateFreezeConfig(DWORD volumeProtected);
BOOLEAN GetConfigFileSectorInfo();
BOOLEAN WriteConfigFile(BOOLEAN bypass);
BOOLEAN InitRedirectFile();
BOOLEAN InitDllFile(DWORD volume);
BOOLEAN DeleteDllFile();