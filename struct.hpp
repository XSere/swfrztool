#pragma once
#include "msrexec_utils.hpp"

#define IRP_MJ_CREATE                   0x00
#define IRP_MJ_OPERATION_END            0xFF

typedef int FLT_PREOP_CALLBACK_STATUS;
#define FLT_PREOP_SUCCESS_WITH_CALLBACK 0
#define FLT_PREOP_SUCCESS_NO_CALLBACK   1
#define FLT_PREOP_PENDING               2
#define FLT_PREOP_SUBMODULE_STATUS      3
#define FLT_PREOP_COMPLETE              4
#define FLT_PREOP_SYNCHRONIZE           5
#define FLT_PREOP_DISALLOW_FASTIO       6

typedef int FLT_POSTOP_CALLBACK_STATUS;
#define FLT_POSTOP_SUCCESS_WITH_CALLBACK 0
#define FLT_POSTOP_FINISHED_PROCESSING   1

typedef struct _FLT_OPERATION_REGISTRATION {
    UCHAR MajorFunction;
    ULONG Flags;
    PVOID PreOperation;
    PVOID PostOperation;
    PVOID Reserved1;
} FLT_OPERATION_REGISTRATION, * PFLT_OPERATION_REGISTRATION;

typedef struct _FLT_REGISTRATION {
    ULONG Size;
    USHORT Version;
    ULONG Flags;
    PVOID ContextRegistration;
    PVOID OperationRegistration;
    PVOID FilterUnloadCallback;
    PVOID InstanceSetupCallback;
    PVOID InstanceQueryTeardownCallback;
    PVOID InstanceTeardownStartCallback;
    PVOID InstanceTeardownCompleteCallback;
    PVOID GenerateFileNameCallback;
    PVOID NormalizeNameComponentCallback;
    PVOID NormalizeContextCleanupCallback;
    PVOID TransactionNotificationCallback;
    PVOID NormalizeNameComponentExCallback;
    PVOID SectionNotificationCallback;
} FLT_REGISTRATION, * PFLT_REGISTRATION;

typedef struct _FLT_RELATED_OBJECTS {
    USHORT Size;
    USHORT TransactionContext;
    PVOID Filter;
    PVOID Instance;
    PVOID DeviceObject;
    PVOID FileObject;
    PVOID Transaction;
} FLT_RELATED_OBJECTS, * PFLT_RELATED_OBJECTS;

typedef struct _FLT_IO_PARAMETER_BLOCK {
    ULONG IrpFlags;
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    UCHAR OperationFlags;
    UCHAR Reserved;
    PVOID TargetFileObject;
    PVOID TargetInstance;
    PVOID SecurityContext;
    ULONG Options;
    USHORT FileAttributes;
    USHORT ShareAccess;
    ULONG EaLength;
    PVOID EaBuffer;
    LARGE_INTEGER AllocationSize;
} FLT_IO_PARAMETER_BLOCK, * PFLT_IO_PARAMETER_BLOCK;

typedef struct _REPARSE_DATA_BUFFER
{
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;

    _Field_size_bytes_(ReparseDataLength)
        union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct
        {
            ULONG StringCount;
            WCHAR StringList[1];
        } AppExecLinkReparseBuffer;
        struct
        {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;


typedef struct _NEW_IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } DUMMYUNIONNAME;
    ULONG_PTR Information;
} NEW_IO_STATUS_BLOCK, * PNEW_IO_STATUS_BLOCK;

typedef struct _FLT_CALLBACK_DATA {
    ULONG Flags;
    PVOID Thread;
    PFLT_IO_PARAMETER_BLOCK Iopb;
    NEW_IO_STATUS_BLOCK IoStatus;
    PVOID TagData;
    union {
        struct {
            LIST_ENTRY QueueLinks;
            PVOID QueueContext[2];
        };
        PVOID FilterContext[4];
    } DUMMYUNIONNAME;
    UCHAR RequestorMode;
} FLT_CALLBACK_DATA, * PFLT_CALLBACK_DATA;

