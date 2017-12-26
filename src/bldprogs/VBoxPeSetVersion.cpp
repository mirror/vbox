/* $Id$ */
/** @file
 * IPRT - Change the OS and SubSystem version to 4.0 (VS2010 trick).
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/formats/mz.h>
#include <iprt/formats/pecoff.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char *g_pszFilename;


static int Error(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    char szTmp[1024];
    _vsnprintf(szTmp, sizeof(szTmp), pszFormat, va);
    va_end(va);
    fprintf(stderr, "VBoxPeSetVersion: %s: error: %s\n", g_pszFilename, szTmp);
    return RTEXITCODE_FAILURE;
}


static int UpdateFile(FILE *pFile, bool fNt31, PIMAGE_SECTION_HEADER *ppaShdr)
{
    /*
     * Locate and read the PE header.
     */
    unsigned long offNtHdrs;
    {
        IMAGE_DOS_HEADER MzHdr;
        if (fread(&MzHdr, sizeof(MzHdr), 1, pFile) != 1)
            return Error("Failed to read MZ header: %s", strerror(errno));
        if (MzHdr.e_magic != IMAGE_DOS_SIGNATURE)
            return Error("Invalid MZ magic: %#x", MzHdr.e_magic);
        offNtHdrs = MzHdr.e_lfanew;
    }

    if (fseek(pFile, offNtHdrs, SEEK_SET) != 0)
        return Error("Failed to seek to PE header at %#lx: %s", offNtHdrs, strerror(errno));
    IMAGE_NT_HEADERS32 NtHdrs;
    if (fread(&NtHdrs, sizeof(NtHdrs), 1, pFile) != 1)
        return Error("Failed to read PE header at %#lx: %s", offNtHdrs, strerror(errno));

    /*
     * Validate it a little bit.
     */
    if (NtHdrs.Signature != IMAGE_NT_SIGNATURE)
        return Error("Invalid PE signature: %#x", NtHdrs.Signature);
    if (NtHdrs.FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return Error("Not I386 machine: %#x", NtHdrs.FileHeader.Machine);
    if (NtHdrs.FileHeader.SizeOfOptionalHeader != sizeof(NtHdrs.OptionalHeader))
        return Error("Invalid optional header size: %#x", NtHdrs.FileHeader.SizeOfOptionalHeader);
    if (NtHdrs.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return Error("Invalid optional header magic: %#x", NtHdrs.OptionalHeader.Magic);

    /*
     * Do the header modifications.
     */
    IMAGE_NT_HEADERS32 NtHdrsNew = NtHdrs;
    if (fNt31)
    {
        NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = 1;
        NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = 0;

        NtHdrsNew.OptionalHeader.MajorSubsystemVersion       = 3;
        NtHdrsNew.OptionalHeader.MinorSubsystemVersion       = 10;
    }
    else
    {
        NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = 4;
        NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = 0;

        NtHdrsNew.OptionalHeader.MajorSubsystemVersion       = 4;
        NtHdrsNew.OptionalHeader.MinorSubsystemVersion       = 0;
    }

    if (memcmp(&NtHdrsNew, &NtHdrs, sizeof(NtHdrs)))
    {
        /** @todo calc checksum. */
        NtHdrsNew.OptionalHeader.CheckSum = 0;

        if (fseek(pFile, offNtHdrs, SEEK_SET) != 0)
            return Error("Failed to seek to PE header at %#lx: %s", offNtHdrs, strerror(errno));
        if (fwrite(&NtHdrsNew, sizeof(NtHdrsNew), 1, pFile) != 1)
            return Error("Failed to write PE header at %#lx: %s", offNtHdrs, strerror(errno));
    }

    /*
     * Make the IAT writable for NT 3.1.
     */
    if (   fNt31
        && NtHdrsNew.FileHeader.NumberOfSections > 0
        && NtHdrsNew.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size > 0)
    {
        uint32_t              cbShdrs = sizeof(IMAGE_SECTION_HEADER) * NtHdrsNew.FileHeader.NumberOfSections;
        PIMAGE_SECTION_HEADER paShdrs = (PIMAGE_SECTION_HEADER)calloc(1, cbShdrs);
        if (!paShdrs)
            return Error("Out of memory");
        *ppaShdr = paShdrs;

        unsigned long offShdrs = offNtHdrs
                               + RT_UOFFSETOF(IMAGE_NT_HEADERS32,
                                              OptionalHeader.DataDirectory[NtHdrsNew.OptionalHeader.NumberOfRvaAndSizes]);
        if (fseek(pFile, offShdrs, SEEK_SET) != 0)
            return Error("Failed to seek to section headers at %#lx: %s", offShdrs, strerror(errno));
        if (fread(paShdrs, cbShdrs, 1, pFile) != 1)
            return Error("Failed to read section headers at %#lx: %s", offShdrs, strerror(errno));

        uint32_t const uRvaIat = NtHdrsNew.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress;
        uint32_t       uRvaEnd = NtHdrsNew.OptionalHeader.SizeOfImage;
        uint32_t       i       = NtHdrsNew.FileHeader.NumberOfSections;
        while (i-- > 0)
            if (!(paShdrs[i].Characteristics & IMAGE_SCN_TYPE_NOLOAD))
            {
                uint32_t uRva = paShdrs[i].VirtualAddress;
                if (uRvaIat >= uRva && uRvaIat < uRvaEnd)
                {
                    if (!(paShdrs[i].Characteristics & IMAGE_SCN_MEM_WRITE))
                    {
                        paShdrs[i].Characteristics |= IMAGE_SCN_MEM_WRITE;
                        unsigned long offShdr = offShdrs + i * sizeof(IMAGE_SECTION_HEADER);
                        if (fseek(pFile, offShdr, SEEK_SET) != 0)
                            return Error("Failed to seek to section header #%u at %#lx: %s", i, offShdr, strerror(errno));
                        if (fwrite(&paShdrs[i], sizeof(IMAGE_SECTION_HEADER), 1, pFile) != 1)
                            return Error("Failed to write IAT section header header at %#lx: %s", offShdr, strerror(errno));
                    }
                    break;
                }
                uRvaEnd = uRva;
            }

    }

    return RTEXITCODE_SUCCESS;
}


/** @todo Rewrite this so it can take options and print out error messages. */
int main(int argc, char **argv)
{
    /*
     * Parse arguments.
     */
    if (argc != 2)
    {
        fprintf(stderr, "VBoxPeSetVersion: syntax error: Expected a only single argument!\n");
        return RTEXITCODE_SYNTAX;
    }
    const char *pszFilename = argv[1];
    g_pszFilename = pszFilename;

    /*
     * Process the file.
     */
    int rcExit;
    FILE *pFile = fopen(pszFilename, "r+b");
    if (pFile)
    {
        PIMAGE_SECTION_HEADER paShdrs = NULL;
        rcExit = UpdateFile(pFile, true /*fNt31*/, &paShdrs);
        if (paShdrs)
            free(paShdrs);
        if (fclose(pFile) != 0)
            rcExit = Error("fclose failed on '%s': %s", pszFilename, strerror(errno));
    }
    else
        rcExit = Error("Failed to open '%s' for updating: %s", pszFilename, strerror(errno));
    return rcExit;
}

