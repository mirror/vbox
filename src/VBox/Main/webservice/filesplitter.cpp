/** @file
 * File splitter: splits a text file according to ###### markers in it.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned long lineNumber(const char *pStr, const char *pPos)
{
    unsigned long cLine = 0;
    while (*pStr && pStr < pPos)
    {
        pStr = strchr(pStr, '\n');
        if (!pStr)
            break;
        ++cLine;
        ++pStr;
    }

    return cLine;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    const char *pcszBeginMarker = "\n// ##### BEGINFILE \"";
    const char *pcszEndMarker = "\n// ##### ENDFILE";
    const size_t cbBeginMarker = strlen(pcszBeginMarker);
    FILE *pFileIn = NULL;
    char *pBuffer = NULL;

    do {
        if (argc != 3)
        {
            fprintf(stderr, "filesplitter: Must be started with exactly two arguments,\n"
                            "1) the input file and 2) the directory where to put the output files\n");
            rc = 2;
            break;
        }

        struct stat lStat;
        if (    stat(argv[2], &lStat) != 0
             || (lStat.st_mode & S_IFMT) != S_IFDIR)
        {
            fprintf(stderr, "filesplitter: Given argument \"%s\" is not a valid directory.\n", argv[2]);
            rc = 2;
            break;
        }

        if (    stat(argv[1], &lStat)
             || !(pFileIn = fopen(argv[1], "r")))
        {
            fprintf(stderr, "filesplitter: Cannot open file \"%s\" for reading.\n", argv[1]);
            rc = 2;
            break;
        }

        if (!(pBuffer = (char*)malloc(lStat.st_size + 1)))
        {
            fprintf(stderr, "filesplitter: Failed to allocate %ld bytes.\n", (long)lStat.st_size);
            rc = 2;
            break;
        }

        if (fread(pBuffer, 1, lStat.st_size, pFileIn) != lStat.st_size)
        {
            fprintf(stderr, "filesplitter: Failed to read %ld bytes from input file.\n", (long)lStat.st_size);
            rc = 2;
            break;
        }
        pBuffer[lStat.st_size] = '\0';

        const char *pSearch = pBuffer;
        unsigned long cFiles = 0;
        size_t cbDirName = strlen(argv[2]);

        do
        {
            /* find begin marker */
            const char *pBegin = strstr(pSearch, pcszBeginMarker);
            if (!pBegin)
                break;

            /* find line after begin marker */
            const char *pLineAfterBegin = strchr(pBegin + cbBeginMarker, '\n');
            if (!pLineAfterBegin)
            {
                fprintf(stderr, "filesplitter: No newline after begin-file marker found.\n");
                rc = 2;
                break;
            }
            ++pLineAfterBegin;

            /* find second quote in begin marker line */
            const char *pSecondQuote = strchr(pBegin + cbBeginMarker, '\"');
            if (    !pSecondQuote
                 || pSecondQuote >= pLineAfterBegin)
            {
                fprintf(stderr, "filesplitter: Can't parse filename after begin-file marker (line %lu).\n", lineNumber(pBuffer, pcszBeginMarker));
                rc = 2;
                break;
            }

            /* find end marker */
            const char *pEnd = strstr(pLineAfterBegin, pcszEndMarker);
            if (!pEnd)
            {
                fprintf(stderr, "filesplitter: No matching end-line marker for begin-file marker found (line %lu).\n", lineNumber(pBuffer, pcszBeginMarker));
                rc = 2;
                break;
            }

            /* construct output filename */
            char *pszFilename;
            size_t cbFilename;
            cbFilename = pSecondQuote - (pBegin + cbBeginMarker);
            if (!(pszFilename = (char*)malloc(cbDirName + 1 + cbFilename + 1)))
            {
                fprintf(stderr, "filesplitter: Can't allocate memory for filename.\n");
                rc = 2;
                break;
            }
            memcpy(pszFilename, argv[2], cbDirName);
            pszFilename[cbDirName] = '/';
            memcpy(pszFilename + cbDirName + 1, pBegin + cbBeginMarker, cbFilename);
            pszFilename[cbFilename + 1 + cbDirName] = '\0';

            /* create output file and write file contents */
            FILE *pFileOut;
            if (!(pFileOut = fopen(pszFilename, "w")))
            {
                fprintf(stderr, "filesplitter: Failed to open file \"%s\" for writing\n", pszFilename);
                rc = 2;
            }
            else
            {
                size_t cbFile = pEnd - pLineAfterBegin;
                if (fwrite(pLineAfterBegin, 1, cbFile, pFileOut) != cbFile)
                {
                    fprintf(stderr, "filesplitter: Failed to write %ld bytes to file \"%s\"\n", (long)cbFile, pszFilename);
                    rc = 2;
                }

                fclose(pFileOut);

                if (!rc)
                {
                    ++cFiles;
                    pSearch = strchr(pEnd, '\n');
                }
            }

            free(pszFilename);

            if (rc)
                break;

        } while (pSearch);

        printf("filesplitter: Created %lu files.\n", cFiles);
    } while (0);

    if (pBuffer)
        free(pBuffer);
    if (pFileIn)
        fclose(pFileIn);

    return rc;
}
