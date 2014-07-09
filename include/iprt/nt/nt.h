/* $Id$ */
/** @file
 * IPRT - Header for code using the Native NT API.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_nt_nt_h___
#define ___iprt_nt_nt_h___

/** @def IPRT_NT_MAP_TO_ZW
 * Map Nt calls to Zw calls.  In ring-0 the Zw calls let you pass kernel memory
 * to the APIs (takes care of the previous context checks).
 */
#ifdef DOXYGEN_RUNNING
# define IPRT_NT_MAP_TO_ZW
#endif

#ifdef IPRT_NT_MAP_TO_ZW
# define NtQueryInformationFile         ZwQueryInformationFile
# define NtQueryInformationProcess      ZwQueryInformationProcess
# define NtQueryInformationThread       ZwQueryInformationThread
# define NtQuerySystemInformation       ZwQuerySystemInformation
# define NtClose                        ZwClose
# define NtCreateFile                   ZwCreateFile
# define NtReadFile                     ZwReadFile
# define NtWriteFile                    ZwWriteFile
/** @todo this is very incomplete! */
#endif

#include <ntstatus.h>

/*
 * Hacks common to both base header sets.
 */
#define NtQueryObject              Incomplete_NtQueryObject
#define ZwQueryObject              Incomplete_ZwQueryObject
#define NtSetInformationObject     Incomplete_NtSetInformationObject
#define _OBJECT_INFORMATION_CLASS  Incomplete_OBJECT_INFORMATION_CLASS
#define OBJECT_INFORMATION_CLASS   Incomplete_OBJECT_INFORMATION_CLASS
#define ObjectBasicInformation     Incomplete_ObjectBasicInformation
#define ObjectTypeInformation      Incomplete_ObjectTypeInformation


#ifdef IPRT_NT_USE_WINTERNL
/*
 * Use Winternl.h.
 */
# define _FILE_INFORMATION_CLASS                IncompleteWinternl_FILE_INFORMATION_CLASS
# define FILE_INFORMATION_CLASS                 IncompleteWinternl_FILE_INFORMATION_CLASS
# define FileDirectoryInformation               IncompleteWinternl_FileDirectoryInformation

# define NtQueryInformationProcess              IncompleteWinternl_NtQueryInformationProcess
# define NtSetInformationProcess                IncompleteWinternl_NtSetInformationProcess
# define PROCESSINFOCLASS                       IncompleteWinternl_PROCESSINFOCLASS
# define _PROCESSINFOCLASS                      IncompleteWinternl_PROCESSINFOCLASS
# define PROCESS_BASIC_INFORMATION              IncompleteWinternl_PROCESS_BASIC_INFORMATION
# define PPROCESS_BASIC_INFORMATION             IncompleteWinternl_PPROCESS_BASIC_INFORMATION
# define _PROCESS_BASIC_INFORMATION             IncompleteWinternl_PROCESS_BASIC_INFORMATION
# define ProcessBasicInformation                IncompleteWinternl_ProcessBasicInformation
# define ProcessDebugPort                       IncompleteWinternl_ProcessDebugPort
# define ProcessWow64Information                IncompleteWinternl_ProcessWow64Information
# define ProcessImageFileName                   IncompleteWinternl_ProcessImageFileName
# define ProcessBreakOnTermination              IncompleteWinternl_ProcessBreakOnTermination

# define NtQueryInformationThread               IncompleteWinternl_NtQueryInformationThread
# define NtSetInformationThread                 IncompleteWinternl_NtSetInformationThread
# define THREADINFOCLASS                        IncompleteWinternl_THREADINFOCLASS
# define _THREADINFOCLASS                       IncompleteWinternl_THREADINFOCLASS
# define ThreadIsIoPending                      IncompleteWinternl_ThreadIsIoPending

# define NtQuerySystemInformation               IncompleteWinternl_NtQuerySystemInformation
# define NtSetSystemInformation                 IncompleteWinternl_NtSetSystemInformation
# define SYSTEM_INFORMATION_CLASS               IncompleteWinternl_SYSTEM_INFORMATION_CLASS
# define _SYSTEM_INFORMATION_CLASS              IncompleteWinternl_SYSTEM_INFORMATION_CLASS
# define SystemBasicInformation                 IncompleteWinternl_SystemBasicInformation
# define SystemPerformanceInformation           IncompleteWinternl_SystemPerformanceInformation
# define SystemTimeOfDayInformation             IncompleteWinternl_SystemTimeOfDayInformation
# define SystemProcessInformation               IncompleteWinternl_SystemProcessInformation
# define SystemProcessorPerformanceInformation  IncompleteWinternl_SystemProcessorPerformanceInformation
# define SystemInterruptInformation             IncompleteWinternl_SystemInterruptInformation
# define SystemExceptionInformation             IncompleteWinternl_SystemExceptionInformation
# define SystemRegistryQuotaInformation         IncompleteWinternl_SystemRegistryQuotaInformation
# define SystemLookasideInformation             IncompleteWinternl_SystemLookasideInformation
# define SystemPolicyInformation                IncompleteWinternl_SystemPolicyInformation


