/* $Id$ */
/** @file
 * bin2c - Binary 2 C Structure Converter.
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

static int usage(const char *argv0)
{
    fprintf(stderr,
            "Syntax: %s [options] <arrayname> <binaryfile> <outname>\n"
            "  -min <n>     check if <binaryfile> is not smaller than <n>KB\n"
            "  -max <n>     check if <binaryfile> is not bigger than <n>KB\n"
            "  -mask <n>    check if size of binaryfile is <n>-aligned\n"
            "  -ascii       show ASCII representation of binary as comment\n",
            argv0);

    return 1;
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
    int           rc = 1;               /* assume the worst... */

    if (argc < 2)
        return usage(argv[0]);

    for (i=1; i<argc; i++)
    {
        if (!strcmp(argv[i], "-min"))
        {
            if (++i >= argc)
                return usage(argv[0]);
            cbMin = 1024 * strtoul(argv[i], NULL, 0);
        }
        else if (!strcmp(argv[i], "-max"))
        {
            if (++i >= argc)
                return usage(argv[0]);
            cbMax = 1024 * strtoul(argv[i], NULL, 0);
        }
        else if (!strcmp(argv[i], "-mask"))
        {
            if (++i >= argc)
                return usage(argv[0]);
            uMask = strtoul(argv[i], NULL, 0);
        }
        else if (!strcmp(argv[i], "-ascii"))
        {
            fAscii = 1;
        }
        else if (!strcmp(argv[i], "-export"))
        {
            fExport = 1;
        }
        else if (i == argc - 3)
            break;
        else
        {
            fprintf(stderr, "%s: syntax error: Unknown argument '%s'\n", 
                    argv[0], argv[i]);
            return usage(argv[0]);
        }
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
           " * from %s\n"
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
        fprintf(stderr, "%s: size=%ld - Not aligned!\n", argv[0], (long)cbBin);
    else if (cbBin < cbMin || cbBin > cbMax)
        fprintf(stderr, "%s: size=%ld - Not %ld-%ldb in size!\n",
                argv[0], (long)cbBin, (long)cbMin, (long)cbMax);
    else
    {
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
            fprintf(stderr, "%s: read error\n", argv[0]);
        else if (off != cbBin)
            fprintf(stderr, "%s: read error off=%ld cbBin=%ld\n", argv[0], (long)off, (long)cbBin);
        else
        {
            /* no errors, finish the structure. */
            fprintf(pFileOut,
                    "};\n"
                    "\n"
                    "%sconst unsigned%s g_cb%s = sizeof(g_ab%s);\n"
                    "/* end of file */\n",
                    fExport ? "DECLEXPORT(" : "", fExport ? ")" : "", argv[i], argv[i]);
    
            /* flush output and check for error. */
            fflush(pFileOut);
            if (ferror(pFileOut))
                fprintf(stderr, "%s: write error\n", argv[0]);
            else
                rc = 0; /* success! */
        }
    }

    /* cleanup, delete the output file on failure. */
    fclose(pFileOut);
    fclose(pFileIn);
    if (rc)
        remove(argv[i+2]);

    return rc;
}
