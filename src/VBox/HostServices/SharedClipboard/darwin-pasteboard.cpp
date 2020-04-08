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
 * @param   pPasteboard    Reference to the global pasteboard.
 * @param   pfFormats      Pointer for the bit combination of the
 *                         supported types.
 * @param   pfChanged      True if something has changed after the
 *                         last call.
 *
 * @returns IPRT status code. (Always VINF_SUCCESS atm.)
 */
int queryNewPasteboardFormats(PasteboardRef pPasteboard, uint32_t *pfFormats, bool *pfChanged)
{
    OSStatus err = noErr;
    *pfFormats = 0;
    *pfChanged = true;

    PasteboardSyncFlags syncFlags;
    /* Make sure all is in sync */
    syncFlags = PasteboardSynchronize(pPasteboard);
    /* If nothing changed return */
    if (!(syncFlags & kPasteboardModified))
    {
        *pfChanged = false;
        Log2(("queryNewPasteboardFormats: no change\n"));
        return VINF_SUCCESS;
    }

    /* Are some items in the pasteboard? */
    ItemCount itemCount;
    err = PasteboardGetItemCount(pPasteboard, &itemCount);
    if (itemCount < 1)
    {
        Log(("queryNewPasteboardFormats: changed: No items on the pasteboard\n"));
        return VINF_SUCCESS;
    }

    /* The id of the first element in the pasteboard */
    int rc = VINF_SUCCESS;
    PasteboardItemID itemID;
    if (!(err = PasteboardGetItemIdentifier(pPasteboard, 1, &itemID)))
    {
        /* Retrieve all flavors in the pasteboard, maybe there
         * is something we can use. */
        CFArrayRef flavorTypeArray;
        if (!(err = PasteboardCopyItemFlavors(pPasteboard, itemID, &flavorTypeArray)))
        {
            CFIndex flavorCount;
            flavorCount = CFArrayGetCount(flavorTypeArray);
            for (CFIndex flavorIndex = 0; flavorIndex < flavorCount; flavorIndex++)
            {
                CFStringRef flavorType;
                flavorType = static_cast <CFStringRef>(CFArrayGetValueAtIndex(flavorTypeArray, flavorIndex));
                /* Currently only unicode supported */
                if (   UTTypeConformsTo(flavorType, kUTTypeUTF8PlainText)
                    || UTTypeConformsTo(flavorType, kUTTypeUTF16PlainText))
                {
                    Log(("queryNewPasteboardFormats: Unicode flavor detected.\n"));
                    *pfFormats |= VBOX_SHCL_FMT_UNICODETEXT;
                }
                else if (UTTypeConformsTo(flavorType, kUTTypeBMP))
                {
                    Log(("queryNewPasteboardFormats: BMP flavor detected.\n"));
                    *pfFormats |= VBOX_SHCL_FMT_BITMAP;
                }
            }
            CFRelease(flavorTypeArray);
        }
    }

    Log(("queryNewPasteboardFormats: changed: *pfFormats=%#x\n", *pfFormats));
    return rc;
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
 * @param   hPasteboard         The pastboard handle (reference).
 * @param   idOwnership         The ownership ID to use now.
 * @param   pszOwnershipFlavor  The ownership indicator flavor
 * @param   pszOwnershipValue   The ownership value (stringified format mask).
 *
 * @todo    Add fFormats so we can make promises about available formats at once
 *          without needing to request any data first.  That might help on
 *          flavor priority.
 */
int takePasteboardOwnership(PasteboardRef hPasteboard, uint64_t idOwnership,
                            const char *pszOwnershipFlavor, const char *pszOwnershipValue)
{
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
                    Log(("takePasteboardOwnership: idOwnership=%RX64 flavor=%s value=%s\n",
                         idOwnership, pszOwnershipFlavor, pszOwnershipValue));
                else
                    Log(("takePasteboardOwnership: PasteboardPutItemFlavor -> %d (%#x)!\n", orc, orc));
                CFRelease(hFlavor);
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
 * @param   pPasteboard    Reference to the global pasteboard.
 * @param   idOwnership    The ownership ID.
 * @param   pv             The source buffer.
 * @param   cb             The size of the source buffer.
 * @param   fFormat        The format type which should be written.
 *
 * @returns IPRT status code.
 */
int writeToPasteboard(PasteboardRef pPasteboard, uint64_t idOwnership, void *pv, uint32_t cb, uint32_t fFormat)
{
    Log(("writeToPasteboard: fFormat = %02X\n", fFormat));

    /* Make sure all is in sync */
    PasteboardSynchronize(pPasteboard);

    int rc = VERR_NOT_SUPPORTED;
    /* Handle the unicode text */
    if (fFormat & VBOX_SHCL_FMT_UNICODETEXT)
    {
        PRTUTF16 pwszSrcText = static_cast <PRTUTF16>(pv);
        size_t cwSrc = cb / 2;
        size_t cwDest = 0;
        /* How long will the converted text be? */
        rc = ShClUtf16GetLinSize(pwszSrcText, cwSrc, &cwDest);
        if (RT_FAILURE(rc))
        {
            Log(("writeToPasteboard: clipboard conversion failed.  vboxClipboardUtf16GetLinSize returned %Rrc.  Abandoning.\n", rc));
            AssertRCReturn(rc, rc);
        }
        /* Empty clipboard? Not critical */
        if (cwDest == 0)
        {
            Log(("writeToPasteboard: received empty clipboard data from the guest, returning false.\n"));
            return VINF_SUCCESS;
        }
        /* Allocate the necessary memory */
        PRTUTF16 pwszDestText = static_cast <PRTUTF16>(RTMemAlloc(cwDest * 2));
        if (pwszDestText == NULL)
        {
            Log(("writeToPasteboard: failed to allocate %d bytes\n", cwDest * 2));
            return VERR_NO_MEMORY;
        }
        /* Convert the EOL */
        rc = ShClUtf16WinToLin(pwszSrcText, cwSrc, pwszDestText, cwDest);
        if (RT_FAILURE(rc))
        {
            Log(("writeToPasteboard: clipboard conversion failed.  vboxClipboardUtf16WinToLin() returned %Rrc.  Abandoning.\n", rc));
            RTMemFree(pwszDestText);
            AssertRCReturn(rc, rc);
        }

        CFDataRef textData = NULL;
        /* Create a CData object which we could pass to the pasteboard */
        if ((textData = CFDataCreate(kCFAllocatorDefault,
                                     reinterpret_cast<UInt8*>(pwszDestText), cwDest * 2)))
        {
            /* Put the Utf-16 version to the pasteboard */
            PasteboardPutItemFlavor(pPasteboard, (PasteboardItemID)idOwnership, kUTTypeUTF16PlainText, textData, 0);
        }
        /* Create a Utf-8 version */
        char *pszDestText;
        rc = RTUtf16ToUtf8(pwszDestText, &pszDestText);
        if (RT_SUCCESS(rc))
        {
            /* Create a CData object which we could pass to the pasteboard */
            if ((textData = CFDataCreate(kCFAllocatorDefault,
                                         reinterpret_cast<UInt8*>(pszDestText), strlen(pszDestText))))
            {
                /* Put the Utf-8 version to the pasteboard */
                PasteboardPutItemFlavor(pPasteboard, (PasteboardItemID)idOwnership, kUTTypeUTF8PlainText, textData, 0);
            }
            RTStrFree(pszDestText);
        }

        RTMemFree(pwszDestText);
        rc = VINF_SUCCESS;
    }
    /* Handle the bitmap */
    else if (fFormat & VBOX_SHCL_FMT_BITMAP)
    {
        /* Create a full BMP from it */
        void *pBmp;
        size_t cbBmpSize;
        CFDataRef bmpData = NULL;

        rc = ShClDibToBmp(pv, cb, &pBmp, &cbBmpSize);
        if (RT_SUCCESS(rc))
        {
            /* Create a CData object which we could pass to the pasteboard */
            if ((bmpData = CFDataCreate(kCFAllocatorDefault,
                                         reinterpret_cast<UInt8*>(pBmp), cbBmpSize)))
            {
                /* Put the Utf-8 version to the pasteboard */
                PasteboardPutItemFlavor(pPasteboard, (PasteboardItemID)idOwnership, kUTTypeBMP, bmpData, 0);
            }
            RTMemFree(pBmp);
        }
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_IMPLEMENTED;

    Log(("writeToPasteboard: rc = %02X\n", rc));
    return rc;
}