# define WIN32_NO_STATUS
# include <windef.h>
# include <winnt.h>
# include <winternl.h>
# undef WIN32_NO_STATUS
# include <ntstatus.h>


# undef _FILE_INFORMATION_CLASS
# undef FILE_INFORMATION_CLASS
# undef FileDirectoryInformation

# undef NtQueryInformationProcess
# undef NtSetInformationProcess
# undef PROCESSINFOCLASS
# undef _PROCESSINFOCLASS
# undef PROCESS_BASIC_INFORMATION
# undef PPROCESS_BASIC_INFORMATION
# undef _PROCESS_BASIC_INFORMATION
# undef ProcessBasicInformation
# undef ProcessDebugPort
# undef ProcessWow64Information
# undef ProcessImageFileName
# undef ProcessBreakOnTermination

# undef NtQueryInformationThread
# undef NtSetInformationThread
# undef THREADINFOCLASS
# undef _THREADINFOCLASS
# undef ThreadIsIoPending

# undef NtQuerySystemInformation
# undef NtSetSystemInformation
# undef SYSTEM_INFORMATION_CLASS
# undef _SYSTEM_INFORMATION_CLASS
# undef SystemBasicInformation
# undef SystemPerformanceInformation
# undef SystemTimeOfDayInformation
# undef SystemProcessInformation
# undef SystemProcessorPerformanceInformation
# undef SystemInterruptInformation
# undef SystemExceptionInformation
# undef SystemRegistryQuotaInformation
# undef SystemLookasideInformation
# undef SystemPolicyInformation

#else
/*
 * Use ntifs.h and wdm.h.
 */
# ifdef RT_ARCH_X86
#  define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#  pragma warning(disable : 4163)
# endif

# include <ntifs.h>
# include <wdm.h>

# ifdef RT_ARCH_X86
#  pragma warning(default : 4163)
#  undef _InterlockedAddLargeStatistic
# endif

# define IPRT_NT_NEED_API_GROUP_NTIFS
#endif

#undef NtQueryObject
#undef ZwQueryObject
#undef NtSetInformationObject
#undef _OBJECT_INFORMATION_CLASS
#undef OBJECT_INFORMATION_CLASS
#undef ObjectBasicInformation
#undef ObjectTypeInformation

#include <iprt/types.h>


/** @name Useful macros
 * @{ */
/** Indicates that we're targetting native NT in the current source. */
#define RTNT_USE_NATIVE_NT              1
/** Initializes a IO_STATUS_BLOCK. */
#define RTNT_IO_STATUS_BLOCK_INITIALIZER  { STATUS_FAILED_DRIVER_ENTRY, ~(uintptr_t)42 }
/** Similar to INVALID_HANDLE_VALUE in the Windows environment. */
#define RTNT_INVALID_HANDLE_VALUE         ( (HANDLE)~(uintptr_t)0 )
/** @}  */


/** @name IPRT helper functions for NT
 * @{ */
RT_C_DECLS_BEGIN

RTDECL(int) RTNtPathOpen(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fFileAttribs, ULONG fShareAccess,
                          ULONG fCreateDisposition, ULONG fCreateOptions, ULONG fObjAttribs,
                          PHANDLE phHandle, PULONG_PTR puDisposition);
RTDECL(int) RTNtPathOpenDir(const char *pszPath, ACCESS_MASK fDesiredAccess, ULONG fShareAccess, ULONG fCreateOptions,
                            ULONG fObjAttribs, PHANDLE phHandle, bool *pfObjDir);
RTDECL(int) RTNtPathClose(HANDLE hHandle);

RT_C_DECLS_END
/** @} */


/** @name NT API delcarations.
 * @{ */
RT_C_DECLS_BEGIN

/** @name Process access rights missing in ntddk headers
 * @{ */
