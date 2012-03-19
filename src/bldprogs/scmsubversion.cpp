/* $Id$ */
/** @file
 * IPRT Testcase / Tool - Source Code Massager, Subversion Access.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define SCM_WITHOUT_LIBSVN

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "scm.h"



#ifdef SCM_WITHOUT_LIBSVN

/**
 * Callback that is call for each path to search.
 */
static DECLCALLBACK(int) scmSvnFindSvnBinaryCallback(char const *pchPath, size_t cchPath, void *pvUser1, void *pvUser2)
{
    char   *pszDst = (char *)pvUser1;
    size_t  cchDst = (size_t)pvUser2;
    if (cchDst > cchPath)
    {
        memcpy(pszDst, pchPath, cchPath);
        pszDst[cchPath] = '\0';
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        int rc = RTPathAppend(pszDst, cchDst, "svn.exe");
#else
        int rc = RTPathAppend(pszDst, cchDst, "svn");
#endif
        if (   RT_SUCCESS(rc)
            && RTFileExists(pszDst))
            return VINF_SUCCESS;
    }
    return VERR_TRY_AGAIN;
}


/**
 * Finds the svn binary.
 *
 * @param   pszPath             Where to store it.  Worst case, we'll return
 *                              "svn" here.
 * @param   cchPath             The size of the buffer pointed to by @a pszPath.
 */
static void scmSvnFindSvnBinary(char *pszPath, size_t cchPath)
{
    /** @todo code page fun... */
    Assert(cchPath >= sizeof("svn"));
#ifdef RT_OS_WINDOWS
    const char *pszEnvVar = RTEnvGet("Path");
#else
    const char *pszEnvVar = RTEnvGet("PATH");
#endif
    if (pszPath)
    {
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        int rc = RTPathTraverseList(pszEnvVar, ';', scmSvnFindSvnBinaryCallback, pszPath, (void *)cchPath);
#else
        int rc = RTPathTraverseList(pszEnvVar, ':', scmSvnFindSvnBinaryCallback, pszPath, (void *)cchPath);
#endif
        if (RT_SUCCESS(rc))
            return;
    }
    strcpy(pszPath, "svn");
}


/**
 * Construct a dot svn filename for the file being rewritten.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state (for the name).
 * @param   pszDir              The directory, including ".svn/".
 * @param   pszSuff             The filename suffix.
 * @param   pszDst              The output buffer.  RTPATH_MAX in size.
 */
static int scmSvnConstructName(PSCMRWSTATE pState, const char *pszDir, const char *pszSuff, char *pszDst)
{
    strcpy(pszDst, pState->pszFilename); /* ASSUMES sizeof(szBuf) <= sizeof(szPath) */
    RTPathStripFilename(pszDst);

    int rc = RTPathAppend(pszDst, RTPATH_MAX, pszDir);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(pszDst, RTPATH_MAX, RTPathFilename(pState->pszFilename));
        if (RT_SUCCESS(rc))
        {
            size_t cchDst  = strlen(pszDst);
            size_t cchSuff = strlen(pszSuff);
            if (cchDst + cchSuff < RTPATH_MAX)
            {
                memcpy(&pszDst[cchDst], pszSuff, cchSuff + 1);
                return VINF_SUCCESS;
            }
            else
                rc = VERR_BUFFER_OVERFLOW;
        }
    }
    return rc;
}

/**
 * Interprets the specified string as decimal numbers.
 *
 * @returns true if parsed successfully, false if not.
 * @param   pch                 The string (not terminated).
 * @param   cch                 The string length.
 * @param   pu                  Where to return the value.
 */
static bool scmSvnReadNumber(const char *pch, size_t cch, size_t *pu)
{
    size_t u = 0;
    while (cch-- > 0)
    {
        char ch = *pch++;
        if (ch < '0' || ch > '9')
            return false;
        u *= 10;
        u += ch - '0';
    }
    *pu = u;
    return true;
}

#endif /* SCM_WITHOUT_LIBSVN */

