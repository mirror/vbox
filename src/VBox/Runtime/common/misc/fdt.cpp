/* $Id$ */
/** @file
 * IPRT - Flattened Devicetree parser and generator API.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FDT
#include <iprt/fdt.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/strcache.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/formats/dtb.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Special error token to denote that the end of the structs block was reached trying to query the next token. */
#define RTFDT_TOKEN_ERROR                       UINT32_MAX


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal Flattened Devicetree instance.
 */
typedef struct RTFDTINT
{
    /** Pointer to the string block. */
    char                    *paszStrings;
    /** Pointer to the raw structs block. */
    uint32_t                *pu32Structs;
    /** Pointer to the array of memory reservation entries. */
    PDTBFDTRSVENTRY         paMemRsv;
    /** Number of memory reservation entries. */
    uint32_t                cMemRsv;
    /** The DTB header (converted to host endianess). */
    DTBFDTHDR               DtbHdr;
} RTFDTINT;
/** Pointer to the internal Flattened Devicetree instance. */
typedef RTFDTINT *PRTFDTINT;


/**
 * DTB property dump callback.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pszProperty     Property name.
 * @param   pvProperty      Pointer to the raw property data.
 * @param   cbProperty      Size of the property in bytes.
 * @param   pErrInfo        Where to return additional error information.
 */
typedef DECLCALLBACKTYPE(int, FNRTFDTDTBPROPERTYDUMP,(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, const char *pszProperty,
                                                      const void *pvProperty, size_t cbProperty, PRTERRINFO pErrInfo));
/** Pointer to a DTB property dump callback. */
typedef FNRTFDTDTBPROPERTYDUMP *PFNRTFDTDTBPROPERTYDUMP;


/**
 * DTB property dump descriptor.
 */
typedef struct RTFDTDTBPROPDUMPDESC
{
    /** Name of the property. */
    const char              *pszProperty;
    /** The dump callback. */
    PFNRTFDTDTBPROPERTYDUMP pfnDump;
} RTFDTDTBPROPDUMPDESC;
/** Pointer to a property dump descriptor. */
typedef RTFDTDTBPROPDUMPDESC *PRTFDTDTBPROPDUMPDESC;
/** Pointer to a const property dump descriptor. */
typedef const RTFDTDTBPROPDUMPDESC *PCRTFDTDTBPROPDUMPDESC;


/**
 * DTB struct dump state.
 */
typedef struct RTFDTDTBDUMP
{
    /** Number of bytes left in the structs block to dump. */
    size_t                  cbLeft;
    /** Pointer to the next item in the structs block. */
    const uint8_t           *pbStructs;
} RTFDTDTBDUMP;
/** Pointer to a DTB struct dump state. */
typedef RTFDTDTBDUMP *PRTFDTDTBDUMP;
/** Pointer to a constant DTB struct dump state. */
typedef const RTFDTDTBDUMP *PCRTFDTDTBDUMP;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef LOG_ENABLED

/**
 * Logs a FDT DTB header.
 *
 * @param   pDtbHdr             The FDT DTB header.
 */
static void rtFdtDtbHdr_Log(PCDTBFDTHDR pDtbHdr)
{
    if (LogIs2Enabled())
    {
        Log2(("RTFdt: Header:\n"));
        Log2(("RTFdt:   u32Magic                    %#RX32\n", RT_BE2H_U32(pDtbHdr->u32Magic)));
        Log2(("RTFdt:   cbFdt                       %#RX32\n", RT_BE2H_U32(pDtbHdr->cbFdt)));
        Log2(("RTFdt:   offDtStruct                 %#RX32\n", RT_BE2H_U32(pDtbHdr->offDtStruct)));
        Log2(("RTFdt:   offDtStrings                %#RX32\n", RT_BE2H_U32(pDtbHdr->offDtStrings)));
        Log2(("RTFdt:   offMemRsvMap                %#RX32\n", RT_BE2H_U32(pDtbHdr->offMemRsvMap)));
        Log2(("RTFdt:   u32Version                  %#RX32\n", RT_BE2H_U32(pDtbHdr->u32Version)));
        Log2(("RTFdt:   u32VersionLastCompatible    %#RX32\n", RT_BE2H_U32(pDtbHdr->u32VersionLastCompatible)));
        Log2(("RTFdt:   u32CpuIdPhysBoot            %#RX32\n", RT_BE2H_U32(pDtbHdr->u32CpuIdPhysBoot)));
        Log2(("RTFdt:   cbDtStrings                 %#RX32\n", RT_BE2H_U32(pDtbHdr->cbDtStrings)));
        Log2(("RTFdt:   cbDtStruct                  %#RX32\n", RT_BE2H_U32(pDtbHdr->cbDtStruct)));
    }
}

