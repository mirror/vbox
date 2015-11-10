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
        if (fseek(pFile, SEEK_END, 0) == 0)
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


static bool convertelf(const char *pszFile, uint8_t *pbFile, size_t cbFile)
{
    /*
     * Validate the header.
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

#if 0
    if (    pEhdr->e_phoff < pEhdr->e_ehsize
        &&  !(pEhdr->e_phoff && pEhdr->e_phnum)
        &&  pEhdr->e_phnum)
    {
        Log(("RTLdrELF: %s: The program headers overlap with the ELF header! e_phoff=" FMT_ELF_OFF "\n",
             pszLogName, pEhdr->e_phoff));
        return VERR_BAD_EXE_FORMAT;
    }
    if (    pEhdr->e_phoff + pEhdr->e_phnum * pEhdr->e_phentsize > cbRawImage
        ||  pEhdr->e_phoff + pEhdr->e_phnum * pEhdr->e_phentsize < pEhdr->e_phoff)
    {
        Log(("RTLdrELF: %s: The program headers extends beyond the file! e_phoff=" FMT_ELF_OFF " e_phnum=" FMT_ELF_HALF "\n",
             pszLogName, pEhdr->e_phoff, pEhdr->e_phnum));
        return VERR_BAD_EXE_FORMAT;
    }


    if (    pEhdr->e_shoff < pEhdr->e_ehsize
        &&  !(pEhdr->e_shoff && pEhdr->e_shnum))
    {
        Log(("RTLdrELF: %s: The section headers overlap with the ELF header! e_shoff=" FMT_ELF_OFF "\n",
             pszLogName, pEhdr->e_shoff));
        return VERR_BAD_EXE_FORMAT;
    }
    if (    pEhdr->e_shoff + pEhdr->e_shnum * pEhdr->e_shentsize > cbRawImage
        ||  pEhdr->e_shoff + pEhdr->e_shnum * pEhdr->e_shentsize < pEhdr->e_shoff)
    {
        Log(("RTLdrELF: %s: The section headers extends beyond the file! e_shoff=" FMT_ELF_OFF " e_shnum=" FMT_ELF_HALF "\n",
             pszLogName, pEhdr->e_shoff, pEhdr->e_shnum));
        return VERR_BAD_EXE_FORMAT;
    }

    if (pEhdr->e_shstrndx == 0 || pEhdr->e_shstrndx > pEhdr->e_shnum)
    {
        Log(("RTLdrELF: %s: The section headers string table is out of bounds! e_shstrndx=" FMT_ELF_HALF " e_shnum=" FMT_ELF_HALF "\n",
             pszLogName, pEhdr->e_shstrndx, pEhdr->e_shnum));
        return VERR_BAD_EXE_FORMAT;
    }
#endif

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
            fprintf(stderr, "error: Don't recognize format of '%s'\n", pszFile);
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


