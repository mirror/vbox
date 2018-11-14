/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folders, Utility for attaching and testing.
 */

/*
 * Copyright (c) 2016-2018 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define INCL_BASE
#define OS2EMX_PLAIN_CHAR
#include <os2.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
extern "C" APIRET __cdecl CallDosQFileMode(const char *pszFilename, PUSHORT pfAttr, ULONG ulReserved);



int vboxSfOs2UtilUse(int argc, char **argv)
{
    /*
     * Continue parsing.
     */
    if (argc != 3)
    {
        fprintf(stderr, "syntax error: Expected three arguments to 'use' command\n");
        return 2;
    }

    /* The drive letter. */
    const char *pszDrive = argv[1];
    if (   (   (pszDrive[0] >= 'A' && pszDrive[0] <= 'Z')
            || (pszDrive[0] >= 'a' && pszDrive[0] <= 'z'))
        && pszDrive[1] == ':'
        && pszDrive[2] == '\0')
    { /* likely */ }
    else
    {
        fprintf(stderr, "syntax error: Invalid drive specification '%s', expected something like 'K:'.\n", pszDrive);
        return 2;
    }

    /* The shared folder. */
    const char *pszFolder = argv[2];
    size_t      cchFolder = strlen(pszFolder);
    if (cchFolder <= 80 && cchFolder != 0)
    { /* likely */ }
    else
    {
        fprintf(stderr, "syntax error: Shared folder name '%s' is too %s!\n", pszFolder, cchFolder >= 1 ? "long" : "short");
        return 2;
    }

    /*
     * Try attach it.
     */
    APIRET rc = DosFSAttach(pszDrive, "VBOXSF", (void *)pszFolder, cchFolder + 1, FS_ATTACH);
    if (rc == NO_ERROR)
    {
        printf("done\n");
        return 0;
    }
    fprintf(stderr, "error: DosFSAttach failed: %lu\n", rc);
    return 1;
}


int vboxSfOs2UtilQPathInfo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        union
        {
            USHORT          fAttribs;
            FILESTATUS      Lvl1r1;
            FILESTATUS3     Lvl1r3;
            FILESTATUS3L    Lvl11;
            FILESTATUS2     Lvl2r2;
            FILESTATUS4     Lvl2r4;
            FILESTATUS4L    Lvl12;
            FEA2LIST        FeaList;
            char            szFullName[260];
            //char            szTest1[sizeof(FILESTATUS3) == sizeof(FILESTATUS)];
            //char            szTest2[sizeof(FILESTATUS4) == sizeof(FILESTATUS2)];
        } u;

        u.fAttribs = 0xffff;
        int rc = CallDosQFileMode(argv[i], &u.fAttribs, 0 /* reserved */);
        printf("%s: DosQFileMode -> %u, %#x\n", argv[i], rc, u.fAttribs);

        memset(&u, 0xaa, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_STANDARD, &u.Lvl1r1, sizeof(u.Lvl1r1));
        printf("%s: FIL_STANDARD/%#x -> %u\n", argv[i], sizeof(u.Lvl1r1), rc);