#endif /* LOG_ENABLED */


/**
 * Validates the given header.
 *
 * @returns IPRT status code.
 * @param   pDtbHdr         The DTB header to validate.
 * @param   cbDtb           Size of the whole DTB in bytes.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbHdr_Validate(PDTBFDTHDR pDtbHdr, uint64_t cbDtb, PRTERRINFO pErrInfo)
{
    /* Convert to host endianess first. */
    pDtbHdr->u32Magic                   = RT_BE2H_U32(pDtbHdr->u32Magic);
    pDtbHdr->cbFdt                      = RT_BE2H_U32(pDtbHdr->cbFdt);
    pDtbHdr->offDtStruct                = RT_BE2H_U32(pDtbHdr->offDtStruct);
    pDtbHdr->offDtStrings               = RT_BE2H_U32(pDtbHdr->offDtStrings);
    pDtbHdr->offMemRsvMap               = RT_BE2H_U32(pDtbHdr->offMemRsvMap);
    pDtbHdr->u32Version                 = RT_BE2H_U32(pDtbHdr->u32Version);
    pDtbHdr->u32VersionLastCompatible   = RT_BE2H_U32(pDtbHdr->u32VersionLastCompatible);
    pDtbHdr->u32CpuIdPhysBoot           = RT_BE2H_U32(pDtbHdr->u32CpuIdPhysBoot);
    pDtbHdr->cbDtStrings                = RT_BE2H_U32(pDtbHdr->cbDtStrings);
    pDtbHdr->cbDtStruct                 = RT_BE2H_U32(pDtbHdr->cbDtStruct);

    if (pDtbHdr->u32Magic != DTBFDTHDR_MAGIC)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_MAGIC_INVALID, "The magic of the DTB header is invalid (expected %#RX32, got %#RX32)",
                             DTBFDTHDR_MAGIC, pDtbHdr->u32Magic);
    if (pDtbHdr->u32Version != DTBFDTHDR_VERSION)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_VERSION_NOT_SUPPORTED, "Version %u of the DTB is not supported (supported is %u)",
                             pDtbHdr->u32Version, DTBFDTHDR_VERSION);
    if (pDtbHdr->u32VersionLastCompatible != DTBFDTHDR_VERSION_LAST_COMP)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_LAST_COMPAT_VERSION_INVALID, "Last compatible version %u of the DTB is invalid (expected %u)",
                             pDtbHdr->u32VersionLastCompatible, DTBFDTHDR_VERSION_LAST_COMP);
    if (pDtbHdr->cbFdt != cbDtb)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_SIZE_INVALID, "The size of the FDT is invalid (expected %RU32, got %RU32)",
                             cbDtb, pDtbHdr->cbFdt);

    /*
     * Check that any of the offsets is inside the bounds of the FDT and that the memory reservation block comes first,
     * then the structs block and strings last.
     */
    if (   pDtbHdr->offMemRsvMap >= pDtbHdr->cbFdt
        || pDtbHdr->offMemRsvMap < sizeof(*pDtbHdr))
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_MEM_RSV_BLOCK_OFF_INVALID, "Memory reservation block offset is out of bounds (offMemRsvMap=%#RX32 vs. cbFdt=%#%RX32)",
                             pDtbHdr->offMemRsvMap, pDtbHdr->cbFdt);
    if (   pDtbHdr->offDtStruct >= pDtbHdr->cbFdt
        || (pDtbHdr->cbFdt - pDtbHdr->offDtStruct < pDtbHdr->cbDtStruct)
        || pDtbHdr->offDtStruct < pDtbHdr->offMemRsvMap + sizeof(DTBFDTRSVENTRY))
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_STRUCT_BLOCK_OFF_INVALID, "Structs block offset/size is out of bounds (offDtStruct=%#RX32 cbDtStruct=%#RX32 vs. cbFdt=%#RX32 offMemRsvMap=%#RX32)",
                             pDtbHdr->offDtStruct, pDtbHdr->cbDtStruct, pDtbHdr->cbFdt, pDtbHdr->offMemRsvMap);
    if (    pDtbHdr->offDtStrings >= pDtbHdr->cbFdt
        || (pDtbHdr->cbFdt - pDtbHdr->offDtStrings < pDtbHdr->cbDtStrings)
        || pDtbHdr->offDtStrings < pDtbHdr->offDtStruct + pDtbHdr->cbDtStruct)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_HDR_STRINGS_BLOCK_OFF_INVALID, "Strings block offset/size is out of bounds (offDtStrings=%#RX32 cbDtStrings=%#RX32 vs. cbFdt=%#%RX32 offDtStruct=%#RX32)",
                             pDtbHdr->offDtStrings, pDtbHdr->cbDtStrings, pDtbHdr->cbFdt, pDtbHdr->offDtStruct);

    return VINF_SUCCESS;
}


