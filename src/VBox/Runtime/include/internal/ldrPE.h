/* $Id$ */
/** @file
 * IPRT - Windows NT PE Structures and Constants.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

#ifndef ___internal_ldrPE_h
#define ___internal_ldrPE_h

#include <iprt/types.h>
#include <iprt/assert.h>

#pragma pack(4) /** @todo Necessary? */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define  IMAGE_NT_SIGNATURE  0x00004550

/* file header */
#define  IMAGE_FILE_MACHINE_I386  0x014c
#define  IMAGE_FILE_MACHINE_AMD64  0x8664

#define  IMAGE_FILE_RELOCS_STRIPPED  0x0001
#define  IMAGE_FILE_EXECUTABLE_IMAGE  0x0002
#define  IMAGE_FILE_LINE_NUMS_STRIPPED  0x0004
#define  IMAGE_FILE_LOCAL_SYMS_STRIPPED  0x0008
#define  IMAGE_FILE_AGGRESIVE_WS_TRIM  0x0010
#define  IMAGE_FILE_LARGE_ADDRESS_AWARE  0x0020
#define  IMAGE_FILE_16BIT_MACHINE  0x0040
#define  IMAGE_FILE_BYTES_REVERSED_LO  0x0080
#define  IMAGE_FILE_32BIT_MACHINE  0x0100
#define  IMAGE_FILE_DEBUG_STRIPPED  0x0200
#define  IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP  0x0400
#define  IMAGE_FILE_NET_RUN_FROM_SWAP  0x0800
#define  IMAGE_FILE_SYSTEM  0x1000
#define  IMAGE_FILE_DLL  0x2000
#define  IMAGE_FILE_UP_SYSTEM_ONLY  0x4000
#define  IMAGE_FILE_BYTES_REVERSED_HI  0x8000


/* optional header */
#define  IMAGE_NT_OPTIONAL_HDR32_MAGIC  0x10B
#define  IMAGE_NT_OPTIONAL_HDR64_MAGIC  0x20B

#define  IMAGE_SUBSYSTEM_UNKNOWN  0x0
#define  IMAGE_SUBSYSTEM_NATIVE  0x1
#define  IMAGE_SUBSYSTEM_WINDOWS_GUI  0x2
#define  IMAGE_SUBSYSTEM_WINDOWS_CUI  0x3
#define  IMAGE_SUBSYSTEM_OS2_GUI  0x4
#define  IMAGE_SUBSYSTEM_OS2_CUI  0x5
#define  IMAGE_SUBSYSTEM_POSIX_CUI  0x7

#define  IMAGE_LIBRARY_PROCESS_INIT  0x0001
#define  IMAGE_LIBRARY_PROCESS_TERM  0x0002
#define  IMAGE_LIBRARY_THREAD_INIT  0x0004
#define  IMAGE_LIBRARY_THREAD_TERM  0x0008
#define  IMAGE_DLLCHARACTERISTICS_NO_ISOLATION  0x0200
#define  IMAGE_DLLCHARACTERISTICS_NO_SEH  0x0400
#define  IMAGE_DLLCHARACTERISTICS_NO_BIND  0x0800
#define  IMAGE_DLLCHARACTERISTICS_WDM_DRIVER  0x2000
#define  IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE  0x8000

#define  IMAGE_NUMBEROF_DIRECTORY_ENTRIES  0x10

#define  IMAGE_DIRECTORY_ENTRY_EXPORT  0x0
#define  IMAGE_DIRECTORY_ENTRY_IMPORT  0x1
#define  IMAGE_DIRECTORY_ENTRY_RESOURCE  0x2
#define  IMAGE_DIRECTORY_ENTRY_EXCEPTION  0x3
#define  IMAGE_DIRECTORY_ENTRY_SECURITY  0x4
#define  IMAGE_DIRECTORY_ENTRY_BASERELOC  0x5
#define  IMAGE_DIRECTORY_ENTRY_DEBUG  0x6
#define  IMAGE_DIRECTORY_ENTRY_ARCHITECTURE  0x7
#define  IMAGE_DIRECTORY_ENTRY_COPYRIGHT IMAGE_DIRECTORY_ENTRY_ARCHITECTURE
#define  IMAGE_DIRECTORY_ENTRY_GLOBALPTR  0x8
#define  IMAGE_DIRECTORY_ENTRY_TLS  0x9
#define  IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG  0xa
#define  IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT  0xb
#define  IMAGE_DIRECTORY_ENTRY_IAT  0xc
#define  IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT  0xd
#define  IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR  0xe


