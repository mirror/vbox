/* $Id$ */
/** @file
 * DevIchHdaCodec - VBox ICH Intel HD Audio Codec.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef DEV_CODEC_H
#define DEV_CODEC_H

/** The ICH HDA (Intel) controller. */
typedef struct HDASTATE *PHDASTATE;
/** The ICH HDA (Intel) codec state. */
typedef struct HDACODEC *PHDACODEC;
/** The HDA host driver backend. */
typedef struct HDADRIVER *PHDADRIVER;
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;
typedef struct PDMAUDIOGSTSTRMOUT *PPDMAUDIOGSTSTRMOUT;
typedef struct PDMAUDIOGSTSTRMIN  *PPDMAUDIOGSTSTRMIN;

/**
 * Verb processor method.
 */
typedef DECLCALLBACK(int) FNHDACODECVERBPROCESSOR(PHDACODEC pThis, uint32_t cmd, uint64_t *pResp);
typedef FNHDACODECVERBPROCESSOR *PFNHDACODECVERBPROCESSOR;
typedef FNHDACODECVERBPROCESSOR **PPFNHDACODECVERBPROCESSOR;

/* PRM 5.3.1 */
#define CODEC_RESPONSE_UNSOLICITED RT_BIT_64(34)


#ifndef VBOX_WITH_HDA_CODEC_EMU
typedef struct CODECVERB
{
    uint32_t verb;
    /** operation bitness mask */
    uint32_t mask;
    PFNHDACODECVERBPROCESSOR pfn;
} CODECVERB;
#endif

#ifndef VBOX_WITH_HDA_CODEC_EMU
# define TYPE union
#else
# define TYPE struct
typedef struct CODECEMU CODECEMU;
typedef CODECEMU *PCODECEMU;
#endif
TYPE CODECNODE;
typedef TYPE CODECNODE CODECNODE;
typedef TYPE CODECNODE *PCODECNODE;

typedef enum
{
    PI_INDEX = 0,    /**< PCM in */
    PO_INDEX,        /**< PCM out */
    MC_INDEX,        /**< Mic in */
    LAST_INDEX
} ENMSOUNDSOURCE;

typedef struct HDACODEC
{
    uint16_t                id;
    uint16_t                u16VendorId;
    uint16_t                u16DeviceId;
    uint8_t                 u8BSKU;
    uint8_t                 u8AssemblyId;
    /** List of assigned HDA drivers to this codec.
     * A driver only can be assigned to one codec at a time. */
    RTLISTANCHOR            lstDrv;
    /** The codec's current audio stream configuration. */
    PDMAUDIOSTREAMCFG       strmCfg;

#ifndef VBOX_WITH_HDA_CODEC_EMU
    CODECVERB const        *paVerbs;
    int                     cVerbs;
#else
    PCODECEMU               pCodecBackend;
#endif
    PCODECNODE              paNodes;
    /** Pointer to HDA state (controller) this
     *  codec is assigned to. */
    PHDASTATE               pHDAState;
    bool                    fInReset;
#ifndef VBOX_WITH_HDA_CODEC_EMU
    const uint8_t           cTotalNodes;
    const uint8_t          *au8Ports;
    const uint8_t          *au8Dacs;
    const uint8_t          *au8AdcVols;
    const uint8_t          *au8Adcs;
    const uint8_t          *au8AdcMuxs;
    const uint8_t          *au8Pcbeeps;
    const uint8_t          *au8SpdifIns;
    const uint8_t          *au8SpdifOuts;
    const uint8_t          *au8DigInPins;
    const uint8_t          *au8DigOutPins;
    const uint8_t          *au8Cds;
    const uint8_t          *au8VolKnobs;
    const uint8_t          *au8Reserveds;
    const uint8_t           u8AdcVolsLineIn;
    const uint8_t           u8DacLineOut;
#endif
    /** Callbacks to the HDA controller, mostly used for multiplexing to the various host backends. */
    DECLR3CALLBACKMEMBER(void, pfnCloseIn, (PHDASTATE pThis, PDMAUDIORECSOURCE enmRecSource));
    DECLR3CALLBACKMEMBER(void, pfnCloseOut, (PHDASTATE pThis));
    DECLR3CALLBACKMEMBER(int, pfnOpenIn, (PHDASTATE pThis, const char *pszName, PDMAUDIORECSOURCE enmRecSource, PPDMAUDIOSTREAMCFG pCfg));
    DECLR3CALLBACKMEMBER(int, pfnOpenOut, (PHDASTATE pThis, const char *pszName, PPDMAUDIOSTREAMCFG pCfg));
    DECLR3CALLBACKMEMBER(int, pfnSetVolume, (PHDASTATE pThis, ENMSOUNDSOURCE enmSource, bool fMute, uint8_t uVolLeft, uint8_t uVolRight));
    /** Callbacks by codec implementation. */
    DECLR3CALLBACKMEMBER(int, pfnLookup, (PHDACODEC pThis, uint32_t verb, PPFNHDACODECVERBPROCESSOR));
    DECLR3CALLBACKMEMBER(int, pfnReset, (PHDACODEC pThis));
    DECLR3CALLBACKMEMBER(int, pfnCodecNodeReset, (PHDACODEC pThis, uint8_t, PCODECNODE));
    /** These callbacks are set by codec implementation to answer debugger requests. */
    DECLR3CALLBACKMEMBER(void, pfnDbgListNodes, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
    DECLR3CALLBACKMEMBER(void, pfnDbgSelector, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
} HDACODEC;

int hdaCodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis, uint16_t uLUN, PCFGMNODE pCfg);
int hdaCodecDestruct(PHDACODEC pThis);
int hdaCodecSaveState(PHDACODEC pThis, PSSMHANDLE pSSM);
int hdaCodecLoadState(PHDACODEC pThis, PSSMHANDLE pSSM, uint32_t uVersion);
int hdaCodecOpenStream(PHDACODEC pThis, ENMSOUNDSOURCE enmSoundSource, PPDMAUDIOSTREAMCFG pCfg);

#define HDA_SSM_VERSION   6
/** Introduced dynamic number of streams + stream identifiers for serialization.
 *  Bug: Did not save the BDLE states correctly.
 *  Those will be skipped on load then. */
#define HDA_SSM_VERSION_5 5
/** Since this version the number of MMIO registers can be flexible. */
#define HDA_SSM_VERSION_4 4
#define HDA_SSM_VERSION_3 3
#define HDA_SSM_VERSION_2 2
#define HDA_SSM_VERSION_1 1

# ifdef VBOX_WITH_HDA_CODEC_EMU
/* */
struct CODECEMU
{
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuConstruct,(PHDACODEC pThis));
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuDestruct,(PHDACODEC pThis));
    DECLR3CALLBACKMEMBER(int, pfnCodecEmuReset,(PHDACODEC pThis, bool fInit));
};
# endif
#endif

