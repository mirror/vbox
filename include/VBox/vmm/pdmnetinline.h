/** @file
 * PDM - Networking Helpers, Inlined Code. (DEV,++)
 *
 * This is all inlined because it's too tedious to create 2-3 libraries to
 * contain it all (same bad excuse as for intnetinline.h).
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 * Checksum type.
 */
typedef enum PDMNETCSUMTYPE
{
    /** No checksum. */
    PDMNETCSUMTYPE_NONE = 0,
    /** Normal TCP checksum. */
    PDMNETCSUMTYPE_COMPLETE,
    /** Checksum on pseudo header (used with GSO). */
    PDMNETCSUMTYPE_PSEUDO,
    /** The usual 32-bit hack. */
    PDMNETCSUMTYPE_32_BIT_HACK = 0x7fffffff
} PDMNETCSUMTYPE;


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
    PDMNETWORKGSOTYPE enmType;

    if (RT_UNLIKELY(cbGsoMax < sizeof(*pGso)))
        return false;

    enmType = (PDMNETWORKGSOTYPE)pGso->u8Type;
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
 * Calculates the number of segments a GSO frame will be segmented into.
 *
 * @returns Segment count.
 * @param   pGso                The GSO context.
 * @param   cbFrame             The GSO frame size (header proto + payload).
 */
DECLINLINE(uint32_t) PDMNetGsoCalcSegmentCount(PCPDMNETWORKGSO pGso, size_t cbFrame)
{
    size_t cbPayload;
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));
    cbPayload = cbFrame - pGso->cbHdrs;
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
 * @param   u32PseudoSum        The pseudo checksum.
 * @param   pbSegHdrs           Pointer to the header bytes / frame start.
 * @param   offUdpHdr           The offset into @a pbSegHdrs of the UDP header.
 * @param   pbPayload           Pointer to the payload bytes.
 * @param   cbPayload           The amount of payload.
 * @param   cbHdrs              The size of all the headers.
 * @param   enmCsumType         Whether to checksum the payload, the pseudo
 *                              header or nothing.
 * @internal
 */
DECLINLINE(void) pdmNetGsoUpdateUdpHdr(uint32_t u32PseudoSum, uint8_t *pbSegHdrs, uint8_t offUdpHdr,
                                       uint8_t const *pbPayload, uint32_t cbPayload, uint8_t cbHdrs,
                                       PDMNETCSUMTYPE enmCsumType)
{
    PRTNETUDP pUdpHdr = (PRTNETUDP)&pbSegHdrs[offUdpHdr];
    pUdpHdr->uh_ulen = cbPayload + cbHdrs - offUdpHdr;
    switch (enmCsumType)
    {
        case PDMNETCSUMTYPE_NONE:
            pUdpHdr->uh_sum = 0;
            break;
        case PDMNETCSUMTYPE_COMPLETE:
            pUdpHdr->uh_sum = RTNetUDPChecksum(u32PseudoSum, pUdpHdr);
            break;
        case PDMNETCSUMTYPE_PSEUDO:
            pUdpHdr->uh_sum = ~RTNetIPv4FinalizeChecksum(u32PseudoSum);
            break;
        default:
            AssertFailed();
            break;
    }
}


/**
 * Update a TCP header after carving out a segment.
 *
 * @param   u32PseudoSum        The pseudo checksum.
 * @param   pbSegHdrs           Pointer to the header bytes / frame start.
 * @param   offTcpHdr           The offset into @a pbSegHdrs of the TCP header.
 * @param   pbPayload           Pointer to the payload bytes.
 * @param   cbPayload           The amount of payload.
 * @param   offPayload          The offset into the payload that we're splitting
 *                              up.  We're ASSUMING that the payload follows
 *                              immediately after the TCP header w/ options.
 * @param   cbHdrs              The size of all the headers.
 * @param   fLastSeg            Set if this is the last segment.
 * @param   enmCsumType         Whether to checksum the payload, the pseudo
 *                              header or nothing.
 * @internal
 */
