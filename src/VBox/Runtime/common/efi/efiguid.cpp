/* $Id$ */
/** @file
 * IPRT - EFI GUID conversion helpers.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <iprt/efi.h>

#include <iprt/cdefs.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

RTDECL(PRTUUID) RTEfiGuidToUuid(PRTUUID pUuid, PCEFI_GUID pEfiGuid)
{
    pUuid->Gen.u32TimeLow              = RT_LE2H_U32(pEfiGuid->u32Data1);
    pUuid->Gen.u16TimeMid              = RT_LE2H_U16(pEfiGuid->u16Data2);
    pUuid->Gen.u16TimeHiAndVersion     = RT_LE2H_U16(pEfiGuid->u16Data3);
    pUuid->Gen.u8ClockSeqHiAndReserved = pEfiGuid->abData4[0];
    pUuid->Gen.u8ClockSeqLow           = pEfiGuid->abData4[1];
    pUuid->Gen.au8Node[0]              = pEfiGuid->abData4[2];
    pUuid->Gen.au8Node[1]              = pEfiGuid->abData4[3];
    pUuid->Gen.au8Node[2]              = pEfiGuid->abData4[4];
    pUuid->Gen.au8Node[3]              = pEfiGuid->abData4[5];
    pUuid->Gen.au8Node[4]              = pEfiGuid->abData4[6];
    pUuid->Gen.au8Node[5]              = pEfiGuid->abData4[7];
    return pUuid;
}


RTDECL(PEFI_GUID) RTEfiGuidFromUuid(PEFI_GUID pEfiGuid, PCRTUUID pUuid)
{
    pEfiGuid->u32Data1   = RT_H2LE_U32(pUuid->Gen.u32TimeLow);
    pEfiGuid->u16Data2   = RT_H2LE_U16(pUuid->Gen.u16TimeMid);
    pEfiGuid->u16Data3   = RT_H2LE_U16(pUuid->Gen.u16TimeHiAndVersion);
    pEfiGuid->abData4[0] = pUuid->Gen.u8ClockSeqHiAndReserved;
    pEfiGuid->abData4[1] = pUuid->Gen.u8ClockSeqLow;
    pEfiGuid->abData4[2] = pUuid->Gen.au8Node[0];
    pEfiGuid->abData4[3] = pUuid->Gen.au8Node[1];
    pEfiGuid->abData4[4] = pUuid->Gen.au8Node[2];
    pEfiGuid->abData4[5] = pUuid->Gen.au8Node[3];
    pEfiGuid->abData4[6] = pUuid->Gen.au8Node[4];
    pEfiGuid->abData4[7] = pUuid->Gen.au8Node[5];
    return pEfiGuid;
}

