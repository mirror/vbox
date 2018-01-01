/* $Id$ */
/** @file
 * IPRT - Change the OS and SubSystem version to value suitable for NT v3.1.
 *
 * Also make sure the IAT is writable, since NT v3.1 expects this.  These are
 * tricks necessary to make binaries created by newer Visual C++ linkers work
 * on ancient NT version like W2K, NT4 and NT 3.x.
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define MK_VER(a_uHi, a_uLo)  ( ((a_uHi) << 8) | (a_uLo))


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char *g_pszFilename;
static unsigned    g_cVerbosity = 0;


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


static void Info(unsigned iLevel, const char *pszFormat, ...)
{
    if (iLevel <= g_cVerbosity)
    {
        va_list va;
        va_start(va, pszFormat);
        char szTmp[1024];
        _vsnprintf(szTmp, sizeof(szTmp), pszFormat, va);
        va_end(va);
        fprintf(stderr, "VBoxPeSetVersion: %s: info: %s\n", g_pszFilename, szTmp);
    }
}


static int UpdateFile(FILE *pFile, unsigned uNtVersion, PIMAGE_SECTION_HEADER *ppaShdr)
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
    NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = uNtVersion >> 8;
    NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = uNtVersion & 0xff;
    NtHdrsNew.OptionalHeader.MajorSubsystemVersion       = uNtVersion >> 8;
    NtHdrsNew.OptionalHeader.MinorSubsystemVersion       = uNtVersion & 0xff;

    if (uNtVersion <= MK_VER(3, 50))
    {
        NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion = 1;
        NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion = 0;
    }

    if (memcmp(&NtHdrsNew, &NtHdrs, sizeof(NtHdrs)))
    {
        /** @todo calc checksum. */
        NtHdrsNew.OptionalHeader.CheckSum = 0;

        if (   NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion != NtHdrs.OptionalHeader.MajorOperatingSystemVersion
            || NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion != NtHdrs.OptionalHeader.MinorOperatingSystemVersion)
            Info(1,"OperatingSystemVersion %u.%u -> %u.%u",
                 NtHdrs.OptionalHeader.MajorOperatingSystemVersion, NtHdrs.OptionalHeader.MinorOperatingSystemVersion,
                 NtHdrsNew.OptionalHeader.MajorOperatingSystemVersion, NtHdrsNew.OptionalHeader.MinorOperatingSystemVersion);
        if (   NtHdrsNew.OptionalHeader.MajorSubsystemVersion != NtHdrs.OptionalHeader.MajorSubsystemVersion
            || NtHdrsNew.OptionalHeader.MinorSubsystemVersion != NtHdrs.OptionalHeader.MinorSubsystemVersion)
            Info(1,"SubsystemVersion %u.%u -> %u.%u",
                 NtHdrs.OptionalHeader.MajorSubsystemVersion, NtHdrs.OptionalHeader.MinorSubsystemVersion,
                 NtHdrsNew.OptionalHeader.MajorSubsystemVersion, NtHdrsNew.OptionalHeader.MinorSubsystemVersion);

        if (fseek(pFile, offNtHdrs, SEEK_SET) != 0)
            return Error("Failed to seek to PE header at %#lx: %s", offNtHdrs, strerror(errno));
        if (fwrite(&NtHdrsNew, sizeof(NtHdrsNew), 1, pFile) != 1)
            return Error("Failed to write PE header at %#lx: %s", offNtHdrs, strerror(errno));
    }

    /*
     * Make the IAT writable for NT 3.1 and drop the non-cachable flag from .bss.
     *
     * The latter is a trick we use to prevent the linker from merging .data and .bss,
     * because NT 3.1 does not honor Misc.VirtualSize and won't zero padd the .bss part
     * if it's not zero padded in the file.  This seemed simpler than adding zero padding.
     */
    if (   uNtVersion <= MK_VER(3, 10)
        && NtHdrsNew.FileHeader.NumberOfSections > 0)
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

        bool     fFoundBss = false;
        uint32_t uRvaEnd   = NtHdrsNew.OptionalHeader.SizeOfImage;
        uint32_t uRvaIat   = NtHdrsNew.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size > 0
                           ? NtHdrsNew.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress : UINT32_MAX;
        uint32_t i         = NtHdrsNew.FileHeader.NumberOfSections;
        while (i-- > 0)
            if (!(paShdrs[i].Characteristics & IMAGE_SCN_TYPE_NOLOAD))
            {
                bool     fModified = false;
                if (uRvaIat >= paShdrs[i].VirtualAddress && uRvaIat < uRvaEnd)
                {
                    if (!(paShdrs[i].Characteristics & IMAGE_SCN_MEM_WRITE))
                    {
                        paShdrs[i].Characteristics |= IMAGE_SCN_MEM_WRITE;
                        fModified = true;
                    }
                    uRvaIat = UINT32_MAX;
                }

                if (   !fFoundBss
                    && strcmp((const char *)paShdrs[i].Name, ".bss") == 0)
                {
                    if (paShdrs[i].Characteristics & IMAGE_SCN_MEM_NOT_CACHED)
                    {
                        paShdrs[i].Characteristics &= ~IMAGE_SCN_MEM_NOT_CACHED;
                        fModified = true;
                    }
                    fFoundBss = true;
                }

                if (fModified)
                {
                    unsigned long offShdr = offShdrs + i * sizeof(IMAGE_SECTION_HEADER);
                    if (fseek(pFile, offShdr, SEEK_SET) != 0)
                        return Error("Failed to seek to section header #%u at %#lx: %s", i, offShdr, strerror(errno));
                    if (fwrite(&paShdrs[i], sizeof(IMAGE_SECTION_HEADER), 1, pFile) != 1)
                        return Error("Failed to write %8.8s section header header at %#lx: %s",
                                     paShdrs[i].Name, offShdr, strerror(errno));
                    if (uRvaIat == UINT32_MAX && fFoundBss)
                        break;
                }

                /* Advance */
                uRvaEnd = paShdrs[i].VirtualAddress;
            }

    }

    return RTEXITCODE_SUCCESS;
}


