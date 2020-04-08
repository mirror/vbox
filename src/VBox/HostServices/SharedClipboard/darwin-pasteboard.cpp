/* $Id$ */
/** @file
 * Shared Clipboard Service - Mac OS X host implementation.
 */

/*
 * Includes contributions from François Revol
 *
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <Carbon/Carbon.h>

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/errcore.h>
#include <iprt/utf16.h>

#include "VBox/log.h"
#include "VBox/HostServices/VBoxClipboardSvc.h"
#include "VBox/GuestHost/SharedClipboard.h"
#include "VBox/GuestHost/clipboard-helper.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/* For debugging */
//#define SHOW_CLIPBOARD_CONTENT


/**
 * Initialize the global pasteboard and return a reference to it.
 *
 * @param pPasteboardRef Reference to the global pasteboard.
 *
 * @returns IPRT status code.
 */
int initPasteboard(PasteboardRef *pPasteboardRef)
{
    int rc = VINF_SUCCESS;

    if (PasteboardCreate(kPasteboardClipboard, pPasteboardRef))
        rc = VERR_NOT_SUPPORTED;

    return rc;
}

/**
 * Release the reference to the global pasteboard.
 *
 * @param pPasteboardRef Reference to the global pasteboard.
 */
void destroyPasteboard(PasteboardRef *pPasteboardRef)
{
    CFRelease(*pPasteboardRef);
    *pPasteboardRef = NULL;
}

/**
 * Inspect the global pasteboard for new content. Check if there is some type
 * that is supported by vbox and return it.
 *
 * @param   hPasteboard         Reference to the global pasteboard.
 * @param   idOwnership         Our ownership ID.
 * @param   hStrOwnershipFlavor The ownership flavor string reference returned
 *                              by takePasteboardOwnership().
 * @param   pfFormats           Pointer for the bit combination of the
 *                              supported types.
 * @param   pfChanged           True if something has changed after the
 *                              last call.
 *
 * @returns VINF_SUCCESS.
 */
int queryNewPasteboardFormats(PasteboardRef hPasteboard, uint64_t idOwnership, void *hStrOwnershipFlavor,
                              uint32_t *pfFormats, bool *pfChanged)
{
    OSStatus orc;

    *pfFormats = 0;
    *pfChanged = true;

    PasteboardSyncFlags syncFlags;
    /* Make sure all is in sync */
    syncFlags = PasteboardSynchronize(hPasteboard);
    /* If nothing changed return */
    if (!(syncFlags & kPasteboardModified))
    {
        *pfChanged = false;
        Log2(("queryNewPasteboardFormats: no change\n"));
        return VINF_SUCCESS;
    }

    /* Are some items in the pasteboard? */
    ItemCount cItems = 0;
    orc = PasteboardGetItemCount(hPasteboard, &cItems);
    if (orc == 0)
    {
        if (cItems < 1)
            Log(("queryNewPasteboardFormats: changed: No items on the pasteboard\n"));
        else
        {
            /* The id of the first element in the pasteboard */
            PasteboardItemID idItem = 0;
            orc = PasteboardGetItemIdentifier(hPasteboard, 1, &idItem);
            if (orc == 0)
            {
                /*
                 * Retrieve all flavors on the pasteboard, maybe there
                 * is something we can use.  Or maybe we're the owner.
                 */
                CFArrayRef hFlavors = 0;
                orc = PasteboardCopyItemFlavors(hPasteboard, idItem, &hFlavors);
                if (orc == 0)
                {
                    CFIndex cFlavors = CFArrayGetCount(hFlavors);
                    for (CFIndex idxFlavor = 0; idxFlavor < cFlavors; idxFlavor++)
                    {
                        CFStringRef hStrFlavor = (CFStringRef)CFArrayGetValueAtIndex(hFlavors, idxFlavor);
                        if (   idItem == (PasteboardItemID)idOwnership
                            && hStrOwnershipFlavor
                            && CFStringCompare(hStrFlavor, (CFStringRef)hStrOwnershipFlavor, 0) == kCFCompareEqualTo)
                        {
                            /* We made the changes ourselves. */
                            Log2(("queryNewPasteboardFormats: no-changed: our clipboard!\n"));
                            *pfChanged = false;
                            *pfFormats = 0;
                            break;
                        }

                        if (UTTypeConformsTo(hStrFlavor, kUTTypeBMP))
                        {
                            Log(("queryNewPasteboardFormats: BMP flavor detected.\n"));
                            *pfFormats |= VBOX_SHCL_FMT_BITMAP;
                        }
                        else if (   UTTypeConformsTo(hStrFlavor, kUTTypeUTF8PlainText)
                                 || UTTypeConformsTo(hStrFlavor, kUTTypeUTF16PlainText))
                        {
                            Log(("queryNewPasteboardFormats: Unicode flavor detected.\n"));
                            *pfFormats |= VBOX_SHCL_FMT_UNICODETEXT;
                        }
#ifdef LOG_ENABLED
                        else if (LogIs2Enabled())
                        {
                            if (CFStringGetCharactersPtr(hStrFlavor))
                                Log2(("queryNewPasteboardFormats: Unknown flavor: %ls.\n", CFStringGetCharactersPtr(hStrFlavor)));
                            else if (CFStringGetCStringPtr(hStrFlavor, kCFStringEncodingUTF8))
                                Log2(("queryNewPasteboardFormats: Unknown flavor: %s.\n",
                                      CFStringGetCStringPtr(hStrFlavor, kCFStringEncodingUTF8)));
                            else
                                Log2(("queryNewPasteboardFormats: Unknown flavor: ???\n"));
                        }
#endif
                    }

                    CFRelease(hFlavors);
                }
                else
                    Log(("queryNewPasteboardFormats: PasteboardCopyItemFlavors failed - %d (%#x)\n", orc, orc));
            }
            else
                Log(("queryNewPasteboardFormats: PasteboardGetItemIdentifier failed - %d (%#x)\n", orc, orc));

            if (*pfChanged)
                Log(("queryNewPasteboardFormats: changed: *pfFormats=%#x\n", *pfFormats));
        }
    }
    else
        Log(("queryNewPasteboardFormats: PasteboardGetItemCount failed - %d (%#x)\n", orc, orc));
    return VINF_SUCCESS;
}