/* section header */
#define  IMAGE_SIZEOF_SHORT_NAME  0x8

#define  IMAGE_SCN_TYPE_REG  0x00000000
#define  IMAGE_SCN_TYPE_DSECT  0x00000001
#define  IMAGE_SCN_TYPE_NOLOAD  0x00000002
#define  IMAGE_SCN_TYPE_GROUP  0x00000004
#define  IMAGE_SCN_TYPE_NO_PAD  0x00000008
#define  IMAGE_SCN_TYPE_COPY  0x00000010

#define  IMAGE_SCN_CNT_CODE  0x00000020
#define  IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define  IMAGE_SCN_CNT_UNINITIALIZED_DATA  0x00000080

#define  IMAGE_SCN_LNK_OTHER  0x00000100
#define  IMAGE_SCN_LNK_INFO  0x00000200
#define  IMAGE_SCN_TYPE_OVER  0x00000400
#define  IMAGE_SCN_LNK_REMOVE  0x00000800
#define  IMAGE_SCN_LNK_COMDAT  0x00001000
#define  IMAGE_SCN_MEM_PROTECTED  0x00004000
#define  IMAGE_SCN_NO_DEFER_SPEC_EXC  0x00004000
#define  IMAGE_SCN_GPREL  0x00008000
#define  IMAGE_SCN_MEM_FARDATA  0x00008000
#define  IMAGE_SCN_MEM_SYSHEAP  0x00010000
#define  IMAGE_SCN_MEM_PURGEABLE  0x00020000
#define  IMAGE_SCN_MEM_16BIT  0x00020000
#define  IMAGE_SCN_MEM_LOCKED  0x00040000
#define  IMAGE_SCN_MEM_PRELOAD  0x00080000

#define  IMAGE_SCN_ALIGN_1BYTES  0x00100000
#define  IMAGE_SCN_ALIGN_2BYTES  0x00200000
#define  IMAGE_SCN_ALIGN_4BYTES  0x00300000
#define  IMAGE_SCN_ALIGN_8BYTES  0x00400000
#define  IMAGE_SCN_ALIGN_16BYTES  0x00500000
#define  IMAGE_SCN_ALIGN_32BYTES  0x00600000
#define  IMAGE_SCN_ALIGN_64BYTES  0x00700000
#define  IMAGE_SCN_ALIGN_128BYTES  0x00800000
#define  IMAGE_SCN_ALIGN_256BYTES  0x00900000
#define  IMAGE_SCN_ALIGN_512BYTES  0x00A00000
#define  IMAGE_SCN_ALIGN_1024BYTES  0x00B00000
#define  IMAGE_SCN_ALIGN_2048BYTES  0x00C00000
#define  IMAGE_SCN_ALIGN_4096BYTES  0x00D00000
#define  IMAGE_SCN_ALIGN_8192BYTES  0x00E00000
#define  IMAGE_SCN_ALIGN_MASK   0x00F00000
#define  IMAGE_SCN_ALIGN_SHIFT  20

#define  IMAGE_SCN_LNK_NRELOC_OVFL  0x01000000
#define  IMAGE_SCN_MEM_DISCARDABLE  0x02000000
#define  IMAGE_SCN_MEM_NOT_CACHED  0x04000000
#define  IMAGE_SCN_MEM_NOT_PAGED  0x08000000
#define  IMAGE_SCN_MEM_SHARED  0x10000000
#define  IMAGE_SCN_MEM_EXECUTE  0x20000000
#define  IMAGE_SCN_MEM_READ  0x40000000
#define  IMAGE_SCN_MEM_WRITE  0x80000000


