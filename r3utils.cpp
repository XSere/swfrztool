#include "r3utils.hpp"

WCHAR configFilePath[] = L"C:\\ProgramData\\SeewoFreezeKernelConfig\\VolumeInfo.config";
WCHAR redirectFilePath[] = L"C:\\ProgramData\\SeewoFreezeKernelConfig\\redirect.config";
WCHAR dllFilePath[] = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\SeewoHugoLauncher_installVer\\USERENV.dll";
WCHAR cmdFilePath[] = L"C:\\Program Files (x86)\\Seewo\\SeewoService\\SeewoHugoLauncher_installVer\\cmd.bat";
PVOLUME_INFO_R3 volumeInfoTable[26] = { NULL };
FILTER_INSTALLATION_STATUS filterInfo;
BYTE config[1024] = { 0 };

VOID R3Logger(LPCSTR fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}

VOID GetMd5(BYTE* data, DWORD size, BYTE* md5_out) {
	MD5Context ctx;
	md5Init(&ctx);
	md5Update(&ctx, data, size);
	md5Finalize(&ctx);
	memcpy(md5_out, ctx.digest, 16);
}

BOOLEAN InitVolumesInfoTable() {
	DWORD driveMask = GetLogicalDrives();

	if (driveMask == 0) {
		R3Logger("[-] Failed to get drive mask.");
		return FALSE;
	}

	for (int i = 0; i < 26; i++) {
		if (driveMask & (1 << i)) {
			char driveName[] = { 'A' + i, ':', '\\', '\0' };
			if (GetDriveTypeA(driveName) == DRIVE_FIXED) {
				PVOLUME_INFO_R3 pVolumeInfo = (PVOLUME_INFO_R3)malloc(sizeof(VOLUME_INFO_R3));
				if (!pVolumeInfo) {
					R3Logger("[-] Failed to allocate memory for volume information.");
					return FALSE;
				}
				RtlZeroMemory(pVolumeInfo, sizeof(VOLUME_INFO_R3));
				pVolumeInfo->name = 'A' + i;
				volumeInfoTable[i] = pVolumeInfo;
			}
		}
	}
	return TRUE;
}

BOOLEAN IsDriverLoaded(const wchar_t* driverName) {
	LPVOID drivers[1024];
	DWORD cbNeeded;
	wchar_t szName[MAX_PATH];

	if (!EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded)) {
		return FALSE;
	}

	int count = cbNeeded / sizeof(drivers[0]);
	for (int i = 0; i < count; i++) {
		if (GetDeviceDriverFileNameW(drivers[i], szName, sizeof(szName))) {
			wchar_t* p = wcsrchr(szName, L'\\');
			p = p ? p + 1 : szName;
			if (_wcsicmp(p, driverName) == 0) {
				return TRUE;
			}
		}
	}
	R3Logger("[-] %ws driver not loaded.\n", driverName);
	return FALSE;
}

BOOLEAN IsFilterDriverLoaded(const wchar_t* filterName) {
	HRESULT hr;
	HANDLE hFilterFind = INVALID_HANDLE_VALUE;
	DWORD dwBytesReturned = 0;

	DWORD bufferSize = sizeof(FILTER_FULL_INFORMATION) + (MAX_PATH * sizeof(WCHAR));
	PFILTER_FULL_INFORMATION pBuffer = (PFILTER_FULL_INFORMATION)malloc(bufferSize);

	if (pBuffer == NULL) {
		return FALSE;
	}

	BOOLEAN found = FALSE;
	hr = FilterFindFirst(FilterFullInformation, pBuffer, bufferSize, &dwBytesReturned, &hFilterFind);
	if (SUCCEEDED(hr)) {
		do {
			USHORT nameLength = pBuffer->FilterNameLength / sizeof(WCHAR);
			if (wcslen(filterName) == nameLength) {
				if (_wcsnicmp(pBuffer->FilterNameBuffer, filterName, nameLength) == 0) {
					found = TRUE;
					break;
				}
			}

			hr = FilterFindNext(hFilterFind, FilterFullInformation, pBuffer, bufferSize, &dwBytesReturned);
		} while (SUCCEEDED(hr));

		FilterFindClose(hFilterFind);
	}

	free(pBuffer);

	if (!found) R3Logger("[-] %ws file filter driver not loaded.\n", filterName);
	return found;
}