/**
 * Fres all resources allocated for the given FDT.
 *
 * @param   pThis           Pointer to the FDT instance to destroy.
 */
static void rtFdtDestroy(PRTFDTINT pThis)
{
    if (pThis->paszStrings)
        RTMemFree(pThis->paszStrings);
    if (pThis->pu32Structs)
        RTMemFree(pThis->pu32Structs);
    if (pThis->paMemRsv)
        RTMemFree(pThis->paMemRsv);

    pThis->paszStrings = NULL;
    pThis->pu32Structs = NULL;
    pThis->paMemRsv    = NULL;
    pThis->cMemRsv     = 0;
    RT_ZERO(pThis->DtbHdr);
    RTMemFree(pThis);
}


/**
 * Loads the memory reservation block from the underlying VFS I/O stream.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbMemRsvLoad(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    AssertReturn(pThis->DtbHdr.offMemRsvMap < pThis->DtbHdr.offDtStruct, VERR_INTERNAL_ERROR);

    uint32_t cMemRsvMax = (pThis->DtbHdr.offDtStruct - pThis->DtbHdr.offMemRsvMap) / sizeof(*pThis->paMemRsv);
    Assert(cMemRsvMax);

    pThis->paMemRsv = (PDTBFDTRSVENTRY)RTMemAllocZ(cMemRsvMax * sizeof(*pThis->paMemRsv));
    if (!pThis->paMemRsv)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %u bytes of memory for the memory reservation block",
                             cMemRsvMax * sizeof(*pThis->paMemRsv));

    /* Read the entries one after another until the terminator is reached. */
    uint32_t cMemRsv = 0;
    for (;;)
    {
        DTBFDTRSVENTRY MemRsv;
        int rc = RTVfsIoStrmRead(hVfsIos, &MemRsv, sizeof(MemRsv), true /*fBlocking*/, NULL /*pcbRead*/);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Failed to read memory reservation entry %u from I/O stream",
                                 cMemRsv);

        /* Check whether the terminator is reached (no need to convert endianness here). */
        if (   MemRsv.PhysAddrStart == 0
            && MemRsv.cbArea == 0)
            break;

        cMemRsv++;

        /*
         * The terminator must be included in the maximum entry count, if not
         * the DTB is malformed and lacks a terminating entry before the start of the structs block.
         */
        if (cMemRsv == cMemRsvMax)
            return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_MEM_RSV_BLOCK_TERMINATOR_MISSING,
                                 "The memory reservation block lacks a terminating entry");

        pThis->paMemRsv[cMemRsv - 1].PhysAddrStart = RT_BE2H_U64(MemRsv.PhysAddrStart);
        pThis->paMemRsv[cMemRsv - 1].cbArea        = RT_BE2H_U64(MemRsv.cbArea);
    }

    pThis->cMemRsv = cMemRsv;
    return VINF_SUCCESS;
}


/**
 * Loads the structs block of the given FDT.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbStructsLoad(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    pThis->pu32Structs = (uint32_t *)RTMemAllocZ(pThis->DtbHdr.cbDtStruct);
    if (!pThis->pu32Structs)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %u bytes of memory for the structs block",
                             pThis->DtbHdr.cbDtStruct);

    int rc = RTVfsIoStrmReadAt(hVfsIos, pThis->DtbHdr.offDtStruct, pThis->pu32Structs, pThis->DtbHdr.cbDtStruct,
                               true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Failed to read structs block from I/O stream");

    return VINF_SUCCESS;
}


/**
 * Loads the strings block of the given FDT.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbStringsLoad(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    pThis->paszStrings = (char *)RTMemAllocZ(pThis->DtbHdr.cbDtStrings);
    if (!pThis->paszStrings)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %u bytes of memory for the strings block",
                             pThis->DtbHdr.cbDtStrings);

    int rc = RTVfsIoStrmReadAt(hVfsIos, pThis->DtbHdr.offDtStrings, pThis->paszStrings, pThis->DtbHdr.cbDtStrings,
                               true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Failed to read strings block from I/O stream");

    /* Verify that the strings block is terminated. */
    if (pThis->paszStrings[pThis->DtbHdr.cbDtStrings - 1] != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRINGS_BLOCK_NOT_TERMINATED, "The strings block is not zero terminated");

    return VINF_SUCCESS;
}


