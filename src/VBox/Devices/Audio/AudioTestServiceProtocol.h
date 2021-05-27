/* $Id$ */
/** @file
 * AudioTestServiceProtocol - Audio test execution server, Protocol Header.
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
 */

#ifndef VBOX_INCLUDED_SRC_Audio_AudioTestServiceProtocol_h
#define VBOX_INCLUDED_SRC_Audio_AudioTestServiceProtocol_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/list.h>

#include <VBox/vmm/pdmaudioifs.h>

#include "AudioTest.h"

RT_C_DECLS_BEGIN

/**
 * Common Packet header (for requests and replies).
 */
typedef struct ATSPKTHDR
{
    /** The unpadded packet length. This include this header. */
    uint32_t        cb;
    /** The CRC-32 for the packet starting from the opcode field.  0 if the packet
     *  hasn't been CRCed. */
    uint32_t        uCrc32;
    /** Packet opcode, an unterminated ASCII string.  */
    uint8_t         achOpcode[8];
} ATSPKTHDR;
AssertCompileSize(ATSPKTHDR, 16);
/** Pointer to a packet header. */
typedef ATSPKTHDR *PATSPKTHDR;
/** Pointer to a packet header. */
typedef ATSPKTHDR const *PCATSPKTHDR;
/** Pointer to a packet header pointer. */
typedef PATSPKTHDR *PPATSPKTHDR;

/** Packet alignment. */
#define ATSPKT_ALIGNMENT                16
/** Max packet size. */
#define ATSPKT_MAX_SIZE                 _256K

/**
 * Status packet.
 */
typedef struct ATSPKTSTS
{
    /** Embedded common packet header. */
    ATSPKTHDR       Hdr;
    /** The IPRT status code of the request. */
    int32_t         rcReq;
    /** Size of the optional status message following this structure -
     * only for errors. */
    uint32_t        cchStsMsg;
    /** Padding - reserved. */
    uint8_t         au8Padding[8];
} ATSPKTSTS;
AssertCompileSizeAlignment(ATSPKTSTS, ATSPKT_ALIGNMENT);
/** Pointer to a status packet header. */
typedef ATSPKTSTS *PATSPKTSTS;

#define ATSPKT_OPCODE_HOWDY             "HOWDY   "

/** 32bit protocol version consisting of a 16bit major and 16bit minor part. */
#define ATS_PROTOCOL_VS (ATS_PROTOCOL_VS_MAJOR | ATS_PROTOCOL_VS_MINOR)
/** The major version part of the protocol version. */
#define ATS_PROTOCOL_VS_MAJOR (1 << 16)
/** The minor version part of the protocol version. */
#define ATS_PROTOCOL_VS_MINOR (0)

/**
 * The HOWDY request structure.
 */
typedef struct ATSPKTREQHOWDY
{
    /** Embedded packet header. */
    ATSPKTHDR       Hdr;
    /** Version of the protocol the client wants to use. */
    uint32_t        uVersion;
    /** Alignment. */
    uint8_t         au8Padding[12];
} ATSPKTREQHOWDY;
AssertCompileSizeAlignment(ATSPKTREQHOWDY, ATSPKT_ALIGNMENT);
/** Pointer to a HOWDY request structure. */
typedef ATSPKTREQHOWDY *PATSPKTREQHOWDY;

/**
 * The HOWDY reply structure.
 */
typedef struct ATSPKTREPHOWDY
{
    /** Status packet. */
    ATSPKTSTS       Sts;
    /** Version to use for the established connection. */
    uint32_t        uVersion;
    /** Padding - reserved. */
    uint8_t         au8Padding[12];
} ATSPKTREPHOWDY;
AssertCompileSizeAlignment(ATSPKTREPHOWDY, ATSPKT_ALIGNMENT);
/** Pointer to a HOWDY reply structure. */
typedef ATSPKTREPHOWDY *PATSPKTREPHOWDY;

#define ATSPKT_OPCODE_BYE               "BYE     "

/* No additional structures for BYE. */

#define ATSPKT_OPCODE_TONE_PLAY         "TNPLY   "

/**
 * The TONE PLAY request structure.
 */
typedef struct ATSPKTREQTONEPLAY
{
    /** Embedded packet header. */
    ATSPKTHDR          Hdr;
    /** Stream configuration to use for playing the tone.
     *  Note: Depending on the actual implementation this configuration might or might not be available / supported. */
    PDMAUDIOSTREAMCFG  StreamCfg;
    /** Test tone parameters for playback. */
    AUDIOTESTTONEPARMS ToneParms;
#if HC_ARCH_BITS == 64
    uint8_t            aPadding[4];
#else
    uint8_t            aPadding[6];
#endif
} ATSPKTREQTONEPLAY;
AssertCompileSizeAlignment(ATSPKTREQTONEPLAY, ATSPKT_ALIGNMENT);
/** Pointer to a ATSPKTREQTONEPLAY structure. */
typedef ATSPKTREQTONEPLAY *PATSPKTREQTONEPLAY;

/* No additional structure for the reply (just standard STATUS packet). */

/**
 * Checks if the two opcodes match.
 *
 * @returns true on match, false on mismatch.
 * @param   pPktHdr             The packet header.
 * @param   pszOpcode2          The opcode we're comparing with.  Does not have
 *                              to be the whole 8 chars long.
 */
DECLINLINE(bool) atsIsSameOpcode(PCATSPKTHDR pPktHdr, const char *pszOpcode2)
{
    if (pPktHdr->achOpcode[0] != pszOpcode2[0])
        return false;
    if (pPktHdr->achOpcode[1] != pszOpcode2[1])
        return false;

    unsigned i = 2;
    while (   i < RT_SIZEOFMEMB(ATSPKTHDR, achOpcode)
           && pszOpcode2[i] != '\0')
    {
        if (pPktHdr->achOpcode[i] != pszOpcode2[i])
            break;
        i++;
    }

    if (   i < RT_SIZEOFMEMB(ATSPKTHDR, achOpcode)
        && pszOpcode2[i] == '\0')
    {
        while (   i < RT_SIZEOFMEMB(ATSPKTHDR, achOpcode)
               && pPktHdr->achOpcode[i] == ' ')
            i++;
    }

    return i == RT_SIZEOFMEMB(ATSPKTHDR, achOpcode);
}

/**
 * Converts a ATS request packet from host to network byte ordering.
 *
 * @returns nothing.
 * @param   pPktHdr           The packet to convert.
 */
DECLHIDDEN(void) atsProtocolReqH2N(PATSPKTHDR pPktHdr);

/**
 * Converts a ATS request packet from network to host byte ordering.
 *
 * @returns nothing.
 * @param   pPktHdr           The packet to convert.
 */
DECLHIDDEN(void) atsProtocolReqN2H(PATSPKTHDR pPktHdr);

/**
 * Converts a ATS reply packet from host to network byte ordering.
 *
 * @returns nothing.
 * @param   pPktHdr           The packet to convert.
 */
DECLHIDDEN(void) atsProtocolRepH2N(PATSPKTHDR pPktHdr);

/**
 * Converts a ATS reply packet from network to host byte ordering.
 *
 * @returns nothing.
 * @param   pPktHdr           The packet to convert.
 */
DECLHIDDEN(void) atsProtocolRepN2H(PATSPKTHDR pPktHdr);

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_Audio_AudioTestServiceProtocol_h */
