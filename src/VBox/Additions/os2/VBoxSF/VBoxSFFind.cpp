/** $Id$ */
/** @file
 * VBoxSF - OS/2 Shared Folders, Find File IFS EPs.
 */

/*
 * Copyright (c) 2007-2018 knut st. osmundsen <bird-src-spam@anduin.net>
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
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxSFInternal.h"

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/err.h>



/**
 * Checks if the given name is 8-dot-3 compatible.
 *
 * @returns true if compatible, false if not.
 * @param   pszName     The name to inspect (UTF-8).
 * @param   cchName     The length of the name.
 * @param   pwszTmp     Buffer for test conversions.
 * @param   cwcTmp      The size of the buffer.
 */
static bool vboxSfOs2IsUtf8Name8dot3(const char *pszName, size_t cchName, PRTUTF16 pwszTmp, size_t cwcTmp)
{
    /* Reject names that must be too long when using maximum UTF-8 encoding. */
    if (cchName > (8 + 1 + 3) * 4)
        return false;

    /* First char cannot be a dot. */
    if (*pszName == '.' || !*pszName)
        return false;

    /*
     * To basic checks on code point level before doing full conversion.
     */
    const char *pszCursor = pszName;
    for (uint32_t cuc = 0; ; cuc++)
    {
        RTUNICP uCp;
        RTStrGetCpEx(&pszCursor, &uCp);
        if (uCp == '.')
        {
            for (uint32_t cuc = 0; ; cuc++)
            {
                RTUNICP uCp;
                RTStrGetCpEx(&pszCursor, &uCp);
                if (!uCp)
                    break;
                if (uCp == '.')
                    return false;
                if (cuc >= 3)
                    return false;
            }
            break;
        }
        if (!uCp)
            break;
        if (cuc >= 8)
            return false;
    }

    /*
     * Convert to UTF-16 and then to native codepage.
     */
    size_t cwcActual = cwcTmp;
    int rc = RTStrToUtf16Ex(pszName, cchName, &pwszTmp, cwcTmp, &cwcActual);
    if (RT_SUCCESS(rc))
    {
        char *pszTmp = (char *)&pwszTmp[cwcActual + 1];
        rc = KernStrFromUcs(NULL, pszTmp, pwszTmp, (cwcTmp - cwcActual - 1) * sizeof(RTUTF16), cwcActual);
        if (rc != NO_ERROR)
        {
            LogRel(("vboxSfOs2IsUtf8Name8dot3: KernStrFromUcs failed: %d\n", rc));
            return false;
        }

        /*
         * Redo the check.
         * Note! This could be bogus if a DBCS leadin sequence collides with '.'.
         */
        for (uint32_t cch = 0; ; cch++)
        {
            char ch = *pszTmp++;
            if (ch == '.')
                break;
            if (ch == '\0')
                return true;
            if (cch >= 8)
                return false;
        }
        for (uint32_t cch = 0; ; cch++)
        {
            char ch = *pszTmp++;
            if (ch == '\0')
                return true;
            if (ch != '.')
                return false;
            if (cch >= 3)
                return false;
        }
    }
    else
        LogRel(("vboxSfOs2IsUtf8Name8dot3: RTStrToUtf16Ex failed: %Rrc\n", rc));
    return false;
}


/**
 * @returns Updated pbDst on success, NULL on failure.
 */
static uint8_t *vboxSfOs2CopyUtf8Name(uint8_t *pbDst, PRTUTF16 pwszTmp, size_t cwcTmp, const char *pszSrc, size_t cchSrc)
{
    /* Convert UTF-8 to UTF-16: */
    int rc = RTStrToUtf16Ex(pszSrc, cchSrc, &pwszTmp, cwcTmp, &cwcTmp);
    if (RT_SUCCESS(rc))
    {
        char *pszDst = (char *)(pbDst + 1);
        rc = KernStrFromUcs(NULL, pszDst, pwszTmp, CCHMAXPATHCOMP, cwcTmp);
        if (rc == NO_ERROR)
        {
            size_t cchDst = strlen(pszDst);
            *pbDst++ = (uint8_t)cchDst;
            pbDst   += cchDst;
            *pbDst++ = '\0';
            return pbDst;
        }
        LogRel(("vboxSfOs2CopyUtf8Name: KernStrFromUcs failed: %d\n", rc));
    }
    else
        LogRel(("vboxSfOs2CopyUtf8Name: RTStrToUtf16Ex failed: %Rrc\n", rc));
    return NULL;
}


/**
 * @returns Updated pbDst on success, NULL on failure.
 */