/* relocations */
#define  IMAGE_REL_BASED_ABSOLUTE  0x0
#define  IMAGE_REL_BASED_HIGH  0x1
#define  IMAGE_REL_BASED_LOW  0x2
#define  IMAGE_REL_BASED_HIGHLOW  0x3
#define  IMAGE_REL_BASED_HIGHADJ  0x4
#define  IMAGE_REL_BASED_MIPS_JMPADDR  0x5
#define  IMAGE_REL_BASED_MIPS_JMPADDR16 0x9
#define  IMAGE_REL_BASED_IA64_IMM64  0x9
#define  IMAGE_REL_BASED_DIR64  0xa
#define  IMAGE_REL_BASED_HIGH3ADJ 0xb


/* imports */
#define  IMAGE_ORDINAL_FLAG32  0x80000000
#define  IMAGE_ORDINAL32(ord)  ((ord) &  0xffff)
#define  IMAGE_SNAP_BY_ORDINAL32(ord)  (!!((ord) & IMAGE_ORDINAL_FLAG32))

#define  IMAGE_ORDINAL_FLAG64  0x8000000000000000ULL
#define  IMAGE_ORDINAL64(ord)  ((ord) &  0xffff)
#define  IMAGE_SNAP_BY_ORDINAL64(ord)  (!!((ord) & IMAGE_ORDINAL_FLAG64))


/* debug dir */
#define  IMAGE_DEBUG_TYPE_UNKNOWN           UINT32_C(0x0)
#define  IMAGE_DEBUG_TYPE_COFF              UINT32_C(0x1)
#define  IMAGE_DEBUG_TYPE_CODEVIEW          UINT32_C(0x2)
#define  IMAGE_DEBUG_TYPE_FPO               UINT32_C(0x3)
#define  IMAGE_DEBUG_TYPE_MISC              UINT32_C(0x4)
#define  IMAGE_DEBUG_TYPE_EXCEPTION         UINT32_C(0x5)
#define  IMAGE_DEBUG_TYPE_FIXUP             UINT32_C(0x6)
#define  IMAGE_DEBUG_TYPE_OMAP_TO_SRC       UINT32_C(0x7)
#define  IMAGE_DEBUG_TYPE_OMAP_FROM_SRC     UINT32_C(0x8)
#define  IMAGE_DEBUG_TYPE_BORLAND           UINT32_C(0x9)
#define  IMAGE_DEBUG_TYPE_RESERVED10        UINT32_C(0x10)

#define  IMAGE_DEBUG_MISC_EXENAME           UINT32_C(1)

/* security directory */
#define  WIN_CERT_REVISION_1_0              UINT16_C(0x0100)
#define  WIN_CERT_REVISION_2_0              UINT16_C(0x0200)

#define  WIN_CERT_TYPE_X509                 UINT16_C(1)
#define  WIN_CERT_TYPE_PKCS_SIGNED_DATA     UINT16_C(2)
#define  WIN_CERT_TYPE_RESERVED_1           UINT16_C(3)
#define  WIN_CERT_TYPE_TS_STACK_SIGNED      UINT16_C(4)
#define  WIN_CERT_TYPE_EFI_PKCS115          UINT16_C(0x0ef0)
#define  WIN_CERT_TYPE_EFI_GUID             UINT16_C(0x0ef1)


/* For .DBG files. */
#define IMAGE_SEPARATE_DEBUG_SIGNATURE  UINT16_C(0x4944)



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct _IMAGE_FILE_HEADER
{
    uint16_t  Machine;                      /**< 0x00 */
    uint16_t  NumberOfSections;             /**< 0x02 */
    uint32_t  TimeDateStamp;                /**< 0x04  */
    uint32_t  PointerToSymbolTable;         /**< 0x08  */
    uint32_t  NumberOfSymbols;              /**< 0x0c  */
    uint16_t  SizeOfOptionalHeader;         /**< 0x10  */
    uint16_t  Characteristics;              /**< 0x12 */
} IMAGE_FILE_HEADER;                        /* size: 0x14 */
AssertCompileSize(IMAGE_FILE_HEADER, 0x14);
typedef IMAGE_FILE_HEADER *PIMAGE_FILE_HEADER;
typedef IMAGE_FILE_HEADER const *PCIMAGE_FILE_HEADER;