/**
 * Loads the DTB from the given VFS file.
 *
 * @returns IPRT status code.
 * @param   phFdt           Where to return the FDT handle on success.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtLoadDtb(PRTFDT phFdt, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    DTBFDTHDR DtbHdr;
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsIoStrmQueryInfo(hVfsIos, &ObjInfo, RTFSOBJATTRADD_UNIX);
    if (RT_SUCCESS(rc))
    {
        if ((uint64_t)ObjInfo.cbObject >= sizeof(DtbHdr) + sizeof(DTBFDTRSVENTRY))
        {
            rc = RTVfsIoStrmRead(hVfsIos, &DtbHdr, sizeof(DtbHdr), true /*fBlocking*/, NULL /*pcbRead*/);
            if (RT_SUCCESS(rc))
            {
#ifdef LOG_ENABLED
                rtFdtDtbHdr_Log(&DtbHdr);
#endif

                /* Validate the header. */
                rc = rtFdtDtbHdr_Validate(&DtbHdr, ObjInfo.cbObject, pErrInfo);
                if (RT_SUCCESS(rc))
                {
                    PRTFDTINT pThis = (PRTFDTINT)RTMemAllocZ(sizeof(*pThis));
                    if (RT_LIKELY(pThis))
                    {
                        memcpy(&pThis->DtbHdr, &DtbHdr, sizeof(DtbHdr));

                        /* Load the memory reservation block. */
                        rc = rtFdtDtbMemRsvLoad(pThis, hVfsIos, pErrInfo);
                        if (RT_SUCCESS(rc))
                        {
                            rc = rtFdtDtbStructsLoad(pThis, hVfsIos, pErrInfo);
                            if (RT_SUCCESS(rc))
                            {
                                rc = rtFdtDtbStringsLoad(pThis, hVfsIos, pErrInfo);
                                if (RT_SUCCESS(rc))
                                {
                                    *phFdt = pThis;
                                    return VINF_SUCCESS;
                                }
                            }
                        }

                        rtFdtDestroy(pThis);
                    }
                    else
                        RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate memory for the FDT");
                }
            }
            else
                RTErrInfoSetF(pErrInfo, rc, "Failed to read %u bytes for the DTB header -> %Rrc",
                              sizeof(DtbHdr), rc);
        }
        else
            RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_MALFORMED, "DTB is too small, needs at least %u bytes, only %u available",
                          sizeof(DtbHdr) + sizeof(DTBFDTRSVENTRY), ObjInfo.cbObject);
    }
    else
        RTErrInfoSetF(pErrInfo, rc, "Failed to query size of the DTB -> %Rrc", rc);

    return rc;
}


/**
 * Returns the next token in the structs block from the given start returning an error
 * if beyond the structs block.
 *
 * @returns Next token or RTFDT_TOKEN_ERROR if the end of the structs block is reached.
 * @param   pDump           Pointer to the dump state.
 */
DECLINLINE(uint32_t) rtFdtStructsGetToken(PRTFDTDTBDUMP pDump)
{
    if (pDump->cbLeft < sizeof(uint32_t))
        return RTFDT_TOKEN_ERROR;

    uint32_t u32Token = *(const uint32_t *)pDump->pbStructs;
    pDump->pbStructs += sizeof(uint32_t);
    pDump->cbLeft    -= sizeof(uint32_t);
    return RT_H2BE_U32(u32Token);
}


#ifdef LOG_ENABLED
/**
 * Gets the offset inside the structs block given from the current pointer.
 *
 * @returns Offset in bytes from the start of the structs block.
 * @param   pThis           Pointer to the FDT instance.
 * @param   pDump           Pointer to the dump state.
 */
DECLINLINE(size_t) rtFdtStructsGetOffset(PRTFDTINT pThis, PCRTFDTDTBDUMP pDump)
{
    return pThis->DtbHdr.cbDtStruct - pDump->cbLeft - sizeof(uint32_t);
}
#endif


/**
 * Advances the pointer inside the dump state by the given amount of bytes taking care of the alignment.
 *
 * @returns IPRT status code.
 * @param   pDump           Pointer to the dump state.
 * @param   cbAdv           How many bytes to advance.
 * @param   rcOob           The status code to set if advancing goes beyond the end of the structs block.
 */
DECLINLINE(int) rtFdtStructsDumpAdvance(PRTFDTDTBDUMP pDump, uint32_t cbAdv, int rcOob)
{
    /* Align the pointer to the next 32-bit boundary. */
    const uint8_t *pbStructsNew = RT_ALIGN_PT(pDump->pbStructs + cbAdv, sizeof(uint32_t), uint8_t *);
    if (((uintptr_t)pbStructsNew - (uintptr_t)pDump->pbStructs) > pDump->cbLeft)
        return rcOob;

    pDump->pbStructs = pbStructsNew;
    pDump->cbLeft   -= (uintptr_t)pbStructsNew - (uintptr_t)pDump->pbStructs;
    return VINF_SUCCESS;
}


