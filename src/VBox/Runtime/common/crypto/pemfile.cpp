/* $Id$ */
/** @file
 * IPRT - Crypto - PEM file reader / writer.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/pem.h>

#include <iprt/base64.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/file.h>
#include <iprt/string.h>



/**
 * Looks for a PEM-like marker.
 *
 * @returns true if found, fasle if not.
 * @param   pbContent           Start of the content to search thru.
 * @param   cbContent           The size of the content to search.
 * @param   offStart            The offset into pbContent to start searching.
 * @param   pszLeadWord         The lead word (BEGIN/END).
 * @param   cchLeadWord         The length of the lead word.
 * @param   paMarkers           Pointer to an array of markers.
 * @param   cMarkers            Number of markers in the array.
 * @param   ppMatch             Where to return the pointer to the matching
 *                              marker. Optional.
 * @param   poffBegin           Where to return the start offset of the marker.
 *                              Optional.
 * @param   poffEnd             Where to return the end offset of the marker
 *                              (trailing whitespace and newlines will be
 *                              skipped).  Optional.
 */
static bool rtCrPemFindMarker(uint8_t const *pbContent, size_t cbContent, size_t offStart,
                              const char *pszLeadWord, size_t cchLeadWord, PCRTCRPEMMARKER paMarkers, size_t cMarkers,
                              PCRTCRPEMMARKER *ppMatch, size_t *poffBegin, size_t *poffEnd)
{
    /* Remember the start of the content for the purpose of calculating offsets. */
    uint8_t const * const pbStart = pbContent;

    /* Skip adhead by offStart */
    if (offStart >= cbContent)
        return false;
    pbContent += offStart;
    cbContent -= offStart;

    /*
     * Search the content.
     */
    while (cbContent > 6)
    {
        /*
         * Look for dashes.
         */
        uint8_t const *pbStartSearch = pbContent;
        pbContent = (uint8_t const *)memchr(pbContent, '-', cbContent);
        if (!pbContent)
            break;

        cbContent -= pbContent - pbStartSearch;
        if (cbContent < 6)
            break;

        /*
         * There must be at least three to interest us.
         */
        if (   pbContent[1] == '-'
            && pbContent[2] == '-')
        {
            unsigned cDashes = 3;
            while (cDashes < cbContent && pbContent[cDashes] == '-')
                cDashes++;

            if (poffBegin)
                *poffBegin = pbContent - pbStart;
            cbContent -= cDashes;
            pbContent += cDashes;

            /*
             * Match lead word.
             */
            if (   cbContent > cchLeadWord
                && memcmp(pbContent, pszLeadWord, cchLeadWord) == 0
                && RT_C_IS_BLANK(pbContent[cchLeadWord]) )
            {
                pbContent += cchLeadWord;
                cbContent -= cchLeadWord;
                while (cbContent > 0 && RT_C_IS_BLANK(*pbContent))
                {
                    pbContent++;
                    cbContent--;
                }

                /*
                 * Match one of the specified markers.
                 */
                uint8_t const  *pbSavedContent = pbContent;
                size_t  const   cbSavedContent = cbContent;
                uint32_t        iMarker = 0;
                while (iMarker < cMarkers)
                {
                    pbContent = pbSavedContent;
                    cbContent = cbSavedContent;

                    uint32_t            cWords = paMarkers[iMarker].cWords;
                    PCRTCRPEMMARKERWORD pWord  = paMarkers[iMarker].paWords;
                    while (cWords > 0)
                    {
                        uint32_t const cchWord = pWord->cchWord;
                        if (cbContent <= cchWord)
                            break;
                        if (memcmp(pbContent, pWord->pszWord, cchWord))
                            break;
                        pbContent += cchWord;
                        cbContent -= cchWord;

                        if (!cbContent || !RT_C_IS_BLANK(*pbContent))
                            break;
                        do
                        {
                            pbContent++;
                            cbContent--;
                        } while (cbContent > 0 && RT_C_IS_BLANK(*pbContent));

                        cWords--;
                        if (cWords == 0)
                        {
                            /*
                             * If there are three or more dashes following now, we've got a hit.
                             */
                            if (   cbContent > 3
                                && pbContent[0] == '-'
                                && pbContent[1] == '-'
                                && pbContent[2] == '-')
                            {
                                cDashes = 3;
                                while (cDashes < cbContent && pbContent[cDashes] == '-')
                                    cDashes++;
                                cbContent -= cDashes;
                                pbContent += cDashes;

                                /*
                                 * Skip spaces and newline.
                                 */
                                while (cbContent > 0 && RT_C_IS_SPACE(*pbContent))
                                    pbContent++, cbContent--;
                                if (poffEnd)
                                    *poffEnd = pbContent - pbStart;
                                if (*ppMatch)
                                    *ppMatch = &paMarkers[iMarker];
                                return true;
                            }
                            break;
                        }
                    } /* for each word in marker. */
                } /* for each marker. */
            }
        }
        else
        {
            pbContent++;
            cbContent--;
        }
    }

    return false;
}


