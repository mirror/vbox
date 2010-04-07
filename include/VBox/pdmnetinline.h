/* $Id$ */
/** @file
 * PDM - Networking Helpers, Inlined Code. (DEV,++)
 *
 * This is all inlined because it's too tedious to create 2-3 libraries to
 * contain it all (same bad excuse as for intnetinline.h).  C++ only because of
 * mixed code and variables.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <VBox/log.h>
#include <VBox/types.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/net.h>
#include <iprt/string.h>



/**
 * Validates the GSO context.
 *
 * @returns true if valid, false if not (not asserted or logged).
 * @param   pGso                The GSO context.
 * @param   cbGsoMax            The max size of the GSO context.
 * @param   cbFrame             The max size of the GSO frame (use to validate
 *                              the MSS).
 */
DECLINLINE(bool) PDMNetGsoIsValid(PCPDMNETWORKGSO pGso, size_t cbGsoMax, size_t cbFrame)
{
    if (RT_UNLIKELY(cbGsoMax < sizeof(*pGso)))
        return false;

    PDMNETWORKGSOTYPE const enmType = (PDMNETWORKGSOTYPE)pGso->u8Type;
    if (RT_UNLIKELY( enmType <= PDMNETWORKGSOTYPE_INVALID || enmType >= PDMNETWORKGSOTYPE_END ))
        return false;

    /* all types requires both headers. */
    if (RT_UNLIKELY( pGso->offHdr1 < sizeof(RTNETETHERHDR) ))
        return false;
    if (RT_UNLIKELY( pGso->offHdr2 <= pGso->offHdr1 ))
        return false;
    if (RT_UNLIKELY( pGso->cbHdrs  <= pGso->offHdr2 ))
        return false;

    /* min size of the 1st header(s). */
    switch (enmType)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            if (RT_UNLIKELY( (unsigned)pGso->offHdr2 - pGso->offHdr1 < RTNETIPV4_MIN_LEN ))
                return false;
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            if (RT_UNLIKELY( (unsigned)pGso->offHdr2 - pGso->offHdr1 < RTNETIPV6_MIN_LEN ))
                return false;
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            if (RT_UNLIKELY( (unsigned)pGso->offHdr2 - pGso->offHdr1 < RTNETIPV4_MIN_LEN + RTNETIPV6_MIN_LEN ))
                return false;
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            break;
        /* no default case! want gcc warnings. */
    }

    /* min size of the 2nd header. */
    switch (enmType)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
        case PDMNETWORKGSOTYPE_IPV6_TCP:
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            if (RT_UNLIKELY( (unsigned)pGso->cbHdrs - pGso->offHdr2 < RTNETTCP_MIN_LEN ))
                return false;
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
        case PDMNETWORKGSOTYPE_IPV6_UDP:
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            if (RT_UNLIKELY( (unsigned)pGso->cbHdrs - pGso->offHdr2 < RTNETUDP_MIN_LEN ))
                return false;
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            break;
        /* no default case! want gcc warnings. */
    }

    /* There must be at more than one segment. */
    if (RT_UNLIKELY( cbFrame <= pGso->cbHdrs ))
        return false;
    if (RT_UNLIKELY( cbFrame - pGso->cbHdrs < pGso->cbMaxSeg ))
        return false;

    return true;
}


/**
 * Calculates the number of segements a GSO frame will be segmented into.
 *
 * @returns Segment count.
 * @param   pGso                The GSO context.
 * @param   cbFrame             The GSO frame size (header proto + payload).
 */
DECLINLINE(uint32_t) PDMNetGsoCalcSegmentCount(PCPDMNETWORKGSO pGso, size_t cbFrame)
{
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));
    size_t cbPayload = cbFrame - pGso->cbHdrs;
    return (uint32_t)((cbPayload + pGso->cbMaxSeg - 1) / pGso->cbMaxSeg);
}