BOOLEAN ReadConfigFile(const wchar_t* filePath, BYTE* config) {
	HANDLE hFile = CreateFile(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to open config file for reading.\n");
		return FALSE;
	}

	DWORD fileSize = 1024;
	DWORD bytesRead = 0;
	if (!ReadFile(hFile, config, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
		CloseHandle(hFile);
		R3Logger("[-] Failed to read config file.\n");
		return FALSE;
	}

	BYTE md5[16];
	GetMd5(config + 16, fileSize - 16, md5);
	if (memcmp(md5, config, 16) != 0) {
		CloseHandle(hFile);
		R3Logger("[-] Config file integrity check failed. MD5 checksum does not match.\n");
		return FALSE;
	}

	CloseHandle(hFile);
	return TRUE;
}

BOOLEAN GenerateFreezeConfig(DWORD volumeProtected) {
	if (volumeProtected == -1) {
		((PFREEZE_CONFIG)config)->volumeProtected = -1;
		return TRUE;
	}

	SIZE_T fileSize = 1024;
	R3Logger("[*] Generating config file...\n");
	ReadConfigFile(configFilePath, config);

	PFREEZE_CONFIG _config = (PFREEZE_CONFIG)config;
	_config->volumeProtected = volumeProtected;
	_config->VolumeProtected2 = volumeProtected;
	GetMd5(config + 16, fileSize - 16, _config->md5);
	R3Logger("[*] MD5 checksum updated.\n");

	R3Logger("[+] Config file generated successfully!\n");
	return TRUE;
}

int CompareRange(const void* a, const void* b) {
	SECTOR_RANGE* ra = (SECTOR_RANGE*)a;
	SECTOR_RANGE* rb = (SECTOR_RANGE*)b;
	if (ra->start < rb->start) return -1;
	if (ra->start > rb->start) return 1;
	return 0;
}

DWORD64* GetFileClusters(HANDLE hFile, DWORD* pCount) {
	RETRIEVAL_POINTERS_BUFFER* rp = NULL;
	DWORD bytesRet = 0;
	LARGE_INTEGER startingVcn = { 0 };
	DWORD64* clusters = NULL;
	*pCount = 0;

	DWORD bufferSize = 4096;
	while (1) {
		rp = (RETRIEVAL_POINTERS_BUFFER*)malloc(bufferSize);
		if (!rp) return NULL;
		BOOL ok = DeviceIoControl(hFile, FSCTL_GET_RETRIEVAL_POINTERS,
			&startingVcn, sizeof(startingVcn),
			rp, bufferSize, &bytesRet, NULL);
		if (ok) break;
		DWORD err = GetLastError();
		if (err == ERROR_MORE_DATA || err == ERROR_INSUFFICIENT_BUFFER) {
			free(rp);
			bufferSize *= 2;
			if (bufferSize > 1024 * 1024) return NULL;
		}
		else {
			free(rp);
			return NULL;
		}
	}

	DWORD total = 0;
	for (DWORD i = 0; i < rp->ExtentCount; i++) {
		DWORD64 prevVcn = (i == 0) ? rp->StartingVcn.QuadPart : rp->Extents[i - 1].NextVcn.QuadPart;
		total += (DWORD)(rp->Extents[i].NextVcn.QuadPart - prevVcn);
	}

	clusters = (DWORD64*)malloc(total * sizeof(DWORD64));
	if (!clusters) { free(rp); return NULL; }

	DWORD idx = 0;
	for (DWORD i = 0; i < rp->ExtentCount; i++) {
		DWORD64 lcn = rp->Extents[i].Lcn.QuadPart;
		DWORD64 prevVcn = (i == 0) ? rp->StartingVcn.QuadPart : rp->Extents[i - 1].NextVcn.QuadPart;
		DWORD64 count = rp->Extents[i].NextVcn.QuadPart - prevVcn;
		for (DWORD64 j = 0; j < count; j++) {
			clusters[idx++] = lcn + j;
		}
	}
	*pCount = total;
	free(rp);
	return clusters;
}

DWORD64 GetFileRecordNumber(HANDLE hFile) {
	BY_HANDLE_FILE_INFORMATION info;
	if (!GetFileInformationByHandle(hFile, &info)) {
		return 0;
	}
	DWORD64 index = ((DWORD64)info.nFileIndexHigh << 32) | info.nFileIndexLow;
	return index & 0x0000FFFFFFFFFFFFULL;
}

BOOLEAN GetFileSectorList(PWCHAR filePath, PSECTOR_RANGE* pSectorList, SIZE_T* length) {
	PSECTOR_RANGE ranges = NULL;
	wchar_t driveRoot[4];
	wcsncpy_s(driveRoot, 4, filePath, 3);
	driveRoot[3] = L'\0';

	DWORD sectorsPerCluster = 0, bytesPerSector = 0;
	if (!GetDiskFreeSpaceW(driveRoot, &sectorsPerCluster, &bytesPerSector, NULL, NULL)) {
		R3Logger("[-] GetDiskFreeSpace failed, error %d\n", GetLastError());
		return FALSE;
	}
	R3Logger("[*] Sectors per cluster: %d, Bytes per sector: %d\n", sectorsPerCluster, bytesPerSector);
	DWORD bytesPerCluster = sectorsPerCluster * bytesPerSector;

	HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to open file, error %d\n", GetLastError());
		return FALSE;
	}

	DWORD rangeCount = 0;

	DWORD count = 0;
	DWORD64* clusters = GetFileClusters(hFile, &count);
	if (clusters) {
		for (DWORD i = 0; i < count; i++) {
			DWORD64 start = clusters[i] * sectorsPerCluster;
			SECTOR_RANGE* newRanges = (SECTOR_RANGE*)realloc(ranges, (rangeCount + 1) * sizeof(SECTOR_RANGE));
			if (!newRanges) {
				free(ranges);
				free(clusters);
				CloseHandle(hFile);
				return FALSE;
			}
			ranges = newRanges;
			ranges[rangeCount].start = start;
			ranges[rangeCount].length = sectorsPerCluster;
			rangeCount++;
		}
		free(clusters);
	}


	DWORD64 recordNum = GetFileRecordNumber(hFile);
	if (recordNum == 0) {
		CloseHandle(hFile);
		free(ranges);
		return FALSE;
	}

	HANDLE hVol = CreateFileW(driveRoot, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hVol == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to open volume, error %d\n", GetLastError());
		CloseHandle(hFile);
		free(ranges);
		return FALSE;
	}

	NTFS_VOLUME_DATA_BUFFER volData;
	DWORD bytesRet;
	if (!DeviceIoControl(hVol, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
		&volData, sizeof(volData), &bytesRet, NULL)) {
		R3Logger("[-] FSCTL_GET_NTFS_VOLUME_DATA failed, error %d\n", GetLastError());
		CloseHandle(hVol);
		CloseHandle(hFile);
		free(ranges);
		return FALSE;
	}
	CloseHandle(hVol);

	DWORD64 mftStartLcn = volData.MftStartLcn.QuadPart;
	DWORD mftRecordSize = volData.BytesPerFileRecordSegment;

	DWORD64 offsetInMft = recordNum * mftRecordSize;
	DWORD64 clusterIndex = offsetInMft / bytesPerCluster;
	DWORD64 offsetInCluster = offsetInMft % bytesPerCluster;
	DWORD64 lcn = mftStartLcn + clusterIndex;
	DWORD64 sectorStart = lcn * sectorsPerCluster + offsetInCluster / bytesPerSector;
	DWORD sectorsPerRecord = mftRecordSize / bytesPerSector;
	SECTOR_RANGE* newRanges = (SECTOR_RANGE*)realloc(ranges, (rangeCount + 1) * sizeof(SECTOR_RANGE));
	if (!newRanges) {
		free(ranges);
		CloseHandle(hFile);
		return FALSE;
	}
	ranges = newRanges;
	ranges[rangeCount].start = sectorStart;
	ranges[rangeCount].length = sectorsPerRecord;
	rangeCount++;

	WCHAR parentPath[MAX_PATH];
	wcscpy_s(parentPath, MAX_PATH, filePath);
	WCHAR* lastSlash = wcsrchr(parentPath, L'\\');
	if (lastSlash) {
		*lastSlash = L'\0';
		HANDLE hParent = CreateFileW(parentPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (hParent != INVALID_HANDLE_VALUE) {
			DWORD parentCount = 0;
			DWORD64* parentClusters = GetFileClusters(hParent, &parentCount);
			if (parentClusters) {
				for (DWORD i = 0; i < parentCount; i++) {
					DWORD64 start = parentClusters[i] * sectorsPerCluster;
					SECTOR_RANGE* newRanges2 = (SECTOR_RANGE*)realloc(ranges, (rangeCount + 1) * sizeof(SECTOR_RANGE));
					if (!newRanges2) {
						free(ranges);
						free(parentClusters);
						CloseHandle(hParent);
						CloseHandle(hFile);
						return FALSE;
					}
					ranges = newRanges2;
					ranges[rangeCount].start = start;
					ranges[rangeCount].length = sectorsPerCluster;
					rangeCount++;
				}
				free(parentClusters);
			}
			CloseHandle(hParent);
		}
		else {
			R3Logger("[-] Failed to open parent directory, error %d\n", GetLastError());
		}
	}

	CloseHandle(hFile);

	if (rangeCount == 0) return TRUE;

	qsort(ranges, rangeCount, sizeof(SECTOR_RANGE), CompareRange);

	DWORD mergedCount = 0;
	for (DWORD i = 0; i < rangeCount; i++) {
		if (mergedCount == 0) {
			ranges[mergedCount++] = ranges[i];
			continue;
		}
		SECTOR_RANGE* last = &ranges[mergedCount - 1];
		if (ranges[i].start <= last->start + last->length) {
			DWORD64 newEnd = ranges[i].start + ranges[i].length;
			if (newEnd > last->start + last->length) {
				last->length = newEnd - last->start;
			}
		}
		else {
			ranges[mergedCount++] = ranges[i];
		}
	}

	R3Logger("[*] File: %ws\n", filePath);
	for (DWORD i = 0; i < mergedCount; i++) {
		printf("[*] Start: %lld, Length: %lld\n", ranges[i].start, ranges[i].length);
	}

	*pSectorList = ranges;
	*length = mergedCount;
	return TRUE;
}

BOOLEAN CreateAndWriteFile(PWCHAR filepath, PVOID context, SIZE_T size) {
	HANDLE hFile = CreateFile(filepath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to create file %ws\n", filepath);
		return FALSE;
	}

	DWORD bytesWritten = 0;
	if (!WriteFile(hFile, context, size, &bytesWritten, NULL) || bytesWritten != size) {
		CloseHandle(hFile);
		R3Logger("[-] Failed to write file %ws.\n", filepath);
		return FALSE;
	}

	CloseHandle(hFile);
	return TRUE;
}

BOOLEAN CreateAndReadFile(PWCHAR filepath, PVOID buf, SIZE_T size) {
	HANDLE hFile = CreateFile(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to open file %ws\n", filepath);
		return FALSE;
	}

	DWORD bytesRead = 0;
	if (!ReadFile(hFile, buf, size, &bytesRead, NULL) || bytesRead != size) {
		CloseHandle(hFile);
		R3Logger("[-] Failed to read file %ws\n", filepath);
		return FALSE;
	}

	CloseHandle(hFile);
	return TRUE;
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
		R3Logger("[-] Fail to write config file.\n");
		return FALSE;
	}
	FlushFileBuffers(hFile);
	CloseHandle(hFile);
	R3Logger("[+] Successfully written to configuration file!\n");
	return TRUE;
}

