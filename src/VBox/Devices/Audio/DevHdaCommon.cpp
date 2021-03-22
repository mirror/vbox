/* $Id$ */
/** @file
 * DevHDACommon.cpp - Shared HDA device functions.
 *
 * @todo r=bird: Shared with whom exactly?
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/time.h>

#include <VBox/AssertGuest.h>

#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include "DevHda.h"
#include "DevHdaCommon.h"
#include "DevHdaStream.h"


/**
 * Processes (de/asserts) the interrupt according to the HDA's current state.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   pszSource           Caller information.
 */
#if defined(LOG_ENABLED) || defined(DOXYGEN_RUNNING)
void hdaProcessInterrupt(PPDMDEVINS pDevIns, PHDASTATE pThis, const char *pszSource)
#else
void hdaProcessInterrupt(PPDMDEVINS pDevIns, PHDASTATE pThis)
#endif
{
    uint32_t uIntSts = hdaGetINTSTS(pThis);

    HDA_REG(pThis, INTSTS) = uIntSts;

    /* NB: It is possible to have GIS set even when CIE/SIEn are all zero; the GIS bit does
     * not control the interrupt signal. See Figure 4 on page 54 of the HDA 1.0a spec.
     */
    /* Global Interrupt Enable (GIE) set? */
    if (   (HDA_REG(pThis, INTCTL) & HDA_INTCTL_GIE)
        && (HDA_REG(pThis, INTSTS) & HDA_REG(pThis, INTCTL) & (HDA_INTCTL_CIE | HDA_STRMINT_MASK)))
    {
        Log3Func(("Asserted (%s)\n", pszSource));

        PDMDevHlpPCISetIrq(pDevIns, 0, 1 /* Assert */);
        pThis->u8IRQL = 1;

#ifdef DEBUG
        pThis->Dbg.IRQ.tsAssertedNs = RTTimeNanoTS();
        pThis->Dbg.IRQ.tsProcessedLastNs = pThis->Dbg.IRQ.tsAssertedNs;
#endif
    }
    else
    {
        Log3Func(("Deasserted (%s)\n", pszSource));

        PDMDevHlpPCISetIrq(pDevIns, 0, 0 /* Deassert */);
        pThis->u8IRQL = 0;
    }
}

/**
 * Retrieves the number of bytes of a FIFOW register.
 *
 * @return Number of bytes of a given FIFOW register.
 * @param  u16RegFIFOW          FIFOW register to convert.
 */
uint8_t hdaSDFIFOWToBytes(uint16_t u16RegFIFOW)
{
    uint32_t cb;
    switch (u16RegFIFOW)
    {
        case HDA_SDFIFOW_8B:  cb = 8;  break;
        case HDA_SDFIFOW_16B: cb = 16; break;
        case HDA_SDFIFOW_32B: cb = 32; break;
        default:
            AssertFailedStmt(cb = 32); /* Paranoia. */
            break;
    }

    Assert(RT_IS_POWER_OF_TWO(cb));
    return cb;
}

#ifdef IN_RING3
/**
 * Returns the default (mixer) sink from a given SD#.
 * Returns NULL if no sink is found.
 *
 * @return  PHDAMIXERSINK
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   uSD                 SD# to return mixer sink for.
 *                              NULL if not found / handled.
 */
PHDAMIXERSINK hdaR3GetDefaultSink(PHDASTATER3 pThisCC, uint8_t uSD)
{
    if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN)
    {
        const uint8_t uFirstSDI = 0;

        if (uSD == uFirstSDI) /* First SDI. */
            return &pThisCC->SinkLineIn;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        if (uSD == uFirstSDI + 1)
            return &pThisCC->SinkMicIn;
# else
        /* If we don't have a dedicated Mic-In sink, use the always present Line-In sink. */
        return &pThisCC->SinkLineIn;
# endif
    }
    else
    {
        const uint8_t uFirstSDO = HDA_MAX_SDI;

        if (uSD == uFirstSDO)
            return &pThisCC->SinkFront;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        if (uSD == uFirstSDO + 1)
            return &pThisCC->SinkCenterLFE;
        if (uSD == uFirstSDO + 2)
            return &pThisCC->SinkRear;
# endif
    }

    return NULL;
}
#endif /* IN_RING3 */