/**
 * Used to find the IPv6 header when handling 4to6 tunneling.
 *
 * @returns Offset of the IPv6 header.
 * @param   pbSegHdrs           The headers / frame start.
 * @param   offIpHdr            The offset of the IPv4 header.
 */
DECLINLINE(uint8_t) pgmNetGsoCalcIpv6Offset(uint8_t *pbSegHdrs, uint8_t offIPv4Hdr)
{
    PCRTNETIPV4 pIPv4Hdr = (PCRTNETIPV4)&pbSegHdrs[offIPv4Hdr];
    return offIPv4Hdr + pIPv4Hdr->ip_hl * 4;
}


/**
 * Update an UDP header after carving out a segment
 *
 * @param   u32PsudoSum         The pseudo checksum.
 * @param   pbSegHdrs           Pointer to the header bytes / frame start.
 * @param   offUdpHdr           The offset into @a pbSegHdrs of the UDP header.
 * @param   pbPayload           Pointer to the payload bytes.
 * @param   cbPayload           The amount of payload.
 * @param   cbHdrs              The size of all the headers.
 * @internal
 */
DECLINLINE(void) pdmNetGsoUpdateUdpHdr(uint32_t u32PsudoSum, uint8_t *pbSegHdrs, uint8_t offUdpHdr,
                                       uint8_t const *pbPayload, uint32_t cbPayload, uint8_t cbHdrs)
{
    PRTNETUDP pUdpHdr = (PRTNETUDP)&pbSegHdrs[offUdpHdr];
    pUdpHdr->uh_ulen = cbPayload + cbHdrs - offUdpHdr;
    pUdpHdr->uh_sum  = RTNetUDPChecksum(u32PsudoSum, pUdpHdr);
}


/**
 * Update a TCP header after carving out a segment.
 *
 * @param   u32PsudoSum         The pseudo checksum.
 * @param   pbSegHdrs           Pointer to the header bytes / frame start.
 * @param   offTcpHdr           The offset into @a pbSegHdrs of the TCP header.
 * @param   pbPayload           Pointer to the payload bytes.
 * @param   cbPayload           The amount of payload.
 * @param   offPayload          The offset into the payload that we're splitting
 *                              up.  We're ASSUMING that the payload follows
 *                              immediately after the TCP header w/ options.
 * @param   cbHdrs              The size of all the headers.
 * @param   fLastSeg            Set if this is the last segment.
 * @internal
 */
DECLINLINE(void) pdmNetGsoUpdateTcpHdr(uint32_t u32PsudoSum, uint8_t *pbSegHdrs, uint8_t offTcpHdr,
                                       uint8_t const *pbPayload, uint32_t cbPayload, uint32_t offPayload, uint8_t cbHdrs, bool fLastSeg)
{
    PRTNETTCP pTcpHdr = (PRTNETTCP)&pbSegHdrs[offTcpHdr];
    pTcpHdr->th_seq = RT_H2N_U32(RT_N2H_U32(pTcpHdr->th_seq) + offPayload);
    if (!fLastSeg)
        pTcpHdr->th_flags &= ~(RTNETTCP_F_FIN | RTNETTCP_F_PSH);
    pTcpHdr->th_sum = RTNetTCPChecksum(u32PsudoSum, pTcpHdr, pbPayload, cbPayload);
}


/**
 * Updates a IPv6 header after carving out a segment.
 *
 * @returns 32-bit intermediary checksum value for the pseudo header.
 * @param   pbSegHdrs           Pointer to the header bytes.
 * @param   offIpHdr            The offset into @a pbSegHdrs of the IP header.
 * @param   cbSegPayload        The amount of segmented payload.  Not to be
 *                              confused with the IP payload.
 * @param   cbHdrs              The size of all the headers.
 * @param   offPktHdr           Offset of the protocol packet header.  For the
 *                              pseudo header checksum calulation.
 * @param   bProtocol           The protocol type.  For the pseudo header.
 * @internal
 */