#define D(x) (*(uint16_t *)&(x))
#define T(x) (*(uint16_t *)&(x))
        if (rc == NO_ERROR)
        {
            printf("  Lvl1r1: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl1r1.fdateCreation), T(u.Lvl1r1.ftimeCreation),  D(u.Lvl1r1.fdateLastWrite), T(u.Lvl1r1.ftimeLastWrite),
                   D(u.Lvl1r1.fdateLastAccess), T(u.Lvl1r1.ftimeLastAccess));
            printf("  Lvl1r1:  attrib=%#x size=%lu alloc=%lu\n", u.Lvl1r1.attrFile, u.Lvl1r1.cbFile, u.Lvl1r1.cbFileAlloc);

        }

        memset(&u, 0xbb, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_STANDARD, &u.Lvl1r3, sizeof(u.Lvl1r3));
        printf("%s: FIL_STANDARD/%#x-> %u\n", argv[i], sizeof(u.Lvl1r3), rc);
        if (rc == NO_ERROR)
        {
            printf("  Lvl1r3: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl1r3.fdateCreation), T(u.Lvl1r3.ftimeCreation), D(u.Lvl1r3.fdateLastWrite), T(u.Lvl1r3.ftimeLastWrite),
                   D(u.Lvl1r3.fdateLastAccess), T(u.Lvl1r3.ftimeLastAccess));
            printf("  Lvl1r3:  attrib=%#lx size=%lu alloc=%lu\n", u.Lvl1r3.attrFile, u.Lvl1r3.cbFile, u.Lvl1r3.cbFileAlloc);

        }

        memset(&u, 0xdd, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_STANDARDL, &u.Lvl11, sizeof(u.Lvl11));
        printf("%s: FIL_STANDARDL/%#x -> %u\n", argv[i], sizeof(u.Lvl11), rc);
        if (rc == NO_ERROR)
        {
            printf("   Lvl11: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl11.fdateCreation), T(u.Lvl11.ftimeCreation), D(u.Lvl11.fdateLastWrite), T(u.Lvl11.ftimeLastWrite),
                   D(u.Lvl11.fdateLastAccess), T(u.Lvl11.ftimeLastAccess));
            printf("   Lvl11:  attrib=%#lx size=%llu alloc=%llu\n", u.Lvl11.attrFile, u.Lvl11.cbFile, u.Lvl11.cbFileAlloc);
        }

        memset(&u, 0xee, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_QUERYEASIZE, &u.Lvl2r2, sizeof(u.Lvl2r2));
        printf("%s: FIL_QUERYEASIZE/%#x -> %u\n", argv[i], sizeof(u.Lvl2r2), rc);
        if (rc == NO_ERROR)
        {
            printf("    Lvl2: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl2r2.fdateCreation), T(u.Lvl2r2.ftimeCreation),  D(u.Lvl2r2.fdateLastWrite), T(u.Lvl2r2.ftimeLastWrite),
                   D(u.Lvl2r2.fdateLastAccess), T(u.Lvl2r2.ftimeLastAccess));
            printf("    Lvl2:  attrib=%#x size=%lu alloc=%lu cbList=%#lx\n",
                   u.Lvl2r2.attrFile, u.Lvl2r2.cbFile, u.Lvl2r2.cbFileAlloc, u.Lvl2r4.cbList);
        }

        memset(&u, 0x55, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_QUERYEASIZE, &u.Lvl2r4, sizeof(u.Lvl2r4));
        printf("%s: FIL_QUERYEASIZE/%#x -> %u\n", argv[i], sizeof(u.Lvl2r4), rc);
        if (rc == NO_ERROR)
        {
            printf("    Lvl2: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl2r4.fdateCreation), T(u.Lvl2r4.ftimeCreation),  D(u.Lvl2r4.fdateLastWrite), T(u.Lvl2r4.ftimeLastWrite),
                   D(u.Lvl2r4.fdateLastAccess), T(u.Lvl2r4.ftimeLastAccess));
            printf("    Lvl2:  attrib=%#lx size=%lu alloc=%lu cbList=%#lx\n",
                   u.Lvl2r4.attrFile, u.Lvl2r4.cbFile, u.Lvl2r4.cbFileAlloc, u.Lvl2r4.cbList);
        }

        memset(&u, 0x99, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_QUERYEASIZEL, &u.Lvl12, sizeof(u.Lvl12));
        printf("%s: FIL_QUERYEASIZEL/%#x -> %u\n", argv[i], sizeof(u.Lvl12), rc);
        if (rc == NO_ERROR)
        {
            printf("   Lvl12: creation=%u:%u write=%u:%u access=%u:%u\n",
                   D(u.Lvl12.fdateCreation), T(u.Lvl12.ftimeCreation),  D(u.Lvl12.fdateLastWrite), T(u.Lvl12.ftimeLastWrite),
                   D(u.Lvl12.fdateLastAccess), T(u.Lvl12.ftimeLastAccess));
            printf("   Lvl12:  attrib=%#lx size=%llu alloc=%llu cbList=%#lx\n",
                   u.Lvl12.attrFile, u.Lvl12.cbFile, u.Lvl12.cbFileAlloc, u.Lvl12.cbList);
        }

        memset(&u, 0x44, sizeof(u));
        rc = DosQueryPathInfo(argv[i], FIL_QUERYFULLNAME, &u.szFullName[0], sizeof(u.szFullName));
        printf("%s: FIL_QUERYFULLNAME -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("   Lvl5: %s<eol>\n", u.szFullName);

        size_t cchInput = strlen(argv[i]);
        if (cchInput >= sizeof(u.szFullName))
            cchInput = sizeof(u.szFullName) - 1;

        /** @todo Cannot get to level 6 thru 32-bit, need 16-bit. DOSCALL1.DLL
         *        chickens out on us.  Sigh. */
        memcpy(u.szFullName, argv[i], cchInput);
        u.szFullName[0] = '\0';
        rc = DosQueryPathInfo(argv[i], 6 /*FIL_VERIFY_SYNTAX*/, &u.szFullName[0], sizeof(u.szFullName));
        printf("%s: FIL_VERIFY_SYNTAX -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("   Lvl6: %s<eol>\n", u.szFullName);

        memcpy(u.szFullName, argv[i], cchInput);
        u.szFullName[0] = '\0';
        rc = DosQueryPathInfo(argv[i], 16 /*FIL_VERIFY_SYNTAX_L*/, &u.szFullName[0], sizeof(u.szFullName));
        printf("%s: FIL_VERIFY_SYNTAX_L -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("  Lvl6L: %s<eol>\n", u.szFullName);

        memcpy(u.szFullName, argv[i], cchInput);
        u.szFullName[0] = '\0';
        rc = DosQueryPathInfo(argv[i], 7 /*FIL_FIX_CASE*/, &u.szFullName[0], sizeof(u.szFullName));
        printf("%s: FIL_FIX_CASE -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("   Lvl7: %s<eol>\n", u.szFullName);

        memcpy(u.szFullName, argv[i], cchInput);
        u.szFullName[0] = '\0';
        rc = DosQueryPathInfo(argv[i], 17 /*FIL_FIX_CASE_L*/, &u.szFullName[0], sizeof(u.szFullName));
        printf("%s: FIL_FIX_CASE_L -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("  Lvl17: %s<eol>\n", u.szFullName);

        struct
        {
            ULONG cbList;
            ULONG oNext;
            BYTE  cchName;
            char  szName[10];
        } Gea2List, Gea2ListOrg = { sizeof(Gea2List), 0, sizeof(".LONGNAME") - 1, ".LONGNAME" };
        EAOP2 EaOp;
        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], FIL_QUERYEASFROMLIST, &EaOp, sizeof(EaOp));
        printf("%s: FIL_QUERYEASFROMLIST -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("  Lvl3: FeaList.cbList=%#lx oError=%#lx\n", u.FeaList.cbList, EaOp.oError);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], FIL_QUERYEASFROMLISTL, &EaOp, sizeof(EaOp));
        if (rc != ERROR_INVALID_LEVEL)
            printf("%s: FIL_QUERYEASFROMLISTL -> %u\n", argv[i], rc);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], 4, &EaOp, sizeof(EaOp));
        printf("%s: FIL_QUERYALLEAS/4 -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("  Lvl4: FeaList.cbList=%#lx oError=%#lx\n", u.FeaList.cbList, EaOp.oError);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], 14, &EaOp, sizeof(EaOp));
        if (rc != ERROR_INVALID_LEVEL)
            printf("%s: FIL_QUERYALLEASL/14 -> %u\n", argv[i], rc);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], 8, &EaOp, sizeof(EaOp));
        printf("%s: FIL_QUERYALLEAS/8 -> %u\n", argv[i], rc);
        if (rc == NO_ERROR)
            printf("  Lvl8: FeaList.cbList=%#lx oError=%#lx\n", u.FeaList.cbList, EaOp.oError);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], 18, &EaOp, sizeof(EaOp));
        if (rc != ERROR_INVALID_LEVEL)
            printf("%s: FIL_QUERYALLEASL/18 -> %u\n", argv[i], rc);

        EaOp.fpGEA2List = (PGEA2LIST)memcpy(&Gea2List, &Gea2ListOrg, sizeof(Gea2List));
        EaOp.fpFEA2List = &u.FeaList;
        EaOp.oError     = 0;
        memset(&u, '\0', sizeof(u));
        u.FeaList.cbList = sizeof(u);
        rc = DosQueryPathInfo(argv[i], 15, &EaOp, sizeof(EaOp));
        if (rc != ERROR_INVALID_LEVEL)
            printf("%s: FIL_QUERYALLEASL/15 -> %u\n", argv[i], rc);

        memset(&u, '\0', sizeof(u));
        rc = DosQueryPathInfo(argv[i], 0, &u, sizeof(u));
        if (rc != ERROR_INVALID_LEVEL)
            printf("%s: 0 -> %u\n", argv[i], rc);
    }
    return 0;
}