/**
 * Checks if the file we're operating on is part of a SVN working copy.
 *
 * @returns true if it is, false if it isn't or we cannot tell.
 * @param   pState              The rewrite state to work on.
 */
bool ScmSvnIsInWorkingCopy(PSCMRWSTATE pState)
{
#ifdef SCM_WITHOUT_LIBSVN
    /*
     * Hack: check if the .svn/text-base/<file>.svn-base file exists.
     */
    char szPath[RTPATH_MAX];
    int rc = scmSvnConstructName(pState, ".svn/text-base/", ".svn-base", szPath);
    if (RT_SUCCESS(rc))
        return RTFileExists(szPath);

#else
    NOREF(pState);
#endif
    return false;
}

/**
 * Queries the value of an SVN property.
 *
 * This will automatically adjust for scheduled changes.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_STATE if not a SVN WC file.
 * @retval  VERR_NOT_FOUND if the property wasn't found.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The property name.
 * @param   ppszValue           Where to return the property value.  Free this
 *                              using RTStrFree.  Optional.
 */
int ScmSvnQueryProperty(PSCMRWSTATE pState, const char *pszName, char **ppszValue)
{
    /*
     * Look it up in the scheduled changes.
     */
    uint32_t i = pState->cSvnPropChanges;
    while (i-- > 0)
        if (!strcmp(pState->paSvnPropChanges[i].pszName, pszName))
        {
            const char *pszValue = pState->paSvnPropChanges[i].pszValue;
            if (!pszValue)
                return VERR_NOT_FOUND;
            if (ppszValue)
                return RTStrDupEx(ppszValue, pszValue);
            return VINF_SUCCESS;
        }

#ifdef SCM_WITHOUT_LIBSVN
    /*
     * Hack: Read the .svn/props/<file>.svn-work file exists.
     */
    char szPath[RTPATH_MAX];
    int rc = scmSvnConstructName(pState, ".svn/props/", ".svn-work", szPath);
    if (RT_SUCCESS(rc) && !RTFileExists(szPath))
        rc = scmSvnConstructName(pState, ".svn/prop-base/", ".svn-base", szPath);
    if (RT_SUCCESS(rc))
    {
        SCMSTREAM Stream;
        rc = ScmStreamInitForReading(&Stream, szPath);
        if (RT_SUCCESS(rc))
        {
            /*
             * The current format is K len\n<name>\nV len\n<value>\n" ... END.
             */
            rc = VERR_NOT_FOUND;
            size_t const    cchName = strlen(pszName);
            SCMEOL          enmEol;
            size_t          cchLine;
            const char     *pchLine;
            while ((pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol)) != NULL)
            {
                /*
                 * Parse the 'K num' / 'END' line.
                 */
                if (   cchLine == 3
                    && !memcmp(pchLine, "END", 3))
                    break;
                size_t cchKey;
                if (   cchLine < 3
                    || pchLine[0] != 'K'
                    || pchLine[1] != ' '
                    || !scmSvnReadNumber(&pchLine[2], cchLine - 2, &cchKey)
                    || cchKey == 0
                    || cchKey > 4096)
                {
                    RTMsgError("%s:%u: Unexpected data '%.*s'\n", szPath, ScmStreamTellLine(&Stream), cchLine, pchLine);
                    rc = VERR_PARSE_ERROR;
                    break;
                }

                /*
                 * Match the key and skip to the value line.  Don't bother with
                 * names containing EOL markers.
                 */
                size_t const offKey = ScmStreamTell(&Stream);
                bool fMatch = cchName == cchKey;
                if (fMatch)
                {
                    pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol);
                    if (!pchLine)
                        break;
                    fMatch = cchLine == cchName
                          && !memcmp(pchLine, pszName, cchName);
                }

                if (RT_FAILURE(ScmStreamSeekAbsolute(&Stream, offKey + cchKey)))
                    break;
                if (RT_FAILURE(ScmStreamSeekByLine(&Stream, ScmStreamTellLine(&Stream) + 1)))
                    break;

                /*
                 * Read and Parse the 'V num' line.
                 */
                pchLine = ScmStreamGetLine(&Stream, &cchLine, &enmEol);
                if (!pchLine)
                    break;
                size_t cchValue;
                if (   cchLine < 3
                    || pchLine[0] != 'V'
                    || pchLine[1] != ' '
                    || !scmSvnReadNumber(&pchLine[2], cchLine - 2, &cchValue)
                    || cchValue > _1M)
                {
                    RTMsgError("%s:%u: Unexpected data '%.*s'\n", szPath, ScmStreamTellLine(&Stream), cchLine, pchLine);
                    rc = VERR_PARSE_ERROR;
                    break;
                }

                /*
                 * If we have a match, allocate a return buffer and read the
                 * value into it.  Otherwise skip this value and continue
                 * searching.
                 */
                if (fMatch)
                {
                    if (!ppszValue)
                        rc = VINF_SUCCESS;
                    else
                    {
                        char *pszValue;
                        rc = RTStrAllocEx(&pszValue, cchValue + 1);
                        if (RT_SUCCESS(rc))
                        {
                            rc = ScmStreamRead(&Stream, pszValue, cchValue);
                            if (RT_SUCCESS(rc))
                                *ppszValue = pszValue;
                            else
                                RTStrFree(pszValue);
                        }
                    }
                    break;
                }

                if (RT_FAILURE(ScmStreamSeekRelative(&Stream, cchValue)))
                    break;
                if (RT_FAILURE(ScmStreamSeekByLine(&Stream, ScmStreamTellLine(&Stream) + 1)))
                    break;
            }

            if (RT_FAILURE(ScmStreamGetStatus(&Stream)))
            {
                rc = ScmStreamGetStatus(&Stream);
                RTMsgError("%s: stream error %Rrc\n", szPath, rc);
            }
            ScmStreamDelete(&Stream);
        }
    }

    if (rc == VERR_FILE_NOT_FOUND)
        rc = VERR_NOT_FOUND;
    return rc;