static int Usage(FILE *pOutput)
{
    fprintf(pOutput,
            "Usage: VBoxPeSetVersion [options] <PE-image>\n"
            "Options:\n"
            "  -v, --verbose\n"
            "    Increases verbosity.\n"
            "  -q, --quiet\n"
            "    Quiet operation (default).\n"
            "  --nt31, --nt350, --nt351, --nt4, --w2k, --xp, --w2k3, --vista,\n"
            "  --w7, --w8, --w81, --w10\n"
            "    Which version to set.  Default: --nt31\n"
            );
    return RTEXITCODE_SYNTAX;
}


/** @todo Rewrite this so it can take options and print out error messages. */
int main(int argc, char **argv)
{
    /*
     * Parse arguments.
     * This stucks
     */
    unsigned    uNtVersion     = MK_VER(3,10);
    const char *pszFilename    = NULL;
    bool        fAcceptOptions = true;
    for (int i = 1; i < argc; i++)
    {
        const char *psz = argv[i];
        if (fAcceptOptions && *psz == '-')
        {
            char ch = psz[1];
            psz += 2;
            if (ch == '-')
            {
                if (!*psz)
                {
                    fAcceptOptions = false;
                    continue;
                }

                if (strcmp(psz, "verbose") == 0)
                    ch = 'v';
                else if (strcmp(psz, "quiet") == 0)
                    ch = 'q';
                else if (strcmp(psz, "help") == 0)
                    ch = 'h';
                else if (strcmp(psz, "version") == 0)
                    ch = 'V';
                else
                {
                    if (strcmp(psz, "nt31") == 0)
                        uNtVersion = MK_VER(3,10);
                    else if (strcmp(psz, "nt350") == 0)
                        uNtVersion = MK_VER(3,50);
                    else if (strcmp(psz, "nt351") == 0)
                        uNtVersion = MK_VER(3,51);
                    else if (strcmp(psz, "nt4") == 0)
                        uNtVersion = MK_VER(4,0);
                    else if (strcmp(psz, "w2k") == 0)
                        uNtVersion = MK_VER(5,0);
                    else if (strcmp(psz, "xp") == 0)
                        uNtVersion = MK_VER(5,1);
                    else if (strcmp(psz, "w2k3") == 0)
                        uNtVersion = MK_VER(5,2);
                    else if (strcmp(psz, "vista") == 0)
                        uNtVersion = MK_VER(6,0);
                    else if (strcmp(psz, "w7") == 0)
                        uNtVersion = MK_VER(6,1);
                    else if (strcmp(psz, "w8") == 0)
                        uNtVersion = MK_VER(6,2);
                    else if (strcmp(psz, "w81") == 0)
                        uNtVersion = MK_VER(6,3);
                    else if (strcmp(psz, "w10") == 0)
                        uNtVersion = MK_VER(10,0);
                    else
                    {
                        fprintf(stderr, "VBoxPeSetVersion: syntax error: Unknown option: --%s\n", psz);
                        return RTEXITCODE_SYNTAX;
                    }
                    continue;
                }
                psz = " ";
            }
            do
            {
                switch (ch)
                {
                    case 'q':
                        g_cVerbosity = 0;
                        break;
                    case 'v':
                        g_cVerbosity++;
                        break;
                    case 'V':
                        printf("2.0\n");
                        return RTEXITCODE_SUCCESS;
                    case 'h':
                        Usage(stdout);
                        return RTEXITCODE_SUCCESS;
                    default:
                        fprintf(stderr, "VBoxPeSetVersion: syntax error: Unknown option: -%c\n", ch ? ch : ' ');
                        return RTEXITCODE_SYNTAX;
                }
            } while ((ch = *psz++) != '\0');

        }
        else if (!pszFilename)
            pszFilename = psz;
        else
        {
            fprintf(stderr, "VBoxPeSetVersion: syntax error: More than one PE-image specified!\n");
            return RTEXITCODE_SYNTAX;
        }
    }

    if (!pszFilename)
    {
        fprintf(stderr, "VBoxPeSetVersion: syntax error: No PE-image specified!\n");
        return RTEXITCODE_SYNTAX;
    }
    g_pszFilename = pszFilename;

    /*
     * Process the file.
     */
    int rcExit;
    FILE *pFile = fopen(pszFilename, "r+b");
    if (pFile)
    {
        PIMAGE_SECTION_HEADER paShdrs = NULL;
        rcExit = UpdateFile(pFile, uNtVersion, &paShdrs);
        if (paShdrs)
            free(paShdrs);
        if (fclose(pFile) != 0)
            rcExit = Error("fclose failed on '%s': %s", pszFilename, strerror(errno));
    }
    else
        rcExit = Error("Failed to open '%s' for updating: %s", pszFilename, strerror(errno));
    return rcExit;
}