/**
 * Adds the proper indentation before a new line.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   uIndentLvl      The level of indentation.
 */
static int rtFdtStructsDumpDtsIndent(RTVFSIOSTREAM hVfsIos, uint32_t uIndentLvl)
{
    while (uIndentLvl--)
    {
        ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "    ");
        if (cch != 4)
            return cch < 0 ? -cch : VERR_BUFFER_UNDERFLOW;
    }

    return VINF_SUCCESS;
}


/**
 * Queries a zero terminated ASCII string from the current location inside the structs block.
 *
 * @returns IPRT status code.
 * @param   pDump           The dump state.
 * @param   pszString       The string buffer to copy the string to.
 * @param   cchStringMax    Maximum size of the string buffer in characters (including the terminator).
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtStructsQueryString(PRTFDTDTBDUMP pDump, char *pszString, size_t cchStringMax, PRTERRINFO pErrInfo)
{
    const char *pszStrSrc = (const char *)pDump->pbStructs;
    size_t cbLeft = pDump->cbLeft;

    AssertReturn(cchStringMax, VERR_INTERNAL_ERROR);

    for (;;)
    {
        *pszString++ = *pszStrSrc;
        cchStringMax--;
        cbLeft--;

        if (*pszStrSrc == '\0')
        {
            pszStrSrc++;

            int rc = rtFdtStructsDumpAdvance(pDump, (uintptr_t)pszStrSrc - (uintptr_t)pDump->pbStructs, VERR_FDT_DTB_STRUCTS_BLOCK_MALFORMED_PADDING);
            if (RT_FAILURE(rc))
                return RTErrInfoSetF(pErrInfo, rc, "String end + padding exceeds structs block");

            return VINF_SUCCESS;
        }

        if (!cchStringMax)
            return RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "Structs string too long to fit into target buffer");

        *pszStrSrc++;
        if (!cbLeft)
            return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_STRING_NOT_TERMINATED, "Structs block contains an unterminated string");
    }

    /* Not reached */
}


/**
 * Dumps a string list property.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pszProperty     Name of the property being dumped.
 * @param   pvProperty      Raw property payload.
 * @param   cbProperty      Size of the property payload in bytes.
 * @param   pErrInfo        Where to return additional error information.
 */
static DECLCALLBACK(int) rtFdtDtbPropDumpStringList(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, const char *pszProperty,
                                                    const void *pvProperty, size_t cbProperty, PRTERRINFO pErrInfo)
{
    RT_NOREF(pThis);

    const char *pszProp = (const char *)pvProperty;
    if (pszProp[cbProperty - 1] != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_PROP_STRING_NOT_TERMINATED,
                             "The string payload of property '%s' is not terminated", pszProperty);

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "\"%s\"", pszProp);
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write property data");

    cch = strlen(pszProp) + 1;
    cbProperty -= cch;
    while (cbProperty)
    {
        pszProp += cch;

        cch = RTVfsIoStrmPrintf(hVfsIos, ", \"%s\"", pszProp);
        if (cch <= 0)
            return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write property data");

        cch = strlen(pszProp) + 1;
        cbProperty -= cch;
    }

    return VINF_SUCCESS;
}


/**
 * Dumps a string property.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pszProperty     Name of the property being dumped.
 * @param   pvProperty      Raw property payload.
 * @param   cbProperty      Size of the property payload in bytes.
 * @param   pErrInfo        Where to return additional error information.
 */
static DECLCALLBACK(int) rtFdtDtbPropDumpString(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, const char *pszProperty,
                                                const void *pvProperty, size_t cbProperty, PRTERRINFO pErrInfo)
{
    RT_NOREF(pThis);

    const char *pszProp = (const char *)pvProperty;
    if (pszProp[cbProperty - 1] != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_PROP_STRING_NOT_TERMINATED,
                             "The string payload of property '%s' is not terminated", pszProperty);

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "\"%s\"", pszProp);
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write property data");

    return VINF_SUCCESS;
}


/**
 * Dumps a u32 cell property.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pszProperty     Name of the property being dumped.
 * @param   pvProperty      Raw property payload.
 * @param   cbProperty      Size of the property payload in bytes.
 * @param   pErrInfo        Where to return additional error information.
 */