static uint8_t *vboxSfOs2CopyUtf8NameAndUpperCase(uint8_t *pbDst, PRTUTF16 pwszTmp, size_t cwcTmp, const char *pszSrc, size_t cchSrc)
{
    /* Convert UTF-8 to UTF-16: */
    int rc = RTStrToUtf16Ex(pszSrc, cchSrc, &pwszTmp, cwcTmp, &cwcTmp);
    if (RT_SUCCESS(rc))
    {
        char *pszDst = (char *)(pbDst + 1);
        rc = KernStrFromUcs(NULL, pszDst, RTUtf16ToUpper(pwszTmp), CCHMAXPATHCOMP, cwcTmp);
        if (rc == NO_ERROR)
        {
            size_t cchDst = strlen(pszDst);
            *pbDst++ = (uint8_t)cchDst;
            pbDst   += cchDst;
            *pbDst++ = '\0';
            return pbDst;
        }
        LogRel(("vboxSfOs2CopyUtf8NameAndUpperCase: KernStrFromUcs failed: %d\n", rc));
    }
    else
        LogRel(("vboxSfOs2CopyUtf8NameAndUpperCase: RTStrToUtf16Ex failed: %Rrc\n", rc));
    return NULL;
}


/**
 * @returns Updated pbDst on success, NULL on failure.
 */
static uint8_t *vboxSfOs2CopyUtf16NameAndUpperCase(uint8_t *pbDst, PRTUTF16 pwszSrc, size_t cwcSrc)
{
    char *pszDst = (char *)(pbDst + 1);
    APIRET rc = KernStrFromUcs(NULL, pszDst, RTUtf16ToUpper(pwszSrc), CCHMAXPATHCOMP, cwcSrc);
    if (rc == NO_ERROR)
    {
        size_t cchDst = strlen(pszDst);
        *pbDst++ = (uint8_t)cchDst;
        pbDst   += cchDst;
        *pbDst++ = '\0';
        return pbDst;
    }
    LogRel(("vboxSfOs2CopyUtf16NameAndUpperCase: KernStrFromUcs failed: %#x\n", rc));
    return NULL;
}



/**
 * Worker for FS32_FINDFIRST, FS32_FINDNEXT and FS32_FINDFROMNAME.
 *
 * @returns OS/2 status code.
 * @param   pFolder     The folder we're working on.
 * @param   pFsFsd      The search handle data.
 * @param   pDataBuf    The search data buffer (some handle data there too).
 * @param   uLevel      The info level to return.
 * @param   fFlags      Position flag.
 * @param   pbData      The output buffer.
 * @param   cbData      The size of the output buffer.
 * @param   cMaxMatches The maximum number of matches to return.
 * @param   pcMatches   Where to set the number of returned matches.
 */
