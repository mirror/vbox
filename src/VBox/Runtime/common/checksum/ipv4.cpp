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
 * @param   pIpHdr      Pointer to the IPv4 header to checksum, network endian (big).
 *                      Assumes the caller already checked the minimum size requirement.
 */
RTDECL(uint16_t) RTNetIPv4Checksum(PCRTNETIPV4 pIpHdr)
{
    uint16_t const *paw = (uint16_t const *)pIpHdr;
    int32_t iSum = paw[0]               /* ip_hl */
                 + paw[1]               /* ip_len */
                 + paw[2]               /* ip_id */
                 + paw[3]               /* ip_off */
                 + paw[4]               /* ip_ttl */
                 /*+ paw[5] == 0 */     /* ip_sum */
                 + paw[6]               /* ip_src */
                 + paw[7]               /* ip_src:16 */
                 + paw[8]               /* ip_dst */
                 + paw[9];              /* ip_dst:16 */
    /* any options */
    if (pIpHdr->ip_hl > 20 / 4)
    {
        /* this is a bit insane... (identical to the TCP header) */
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
 * @param   pIpHdr      Pointer to the IPv4 header to validate. Network endian (big).
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


/**
 * Calculates the checksum of a pseudo header given an IPv4 header.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pIpHdr      The IP header (network endian (big)).
 */
RTDECL(uint32_t) RTNetIPv4PseudoChecksum(PCRTNETIPV4 pIpHdr)
{
    uint16_t cbPayload = RT_BE2H_U16(pIpHdr->ip_len) - pIpHdr->ip_hl * 4;
    uint32_t iSum = pIpHdr->ip_src.au16[0]
                  + pIpHdr->ip_src.au16[1]
                  + pIpHdr->ip_dst.au16[0]
                  + pIpHdr->ip_dst.au16[1]
#ifdef RT_BIG_ENDIAN
                  + pIpHdr->ip_p
#else
                  + ((uint32_t)pIpHdr->ip_p << 8)
#endif
                  + RT_H2BE_U16(cbPayload);
    return iSum;
}


/**
 * Calculates the checksum of a pseudo header given the individual components.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   SrcAddr         The source address in host endian.
 * @param   DstAddr         The destination address in host endian.
 * @param   bProtocol       The protocol number.
 * @param   cbPkt           The packet size (host endian of course) (no IPv4 header).
 */
RTDECL(uint32_t) RTNetIPv4PseudoChecksumBits(RTNETADDRIPV4 SrcAddr, RTNETADDRIPV4 DstAddr, uint8_t bProtocol, uint16_t cbPkt)
{
    uint32_t iSum = RT_H2BE_U16(SrcAddr.au16[0])
                  + RT_H2BE_U16(SrcAddr.au16[1])
                  + RT_H2BE_U16(DstAddr.au16[0])
                  + RT_H2BE_U16(DstAddr.au16[1])
#ifdef RT_BIG_ENDIAN
                  + bProtocol
#else
                  + ((uint32_t)bProtocol << 8)
#endif
                  + RT_H2BE_U16(cbPkt);
    return iSum;
}


/**
 * Adds the checksum of the UDP header to the intermediate checksum value.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pUdpHdr         Pointer to the UDP header to checksum, network endian (big).
 * @param   iSum            The 32-bit intermediate checksum value.
 */
RTDECL(uint32_t) RTNetIPv4AddUDPChecksum(PCRTNETUDP pUdpHdr, uint32_t iSum)
{
    iSum += pUdpHdr->uh_sport
          + pUdpHdr->uh_dport
          /*+ pUdpHdr->uh_sum = 0 */
          + pUdpHdr->uh_ulen;
    return iSum;
}


/**
 * Adds the checksum of the TCP header to the intermediate checksum value.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pUdpHdr         Pointer to the TCP header to checksum, network endian (big).
 *                          Assums the caller has already validate it and made sure the
 *                          entire header is present.
 * @param   iSum            The 32-bit intermediate checksum value.
 */
RTDECL(uint32_t) RTNetIPv4AddTCPChecksum(PCRTNETTCP pTcpHdr, uint32_t iSum)
{
    uint16_t const *paw = (uint16_t const *)pTcpHdr;
    iSum += paw[0]                      /* th_sport */
          + paw[1]                      /* th_dport */
          + paw[2]                      /* th_seq */
          + paw[3]                      /* th_seq:16 */
          + paw[4]                      /* th_ack */
          + paw[5]                      /* th_ack:16 */
          + paw[6]                      /* th_off, th_x2, th_flags */
          + paw[7]                      /* th_win */
          /*+ paw[8] == 0 */            /* th_sum */
          + paw[9];                     /* th_urp */
    if (pTcpHdr->th_off > RTNETTCP_MIN_LEN / 4)
    {
        /* this is a bit insane... (identical to the IPv4 header) */
        switch (pTcpHdr->th_off)
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

    return iSum;
}


/**
 * Adds the checksum of the specified data segment to the intermediate checksum value.
 *
 * @returns 32-bit intermediary checksum value.
 * @param   pUdpHdr         Pointer to the UDP header to checksum, network endian (big).
 * @param   iSum            The 32-bit intermediate checksum value.
 * @param   pfOdd           This is used to keep track of odd bits, initialize to false
 *                          when starting to checksum the data (aka text) after a TCP
 *                          or UDP header (data never start at an odd offset).
 */
RTDECL(uint32_t) RTNetIPv4AddDataChecksum(void const *pvData, size_t cbData, uint32_t iSum, bool *pfOdd)
{
    if (*pfOdd)
    {
#ifdef RT_BIG_ENDIAN
        /* there was an odd byte in the previous chunk, add the lower byte. */
        iSum += *(uint8_t *)pvData;
#else
        /* there was an odd byte in the previous chunk, add the upper byte. */
        iSum += (uint32_t)*(uint8_t *)pvData << 8;
#endif
        /* skip the byte. */
        cbData--;
        if (!cbData)
            return iSum;
        pvData = (uint8_t const *)pvData + 1;
    }

    /* iterate the data. */
    uint16_t const *pw = (uint16_t const *)pvData;
    while (cbData > 1)
    {
        iSum += *pw;
        pw += 2;
        cbData -= 2;
    }

    /* handle odd byte. */
    if (cbData)
    {
#ifdef RT_BIG_ENDIAN
        iSum += (uint32_t)*(uint8_t *)pw << 8;
#else
        iSum += *(uint8_t *)pw;
#endif
        *pfOdd = true;
    }
    else
        *pfOdd = false;
    return iSum;
}


/**
 * Finalizes a IPv4 checksum.
 *
 * @returns The checksum.
 * @param   iSum            The 32-bit intermediate checksum value.
 */
RTDECL(uint16_t) RTNetIPv4FinalizeChecksum(uint32_t iSum)
{
    /* 16-bit one complement fun */
    iSum = (iSum >> 16) + (iSum & 0xffff);  /* hi + low words */
    iSum += iSum >> 16;                     /* carry */
    return (uint16_t)~iSum;
}



