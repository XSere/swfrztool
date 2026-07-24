#include "msrexec.hpp"
#include "core.hpp"
#include "vdm.hpp"
#include <locale.h>

#ifdef _M_IX86
#error "x86 compilation is not supported!"
#endif

DWORD volumeArguments = 0;
DWORD64 start;
DWORD64 sectorCount;
R0_PARAMS params = { 0 };
BOOLEAN ret;

BOOLEAN InitVolumeArguments(PWCHAR argv) {
    DWORD64 off = 0;
    for (int j = 0; j < wcslen(argv); j++) {
        off = 1ull << (toupper(argv[j]) - 'A');
        if (wcslen(argv) != 1 || off < 0 || off > 1 << 25) {
            return FALSE;
        }
        if (volumeArguments & off) {
            return FALSE;
        }
        volumeArguments |= off;
    }
    return TRUE;
}

BOOLEAN StringToDWORD64(const wchar_t* str, DWORD64* pValue) {
    wchar_t* endptr;
    *pValue = _wcstoui64(str, &endptr, 0);
    if (*pValue == _UI64_MAX && errno == ERANGE) {
        return FALSE;
    }
    return TRUE;
}

TASK ArgumentsHandler(int argc, wchar_t** argv) {
    TASK task = TASK_HELP;
    if (argc < 2) {
        return task;
    }

    if (wcscmp(argv[1], L"cfg") == 0) {
        task = TASK_UPDATE_CONFIG;

        for (int i = 2; i < argc; i++) {
            if (!InitVolumeArguments(argv[i])) {
                task = TASK_ERROR;
                return task;
            }
        }
        if (volumeArguments != 0) {
            volumeArguments |= 0x4;
        }
        return task;
    }

    else if (argc == 3 && wcscmp(argv[1], L"mjfunc") == 0) {
        task = TASK_SET_MJ_FUNC;
        if (wcscmp(argv[2], L"0") == 0) {
            params.mjFunc.recoverDiskMjFunc = FALSE;
        }
        else if (wcscmp(argv[2], L"1") == 0) {
            params.mjFunc.recoverDiskMjFunc = TRUE;
        }
        else {
            task = TASK_ERROR;
        }
        return task;
    }

    else if (argc >= 4 && wcscmp(argv[1], L"volume") == 0) {
        task = TASK_MODIFY_VOLUME_PROTECTION_STATUS;
        for (int i = 2; i < argc - 1; i++) {
            if (i == 2 && wcscmp(argv[i], L"0") == 0) {
                params.volumeProtection.volume = 0;
                break;
            }
            if (!InitVolumeArguments(argv[i])) {
                task = TASK_ERROR;
                return task;
            }
        }
        params.volumeProtection.volume = volumeArguments;

        if (wcscmp(argv[argc - 1], L"0") == 0) {
            params.volumeProtection.protection = FALSE;
        }
        else if (wcscmp(argv[argc - 1], L"1") == 0) {
            params.volumeProtection.protection = TRUE;
        }
        else {
            task = TASK_ERROR;
        }

        return task;
    }

    else if (argc >= 4 && wcscmp(argv[1], L"whitelist") == 0) {
        task = TASK_ERROR;

        if (argc == 5 && wcscmp(argv[2], L"file") == 0) {
            task = TASK_MODIFY_WHITE_LIST_BY_FILE;
            params.whiteList.volumeIndex = toupper(argv[3][0]) - 'A';
            if (wcscmp(argv[argc - 1], L"0") == 0) {
                params.whiteList.value = FALSE;
            }
            else if (wcscmp(argv[argc - 1], L"1") == 0) {
                params.whiteList.value = TRUE;
            }
            else {
                task = TASK_ERROR;
            }
        }

        else if (argc == 7 && wcscmp(argv[2], L"sec") == 0) {
            task = TASK_MODIFY_WHITE_LIST_BY_RANGE;
            params.whiteList.volumeIndex = toupper(argv[3][0]) - 'A';
            if (!StringToDWORD64(argv[4], &start)) {
                task = TASK_ERROR;
                return task;
            }
            if (!StringToDWORD64(argv[5], &sectorCount)) {
                task = TASK_ERROR;
                return task;
            }
            if (start % 8 || sectorCount % 8) {
                task = TASK_ERROR;
                return task;
            }

            params.whiteList.ranges = (PSECTOR_RANGE)malloc(sizeof(SECTOR_RANGE));
            params.whiteList.ranges[0].start = start;
            params.whiteList.ranges[0].length = sectorCount;
            params.whiteList.length = 1;

            if (wcscmp(argv[argc - 1], L"0") == 0) {
                params.whiteList.value = FALSE;
            }
            else if (wcscmp(argv[argc - 1], L"1") == 0) {
                params.whiteList.value = TRUE;
            }
            else {
                task = TASK_ERROR;
            }
        }

        else if (argc == 7 && wcscmp(argv[2], L"in") == 0) {
            task = TASK_MODIFY_WHITE_LIST_BY_BITMAP;
            params.whiteList.volumeIndex = toupper(argv[3][0]) - 'A';
            if (!StringToDWORD64(argv[4], &start)) {
                task = TASK_ERROR;
                return task;
            }
            if (!StringToDWORD64(argv[5], &sectorCount)) {
                task = TASK_ERROR;
                return task;
            }
            if (start % 8 || sectorCount % 8) {
                task = TASK_ERROR;
                return task;
            }
            params.whiteList.startSector = start;
            params.whiteList.sectorCount = sectorCount;
            
        }

        else if (argc == 7 && wcscmp(argv[2], L"out") == 0) {
            task = TASK_GET_WHITE_LIST_BITMAP;
            params.whiteList.volumeIndex = toupper(argv[3][0]) - 'A';
            if (!StringToDWORD64(argv[4], &start)) {
                task = TASK_ERROR;
                return task;
            }
            if (!StringToDWORD64(argv[5], &sectorCount)) {
                task = TASK_ERROR;
                return task;
            }
            if (start % 8 || sectorCount % 8) {
                task = TASK_ERROR;
                return task;
            }
            params.whiteList.startSector = start;
            params.whiteList.sectorCount = sectorCount;
        }
        return task;
    }

    else if (wcscmp(argv[1], L"flt") == 0) {
        task = TASK_INSTALL_FILE_FILTER;
        if (argc == 3 && wcscmp(argv[2], L"off") == 0) {
            volumeArguments = -1;
        }
        else {
            for (int i = 2; i < argc; i++) {
                if (!InitVolumeArguments(argv[i])) {
                    task = TASK_ERROR;
                    return task;
                }
            }
            params.flt.volume = volumeArguments;
        }
        return task;
    }

    else if (wcscmp(argv[1], L"info") == 0) {
        task = TASK_GET_FREEZE_INFO;
        return task;
    }

    else if (wcscmp(argv[1], L"help") == 0) {
        task = TASK_HELP;
        return task;
    }

    else {
        task = TASK_ERROR;
        return task;
    }
}

