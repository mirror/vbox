/* $Id$ */
/** @file
 * VBoxNetUDP - UDP Library for IntNet.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxNetUDP.h"
#include <iprt/stream.h>
#include <iprt/string.h>


/**
 * Checks if the head of the receive ring is a UDP packet matching the given
 * criteria.
 *
 * @returns Pointer to the data if it matches.
 * @param   pBuf            The IntNet buffers.
 * @param   uDstPort        The destination port to match.
 * @param   pDstMac         The destination address to match if
 *                          VBOXNETUDP_MATCH_UNICAST is specied.
 * @param   fFlags          Flags indicating what to match and some debug stuff.
 *                          See VBOXNETUDP_MATCH_*.
 * @param   pHdrs           Where to return the pointers to the headers.
 *                          Optional.
 * @param   pcb             Where to return the size of the data on success.
 */
void *VBoxNetUDPMatch(PCINTNETBUF pBuf, unsigned uDstPort, PCRTMAC pDstMac, uint32_t fFlags, PVBOXNETUDPHDRS pHdrs, size_t *pcb)
{
    /*
     * Clear return values so we can return easier on mismatch.
     */
    *pcb = 0;
    if (pHdrs)
    {
        pHdrs->pEth  = NULL;
        pHdrs->pIpv4 = NULL;
        pHdrs->pUdp  = NULL;
    }

    /*
     * Valid IntNet Ethernet frame?
     */
    PCINTNETHDR pHdr = (PINTNETHDR)((uintptr_t)pBuf + pBuf->Recv.offRead);
    if (pHdr->u16Type != INTNETHDR_TYPE_FRAME)
        return NULL;

    size_t      cbFrame = pHdr->cbFrame;
    const void *pvFrame = INTNETHdrGetFramePtr(pHdr, pBuf);
    PCRTNETETHERHDR pEthHdr = (PCRTNETETHERHDR)pvFrame;
    if (pHdrs)
        pHdrs->pEth = pEthHdr;

#ifdef IN_RING3
    /* Dump if to stderr/log if that's wanted. */
    if (fFlags & VBOXNETUDP_MATCH_PRINT_STDERR)
    {
        RTStrmPrintf(g_pStdErr, "frame: cb=%04x dst=%.6Rhxs src=%.6Rhxs type=%04x%s\n",
                     cbFrame, &pEthHdr->DstMac, &pEthHdr->SrcMac, RT_BE2H_U16(pEthHdr->EtherType),
                     !memcmp(&pEthHdr->DstMac, pDstMac, sizeof(*pDstMac)) ? " Mine!" : "");
    }
#endif

    /*
     * Ethernet matching.
     */

    /* Ethernet min frame size. */
    if (cbFrame < 64)
        return NULL;

    /* Match Ethertype: IPV4? */
    /** @todo VLAN tagging? */
    if (pEthHdr->EtherType != RT_H2BE_U16_C(RTNET_ETHERTYPE_IPV4))
        return NULL;

    /* Match destination address (ethernet) */
    if (    (   !(fFlags & VBOXNETUDP_MATCH_UNICAST)
             || memcmp(&pEthHdr->DstMac, pDstMac, sizeof(pEthHdr->DstMac)))
        &&  (   !(fFlags & VBOXNETUDP_MATCH_BROADCAST)
             || pEthHdr->DstMac.au16[0] != 0xffff
             || pEthHdr->DstMac.au16[1] != 0xffff
             || pEthHdr->DstMac.au16[2] != 0xffff))
        return NULL;

    /*
     * IP validation and matching.
     */
    PCRTNETIPV4 pIpHdr = (PCRTNETIPV4)(pEthHdr + 1);
    if (pHdrs)
        pHdrs->pIpv4 = pIpHdr;

    /* Protocol: UDP */
    if (pIpHdr->ip_p != RTNETIPV4_PROT_UDP)
        return NULL;

    /* Valid IPv4 header? */
    size_t const offIpHdr = (uintptr_t)pIpHdr - (uintptr_t)pEthHdr;
    if (!RTNetIPv4IsHdrValid(pIpHdr, cbFrame, cbFrame - offIpHdr))
        return NULL;

    /*
     * UDP matching and validation.
     */
    PCRTNETUDP  pUdpHdr   = (PCRTNETUDP)((uint32_t *)pIpHdr + pIpHdr->ip_hl);
    if (pHdrs)
        pHdrs->pUdp = pUdpHdr;

    /* Destination port */
    if (RT_BE2H_U16(pUdpHdr->uh_dport) != uDstPort)
        return NULL;

    /* Validate the UDP header according to flags. */
    size_t      offUdpHdr = (uintptr_t)pUdpHdr - (uintptr_t)pEthHdr;
    if (fFlags & (VBOXNETUDP_MATCH_CHECKSUM | VBOXNETUDP_MATCH_REQUIRE_CHECKSUM))
    {
        if (!RTNetIPv4IsUDPValid(pIpHdr, pUdpHdr, pUdpHdr + 1, cbFrame - offUdpHdr))
            return NULL;
        if (    (fFlags & VBOXNETUDP_MATCH_REQUIRE_CHECKSUM)
            &&  !pUdpHdr->uh_sum)
            return NULL;
    }
    else
    {
        if (!RTNetIPv4IsUDPSizeValid(pIpHdr, pUdpHdr, cbFrame - offUdpHdr))
            return NULL;
    }

    /*
     * We've got a match!
     */
    *pcb = pUdpHdr->uh_ulen - sizeof(*pUdpHdr);
    return (void *)(pUdpHdr + 1);
}