typedef struct _IMAGE_DATA_DIRECTORY
{
    uint32_t  VirtualAddress;
    uint32_t  Size;
} IMAGE_DATA_DIRECTORY;
typedef IMAGE_DATA_DIRECTORY *PIMAGE_DATA_DIRECTORY;
typedef IMAGE_DATA_DIRECTORY const *PCIMAGE_DATA_DIRECTORY;


typedef struct _IMAGE_OPTIONAL_HEADER32
{
    uint16_t  Magic;                        /**< 0x00 */
    uint8_t   MajorLinkerVersion;           /**< 0x02 */
    uint8_t   MinorLinkerVersion;           /**< 0x03 */
    uint32_t  SizeOfCode;                   /**< 0x04 */
    uint32_t  SizeOfInitializedData;        /**< 0x08 */
    uint32_t  SizeOfUninitializedData;      /**< 0x0c */
    uint32_t  AddressOfEntryPoint;          /**< 0x10 */
    uint32_t  BaseOfCode;                   /**< 0x14 */
    uint32_t  BaseOfData;                   /**< 0x18 */
    uint32_t  ImageBase;                    /**< 0x1c */
    uint32_t  SectionAlignment;             /**< 0x20 */
    uint32_t  FileAlignment;                /**< 0x24 */
    uint16_t  MajorOperatingSystemVersion;  /**< 0x28 */
    uint16_t  MinorOperatingSystemVersion;  /**< 0x2a */
    uint16_t  MajorImageVersion;            /**< 0x2c */
    uint16_t  MinorImageVersion;            /**< 0x2e */
    uint16_t  MajorSubsystemVersion;        /**< 0x30 */
    uint16_t  MinorSubsystemVersion;        /**< 0x32 */
    uint32_t  Win32VersionValue;            /**< 0x34 */
    uint32_t  SizeOfImage;                  /**< 0x38 */
    uint32_t  SizeOfHeaders;                /**< 0x3c */
    uint32_t  CheckSum;                     /**< 0x40 */
    uint16_t  Subsystem;                    /**< 0x44 */
    uint16_t  DllCharacteristics;           /**< 0x46 */
    uint32_t  SizeOfStackReserve;           /**< 0x48 */
    uint32_t  SizeOfStackCommit;            /**< 0x4c */
    uint32_t  SizeOfHeapReserve;            /**< 0x50 */
    uint32_t  SizeOfHeapCommit;             /**< 0x54 */
    uint32_t  LoaderFlags;                  /**< 0x58 */
    uint32_t  NumberOfRvaAndSizes;          /**< 0x5c */
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; /**< 0x60; 0x10*8 = 0x80 */
} IMAGE_OPTIONAL_HEADER32;                  /* size: 0xe0 */
AssertCompileSize(IMAGE_OPTIONAL_HEADER32, 0xe0);
typedef IMAGE_OPTIONAL_HEADER32 *PIMAGE_OPTIONAL_HEADER32;
typedef IMAGE_OPTIONAL_HEADER32 const *PCIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_OPTIONAL_HEADER64
{
    uint16_t  Magic;                        /**< 0x00 */
    uint8_t   MajorLinkerVersion;           /**< 0x02 */
    uint8_t   MinorLinkerVersion;           /**< 0x03 */
    uint32_t  SizeOfCode;                   /**< 0x04 */
    uint32_t  SizeOfInitializedData;        /**< 0x08 */
    uint32_t  SizeOfUninitializedData;      /**< 0x0c */
    uint32_t  AddressOfEntryPoint;          /**< 0x10 */
    uint32_t  BaseOfCode;                   /**< 0x14 */
    uint64_t  ImageBase;                    /**< 0x18 */
    uint32_t  SectionAlignment;             /**< 0x20 */
    uint32_t  FileAlignment;                /**< 0x24 */
    uint16_t  MajorOperatingSystemVersion;  /**< 0x28 */
    uint16_t  MinorOperatingSystemVersion;  /**< 0x2a */
    uint16_t  MajorImageVersion;            /**< 0x2c */
    uint16_t  MinorImageVersion;            /**< 0x2e */
    uint16_t  MajorSubsystemVersion;        /**< 0x30 */
    uint16_t  MinorSubsystemVersion;        /**< 0x32 */
    uint32_t  Win32VersionValue;            /**< 0x34 */
    uint32_t  SizeOfImage;                  /**< 0x38 */
    uint32_t  SizeOfHeaders;                /**< 0x3c */
    uint32_t  CheckSum;                     /**< 0x40 */
    uint16_t  Subsystem;                    /**< 0x44 */
    uint16_t  DllCharacteristics;           /**< 0x46 */
    uint64_t  SizeOfStackReserve;           /**< 0x48 */
    uint64_t  SizeOfStackCommit;            /**< 0x50 */
    uint64_t  SizeOfHeapReserve;            /**< 0x58 */
    uint64_t  SizeOfHeapCommit;             /**< 0x60 */
    uint32_t  LoaderFlags;                  /**< 0x68 */
    uint32_t  NumberOfRvaAndSizes;          /**< 0x6c */
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; /**< 0x70; 0x10*8 = 0x80 */
} IMAGE_OPTIONAL_HEADER64;                  /* size: 0xf0 */
typedef IMAGE_OPTIONAL_HEADER64 *PIMAGE_OPTIONAL_HEADER64;
typedef IMAGE_OPTIONAL_HEADER64 const *PCIMAGE_OPTIONAL_HEADER64;