static bool rtCrPemFindMarkerSection(uint8_t const *pbContent, size_t cbContent, size_t offStart,
                                     PCRTCRPEMMARKER paMarkers, size_t cMarkers,
                                     PCRTCRPEMMARKER *ppMatch, size_t *poffBegin, size_t *poffEnd, size_t *poffResume)
{
    /** @todo Detect BEGIN / END mismatch. */
    PCRTCRPEMMARKER pMatch;
    if (rtCrPemFindMarker(pbContent, cbContent, offStart, "BEGIN", 5, paMarkers, cMarkers,
                          &pMatch, NULL /*poffStart*/, poffBegin))
    {
        if (rtCrPemFindMarker(pbContent, cbContent, *poffBegin, "END", 3, pMatch, 1,
                              NULL /*ppMatch*/, poffEnd, poffResume))
        {
            *ppMatch = pMatch;
            return true;
        }
    }
    *ppMatch = NULL;
    return false;
}



/**
 * Does the decoding of a PEM-like data blob after it has been located.
 *
 * @returns IPRT status ocde
 * @param   pbContent           The start of the PEM-like content (text).
 * @param   cbContent           The max size of the PEM-like content.
 * @param   ppvDecoded          Where to return a heap block containing the
 *                              decoded content.
 * @param   pcbDecoded          Where to return the size of the decoded content.
 */
static int rtCrPemDecodeBase64(uint8_t const *pbContent, size_t cbContent, void **ppvDecoded, size_t *pcbDecoded)
{
    ssize_t cbDecoded = RTBase64DecodedSizeEx((const char *)pbContent, cbContent, NULL);
    if (cbDecoded < 0)
        return VERR_INVALID_BASE64_ENCODING;

    *pcbDecoded = cbDecoded;
    void *pvDecoded = RTMemAlloc(cbDecoded);
    if (!pvDecoded)
        return VERR_NO_MEMORY;

    size_t cbActual;
    int rc = RTBase64DecodeEx((const char *)pbContent, cbContent, pvDecoded, cbDecoded, &cbActual, NULL);
    if (RT_SUCCESS(rc))
    {
        if (cbActual == (size_t)cbDecoded)
        {
            *ppvDecoded = pvDecoded;
            return VINF_SUCCESS;
        }
        rc = VERR_INTERNAL_ERROR_3;
    }
    RTMemFree(pvDecoded);
    return rc;
}


/**
 * Checks if the content of a file looks to be binary or not.
 *
 * @returns true if likely to be binary, false if not binary.
 * @param   pbFile              The file bytes to scan.
 * @param   cbFile              The number of bytes.
 */
static bool rtCrPemIsBinaryFile(uint8_t *pbFile, size_t cbFile)
{
    /*
     * Assume a well formed PEM file contains only 7-bit ASCII and restricts
     * itself to the following control characters:
     *      tab, newline, return, form feed
     */
    while (cbFile-- > 0)
    {
        uint8_t const b = *pbFile++;
        if (   b >= 0x7f
            || (b < 32 && b != '\t' && b != '\n' && b != '\r' && b != '\f') )
        {
            /* Ignore EOT (4), SUB (26) and NUL (0) at the end of the file. */
            if (   (b == 4 || b == 26)
                && (   cbFile == 0
                    || (   cbFile == 1
                        && *pbFile == '\0')))
                return true;
            if (b == 0 && cbFile == 0)
                return true;
            return false;
        }
    }
    return true;
}