typedef struct _FILE_OBJECT {
    USHORT Type;
    USHORT Size;
    PVOID DeviceObject;
    PVOID Vpb;
    PVOID FsContext;
    PVOID FsContext2;
    PVOID SectionObjectPointer;
    PVOID PrivateCacheMap;
    LONG FinalStatus;
    struct _FILE_OBJECT* RelatedFileObject;
    UCHAR LockOperation;
    UCHAR DeletePending;
    UCHAR ReadAccess;
    UCHAR WriteAccess;
    UCHAR DeleteAccess;
    UCHAR SharedRead;
    UCHAR SharedWrite;
    UCHAR SharedDelete;
    ULONG Flags;
    UNICODE_STRING FileName;
} FILE_OBJECT, * PFILE_OBJECT;

typedef struct _FLT_FILE_NAME_INFORMATION {
    USHORT Size;
    USHORT NamesParsed;
    ULONG Format;
    UNICODE_STRING Name;
    UNICODE_STRING Volume;
    UNICODE_STRING Share;
    UNICODE_STRING Extension;
    UNICODE_STRING Stream;
    UNICODE_STRING FinalComponent;
    UNICODE_STRING ParentDir;
} FLT_FILE_NAME_INFORMATION, * PFLT_FILE_NAME_INFORMATION;

typedef enum _LOCK_OPERATION {
    IoReadAccess,
    IoWriteAccess,
    IoModifyAccess
} LOCK_OPERATION;

typedef enum _MM_PAGE_PRIORITY {
    LowPagePriority,
    NormalPagePriority = 16,
    HighPagePriority = 32,
    MdlMappingNoExecute = 0x40000000
} MM_PAGE_PRIORITY;

typedef struct _DRIVER_OBJECT {
    SHORT Type;
    SHORT Size;
    PVOID DeviceObject;
    ULONG Flags;
    PVOID DriverStart;
    ULONG DriverSize;
    PVOID DriverSection;
    PVOID DriverExtension;
    UNICODE_STRING DriverName;
    PUNICODE_STRING HardwareDatabase;
    PVOID FastIoDispatch;
    PVOID DriverInit;
    PVOID DriverStartIo;
    PVOID DriverUnload;
    PVOID MajorFunction[28];
} DRIVER_OBJECT, * PDRIVER_OBJECT;

typedef enum _KPROCESSOR_MODE {
    KernelMode,
    UserMode
} KPROCESSOR_MODE;

typedef struct _MDL {
    struct _MDL* Next;
    SHORT Size;
    SHORT MdlFlags;
    PVOID Process;
    PVOID MappedSystemVa;
    PVOID StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
} MDL, * PMDL;

#define RTL_CONSTANT_STRING(s) \
{ \
    sizeof( s ) - sizeof( (s)[0] ), \
    sizeof( s ), \
    (PWSTR)s \
}

#pragma pack(push, 1)
typedef struct _FREEZE_CONFIG
{
    BYTE md5[16];
    DWORD volumeProtected;
    DWORD align1;
    BYTE byte_18DA8;
    DWORD isStopProtect;
    DWORD dword_18DAD;
    DWORD64 qword_18DB1;
    BYTE align2[40];
    DWORD dword_18DE1;
    BYTE align3[54];
    BYTE configVersion;
    DWORD VolumeProtected2;
    DWORD updatingTimeSet;
    DWORD UpdateExpirationTime;
    BYTE align4[872];
} FREEZE_CONFIG, * PFREEZE_CONFIG;

typedef struct _FRZ_BITMAP
{
    DWORD64 TotalBits;
    DWORD64 Reserved;
    DWORD64 BitsPerBlock;
    DWORD64 BlockShift;
    DWORD64 BlockMask;
    DWORD64 LeafNodeSize;
    DWORD64 BlockCount;
    PVOLUME_BITMAP_BUFFER* BlockTable;
} FRZ_BITMAP, * PFRZ_BITMAP;

typedef struct _VOLUME_INFO_R0
{
    BYTE isProtected;
    BYTE isProtected2;
    WCHAR name;
    BYTE align1[20];
    DWORD64 reservedBlockBytes;
    DWORD64 physicalStartingOffset;
    DWORD64 volumeTotalBytes;
    DWORD bytesPerSector;
    DWORD bytesPerCluster;
    DWORD64 extendCluster;
    BYTE align2[8];
    PVOID pDeviceObject;
    DWORD64 volumeSectorCount;
    BYTE align3[24];
    _FRZ_BITMAP* pFrzBitMapTable[4];
    BYTE align4[80];
    DWORD64 itemStartSectorList[4];
    DWORD64 itemSectorOffsetList[4];
    DWORD64 whiteListItemCount;
    DWORD64 itemStartSectorList2[4];
    DWORD64 itemSectorOffsetList2[4];
    DWORD64 WhiteList2ItemCount;
} VOLUME_INFO_R0, * PVOLUME_INFO_R0;
#pragma pack(pop)

