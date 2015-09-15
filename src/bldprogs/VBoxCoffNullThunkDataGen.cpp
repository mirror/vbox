/* $Id$ */
/** @file
 * VBoxCoffNullThunkDataGen - Generates a <dll>_NULL_THUNK_DATA object.
 *                                                                    .
 * This differs from the on generated automatically by the Microsoft linker and
 * librarian by that the 8 bytes aren't zeros.  It's a shot at avoid
 * VERR_LDR_MISMATCH_NATIVE problems with some Windows 10 systems.  An unknown
 * party has been observed putting some kind of pointer in the
 * VBoxDrv_NULL_THUNK_DATA entry for VMMR0.r0.
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/types.h>

#include <stdio.h>
#include <string.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** A template. */
static uint8_t g_abTemplate[] =
{
#ifdef RT_ARCH_AMD64
    /* 0x0000: */ 0x64, 0x86,                 /* IMAGE_FILE_HEADER::Machine = IMAGE_FILE_MACHINE_AMD64 */
#elif defined(RT_ARCH_X86)
    /* 0x0000: */ 0x4c, 0x01,                 /* IMAGE_FILE_HEADER::Machine = IMAGE_FILE_MACHINE_I386 */
#else
#endif
    /* 0x0002: */ 0x02, 0x00,                 /* IMAGE_FILE_HEADER::NumberOfSections      = 2 */
    /* 0x0004: */ 0x00, 0x00, 0x00, 0x00,     /* IMAGE_FILE_HEADER::TimeDateStamp         = 0 */
    /* 0x0008: */ 0x74, 0x00, 0x00, 0x00,     /* IMAGE_FILE_HEADER::PointerToSymbolTable  = 0x74 */
    /* 0x000c: */ 0x02, 0x00, 0x00, 0x00,     /* IMAGE_FILE_HEADER::NumberOfSymbols       = 2 */
    /* 0x0010: */ 0x00, 0x00,                 /* IMAGE_FILE_HEADER::SizeOfOptionalHeader  = 0 */
    /* 0x0012: */ 0x05, 0x00,                 /* IMAGE_FILE_HEADER::Characteristics       = IMAGE_FILE_LINE_NUMS_STRIPPED | IMAGE_FILE_RELOCS_STRIPPED */
    /* Section Table: */
    /* 0x0014: */ 0x2e, 0x69, 0x64, 0x61, 0x74, 0x61, 0x24, 0x35, /* IMAGE_SECTION_HEADER::Name = ".idata5" */
    /* 0x001c: */ 0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::Misc.VirtualSize     = 0 */
    /* 0x0020: */ 0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::VirtualAddress       = 0  */
#if ARCH_BITS == 64
    /* 0x0024: */ 0x08, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::SizeOfRawData        = 8  */
#else
    /* 0x0024: */ 0x04, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::SizeOfRawData        = 4  */
#endif
                  0x64, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToRawData     = 0x64  */
                  0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToRelocations = 0 */
    /* 0x0030: */ 0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToLinenumbers = 0 */
                  0x00, 0x00,                 /* IMAGE_SECTION_HEADER::NumberOfRelocations  = 0 */
                  0x00, 0x00,                 /* IMAGE_SECTION_HEADER::NumberOfLinenumbers  = 0 */
                  0x40, 0x00, 0x40, 0xc0,     /* IMAGE_SECTION_HEADER::Characteristics */
                  0x2e, 0x69, 0x64, 0x61, 0x74, 0x61, 0x24, 0x34, /* IMAGE_SECTION_HEADER::Name = ".idata4" */
    /* 0x0044: */ 0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::Misc.VirtualSize     = 0 */
                  0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::VirtualAddress       = 0  */
#if ARCH_BITS == 64
                  0x08, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::SizeOfRawData        = 8  */
#else
                  0x04, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::SizeOfRawData        = 4  */
#endif
    /* 0x0050: */ 0x6c, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToRawData     = 0x6c  */
                  0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToRelocations = 0 */
                  0x00, 0x00, 0x00, 0x00,     /* IMAGE_SECTION_HEADER::PointerToLinenumbers = 0 */
                  0x00, 0x00,                 /* IMAGE_SECTION_HEADER::NumberOfRelocations  = 0 */
                  0x00, 0x00,                 /* IMAGE_SECTION_HEADER::NumberOfLinenumbers  = 0 */
    /* 0x0060: */ 0x40, 0x00, 0x40, 0xc0,     /* IMAGE_SECTION_HEADER::Characteristics */

    /* 0x0064: */ 0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, /* .idata5 content = NULL_THUNK_DATA - normally zeros */
    /* 0x006c: */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* .idata4 content = ??? */

    /* Symbol table: */
    /* 0x0074: */                         0x40, 0x63, 0x6f, 0x6d,  0x70, 0x2e, 0x69, 0x64, 0x1b, 0x9d, 0x9c, 0x00,
    /* 0x0080: */ 0xff, 0xff, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0x0090: */ 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,
#define SYM_LEN_OFF 0x98
    /* 0x0098: */ 0x1d, 0x00, 0x00, 0x00, /* The symbol name length (32-bit?) */
    /* 0x00a0: */ 0x7f /* the \177 byte of the \177<dllname>_NULL_THUNK_DATA string.  */
};