DECLINLINE(void) pdmNetGsoUpdateTcpHdr(uint32_t u32PseudoSum, uint8_t *pbSegHdrs, uint8_t offTcpHdr,
                                       uint8_t const *pbPayload, uint32_t cbPayload, uint32_t offPayload, uint8_t cbHdrs,
                                       bool fLastSeg, PDMNETCSUMTYPE enmCsumType)
{
    PRTNETTCP pTcpHdr = (PRTNETTCP)&pbSegHdrs[offTcpHdr];
    pTcpHdr->th_seq = RT_H2N_U32(RT_N2H_U32(pTcpHdr->th_seq) + offPayload);
    if (!fLastSeg)
        pTcpHdr->th_flags &= ~(RTNETTCP_F_FIN | RTNETTCP_F_PSH);
    switch (enmCsumType)
    {
        case PDMNETCSUMTYPE_NONE:
            pTcpHdr->th_sum = 0;
            break;
        case PDMNETCSUMTYPE_COMPLETE:
            pTcpHdr->th_sum = RTNetTCPChecksum(u32PseudoSum, pTcpHdr, pbPayload, cbPayload);
            break;
        case PDMNETCSUMTYPE_PSEUDO:
            pTcpHdr->th_sum = ~RTNetIPv4FinalizeChecksum(u32PseudoSum);
            break;
        default:
            AssertFailed();
            break;
    }
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
    pIpHdr->ip_sum    = RTNetIPv4HdrChecksum(pIpHdr);
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
     * Check assumptions (doing it after declaring the variables because of C).
     */
    Assert(iSeg < cSegs);
    Assert(cSegs == PDMNetGsoCalcSegmentCount(pGso, cbFrame));
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));

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
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            /* no default! wnat gcc warnings. */
            break;
    }

    *pcbSegFrame = cbSegFrame;
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
 * @param   pbFrame             Pointer to the GSO frame.  Used for retrieving
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
     * Figure out where the payload is and where the header starts before we
     * do the protocol specific carving.
     */
    uint8_t const * const pbSegPayload = pbFrame + pGso->cbHdrs + iSeg * pGso->cbMaxSeg;
    uint32_t const        cbSegPayload = iSeg + 1 != cSegs
                                       ? pGso->cbMaxSeg
                                       : (uint32_t)(cbFrame - iSeg * pGso->cbMaxSeg - pGso->cbHdrs);

    /*
     * Check assumptions (doing it after declaring the variables because of C).
     */
    Assert(iSeg < cSegs);
    Assert(cSegs == PDMNetGsoCalcSegmentCount(pGso, cbFrame));
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));

    /*
     * Copy the header and do the protocol specific massaging of it.
     */
    memcpy(pbSegHdrs, pbFrame, pGso->cbHdrs);

    switch ((PDMNETWORKGSOTYPE)pGso->u8Type)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, iSeg * pGso->cbMaxSeg,
                                  pGso->cbHdrs, iSeg + 1 == cSegs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            pdmNetGsoUpdateIPv4Hdr(pbSegHdrs, pGso->offHdr1, cbSegPayload, iSeg, pGso->cbHdrs);
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbSegHdrs, pgmNetGsoCalcIpv6Offset(pbSegHdrs, pGso->offHdr1),
                                                         cbSegPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbSegHdrs, pGso->offHdr2, pbSegPayload, cbSegPayload, pGso->cbHdrs, PDMNETCSUMTYPE_COMPLETE);
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            /* no default! wnat gcc warnings. */
            break;
    }

    *pcbSegPayload = cbSegPayload;
    return pGso->cbHdrs + iSeg * pGso->cbMaxSeg;
}


/**
 * Prepares the GSO frame for direct use without any segmenting.
 *
 * @param   pGso                The GSO context.
 * @param   pvFrame             The frame to prepare.
 * @param   cbFrame             The frame size.
 * @param   enmCsumType         Whether to checksum the payload, the pseudo
 *                              header or nothing.
 */