static APIRET vboxSfOs2ReadDirEntries(PVBOXSFFOLDER pFolder, PVBOXSFFS pFsFsd, PVBOXSFFSBUF pDataBuf, ULONG uLevel, ULONG fFlags,
                                      PBYTE pbData, ULONG cbData, USHORT cMaxMatches, PUSHORT pcMatches)
{
    APIRET rc = NO_ERROR;

    /*
     * If we're doing EAs, the buffer starts with an EAOP structure.
     */
    EAOP    EaOp;
    PEAOP   pEaOpUser;
    switch (uLevel)
    {
        case FI_LVL_EAS_FROM_LIST:
        case FI_LVL_EAS_FROM_LIST_64:
        case FI_LVL_EAS_FULL:
        case FI_LVL_EAS_FULL_5:
        case FI_LVL_EAS_FULL_8:
            if (cbData >= sizeof(EaOp))
            {
                rc = KernCopyIn(&EaOp, pbData, sizeof(EaOp));
                if (rc == NO_ERROR)
                {
                    EaOp.fpGEAList = (PGEALIST)KernSelToFlat((uintptr_t)EaOp.fpGEAList);
                    EaOp.fpFEAList = NULL;

                    pEaOpUser = (PEAOP)pbData;
                    pbData += sizeof(*pEaOpUser);
                    cbData -= sizeof(*pEaOpUser);
                    break;
                }
            }
            else
                rc = ERROR_BUFFER_OVERFLOW;
            Log(("vboxSfOs2ReadDirEntries: Failed to read EAOP: %u\n", rc));
            return rc;
    }

    /*
     * Do the reading.
     */
    USHORT cMatches;
    for (cMatches = 0; cMatches < cMaxMatches;)
    {
        /*
         * Do we need to fetch more directory entries?
         */
        PSHFLDIRINFO pEntry = pDataBuf->pEntry;
        if (   pDataBuf->cEntriesLeft == 0
            || pEntry == NULL /* paranoia */)
        {
            pDataBuf->pEntry  = pEntry = (PSHFLDIRINFO)(pDataBuf + 1);
            pDataBuf->cbValid = pDataBuf->cbBuf - sizeof(*pDataBuf);
            int vrc = VbglR0SfDirInfo(&g_SfClient, &pFolder->hHostFolder, pFsFsd->hHostDir, pDataBuf->pFilter,
                                      cMaxMatches == 1 ? SHFL_LIST_RETURN_ONE : 0, 0 /*index*/, &pDataBuf->cbValid,
                                      pEntry, &pDataBuf->cEntriesLeft);
            if (RT_SUCCESS(vrc))
            {
                AssertReturn(pDataBuf->cbValid >= RT_UOFFSETOF(SHFLDIRINFO, name.String), ERROR_SYS_INTERNAL);
                AssertReturn(pDataBuf->cbValid >= RT_UOFFSETOF(SHFLDIRINFO, name.String) + pEntry->name.u16Size, ERROR_SYS_INTERNAL);
                Log4(("vboxSfOs2ReadDirEntries: VbglR0SfDirInfo returned %#x matches in %#x bytes\n", pDataBuf->cEntriesLeft, pDataBuf->cbValid));
            }
            else
            {
                if (vrc == VERR_NO_MORE_FILES)
                    Log(("vboxSfOs2ReadDirEntries: VbglR0SfDirInfo failed %Rrc (%d,%d)\n", vrc, pDataBuf->cEntriesLeft, pDataBuf->cbValid));
                else
                    Log4(("vboxSfOs2ReadDirEntries: VbglR0SfDirInfo returned VERR_NO_MORE_FILES (%d,%d)\n", pDataBuf->cEntriesLeft, pDataBuf->cbValid));
                pDataBuf->pEntry       = NULL;
                pDataBuf->cEntriesLeft = 0;
                if (cMatches == 0)
                {
                    if (vrc == VERR_NO_MORE_FILES)
                        rc = ERROR_NO_MORE_FILES;
                    else
                        rc = vboxSfOs2ConvertStatusToOs2(vrc, ERROR_GEN_FAILURE);
                }
                break;
            }
        }

        /*
         * Do matching and stuff the return buffer.
         */
        if (   !((pEntry->Info.Attr.fMode >> RTFS_DOS_SHIFT) & pDataBuf->fExcludedAttribs)
            && ((pEntry->Info.Attr.fMode >> RTFS_DOS_SHIFT) & pDataBuf->fMustHaveAttribs) == pDataBuf->fMustHaveAttribs
            && (   pDataBuf->fLongFilenames
                || pEntry->cucShortName
                || vboxSfOs2IsUtf8Name8dot3((char *)pEntry->name.String.utf8, pEntry->name.u16Length,
                                            pDataBuf->wszTmp, sizeof(pDataBuf->wszTmp))))
        {
            /*
             * We stages all but FEAs (level 3, 4, 13 and 14).
             */
            PBYTE const pbUserBufStart = pbData; /* In case we need to skip a bad name. */
            uint8_t    *pbToCopy       = pDataBuf->abStaging;
            uint8_t    *pbDst          = pbToCopy;

            /* Position (originally used for FS32_FINDFROMNAME 'position', but since reused
               for FILEFINDBUF3::oNextEntryOffset and FILEFINDBUF4::oNextEntryOffset): */
            if (fFlags & FF_GETPOS)
            {
                *(uint32_t *)pbDst = pFsFsd->offLastFile + 1;
                pbDst += sizeof(uint32_t);
            }

            /* Dates: Creation, Access, Write */
            vboxSfOs2DateTimeFromTimeSpec((FDATE *)pbDst, (FTIME *)(pbDst + 2), pEntry->Info.BirthTime, pDataBuf->cMinLocalTimeDelta);
            pbDst += sizeof(FDATE) + sizeof(FTIME);
            vboxSfOs2DateTimeFromTimeSpec((FDATE *)pbDst, (FTIME *)(pbDst + 2), pEntry->Info.AccessTime, pDataBuf->cMinLocalTimeDelta);
            pbDst += sizeof(FDATE) + sizeof(FTIME);
            vboxSfOs2DateTimeFromTimeSpec((FDATE *)pbDst, (FTIME *)(pbDst + 2), pEntry->Info.ModificationTime, pDataBuf->cMinLocalTimeDelta);
            pbDst += sizeof(FDATE) + sizeof(FTIME);

            /* File size, allocation size, attributes: */
            if (uLevel >= FI_LVL_STANDARD_64)
            {
                *(uint64_t *)pbDst = pEntry->Info.cbObject;
                pbDst += sizeof(uint64_t);
                *(uint64_t *)pbDst = pEntry->Info.cbAllocated;
                pbDst += sizeof(uint64_t);
                *(uint32_t *)pbDst = (pEntry->Info.Attr.fMode & RTFS_DOS_MASK_OS2) >> RTFS_DOS_SHIFT;
                pbDst += sizeof(uint32_t);
            }
            else
            {
                *(uint32_t *)pbDst = (uint32_t)RT_MIN(pEntry->Info.cbObject, _2G - 1);
                pbDst += sizeof(uint32_t);
                *(uint32_t *)pbDst = (uint32_t)RT_MIN(pEntry->Info.cbAllocated, _2G - 1);
                pbDst += sizeof(uint32_t);
                *(uint16_t *)pbDst = (uint16_t)((pEntry->Info.Attr.fMode & RTFS_DOS_MASK_OS2) >> RTFS_DOS_SHIFT);
                pbDst += sizeof(uint16_t); /* (Curious: Who is expanding this to 32-bits for 32-bit callers? */
            }

            /* Extra EA related fields: */
            if (   uLevel == FI_LVL_STANDARD
                || uLevel == FI_LVL_STANDARD_64)
            { /* nothing */ }
            else if (   uLevel == FI_LVL_STANDARD_EASIZE
                     || uLevel == FI_LVL_STANDARD_EASIZE_64)
            {
                /* EA size: */
                *(uint32_t *)pbDst = 0;
                pbDst += sizeof(uint32_t);
            }
            else
            {
                /* Empty FEALIST - flush pending data first: */
                uint32_t cbToCopy = pbDst - pbToCopy;
                if (cbToCopy < cbData)
                {
                    rc = KernCopyOut(pbData, pbToCopy, cbToCopy);
                    if (rc == NO_ERROR)
                    {
                        pbData += cbToCopy;
                        cbData -= cbToCopy;
                        pbDst   = pbToCopy;

                        uint32_t cbWritten = 0;
                        EaOp.fpFEAList = (PFEALIST)pbData;
                        rc = vboxSfOs2MakeEmptyEaListEx(&EaOp, uLevel, &cbWritten, &pEaOpUser->oError);
                        if (rc == NO_ERROR)
                        {
                            cbData -= cbWritten;
                            pbData += cbWritten;
                        }
                    }
                }
                else
                    rc = ERROR_BUFFER_OVERFLOW;
                if (rc != NO_ERROR)
                    break;
            }

            /* The length prefixed filename. */
            if (pDataBuf->fLongFilenames)
                pbDst = vboxSfOs2CopyUtf8Name(pbDst, pDataBuf->wszTmp, sizeof(pDataBuf->wszTmp),
                                           (char *)pEntry->name.String.utf8, pEntry->name.u16Length);
            else if (pEntry->cucShortName == 0)
                pbDst = vboxSfOs2CopyUtf8NameAndUpperCase(pbDst, pDataBuf->wszTmp, sizeof(pDataBuf->wszTmp),
                                                       (char *)pEntry->name.String.utf8, pEntry->name.u16Length);
            else
                pbDst = vboxSfOs2CopyUtf16NameAndUpperCase(pbDst, pEntry->uszShortName, pEntry->cucShortName);
            if (pbDst)
            {
                /*
                 * Copy out the staged data.
                 */
                uint32_t cbToCopy = pbDst - pbToCopy;
                if (cbToCopy <= cbData)
                {
                    rc = KernCopyOut(pbData, pbToCopy, cbToCopy);
                    if (rc == NO_ERROR)
                    {
                        Log4(("vboxSfOs2ReadDirEntries: match #%u LB %#x: '%s'\n", cMatches, cbToCopy, pEntry->name.String.utf8));
                        Log4(("%.*Rhxd\n", cbToCopy, pbToCopy));

                        pbData += cbToCopy;
                        cbData -= cbToCopy;
                        pbDst   = pbToCopy;

                        cMatches++;
                        pFsFsd->offLastFile++;
                    }
                    else
                        break;
                }
                else
                {
                    rc = ERROR_BUFFER_OVERFLOW;
                    break;
                }
            }
            else
            {
                /* Name conversion issue, just skip the entry. */
                Log3(("vboxSfOs2ReadDirEntries: Skipping '%s' due to name conversion issue.\n", pEntry->name.String.utf8));
                cbData -= pbUserBufStart - pbData;
                pbData  = pbUserBufStart;
            }
        }
        else
            Log3(("vboxSfOs2ReadDirEntries: fMode=%#x filter out by %#x/%#x; '%s'\n",
                  pEntry->Info.Attr.fMode, pDataBuf->fMustHaveAttribs, pDataBuf->fExcludedAttribs, pEntry->name.String.utf8));

        /*
         * Advance to the next directory entry from the host.
         */
        if (pDataBuf->cEntriesLeft-- > 1)
        {
            pDataBuf->pEntry = pEntry = (PSHFLDIRINFO)&pEntry->name.String.utf8[pEntry->name.u16Size];
            uintptr_t offEntry = (uintptr_t)pEntry - (uintptr_t)(pDataBuf + 1);
            AssertMsgReturn(offEntry + RT_UOFFSETOF(SHFLDIRINFO, name.String) <= pDataBuf->cbValid,
                            ("offEntry=%#x cbValid=%#x\n", offEntry, pDataBuf->cbValid), ERROR_SYS_INTERNAL);
            AssertMsgReturn(offEntry + RT_UOFFSETOF(SHFLDIRINFO, name.String) + pEntry->name.u16Size <= pDataBuf->cbValid,
                            ("offEntry=%#x cbValid=%#x\n", offEntry, pDataBuf->cbValid), ERROR_SYS_INTERNAL);
        }
        else
            pDataBuf->pEntry = NULL;
    }

    *pcMatches = cMatches;

    /* Ignore buffer overflows if we've got matches to return. */
    if (rc == ERROR_BUFFER_OVERFLOW && cMatches > 0)
        rc = NO_ERROR;
    return rc;
}