RTDECL(int) RTCrPemFreeSections(PCRTCRPEMSECTION pSectionHead)
{
    while (pSectionHead != NULL)
    {
        PRTCRPEMSECTION pFree = (PRTCRPEMSECTION)pSectionHead;
        pSectionHead = pSectionHead->pNext;

        if (pFree->pMarker)
        {
            if (pFree->pbData)
            {
                RTMemFree(pFree->pbData);
                pFree->pbData = NULL;
                pFree->cbData = 0;
            }

            if (pFree->pszPreamble)
            {
                RTMemFree(pFree->pszPreamble);
                pFree->pszPreamble = NULL;
                pFree->cchPreamble = 0;
            }
        }
        else
        {
            RTFileReadAllFree(pFree->pbData, pFree->cbData);
            Assert(!pFree->pszPreamble);
        }
        pFree->pbData = NULL;
        pFree->cbData = 0;
    }
    return VINF_SUCCESS;
}


RTDECL(int) RTCrPemReadFile(const char *pszFilename, uint32_t fFlags, PCRTCRPEMMARKER paMarkers, size_t cMarkers,
                            PCRTCRPEMSECTION *ppSectionHead, PRTERRINFO pErrInfo)
{
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    size_t      cbContent;
    uint8_t    *pbContent;
    int rc = RTFileReadAllEx(pszFilename, 0, 64U*_1M, RTFILE_RDALL_O_DENY_WRITE, (void **)&pbContent, &cbContent);
    if (RT_SUCCESS(rc))
    {
        PRTCRPEMSECTION pSection = (PRTCRPEMSECTION)RTMemAllocZ(sizeof(*pSection));
        if (pSection)
        {
            /*
             * Try locate the first section.
             */
            size_t          offBegin, offEnd, offResume;
            PCRTCRPEMMARKER pMatch;
            if (   !rtCrPemIsBinaryFile(pbContent, cbContent)
                && rtCrPemFindMarkerSection(pbContent, cbContent, 0 /*offStart*/, paMarkers, cMarkers,
                                            &pMatch, &offBegin, &offEnd, &offResume) )
            {
                PCRTCRPEMSECTION *ppNext = ppSectionHead;
                for (;;)
                {
                    //pSection->pNext         = NULL;
                    pSection->pMarker           = pMatch;
                    //pSection->pbData        = NULL;
                    //pSection->cbData        = 0;
                    //pSection->pszPreamble   = NULL;
                    //pSection->cchPreamble   = 0;

                    *ppNext = pSection;
                    ppNext = &pSection->pNext;

                    /* Decode the section. */
                    /** @todo copy the preamble as well. */
                    rc = rtCrPemDecodeBase64(pbContent + offBegin, offEnd - offBegin,
                                             (void **)&pSection->pbData, &pSection->cbData);
                    if (RT_FAILURE(rc))
                    {
                        pSection->pbData = NULL;
                        pSection->cbData = 0;
                        break;
                    }

                    /* More sections? */
                    if (   offResume + 12 >= cbContent
                        || offResume      >= cbContent
                        || !rtCrPemFindMarkerSection(pbContent, cbContent, offResume, paMarkers, cMarkers,
                                                     &pMatch, &offBegin, &offEnd, &offResume) )
                        break; /* No. */

                    /* Ok, allocate a new record for it. */
                    pSection = (PRTCRPEMSECTION)RTMemAllocZ(sizeof(*pSection));
                    if (RT_UNLIKELY(!pSection))
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                }
                if (RT_SUCCESS(rc))
                {
                    RTFileReadAllFree(pbContent, cbContent);
                    return rc;
                }

                RTCrPemFreeSections(*ppSectionHead);
            }
            else
            {
                /*
                 * No PEM section found.  Return the whole file as one binary section.
                 */
                //pSection->pNext         = NULL;
                //pSection->pMarker       = NULL;
                pSection->pbData        = pbContent;
                pSection->cbData        = cbContent;
                //pSection->pszPreamble   = NULL;
                //pSection->cchPreamble   = 0;
                *ppSectionHead = pSection;
                return VINF_SUCCESS;
            }
        }
        else
            rc = VERR_NO_MEMORY;
        RTFileReadAllFree(pbContent, cbContent);
    }
    *ppSectionHead = NULL;
    return rc;
}

