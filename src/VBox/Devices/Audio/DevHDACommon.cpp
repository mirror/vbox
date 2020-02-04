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

#include <VBox/AssertGuest.h>

#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include "DrvAudio.h"

#include "DevHDA.h"
#include "DevHDACommon.h"

#include "HDAStream.h"


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
 * Retrieves the currently set value for the wall clock.
 *
 * @return  IPRT status code.
 * @return  Currently set wall clock value.
 * @param   pThis               The shared HDA device state.
 *
 * @remark  Operation is atomic.
 */
uint64_t hdaWalClkGetCurrent(PHDASTATE pThis)
{
    return ASMAtomicReadU64(&pThis->u64WalClk);
}

#ifdef IN_RING3

/**
 * Helper for hdaR3WalClkSet.
 */
DECLINLINE(PHDASTREAMPERIOD) hdaR3SinkToStreamPeriod(PHDAMIXERSINK pSink)
{
    PHDASTREAM pStream = hdaR3GetSharedStreamFromSink(pSink);
    if (pStream)
        return &pStream->State.Period;
    return NULL;
}

/**
 * Sets the actual WALCLK register to the specified wall clock value.
 * The specified wall clock value only will be set (unless fForce is set to true) if all
 * handled HDA streams have passed (in time) that value. This guarantees that the WALCLK
 * register stays in sync with all handled HDA streams.
 *
 * @return  true if the WALCLK register has been updated, false if not.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   u64WalClk           Wall clock value to set WALCLK register to.
 * @param   fForce              Whether to force setting the wall clock value or not.
 */
bool hdaR3WalClkSet(PHDASTATE pThis, PHDASTATER3 pThisCC, uint64_t u64WalClk, bool fForce)
{
    const bool     fFrontPassed       = hdaR3StreamPeriodHasPassedAbsWalClk( hdaR3SinkToStreamPeriod(&pThisCC->SinkFront), u64WalClk);
    const uint64_t u64FrontAbsWalClk  = hdaR3StreamPeriodGetAbsElapsedWalClk(hdaR3SinkToStreamPeriod(&pThisCC->SinkFront));
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
#  error "Implement me!"
# endif

    const bool     fLineInPassed      = hdaR3StreamPeriodHasPassedAbsWalClk (hdaR3SinkToStreamPeriod(&pThisCC->SinkLineIn), u64WalClk);
    const uint64_t u64LineInAbsWalClk = hdaR3StreamPeriodGetAbsElapsedWalClk(hdaR3SinkToStreamPeriod(&pThisCC->SinkLineIn));
# ifdef VBOX_WITH_HDA_MIC_IN
    const bool     fMicInPassed       = hdaR3StreamPeriodHasPassedAbsWalClk (hdaR3SinkToStreamPeriod(&pThisCC->SinkMicIn),  u64WalClk);
    const uint64_t u64MicInAbsWalClk  = hdaR3StreamPeriodGetAbsElapsedWalClk(hdaR3SinkToStreamPeriod(&pThisCC->SinkMicIn));
# endif

# ifdef VBOX_STRICT
    const uint64_t u64WalClkCur       = ASMAtomicReadU64(&pThis->u64WalClk);
# endif

    /* Only drive the WALCLK register forward if all (active) stream periods have passed
     * the specified point in time given by u64WalClk. */
    if (  (   fFrontPassed
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
#  error "Implement me!"
# endif
           && fLineInPassed
# ifdef VBOX_WITH_HDA_MIC_IN
           && fMicInPassed
# endif
          )
       || fForce)
    {
        if (!fForce)
        {
            /* Get the maximum value of all periods we need to handle.
             * Not the most elegant solution, but works for now ... */
            u64WalClk = RT_MAX(u64WalClk, u64FrontAbsWalClk);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
#  error "Implement me!"
# endif
            u64WalClk = RT_MAX(u64WalClk, u64LineInAbsWalClk);
# ifdef VBOX_WITH_HDA_MIC_IN
            u64WalClk = RT_MAX(u64WalClk, u64MicInAbsWalClk);
# endif

# ifdef VBOX_STRICT
            AssertMsg(u64WalClk >= u64WalClkCur,
                      ("Setting WALCLK to a value going backwards does not make any sense (old %RU64 vs. new %RU64)\n",
                       u64WalClkCur, u64WalClk));
            if (u64WalClk == u64WalClkCur)     /* Setting a stale value? */
            {
                if (pThis->u8WalClkStaleCnt++ > 3)
                    AssertMsgFailed(("Setting WALCLK to a stale value (%RU64) too often isn't a good idea really. "
                                     "Good luck with stuck audio stuff.\n", u64WalClk));
            }
            else
                pThis->u8WalClkStaleCnt = 0;
# endif
        }

        /* Set the new WALCLK value. */
        ASMAtomicWriteU64(&pThis->u64WalClk, u64WalClk);
    }

    const uint64_t u64WalClkNew = hdaWalClkGetCurrent(pThis);

    Log3Func(("Cur: %RU64, New: %RU64 (force %RTbool) -> %RU64 %s\n",
              u64WalClkCur, u64WalClk, fForce,
              u64WalClkNew, u64WalClkNew == u64WalClk ? "[OK]" : "[DELAYED]"));

    return (u64WalClkNew == u64WalClk);
}

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

