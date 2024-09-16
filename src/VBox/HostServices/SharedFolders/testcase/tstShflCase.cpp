/** @file
 * Testcase for shared folder case conversion code.
 *
 * @todo r=andy Testcase needs to be rewritten using the test framework.
 *              Otherwise it's almost pointless to run this on the testboxes.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_MISC
#define LOG_ENABLED
#include <VBox/shflsvc.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/dir.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/uni.h>
#include <stdio.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* Override slash for non-windows hosts. */
#undef RTPATH_DELIMITER
#define RTPATH_DELIMITER       '\\'

/* Use our own RTPath and RTDir methods. */
#define RTPathQueryInfo     rtPathQueryInfo
#define RTDirOpenFiltered   rtDirOpenFiltered
#define RTDirClose          rtDirClose
#define RTDirReadEx         rtDirReadEx


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static int iDirList = 0;
static int iDirFile = 0;

static const char *g_apszDirs[] =
{
    "c:",
    "c:\\test dir",
    "c:\\test dir\\SUBDIR",
};

static const char *g_apszDirsC[] =
{
    ".",
    "..",
    "test dir"
};

static const char *g_apszTestdirEntries[] =
{
    ".",
    "..",
    "SUBDIR",
    "a.bat",
    "aTestJe.bat",
    "aTestje.bat",
    "b.bat",
    "c.bat",
    "d.bat",
    "e.bat",
    "f.bat",
    "g.bat",
    "h.bat",
    "x.bat",
    "z.bat",
};

static const char *g_apszSUBDIREntries[] =
{
    ".",
    "..",
    "a.bat",
    "aTestJe.bat",
    "aTestje.bat",
    "b.bat",
    "c.bat",
    "d.bat",
    "e.bat",
    "f.bat",
    "g.bat",
    "h.bat",
    "x.bat",
    "z.bat",
};

static int rtDirOpenFiltered(RTDIR *phDir, const char *pszPath, RTDIRFILTER enmFilter, uint32_t fFlags)
{
    RT_NOREF2(enmFilter, fFlags);
    if (!strcmp(pszPath, "c:\\*"))
        iDirList = 1;
    else if (!strcmp(pszPath, "c:\\test dir\\*"))
        iDirList = 2;
    else if (!strcmp(pszPath, "c:\\test dir\\SUBDIR\\*"))
        iDirList = 3;
    else
        AssertFailed();

    *phDir = (RTDIR)1;
    return VINF_SUCCESS;
}

static int rtDirClose(RTDIR hDir)
{
    RT_NOREF1(hDir);
    iDirFile = 0;
    return VINF_SUCCESS;
}

static int rtDirReadEx(RTDIR hDir, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry, RTFSOBJATTRADD enmAdditionalAttribs, uint32_t fFlags)
{
    RT_NOREF4(hDir, pcbDirEntry, enmAdditionalAttribs, fFlags);
    const char *pszSrcName;
    switch (iDirList)
    {
        case 1:
            if (iDirFile == RT_ELEMENTS(g_apszDirsC))
                return VERR_NO_MORE_FILES;
            pszSrcName = g_apszDirsC[iDirFile];
            break;
        case 2:
            if (iDirFile == RT_ELEMENTS(g_apszTestdirEntries))
                return VERR_NO_MORE_FILES;
            pszSrcName = g_apszTestdirEntries[iDirFile];
            break;
        case 3:
            if (iDirFile == RT_ELEMENTS(g_apszSUBDIREntries))
                return VERR_NO_MORE_FILES;
            pszSrcName = g_apszSUBDIREntries[iDirFile];
            break;
        default:
            AssertFailed();
            return VERR_NO_MORE_FILES;
    }

    size_t const cbDirEntry  = *pcbDirEntry;
    size_t const cbNameAvail = cbDirEntry - RT_UOFFSETOF(RTDIRENTRYEX, szName);
    size_t const cchSrcName  = strlen(pszSrcName);
    *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + cchSrcName + 1;
    pDirEntry->cbName = (uint16_t)cchSrcName;
    AssertReturn(cchSrcName < cbNameAvail, VERR_BUFFER_OVERFLOW);

    memcpy(pDirEntry->szName, pszSrcName, cchSrcName + 1);
    iDirFile++;
    return VINF_SUCCESS;
}