DECLASM(APIRET)
FS32_FINDFIRST(PCDFSI pCdFsi, PVBOXSFCD pCdFsd, PCSZ pszPath, LONG offCurDirEnd, ULONG fAttribs,
               PFSFSI pFsFsi, PVBOXSFFS pFsFsd, PBYTE pbData, ULONG cbData, PUSHORT pcMatches, ULONG uLevel, ULONG fFlags)
{
    LogFlow(("pCdFsi=%p pCdFsd=%p pszPath=%p:{%s} offCurDirEnd=%d fAttribs=%#x pFsFsi=%p pFsFsd=%p pbData=%p cbData=%#x pcMatches=%p:{%#x} uLevel=%#x fFlags=%#x\n",
             pCdFsi, pCdFsd, pszPath, pszPath, offCurDirEnd, fAttribs, pFsFsi, pFsFsd, pbData, cbData, pcMatches, *pcMatches, uLevel, fFlags));
    USHORT const cMaxMatches = *pcMatches;
    *pcMatches = 0;

    /*
     * Input validation.
     */
    switch (uLevel)
    {
        case FI_LVL_STANDARD:
        case FI_LVL_STANDARD_64:
        case FI_LVL_STANDARD_EASIZE:
        case FI_LVL_STANDARD_EASIZE_64:
            break;

        case FI_LVL_EAS_FROM_LIST:
        case FI_LVL_EAS_FROM_LIST_64:
            if (cbData < sizeof(EAOP))
            {
                Log(("FS32_FINDFIRST: Buffer smaller than EAOP: %#x\n", cbData));
                return ERROR_BUFFER_OVERFLOW;
            }
            break;

        default:
            LogRel(("FS32_FINDFIRST: Unsupported info level %u!\n", uLevel));
            return ERROR_INVALID_LEVEL;
    }

    /*
     * Resolve path to a folder and folder relative path.
     */
    PVBOXSFFOLDER pFolder;
    PSHFLSTRING   pStrFolderPath;
    RT_NOREF(pCdFsi);
    APIRET rc = vboxSfOs2ResolvePath(pszPath, pCdFsd, offCurDirEnd, &pFolder, &pStrFolderPath);
    LogFlow(("FS32_FINDFIRST: vboxSfOs2ResolvePath: -> %u pFolder=%p\n", rc, pFolder));
    if (rc == NO_ERROR)
    {
        /*
         * Look for a wildcard filter at the end of the path, saving it all for
         * later in NT filter speak if present.
         */
        PSHFLSTRING pFilter = NULL;
        char *pszFilter = RTPathFilename((char *)pStrFolderPath->String.utf8);
        if (   pszFilter
            && (   strchr(pszFilter, '*') != NULL
                || strchr(pszFilter, '?') != NULL))
        {
            if (strcmp(pszFilter, "*.*") == 0)
            {
                /* All files, no filtering needed. Just drop the filter expression from the directory path. */
                *pszFilter = '\0';
                pStrFolderPath->u16Length = (uint16_t)((uint8_t *)pszFilter - &pStrFolderPath->String.utf8[0]);
            }
            else
            {
                /* Duplicate the whole path. */
                pFilter = vboxSfOs2StrDup(pStrFolderPath->String.ach, pStrFolderPath->u16Length);
                if (pFilter)
                {
                    /* Drop filter from directory path. */
                    *pszFilter = '\0';
                    pStrFolderPath->u16Length = (uint16_t)((uint8_t *)pszFilter - &pStrFolderPath->String.utf8[0]);

                    /* Convert filter part of the copy to NT speak. */
                    pszFilter = (char *)&pFilter->String.utf8[(uint8_t *)pszFilter - &pStrFolderPath->String.utf8[0]];
                    for (;;)
                    {
                        char ch = *pszFilter;
                        if (ch == '?')
                            *pszFilter = '>';       /* The DOS question mark: Matches one char, but dots and end-of-name eats them. */
                        else if (ch == '.')
                        {
                            char ch2 = pszFilter[1];
                            if (ch2 == '*' || ch2 == '?')
                                *pszFilter = '"';   /* The DOS dot: Matches a dot or end-of-name. */
                        }
                        else if (ch == '*')
                        {
                            if (pszFilter[1] == '.')
                                *pszFilter = '<';   /* The DOS star: Matches zero or more chars except the DOS dot.*/
                        }
                        else if (ch == '\0')
                            break;
                        pszFilter++;
                    }
                }
                else
                    rc = ERROR_NOT_ENOUGH_MEMORY;
            }
        }
        /*
         * When no wildcard is specified, we're supposed to return a single entry
         * with the name in the final component.  Exception is the root, where we
         * always list the whole thing.
         *
         * Not sure if we'll ever see a trailing slash here (pszFilter == NULL),
         * but if we do we should accept it only for the root.
         */
        else if (pszFilter)
        {
            pFilter = pStrFolderPath;
            pStrFolderPath = vboxSfOs2StrDup(pFilter->String.ach, pszFilter - pFilter->String.ach);
            if (!pStrFolderPath)
                rc = ERROR_NOT_ENOUGH_MEMORY;
        }
        else if (!pszFilter && pStrFolderPath->u16Length > 1)
        {
            LogFlow(("FS32_FINDFIRST: Trailing slash (%s)\n", pStrFolderPath->String.utf8));
            rc = ERROR_PATH_NOT_FOUND;
        }
        else
            LogFlow(("FS32_FINDFIRST: Root dir (%s)\n", pStrFolderPath->String.utf8));

        /*
         * Make sure we've got a buffer for keeping unused search results.
         */
        PVBOXSFFSBUF pDataBuf = NULL;
        if (rc == NO_ERROR)
        {
            pDataBuf = (PVBOXSFFSBUF)RTMemAlloc(cMaxMatches == 1 ? VBOXSFFSBUF_MIN_SIZE : _16K - ALLOC_HDR_SIZE);
            if (pDataBuf)
                pDataBuf->cbBuf = cMaxMatches == 1 ? VBOXSFFSBUF_MIN_SIZE : _16K - ALLOC_HDR_SIZE;
            else
            {
                pDataBuf = (PVBOXSFFSBUF)RTMemAlloc(VBOXSFFSBUF_MIN_SIZE);
                if (pDataBuf)
                    pDataBuf->cbBuf = VBOXSFFSBUF_MIN_SIZE;
                else
                    rc = ERROR_NOT_ENOUGH_MEMORY;
            }
        }
        if (rc == NO_ERROR)
        {
            /*
             * Now, try open the directory for reading.
             * We pre-use the data buffer for parameter passin to avoid
             * wasting any stack space.
             */
            PSHFLCREATEPARMS pParams = (PSHFLCREATEPARMS)(pDataBuf + 1);
            RT_ZERO(*pParams);
            pParams->CreateFlags = SHFL_CF_DIRECTORY   | SHFL_CF_ACT_FAIL_IF_NEW  | SHFL_CF_ACT_OPEN_IF_EXISTS
                                 | SHFL_CF_ACCESS_READ | SHFL_CF_ACCESS_ATTR_READ | SHFL_CF_ACCESS_DENYNONE;
            int vrc = VbglR0SfCreate(&g_SfClient, &pFolder->hHostFolder, pStrFolderPath, pParams);
            LogFlow(("FS32_FINDFIRST: VbglR0SfCreate(%s) -> %Rrc Result=%d fMode=%#x hHandle=%#RX64\n",
                     pStrFolderPath->String.ach, vrc, pParams->Result, pParams->Info.Attr.fMode, pParams->Handle));
            if (RT_SUCCESS(vrc))
            {
                switch (pParams->Result)
                {
                    case SHFL_FILE_EXISTS:
                        if (pParams->Handle != SHFL_HANDLE_NIL)
                        {
                            /*
                             * Initialize the structures.
                             */
                            pFsFsd->hHostDir        = pParams->Handle;
                            pFsFsd->u32Magic        = VBOXSFFS_MAGIC;
                            pFsFsd->pFolder         = pFolder;
                            pFsFsd->pBuf            = pDataBuf;
                            pFsFsd->offLastFile     = 0;
                            pDataBuf->u32Magic      = VBOXSFFSBUF_MAGIC;
                            pDataBuf->cbValid       = 0;
                            pDataBuf->cEntriesLeft  = 0;
                            pDataBuf->pEntry        = NULL;
                            pDataBuf->pFilter       = pFilter;
                            pDataBuf->fMustHaveAttribs   = (uint8_t)((fAttribs >> 8) & (RTFS_DOS_MASK_OS2 >> RTFS_DOS_SHIFT));
                            pDataBuf->fExcludedAttribs   = (uint8_t)(~fAttribs
                                                                     & (  (RTFS_DOS_MASK_OS2 & ~(RTFS_DOS_ARCHIVED | RTFS_DOS_READONLY)
                                                                        >> RTFS_DOS_SHIFT)));
                            pDataBuf->fLongFilenames     = RT_BOOL(fAttribs & FF_ATTR_LONG_FILENAME);
                            pDataBuf->cMinLocalTimeDelta = vboxSfOs2GetLocalTimeDelta();

                            rc = vboxSfOs2ReadDirEntries(pFolder, pFsFsd, pDataBuf, uLevel, fFlags,
                                                         pbData, cbData, cMaxMatches ? cMaxMatches : UINT16_MAX, pcMatches);
                            if (rc == NO_ERROR)
                            {
                                uint32_t cRefs = ASMAtomicIncU32(&pFolder->cOpenSearches);
                                Assert(cRefs < _4K); RT_NOREF(cRefs);

                                /* We keep these on success: */
                                if (pFilter == pStrFolderPath)
                                    pStrFolderPath = NULL;
                                pFilter  = NULL;
                                pDataBuf = NULL;
                                pFolder  = NULL;
                            }
                            else
                            {
                                vrc = VbglR0SfClose(&g_SfClient, &pFolder->hHostFolder, pFsFsd->hHostDir);
                                AssertRC(vrc);
                                pFsFsd->u32Magic = ~VBOXSFFS_MAGIC;
                                pDataBuf->u32Magic = ~VBOXSFFSBUF_MAGIC;
                                pFsFsd->pFolder  = NULL;
                                pFsFsd->hHostDir = NULL;
                            }
                        }
                        else
                        {
                            LogFlow(("FS32_FINDFIRST: VbglR0SfCreate returns NIL handle for '%s'\n", pStrFolderPath->String.utf8));
                            rc = ERROR_PATH_NOT_FOUND;
                        }
                        break;

                    case SHFL_PATH_NOT_FOUND:
                        rc = ERROR_PATH_NOT_FOUND;
                        break;

                    default:
                    case SHFL_FILE_NOT_FOUND:
                        rc = ERROR_FILE_NOT_FOUND;
                        break;
                }
            }
            else
                rc = vboxSfOs2ConvertStatusToOs2(vrc, ERROR_GEN_FAILURE);
        }

        RTMemFree(pDataBuf);
        if (pFilter != pStrFolderPath)
            vboxSfOs2StrFree(pFilter);
        vboxSfOs2ReleasePathAndFolder(pStrFolderPath, pFolder);
    }

    RT_NOREF_PV(pFsFsi);
    LogFlow(("FS32_FINDFIRST: returns %u\n", rc));
    return rc;
}