#ifndef  PROCESS_TERMINATE
# define PROCESS_TERMINATE                  UINT32_C(0x00000001)
#endif
#ifndef  PROCESS_CREATE_THREAD
# define PROCESS_CREATE_THREAD              UINT32_C(0x00000002)
#endif
#ifndef  PROCESS_SET_SESSIONID
# define PROCESS_SET_SESSIONID              UINT32_C(0x00000004)
#endif
#ifndef  PROCESS_VM_OPERATION
# define PROCESS_VM_OPERATION               UINT32_C(0x00000008)
#endif
#ifndef  PROCESS_VM_READ
# define PROCESS_VM_READ                    UINT32_C(0x00000010)
#endif
#ifndef  PROCESS_VM_WRITE
# define PROCESS_VM_WRITE                   UINT32_C(0x00000020)
#endif
#ifndef  PROCESS_DUP_HANDLE
# define PROCESS_DUP_HANDLE                 UINT32_C(0x00000040)
#endif
#ifndef  PROCESS_CREATE_PROCESS
# define PROCESS_CREATE_PROCESS             UINT32_C(0x00000080)
#endif
#ifndef  PROCESS_SET_QUOTA
# define PROCESS_SET_QUOTA                  UINT32_C(0x00000100)
#endif
#ifndef  PROCESS_SET_INFORMATION
# define PROCESS_SET_INFORMATION            UINT32_C(0x00000200)
#endif
#ifndef  PROCESS_QUERY_INFORMATION
# define PROCESS_QUERY_INFORMATION          UINT32_C(0x00000400)
#endif
#ifndef  PROCESS_SUSPEND_RESUME
# define PROCESS_SUSPEND_RESUME             UINT32_C(0x00000800)
#endif
#ifndef  PROCESS_QUERY_LIMITED_INFORMATION
# define PROCESS_QUERY_LIMITED_INFORMATION  UINT32_C(0x00001000)
#endif
#ifndef  PROCESS_SET_LIMITED_INFORMATION
# define PROCESS_SET_LIMITED_INFORMATION    UINT32_C(0x00002000)
#endif
#define PROCESS_UNKNOWN_4000                UINT32_C(0x00004000)
#define PROCESS_UNKNOWN_6000                UINT32_C(0x00008000)
#ifndef PROCESS_ALL_ACCESS
# define PROCESS_ALL_ACCESS                 ( STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | UINT32_C(0x0000ffff) )
#endif
/** @} */

/** @name Thread access rights missing in ntddk headers
 * @{ */
#ifndef THREAD_QUERY_INFORMATION
# define THREAD_QUERY_INFORMATION           UINT32_C(0x00000040)
#endif
#ifndef THREAD_SET_THREAD_TOKEN
# define THREAD_SET_THREAD_TOKEN            UINT32_C(0x00000080)
#endif
#ifndef THREAD_IMPERSONATE
# define THREAD_IMPERSONATE                 UINT32_C(0x00000100)
#endif
#ifndef THREAD_DIRECT_IMPERSONATION
# define THREAD_DIRECT_IMPERSONATION        UINT32_C(0x00000200)
#endif
#ifndef THREAD_RESUME
# define THREAD_RESUME                      UINT32_C(0x00001000)
#endif
#define THREAD_UNKNOWN_2000                 UINT32_C(0x00002000)
#define THREAD_UNKNOWN_4000                 UINT32_C(0x00004000)
#define THREAD_UNKNOWN_8000                 UINT32_C(0x00008000)
/** @} */

/** @name Special handle values.
 * @{ */
#ifndef NtCurrentProcess
# define NtCurrentProcess()                 ( (HANDLE)-(intptr_t)1 )
#endif
#ifndef NtCurrentThread
# define NtCurrentThread()                  ( (HANDLE)-(intptr_t)2 )
#endif
#ifndef ZwCurrentProcess
# define ZwCurrentProcess()                 NtCurrentProcess()
#endif
#ifndef ZwCurrentThread
# define ZwCurrentThread()                  NtCurrentThread()
#endif
/** @} */


/** @name Directory object access rights.
 * @{ */
#ifndef DIRECTORY_QUERY
# define DIRECTORY_QUERY                    UINT32_C(0x00000001)
#endif
#ifndef DIRECTORY_TRAVERSE
# define DIRECTORY_TRAVERSE                 UINT32_C(0x00000002)
#endif
#ifndef DIRECTORY_CREATE_OBJECT
# define DIRECTORY_CREATE_OBJECT            UINT32_C(0x00000004)
#endif
#ifndef DIRECTORY_CREATE_SUBDIRECTORY
# define DIRECTORY_CREATE_SUBDIRECTORY      UINT32_C(0x00000008)
#endif
#ifndef DIRECTORY_ALL_ACCESS
# define DIRECTORY_ALL_ACCESS               ( STANDARD_RIGHTS_REQUIRED | UINT32_C(0x0000000f) )
#endif
/** @} */