/**
 * Read content from the host clipboard and write it to the internal clipboard
 * structure for further processing.
 *
 * @param   pPasteboard    Reference to the global pasteboard.
 * @param   fFormat        The format type which should be read.
 * @param   pv             The destination buffer.
 * @param   cb             The size of the destination buffer.
 * @param   pcbActual      The size which is needed to transfer the content.
 *
 * @returns IPRT status code.
 */
int readFromPasteboard(PasteboardRef pPasteboard, uint32_t fFormat, void *pv, uint32_t cb, uint32_t *pcbActual)
{
    Log(("readFromPasteboard: fFormat = %02X\n", fFormat));

    OSStatus err = noErr;

    /* Make sure all is in sync */
    PasteboardSynchronize(pPasteboard);

    /* Are some items in the pasteboard? */
    ItemCount itemCount;
    err = PasteboardGetItemCount(pPasteboard, &itemCount);
    if (itemCount < 1)
        return VINF_SUCCESS;

    /* The id of the first element in the pasteboard */
    int rc = VERR_NOT_SUPPORTED;
    PasteboardItemID itemID;
    if (!(err = PasteboardGetItemIdentifier(pPasteboard, 1, &itemID)))
    {
        /* The guest request unicode */
        if (fFormat & VBOX_SHCL_FMT_UNICODETEXT)
        {
            CFDataRef outData;
            PRTUTF16 pwszTmp = NULL;
            /* Try utf-16 first */
            if (!(err = PasteboardCopyItemFlavorData(pPasteboard, itemID, kUTTypeUTF16PlainText, &outData)))
            {
                Log(("Clipboard content is utf-16\n"));

                PRTUTF16 pwszString = (PRTUTF16)CFDataGetBytePtr(outData);
                if (pwszString)
                    rc = RTUtf16DupEx(&pwszTmp, pwszString, 0);
                else
                    rc = VERR_INVALID_PARAMETER;
            }
            /* Second try is utf-8 */
            else
                if (!(err = PasteboardCopyItemFlavorData(pPasteboard, itemID, kUTTypeUTF8PlainText, &outData)))
                {
                    Log(("readFromPasteboard: clipboard content is utf-8\n"));
                    const char *pszString = (const char *)CFDataGetBytePtr(outData);
                    if (pszString)
                        rc = RTStrToUtf16(pszString, &pwszTmp);
                    else
                        rc = VERR_INVALID_PARAMETER;
                }
            if (pwszTmp)
            {
                /* Check how much longer will the converted text will be. */
                size_t cwSrc = RTUtf16Len(pwszTmp);
                size_t cwDest;
                rc = ShClUtf16GetWinSize(pwszTmp, cwSrc, &cwDest);
                if (RT_FAILURE(rc))
                {
                    RTUtf16Free(pwszTmp);
                    Log(("readFromPasteboard: clipboard conversion failed.  vboxClipboardUtf16GetWinSize returned %Rrc.  Abandoning.\n", rc));
                    AssertRCReturn(rc, rc);
                }
                /* Set the actually needed data size */
                *pcbActual = cwDest * 2;
                /* Return success state */
                rc = VINF_SUCCESS;
                /* Do not copy data if the dst buffer is not big enough. */
                if (*pcbActual <= cb)
                {
                    rc = ShClUtf16LinToWin(pwszTmp, RTUtf16Len(pwszTmp), static_cast <PRTUTF16>(pv), cb / 2);
                    if (RT_FAILURE(rc))
                    {
                        RTUtf16Free(pwszTmp);
                        Log(("readFromPasteboard: clipboard conversion failed.  vboxClipboardUtf16LinToWin() returned %Rrc.  Abandoning.\n", rc));
                        AssertRCReturn(rc, rc);
                    }
#ifdef SHOW_CLIPBOARD_CONTENT
                    Log(("readFromPasteboard: clipboard content: %ls\n", static_cast <PRTUTF16>(pv)));
#endif
                }
                /* Free the temp string */
                RTUtf16Free(pwszTmp);
            }
        }
        /* The guest request BITMAP */
        else if (fFormat & VBOX_SHCL_FMT_BITMAP)
        {
            CFDataRef outData;
            const void *pTmp = NULL;
            size_t cbTmpSize;
            /* Get the data from the pasteboard */
            if (!(err = PasteboardCopyItemFlavorData(pPasteboard, itemID, kUTTypeBMP, &outData)))
            {
                Log(("Clipboard content is BMP\n"));
                pTmp = CFDataGetBytePtr(outData);
                cbTmpSize = CFDataGetLength(outData);
            }
            if (pTmp)
            {
                const void *pDib;
                size_t cbDibSize;
                rc = ShClBmpGetDib(pTmp, cbTmpSize, &pDib, &cbDibSize);
                if (RT_FAILURE(rc))
                {
                    rc = VERR_NOT_SUPPORTED;
                    Log(("readFromPasteboard: unknown bitmap format. vboxClipboardBmpGetDib returned %Rrc.  Abandoning.\n", rc));
                    AssertRCReturn(rc, rc);
                }

                *pcbActual = cbDibSize;
                /* Return success state */
                rc = VINF_SUCCESS;
                /* Do not copy data if the dst buffer is not big enough. */
                if (*pcbActual <= cb)
                {
                    memcpy(pv, pDib, cbDibSize);
#ifdef SHOW_CLIPBOARD_CONTENT
                    Log(("readFromPasteboard: clipboard content bitmap %d bytes\n", cbDibSize));
#endif
                }
            }
        }
    }

    Log(("readFromPasteboard: rc = %02X\n", rc));
    return rc;
}