int vboxSfOs2UtilFindFile(int argc, char **argv)
{
    unsigned    cMaxMatches = 1;
    unsigned    cbBuf       = 1024;
    unsigned    uLevel      = FIL_STANDARDL;
    unsigned    fAttribs    = FILE_DIRECTORY | FILE_HIDDEN | FILE_SYSTEM;
    bool        fOptions    = true;

    uint8_t    *pbBuf = NULL;
    for (int i = 1; i < argc; i++)
    {
        const char *pszArg = argv[i];

        /*
         * Deal with options.
         */
        if (fOptions && *pszArg == '-')
        {
            pszArg++;
            if (*pszArg == '-')
            {
                pszArg++;
                if (!*pszArg)
                {
                    fOptions = false;
                    continue;
                }
                if (strcmp(pszArg, "attribs") == 0)
                    pszArg = "a";
                else if (strcmp(pszArg, "buffer-size") == 0)
                    pszArg = "b";
                else if (strcmp(pszArg, "level") == 0)
                    pszArg = "l";
                else if (strcmp(pszArg, "matches") == 0)
                    pszArg = "m";
                else if (strcmp(pszArg, "help") == 0)
                    pszArg = "h";
                else
                {
                    fprintf(stderr, "syntax error: Unknown option: %s\n", argv[i]);
                    return 2;
                }
            }
            do
            {
                const char *pszValue = NULL;
                char chOpt = *pszArg++;
                if (strchr("ablm", chOpt) != NULL)
                {
                    if (*pszArg != '\0')
                        pszValue = *pszArg == ':' || *pszArg == '=' ? ++pszArg : pszArg;
                    else if (i + 1 < argc)
                        pszValue = argv[++i];
                    else
                    {
                        fprintf(stderr, "syntax error: -%c takes a value\n", chOpt);
                        return 2;
                    }
                    pszArg = "";
                }
                switch (chOpt)
                {
                    case 'a':
                        fAttribs = atoi(pszValue);
                        break;
                    case 'b':
                        cbBuf = atoi(pszValue);
                        free(pbBuf);
                        pbBuf = NULL;
                        break;
                    case 'l':
                        uLevel = atoi(pszValue);
                        break;
                    case 'm':
                        cMaxMatches = atoi(pszValue);
                        break;
                    case 'h':
                        printf("usage: findfile [-a|--attribs <mask>] [-b|--buffer-size <bytes>]\n"
                               "           [-l|--level <num>] [-m|--matches <num>] [--] <dir1> [dir2..N]\n");
                        return 0;
                    default:
                        fprintf(stderr, "syntax error: Unknown option '%c' (%s)\n", chOpt, argv[i - (pszValue != NULL)]);
                        return 2;
                }

            } while (pszArg && *pszArg != '\0');
        }
        else
        {
            /*
             * Search the specified directory/whatever.
             */
            if (!pbBuf)
            {
                pbBuf = (uint8_t *)malloc(cbBuf);
                if (!pbBuf)
                {
                    fprintf(stderr, "error: out of memory (cbBuf=%#x)\n", cbBuf);
                    return 1;
                }
            }

            HDIR  hDir     = HDIR_CREATE;
            ULONG cMatches = cMaxMatches;
            memset(pbBuf, 0xf6, cbBuf);
            APIRET rc = DosFindFirst(pszArg, &hDir, fAttribs, pbBuf, cbBuf, &cMatches, uLevel);
            printf("DosFindFirst -> %lu hDir=%#lx cMatches=%#lx\n", rc, hDir, cMatches);
            if (rc == NO_ERROR)
            {
                do
                {
                    uint8_t *pbTmp = pbBuf;
                    for (uint32_t iMatch = 0; iMatch < cMatches; iMatch++)
                    {
                        uint32_t offNext = *(uint32_t *)pbTmp;
                        switch (uLevel)
                        {
                            case FIL_STANDARD:
                            {
                                PFILEFINDBUF3 pBuf = (PFILEFINDBUF3)pbTmp;
                                printf("#%u: nx=%#lx sz=%#lx at=%#lx nm=%#x:%s\n",
                                       iMatch, pBuf->oNextEntryOffset, pBuf->cbFile, pBuf->attrFile, pBuf->cchName, pBuf->achName);
                                if (strlen(pBuf->achName) != pBuf->cchName)
                                    printf("Bad name length!\n");
                                break;
                            }
                            case FIL_STANDARDL:
                            {
                                PFILEFINDBUF3L pBuf = (PFILEFINDBUF3L)pbTmp;
                                printf("#%u: nx=%#lx sz=%#llx at=%#lx nm=%#x:%s\n",
                                       iMatch, pBuf->oNextEntryOffset, pBuf->cbFile, pBuf->attrFile, pBuf->cchName, pBuf->achName);
                                if (strlen(pBuf->achName) != pBuf->cchName)
                                    printf("Bad name length!\n");
                                break;
                            }
                            case FIL_QUERYEASIZE:
                            {
                                PFILEFINDBUF4 pBuf = (PFILEFINDBUF4)pbTmp;
                                printf("#%u: nx=%#lx sz=%#lx at=%#lx nm=%#x:%s\n",
                                       iMatch, pBuf->oNextEntryOffset, pBuf->cbFile, pBuf->attrFile, pBuf->cchName, pBuf->achName);
                                if (strlen(pBuf->achName) != pBuf->cchName)
                                    printf("Bad name length!\n");
                                break;
                            }
                            case FIL_QUERYEASIZEL:
                            {
                                PFILEFINDBUF4L pBuf = (PFILEFINDBUF4L)pbTmp;
                                printf("#%u: nx=%#lx sz=%#llx at=%#lx nm=%#x:%s\n",
                                       iMatch, pBuf->oNextEntryOffset, pBuf->cbFile, pBuf->attrFile, pBuf->cchName, pBuf->achName);
                                if (strlen(pBuf->achName) != pBuf->cchName)
                                    printf("Bad name length!\n");
                                break;
                            }

                        }

                        pbTmp += offNext;
                    }

                    /* Next bunch. */
                    memset(pbBuf, 0xf6, cbBuf);
                    cMatches = cMaxMatches;
                    rc = DosFindNext(hDir, pbBuf, cbBuf, &cMatches);
                    printf("DosFindNext -> %lu hDir=%#lx cMatches=%#lx\n", rc, hDir, cMatches);
                } while (rc == NO_ERROR);

                rc = DosFindClose(hDir);
                printf("DosFindClose -> %lu\n", rc);
            }
        }
    }
    return 0;
}


