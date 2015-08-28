/* $Id$ */
/** @file
 * IPRT - Cryptographic (Certificate) Store, RTCrStoreCertAddFromDir.
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
#include "internal/iprt.h"
#include <iprt/crypto/store.h>

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>


/**
 * Checks if the directory entry matches the specified suffixes.
 *
 * @returns true on match, false on mismatch.
 * @param   pDirEntry           The directory to check.
 * @param   paSuffixes          The array of suffixes to match against.
 * @param   cSuffixes           The number of suffixes in the array.
 */
DECLINLINE(bool) rtCrStoreIsSuffixMatch(PCRTDIRENTRY pDirEntry, PCRTSTRTUPLE paSuffixes, size_t cSuffixes)
{
    if (cSuffixes == 0)
        return true;

    size_t const cchName = pDirEntry->cbName;
    size_t i = cSuffixes;
    while (i-- > 0)
        if (   cchName > paSuffixes[i].cch
            && memcmp(&pDirEntry->szName[cchName - paSuffixes[i].cch], paSuffixes[i].psz, paSuffixes[i].cch) == 0)
            return true;

    return false;
}


RTDECL(int) RTCrStoreCertAddFromDir(RTCRSTORE hStore, uint32_t fFlags, const char *pszDir,
                                    PCRTSTRTUPLE paSuffixes, size_t cSuffixes, PRTERRINFO pErrInfo)
{
    /*
     * Validate input.
     */
    AssertReturn(!(fFlags & ~(RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR)), VERR_INVALID_FLAGS);
    size_t i = cSuffixes;
    while (i-- > 0)
    {
        Assert(paSuffixes[i].cch > 0);
        Assert(strlen(paSuffixes[i].psz) == paSuffixes[i].cch);
    }

    /*
     * Prepare for constructing path to the files in the directory, so that we
     * can open them.
     */
    char szPath[RTPATH_MAX];
    int rc = RTStrCopy(szPath, sizeof(szPath), pszDir);
    if (RT_SUCCESS(rc))
    {
        size_t cchPath = RTPathEnsureTrailingSeparator(szPath, sizeof(szPath));
        if (cchPath > 0)
        {
            size_t const cbFilename = sizeof(szPath) - cchPath;

            /*
             * Enumerate the directory.
             */
            PRTDIR hDir;
            rc = RTDirOpen(&hDir, pszDir);
            if (RT_SUCCESS(rc))
            {
                for (;;)
                {
                    /* Read the next entry. */
                    union
                    {
                        RTDIRENTRY  DirEntry;
                        uint8_t     ab[RTPATH_MAX];
                    } u;
                    size_t cbBuf = sizeof(u);
                    int rc2 = RTDirRead(hDir, &u.DirEntry, &cbBuf);
                    if (RT_SUCCESS(rc2))
                    {
                        if (   !RTDirEntryIsStdDotLink(&u.DirEntry)
                            && rtCrStoreIsSuffixMatch(&u.DirEntry, paSuffixes, cSuffixes) )
                        {
                            if (u.DirEntry.cbName < cbFilename)
                            {
                                memcpy(&szPath[cchPath], u.DirEntry.szName, u.DirEntry.cbName + 1);
                                rc2 = RTDirQueryUnknownType(szPath, true /*fFollowSymlinks*/, &u.DirEntry.enmType);
                                if (   RT_SUCCESS(rc2)
                                    && u.DirEntry.enmType == RTDIRENTRYTYPE_FILE)
                                {
                                    /*
                                     * Add it.
                                     */
                                    rc2 = RTCrStoreCertAddFromFile(hStore, fFlags, szPath, pErrInfo);
                                    if (RT_FAILURE(rc2))
                                    {
                                        rc = rc2;
                                        if (!(fFlags & RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR))
                                            break;
                                    }
                                }
                            }
                            else if (   u.DirEntry.enmType == RTDIRENTRYTYPE_FILE
                                     || u.DirEntry.enmType == RTDIRENTRYTYPE_SYMLINK
                                     || u.DirEntry.enmType == RTDIRENTRYTYPE_UNKNOWN)
                            {
                                rc = RTErrInfoAddF(pErrInfo, VERR_FILENAME_TOO_LONG,
                                                   "  Too long filename (%u bytes)", u.DirEntry.cbName);
                                if (!(fFlags & RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR))
                                    break;
                            }
                        }
                    }
                    else
                    {
                        if (rc2 != VERR_NO_MORE_FILES)
                            rc = RTErrInfoAddF(pErrInfo, rc2, "  RTDirRead failed: %Rrc", rc2);
                        break;
                    }
                }

                RTDirClose(hDir);
            }
            else
                rc = RTErrInfoAddF(pErrInfo, rc, "  RTDirOpen('%s'): %Rrc", pszDir, rc);
        }
        else
            rc = VERR_FILENAME_TOO_LONG;
    }
    return rc;
}