/**
 * Returns the audio direction of a specified stream descriptor.
 *
 * The register layout specifies that input streams (SDI) come first,
 * followed by the output streams (SDO). So every stream ID below HDA_MAX_SDI
 * is an input stream, whereas everything >= HDA_MAX_SDI is an output stream.
 *
 * Note: SDnFMT register does not provide that information, so we have to judge
 *       for ourselves.
 *
 * @return  Audio direction.
 */
PDMAUDIODIR hdaGetDirFromSD(uint8_t uSD)
{
    AssertReturn(uSD < HDA_MAX_STREAMS, PDMAUDIODIR_UNKNOWN);

    if (uSD < HDA_MAX_SDI)
        return PDMAUDIODIR_IN;

    return PDMAUDIODIR_OUT;
}

/**
 * Returns the HDA stream of specified stream descriptor number.
 *
 * @return  Pointer to HDA stream, or NULL if none found.
 */
PHDASTREAM hdaGetStreamFromSD(PHDASTATE pThis, uint8_t uSD)
{
    AssertPtr(pThis);
    ASSERT_GUEST_MSG_RETURN(uSD < HDA_MAX_STREAMS, ("uSD=%u (%#x)\n", uSD, uSD), NULL);
    return &pThis->aStreams[uSD];
}

#ifdef IN_RING3

/**
 * Returns the HDA stream of specified HDA sink.
 *
 * @return  Pointer to HDA stream, or NULL if none found.
 */
PHDASTREAMR3 hdaR3GetR3StreamFromSink(PHDAMIXERSINK pSink)
{
    AssertPtrReturn(pSink, NULL);

    /** @todo Do something with the channel mapping here? */
    return pSink->pStreamR3;
}


/**
 * Returns the HDA stream of specified HDA sink.
 *
 * @return  Pointer to HDA stream, or NULL if none found.
 */
PHDASTREAM hdaR3GetSharedStreamFromSink(PHDAMIXERSINK pSink)
{
    AssertPtrReturn(pSink, NULL);

    /** @todo Do something with the channel mapping here? */
    return pSink->pStreamShared;
}

#endif /* IN_RING3 */

/**
 * Returns a new INTSTS value based on the current device state.
 *
 * @returns Determined INTSTS register value.
 * @param   pThis               The shared HDA device state.
 *
 * @remark  This function does *not* set INTSTS!
 */
uint32_t hdaGetINTSTS(PHDASTATE pThis)
{
    uint32_t intSts = 0;

    /* Check controller interrupts (RIRB, STATEST). */
    if (HDA_REG(pThis, RIRBSTS) & HDA_REG(pThis, RIRBCTL) & (HDA_RIRBCTL_ROIC | HDA_RIRBCTL_RINTCTL))
    {
        intSts |= HDA_INTSTS_CIS; /* Set the Controller Interrupt Status (CIS). */
    }

    /* Check SDIN State Change Status Flags. */
    if (HDA_REG(pThis, STATESTS) & HDA_REG(pThis, WAKEEN))
    {
        intSts |= HDA_INTSTS_CIS; /* Touch Controller Interrupt Status (CIS). */
    }

    /* For each stream, check if any interrupt status bit is set and enabled. */
    for (uint8_t iStrm = 0; iStrm < HDA_MAX_STREAMS; ++iStrm)
    {
        if (HDA_STREAM_REG(pThis, STS, iStrm) & HDA_STREAM_REG(pThis, CTL, iStrm) & (HDA_SDCTL_DEIE | HDA_SDCTL_FEIE  | HDA_SDCTL_IOCE))
        {
            Log3Func(("[SD%d] interrupt status set\n", iStrm));
            intSts |= RT_BIT(iStrm);
        }
    }

    if (intSts)
        intSts |= HDA_INTSTS_GIS; /* Set the Global Interrupt Status (GIS). */

    Log3Func(("-> 0x%x\n", intSts));

    return intSts;
}

#ifdef IN_RING3

/**
 * Converts an HDA stream's SDFMT register into a given PCM properties structure.
 *
 * @return  IPRT status code.
 * @param   u16SDFMT            The HDA stream's SDFMT value to convert.
 * @param   pProps              PCM properties structure to hold converted result on success.
 */