/**
 * Takes the ownership of the pasteboard.
 *
 * This is called when the other end reports available formats.
 *
 * @returns VBox status code.
 * @param   hPasteboard             The pastboard handle (reference).
 * @param   idOwnership             The ownership ID to use now.
 * @param   pszOwnershipFlavor      The ownership indicator flavor
 * @param   pszOwnershipValue       The ownership value (stringified format mask).
 * @param   phStrOwnershipFlavor    Pointer to a CFStringRef variable holding
 *                                  the current ownership flavor string.  This
 *                                  will always be released, and set again on
 *                                  success.
 *
 * @todo    Add fFormats so we can make promises about available formats at once
 *          without needing to request any data first.  That might help on
 *          flavor priority.
 */
int takePasteboardOwnership(PasteboardRef hPasteboard, uint64_t idOwnership, const char *pszOwnershipFlavor,
                            const char *pszOwnershipValue, void **phStrOwnershipFlavor)
{
    /*
     * Release the old string.
     */
    if (*phStrOwnershipFlavor)
    {
        CFStringRef hOldFlavor = (CFStringRef)*phStrOwnershipFlavor;
        CFRelease(hOldFlavor);
        *phStrOwnershipFlavor = NULL;
    }

    /*
     * Clear the pasteboard and take ownership over it.
     */
    OSStatus orc = PasteboardClear(hPasteboard);
    if (orc == 0)
    {
        /* For good measure. */
        PasteboardSynchronize(hPasteboard);

        /*
         * Put the ownership flavor and value onto the clipboard.
         */
        CFDataRef hData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)pszOwnershipValue, strlen(pszOwnershipValue));
        if (hData)
        {
            CFStringRef hFlavor = CFStringCreateWithCString(kCFAllocatorDefault, pszOwnershipFlavor, kCFStringEncodingUTF8);
            if (hFlavor)
            {
                orc = PasteboardPutItemFlavor(hPasteboard, (PasteboardItemID)idOwnership,
                                              hFlavor, hData, kPasteboardFlavorNoFlags);
                if (orc == 0)
                {
                    *phStrOwnershipFlavor = (void *)hFlavor;
                    Log(("takePasteboardOwnership: idOwnership=%RX64 flavor=%s value=%s\n",
                         idOwnership, pszOwnershipFlavor, pszOwnershipValue));
                }
                else
                {
                    Log(("takePasteboardOwnership: PasteboardPutItemFlavor -> %d (%#x)!\n", orc, orc));
                    CFRelease(hFlavor);
                }
            }
            else
                Log(("takePasteboardOwnership: CFStringCreateWithCString failed!\n"));
            CFRelease(hData);
        }
        else
            Log(("takePasteboardOwnership: CFDataCreate failed!\n"));
    }
    else
        Log(("takePasteboardOwnership: PasteboardClear failed -> %d (%#x)\n", orc, orc));
    return orc == 0 ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