DECLASM(APIRET)
FS32_FINDFROMNAME(PFSFSI pFsFsi, PVBOXSFFS pFsFsd, PBYTE pbData, ULONG cbData, PUSHORT pcMatches,
                  ULONG uLevel, ULONG uPosition, PCSZ pszName, ULONG fFlags)
{
    LogFlow(("FS32_FINDFROMNAME: pFsFsi=%p pFsFsd=%p pbData=%p cbData=%#x pcMatches=%p:{%#x} uLevel=%#x uPosition=%#x pszName=%p:{%s} fFlags=%#x\n",
             pFsFsi, pFsFsd, pbData, cbData, pcMatches, *pcMatches, uLevel, uPosition, pszName, pszName, fFlags));

    /*
     * Input validation.
     */
    USHORT const cMaxMatches = *pcMatches;
    *pcMatches = 0;
    AssertReturn(pFsFsd->u32Magic == VBOXSFFS_MAGIC, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pFsFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenSearches > 0);
    PVBOXSFFSBUF pDataBuf = pFsFsd->pBuf;
    AssertReturn(pDataBuf, ERROR_SYS_INTERNAL);
    Assert(pDataBuf->u32Magic == VBOXSFFSBUF_MAGIC);

    switch (uLevel)
    {
        case FI_LVL_STANDARD:
        case FI_LVL_STANDARD_64:
        case FI_LVL_STANDARD_EASIZE:
        case FI_LVL_STANDARD_EASIZE_64:
            break;

        case FI_LVL_EAS_FROM_LIST:
        case FI_LVL_EAS_FROM_LIST_64:
            Log(("FS32_FINDFIRST: FI_LVL_EAS_FROM_LIST[_64] -> ERROR_EAS_NOT_SUPPORTED\n"));
            return ERROR_EAS_NOT_SUPPORTED;

        default:
            LogRel(("FS32_FINDFIRST: Unsupported info level %u!\n", uLevel));
            return ERROR_INVALID_LEVEL;
    }

    /*
     * Check if we're just continuing.  This is usually the case.
     */
    APIRET rc;
    if (uPosition == pFsFsd->offLastFile)
        rc = vboxSfOs2ReadDirEntries(pFolder, pFsFsd, pDataBuf, uLevel, fFlags, pbData, cbData,
                                     cMaxMatches ? cMaxMatches : UINT16_MAX, pcMatches);
    else
    {
        Log(("TODO: uPosition differs: %#x, expected %#x (%s)\n", uPosition, pFsFsd->offLastFile, pszName));
        rc = vboxSfOs2ReadDirEntries(pFolder, pFsFsd, pDataBuf, uLevel, fFlags, pbData, cbData,
                                     cMaxMatches ? cMaxMatches : UINT16_MAX, pcMatches);
    }

    RT_NOREF(pFsFsi, pszName);
    LogFlow(("FS32_FINDFROMNAME: returns %u (*pcMatches=%#x)\n", rc, *pcMatches));
    return rc;
}