static int rtPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    RT_NOREF2(pObjInfo, enmAdditionalAttribs);
    int cMax;

    /* first try g_apszDirs */
    for (unsigned int i=0;i<RT_ELEMENTS(g_apszDirs);i++)
    {
        if(!strcmp(pszPath, g_apszDirs[i]))
            return VINF_SUCCESS;
    }

    const char **papszDirList;
    switch (iDirList)
    {
        case 1:
            cMax = RT_ELEMENTS(g_apszDirsC);
            papszDirList = g_apszDirsC;
            break;
        case 2:
            cMax = RT_ELEMENTS(g_apszTestdirEntries);
            papszDirList = g_apszTestdirEntries;
            break;
        case 3:
            cMax = RT_ELEMENTS(g_apszSUBDIREntries);
            papszDirList = g_apszSUBDIREntries;
            break;
        default:
            return VERR_FILE_NOT_FOUND;
    }
    for (int i = 0; i < cMax; i++)
    {
        if (!strcmp(pszPath, papszDirList[i]))
            return VINF_SUCCESS;
    }
    return VERR_FILE_NOT_FOUND;
}

static int vbsfCorrectCasing(char *pszFullPath, char *pszStartComponent)
{
    size_t const   cchComponent   = strlen(pszStartComponent);

    Log2(("vbsfCorrectCasing: '%s' '%s' (%zu)\n", pszFullPath, pszStartComponent, cchComponent));

    uint32_t const cbDirEntry     = 4096;
    uint32_t const cbDirEntryName = cbDirEntry - RT_UOFFSETOF(RTDIRENTRYEX, szName);
    PRTDIRENTRYEX  pDirEntry  = (PRTDIRENTRYEX)RTMemAlloc(cbDirEntry);
    AssertReturn(pDirEntry, VERR_NO_MEMORY);

    /** @todo this is quite inefficient, especially for directories with many files */
    Assert(pszFullPath < pszStartComponent - 1);
    Assert(pszStartComponent[-1] == RTPATH_DELIMITER);
    pszStartComponent[-1] = '\0';
    int rc = RTStrCopy(pDirEntry->szName, cbDirEntryName - sizeof(RTPATH_SLASH_STR "*"), pszFullPath);
    pszStartComponent[-1] = RTPATH_DELIMITER;
    if (RT_SUCCESS(rc))
    {
        char szWildCard[4];
        szWildCard[0] = RTPATH_DELIMITER;
        szWildCard[1] = '*';
        szWildCard[2] = '\0';
        rc = RTStrCat(pDirEntry->szName, cbDirEntryName, szWildCard);
    }
    if (RT_SUCCESS(rc))
    {
        RTDIR hSearch = NIL_RTDIR;
        rc = RTDirOpenFiltered(&hSearch, pDirEntry->szName, RTDIRFILTER_WINNT, 0 /*fFlags*/);
        if (RT_SUCCESS(rc))
        {
            for(;;)
            {
                size_t cbDirEntryRet = cbDirEntry;
                rc = RTDirReadEx(hSearch, pDirEntry, &cbDirEntryRet, RTFSOBJATTRADD_NOTHING, RTPATH_F_FOLLOW_LINK);
                if (rc == VERR_NO_MORE_FILES)
                    break;

                if (VINF_SUCCESS != rc && rc != VWRN_NO_DIRENT_INFO)
                {
                    AssertFailed();
                    if (rc != VERR_NO_TRANSLATION)
                        break;
                    continue;
                }

                Log2(("vbsfCorrectCasing: found '%s'\n", &pDirEntry->szName[0]));
                if (   pDirEntry->cbName == cchComponent
                    && !RTStrICmp(pszStartComponent, &pDirEntry->szName[0]))
                {
                    Log(("Found original name %s (%s)\n", &pDirEntry->szName[0], pszStartComponent));
                    memcpy(pszStartComponent, &pDirEntry->szName[0], cchComponent);
                    pszStartComponent[cchComponent] = '\0'; /* paranoia */
                    rc = VINF_SUCCESS;
                    break;
                }
            }
            if (RT_FAILURE(rc))
                Log(("vbsfCorrectCasing %s failed with %d\n", pszStartComponent, rc));

            RTDirClose(hSearch);
        }
    }

    RTMemFree(pDirEntry);
    return rc;
}