BOOLEAN InitRedirectFile() {
	HANDLE hFile = CreateFile(redirectFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		R3Logger("[-] Failed to create redirect file.\n");
		return FALSE;
	}
	CloseHandle(hFile);
	return TRUE;
}

BOOLEAN InitDllFile(DWORD volume) {
	if (!CreateAndWriteFile(configFilePath, (PVOID)raw_dll, sizeof(raw_dll))) {
		return FALSE;
	}

	CHAR cmdline[MAX_PATH + 5 + 26 * 2 * sizeof(CHAR) + 1] = { 0 };
	PCHAR ptr = cmdline;

	DWORD pathLen = GetModuleFileNameA(NULL, cmdline, sizeof(cmdline));
	ptr += pathLen;
	if (pathLen == 0) {
		R3Logger("[-] Failed to get file path.");
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

	if (!CreateAndWriteFile(cmdFilePath, cmdline, sizeof(cmdline))) {
		return FALSE;
	}

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

VOID PrintVolumeInfo() {
	R3Logger("---------------VOLUME INFO---------------\n");
	for (int i = 0; i < 26; i++) {
		PVOLUME_INFO_R3 pVolumeInfo = volumeInfoTable[i];
		if (!pVolumeInfo) {
			continue;
		}

		R3Logger("[*] volume name: %c\n", pVolumeInfo->name);
		R3Logger("[*] volume protection status: ");
		switch (pVolumeInfo->volumeProtectType) {
		case UNPROTECTED:
			R3Logger("unprotected\n\n");
			continue;
		case PROTECTED:
			R3Logger("protected\n");
			break;
		case BYPASS:
			R3Logger("bypass\n");
			break;
		default:
			R3Logger("unknown\n\n");
			continue;
		}
		R3Logger("[*] starting offset: %llu\n", pVolumeInfo->physicalStartingOffset);
		R3Logger("[*] volume size: %llu Bytes\n", pVolumeInfo->volumeTotalBytes);
		R3Logger("[*] volume sector count: %llu\n", pVolumeInfo->volumeSectorCount);
		R3Logger("\n");
	}

	R3Logger("---------------CONFIG INFO---------------\n");
	FREEZE_CONFIG _config = { 0 };
	if (!ReadConfigFile(configFilePath, (PBYTE) & _config)) {
		R3Logger("[-] Failed to analysis config info.\n");
		return;
	}

	R3Logger("[*] protected volumes: ");
	BOOLEAN protect = FALSE;
	for (int i = 0; i < 26; i++) {
		if (_config.volumeProtected & 1llu << i) {
			R3Logger("%c ", 'A' + i);
			protect = TRUE;
		}
	}
	if (!protect) R3Logger("none");
	R3Logger("\n\n");

	R3Logger("[*] file filter for config: ");
	switch (filterInfo) {
		case NOT_INSTALLED: {
			R3Logger("not installed\n\n");
			return;
		}
		case INSTALLED: {
			R3Logger("installed\n");
			break;
		}
		case DISABLED: {
			R3Logger("disabled\n\n");
			return;
		}
	}

	if (!ReadConfigFile(redirectFilePath, (PBYTE)&_config)) {
		R3Logger("[-] Failed to analysis config info.\n");
		return;
	}

	R3Logger("[*] displayed protected volumes: ");
	protect = FALSE;
	for (int i = 0; i < 26; i++) {
		if (_config.volumeProtected & 1llu << i) {
			R3Logger("%c ", 'A' + i);
			protect = TRUE;
		}
	}
	if (!protect) R3Logger("none");
	R3Logger("\n\n");
}