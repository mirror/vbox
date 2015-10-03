/* $Id$ */
/** @file
 * USB device vendor and product ID database.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___USBIdDatabase_h
#define ___USBIdDatabase_h

#include <iprt/assert.h>
#include <iprt/stdint.h>
#include <iprt/cpp/ministring.h>


/** Saves a few bytes (~25%) on strings.  */
#define USB_ID_DATABASE_WITH_COMPRESSION

/** Max string length. */
#define USB_ID_DATABASE_MAX_STRING      _1K


/**
 * USB ID database string table reference.
 */
typedef struct USBIDDBSTR
{
    /** Offset of the string in the string table. */
    uint32_t off : 22;
    /** The length of the string. */
    uint32_t cch : 10;
} USBIDDBSTR;
AssertCompileSize(USBIDDBSTR, sizeof(uint32_t));


/**
 * Elements of product table.
 */
typedef struct USBIDDBPROD
{
    /** Product ID. */
    uint16_t idProduct;
} USBIDDBPROD;
AssertCompileSize(USBIDDBPROD, sizeof(uint16_t));


/**
 * Element of vendor table.
 */
typedef struct USBIDDBVENDOR
{
    /** Vendor ID. */
    uint16_t idVendor;
    /** Index of the first product. */
    uint16_t iProduct;
    /** Number of products. */
    uint16_t cProducts;
} USBIDDBVENDOR;
AssertCompileSize(USBIDDBVENDOR, sizeof(uint16_t) * 3);


/**
 * Wrapper for static array of Aliases.
 */
class USBIdDatabase
{
public: // For assertions and statis in the generator.
    /** String table. */
    static const char           s_achStrTab[];
    /** The size of the string table (for bounds checking). */
    static const size_t         s_cchStrTab;
#ifdef USB_ID_DATABASE_WITH_COMPRESSION
    /** Dictionary containing the 127 most used substrings (that we managed
     * to detect without lousy word based searching). */
    static const USBIDDBSTR     s_aCompDict[127];
#endif

    /** Number of vendors in the two parallel arrays.   */
    static const size_t         s_cVendors;
    /** Vendor IDs lookup table. */
    static const USBIDDBVENDOR  s_aVendors[];
    /** Vendor names table running parallel to s_aVendors. */
    static const USBIDDBSTR     s_aVendorNames[];

    /** Number of products in the two parallel arrays. */
    static const size_t         s_cProducts;
    /** Vendor+Product keys for lookup purposes. */
    static const USBIDDBPROD    s_aProducts[];
    /** Product names table running parallel to s_aProducts. */
    static const USBIDDBSTR     s_aProductNames[];

public:
    static RTCString returnString(USBIDDBSTR const *pStr)
    {
        Assert(pStr->cch < s_cchStrTab);
        Assert(pStr->off < s_cchStrTab);
        Assert(pStr->off + (size_t)pStr->cch < s_cchStrTab);

#ifdef USB_ID_DATABASE_WITH_COMPRESSION
        char        szTmp[USB_ID_DATABASE_MAX_STRING * 2];
        char       *pchDst = &szTmp[0];
        size_t      cchSrc = pStr->cch;
        const char *pchSrc = &s_achStrTab[pStr->off];
        Assert(cchSrc <= USB_ID_DATABASE_MAX_STRING);
        while (cchSrc-- > 0)
        {
            unsigned char uch = *pchSrc++;
            if (!(uch & 0x80))
            {
                *pchDst++ = (char)uch;
                Assert(uch != 0);
                Assert((uintptr_t)(pchDst - &szTmp[0]) < USB_ID_DATABASE_MAX_STRING);
            }
            else if (uch == 0xff)
            {
                RTUNICP uc = ' ';
                int rc = RTStrGetCpNEx(&pchSrc, &cchSrc, &uc);
                AssertStmt(RT_SUCCESS(rc), (uc = '?', pchSrc++, cchSrc--));
                pchDst = RTStrPutCp(pchDst, uc);
                Assert((uintptr_t)(pchDst - &szTmp[0]) < USB_ID_DATABASE_MAX_STRING);
            }
            else
            {
                /* Dictionary reference. No unescaping necessary here. */
                const USBIDDBSTR *pStr2 = &s_aCompDict[uch & 0x7f];
                Assert(pStr2->cch < s_cchStrTab);
                Assert(pStr2->off < s_cchStrTab);
                Assert(pStr2->off + (size_t)pStr->cch < s_cchStrTab);
                Assert((uintptr_t)(&pchDst[pStr2->cch] - &szTmp[0]) < USB_ID_DATABASE_MAX_STRING);
                memcpy(pchDst, &s_achStrTab[pStr2->off], pStr2->cch);
                pchDst += pStr2->cch;
            }
        }
        *pchDst = '\0';
        return RTCString(szTmp, pchDst - &szTmp[0]);
#else  /* !USB_ID_DATABASE_WITH_COMPRESSION */
        return RTCString(&s_achStrTab[pStr->off], pStr->cch);
#endif /* !USB_ID_DATABASE_WITH_COMPRESSION */
    }

private:
    /**
     * Performs a binary lookup of @a idVendor.
     *
     * @returns The index in the vendor tables, UINT32_MAX if not found.
     * @param   idVendor        The vendor ID.
     */
    static uint32_t lookupVendor(uint16_t idVendor)
    {
        size_t iEnd   = s_cVendors;
        if (iEnd)
        {
            size_t iStart = 0;
            for (;;)
            {
                size_t idx = iStart + (iEnd - iStart) / 2;
                if (s_aVendors[idx].idVendor < idVendor)
                {
                    idx++;
                    if (idx < iEnd)
                        iStart = idx;
                    else
                        break;
                }
                else if (s_aVendors[idx].idVendor > idVendor)
                {
                    if (idx != iStart)
                        iEnd = idx;
                    else
                        break;
                }
                else
                    return (uint32_t)idx;
            }
        }
        return UINT32_MAX;
    }