static DECLCALLBACK(int) rtFdtDtbPropDumpCellsU32(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, const char *pszProperty,
                                                  const void *pvProperty, size_t cbProperty, PRTERRINFO pErrInfo)
{
    RT_NOREF(pThis);

    if (cbProperty % sizeof(uint32_t))
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_PROP_SIZE_MALFORMED,
                             "Property '%s' payload is not a multiple of 32-bit", pszProperty);

    const uint32_t *pu32Prop = (const uint32_t *)pvProperty;

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "<");
    if (cch == 1)
    {
        cch = RTVfsIoStrmPrintf(hVfsIos, "%#RX32", RT_BE2H_U32(*pu32Prop));
        pu32Prop++;
        if (cch > 0)
        {
            for (uint32_t i = 1; i < cbProperty / sizeof(uint32_t); i++)
            {
                cch = RTVfsIoStrmPrintf(hVfsIos, " %#RX32", RT_BE2H_U32(*pu32Prop));
                pu32Prop++;
                if (cch <= 0)
                    break;
            }
        }

        if (cch > 0)
            cch = RTVfsIoStrmPrintf(hVfsIos, ">");
    }

    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write property data");

    return VINF_SUCCESS;
}


/**
 * The known properties to dump.
 */
static const RTFDTDTBPROPDUMPDESC g_aPropDump[] =
{
    { "compatible",             rtFdtDtbPropDumpStringList  }, /** @todo stringlist */
    { "model",                  rtFdtDtbPropDumpString      },
    { "status",                 rtFdtDtbPropDumpString      },
    { "phandle",                rtFdtDtbPropDumpCellsU32    },
    { "linux,phandle",          rtFdtDtbPropDumpCellsU32    },
    { "#address-cells",         rtFdtDtbPropDumpCellsU32    },
    { "#size-cells",            rtFdtDtbPropDumpCellsU32    },
    { "reg",                    rtFdtDtbPropDumpCellsU32    },
    { "virtual-reg",            rtFdtDtbPropDumpCellsU32    },
    { "ranges",                 rtFdtDtbPropDumpCellsU32    },
    { "dma-ranges",             rtFdtDtbPropDumpCellsU32    },
    { "name",                   rtFdtDtbPropDumpString      },
    { "device_type",            rtFdtDtbPropDumpString      },
    { "interrupts",             rtFdtDtbPropDumpCellsU32    },
    { "interrupt-parent",       rtFdtDtbPropDumpCellsU32    },
    { "interrupts-extended",    rtFdtDtbPropDumpCellsU32    },
    { "#interrupt-cells",       rtFdtDtbPropDumpCellsU32    },
    { "interrupt-map",          rtFdtDtbPropDumpCellsU32    },
    { "interrupt-map-mask",     rtFdtDtbPropDumpCellsU32    },
    { "serial-number",          rtFdtDtbPropDumpString      },
    { "chassis-type",           rtFdtDtbPropDumpString      },
    { "clock-frequency",        rtFdtDtbPropDumpCellsU32    },
    { "reg-shift",              rtFdtDtbPropDumpCellsU32    },
    { "label",                  rtFdtDtbPropDumpString      },
    { "clock-names",            rtFdtDtbPropDumpStringList  },
    { "clock-output-names",     rtFdtDtbPropDumpStringList  },
    { "stdout-path",            rtFdtDtbPropDumpString      },
    { "method",                 rtFdtDtbPropDumpString      },
};


/**
 * Dumps the property as a DTS source starting at the given location inside the structs block.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pDump           The dump state.
 * @param   uIndentLvl      The level of indentation.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtStructsDumpPropertyAsDts(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTFDTDTBDUMP pDump, uint32_t uIndentLvl, PRTERRINFO pErrInfo)
{
    DTBFDTPROP Prop;

    if (pDump->cbLeft < sizeof(Prop))
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_PREMATURE_END,
                             "Not enough space left in the structs block to read a property entry");

    memcpy(&Prop, pDump->pbStructs, sizeof(Prop));
    pDump->pbStructs += sizeof(Prop);
    pDump->cbLeft    -= sizeof(Prop);
    Prop.offPropertyName = RT_BE2H_U32(Prop.offPropertyName);
    Prop.cbProperty      = RT_BE2H_U32(Prop.cbProperty);

    if (Prop.offPropertyName >= pThis->DtbHdr.cbDtStrings)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_PROP_NAME_OFF_TOO_LARGE, "Property name offset points past the string block");

    int rc = rtFdtStructsDumpDtsIndent(hVfsIos, uIndentLvl);
    if (RT_FAILURE(rc))
        return rc;

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "%s", &pThis->paszStrings[Prop.offPropertyName]);
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write property name: '%s'",
                             &pThis->paszStrings[Prop.offPropertyName]);

    const uint8_t *pbProp = pDump->pbStructs;
    if (Prop.cbProperty)
    {
        if (Prop.cbProperty > pDump->cbLeft)
            return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_PREMATURE_END, "Property '%s' data exceeds struct blocks",
                                 &pThis->paszStrings[Prop.offPropertyName]);

        cch = RTVfsIoStrmPrintf(hVfsIos, " = ");
        if (cch <= 0)
            return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to output property");

        rc = VERR_NOT_FOUND;
        for (uint32_t i = 0; i < RT_ELEMENTS(g_aPropDump); i++)
        {
            if (!strcmp(g_aPropDump[i].pszProperty, &pThis->paszStrings[Prop.offPropertyName]))
            {
                rc = g_aPropDump[i].pfnDump(pThis, hVfsIos, &pThis->paszStrings[Prop.offPropertyName],
                                            pbProp, Prop.cbProperty, pErrInfo);
                break;
            }
        }

        /* If not found use the standard U32 cells dumper. */
        if (rc == VERR_NOT_FOUND)
            rc = rtFdtDtbPropDumpCellsU32(pThis, hVfsIos, &pThis->paszStrings[Prop.offPropertyName],
                                          pbProp, Prop.cbProperty, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;

        cch = RTVfsIoStrmPrintf(hVfsIos, ";\n");
        if (cch <= 0)
            return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to output property");

        rc = rtFdtStructsDumpAdvance(pDump, Prop.cbProperty, VERR_FDT_DTB_STRUCTS_BLOCK_PREMATURE_END);
    }
    else
    {
        cch = RTVfsIoStrmPrintf(hVfsIos, ";\n");
        if (cch <= 0)
            return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write newline");
    }

    return rc;
}


