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
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxNetUDP.h"
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/rand.h>
#include <VBox/vmm.h>
#include <VBox/log.h>


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
    if (!RTNetIPv4IsHdrValid(pIpHdr, cbFrame - offIpHdr, cbFrame - offIpHdr))
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


/**
 * Flushes the send buffer.
 *
 * @returns VBox status code.
 * @param   pSession        The support driver session.
 * @param   hIf             The interface handle to flush.
 */
static int vboxnetudpFlush(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf)
{
    INTNETIFSENDREQ SendReq;
    SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SendReq.Hdr.cbReq    = sizeof(SendReq);
    SendReq.pSession     = pSession;
    SendReq.hIf          = hIf;
    return SUPCallVMMR0Ex(NIL_RTR0PTR, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);
}


/**
 * Copys the SG segments into the specified fram.
 *
 * @param   pvFrame     The frame buffer.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 */
static void vboxnetudpCopySG(void *pvFrame, size_t cSegs, PCINTNETSEG paSegs)
{
    uint8_t *pbDst = (uint8_t *)pvFrame;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
    {
        memcpy(pbDst, paSegs[iSeg].pv, paSegs[iSeg].cb);
        pbDst += paSegs[iSeg].cb;
    }
}


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 * @remark  This is the same as INTNETRingWriteFrame and
 *          drvIntNetRingWriteFrame.
 */
static int vboxnetudpRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf,
                                    size_t cSegs, PCINTNETSEG paSegs)
{
    /*
     * Validate input.
     */
    Assert(pBuf);
    Assert(pRingBuf);
    uint32_t offWrite = pRingBuf->offWrite;
    Assert(offWrite == RT_ALIGN_32(offWrite, sizeof(INTNETHDR)));
    uint32_t offRead = pRingBuf->offRead;
    Assert(offRead == RT_ALIGN_32(offRead, sizeof(INTNETHDR)));
    AssertPtr(paSegs);
    Assert(cSegs > 0);

    /* Calc frame size. */
    uint32_t cbFrame = 0;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
        cbFrame += paSegs[iSeg].cb;

    Assert(cbFrame >= sizeof(RTMAC) * 2);


    const uint32_t cb = RT_ALIGN_32(cbFrame, sizeof(INTNETHDR));
    if (offRead <= offWrite)
    {
        /*
         * Try fit it all before the end of the buffer.
         */
        if (pRingBuf->offEnd - offWrite >= cb + sizeof(INTNETHDR))
        {
            PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = sizeof(INTNETHDR);

            vboxnetudpCopySG(pHdr + 1, cSegs, paSegs);

            offWrite += cb + sizeof(INTNETHDR);
            Assert(offWrite <= pRingBuf->offEnd && offWrite >= pRingBuf->offStart);
            if (offWrite >= pRingBuf->offEnd)
                offWrite = pRingBuf->offStart;
            Log2(("WriteFrame: offWrite: %#x -> %#x (1)\n", pRingBuf->offWrite, offWrite));
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            return VINF_SUCCESS;
        }

        /*
         * Try fit the frame at the start of the buffer.
         * (The header fits before the end of the buffer because of alignment.)
         */
        AssertMsg(pRingBuf->offEnd - offWrite >= sizeof(INTNETHDR), ("offEnd=%x offWrite=%x\n", pRingBuf->offEnd, offWrite));
        if (offRead - pRingBuf->offStart > cb) /* not >= ! */
        {
            PINTNETHDR  pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
            void       *pvFrameOut = (PINTNETHDR)((uint8_t *)pBuf + pRingBuf->offStart);
            pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
            pHdr->cbFrame  = cbFrame;
            pHdr->offFrame = (intptr_t)pvFrameOut - (intptr_t)pHdr;

            vboxnetudpCopySG(pvFrameOut, cSegs, paSegs);

            offWrite = pRingBuf->offStart + cb;
            ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
            Log2(("WriteFrame: offWrite: %#x -> %#x (2)\n", pRingBuf->offWrite, offWrite));
            return VINF_SUCCESS;
        }
    }
    /*
     * The reader is ahead of the writer, try fit it into that space.
     */
    else if (offRead - offWrite > cb + sizeof(INTNETHDR)) /* not >= ! */
    {
        PINTNETHDR pHdr = (PINTNETHDR)((uint8_t *)pBuf + offWrite);
        pHdr->u16Type  = INTNETHDR_TYPE_FRAME;
        pHdr->cbFrame  = cbFrame;
        pHdr->offFrame = sizeof(INTNETHDR);

        vboxnetudpCopySG(pHdr + 1, cSegs, paSegs);

        offWrite += cb + sizeof(INTNETHDR);
        ASMAtomicXchgU32(&pRingBuf->offWrite, offWrite);
        Log2(("WriteFrame: offWrite: %#x -> %#x (3)\n", pRingBuf->offWrite, offWrite));
        return VINF_SUCCESS;
    }

    /* (it didn't fit) */
    /** @todo stats */
    return VERR_BUFFER_OVERFLOW;
}