DECLASM(APIRET)
FS32_FINDNEXT(PFSFSI pFsFsi, PVBOXSFFS pFsFsd, PBYTE pbData, ULONG cbData, PUSHORT pcMatches, ULONG uLevel, ULONG fFlags)
{
    LogFlow(("FS32_FINDNEXT: pFsFsi=%p pFsFsd=%p pbData=%p cbData=%#x pcMatches=%p:{%#x} uLevel=%#x fFlags=%#x\n",
             pFsFsi, pFsFsd, pbData, cbData, pcMatches, *pcMatches, uLevel, fFlags));

    /*
     * Input validation.
     */
    USHORT const cMaxMatches = *pcMatches;
    *pcMatches = 0;
    AssertReturn(pFsFsd->u32Magic == VBOXSFFS_MAGIC, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pFsFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenSearches > 0);
    PVBOXSFFSBUF pDataBuf = pFsFsd->pBuf;
    AssertReturn(pDataBuf, ERROR_SYS_INTERNAL);
    Assert(pDataBuf->u32Magic == VBOXSFFSBUF_MAGIC);

    switch (uLevel)
    {
        case FI_LVL_STANDARD:
        case FI_LVL_STANDARD_64:
        case FI_LVL_STANDARD_EASIZE:
        case FI_LVL_STANDARD_EASIZE_64:
            break;

        case FI_LVL_EAS_FROM_LIST:
        case FI_LVL_EAS_FROM_LIST_64:
            Log(("FS32_FINDFIRST: FI_LVL_EAS_FROM_LIST[_64] -> ERROR_EAS_NOT_SUPPORTED\n"));
            return ERROR_EAS_NOT_SUPPORTED;

        default:
            LogRel(("FS32_FINDFIRST: Unsupported info level %u!\n", uLevel));
            return ERROR_INVALID_LEVEL;
    }

    /*
     * Read more.
     */
    APIRET rc = vboxSfOs2ReadDirEntries(pFolder, pFsFsd, pDataBuf, uLevel, fFlags, pbData, cbData,
                                        cMaxMatches ? cMaxMatches : UINT16_MAX, pcMatches);

    NOREF(pFsFsi);
    LogFlow(("FS32_FINDNEXT: returns %u (*pcMatches=%#x)\n", rc, *pcMatches));
    return rc;
}