DECLINLINE(void) PDMNetGsoPrepForDirectUse(PCPDMNETWORKGSO pGso, void *pvFrame, size_t cbFrame, PDMNETCSUMTYPE enmCsumType)
{
    /*
     * Figure out where the payload is and where the header starts before we
     * do the protocol bits.
     */
    uint8_t * const pbHdrs    = (uint8_t *)pvFrame;
    uint8_t * const pbPayload = pbHdrs  + pGso->cbHdrs;
    uint32_t  const cbFrame32 = (uint32_t)cbFrame;
    uint32_t  const cbPayload = cbFrame32 - pGso->cbHdrs;

    /*
     * Check assumptions (doing it after declaring the variables because of C).
     */
    Assert(PDMNetGsoIsValid(pGso, sizeof(*pGso), cbFrame));

    /*
     * Get down to busienss.
     */
    switch ((PDMNETWORKGSOTYPE)pGso->u8Type)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv4Hdr(pbHdrs, pGso->offHdr1, cbFrame32 - pGso->cbHdrs, 0, pGso->cbHdrs),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, 0, pGso->cbHdrs, true, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_IPV4_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv4Hdr(pbHdrs, pGso->offHdr1, cbFrame32 - pGso->cbHdrs, 0, pGso->cbHdrs),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, pGso->cbHdrs, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_IPV6_TCP:
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbHdrs, pGso->offHdr1, cbPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, 0, pGso->cbHdrs, true, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_IPV6_UDP:
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbHdrs, pGso->offHdr1, cbPayload, pGso->cbHdrs,
                                                         pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, pGso->cbHdrs, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:
            pdmNetGsoUpdateIPv4Hdr(pbHdrs, pGso->offHdr1, cbPayload, 0, pGso->cbHdrs);
            pdmNetGsoUpdateTcpHdr(pdmNetGsoUpdateIPv6Hdr(pbHdrs, pgmNetGsoCalcIpv6Offset(pbHdrs, pGso->offHdr1),
                                                         cbPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_TCP),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, 0, pGso->cbHdrs, true, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:
            pdmNetGsoUpdateIPv4Hdr(pbHdrs, pGso->offHdr1, cbPayload, 0, pGso->cbHdrs);
            pdmNetGsoUpdateUdpHdr(pdmNetGsoUpdateIPv6Hdr(pbHdrs, pgmNetGsoCalcIpv6Offset(pbHdrs, pGso->offHdr1),
                                                         cbPayload, pGso->cbHdrs, pGso->offHdr2, RTNETIPV4_PROT_UDP),
                                  pbHdrs, pGso->offHdr2, pbPayload, cbPayload, pGso->cbHdrs, enmCsumType);
            break;
        case PDMNETWORKGSOTYPE_INVALID:
        case PDMNETWORKGSOTYPE_END:
            /* no default! wnat gcc warnings. */
            break;
    }
}


/**
 * Gets the GSO type name string.
 *
 * @returns Pointer to read only name string.
 * @param   enmType             The type.
 */
DECLINLINE(const char *) PDMNetGsoTypeName(PDMNETWORKGSOTYPE enmType)
{
    switch (enmType)
    {
        case PDMNETWORKGSOTYPE_IPV4_TCP:        return "TCPv4";
        case PDMNETWORKGSOTYPE_IPV6_TCP:        return "TCPv6";
        case PDMNETWORKGSOTYPE_IPV4_UDP:        return "UDPv4";
        case PDMNETWORKGSOTYPE_IPV6_UDP:        return "UDPv6";
        case PDMNETWORKGSOTYPE_IPV4_IPV6_TCP:   return "4to6TCP";
        case PDMNETWORKGSOTYPE_IPV4_IPV6_UDP:   return "4to6UDP";
        case PDMNETWORKGSOTYPE_INVALID:         return "invalid";
        case PDMNETWORKGSOTYPE_END:             return "end";
    }
    return "bad-gso-type";
}