#ifdef IPRT_NT_USE_WINTERNL
typedef struct _CLIENT_ID
{
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

NTSYSAPI NTSTATUS NTAPI NtCreateSection(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);

typedef struct _FILE_FS_ATTRIBUTE_INFORMATION
{
    ULONG   FileSystemAttributes;
    LONG    MaximumComponentNameLength;
    ULONG   FileSystemNameLength;
    WCHAR   FileSystemName[1];
} FILE_FS_ATTRIBUTE_INFORMATION;
typedef FILE_FS_ATTRIBUTE_INFORMATION *PFILE_FS_ATTRIBUTE_INFORMATION;

NTSYSAPI NTSTATUS NTAPI NtOpenProcessToken(HANDLE, ACCESS_MASK, PHANDLE);
NTSYSAPI NTSTATUS NTAPI NtOpenThreadToken(HANDLE, ACCESS_MASK, BOOLEAN, PHANDLE);

typedef enum _FSINFOCLASS
{
    FileFsVolumeInformation = 1,
    FileFsLabelInformation,
    FileFsSizeInformation,
    FileFsDeviceInformation,
    FileFsAttributeInformation,
    FileFsControlInformation,
    FileFsFullSizeInformation,
    FileFsObjectIdInformation,
    FileFsDriverPathInformation,
    FileFsVolumeFlagsInformation,
    FileFsSectorSizeInformation,
    FileFsDataCopyInformation,
    FileFsMaximumInformation
} FS_INFORMATION_CLASS;
typedef FS_INFORMATION_CLASS *PFS_INFORMATION_CLASS;
NTSYSAPI NTSTATUS NTAPI NtQueryVolumeInformationFile(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FS_INFORMATION_CLASS);

typedef struct _FILE_STANDARD_INFORMATION
{
    LARGE_INTEGER   AllocationSize;
    LARGE_INTEGER   EndOfFile;
    ULONG           NumberOfLinks;
    BOOLEAN         DeletePending;
    BOOLEAN         Directory;
} FILE_STANDARD_INFORMATION;
typedef FILE_STANDARD_INFORMATION *PFILE_STANDARD_INFORMATION;
typedef enum _FILE_INFORMATION_CLASS
{
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation,
    FileBothDirectoryInformation,
    FileBasicInformation,
    FileStandardInformation,
    FileInternalInformation,
    FileEaInformation,
    FileAccessInformation,
    FileNameInformation,
    FileRenameInformation,
    FileLinkInformation,
    FileNamesInformation,
    FileDispositionInformation,
    FilePositionInformation,
    FileFullEaInformation,
    FileModeInformation,
    FileAlignmentInformation,
    FileAllInformation,
    FileAllocationInformation,
    FileEndOfFileInformation,
    FileAlternateNameInformation,
    FileStreamInformation,
    FilePipeInformation,
    FilePipeLocalInformation,
    FilePipeRemoteInformation,
    FileMailslotQueryInformation,
    FileMailslotSetInformation,
    FileCompressionInformation,
    FileObjectIdInformation,
    FileCompletionInformation,
    FileMoveClusterInformation,
    FileQuotaInformation,
    FileReparsePointInformation,
    FileNetworkOpenInformation,
    FileAttributeTagInformation,
    FileTrackingInformation,
    FileIdBothDirectoryInformation,
    FileIdFullDirectoryInformation,
    FileValidDataLengthInformation,
    FileShortNameInformation,
    FileIoCompletionNotificationInformation,
    FileIoStatusBlockRangeInformation,
    FileIoPriorityHintInformation,
    FileSfioReserveInformation,
    FileSfioVolumeInformation,
    FileHardLinkInformation,
    FileProcessIdsUsingFileInformation,
    FileNormalizedNameInformation,
    FileNetworkPhysicalNameInformation,
    FileIdGlobalTxDirectoryInformation,
    FileIsRemoteDeviceInformation,
    FileUnusedInformation,
    FileNumaNodeInformation,
    FileStandardLinkInformation,
    FileRemoteProtocolInformation,
    FileRenameInformationBypassAccessCheck,
    FileLinkInformationBypassAccessCheck,
    FileVolumeNameInformation,
    FileIdInformation,
    FileIdExtdDirectoryInformation,
    FileReplaceCompletionInformation,
    FileHardLinkFullIdInformation,
    FileMaximumInformation
} FILE_INFORMATION_CLASS;
typedef FILE_INFORMATION_CLASS *PFILE_INFORMATION_CLASS;
NTSYSAPI NTSTATUS NTAPI NtQueryInformationFile(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);

typedef struct _MEMORY_SECTION_NAME
{
    UNICODE_STRING  SectionFileName;
    WCHAR           NameBuffer[1];
} MEMORY_SECTION_NAME;

#ifdef IPRT_NT_USE_WINTERNL
typedef struct _PROCESS_BASIC_INFORMATION
{
    NTSTATUS ExitStatus;
    PPEB PebBaseAddress;
    ULONG_PTR AffinityMask;
    int32_t BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION;
typedef PROCESS_BASIC_INFORMATION *PPROCESS_BASIC_INFORMATION;
#endif

typedef enum _PROCESSINFOCLASS
{
    ProcessBasicInformation = 0,
    ProcessQuotaLimits,
    ProcessIoCounters,
    ProcessVmCounters,
    ProcessTimes,
    ProcessBasePriority,
    ProcessRaisePriority,
    ProcessDebugPort,
    ProcessExceptionPort,
    ProcessAccessToken,
    ProcessLdtInformation,
    ProcessLdtSize,
    ProcessDefaultHardErrorMode,
    ProcessIoPortHandlers,
    ProcessPooledUsageAndLimits,
    ProcessWorkingSetWatch,
    ProcessUserModeIOPL,
    ProcessEnableAlignmentFaultFixup,
    ProcessPriorityClass,
    ProcessWx86Information,
    ProcessHandleCount,
    ProcessAffinityMask,
    ProcessPriorityBoost,
    ProcessDeviceMap,
    ProcessSessionInformation,
    ProcessForegroundInformation,
    ProcessWow64Information,
    ProcessImageFileName,
    ProcessLUIDDeviceMapsEnabled,
    ProcessBreakOnTermination,
    ProcessDebugObjectHandle,
    ProcessDebugFlags,
    ProcessHandleTracing,
    ProcessIoPriority,
    ProcessExecuteFlags,
    ProcessTlsInformation,
    ProcessCookie,
    ProcessImageInformation,
    ProcessCycleTime,
    ProcessPagePriority,
    ProcessInstrumentationCallbak,
    ProcessThreadStackAllocation,
    ProcessWorkingSetWatchEx,
    ProcessImageFileNameWin32,
    ProcessImageFileMapping,
    ProcessAffinityUpdateMode,
    ProcessMemoryAllocationMode,
    ProcessGroupInformation,
    ProcessTokenVirtualizationEnabled,
    ProcessConsoleHostProcess,
    ProcessWindowsInformation,
    MaxProcessInfoClass
} PROCESSINFOCLASS;
NTSYSAPI NTSTATUS NTAPI NtQueryInformationProcess(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

typedef enum _THREADINFOCLASS
{
    ThreadBasicInformation = 0,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    ThreadBreakOnTermination,
    ThreadSwitchLegacyState,
    ThreadIsTerminated,
    ThreadLastSystemCall,
    ThreadIoPriority,
    ThreadCycleTime,
    ThreadPagePriority,
    ThreadActualBasePriority,
    ThreadTebInformation,
    ThreadCSwitchMon,
    ThreadCSwitchPmu,
    ThreadWow64Context,
    ThreadGroupInformation,
    ThreadUmsInformation,
    ThreadCounterProfiling,
    ThreadIdealProcessorEx,
    ThreadCpuAccountingInformation,
    MaxThreadInfoClass
} THREADINFOCLASS;
NTSYSAPI NTSTATUS NTAPI NtSetInformationThread(HANDLE, THREADINFOCLASS, LPCVOID, ULONG);

NTSYSAPI NTSTATUS NTAPI NtQueryInformationToken(HANDLE, TOKEN_INFORMATION_CLASS, PVOID, ULONG, PULONG);

NTSYSAPI NTSTATUS NTAPI NtReadFile(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG, PLARGE_INTEGER, PULONG);
NTSYSAPI NTSTATUS NTAPI NtWriteFile(HANDLE, HANDLE, PIO_APC_ROUTINE, void const *, PIO_STATUS_BLOCK, PVOID, ULONG, PLARGE_INTEGER, PULONG);

NTSYSAPI NTSTATUS NTAPI NtReadVirtualMemory(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
NTSYSAPI NTSTATUS NTAPI NtWriteVirtualMemory(HANDLE, PVOID, void const *, SIZE_T, PSIZE_T);

NTSYSAPI NTSTATUS NTAPI RtlAddAccessAllowedAce(PACL, ULONG, ULONG, PSID);
NTSYSAPI NTSTATUS NTAPI RtlCopySid(ULONG, PSID, PSID);
NTSYSAPI NTSTATUS NTAPI RtlCreateAcl(PACL, ULONG, ULONG);
NTSYSAPI NTSTATUS NTAPI RtlCreateSecurityDescriptor(PSECURITY_DESCRIPTOR, ULONG);
NTSYSAPI NTSTATUS NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW);
NTSYSAPI NTSTATUS NTAPI RtlInitializeSid(PSID, PSID_IDENTIFIER_AUTHORITY, UCHAR);
NTSYSAPI NTSTATUS NTAPI RtlSetDaclSecurityDescriptor(PSECURITY_DESCRIPTOR, BOOLEAN, PACL, BOOLEAN);
NTSYSAPI PULONG   NTAPI RtlSubAuthoritySid(PSID, ULONG);

#endif /* IPRT_NT_USE_WINTERNL */

typedef enum _OBJECT_INFORMATION_CLASS
{
    ObjectBasicInformation = 0,
    ObjectNameInformation,
    ObjectTypeInformation,
    ObjectAllInformation,
    ObjectDataInformation
} OBJECT_INFORMATION_CLASS;
typedef OBJECT_INFORMATION_CLASS *POBJECT_INFORMATION_CLASS;
#ifdef IN_RING0
# define NtQueryObject ZwQueryObject
#endif
NTSYSAPI NTSTATUS NTAPI NtQueryObject(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG);
NTSYSAPI NTSTATUS NTAPI NtSetInformationObject(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG);
NTSYSAPI NTSTATUS NTAPI NtDuplicateObject(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);

NTSYSAPI NTSTATUS NTAPI NtOpenDirectoryObject(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);

typedef struct _OBJECT_DIRECTORY_INFORMATION
{
    UNICODE_STRING Name;
    UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION;
typedef OBJECT_DIRECTORY_INFORMATION *POBJECT_DIRECTORY_INFORMATION;
NTSYSAPI NTSTATUS NTAPI NtQueryDirectoryObject(HANDLE, PVOID, ULONG, BOOLEAN, BOOLEAN, PULONG, PULONG);

/** Retured by ProcessImageInformation as well as NtQuerySection. */
typedef struct _SECTION_IMAGE_INFORMATION
{
    PVOID TransferAddress;
    ULONG ZeroBits;
    SIZE_T MaximumStackSize;
    SIZE_T CommittedStackSize;
    ULONG SubSystemType;
    union
    {
        struct
        {
            USHORT SubSystemMinorVersion;
            USHORT SubSystemMajorVersion;
        };
        ULONG SubSystemVersion;
    };
    ULONG GpValue;
    USHORT ImageCharacteristics;
    USHORT DllCharacteristics;
    USHORT Machine;
    BOOLEAN ImageContainsCode;
    union /**< Since Vista, used to be a spare BOOLEAN. */
    {
        struct
        {
            UCHAR ComPlusNativeRead : 1;
            UCHAR ComPlusILOnly : 1;
            UCHAR ImageDynamicallyRelocated : 1;
            UCHAR ImageMAppedFlat : 1;
            UCHAR Reserved : 4;
        };
        UCHAR ImageFlags;
    };
    ULONG LoaderFlags;
    ULONG ImageFileSize; /**< Since XP? */
    ULONG CheckSum; /**< Since Vista, Used to be a reserved/spare ULONG. */
} SECTION_IMAGE_INFORMATION;
typedef SECTION_IMAGE_INFORMATION *PSECTION_IMAGE_INFORMATION;

typedef enum _SECTION_INFORMATION_CLASS
{
    SectionBasicInformation = 0,
    SectionImageInformation,
    MaxSectionInfoClass
} SECTION_INFORMATION_CLASS;
NTSYSAPI NTSTATUS NTAPI NtQuerySection(HANDLE, SECTION_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);

NTSYSAPI NTSTATUS NTAPI NtQueryInformationThread(HANDLE, THREADINFOCLASS, PVOID, ULONG, PULONG);

#ifndef SEC_FILE
# define SEC_FILE               UINT32_C(0x00800000)
#endif
#ifndef SEC_IMAGE
# define SEC_IMAGE              UINT32_C(0x01000000)
#endif
#ifndef SEC_PROTECTED_IMAGE
# define SEC_PROTECTED_IMAGE    UINT32_C(0x02000000)
#endif
#ifndef SEC_NOCACHE
# define SEC_NOCACHE            UINT32_C(0x10000000)
#endif
#ifndef MEM_ROTATE
# define MEM_ROTATE             UINT32_C(0x00800000)
#endif
typedef enum _MEMORY_INFORMATION_CLASS
{
    MemoryBasicInformation = 0,
    MemoryWorkingSetList,
    MemorySectionName,
    MemoryBasicVlmInformation
} MEMORY_INFORMATION_CLASS;
#ifdef IN_RING0
typedef struct _MEMORY_BASIC_INFORMATION
{
    PVOID BaseAddress;
    PVOID AllocationBase;
    ULONG AllocationProtect;
    SIZE_T RegionSize;
    ULONG State;
    ULONG Protect;
    ULONG Type;
} MEMORY_BASIC_INFORMATION;
typedef MEMORY_BASIC_INFORMATION *PMEMORY_BASIC_INFORMATION;
# define NtQueryVirtualMemory ZwQueryVirtualMemory
#endif
NTSYSAPI NTSTATUS NTAPI NtQueryVirtualMemory(HANDLE, void const *, MEMORY_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);

typedef enum _SYSTEM_INFORMATION_CLASS
{
    SystemBasicInformation = 0,
    SystemCpuInformation,
    SystemPerformanceInformation,
    SystemTimeOfDayInformation,
    SystemInformation_Unknown_4,
    SystemProcessInformation,
    SystemInformation_Unknown_6,
    SystemInformation_Unknown_7,
    SystemProcessorPerformanceInformation,
    SystemInformation_Unknown_9,
    SystemInformation_Unknown_10,
    SystemModuleInformation,
    SystemInformation_Unknown_12,
    SystemInformation_Unknown_13,
    SystemInformation_Unknown_14,
    SystemInformation_Unknown_15,
    SystemHandleInformation,
    SystemInformation_Unknown_17,
    SystemPageFileInformation,
    SystemInformation_Unknown_19,
    SystemInformation_Unknown_20,
    SystemCacheInformation,
    SystemInformation_Unknown_22,
    SystemInterruptInformation,
    SystemDpcBehaviourInformation,
    SystemFullMemoryInformation,
    SystemLoadGdiDriverInformation, /* 26 */
    SystemUnloadGdiDriverInformation, /* 27 */
    SystemTimeAdjustmentInformation,
    SystemSummaryMemoryInformation,
    SystemInformation_Unknown_30,
    SystemInformation_Unknown_31,
    SystemInformation_Unknown_32,
    SystemExceptionInformation,
    SystemCrashDumpStateInformation,
    SystemKernelDebuggerInformation,
    SystemContextSwitchInformation,
    SystemRegistryQuotaInformation,
    SystemInformation_Unknown_38,
    SystemInformation_Unknown_39,
    SystemInformation_Unknown_40,
    SystemInformation_Unknown_41,
    SystemInformation_Unknown_42,
    SystemInformation_Unknown_43,
    SystemCurrentTimeZoneInformation,
    SystemLookasideInformation,
    SystemSetTimeSlipEvent,
    SystemCreateSession,
    SystemDeleteSession,
    SystemInformation_Unknown_49,
    SystemRangeStartInformation,
    SystemVerifierInformation,
    SystemInformation_Unknown_52,
    SystemSessionProcessInformation,
    SystemLoadGdiDriverInSystemSpaceInformation, /* 54 */
    SystemInformation_Unknown_55,
    SystemInformation_Unknown_56,
    SystemExtendedProcessInformation,
    SystemInformation_Unknown_58,
    SystemInformation_Unknown_59,
    SystemInformation_Unknown_60,
    SystemInformation_Unknown_61,
    SystemInformation_Unknown_62,
    SystemInformation_Unknown_63,
    SystemExtendedHandleInformation, /* 64 */

    /** @todo fill gap. they've added a whole bunch of things  */
    SystemPolicyInformation = 134,
    SystemInformationClassMax
} SYSTEM_INFORMATION_CLASS;

#ifdef IPRT_NT_USE_WINTERNL
typedef struct _VM_COUNTERS
{
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} VM_COUNTERS;
typedef VM_COUNTERS *PVM_COUNTERS;
#endif

#if 0
typedef struct _IO_COUNTERS
{
    ULONGLONG ReadOperationCount;
    ULONGLONG WriteOperationCount;
    ULONGLONG OtherOperationCount;
    ULONGLONG ReadTransferCount;
    ULONGLONG WriteTransferCount;
    ULONGLONG OtherTransferCount;
} IO_COUNTERS;
typedef IO_COUNTERS *PIO_COUNTERS;
#endif

typedef struct _RTNT_SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;          /**< 0x00 / 0x00 */
    ULONG NumberOfThreads;          /**< 0x04 / 0x04 */
    LARGE_INTEGER Reserved1[3];     /**< 0x08 / 0x08 */
    LARGE_INTEGER CreationTime;     /**< 0x20 / 0x20 */
    LARGE_INTEGER UserTime;         /**< 0x28 / 0x28 */
    LARGE_INTEGER KernelTime;       /**< 0x30 / 0x30 */
    UNICODE_STRING ProcessName;     /**< 0x38 / 0x38 Clean unicode encoding? */
    int32_t BasePriority;           /**< 0x40 / 0x48 */
    HANDLE UniqueProcessId;         /**< 0x44 / 0x50 */
    HANDLE ParentProcessId;         /**< 0x48 / 0x58 */
    ULONG HandleCount;              /**< 0x4c / 0x60 */
    ULONG Reserved2;                /**< 0x50 / 0x64 Session ID? */
    ULONG_PTR Reserved3;            /**< 0x54 / 0x68 */
    VM_COUNTERS VmCounters;         /**< 0x58 / 0x70 */
    IO_COUNTERS IoCounters;         /**< 0x88 / 0xd0 Might not be present in earlier windows versions. */
    /* After this follows the threads, then the ProcessName.Buffer. */
} RTNT_SYSTEM_PROCESS_INFORMATION;
typedef RTNT_SYSTEM_PROCESS_INFORMATION *PRTNT_SYSTEM_PROCESS_INFORMATION;
#ifndef IPRT_NT_USE_WINTERNL
typedef RTNT_SYSTEM_PROCESS_INFORMATION SYSTEM_PROCESS_INFORMATION ;
typedef SYSTEM_PROCESS_INFORMATION *PSYSTEM_PROCESS_INFORMATION;
#endif

typedef struct _SYSTEM_HANDLE_ENTRY_INFO
{
    USHORT UniqueProcessId;
    USHORT CreatorBackTraceIndex;
    UCHAR ObjectTypeIndex;
    UCHAR HandleAttributes;
    USHORT HandleValue;
    PVOID Object;
    ULONG GrantedAccess;
} SYSTEM_HANDLE_ENTRY_INFO;
typedef SYSTEM_HANDLE_ENTRY_INFO *PSYSTEM_HANDLE_ENTRY_INFO;

/** Returned by SystemHandleInformation  */
typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG NumberOfHandles;
    SYSTEM_HANDLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION;
typedef SYSTEM_HANDLE_INFORMATION *PSYSTEM_HANDLE_INFORMATION;

/** Extended handle information entry.
 * @remarks 3 x PVOID + 4 x ULONG = 28 bytes on 32-bit / 40 bytes on 64-bit  */
typedef struct _SYSTEM_HANDLE_ENTRY_INFO_EX
{
    PVOID Object;
    HANDLE UniqueProcessId;
    HANDLE HandleValue;
    ACCESS_MASK GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
} SYSTEM_HANDLE_ENTRY_INFO_EX;
typedef SYSTEM_HANDLE_ENTRY_INFO_EX *PSYSTEM_HANDLE_ENTRY_INFO_EX;

/** Returned by SystemExtendedHandleInformation.  */
typedef struct _SYSTEM_HANDLE_INFORMATION_EX
{
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    SYSTEM_HANDLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX;
typedef SYSTEM_HANDLE_INFORMATION_EX *PSYSTEM_HANDLE_INFORMATION_EX;

/** Input to SystemSessionProcessInformation. */
typedef struct _SYSTEM_SESSION_PROCESS_INFORMATION
{
    ULONG SessionId;
    ULONG BufferLength;
    /** Return buffer, SYSTEM_PROCESS_INFORMATION entries. */
    PVOID Buffer;
} SYSTEM_SESSION_PROCESS_INFORMATION;
typedef SYSTEM_SESSION_PROCESS_INFORMATION *PSYSTEM_SESSION_PROCESS_INFORMATION;

NTSYSAPI NTSTATUS NTAPI NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

NTSYSAPI NTSTATUS NTAPI NtDelayExecution(BOOLEAN, PLARGE_INTEGER);
NTSYSAPI NTSTATUS NTAPI NtYieldExecution(void);

NTSYSAPI NTSTATUS NTAPI RtlAddAccessDeniedAce(PACL, ULONG, ULONG, PSID);

RT_C_DECLS_END
/** @} */


#if defined(IN_RING0) || defined(DOXYGEN_RUNNING)
/** @name NT Kernel APIs
 * @{ */
NTSYSAPI BOOLEAN  NTAPI ObFindHandleForObject(PEPROCESS pProcess, PVOID pvObject, POBJECT_TYPE pObjectType,
                                              PVOID pvOptionalConditions, PHANDLE phFound);
NTSYSAPI NTSTATUS NTAPI ObReferenceObjectByName(PUNICODE_STRING pObjectPath, ULONG fAttributes, PACCESS_STATE pAccessState,
                                                ACCESS_MASK fDesiredAccess, POBJECT_TYPE pObjectType,
                                                KPROCESSOR_MODE enmAccessMode, PVOID pvParseContext, PVOID *ppvObject);
NTSYSAPI HANDLE   NTAPI PsGetProcessInheritedFromUniqueProcessId(PEPROCESS);
NTSYSAPI UCHAR *  NTAPI PsGetProcessImageFileName(PEPROCESS);
NTSYSAPI BOOLEAN  NTAPI PsIsProcessBeingDebugged(PEPROCESS);
NTSYSAPI ULONG    NTAPI PsGetProcessSessionId(PEPROCESS);
extern DECLIMPORT(POBJECT_TYPE *) LpcPortObjectType;            /**< In vista+ this is the ALPC port object type. */
extern DECLIMPORT(POBJECT_TYPE *) LpcWaitablePortObjectType;    /**< In vista+ this is the ALPC port object type. */

/** @ */
#endif /* IN_RING0 */

#endif