/**
 * Write clipboard content to the host clipboard from the internal clipboard
 * structure.
 *
 * @param   hPasteboard    Reference to the global pasteboard.
 * @param   idOwnership    The ownership ID.
 * @param   pv             The source buffer.
 * @param   cb             The size of the source buffer.
 * @param   fFormat        The format type which should be written.
 *
 * @returns IPRT status code.
 */
int writeToPasteboard(PasteboardRef hPasteboard, uint64_t idOwnership, const void *pv, uint32_t cb, uint32_t fFormat)
{
    int       rc;
    OSStatus  orc;
    CFDataRef hData;
    Log(("writeToPasteboard: fFormat = %02X\n", fFormat));

    /* Make sure all is in sync */
    PasteboardSynchronize(hPasteboard);

    /*
     * Handle the unicode text
     */
    /** @todo Figure out the format of kUTTypeHTML.  Seems it is neiter UTF-8 or
     *        UTF-16. */
    if (fFormat & (VBOX_SHCL_FMT_UNICODETEXT /*| VBOX_SHCL_FMT_HTML*/))
    {
        PCRTUTF16 const pwszSrc = (PCRTUTF16)pv;
        size_t const    cwcSrc  = cb / sizeof(RTUTF16);

        /*
         * If the other side is windows or OS/2, we may have to convert
         * '\r\n' -> '\n' and the drop ending marker.
         */

        /* How long will the converted text be? */
        size_t cwcDst = 0;
        rc = ShClUtf16GetLinSize(pwszSrc, cwcSrc, &cwcDst);
        AssertMsgRCReturn(rc, ("ShClUtf16GetLinSize failed: %Rrc\n", rc), rc);

        /* Ignore empty strings? */
        if (cwcDst == 0)
        {
            Log(("writeToPasteboard: received empty string from the guest; ignoreing it.\n"));
            return VINF_SUCCESS;
        }

        /* Allocate the necessary memory and do the conversion. */
        PRTUTF16 pwszDst = (PRTUTF16)RTMemAlloc(cwcDst * sizeof(RTUTF16));
        AssertMsgReturn(pwszDst, ("cwcDst=%#zx\n", cwcDst), VERR_NO_UTF16_MEMORY);

        rc = ShClUtf16WinToLin(pwszSrc, cwcSrc, pwszDst, cwcDst);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create an immutable CFData object that we can place on the clipboard.
             */
            rc = VINF_SUCCESS;
            //if (fFormat & (VBOX_SHCL_FMT_UNICODETEXT | VBOX_SHCL_FMT_HTML))
            {
                hData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)pwszDst, cwcDst * sizeof(RTUTF16));
                if (hData)
                {
                    orc = PasteboardPutItemFlavor(hPasteboard, (PasteboardItemID)idOwnership,
                                                  fFormat & VBOX_SHCL_FMT_UNICODETEXT ? kUTTypeUTF16PlainText : kUTTypeHTML,
                                                  hData, kPasteboardFlavorNoFlags);
                    if (orc != 0)
                    {
                        Log(("writeToPasteboard: PasteboardPutItemFlavor/%s failed: %d (%#x)\n",
                             fFormat & VBOX_SHCL_FMT_UNICODETEXT ? "kUTTypeUTF16PlainText" : "kUTTypeHTML", orc, orc));
                        rc = VERR_GENERAL_FAILURE;
                    }
                    CFRelease(hData);
                }
                else
                {
                    Log(("writeToPasteboard: CFDataCreate/UTF16 failed!\n"));
                    rc = VERR_NO_MEMORY;
                }
            }

            /*
             * Now for the UTF-8 version.
             */
            //if (fFormat & VBOX_SHCL_FMT_UNICODETEXT)
            {
                char *pszDst;
                int rc2 = RTUtf16ToUtf8(pwszDst, &pszDst);
                if (RT_SUCCESS(rc2))
                {
                    hData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)pszDst, strlen(pszDst));
                    if (hData)
                    {
                        orc = PasteboardPutItemFlavor(hPasteboard, (PasteboardItemID)idOwnership,
                                                      fFormat & VBOX_SHCL_FMT_UNICODETEXT ? kUTTypeUTF8PlainText : kUTTypeHTML,
                                                      hData, kPasteboardFlavorNoFlags);
                        if (orc != 0)
                        {
                            Log(("writeToPasteboard: PasteboardPutItemFlavor/%s failed: %d (%#x)\n",
                                 fFormat & VBOX_SHCL_FMT_UNICODETEXT ? "kUTTypeUTF8PlainText" : "kUTTypeHTML", orc, orc));
                            rc = VERR_GENERAL_FAILURE;
                        }
                        CFRelease(hData);
                    }
                    else
                    {
                        Log(("writeToPasteboard: CFDataCreate/UTF8 failed!\n"));
                        rc = VERR_NO_MEMORY;
                    }
                    RTStrFree(pszDst);
                }
                else
                    rc = rc2;
            }
        }
        else
            Log(("writeToPasteboard: clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));

        RTMemFree(pwszDst);
    }
    /*
     * Handle the bitmap.  We convert the DIB to a bitmap and put it on
     * the pasteboard using the BMP flavor.
     */
    else if (fFormat & VBOX_SHCL_FMT_BITMAP)
    {
        /* Create a full BMP from it */
        void  *pvBmp;
        size_t cbBmp;
        rc = ShClDibToBmp(pv, cb, &pvBmp, &cbBmp);
        if (RT_SUCCESS(rc))
        {
            hData = CFDataCreate(kCFAllocatorDefault, (UInt8 const *)pvBmp, cbBmp);
            if (hData)
            {
                orc = PasteboardPutItemFlavor(hPasteboard, (PasteboardItemID)idOwnership,
                                              kUTTypeBMP, hData, kPasteboardFlavorNoFlags);
                if (orc != 0)
                {
                    Log(("writeToPasteboard: PasteboardPutItemFlavor/kUTTypeBMP failed: %d (%#x)\n", orc, orc));
                    rc = VERR_GENERAL_FAILURE;
                }
                CFRelease(hData);
            }
            else
            {
                Log(("writeToPasteboard: CFDataCreate/UTF8 failed!\n"));
                rc = VERR_NO_MEMORY;
            }
            RTMemFree(pvBmp);
        }
    }
    else
        rc = VERR_NOT_IMPLEMENTED;

    Log(("writeToPasteboard: rc=%Rrc\n", rc));
    return rc;
}