    /**
     * Performs a binary lookup of @a idProduct.
     *
     * @returns The index in the product tables, UINT32_MAX if not found.
     * @param   idProduct       The product ID.
     * @param   iStart          The index of the first entry for the vendor.
     * @param   iEnd            The index of after the last entry.
     */
    static uint32_t lookupProduct(uint16_t idProduct, size_t iStart, size_t iEnd)
    {
        if (iStart < iEnd)
        {
            for (;;)
            {
                size_t idx = iStart + (iEnd - iStart) / 2;
                if (s_aProducts[idx].idProduct < idProduct)
                {
                    idx++;
                    if (idx < iEnd)
                        iStart = idx;
                    else
                        break;
                }
                else if (s_aProducts[idx].idProduct > idProduct)
                {
                    if (idx != iStart)
                        iEnd = idx;
                    else
                        break;
                }
                else
                    return (uint32_t)idx;
            }
        }
        return UINT32_MAX;
    }


public:
    static RTCString findProduct(uint16_t idVendor, uint16_t idProduct)
    {
        uint32_t idxVendor = lookupVendor(idVendor);
        if (idxVendor != UINT32_MAX)
        {
            uint32_t idxProduct = lookupProduct(idProduct, s_aVendors[idxVendor].iProduct,
                                                s_aVendors[idxVendor].iProduct + s_aVendors[idxVendor].cProducts);
            if (idxProduct != UINT32_MAX)
                return returnString(&s_aProductNames[idxProduct]);
        }
        return RTCString();
    }

    static RTCString findVendor(uint16_t idVendor)
    {
        uint32_t idxVendor = lookupVendor(idVendor);
        if (idxVendor != UINT32_MAX)
            return returnString(&s_aVendorNames[idxVendor]);
        return RTCString();
    }

    static RTCString findVendorAndProduct(uint16_t idVendor, uint16_t idProduct, RTCString *pstrProduct)
    {
        uint32_t idxVendor = lookupVendor(idVendor);
        if (idxVendor != UINT32_MAX)
        {
            uint32_t idxProduct = lookupProduct(idProduct, s_aVendors[idxVendor].iProduct,
                                                s_aVendors[idxVendor].iProduct + s_aVendors[idxVendor].cProducts);
            if (idxProduct != UINT32_MAX)
                *pstrProduct = returnString(&s_aProductNames[idxProduct]);
            else
                pstrProduct->setNull();
            return returnString(&s_aVendorNames[idxVendor]);
        }
        pstrProduct->setNull();
        return RTCString();
    }

};


#endif

