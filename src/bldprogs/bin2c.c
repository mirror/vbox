/* $Id$ */
/** @file
 * PC-BIOS - Binary 2 C Structure Converter.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>


/**
 * File size.
 *
 * @returns file size in bytes.
 * @returns 0 on failure.
 * @param   pFile   File to size.
 */
static size_t fsize(FILE *pFile)
{
    long    cbFile;
    off_t   Pos = ftell(pFile);
    if (    Pos >= 0
        &&  !fseek(pFile, 0, SEEK_END))
    {
        cbFile = ftell(pFile);
        if (    cbFile >= 0
            &&  !fseek(pFile, 0, SEEK_SET))
            return cbFile;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    FILE          *pFileIn;
    FILE          *pFileOut;
    int           i;
    size_t        cbMin = 0;
    size_t        cbMax = ~0U;
    size_t        uMask = 0;
    int           fAscii = 0;
    int           fExport = 0;
    unsigned char abLine[16];
    size_t        off;
    size_t        cbRead;
    size_t        cbBin;

    if (argc < 2)
        goto syntax_error;

    for (i=1; i<argc; i++)
    {
        if (!strcmp(argv[i], "-min"))
        {
            if (++i>=argc)
                goto syntax_error;
            cbMin = 1024 * strtoul(argv[i], NULL, 0);
            continue;
        }
        else if (!strcmp(argv[i], "-max"))
        {
            if (++i>=argc)
                goto syntax_error;
            cbMax = 1024 * strtoul(argv[i], NULL, 0);
            continue;
        }
        else if (!strcmp(argv[i], "-mask"))
        {
            if (++i>=argc)
                goto syntax_error;
            uMask = strtoul(argv[i], NULL, 0);
            continue;
        }
        else if (!strcmp(argv[i], "-ascii"))
        {
            fAscii = 1;
            continue;
        }
        else if (!strcmp(argv[i], "-export"))
        {
            fExport = 1;
            continue;
        }
        else if (i==argc-3)
            break;

syntax_error:
        fprintf(stderr,
                "Syntax: %s [options] <arrayname> <binaryfile> <outname>\n"
                "  -min <n>     check if <binaryfile> is not smaller than <n>KB\n"
                "  -max <n>     check if <binaryfile> is not bigger than <n>KB\n"
                "  -mask <n>    check if size of binaryfile is <n>-aligned\n"
                "  -ascii       show ASCII representation of binary as comment\n",
                argv[0]);
        return 1;
    }

    pFileIn = fopen(argv[i+1], "rb");
    if (!pFileIn)
    {
        fprintf(stderr, "Error: failed to open input file '%s'!\n", argv[i+1]);
        return 1;
    }

    pFileOut = fopen(argv[i+2], "wb");
    if (!pFileOut)
    {
        fprintf(stderr, "Error: failed to open output file '%s'!\n", argv[i+2]);
        fclose(pFileIn);
        return 1;
    }

    cbBin = fsize(pFileIn);

    fprintf(pFileOut,
           "/*\n"
           " * This file was automatically generated\n"
           " * from %s by\n"
           " * by %s.\n"
           " */\n"
           "\n"
           "#include <iprt/cdefs.h>\n"
           "\n"
           "%sconst unsigned char%s g_ab%s[] =\n"
           "{\n",
           argv[i+1], argv[0], fExport ? "DECLEXPORT(" : "", fExport ? ")" : "", argv[i]);

    /* check size restrictions */
    if (uMask && (cbBin & uMask))
    {
        fprintf(stderr, "%s: size=%ld - Not aligned!\n", argv[0], (long)cbBin);
        return 1;
    }
    if (cbBin < cbMin || cbBin > cbMax)
    {
        fprintf(stderr, "%s: size=%ld - Not %ld-%ldb in size!\n",
                argv[0], (long)cbBin, (long)cbMin, (long)cbMax);
        return 1;
    }

    /* the binary data */
    off = 0;
    while ((cbRead = fread(&abLine[0], 1, sizeof(abLine), pFileIn)) > 0)
    {
        size_t i;
        fprintf(pFileOut, "   ");
        for (i = 0; i < cbRead; i++)
            fprintf(pFileOut, " 0x%02x,", abLine[i]);
        for (; i < sizeof(abLine); i++)
            fprintf(pFileOut, "      ");
        if (fAscii)
        {
            fprintf(pFileOut, " /* 0x%08lx: ", (long)off);
            for (i = 0; i < cbRead; i++)
                /* be careful with '/' prefixed/followed by a '*'! */
                fprintf(pFileOut, "%c", 
                        isprint(abLine[i]) && abLine[i] != '/' ? abLine[i] : '.');
            for (; i < sizeof(abLine); i++)
                fprintf(pFileOut, " ");
            fprintf(pFileOut, " */");
        }
        fprintf(pFileOut, "\n");

        off += cbRead;
    }

    /* check for errors */
    if (ferror(pFileIn) && !feof(pFileIn))
    {
        fprintf(stderr, "%s: read error\n", argv[0]);
        goto error;
    }
    if (off != cbBin)
    {
        fprintf(stderr, "%s: read error off=%ld cbBin=%ld\n", argv[0], (long)off, (long)cbBin);
        goto error;
    }

    /* finish the structure. */
    fprintf(pFileOut,
            "};\n"
            "\n"
            "%sconst unsigned%s g_cb%s = sizeof(g_ab%s);\n"
            "/* end of file */\n",
            fExport ? "DECLEXPORT(" : "", fExport ? ")" : "", argv[i], argv[i]);
    fclose(pFileIn);

    /* flush output and check for error. */
    fflush(pFileOut);
    if (ferror(pFileOut))
    {
        fprintf(stderr, "%s: write error\n", argv[0]);
        goto error;
    }
    fclose(pFileOut);

    return 0;

error:
    fclose(pFileOut);
    remove(argv[i+2]);
    return 1;
}