int hdaR3SDFMTToPCMProps(uint16_t u16SDFMT, PPDMAUDIOPCMPROPS pProps)
{
    AssertPtrReturn(pProps, VERR_INVALID_POINTER);

# define EXTRACT_VALUE(v, mask, shift) ((v & ((mask) << (shift))) >> (shift))

    int rc = VINF_SUCCESS;

    uint32_t u32Hz     = EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_BASE_RATE_MASK, HDA_SDFMT_BASE_RATE_SHIFT)
                       ? 44100 : 48000;
    uint32_t u32HzMult = 1;
    uint32_t u32HzDiv  = 1;

    switch (EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT))
    {
        case 0: u32HzMult = 1; break;
        case 1: u32HzMult = 2; break;
        case 2: u32HzMult = 3; break;
        case 3: u32HzMult = 4; break;
        default:
            LogFunc(("Unsupported multiplier %x\n",
                     EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_MULT_MASK, HDA_SDFMT_MULT_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }
    switch (EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT))
    {
        case 0: u32HzDiv = 1; break;
        case 1: u32HzDiv = 2; break;
        case 2: u32HzDiv = 3; break;
        case 3: u32HzDiv = 4; break;
        case 4: u32HzDiv = 5; break;
        case 5: u32HzDiv = 6; break;
        case 6: u32HzDiv = 7; break;
        case 7: u32HzDiv = 8; break;
        default:
            LogFunc(("Unsupported divisor %x\n",
                     EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_DIV_MASK, HDA_SDFMT_DIV_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    uint8_t cBytes = 0;
    switch (EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT))
    {
        case 0:
            cBytes = 1;
            break;
        case 1:
            cBytes = 2;
            break;
        case 4:
            cBytes = 4;
            break;
        default:
            AssertMsgFailed(("Unsupported bits per sample %x\n",
                             EXTRACT_VALUE(u16SDFMT, HDA_SDFMT_BITS_MASK, HDA_SDFMT_BITS_SHIFT)));
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        RT_BZERO(pProps, sizeof(PDMAUDIOPCMPROPS));

        pProps->cbSample  = cBytes;
        pProps->fSigned   = true;
        pProps->cChannels = (u16SDFMT & 0xf) + 1;
        pProps->uHz       = u32Hz * u32HzMult / u32HzDiv;
        pProps->cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pProps->cbSample, pProps->cChannels);
    }

# undef EXTRACT_VALUE
    return rc;
}

# ifdef LOG_ENABLED
void hdaR3BDLEDumpAll(PPDMDEVINS pDevIns, PHDASTATE pThis, uint64_t u64BDLBase, uint16_t cBDLE)
{
    LogFlowFunc(("BDLEs @ 0x%x (%RU16):\n", u64BDLBase, cBDLE));
    if (!u64BDLBase)
        return;

    uint32_t cbBDLE = 0;
    for (uint16_t i = 0; i < cBDLE; i++)
    {
        HDABDLEDESC bd;
        PDMDevHlpPhysRead(pDevIns, u64BDLBase + i * sizeof(HDABDLEDESC), &bd, sizeof(bd));

        LogFunc(("\t#%03d BDLE(adr:0x%llx, size:%RU32, ioc:%RTbool)\n",
                 i, bd.u64BufAddr, bd.u32BufSize, bd.fFlags & HDA_BDLE_F_IOC));

        cbBDLE += bd.u32BufSize;
    }

    LogFlowFunc(("Total: %RU32 bytes\n", cbBDLE));

    if (!pThis->u64DPBase) /* No DMA base given? Bail out. */
        return;

    LogFlowFunc(("DMA counters:\n"));

    for (int i = 0; i < cBDLE; i++)
    {
        uint32_t uDMACnt;
        PDMDevHlpPhysRead(pDevIns, (pThis->u64DPBase & DPBASE_ADDR_MASK) + (i * 2 * sizeof(uint32_t)),
                          &uDMACnt, sizeof(uDMACnt));

        LogFlowFunc(("\t#%03d DMA @ 0x%x\n", i , uDMACnt));
    }
}
# endif /* LOG_ENABLED */

#endif /* IN_RING3 */
