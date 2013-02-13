/* $Id$ */
/** @file
 * DevIchHdaCodec - VBox ICH Intel HD Audio Codec.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

/** The ICH HDA (Intel) codec state. */
typedef struct HDACODEC HDACODEC;
/** Pointer to the Intel ICH HDA codec state. */
typedef HDACODEC *PHDACODEC;

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
    /* operation bitness mask */
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
    PI_INDEX = 0,    /* PCM in */
    PO_INDEX,        /* PCM out */
    MC_INDEX,        /* Mic in */
    LAST_INDEX
} ENMSOUNDSOURCE;


typedef struct HDACODEC
{
    uint16_t                id;
    uint16_t                u16VendorId;
    uint16_t                u16DeviceId;
    uint8_t                 u8BSKU;
    uint8_t                 u8AssemblyId;
#ifndef VBOX_WITH_HDA_CODEC_EMU
    CODECVERB const        *paVerbs;
    int                     cVerbs;
#else
    PCODECEMU               pCodecBackend;
#endif
    PCODECNODE              paNodes;
    QEMUSoundCard           card;
    /** PCM in */
    SWVoiceIn               *SwVoiceIn;
    /** PCM out */
    SWVoiceOut              *SwVoiceOut;
    void                   *pvHDAState;
    bool                    fInReset;
#ifndef VBOX_WITH_HDA_CODEC_EMU
    const uint8_t           cTotalNodes;
    const uint8_t           *au8Ports;
    const uint8_t           *au8Dacs;
    const uint8_t           *au8AdcVols;
    const uint8_t           *au8Adcs;
    const uint8_t           *au8AdcMuxs;
    const uint8_t           *au8Pcbeeps;
    const uint8_t           *au8SpdifIns;
    const uint8_t           *au8SpdifOuts;
    const uint8_t           *au8DigInPins;
    const uint8_t           *au8DigOutPins;
    const uint8_t           *au8Cds;
    const uint8_t           *au8VolKnobs;
    const uint8_t           *au8Reserveds;
    const uint8_t           u8AdcVolsLineIn;
    const uint8_t           u8DacLineOut;
#endif
    DECLR3CALLBACKMEMBER(int, pfnProcess, (PHDACODEC pCodec));
    DECLR3CALLBACKMEMBER(void, pfnTransfer, (PHDACODEC pCodec, ENMSOUNDSOURCE, int avail));
    /* These callbacks are set by Codec implementation. */
    DECLR3CALLBACKMEMBER(int, pfnLookup, (PHDACODEC pThis, uint32_t verb, PPFNHDACODECVERBPROCESSOR));
    DECLR3CALLBACKMEMBER(int, pfnReset, (PHDACODEC pThis));
    DECLR3CALLBACKMEMBER(int, pfnCodecNodeReset, (PHDACODEC pThis, uint8_t, PCODECNODE));
    /* These callbacks are set by codec implementation to answer debugger requests. */
    DECLR3CALLBACKMEMBER(void, pfnCodecDbgListNodes, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
    DECLR3CALLBACKMEMBER(void, pfnCodecDbgSelector, (PHDACODEC pThis, PCDBGFINFOHLP pHlp, const char *pszArgs));
} CODECState;

int hdaCodecConstruct(PPDMDEVINS pDevIns, PHDACODEC pThis, PCFGMNODE pCfg);
int hdaCodecDestruct(PHDACODEC pThis);
int hdaCodecSaveState(PHDACODEC pThis, PSSMHANDLE pSSM);
int hdaCodecLoadState(PHDACODEC pThis, PSSMHANDLE pSSM, uint32_t uVersion);
int hdaCodecOpenVoice(PHDACODEC pThis, ENMSOUNDSOURCE enmSoundSource, audsettings_t *pAudioSettings);

#define HDA_SSM_VERSION   4
#define HDA_SSM_VERSION_1 1
#define HDA_SSM_VERSION_2 2
#define HDA_SSM_VERSION_3 3

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
