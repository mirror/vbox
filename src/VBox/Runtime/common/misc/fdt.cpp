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
    const char              *paszStrings;
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
static int rtFdtDtbHdr_Validate(PCDTBFDTHDR pDtbHdr, uint64_t cbDtb, PRTERRINFO pErrInfo)
{
    RT_NOREF(pDtbHdr, cbDtb, pErrInfo);
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
                    RT_NOREF(phFdt);
                }
            }
            else
                RTErrInfoSetF(pErrInfo, rc, "Failed to read %u bytes for the DTB header -> %Rrc\n",
                              sizeof(DtbHdr));
        }
        else
            RTErrInfoSetF(pErrInfo, VERR_FDT_DTB_MALFORMED, "DTB is too small, needs at least %u bytes, only %u available\n",
                          sizeof(DtbHdr) + sizeof(DTBFDTRSVENTRY), ObjInfo.cbObject);
    }
    else
        RTErrInfoSetF(pErrInfo, rc, "Failed to query size of the DTB -> %Rrc\n", rc);

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
    RT_NOREF(hFdt);
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
