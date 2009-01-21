/** @file
 * File splitter: splits a text file according to ###### markers in it.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned long lineNumber(const char *pBegin, const char *pPos);

unsigned long lineNumber(const char *pBegin, const char *pPos)
{
    const char *p = pBegin;
    unsigned long c = 0;
    while (*p && p < pPos)
    {
        if (*p == '\n')
            ++c;
        ++p;
    }

    return c;
}

int main(int argc, char *argv[])
{
    int rc = 0;

    if (argc != 3)
    {
        fprintf(stderr, "filesplitter: Must be started with exactly two arguments,\n"
                        "1) the input file and 2) the directory where to put the output files\n");
        rc = 2;
    }
    else
    {
        FILE          *pFileIn;
        struct stat   statIn;
        if (    (stat(argv[1], &statIn))
             || (!(pFileIn = fopen(argv[1], "r")))
           )
        {
            fprintf(stderr, "filesplitter: Cannot open file \"%s\" for reading.\n", argv[1]);
            rc = 2;
        }
        else
        {
            struct stat statOut;
            if (    (stat(argv[2], &statOut))
                 || ((statOut.st_mode & S_IFMT) != S_IFDIR)
               )
            {
                fprintf(stderr, "filesplitter: Given argument \"%s\" is not a valid directory.\n", argv[2]);
                rc = 2;
            }
            else
            {
                char *p;
                if (!(p = (char*)malloc(statIn.st_size + 1)))
                    fprintf(stderr, "filesplitter: Failed to allocate %ld bytes.\n", (long)statIn.st_size);
                else
                {
                    size_t read;
                    if (!(read = fread(p, 1, statIn.st_size, pFileIn)))
                    {
                        fprintf(stderr, "filesplitter: Failed to read %ld bytes from input file.\n", (long)statIn.st_size);
                        rc = 2;
                    }
                    else
                    {
                        const char *pSearch = p;
                        const char *pBegin;
                        unsigned long c = 0;

                        size_t lengthDirName = strlen(argv[2]);

                        p[read] = '\0';

                        do
                        {
                            const char *pcszBeginMarker = "\n// ##### BEGINFILE \"";
                            const char *pcszEndMarker = "\n// ##### ENDFILE";
                            size_t lengthBeginMarker = strlen(pcszBeginMarker);
                            const char *pSecondQuote, *pLineAfterBegin;
                            const char *pEnd;
                            char *pszFilename;
                            size_t lengthFilename;
                            FILE *pFileOut;

                            if (!(pBegin = strstr(pSearch, pcszBeginMarker)))
                                break;
                            if (!(pLineAfterBegin = strchr(pBegin + lengthBeginMarker, '\n')))
                            {
                                fprintf(stderr, "filesplitter: No newline after begin-file marker found.\n");
                                rc = 2;
                                break;
                            }
                            if (    (!(pSecondQuote = strchr(pBegin + lengthBeginMarker, '\"')))
                                 || (pSecondQuote > pLineAfterBegin)
                               )
                            {
                                fprintf(stderr, "filesplitter: Can't parse filename after begin-file marker (line %lu).\n", lineNumber(p, pcszBeginMarker));
                                rc = 2;
                                break;
                            }

                            ++pLineAfterBegin;

                            if (!(pEnd = strstr(pLineAfterBegin, pcszEndMarker)))
                            {
                                fprintf(stderr, "filesplitter: No matching end-line marker for begin-file marker found (line %lu).\n", lineNumber(p, pcszBeginMarker));
                                rc = 2;
                                break;
                            }

                            lengthFilename = pSecondQuote - (pBegin + lengthBeginMarker);
                            if (!(pszFilename = (char*)malloc(lengthDirName + 1 + lengthFilename + 1)))
                            {
                                fprintf(stderr, "filesplitter: Can't allocate memory for filename.\n");
                                rc = 2;
                                break;
                            }
                            memcpy(pszFilename, argv[2], lengthDirName);
                            pszFilename[lengthDirName] = '/';
                            memcpy(pszFilename + lengthDirName + 1, pBegin + lengthBeginMarker, lengthFilename);
                            pszFilename[lengthFilename + 1 + lengthDirName] = '\0';

                            if (!(pFileOut = fopen(pszFilename, "w")))
                            {
                                fprintf(stderr, "filesplitter: Failed to open file \"%s\" for writing\n", pszFilename);
                                rc = 2;
                            }
                            else
                            {
                                unsigned long cbFile = pEnd - pLineAfterBegin;
                                unsigned long cbWritten = fwrite(pLineAfterBegin,
                                                                 1,
                                                                 cbFile,
                                                                 pFileOut);
                                if (!cbWritten)
                                {
                                    fprintf(stderr, "filesplitter: Failed to write %lu bytes to file \"%s\"\n", cbFile, pszFilename);
                                    rc = 2;
                                }

                                fclose(pFileOut);

                                if (!rc)
                                {
                                    ++c;
                                    pSearch = strchr(pEnd, '\n');
                                }
                            }

                            free(pszFilename);

                            if (rc)
                                break;

                        } while (pSearch);

                        printf("filesplitter: Created %lu files.\n", c);
                    }
                }

                free(p);
            }
        }
    }

    return rc;
}
