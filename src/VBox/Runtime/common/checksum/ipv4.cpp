/* $Id$ */
/** @file
 * IPRT - IPv4 Checksum calculation and validation.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/net.h>
#include <iprt/assert.h>


/**
 * Calculates the checksum of the IPv4 header.
 *
 * @returns Checksum.
 * @param   pIpHdr      Pointer to the IPv4 header to checksum. Assumes
 *                      the caller already checked the minimum size requirement.
 */
RTDECL(uint16_t) RTNetIPv4Checksum(PCRTNETIPV4 pIpHdr)
{
    uint16_t const *paw = (uint16_t const *)pIpHdr;
    int32_t iSum = paw[0]              /* ip_hl */
                 + paw[1]              /* ip_len */
                 + paw[2]              /* ip_id */
                 + paw[3]              /* ip_off */
                 + paw[4]              /* ip_ttl */
                 /*+ paw[5] == 0 */    /* ip_sum */
                 + paw[6]              /* ip_src */
                 + paw[7]              /* ip_src:16 */
                 + paw[8]              /* ip_dst */
                 + paw[9];             /* ip_dst:16 */
    /* any options */
    if (pIpHdr->ip_hl > 20 / 4)
    {
        /* this is a bit insane... */
        switch (pIpHdr->ip_hl)
        {
            case 6:  iSum += paw[10] + paw[11]; break;
            case 7:  iSum += paw[10] + paw[11] + paw[12] + paw[13]; break;
            case 8:  iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15]; break;
            case 9:  iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17]; break;
            case 10: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19]; break;
            case 11: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19] + paw[20] + paw[21]; break;
            case 12: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19] + paw[20] + paw[21] + paw[22] + paw[23]; break;
            case 13: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19] + paw[20] + paw[21] + paw[22] + paw[23] + paw[24] + paw[25]; break;
            case 14: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19] + paw[20] + paw[21] + paw[22] + paw[23] + paw[24] + paw[25] + paw[26] + paw[27]; break;
            case 15: iSum += paw[10] + paw[11] + paw[12] + paw[13] + paw[14] + paw[15] + paw[16] + paw[17] + paw[18] + paw[19] + paw[20] + paw[21] + paw[22] + paw[23] + paw[24] + paw[25] + paw[26] + paw[27] + paw[28] + paw[29]; break;
            default:
                AssertFailed();
        }
    }

    /* 16-bit one complement fun */
    iSum = (iSum >> 16) + (iSum & 0xffff);  /* hi + low words */
    iSum += iSum >> 16;                     /* carry */
    return (uint16_t)~iSum;
}



/**
 * Verifies the header version, header size, packet size, and header checksum
 * of the specified IPv4 header.
 *
 * @returns true if valid, false if invalid.
 * @param   pIpHdr      Pointer to the IPv4 header to validate.
 * @param   cbHdrMax    The max header size, or  the max size of what pIpHdr points
 *                      to if you like. Note that an IPv4 header can be up to 60 bytes.
 * @param   cbPktMax    The max IP packet size, IP header and payload. This doesn't have
 *                      to be mapped following pIpHdr.
 */
RTDECL(bool) RTNetIPv4IsValid(PCRTNETIPV4 pIpHdr, size_t cbHdrMax, size_t cbPktMax)
{
    Assert(cbPktMax >= cbHdrMax);
    if (RT_UNLIKELY(cbHdrMax < RTNETIPV4_MIN_LEN))
        return false;
    if (RT_UNLIKELY(pIpHdr->ip_hl * 4 < RTNETIPV4_MIN_LEN))
        return false;
    if (RT_UNLIKELY(pIpHdr->ip_hl * 4 > cbHdrMax))
    {
        Assert(pIpHdr->ip_hl * 4 > cbPktMax); /* You'll hit this if you mapped/copy too little of the header! */
        return false;
    }
    if (RT_UNLIKELY(pIpHdr->ip_v != 4))
        return false;
    if (RT_UNLIKELY(RT_BE2H_U16(pIpHdr->ip_len) > cbPktMax))
        return false;
    uint16_t u16Sum = RTNetIPv4Checksum(pIpHdr);
    if (RT_UNLIKELY(RT_BE2H_U16(pIpHdr->ip_sum) != u16Sum))
        return false;
    return true;
}