static int testCase(char *pszFullPath, bool fWildCard = false)
{
    int rc;
    RTFSOBJINFO info;
    char *pszWildCardComponent = NULL;

    if (fWildCard)
    {
        /* strip off the last path component, that contains the wildcard(s) */
        size_t cchLlen = strlen(pszFullPath);
        char  *pszSrc  = pszFullPath + cchLlen - 1;

        while(pszSrc > pszFullPath)
        {
            if (*pszSrc == RTPATH_DELIMITER)
                break;
            pszSrc--;
        }
        if (*pszSrc == RTPATH_DELIMITER)
        {
            bool fHaveWildcards = false;
            char *temp = pszSrc;

            while(*temp)
            {
                char uc = *temp;
                /** @todo should depend on the guest OS */
                if (uc == '*' || uc == '?' || uc == '>' || uc == '<' || uc == '"')
                {
                    fHaveWildcards = true;
                    break;
                }
                temp++;
            }

            if (fHaveWildcards)
            {
                pszWildCardComponent = pszSrc;
                *pszWildCardComponent = 0;
            }
        }
    }

    rc = RTPathQueryInfo(pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
    if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
    {
        char *pszSrc = &pszFullPath[strlen(pszFullPath) - 1];

        Log(("Handle case insensitive guest fs on top of host case sensitive fs for %s\n", pszFullPath));

        /* Find partial path that's valid */
        while (pszSrc > pszFullPath)
        {
            if (*pszSrc == RTPATH_DELIMITER)
            {
                *pszSrc = '\0';
                rc = RTPathQueryInfo (pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
                *pszSrc = RTPATH_DELIMITER;
                if (rc == VINF_SUCCESS)
                {
#ifdef DEBUG
                    *pszSrc = '\0';
                    Log(("Found valid partial path %s\n", pszFullPath));
                    *pszSrc = RTPATH_DELIMITER;
#endif
                    break;
                }
            }

            pszSrc--;
        }
        Assert(*pszSrc == RTPATH_DELIMITER && RT_SUCCESS(rc));
        if (    *pszSrc == RTPATH_DELIMITER
            &&  RT_SUCCESS(rc))
        {
            pszSrc++;
            for(;;)
            {
                char *end = pszSrc;
                bool fEndOfString = true;

                while(*end)
                {
                    if (*end == RTPATH_DELIMITER)
                        break;
                    end++;
                }

                if (*end == RTPATH_DELIMITER)
                {
                    fEndOfString = false;
                    *end = 0;
                    rc = RTPathQueryInfo(pszSrc, &info, RTFSOBJATTRADD_NOTHING);
                    Assert(rc == VINF_SUCCESS || rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND);
                }
                else if (end == pszSrc)
                    rc = VINF_SUCCESS;  /* trailing delimiter */
                else
                    rc = VERR_FILE_NOT_FOUND;

                if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
                {
                    /* path component is invalid; try to correct the casing */
                    rc = vbsfCorrectCasing(pszFullPath, pszSrc);
                    if (RT_FAILURE(rc))
                    {
                        if (!fEndOfString)
                            *end = RTPATH_DELIMITER;
                        break;
                    }
                }

                if (fEndOfString)
                    break;

                *end = RTPATH_DELIMITER;
                pszSrc = end + 1;
            }
            if (RT_FAILURE(rc))
                Log(("Unable to find suitable component rc=%d\n", rc));
        }
        else
            rc = VERR_FILE_NOT_FOUND;

    }
    if (pszWildCardComponent)
        *pszWildCardComponent = RTPATH_DELIMITER;

    if (RT_SUCCESS(rc))
        Log(("New valid path %s\n", pszFullPath));
    else
        Log(("Old invalid path %s\n", pszFullPath));
    return rc;
}


int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstShflCase", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    RTLogFlush(NULL);
    RTLogDestinations(NULL, "stdout");
    RTLogGroupSettings(NULL, "misc=~0");
    RTLogFlags(NULL, "unbuffered");

    static const struct
    {
        int         rcExpect;
        bool        fWildcard;
        const char *pszPath;
        size_t      cchPath;
    } s_aTests[] =
    {
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\test Dir\\z.bAt") },
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\test dir\\z.bAt") },
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\test dir\\SUBDIR\\z.bAt") },
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\test dir\\SUBDiR\\atestje.bat") },
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\TEST dir\\subDiR\\aTestje.baT") },
        { VINF_SUCCESS,          true, RT_STR_TUPLE("c:\\TEST dir\\subDiR\\*") },
        { VINF_SUCCESS,          true, RT_STR_TUPLE("c:\\TEST dir\\subDiR\\") },
        { VINF_SUCCESS,         false, RT_STR_TUPLE("c:\\test dir\\SUBDIR\\") },
        { VERR_NO_MORE_FILES,   false, RT_STR_TUPLE("c:\\test dir\\invalid\\SUBDIR\\test.bat") },
    };
    for (size_t i = 0; i < RT_ELEMENTS(s_aTests); i++)
    {
        char szTest[128];
        AssertContinueStmt(s_aTests[i].cchPath < sizeof(szTest), RTTestFailed(hTest, "too long: #%zu", i));
        memcpy(szTest, s_aTests[i].pszPath, s_aTests[i].cchPath + 1);
        int rc = testCase(szTest, s_aTests[i].fWildcard);
        if (rc != s_aTests[i].rcExpect)
            RTTestFailed(hTest, "testCase returned %Rrc, expected %Rrc (path: %s)",
                         rc, s_aTests[i].rcExpect, s_aTests[i].pszPath);
    }
    return RTTestSummaryAndDestroy(hTest);
}

