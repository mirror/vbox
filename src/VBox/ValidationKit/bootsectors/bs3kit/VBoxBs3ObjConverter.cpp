/* $Id$ */
/** @file
 * VirtualBox Validation Kit - Boot Sector 3 object file convert.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iprt/types.h>
#include <iprt/assert.h>

#include <iprt/formats/elf64.h>
#include <iprt/formats/elf-amd64.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Verbosity level. */
static unsigned g_cVerbose = 0;


/**
 * Opens a file for binary reading or writing.
 *
 * @returns File stream handle.
 * @param   pszFile             The name of the file.
 * @param   fWrite              Whether to open for writing or reading.
 */
static FILE *openfile(const char *pszFile,  bool fWrite)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    FILE *pFile = fopen(pszFile, fWrite ? "wb" : "rb");
#else
    FILE *pFile = fopen(pszFile, fWrite ? "w" : "r");
#endif
    if (!pFile)
        fprintf(stderr, "error: Failed to open '%s' for %s: %s (%d)\n",
                pszFile, fWrite ? "writing" : "reading", strerror(errno), errno);
    return pFile;
}


/**
 * Read the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to read.
 * @param   ppvFile     Where to return the memory.
 * @param   pcbFile     Where to return the size.
 */
static bool readfile(const char *pszFile, void **ppvFile, size_t *pcbFile)
{
    FILE *pFile = openfile(pszFile, false);
    if (pFile)
    {
        /*
         * Figure the size.
         */
        if (fseek(pFile, 0, SEEK_END) == 0)
        {
            long cbFile = ftell(pFile);
            if (cbFile > 0)
            {
                if (fseek(pFile, SEEK_SET, 0) == 0)
                {
                    /*
                     * Allocate and read content.
                     */
                    void *pvFile = malloc((size_t)cbFile);
                    if (pvFile)
                    {
                        if (fread(pvFile, cbFile, 1, pFile) == 1)
                        {
                            *ppvFile = pvFile;
                            *pcbFile = (size_t)cbFile;
                            fclose(pFile);
                            return true;
                        }
                        free(pvFile);
                        fprintf(stderr, "error: fread failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
                    }
                    else
                        fprintf(stderr, "error: failed to allocate %ld bytes of memory for '%s'\n", cbFile, pszFile);
                }
                else
                    fprintf(stderr, "error: fseek #2 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
            }
            else
                fprintf(stderr, "error: ftell failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        }
        else
            fprintf(stderr, "error: fseek #1 failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Write the given file into memory.
 *
 * @returns true on success, false on failure.
 * @param   pszFile     The file to write.
 * @param   pvFile      Where to return the memory.
 * @param   cbFile      Where to return the size.
 */
static bool writefile(const char *pszFile, void const *pvFile, size_t cbFile)
{
    remove(pszFile);

    int rc = -1;
    FILE *pFile = openfile(pszFile, true);
    if (pFile)
    {
        if (fwrite(pvFile, cbFile, 1, pFile) == 1)
        {
            fclose(pFile);
            return true;
        }
        fprintf(stderr, "error: fwrite failed in '%s': %s (%d)\n", pszFile, strerror(errno), errno);
        fclose(pFile);
    }
    return false;
}


/**
 * Reports an error and returns false.
 *
 * @returns false
 * @param   pszFile             The filename.
 * @param   pszFormat           The message format string.
 * @param   ...                 Format arguments.
 */
static bool error(const char *pszFile, const char *pszFormat, ...)
{
    fprintf(stderr, "error: %s: ", pszFile);
    va_list va;
    va_start(va, pszFormat);
    vfprintf(stderr, pszFormat, va);
    va_end(va);
    return false;
}


/** AMD64 relocation type names for ELF. */
static const char * const g_apszElfAmd64RelTypes[] =
{
    "R_X86_64_NONE",
    "R_X86_64_64",
    "R_X86_64_PC32",
    "R_X86_64_GOT32",
    "R_X86_64_PLT32",
    "R_X86_64_COPY",
    "R_X86_64_GLOB_DAT",
    "R_X86_64_JMP_SLOT",
    "R_X86_64_RELATIVE",
    "R_X86_64_GOTPCREL",
    "R_X86_64_32",
    "R_X86_64_32S",
    "R_X86_64_16",
    "R_X86_64_PC16",
    "R_X86_64_8",
    "R_X86_64_PC8",
    "R_X86_64_DTPMOD64",
    "R_X86_64_DTPOFF64",
    "R_X86_64_TPOFF64",
    "R_X86_64_TLSGD",
    "R_X86_64_TLSLD",
    "R_X86_64_DTPOFF32",
    "R_X86_64_GOTTPOFF",
    "R_X86_64_TPOFF32",
};


static bool convertelf(const char *pszFile, uint8_t *pbFile, size_t cbFile)
{
    /*
     * Validate the header and our other expectations.
     */
    Elf64_Ehdr const *pEhdr = (Elf64_Ehdr const *)pbFile;
    if (   pEhdr->e_ident[EI_CLASS] != ELFCLASS64
        || pEhdr->e_ident[EI_DATA]  != ELFDATA2LSB
        || pEhdr->e_ehsize          != sizeof(Elf64_Ehdr)
        || pEhdr->e_shentsize       != sizeof(Elf64_Shdr)
        || pEhdr->e_version         != EV_CURRENT )
        return error(pszFile, "Unsupported ELF config\n");
    if (pEhdr->e_type != ET_REL)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_type);
    if (pEhdr->e_machine != EM_X86_64)
        return error(pszFile, "Expected relocatable ELF file (e_type=%d)\n", pEhdr->e_machine);
    if (pEhdr->e_phnum != 0)
        return error(pszFile, "Expected e_phnum to be zero not %u\n", pEhdr->e_phnum);
    if (pEhdr->e_shnum < 2)
        return error(pszFile, "Expected e_shnum to be two or higher\n");
    if (pEhdr->e_shstrndx >= pEhdr->e_shnum || pEhdr->e_shstrndx == 0)
        return error(pszFile, "Bad e_shstrndx=%u (e_shnum=%u)\n", pEhdr->e_shstrndx, pEhdr->e_shnum);
    if (   pEhdr->e_shoff >= cbFile
        || pEhdr->e_shoff + pEhdr->e_shnum * sizeof(Elf64_Shdr) > cbFile)
        return error(pszFile, "Section table is outside the file (e_shoff=%#llx, e_shnum=%u, cbFile=%#llx)\n",
                     pEhdr->e_shstrndx, pEhdr->e_shnum, (uint64_t)cbFile);

    /*
     * Locate the section name string table.
     * We assume it's okay as we only reference it in verbose mode.
     */
    Elf64_Shdr const *paShdrs = (Elf64_Shdr const *)&pbFile[pEhdr->e_shoff];
    const char * pszStrTab = (const char *)&pbFile[paShdrs[pEhdr->e_shstrndx].sh_offset];

    /*
     * Work the section table.
     */
    for (uint32_t i = 1; i < pEhdr->e_shnum; i++)
    {
        if (g_cVerbose)
            printf("shdr[%u]: name=%#x '%s' type=%#x flags=%#llx addr=%#llx off=%#llx size=%#llx\n"
                   "          link=%u info=%#x align=%#llx entsize=%#llx\n",
                   i, paShdrs[i].sh_name, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_type, paShdrs[i].sh_flags,
                   paShdrs[i].sh_addr, paShdrs[i].sh_offset, paShdrs[i].sh_size,
                   paShdrs[i].sh_link, paShdrs[i].sh_info, paShdrs[i].sh_addralign, paShdrs[i].sh_entsize);
        if (paShdrs[i].sh_type == SHT_RELA)
        {
            if (paShdrs[i].sh_entsize != sizeof(Elf64_Rela))
                return error(pszFile, "Expected sh_entsize to be %u not %u for section #%u (%s)\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, &pszStrTab[paShdrs[i].sh_name]);
            uint32_t const cRelocs = paShdrs[i].sh_size / sizeof(Elf64_Rela);
            if (cRelocs * sizeof(Elf64_Rela) != paShdrs[i].sh_size)
                return error(pszFile, "Uneven relocation entry count in #%u (%s): sh_size=%#llx\n", (unsigned)sizeof(Elf64_Rela),
                             paShdrs[i].sh_entsize, i, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_size);
            if (   paShdrs[i].sh_offset > cbFile
                || paShdrs[i].sh_size  >= cbFile
                || paShdrs[i].sh_offset + paShdrs[i].sh_size > cbFile)
                return error(pszFile, "The content of section #%u '%s' is outside the file (%#llx LB %#llx, cbFile=%#lx)\n",
                             i, &pszStrTab[paShdrs[i].sh_name], paShdrs[i].sh_offset, paShdrs[i].sh_size, (unsigned long)cbFile);
            Elf64_Rela *paRels = (Elf64_Rela *)&pbFile[paShdrs[i].sh_offset];
            for (uint32_t j = 0; j < cRelocs; j++)
            {
                uint8_t const bType = ELF64_R_TYPE(paRels[j].r_info);
                if (g_cVerbose > 1)
                    printf("%#018llx  %#018llx %s  %+lld\n", paRels[j].r_offset, paRels[j].r_info,
                           bType < RT_ELEMENTS(g_apszElfAmd64RelTypes) ? g_apszElfAmd64RelTypes[bType] : "unknown", paRels[j].r_addend);

                /* Truncate 64-bit wide absolute relocations, ASSUMING that the high bits
                   are already zero and won't be non-zero after calculating the fixup value. */
                if (bType == R_X86_64_64)
                {
                    paRels[j].r_info &= ~(uint64_t)0xff;
                    paRels[j].r_info |= R_X86_64_32;
                }
            }
        }
        else if (paShdrs[i].sh_type == SHT_REL)
            return error(pszFile, "Did not expect SHT_REL sections (#%u '%s')\n", i, &pszStrTab[paShdrs[i].sh_name]);
    }
    return true;
}


/**
 * Does the convertion using convertelf and convertcoff.
 *
 * @returns exit code (0 on success, non-zero on failure)
 * @param   pszFile     The file to convert.
 */
static int convertit(const char *pszFile)
{
    void  *pvFile;
    size_t cbFile;
    if (readfile(pszFile, &pvFile, &cbFile))
    {
        bool fRc = false;
        uint8_t *pbFile = (uint8_t *)pvFile;
        if (   cbFile > sizeof(Elf64_Ehdr)
            && pbFile[0] == ELFMAG0
            && pbFile[1] == ELFMAG1
            && pbFile[2] == ELFMAG2
            && pbFile[3] == ELFMAG3)
            fRc = convertelf(pszFile, pbFile, cbFile);
        else
            fprintf(stderr, "error: Don't recognize format of '%s' (%#x %#x %#x %#x, cbFile=%lu)\n",
                    pszFile, pbFile[0], pbFile[1], pbFile[2], pbFile[3], (unsigned long)cbFile);
        if (fRc)
            fRc = writefile(pszFile, pvFile, cbFile);
        free(pvFile);
        if (fRc)
            return 0;
    }
    return 1;
}


int main(int argc, char **argv)
{
    int rcExit = 0;

    /*
     * Scan the arguments.
     */
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            const char *pszOpt = &argv[i][1];
            if (*pszOpt == '-')
            {
                /* Convert long options to short ones. */
                pszOpt--;
                if (!strcmp(pszOpt, "--verbose"))
                    pszOpt = "v";
                else if (!strcmp(pszOpt, "--version"))
                    pszOpt = "V";
                else if (!strcmp(pszOpt, "--help"))
                    pszOpt = "h";
                else
                {
                    fprintf(stderr, "syntax errro: Unknown options '%s'\n", pszOpt);
                    return 2;
                }
            }

            /* Process the list of short options. */
            while (*pszOpt)
            {
                switch (*pszOpt++)
                {
                    case 'v':
                        g_cVerbose++;
                        break;

                    case 'V':
                        printf("%s\n", "$Revision$");
                        return 0;

                    case '?':
                    case 'h':
                        printf("usage: %s [options] -o <output> <input1> [input2 ... [inputN]]\n",
                               argv[0]);
                        return 0;
                }
            }
        }
        else
        {
            /*
             * File to convert.  Do the job right away.
             */
            rcExit = convertit(argv[i]);
            if (rcExit != 0)
                break;
        }
    }

    return rcExit;
}