typedef struct _IMAGE_NT_HEADERS
{
    uint32_t  Signature;                    /**< 0x00 */
    IMAGE_FILE_HEADER FileHeader;           /**< 0x04 */
    IMAGE_OPTIONAL_HEADER32 OptionalHeader; /**< 0x18 */
} IMAGE_NT_HEADERS32;                       /* size:  0xf8 */
AssertCompileSize(IMAGE_NT_HEADERS32, 0xf8);
AssertCompileMemberOffset(IMAGE_NT_HEADERS32, FileHeader, 4);
AssertCompileMemberOffset(IMAGE_NT_HEADERS32, OptionalHeader, 24);
typedef IMAGE_NT_HEADERS32 *PIMAGE_NT_HEADERS32;
typedef IMAGE_NT_HEADERS32 const *PCIMAGE_NT_HEADERS32;

typedef struct _IMAGE_NT_HEADERS64
{
    uint32_t  Signature;                    /**< 0x00 */
    IMAGE_FILE_HEADER FileHeader;           /**< 0x04 */
    IMAGE_OPTIONAL_HEADER64 OptionalHeader; /**< 0x18 */
} IMAGE_NT_HEADERS64;                       /**< 0x108 */
AssertCompileSize(IMAGE_NT_HEADERS64, 0x108);
AssertCompileMemberOffset(IMAGE_NT_HEADERS64, FileHeader, 4);
AssertCompileMemberOffset(IMAGE_NT_HEADERS64, OptionalHeader, 24);
typedef IMAGE_NT_HEADERS64 *PIMAGE_NT_HEADERS64;
typedef IMAGE_NT_HEADERS64 const *PCIMAGE_NT_HEADERS64;


typedef struct _IMAGE_SECTION_HEADER
{
    uint8_t  Name[IMAGE_SIZEOF_SHORT_NAME];
    union
    {
        uint32_t  PhysicalAddress;
        uint32_t  VirtualSize;
    } Misc;
    uint32_t  VirtualAddress;
    uint32_t  SizeOfRawData;
    uint32_t  PointerToRawData;
    uint32_t  PointerToRelocations;
    uint32_t  PointerToLinenumbers;
    uint16_t  NumberOfRelocations;
    uint16_t  NumberOfLinenumbers;
    uint32_t  Characteristics;
} IMAGE_SECTION_HEADER;
typedef IMAGE_SECTION_HEADER *PIMAGE_SECTION_HEADER;
typedef IMAGE_SECTION_HEADER const *PCIMAGE_SECTION_HEADER;


typedef struct _IMAGE_BASE_RELOCATION
{
    uint32_t  VirtualAddress;
    uint32_t  SizeOfBlock;
} IMAGE_BASE_RELOCATION;
typedef IMAGE_BASE_RELOCATION *PIMAGE_BASE_RELOCATION;
typedef IMAGE_BASE_RELOCATION const *PCIMAGE_BASE_RELOCATION;