#if 0   /* AMD64 version: */

    0x64, 0x86, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,  0x74, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x05, 0x00, 0x2e, 0x69, 0x64, 0x61,  0x74, 0x61, 0x24, 0x35, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x40, 0x00, 0x40, 0xc0, 0x2e, 0x69, 0x64, 0x61,
    0x74, 0x61, 0x24, 0x34, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x40, 0xc0, 0x00, 0x00, 0x00, 0x00,  0x33, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x63, 0x6f, 0x6d,  0x70, 0x2e, 0x69, 0x64, 0x1b, 0x9d, 0x9c, 0x00,
    0xff, 0xff, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,  0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00,  0x1d, 0x00, 0x00, 0x00, 0x7f

#endif

/** Symbol suffix (zero terminator should be included in file). */
static const char g_szSymbolSuffix[] = "_NULL_THUNK_DATA";


int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        size_t cchDllNm = strlen(argv[1]);
        FILE  *pFile    = fopen(argv[2], "wb");
        if (pFile)
        {
            g_abTemplate[SYM_LEN_OFF] = 4 + 1 + cchDllNm + sizeof(g_szSymbolSuffix) - 1;
            size_t cbWritten = fwrite(g_abTemplate, 1, sizeof(g_abTemplate), pFile);
            if (cbWritten == sizeof(g_abTemplate))
            {
                cbWritten = fwrite(argv[1], 1, cchDllNm, pFile);
                if (cbWritten == cchDllNm)
                {
                    cbWritten = fwrite(g_szSymbolSuffix, 1, sizeof(g_szSymbolSuffix), pFile);
                    if (cbWritten == sizeof(g_szSymbolSuffix))
                    {
                        if (fclose(pFile) == 0)
                            return 0;

                        fprintf(stderr, "VBoxCoffNullThunkDataGen: error: fclose failed\n");
                    }
                    else
                        fprintf(stderr, "VBoxCoffNullThunkDataGen: error: write error\n");
                }
                else
                    fprintf(stderr, "VBoxCoffNullThunkDataGen: error: write error\n");
            }
            else
                fprintf(stderr, "VBoxCoffNullThunkDataGen: error: write error\n");
            fclose(pFile);
        }
        else
            fprintf(stderr, "VBoxCoffNullThunkDataGen: error: fopen('%s', 'wb') failed\n", argv[2]);
    }
    else
        fprintf(stderr, "Syntax error: usage: VBoxCoffNullThunkDataGen dllname file.obj\n");
    return RTEXITCODE_FAILURE;
}

