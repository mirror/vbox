/** @file
 * File splitter: splits soapC.cpp into manageable pieces. It is somewhat
 * intelligent and avoids splitting inside functions or similar places.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>


int main(int argc, char *argv[])
{
    int rc = 0;
    FILE *pFileIn = NULL;
    FILE *pFileOut = NULL;
    char *pBuffer = NULL;

    do
    {
        if (argc != 4)
        {
            fprintf(stderr, "split-soapC: Must be started with exactly three arguments,\n"
                            "1) the input file, 2) the directory where to put the output files and\n"
                            "3) the number chunks to create.\n");
            rc = 2;
            break;
        }

        char *pEnd = NULL;
        unsigned long cChunk = strtoul(argv[3], &pEnd, 0);
        if (cChunk == ULONG_MAX || cChunk == 0 || !argv[3] || *pEnd)
        {
            fprintf(stderr, "split-soapC: Given argument \"%s\" is not a valid chunk count.\n", argv[3]);
            rc = 2;
            break;
        }

        pFileIn = fopen(argv[1], "rb");
        if (!pFileIn)
        {
            fprintf(stderr, "split-soapC: Cannot open file \"%s\" for reading.\n", argv[1]);
            rc = 2;
            break;
        }
        int rc2 = fseek(pFileIn, 0, SEEK_END);
        long cbFileIn = ftell(pFileIn);
        int rc3 = fseek(pFileIn, 0, SEEK_SET);
        if (rc3 == -1 || rc2 == -1 || cbFileIn < 0)
        {
            fprintf(stderr, "split-soapC: Seek failure.\n");
            rc = 2;
            break;
        }

        if (!(pBuffer = (char*)malloc(cbFileIn + 1)))
        {
            fprintf(stderr, "split-soapC: Failed to allocate %ld bytes.\n", cbFileIn);
            rc = 2;
            break;
        }

        if (fread(pBuffer, 1, cbFileIn, pFileIn) != (size_t)cbFileIn)
        {
            fprintf(stderr, "split-soapC: Failed to read %ld bytes from input file.\n", cbFileIn);
            rc = 2;
            break;
        }
        pBuffer[cbFileIn] = '\0';

        const char *pLine = pBuffer;
        unsigned long cbChunk = cbFileIn / cChunk;
        unsigned long cFiles = 0;
        unsigned long uLimit = 0;
        unsigned long cbWritten = 0;
        unsigned long cIfNesting = 0;
        unsigned long cBraceNesting = 0;
        unsigned long cLinesSinceStaticMap = ~0UL / 2;
        bool fJustZero = false;

        do
        {
            if (!pFileOut)
            {
                /* construct output filename */
                char szFilename[1024];
                sprintf(szFilename, "%s/soapC-%lu.cpp", argv[2], ++cFiles);
                szFilename[sizeof(szFilename)-1] = '\0';
                printf("info: soapC-%lu.cpp\n", cFiles);

                /* create output file */
                if (!(pFileOut = fopen(szFilename, "wb")))
                {
                    fprintf(stderr, "split-soapC: Failed to open file \"%s\" for writing\n", szFilename);
                    rc = 2;
                    break;
                }
                if (cFiles > 1)
                    fprintf(pFileOut, "#include \"soapH.h\"%s\n",
#ifdef RT_OS_WINDOWS
                                      "\r"
#else /* !RT_OS_WINDOWS */
                                      ""
#endif /* !RT_OS_WINDOWS */
                           );
                uLimit += cbChunk;
                cLinesSinceStaticMap = ~0UL / 2;
            }

            /* find begin of next line and print current line */
            const char *pNextLine = strchr(pLine, '\n');
            size_t cbLine;
            if (pNextLine)
            {
                pNextLine++;
                cbLine = pNextLine - pLine;
            }
            else
                cbLine = strlen(pLine);
            if (fwrite(pLine, 1, cbLine, pFileOut) != cbLine)
            {
                fprintf(stderr, "split-soapC: Failed to write to output file\n");
                rc = 2;
                break;
            }
            cbWritten += cbLine;

            /* process nesting depth information */
            if (!strncmp(pLine, "#if", 3))
                cIfNesting++;
            else if (!strncmp(pLine, "#endif", 6))
            {
                cIfNesting--;
                if (!cBraceNesting && !cIfNesting)
                    fJustZero = true;
            }
            else
            {
                for (const char *p = pLine; p < pLine + cbLine; p++)
                {
                    if (*p == '{')
                        cBraceNesting++;
                    else if (*p == '}')
                    {
                        cBraceNesting--;
                        if (!cBraceNesting && !cIfNesting)
                            fJustZero = true;
                    }
                }
            }

            /* look for static variables used for enum conversion. */
            if (!strncmp(pLine, "static const struct soap_code_map", sizeof("static const struct soap_code_map") - 1))
                cLinesSinceStaticMap = 0;
            else
                cLinesSinceStaticMap++;

            /* start a new output file if necessary and possible */
            if (   cbWritten >= uLimit
                && cIfNesting == 0
                && fJustZero
                && cFiles < cChunk
                && cLinesSinceStaticMap > 150 /*hack!*/)
            {
                fclose(pFileOut);
                pFileOut = NULL;
            }

            if (rc)
                break;

            fJustZero = false;
            pLine = pNextLine;
        } while (pLine);

        printf("split-soapC: Created %lu files.\n", cFiles);
    } while (0);

    if (pBuffer)
        free(pBuffer);
    if (pFileIn)
        fclose(pFileIn);
    if (pFileOut)
        fclose(pFileOut);

    return rc;
}