BOOLEAN InitR0Executer() {
    const auto [drv_handle, drv_key, drv_status] = vdm::load_drv();
    if (drv_status != STATUS_SUCCESS && drv_status != STATUS_OBJECT_NAME_COLLISION)
    {
        R3Logger("[-] Failed to load driver... reason -> 0x%x\n", drv_status);
        return FALSE;
    }
    else if (drv_handle == INVALID_HANDLE_VALUE)
    {
        R3Logger("[-] Failed to get driver handle... reason -> 0x%x\n", GetLastError());
        return FALSE;
    }

    R3Logger("[*] drv handle -> 0x%x, drv key -> %s, drv status -> 0x%x\n", (ULONG)drv_handle, drv_key.c_str(), drv_status);
    R3Logger("[*] ntoskrnl base address -> 0x%p\n", (PVOID)utils::kmodule::get_base("ntoskrnl.exe"));
    R3Logger("[*] NtShutdownSystem -> 0x%p\n", (PVOID)utils::kmodule::get_export("ntoskrnl.exe", "NtShutdownSystem"));

    return TRUE;
}

BOOLEAN R0Executer(r0func callback) {
    writemsr_t _write_msr =
        [&](std::uint32_t reg, std::uintptr_t value) -> bool
        {
            return vdm::writemsr(reg, value);
        };

    callback_t _callback =
        [&](void* krnl_base, get_system_routine_t func)
        {
            ret = callback(krnl_base, func, &params);
        };

    vdm::msrexec_ctx msrexec(_write_msr);
    if (!msrexec.success)
    {
        R3Logger("[-] Failed to initialize msrexec...\n");
        return FALSE;
    }

    R3Logger("[*] r3 -> r0...\n");
    msrexec.exec(_callback);

    R3Logger("%s", logBuf);

    return ret;
}