/** Internal worker for VBoxNetUDPUnicast and VBoxNetUDPBroadcast. */
static int vboxnetudpSend(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                          RTNETADDRIPV4 SrcIPv4Addr, PCRTMAC pSrcMacAddr, unsigned uSrcPort,
                          RTNETADDRIPV4 DstIPv4Addr, PCRTMAC pDstMacAddr, unsigned uDstPort,
                          void const *pvData, size_t cbData)
{
    INTNETSEG aSegs[4];

    /* the Ethernet header */
    RTNETETHERHDR EtherHdr;
    EtherHdr.DstMac     = *pDstMacAddr;
    EtherHdr.SrcMac     = *pSrcMacAddr;
    EtherHdr.EtherType  = RT_H2BE_U16_C(RTNET_ETHERTYPE_IPV4);

    aSegs[0].pv   = &EtherHdr;
    aSegs[0].cb   = sizeof(EtherHdr);
    aSegs[0].Phys = NIL_RTHCPHYS;

    /* the IP header */
    RTNETIPV4 IpHdr;
    IpHdr.ip_v          = 4;
    IpHdr.ip_hl         = sizeof(RTNETIPV4) / sizeof(uint32_t);
    IpHdr.ip_tos        = 0;
    IpHdr.ip_len        = RT_H2BE_U16(cbData + sizeof(RTNETUDP) + sizeof(RTNETIPV4));
    IpHdr.ip_id         = (uint16_t)RTRandU32();
    IpHdr.ip_off        = 0;
    IpHdr.ip_ttl        = 255;
    IpHdr.ip_p          = RTNETIPV4_PROT_UDP;
    IpHdr.ip_sum        = 0;
    IpHdr.ip_src.u      = 0;
    IpHdr.ip_dst.u      = UINT32_C(0xffffffff); /* broadcast */
    IpHdr.ip_sum        = RTNetIPv4HdrChecksum(&IpHdr);

    aSegs[1].pv   = &IpHdr;
    aSegs[1].cb   = sizeof(IpHdr);
    aSegs[1].Phys = NIL_RTHCPHYS;


    /* the UDP bit */
    RTNETUDP UdpHdr;
    UdpHdr.uh_sport     = RT_H2BE_U16(uSrcPort);
    UdpHdr.uh_dport     = RT_H2BE_U16(uDstPort);
    UdpHdr.uh_ulen      = RT_H2BE_U16(cbData + sizeof(RTNETUDP));
#if 0
    UdpHdr.uh_sum       = 0; /* pretend checksumming is disabled */
#else
    UdpHdr.uh_sum       = RTNetIPv4UDPChecksum(&IpHdr, &UdpHdr, pvData);
#endif

    aSegs[2].pv   = &UdpHdr;
    aSegs[2].cb   = sizeof(UdpHdr);
    aSegs[2].Phys = NIL_RTHCPHYS;

    /* the payload */
    aSegs[3].pv   = (void *)pvData;
    aSegs[3].cb   = cbData;
    aSegs[3].Phys = NIL_RTHCPHYS;


    /* write it */
    int rc = vboxnetudpRingWriteFrame(pBuf, &pBuf->Send, RT_ELEMENTS(aSegs), &aSegs[0]);
    int rc2 = vboxnetudpFlush(pSession, hIf);
    if (RT_FAILURE(rc))
    {
        rc = vboxnetudpRingWriteFrame(pBuf, &pBuf->Send, RT_ELEMENTS(aSegs), &aSegs[0]);
        rc2 = vboxnetudpFlush(pSession, hIf);
    }

    if (RT_SUCCESS(rc))
        rc = rc2;
    return rc;
}


/**
 * Sends an unicast UDP packet.
 *
 * @returns VBox status code.
 * @param   pSession        The support driver session handle.
 * @param   hIf             The interface handle.
 * @param   pBuf            The interface buffer.
 * @param   SrcIPv4Addr     The source IPv4 address.
 * @param   pSrcMacAddr     The source MAC address.
 * @param   uSrcPort        The source port number.
 * @param   DstIPv4Addr     The destination IPv4 address. Can be broadcast.
 * @param   pDstMacAddr     The destination MAC address.
 * @param   uDstPort        The destination port number.
 * @param   pvData          The data payload.
 * @param   cbData          The size of the data payload.
 */
int     VBoxNetUDPUnicast(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                          RTNETADDRIPV4 SrcIPv4Addr, PCRTMAC pSrcMacAddr, unsigned uSrcPort,
                          RTNETADDRIPV4 DstIPv4Addr, PCRTMAC pDstMacAddr, unsigned uDstPort,
                          void const *pvData, size_t cbData)
{
    return vboxnetudpSend(pSession, hIf, pBuf,
                          SrcIPv4Addr, pSrcMacAddr, uSrcPort,
                          DstIPv4Addr, pDstMacAddr, uDstPort,
                          pvData, cbData);
}


/**
 * Sends a broadcast UDP packet.
 *
 * @returns VBox status code.
 * @param   pSession        The support driver session handle.
 * @param   hIf             The interface handle.
 * @param   pBuf            The interface buffer.
 * @param   SrcIPv4Addr     The source IPv4 address.
 * @param   pSrcMacAddr     The source MAC address.
 * @param   uSrcPort        The source port number.
 * @param   uDstPort        The destination port number.
 * @param   pvData          The data payload.
 * @param   cbData          The size of the data payload.
 */
int     VBoxNetUDPBroadcast(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                            RTNETADDRIPV4 SrcIPv4Addr, PCRTMAC pSrcMacAddr, unsigned uSrcPort,
                            unsigned uDstPort,
                            void const *pvData, size_t cbData)
{
    RTNETADDRIPV4   IPv4AddrBrdCast;
    IPv4AddrBrdCast.u = UINT32_C(0xffffffff);
    RTMAC           MacBrdCast;
    MacBrdCast.au16[0] = MacBrdCast.au16[1] = MacBrdCast.au16[2] = UINT16_C(0xffff);

    return vboxnetudpSend(pSession, hIf, pBuf,
                          SrcIPv4Addr, pSrcMacAddr, uSrcPort,
                          IPv4AddrBrdCast, &MacBrdCast, uDstPort,
                          pvData, cbData);
}