/**
 * Dumps the node name as a DTS source starting at the given location inside the structs block.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pDump           The dump state.
 * @param   uIndentLvl      The level of indentation.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtStructsDumpNodeAsDts(RTVFSIOSTREAM hVfsIos, PRTFDTDTBDUMP pDump, uint32_t uIndentLvl, PRTERRINFO pErrInfo)
{
    char szNdName[512]; /* Should be plenty. */

    int rc = rtFdtStructsQueryString(pDump, &szNdName[0], sizeof(szNdName), pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "\n", szNdName);
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write DTS node name");

    rc = rtFdtStructsDumpDtsIndent(hVfsIos, uIndentLvl);
    if (RT_FAILURE(rc))
        return rc;

    cch = RTVfsIoStrmPrintf(hVfsIos, "%s {\n", szNdName);
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write DTS node name");

    return VINF_SUCCESS;
}


static int rtFdtStructsDumpEndNodeAsDts(RTVFSIOSTREAM hVfsIos, uint32_t uIndentLvl, PRTERRINFO pErrInfo)
{
    int rc = rtFdtStructsDumpDtsIndent(hVfsIos, uIndentLvl);
    if (RT_FAILURE(rc))
        return rc;

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "};\n");
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write DTS node closing");

    return VINF_SUCCESS;
}