#else
    NOREF(pState);
#endif
    return VERR_NOT_FOUND;
}


/**
 * Schedules the setting of a property.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_STATE if not a SVN WC file.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The name of the property to set.
 * @param   pszValue            The value.  NULL means deleting it.
 */
int ScmSvnSetProperty(PSCMRWSTATE pState, const char *pszName, const char *pszValue)
{
    /*
     * Update any existing entry first.
     */
    size_t i = pState->cSvnPropChanges;
    while (i-- > 0)
        if (!strcmp(pState->paSvnPropChanges[i].pszName,  pszName))
        {
            if (!pszValue)
            {
                RTStrFree(pState->paSvnPropChanges[i].pszValue);
                pState->paSvnPropChanges[i].pszValue = NULL;
            }
            else
            {
                char *pszCopy;
                int rc = RTStrDupEx(&pszCopy, pszValue);
                if (RT_FAILURE(rc))
                    return rc;
                pState->paSvnPropChanges[i].pszValue = pszCopy;
            }
            return VINF_SUCCESS;
        }

    /*
     * Insert a new entry.
     */
    i = pState->cSvnPropChanges;
    if ((i % 32) == 0)
    {
        void *pvNew = RTMemRealloc(pState->paSvnPropChanges, (i + 32) * sizeof(SCMSVNPROP));
        if (!pvNew)
            return VERR_NO_MEMORY;
        pState->paSvnPropChanges = (PSCMSVNPROP)pvNew;
    }

    pState->paSvnPropChanges[i].pszName  = RTStrDup(pszName);
    pState->paSvnPropChanges[i].pszValue = pszValue ? RTStrDup(pszValue) : NULL;
    if (   pState->paSvnPropChanges[i].pszName
        && (pState->paSvnPropChanges[i].pszValue || !pszValue) )
        pState->cSvnPropChanges = i + 1;
    else
    {
        RTStrFree(pState->paSvnPropChanges[i].pszName);
        pState->paSvnPropChanges[i].pszName = NULL;
        RTStrFree(pState->paSvnPropChanges[i].pszValue);
        pState->paSvnPropChanges[i].pszValue = NULL;
        return VERR_NO_MEMORY;
    }
    return VINF_SUCCESS;
}


