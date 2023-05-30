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
        || pDtbHdr->offDtStruct <= pDtbHdr->offMemRsvMap + sizeof(DTBFDTRSVENTRY))
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
 * @param   pThis           The FDT instance to destroy.
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
 * @param   pThis           The FDT instance.
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

        /*
         * The terminator must be included in the maximum entry count, if not
         * the DTB is malformed and lacks a terminating entry before the start of the structs block.
         */
        if (cMemRsv + 1 == cMemRsvMax)
            return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_MEM_RSV_BLOCK_TERMINATOR_MISSING,
                                 "The memory reservation block lacks a terminating entry");

        pThis->paMemRsv[cMemRsv].PhysAddrStart = RT_BE2H_U64(MemRsv.PhysAddrStart);
        pThis->paMemRsv[cMemRsv].cbArea        = RT_BE2H_U64(MemRsv.cbArea);
        cMemRsv++;
    }

    pThis->cMemRsv = cMemRsv + 1;
    return VINF_SUCCESS;
}


/**
 * Loads the structs block of the given FDT.
 *
 * @returns IPRT status code.
 * @param   pThis           The FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbStructsLoad(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    pThis->pu32Structs = (uint32_t *)RTMemAllocZ(pThis->DtbHdr.cbDtStruct);
    if (!pThis->pu32Structs)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %u bytes of memory for the structs block",
                             pThis->DtbHdr.cbDtStruct);

    int rc = RTVfsIoStrmRead(hVfsIos, pThis->pu32Structs, pThis->DtbHdr.cbDtStruct, true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Failed to read structs block from I/O stream");

    return VINF_SUCCESS;
}


/**
 * Loads the strings block of the given FDT.
 *
 * @returns IPRT status code.
 * @param   pThis           The FDT instance.
 * @param   hVfsIos         The VFS I/O stream handle to load the DTB from.
 * @param   pErrInfo        Where to return additional error information.
 */
static int rtFdtDtbStringsLoad(PRTFDTINT pThis, RTVFSIOSTREAM hVfsIos, PRTERRINFO pErrInfo)
{
    pThis->paszStrings = (char *)RTMemAllocZ(pThis->DtbHdr.cbDtStrings);
    if (!pThis->paszStrings)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %u bytes of memory for the strings block",
                             pThis->DtbHdr.cbDtStrings);

    int rc = RTVfsIoStrmRead(hVfsIos, pThis->paszStrings, pThis->DtbHdr.cbDtStrings, true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTErrInfoSetF(pErrInfo, rc, "Failed to read strings block from I/O stream");

    /* Verify that the strings block is terminated. */
    if (pThis->paszStrings[pThis->DtbHdr.cbDtStrings - 1] != '\0')
        return RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_STRINGS_BLOCK_NOT_TERMINATED, "The strings block is not zero temrinated");

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


RTDECL(int) RTFdtDumpToVfsIoStrm(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, RTVFSIOSTREAM hVfsIos)
{
    RT_NOREF(hFdt, enmOutType, fFlags, hVfsIos);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFdtDumpToFile(RTFDT hFdt, RTFDTTYPE enmOutType, uint32_t fFlags, const char *pszFilename)
{
    RT_NOREF(hFdt, enmOutType, fFlags, pszFilename);
    return VERR_NOT_IMPLEMENTED;
}