/**
 * Dumps the given FDT as DTS v1 sources from the root node.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDumpRootAsDts(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    RTFDTDTBDUMP Dump;

    Dump.cbLeft    = pThis->DtbHdr.cbDtStruct;
    Dump.pbStructs = (const uint8_t *)pThis->pu32Structs;

    /* Skip any NOP tokens. */
    uint32_t u32Token = rtFdtStructsGetToken(&Dump);
    while (u32Token == DTB_FDT_TOKEN_NOP)
        u32Token = rtFdtStructsGetToken(&Dump);

    /* The root node starts with a BEGIN_NODE token. */
    if (u32Token != DTB_FDT_TOKEN_BEGIN_NODE)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_TOKEN_INVALID, "The structs block doesn't start with the BEGIN_NODE token for the root node: %#RX32",
                             RT_BE2H_U32(u32Token));

    /* Load the name for the root node (should be an empty string). */
    char chNdRootName;
    int rc = rtFdtStructsQueryString(&Dump, &chNdRootName, sizeof(chNdRootName), pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    if (chNdRootName != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_NODE_NAME_INVALID, "The root node name isn't zero terminated: %c",
                             chNdRootName);

    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "/ {\n");
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write DTS root node");

    uint32_t uNdLvl = 1;
    u32Token = rtFdtStructsGetToken(&Dump);
    while (u32Token != DTB_FDT_TOKEN_END)
    {
        Log4(("rtFdtDumpAsDtsRoot: Token %#RX32 at offset %#zx\n", RT_BE2H_U32(u32Token), rtFdtStructsGetOffset(pThis, &Dump)));

        switch (u32Token)
        {
            case DTB_FDT_TOKEN_BEGIN_NODE:
                Log3(("rtFdtDumpAsDtsRoot: BEGIN_NODE token at offset %#zx\n", rtFdtStructsGetOffset(pThis, &Dump)));
                rc = rtFdtStructsDumpNodeAsDts(hVfsIos, &Dump, uNdLvl, pErrInfo);
                if (RT_FAILURE(rc))
                    return rc;

                uNdLvl++;
                break;
            case DTB_FDT_TOKEN_PROPERTY:
                Log3(("rtFdtDumpAsDtsRoot: PROP token at offset %#zx\n", rtFdtStructsGetOffset(pThis, &Dump)));
                rc = rtFdtStructsDumpPropertyAsDts(pThis, hVfsIos, &Dump, uNdLvl, pErrInfo);
                if (RT_FAILURE(rc))
                    return rc;
                break;
            case DTB_FDT_TOKEN_NOP:
                Log3(("rtFdtDumpAsDtsRoot: NOP token at offset %#zx\n", rtFdtStructsGetOffset(pThis, &Dump)));
                break;
            case DTB_FDT_TOKEN_END_NODE:
                Log3(("rtFdtDumpAsDtsRoot: END_NODE token at offset %#zx\n", rtFdtStructsGetOffset(pThis, &Dump)));
                if (!uNdLvl)
                    return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_PREMATURE_END,
                                         "END_NODE token encountered at the root node");

                uNdLvl--;
                rc = rtFdtStructsDumpEndNodeAsDts(hVfsIos, uNdLvl, pErrInfo);
                if (RT_FAILURE(rc))
                    return rc;
                break;
            case RTFDT_TOKEN_ERROR:
                return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_TOKEN_INVALID, "The structs block is malformed");
            default:
                return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_TOKEN_INVALID, "The structs block contains an invalid/unknown token: %#RX32",
                                     RT_BE2H_U32(u32Token));
        }

        u32Token = rtFdtStructsGetToken(&Dump);
        if (u32Token == DTB_FDT_TOKEN_END)
            Log3(("rtFdtDumpAsDtsRoot: END token at offset %#zx\n", rtFdtStructsGetOffset(pThis, &Dump)));
    }

    /* Need to end on an END token. */
    if (u32Token != DTB_FDT_TOKEN_END)
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRUCTS_BLOCK_TOKEN_INVALID, "The structs block doesn't end with an END token (got %#RX32, expected %#RX32)",
                             RT_BE2H_U32(u32Token), DTB_FDT_TOKEN_END);

    return VINF_SUCCESS;
}


/**
 * Dumps the given FDT instance as DTS source.
 *
 * @returns IPRT status code.
 * @param   pThis           Pointer to the FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to dump the DTS to.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDumpAsDts(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "/dts-v1/;\n");
    if (cch <= 0)
        return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write DTS header");

    /* Write memory reservations. */
    for (uint32_t i = 0; i < pThis->cMemRsv; i++)
    {
        cch = RTVfsIoStrmPrintf(hVfsIos, "/memreserve/ %#RX64 %#RX64;\n", pThis->paMemRsv[i].PhysAddrStart, pThis->paMemRsv[i].cbArea);
        if (cch <= 0)
            return RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : -cch, "Failed to write memory reervation block %u", i);
    }

    /* Dump the tree. */
    return rtFdtDumpRootAsDts(pThis, hVfsIos, pErrInfo);
}


RTDECL(int) RTFdtCreateEmpty(PRTFDT phFdt)
{
    RT_NOREF(phFdt);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFdtCreateFromVfsIoStrm(PRTFDT phFdt, RTVFSIOSTREAM hVfsIos, RTFDTTYPE enmInType, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phFdt, VERR_INVALID_POINTER);
    AssertReturn(hVfsIos != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);

    if (enmInType == RTFDTTYPE_DTB)
        return rtFdtLoadDtb(phFdt, hVfsIos, pErrInfo);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFdtCreateFromFile(PRTFDT phFdt, const char *pszFilename, RTFDTTYPE enmInType, PRTERRINFO pErrInfo)
{
    RT_NOREF(phFdt, pszFilename, enmInType, pErrInfo);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(void) RTFdtDestroy(RTFDT hFdt)
{
    PRTFDTINT pThis = hFdt;
    AssertPtrReturnVoid(pThis);

    rtFdtDestroy(pThis);
}


RTDECL(int) RTFdtDumpToVfsIoStrm(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    PRTFDTINT pThis = hFdt;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    RT_NOREF(fFlags);
    if (enmOutType == RTFDTTYPE_DTS)
        return rtFdtDumpAsDts(pThis, hVfsIos, pErrInfo);

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFdtDumpToFile(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, const char *pszFilename, PRTERRINFO pErrInfo)
{
    RT_NOREF(hFdt, enmOutType, fFlags, pszFilename, pErrInfo);
    return VERR_NOT_IMPLEMENTED;
}