enum TASK {
    TASK_ERROR,
    TASK_HELP,
    TASK_MODIFY_CONFIG_BY_MJ_FUNC,
    TASK_MODIFY_CONFIG_BY_WHITE_LIST,
    TASK_MODIFY_CONFIG_BY_WHITE_LIST_EX,
    TASK_INSTALL_FILE_FILTER,
    TASK_GET_FREEZE_INFO
};

using get_system_routine_t = void* (*)(void*, const char*);
using DbgPrint_T = NTSTATUS(*)(PCSTR, ...);
using IoGetDeviceObjectPointer_T = NTSTATUS(*)(PUNICODE_STRING, ACCESS_MASK, PVOID*, PVOID*);
using MmIsAddressValid_T = BOOLEAN(*)(PVOID);
using DbgBreakPoint_T = VOID(*)();
using ZwQuerySystemInformation_T = NTSTATUS(*)(ULONG, PVOID, ULONG, PULONG);
using ExAllocatePool_T = PVOID(*)(ULONG, SIZE_T);
using ExFreePool_T = VOID(*)(PVOID);
using ObReferenceObjectByName_T = NTSTATUS(*)(PUNICODE_STRING, ULONG, PVOID, ULONG, PVOID, KPROCESSOR_MODE, PVOID, PVOID*);
using IoAllocateMdl_T = PMDL(*)(PVOID, ULONG, BOOLEAN, BOOLEAN, PVOID);
using MmProbeAndLockPages_T = VOID(*)(PMDL, KPROCESSOR_MODE, LOCK_OPERATION);
using MmGetSystemAddressForMdlSafe_T = PVOID(*)(PMDL, MM_PAGE_PRIORITY);
using MmUnlockPages_T = VOID(*)(PMDL);
using MmUnmapLockedPages_T = VOID(*)(PVOID, PMDL);
using MmMapLockedPagesSpecifyCache_T = PVOID(*)(PMDL, KPROCESSOR_MODE, ULONG, PVOID, BOOLEAN, ULONG);
using IoFreeMdl_T = VOID(*)(PMDL);
using FltGetFileNameInformation_T = NTSTATUS(*)(PVOID, ULONG, PFLT_FILE_NAME_INFORMATION*);
using FltParseFileNameInformation_T = NTSTATUS(*)(PVOID);
using FltReleaseFileNameInformation_T = VOID(*)(PVOID);
using RtlCompareUnicodeString_T = LONG(*)(PCUNICODE_STRING, PCUNICODE_STRING, BOOLEAN);
using RtlCopyUnicodeString_T = NTSTATUS(*)(PUNICODE_STRING, PCUNICODE_STRING);

extern DbgPrint_T DbgPrint;
extern IoGetDeviceObjectPointer_T IoGetDeviceObjectPointer;
extern MmIsAddressValid_T MmIsAddressValid;
extern DbgBreakPoint_T DbgBreakPoint;
extern ZwQuerySystemInformation_T ZwQuerySystemInformation;
extern ExAllocatePool_T ExAllocatePool;
extern ExFreePool_T ExFreePool;
extern ObReferenceObjectByName_T ObReferenceObjectByName;
extern FltGetFileNameInformation_T FltGetFileNameInformation;
extern FltParseFileNameInformation_T FltParseFileNameInformation;
extern FltReleaseFileNameInformation_T FltReleaseFileNameInformation;
extern RtlCompareUnicodeString_T RtlCompareUnicodeString;
extern RtlCopyUnicodeString_T RtlCopyUnicodeString;
extern IoAllocateMdl_T IoAllocateMdl;
extern MmProbeAndLockPages_T MmProbeAndLockPages;
extern MmGetSystemAddressForMdlSafe_T MmGetSystemAddressForMdlSafe;
extern MmUnlockPages_T MmUnlockPages;
extern MmUnmapLockedPages_T MmUnmapLockedPages;
extern MmMapLockedPagesSpecifyCache_T MmMapLockedPagesSpecifyCache;
extern IoFreeMdl_T IoFreeMdl;
extern PVOID* IoDriverObjectType;