static int vboxSfOs2UtilMkDir(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        APIRET rc = DosCreateDir(argv[i], NULL);
        printf("DosCreateDir -> %lu for '%s'\n", rc, argv[i]);
    }
    return 0;
}


int main(int argc, char **argv)
{
    /*
     * Parse input.
     */
    for (int i = 1; i < argc; i++)
    {
        const char *pszArg = argv[i];
        if (strcmp(pszArg, "use") == 0)
            return vboxSfOs2UtilUse(argc - i, argv + i);
        if (strcmp(pszArg, "qpathinfo") == 0)
            return vboxSfOs2UtilQPathInfo(argc - i, argv + i);
        if (strcmp(pszArg, "findfile") == 0)
            return vboxSfOs2UtilFindFile(argc - i, argv + i);
        if (strcmp(pszArg, "mkdir") == 0)
            return vboxSfOs2UtilMkDir(argc - i, argv + i);

        fprintf(stderr,  "Unknown command/option: %s\n", pszArg);
        return 2;
    }
    fprintf(stderr,
            "usage: VBoxSFUtil.exe use [drive] [shared-folder]\n"
            "    or VBoxSFUtil.exe unuse [drive|shared-folder] [..]\n"
            "    or VBoxSFUtil.exe list\n");
    return 2;
}