DECLASM(APIRET)
FS32_FINDCLOSE(PFSFSI pFsFsi, PVBOXSFFS pFsFsd)
{
    /*
     * Input validation.
     */
    AssertReturn(pFsFsd->u32Magic == VBOXSFFS_MAGIC, ERROR_SYS_INTERNAL);
    PVBOXSFFOLDER pFolder = pFsFsd->pFolder;
    AssertReturn(pFolder != NULL, ERROR_SYS_INTERNAL);
    Assert(pFolder->u32Magic == VBOXSFFOLDER_MAGIC);
    Assert(pFolder->cOpenSearches > 0);
    PVBOXSFFSBUF pDataBuf = pFsFsd->pBuf;
    AssertReturn(pDataBuf, ERROR_SYS_INTERNAL);
    Assert(pDataBuf->u32Magic == VBOXSFFSBUF_MAGIC);

    /*
     * Close it.
     */
    if (pFsFsd->hHostDir != SHFL_HANDLE_NIL)
    {
        int vrc = VbglR0SfClose(&g_SfClient, &pFolder->hHostFolder, pFsFsd->hHostDir);
        AssertRC(vrc);
    }

    pFsFsd->u32Magic   = ~VBOXSFFS_MAGIC;
    pFsFsd->hHostDir   = SHFL_HANDLE_NIL;
    pFsFsd->pFolder    = NULL;
    pFsFsd->pBuf       = NULL;
    vboxSfOs2StrFree(pDataBuf->pFilter);
    pDataBuf->pFilter  = NULL;
    pDataBuf->u32Magic = ~VBOXSFFSBUF_MAGIC;
    pDataBuf->cbBuf    = 0;
    RTMemFree(pDataBuf);

    uint32_t cRefs = ASMAtomicDecU32(&pFolder->cOpenSearches);
    Assert(cRefs < _4K); RT_NOREF(cRefs);
    vboxSfOs2ReleaseFolder(pFolder);

    RT_NOREF(pFsFsi);
    LogFlow(("FS32_FINDCLOSE: returns NO_ERROR\n"));
    return NO_ERROR;
}





DECLASM(APIRET)
FS32_FINDNOTIFYFIRST(PCDFSI pCdFsi, PVBOXSFCD pCdFsd, PCSZ pszPath, LONG offCurDirEnd, ULONG fAttribs,
                     PUSHORT phHandle, PBYTE pbData, ULONG cbData, PUSHORT pcMatches,
                     ULONG uLevel, ULONG fFlags)
{
    RT_NOREF(pCdFsi, pCdFsd, pszPath, offCurDirEnd, fAttribs, phHandle, pbData, cbData, pcMatches, uLevel, fFlags);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_FINDNOTIFYNEXT(ULONG hHandle, PBYTE pbData, ULONG cbData, PUSHORT pcMatchs, ULONG uLevel, ULONG cMsTimeout)
{
    RT_NOREF(hHandle, pbData, cbData, pcMatchs, uLevel, cMsTimeout);
    return ERROR_NOT_SUPPORTED;
}


DECLASM(APIRET)
FS32_FINDNOTIFYCLOSE(ULONG hHandle)
{
    NOREF(hHandle);
    return ERROR_NOT_SUPPORTED;
}