DECLINLINE(uint32_t) pdmNetGsoUpdateIPv6Hdr(uint8_t *pbSegHdrs, uint8_t offIpHdr, uint32_t cbSegPayload, uint8_t cbHdrs,
                                            uint8_t offPktHdr, uint8_t bProtocol)
{
    PRTNETIPV6 pIpHdr  = (PRTNETIPV6)&pbSegHdrs[offIpHdr];
    uint16_t cbPayload = (uint16_t)(cbHdrs - (offIpHdr + sizeof(RTNETIPV6)) + cbSegPayload);
    pIpHdr->ip6_plen   = RT_H2N_U16(cbPayload);
    return RTNetIPv6PseudoChecksumEx(pIpHdr, bProtocol, (uint16_t)(cbHdrs - offPktHdr + cbSegPayload));
}


/**
 * Updates a IPv4 header after carving out a segment.
 *
 * @returns 32-bit intermediary checksum value for the pseudo header.
 * @param   pbSegHdrs           Pointer to the header bytes.
 * @param   offIpHdr            The offset into @a pbSegHdrs of the IP header.
 * @param   cbSegPayload        The amount of segmented payload.
 * @param   iSeg                The segment index.
 * @param   cbHdrs              The size of all the headers.
 * @internal
 */
DECLINLINE(uint32_t) pdmNetGsoUpdateIPv4Hdr(uint8_t *pbSegHdrs, uint8_t offIpHdr, uint32_t cbSegPayload,
                                            uint32_t iSeg, uint8_t cbHdrs)
{
    PRTNETIPV4 pIpHdr = (PRTNETIPV4)&pbSegHdrs[offIpHdr];
    pIpHdr->ip_len    = RT_H2N_U16(cbHdrs - offIpHdr + cbSegPayload);
    pIpHdr->ip_id     = RT_H2N_U16(RT_N2H_U16(pIpHdr->ip_id) + iSeg);
    pIpHdr->ip_sum    = RT_H2N_U16(RTNetIPv4HdrChecksum(pIpHdr));
    return RTNetIPv4PseudoChecksum(pIpHdr);
}


/**
 * Carves out the specified segment in a destructive manner.
 *
 * This is for sequentially carving out segments and pushing them along for
 * processing or sending.  To avoid allocating a temporary buffer for
 * constructing the segment in, we trash the previous frame by putting the
 * header at the end of it.
 *
 * @returns Pointer to the segment frame that we've carved out.
 * @param   pGso                The GSO context data.
 * @param   pbFrame             Pointer to the GSO frame.
 * @param   cbFrame             The size of the GSO frame.
 * @param   pbHdrScatch         Pointer to a pGso->cbHdrs sized area where we
 *                              can save the original header prototypes on the
 *                              first call (@a iSeg is 0) and retrieve it on
 *                              susequent calls. (Just use a 256 bytes
 *                              buffer to make life easy.)
 * @param   iSeg                The segment that we're carving out (0-based).
 * @param   cSegs               The number of segments in the GSO frame.  Use
 *                              PDMNetGsoCalcSegmentCount to find this.
 * @param   pcbSegFrame         Where to return the size of the returned segment
 *                              frame.
 */