/*
 * Reads DMA data from a given HDA output stream.
 *
 * @return  IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state (for stats).
 * @param   pStreamShared       HDA output stream to read DMA data from - shared bits.
 * @param   pStreamR3           HDA output stream to read DMA data from - shared ring-3.
 * @param   pvBuf               Where to store the read data.
 * @param   cbBuf               How much to read in bytes.
 * @param   pcbRead             Returns read bytes from DMA. Optional.
 */
int hdaR3DMARead(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3,
                 void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pThis);
    PHDABDLE pBDLE = &pStreamShared->State.BDLE;

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;
    uint32_t cbLeft      = RT_MIN(cbBuf, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

# ifdef HDA_DEBUG_SILENCE
    uint64_t   csSilence = 0;

    pStreamCC->Dbg.cSilenceThreshold = 100;
    pStreamCC->Dbg.cbSilenceReadMin  = _1M;
# endif

    RTGCPHYS GCPhysChunk = pBDLE->Desc.u64BufAddr + pBDLE->State.u32BufOff;

    while (cbLeft)
    {
        uint32_t cbChunk = RT_MIN(cbLeft, pStreamShared->u16FIFOS);

        rc = PDMDevHlpPhysRead(pDevIns, GCPhysChunk, (uint8_t *)pvBuf + cbReadTotal, cbChunk);
        AssertRCBreak(rc);

# ifdef HDA_DEBUG_SILENCE
        uint16_t *pu16Buf = (uint16_t *)pvBuf;
        for (size_t i = 0; i < cbChunk / sizeof(uint16_t); i++)
        {
            if (*pu16Buf == 0)
                csSilence++;
            else
                break;
            pu16Buf++;
        }
# endif
        if (RT_LIKELY(!pStreamR3->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
            DrvAudioHlpFileWrite(pStreamR3->Dbg.Runtime.pFileDMARaw, (uint8_t *)pvBuf + cbReadTotal, cbChunk, 0 /* fFlags */);

        STAM_COUNTER_ADD(&pThis->StatBytesRead, cbChunk);

        /* advance */
        Assert(cbLeft    >= cbChunk);
        GCPhysChunk       = (GCPhysChunk + cbChunk) % pBDLE->Desc.u32BufSize;
        cbReadTotal      += cbChunk;
        cbLeft           -= cbChunk;
    }

# ifdef HDA_DEBUG_SILENCE
    if (csSilence)
        pStreamR3->Dbg.csSilence += csSilence;

    if (   csSilence == 0
        && pStreamR3->Dbg.csSilence   >  pStreamR3->Dbg.cSilenceThreshold
        && pStreamR3->Dbg.cbReadTotal >= pStreamR3->Dbg.cbSilenceReadMin)
    {
        LogFunc(("Silent block detected: %RU64 audio samples\n", pStreamR3->Dbg.csSilence));
        pStreamR3->Dbg.csSilence = 0;
    }
# endif

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbReadTotal;
    }

    return rc;
}

/**
 * Writes audio data from an HDA input stream's FIFO to its associated DMA area.
 *
 * @return  IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state (for stats).
 * @param   pStreamShared       HDA input stream to write audio data to - shared.
 * @param   pStreamR3           HDA input stream to write audio data to - ring-3.
 * @param   pvBuf               Data to write.
 * @param   cbBuf               How much (in bytes) to write.
 * @param   pcbWritten          Returns written bytes on success. Optional.
 */
int hdaR3DMAWrite(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3,
                  const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pThis);
    PHDABDLE    pBDLE          = &pStreamShared->State.BDLE;
    int         rc             = VINF_SUCCESS;
    uint32_t    cbWrittenTotal = 0;
    uint32_t    cbLeft         = RT_MIN(cbBuf, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);
    RTGCPHYS    GCPhysChunk    = pBDLE->Desc.u64BufAddr + pBDLE->State.u32BufOff;
    while (cbLeft)
    {
        uint32_t cbChunk = RT_MIN(cbLeft, pStreamShared->u16FIFOS);

        /* Sanity checks. */
        Assert(cbChunk <= pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

        if (RT_LIKELY(!pStreamR3->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
            DrvAudioHlpFileWrite(pStreamR3->Dbg.Runtime.pFileDMARaw, (uint8_t *)pvBuf + cbWrittenTotal, cbChunk, 0 /* fFlags */);

        rc = PDMDevHlpPCIPhysWrite(pDevIns, GCPhysChunk, (uint8_t *)pvBuf + cbWrittenTotal, cbChunk);
        AssertRCReturn(rc, rc);

        STAM_COUNTER_ADD(&pThis->StatBytesWritten, cbChunk);

        /* advance */
        Assert(cbLeft  >= cbChunk);
        cbWrittenTotal += (uint32_t)cbChunk;
        GCPhysChunk     = (GCPhysChunk + cbChunk) % pBDLE->Desc.u32BufSize;
        cbLeft         -= (uint32_t)cbChunk;
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWrittenTotal;
    }
    else
        LogFunc(("Failed with %Rrc\n", rc));

    return rc;
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

/**
 * Fetches a Bundle Descriptor List Entry (BDLE) from the DMA engine.
 *
 * @param   pDevIns             The device instance.
 * @param   pBDLE               Where to store the fetched result.
 * @param   u64BaseDMA          Address base of DMA engine to use.
 * @param   u16Entry            BDLE entry to fetch.
 */
int hdaR3BDLEFetch(PPDMDEVINS pDevIns, PHDABDLE pBDLE, uint64_t u64BaseDMA, uint16_t u16Entry)
{
    AssertPtrReturn(pBDLE,   VERR_INVALID_POINTER);
    AssertReturn(u64BaseDMA, VERR_INVALID_PARAMETER);

    if (!u64BaseDMA)
    {
        LogRel2(("HDA: Unable to fetch BDLE #%RU16 - no base DMA address set (yet)\n", u16Entry));
        return VERR_NOT_FOUND;
    }
    /** @todo Compare u16Entry with LVI. */

    int rc = PDMDevHlpPhysRead(pDevIns, u64BaseDMA + (u16Entry * sizeof(HDABDLEDESC)),
                               &pBDLE->Desc, sizeof(pBDLE->Desc));

    if (RT_SUCCESS(rc))
    {
        /* Reset internal state. */
        RT_ZERO(pBDLE->State);
        pBDLE->State.u32BDLIndex = u16Entry;
    }

    Log3Func(("Entry #%d @ 0x%x: %R[bdle], rc=%Rrc\n", u16Entry, u64BaseDMA + (u16Entry * sizeof(HDABDLEDESC)), pBDLE, rc));


    return VINF_SUCCESS;
}

/**
 * Tells whether a given BDLE is complete or not.
 *
 * @return  true if BDLE is complete, false if not.
 * @param   pBDLE               BDLE to retrieve status for.
 */
bool hdaR3BDLEIsComplete(PHDABDLE pBDLE)
{
    bool fIsComplete = false;

    if (   !pBDLE->Desc.u32BufSize /* There can be BDLEs with 0 size. */
        || (pBDLE->State.u32BufOff >= pBDLE->Desc.u32BufSize))
    {
        Assert(pBDLE->State.u32BufOff == pBDLE->Desc.u32BufSize);
        fIsComplete = true;
    }

    Log3Func(("%R[bdle] => %s\n", pBDLE, fIsComplete ? "COMPLETE" : "INCOMPLETE"));

    return fIsComplete;
}

/**
 * Tells whether a given BDLE needs an interrupt or not.
 *
 * @return  true if BDLE needs an interrupt, false if not.
 * @param   pBDLE               BDLE to retrieve status for.
 */
bool hdaR3BDLENeedsInterrupt(PHDABDLE pBDLE)
{
    return (pBDLE->Desc.fFlags & HDA_BDLE_F_IOC);
}

/**
 * Sets the virtual device timer to a new expiration time.
 *
 * @returns Whether the new expiration time was set or not.
 * @param   pDevIns         The device instance.
 * @param   pStreamShared   HDA stream to set timer for (shared).
 * @param   tsExpire        New (virtual) expiration time to set.
 * @param   fForce          Whether to force setting the expiration time or not.
 * @param   tsNow           The current clock timestamp if available, 0 if not.
 *
 * @remark  This function takes all active HDA streams and their
 *          current timing into account. This is needed to make sure
 *          that all streams can match their needed timing.
 *
 *          To achieve this, the earliest (lowest) timestamp of all
 *          active streams found will be used for the next scheduling slot.
 *
 *          Forcing a new expiration time will override the above mechanism.
 */
bool hdaR3TimerSet(PPDMDEVINS pDevIns, PHDASTREAM pStreamShared, uint64_t tsExpire, bool fForce, uint64_t tsNow)
{
    AssertPtr(pStreamShared);

    if (!tsNow)
        tsNow = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer);

    if (!fForce)
    {
        /** @todo r=bird: hdaR3StreamTransferIsScheduled() also does a
         * PDMDevHlpTimerGet(), so, some callers does one, this does, and then we do
         * right afterwards == very inefficient! */
        if (hdaR3StreamTransferIsScheduled(pStreamShared, tsNow))
        {
            uint64_t const tsNext = hdaR3StreamTransferGetNext(pStreamShared);
            if (tsExpire > tsNext)
                tsExpire = tsNext;
        }
    }

    /*
     * Make sure to not go backwards in time, as this will assert in TMTimerSet().
     * This in theory could happen in hdaR3StreamTransferGetNext() from above.
     */
    if (tsExpire < tsNow)
        tsExpire = tsNow;

    int rc = PDMDevHlpTimerSet(pDevIns, pStreamShared->hTimer, tsExpire);
    AssertRCReturn(rc, false);

    return true;
}

#endif /* IN_RING3 */