/**
 * Schedules a property deletion.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state to work on.
 * @param   pszName             The name of the property to delete.
 */
int ScmSvnDelProperty(PSCMRWSTATE pState, const char *pszName)
{
    return ScmSvnSetProperty(pState, pszName, NULL);
}


/**
 * Applies any SVN property changes to the work copy of the file.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state which SVN property changes
 *                              should be applied.
 */
int ScmSvnDisplayChanges(PSCMRWSTATE pState)
{
    size_t i = pState->cSvnPropChanges;
    while (i-- > 0)
    {
        const char *pszName  = pState->paSvnPropChanges[i].pszName;
        const char *pszValue = pState->paSvnPropChanges[i].pszValue;
        if (pszValue)
            ScmVerbose(pState, 0, "svn ps '%s' '%s'  %s\n", pszName, pszValue, pState->pszFilename);
        else
            ScmVerbose(pState, 0, "svn pd '%s'  %s\n", pszName, pszValue, pState->pszFilename);
    }

    return VINF_SUCCESS;
}

/**
 * Applies any SVN property changes to the work copy of the file.
 *
 * @returns IPRT status code.
 * @param   pState              The rewrite state which SVN property changes
 *                              should be applied.
 */
int ScmSvnApplyChanges(PSCMRWSTATE pState)
{
#ifdef SCM_WITHOUT_LIBSVN
    /*
     * This sucks. We gotta find svn(.exe).
     */
    static char s_szSvnPath[RTPATH_MAX];
    if (s_szSvnPath[0] == '\0')
        scmSvnFindSvnBinary(s_szSvnPath, sizeof(s_szSvnPath));

    /*
     * Iterate thru the changes and apply them by starting the svn client.
     */
    for (size_t i = 0; i < pState->cSvnPropChanges; i++)
    {
        const char *apszArgv[6];
        apszArgv[0] = s_szSvnPath;
        apszArgv[1] = pState->paSvnPropChanges[i].pszValue ? "ps" : "pd";
        apszArgv[2] = pState->paSvnPropChanges[i].pszName;
        int iArg = 3;
        if (pState->paSvnPropChanges[i].pszValue)
            apszArgv[iArg++] = pState->paSvnPropChanges[i].pszValue;
        apszArgv[iArg++] = pState->pszFilename;
        apszArgv[iArg++] = NULL;
        ScmVerbose(pState, 2, "executing: %s %s %s %s %s\n",
                   apszArgv[0], apszArgv[1], apszArgv[2], apszArgv[3], apszArgv[4]);

        RTPROCESS pid;
        int rc = RTProcCreate(s_szSvnPath, apszArgv, RTENV_DEFAULT, 0 /*fFlags*/, &pid);
        if (RT_SUCCESS(rc))
        {
            RTPROCSTATUS Status;
            rc = RTProcWait(pid, RTPROCWAIT_FLAGS_BLOCK, &Status);
            if (    RT_SUCCESS(rc)
                &&  (   Status.enmReason != RTPROCEXITREASON_NORMAL
                     || Status.iStatus != 0) )
            {
                RTMsgError("%s: %s %s %s %s %s -> %s %u\n",
                           pState->pszFilename, apszArgv[0], apszArgv[1], apszArgv[2], apszArgv[3], apszArgv[4],
                           Status.enmReason == RTPROCEXITREASON_NORMAL   ? "exit code"
                           : Status.enmReason == RTPROCEXITREASON_SIGNAL ? "signal"
                           : Status.enmReason == RTPROCEXITREASON_ABEND  ? "abnormal end"
                           : "abducted by alien",
                           Status.iStatus);
                return VERR_GENERAL_FAILURE;
            }
        }
        if (RT_FAILURE(rc))
        {
            RTMsgError("%s: error executing %s %s %s %s %s: %Rrc\n",
                       pState->pszFilename, apszArgv[0], apszArgv[1], apszArgv[2], apszArgv[3], apszArgv[4], rc);
            return rc;
        }
    }

    return VINF_SUCCESS;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}