DECLINLINE(void *) PDMNetGsoCarveSegmentQD(PCPDMNETWORKGSO pGso, uint8_t *pbFrame, size_t cbFrame, uint8_t *pbHdrScatch,
                                           uint32_t iSeg, uint32_t cSegs, uint32_t *pcbSegFrame)
{
    /*
     * Check assumptions.
     */
    Assert(iSeg < cSegs);
    Assert(cSegs == PDMNetGsoCalcSegmentCount(pGso, cbFrame));
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));

    /*
     * Figure out where the payload is and where the header starts before we
     * do the protocol specific carving.
     */
    uint8_t * const pbSegHdrs    = pbFrame + pGso->cbMaxSeg * iSeg;
    uint8_t * const pbSegPayload = pbSegHdrs + pGso->cbHdrs;
    uint32_t const  cbSegPayload = iSeg + 1 != cSegs
                                 ? pGso->cbMaxSeg
                                 : (uint32_t)(cbFrame - iSeg * pGso->cbMaxSeg - pGso->cbHdrs);
    uint32_t const  cbSegFrame   = cbSegPayload + pGso->cbHdrs;

    /*
     * Copy the header and do the protocol specific massaging of it.
     */
    if (iSeg != 0)
        memcpy(pbSegHdrs, pbHdrScatch, pGso->cbHdrs);
    else
        memcpy(pbHdrScatch, pbSegHdrs, pGso->cbHdrs);

    switch ((PDMNETWORKGSOTYPE)pGso->u8Type)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            /* no default! wnat gcc warnings. */
            break;
    }

    *pcbSegFrame = cbSegPayload + pGso->cbHdrs;
    return pbSegHdrs;
}


/**
 * Carves out the specified segment in a non-destructive manner.
 *
 * The segment headers and segment payload is kept separate here.  The GSO frame
 * is still expected to be one linear chunk of data, but we don't modify any of
 * it.
 *
 * @returns The offset into the GSO frame of the payload.
 * @param   pGso                The GSO context data.
 * @param   pbFrame             Pointer to the GSO frame.  Used for retriving
 *                              the header prototype and for checksumming the
 *                              payload.  The buffer is not modified.
 * @param   cbFrame             The size of the GSO frame.
 * @param   iSeg                The segment that we're carving out (0-based).
 * @param   cSegs               The number of segments in the GSO frame.  Use
 *                              PDMNetGsoCalcSegmentCount to find this.
 * @param   pbSegHdrs           Where to return the headers for the segment
 *                              that's been carved out.  The buffer must be at
 *                              least pGso->cbHdrs in size, using a 256 byte
 *                              buffer is a recommended simplification.
 * @param   pcbSegPayload       Where to return the size of the returned
 *                              segment payload.
 */
DECLINLINE(uint32_t) PDMNetGsoCarveSegment(PCPDMNETWORKGSO pGso, const uint8_t *pbFrame, size_t cbFrame,
                                           uint32_t iSeg, uint32_t cSegs, uint8_t *pbSegHdrs, uint32_t *pcbSegPayload)
{
    /*
     * Check assumptions.
     */
    Assert(iSeg < cSegs);
    Assert(cSegs == PDMNetGsoCalcSegmentCount(pGso, cbFrame));
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));

    /*
     * Figure out where the payload is and where the header starts before we
     * do the protocol specific carving.
     */
    uint8_t const * const pbSegPayload = pbFrame + pGso->cbHdrs + iSeg * pGso->cbMaxSeg;
    uint32_t const        cbSegPayload = iSeg + 1 != cSegs
                                       ? pGso->cbMaxSeg
                                       : (uint32_t)(cbFrame - iSeg * pGso->cbMaxSeg - pGso->cbHdrs);

    /*
     * Copy the header and do the protocol specific massaging of it.
     */
    memcpy(pbSegHdrs, pbFrame, pGso->cbHdrs);

    switch ((PDMNETWORKGSOTYPE)pGso->u8Type)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs);
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            /* no default! wnat gcc warnings. */
            break;
    }

    *pcbSegPayload = cbSegPayload;
    return pGso->cbHdrs + iSeg * pGso->cbMaxSeg;
}




DECLINLINE(void) PDMNetGsoPrepForDirectUse(PCPDMNETWORKGSO pGso, void *pvFrame, size_t cbFrame,
                                           bool fHeaderChecskum, bool fPayloadChecksum)
{
/** @todo Need to implement this eventually, not important yet as it's only use
 *        by DHCP where GSO isn't normally used. */
}



