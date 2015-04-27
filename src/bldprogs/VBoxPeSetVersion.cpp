/* $Id$ */
/** @file
 * IPRT - Change the OS and SubSystem version to 4.0 (VS2010 trick).
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef RT_OS_WINDOWS
# include <Windows.h>
#else
# include <iprt/stdint.h>

typedef struct _IMAGE_DOS_HEADER
{
  uint16_t e_magic;
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  int32_t e_lfanew;
} IMAGE_DOS_HEADER,*PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER
{
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} IMAGE_FILE_HEADER,*PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY
{
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY,*PIMAGE_DATA_DIRECTORY;

# define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

typedef struct _IMAGE_OPTIONAL_HEADER
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER32,*PIMAGE_OPTIONAL_HEADER32;

typedef struct _IMAGE_NT_HEADERS
{
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32,*PIMAGE_NT_HEADERS32;

# define IMAGE_NT_SIGNATURE 0x00004550
# define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
# define IMAGE_FILE_MACHINE_I386 0x014c

#endif
#include <stdio.h>
#include <string.h>


/** @todo Rewrite this so it can take options and print out error messages. */
int main(int argc, char **argv)
{
    if (argc != 2)
        return 30;
    FILE *pFile = fopen(argv[1], "r+b");
    if (!pFile)
        return 1;
    IMAGE_DOS_HEADER MzHdr;
    if (fread(&MzHdr, sizeof(MzHdr), 1, pFile) != 1)
        return 2;

    if (fseek(pFile, MzHdr.e_lfanew, SEEK_SET) != 0)
        return 3;

    IMAGE_NT_HEADERS32 NtHdrs;
    if (fread(&NtHdrs, sizeof(NtHdrs), 1, pFile) != 1)
        return 4;
    if (NtHdrs.Signature != IMAGE_NT_SIGNATURE)
        return 5;
    if (NtHdrs.FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return 6;
    if (NtHdrs.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return 7;

    if (NtHdrs.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return 7;

    IMAGE_NT_HEADERS32 NtHdrsNew = NtHdrs;
    if (NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion > 4)
    {
        NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = 4;
        NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = 0;
    }
    if (NtHdrsNew.OptionalHeader.MajorSubsystemVersion > 4)
    {
        NtHdrsNew.OptionalHeader.MajorSubsystemVersion = 4;
        NtHdrsNew.OptionalHeader.MinorSubsystemVersion = 0;
    }

    if (memcmp(&NtHdrsNew, &NtHdrs, sizeof(NtHdrs)))
    {
        /** @todo calc checksum. */
        NtHdrsNew.OptionalHeader.CheckSum = 0;

        if (fseek(pFile, MzHdr.e_lfanew, SEEK_SET) != 0)
            return 10;
        if (fwrite(&NtHdrsNew, sizeof(NtHdrsNew), 1, pFile) != 1)
            return 11;
    }

    if (fclose(pFile) != 0)
        return 29;
    return 0;
}