typedef struct _IMAGE_EXPORT_DIRECTORY
{
    uint32_t  Characteristics;
    uint32_t  TimeDateStamp;
    uint16_t  MajorVersion;
    uint16_t  MinorVersion;
    uint32_t  Name;
    uint32_t  Base;
    uint32_t  NumberOfFunctions;
    uint32_t  NumberOfNames;
    uint32_t  AddressOfFunctions;
    uint32_t  AddressOfNames;
    uint32_t  AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY;
typedef IMAGE_EXPORT_DIRECTORY *PIMAGE_EXPORT_DIRECTORY;
typedef IMAGE_EXPORT_DIRECTORY const *PCIMAGE_EXPORT_DIRECTORY;


typedef struct _IMAGE_IMPORT_DESCRIPTOR
{
    union
    {
        uint32_t  Characteristics;
        uint32_t  OriginalFirstThunk;
    } u;
    uint32_t  TimeDateStamp;
    uint32_t  ForwarderChain;
    uint32_t  Name;
    uint32_t  FirstThunk;
} IMAGE_IMPORT_DESCRIPTOR;
typedef IMAGE_IMPORT_DESCRIPTOR *PIMAGE_IMPORT_DESCRIPTOR;
typedef IMAGE_IMPORT_DESCRIPTOR const *PCIMAGE_IMPORT_DESCRIPTOR;


typedef struct _IMAGE_IMPORT_BY_NAME
{
    uint16_t  Hint;
    uint8_t   Name[1];
} IMAGE_IMPORT_BY_NAME;
typedef IMAGE_IMPORT_BY_NAME *PIMAGE_IMPORT_BY_NAME;
typedef IMAGE_IMPORT_BY_NAME const *PCIMAGE_IMPORT_BY_NAME;


/* The image_thunk_data32/64 structures are not very helpful except for getting RSI. keep them around till all the code has been converted. */
typedef struct _IMAGE_THUNK_DATA64
{
    union
    {
        uint64_t  ForwarderString;
        uint64_t  Function;
        uint64_t  Ordinal;
        uint64_t  AddressOfData;
    } u1;
} IMAGE_THUNK_DATA64;
typedef IMAGE_THUNK_DATA64 *PIMAGE_THUNK_DATA64;
typedef IMAGE_THUNK_DATA64 const *PCIMAGE_THUNK_DATA64;

typedef struct _IMAGE_THUNK_DATA32
{
    union
    {
        uint32_t  ForwarderString;
        uint32_t  Function;
        uint32_t  Ordinal;
        uint32_t  AddressOfData;
    } u1;
} IMAGE_THUNK_DATA32;
typedef IMAGE_THUNK_DATA32 *PIMAGE_THUNK_DATA32;
typedef IMAGE_THUNK_DATA32 const *PCIMAGE_THUNK_DATA32;


typedef struct _IMAGE_LOAD_CONFIG_DIRECTORY32
{
    uint32_t  Size;
    uint32_t  TimeDateStamp;
    uint16_t  MajorVersion;
    uint16_t  MinorVersion;
    uint32_t  GlobalFlagsClear;
    uint32_t  GlobalFlagsSet;
    uint32_t  CriticalSectionDefaultTimeout;
    uint32_t  DeCommitFreeBlockThreshold;
    uint32_t  DeCommitTotalFreeThreshold;
    uint32_t  LockPrefixTable;
    uint32_t  MaximumAllocationSize;
    uint32_t  VirtualMemoryThreshold;
    uint32_t  ProcessHeapFlags;
    uint32_t  ProcessAffinityMask;
    uint16_t  CSDVersion;
    uint16_t  Reserved1;
    uint32_t  EditList;
    uint32_t  SecurityCookie;
    uint32_t  SEHandlerTable;
    uint32_t  SEHandlerCount;
} IMAGE_LOAD_CONFIG_DIRECTORY32;
typedef IMAGE_LOAD_CONFIG_DIRECTORY32 *PIMAGE_LOAD_CONFIG_DIRECTORY32;
typedef IMAGE_LOAD_CONFIG_DIRECTORY32 const *PCIMAGE_LOAD_CONFIG_DIRECTORY32;

typedef struct _IMAGE_LOAD_CONFIG_DIRECTORY64
{
    uint32_t  Size;
    uint32_t  TimeDateStamp;
    uint16_t  MajorVersion;
    uint16_t  MinorVersion;
    uint32_t  GlobalFlagsClear;
    uint32_t  GlobalFlagsSet;
    uint32_t  CriticalSectionDefaultTimeout;
    uint64_t  DeCommitFreeBlockThreshold;
    uint64_t  DeCommitTotalFreeThreshold;
    uint64_t  LockPrefixTable;
    uint64_t  MaximumAllocationSize;
    uint64_t  VirtualMemoryThreshold;
    uint64_t  ProcessAffinityMask;
    uint32_t  ProcessHeapFlags;
    uint16_t  CSDVersion;
    uint16_t  Reserved1;
    uint64_t  EditList;
    uint64_t  SecurityCookie;
    uint64_t  SEHandlerTable;
    uint64_t  SEHandlerCount;
} IMAGE_LOAD_CONFIG_DIRECTORY64;
typedef IMAGE_LOAD_CONFIG_DIRECTORY64 *PIMAGE_LOAD_CONFIG_DIRECTORY64;
typedef IMAGE_LOAD_CONFIG_DIRECTORY64 const *PCIMAGE_LOAD_CONFIG_DIRECTORY64;


typedef struct _IMAGE_DEBUG_DIRECTORY
{
    uint32_t  Characteristics;
    uint32_t  TimeDateStamp;
    uint16_t  MajorVersion;
    uint16_t  MinorVersion;
    uint32_t  Type;
    uint32_t  SizeOfData;
    uint32_t  AddressOfRawData;
    uint32_t  PointerToRawData;
} IMAGE_DEBUG_DIRECTORY;
typedef IMAGE_DEBUG_DIRECTORY *PIMAGE_DEBUG_DIRECTORY;
typedef IMAGE_DEBUG_DIRECTORY const *PCIMAGE_DEBUG_DIRECTORY;

typedef struct _IMAGE_DEBUG_MISC
{
    uint32_t   DataType;
    uint32_t   Length;
    uint8_t    Unicode;
    uint8_t    Reserved[3];
    uint8_t    Data[1];
} IMAGE_DEBUG_MISC;
typedef IMAGE_DEBUG_MISC *PIMAGE_DEBUG_MISC;
typedef IMAGE_DEBUG_MISC const *PCIMAGE_DEBUG_MISC;


typedef struct WIN_CERTIFICATE
{
    uint32_t    dwLength;
    uint16_t    wRevision;
    uint16_t    wCertificateType;
    uint8_t     bCertificate[8];
} WIN_CERTIFICATE;
typedef WIN_CERTIFICATE *PWIN_CERTIFICATE;
typedef WIN_CERTIFICATE const *PCWIN_CERTIFICATE;


/** The header of a .DBG file (NT4). */
typedef struct _IMAGE_SEPARATE_DEBUG_HEADER
{
    uint16_t    Signature;              /**< 0x00 */
    uint16_t    Flags;                  /**< 0x02 */
    uint16_t    Machine;                /**< 0x04 */
    uint16_t    Characteristics;        /**< 0x06 */
    uint32_t    TimeDateStamp;          /**< 0x08 */
    uint32_t    CheckSum;               /**< 0x0c */
    uint32_t    ImageBase;              /**< 0x10 */
    uint32_t    SizeOfImage;            /**< 0x14 */
    uint32_t    NumberOfSections;       /**< 0x18 */
    uint32_t    ExportedNamesSize;      /**< 0x1c */
    uint32_t    DebugDirectorySize;     /**< 0x20 */
    uint32_t    SectionAlignment;       /**< 0x24 */
    uint32_t    Reserved[2];            /**< 0x28 */
} IMAGE_SEPARATE_DEBUG_HEADER;          /* size: 0x30 */
typedef IMAGE_SEPARATE_DEBUG_HEADER *PIMAGE_SEPARATE_DEBUG_HEADER;
typedef IMAGE_SEPARATE_DEBUG_HEADER const *PCIMAGE_SEPARATE_DEBUG_HEADER;


#pragma pack()

#endif