int __cdecl wmain(int argc, wchar_t** argv) {
    setlocale(LC_ALL, "");

    TASK task = ArgumentsHandler(argc, argv);
    switch (task) {
        case TASK_ERROR:
        {
            R3Logger("[-] Invalid arguments. Use help for help...\n");
            return 1;
        }

        case TASK_HELP:
        {
            wprintf(L"Usage:\n");
            wprintf(L"  cfg <drive1> <drive2>...                                 修改冰点还原配置(重启生效) 若开启保护而未保护卷C则自动保护卷C\n");
            wprintf(L"\n");
            wprintf(L"  mjfunc <status>                                          设置磁盘驱动读写ioctl分发例程 0-SWFreeze 1-disk.sys\n");
            wprintf(L"  volume <dirve> <status>                                  修改卷保护状态(仅支持本就受保护的卷) 0-禁用保护 1-开启保护\n");
            wprintf(L"  whitelist file <filepath> <status>                       计算文件扇区(数据流、MFT记录、父目录索引)并修改白扇区位图 0-重定向 1-直接读写\n");
            wprintf(L"  whitelist sec <volume> <start> <length> <status>         设置指定一块白扇区位图的值\n");
            wprintf(L"  whitelist in <volume> <start> <length> <infile>          读取文件并写入到白扇区中\n");
            wprintf(L"  whitelist out <volume> <start> <length> <outfile>        输出白扇区位图到文件中(起始扇区和偏移必须是8的倍数)\n");
            wprintf(L"\n");
            wprintf(L"  flt <drive1/off> <drive2>...                             伪装冰点还原状态 使用前确保加载SeewoKeLiteLady驱动 并禁用卷C还原\n");
            wprintf(L"\n");
            wprintf(L"  info                                                     获取冰点还原的各种信息\n");
            wprintf(L"\n");
            wprintf(L"  help                                                     显示帮助信息\n");
            wprintf(L"\n");
            wprintf(L"Example:\n");
            wprintf(L"  %s cfg                                  修改配置为解除冰点还原\n", argv[0]);
            wprintf(L"  %s cfg C D E                            修改配置为保护卷C D E\n", argv[0]);
            wprintf(L"  %s mjfunc 1                             恢复磁盘驱动默认分发例程(即插即用除外)\n", argv[0]);
            wprintf(L"  %s volume C 0                           禁用卷C保护\n", argv[0]);
            wprintf(L"  %s whitelist file file.txt              使file.txt所占扇区可以直接读写\n", argv[0]);
            wprintf(L"  %s whitelist sec C 0 2048 1             将扇区0-2048设为直接读写\n", argv[0]);
            wprintf(L"  %s whitelist out C 0 2048 file.txt      将卷C的白扇区位图输出到file.txt中\n", argv[0]);
            wprintf(L"  %s flt C D E                            模拟还原保护卷C D E\n", argv[0]);
            return 0;
        }

        case TASK_UPDATE_CONFIG:
        {
            if (!GenerateFreezeConfig(volumeArguments)) return 1;
            if (!WriteConfigFile(TRUE)) return 1;
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_SET_MJ_FUNC:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(SetMjFunc)) return 1;
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_MODIFY_WHITE_LIST_BY_FILE:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!GetFileSectorList(argv[3], &params.whiteList.ranges, &params.whiteList.sectorCount)) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyWhiteListByRanges)) return 1;
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_MODIFY_WHITE_LIST_BY_RANGE:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyWhiteListByRanges)) return 1;
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_MODIFY_WHITE_LIST_BY_BITMAP:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;

            params.whiteList.buffer = malloc(sectorCount / 8);
            if (!params.whiteList.buffer) {
                R3Logger("[-] Failed to allocate buffer.\n");
                return 1;
            }
            if (!CreateAndReadFile(argv[6], params.whiteList.buffer, sectorCount / 8)) {
                free(params.whiteList.buffer);
                return 1;
            }

            if (!R0Executer(ModifyWhiteListByBuffer)) {
                free(params.whiteList.buffer);
                return 1;
            }

            R3Logger("[+] Finished\n");
            free(params.whiteList.buffer);
            return 0;
        }

        case TASK_GET_WHITE_LIST_BITMAP:
        {   
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;

            params.whiteList.buffer = malloc(sectorCount / 8);
            if (!params.whiteList.buffer) {
                R3Logger("[-] Failed to allocate buffer.\n");
                return 1;
            }

            if (!R0Executer(GetWhiteListBitmap)) {
                free(params.whiteList.buffer);
                return 1;
            }
            if (!CreateAndWriteFile(argv[6], params.whiteList.buffer, sectorCount / 8)) {
                free(params.whiteList.buffer);
                return 1;
            }

            R3Logger("[+] Finished\n");
            free(params.whiteList.buffer);
            return 0;
        }

        case TASK_MODIFY_VOLUME_PROTECTION_STATUS:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyVolumeProtectionStatus)) return 1;
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_INSTALL_FILE_FILTER:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!IsFilterDriverLoaded(L"SeewoKeLiteLady")) return 1;
            if (!GenerateFreezeConfig(volumeArguments)) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(InstallCreateFileCallback)) return 1;
            if (volumeArguments != -1) {
                if (!InitRedirectFile()) return 1;
                if (!WriteConfigFile(FALSE)) return 1;
                if (!InitDllFile(volumeArguments)) return 1;
            }
            else {
                if (!DeleteDllFile()) return 1;
            }
            R3Logger("[+] Finished\n");
            return 0;
        }

        case TASK_GET_FREEZE_INFO:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitVolumesInfoTable()) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(GetFreezeInfo)) return 1;
            PrintVolumeInfo();
            R3Logger("[+] Finished\n");
            return 0;
        }
    }
}
