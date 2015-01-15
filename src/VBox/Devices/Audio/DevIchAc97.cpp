/* $Id$ */
/** @file
 * DevIchAc97 - VBox ICH AC97 Audio Controller.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
# include "AudioMixer.h"
#else
 extern "C" {
  #include "audio.h"
 }
#endif

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#undef LOG_VOICES
#ifndef VBOX
//#define USE_MIXER
#else
# define USE_MIXER
#endif

#ifdef DEBUG
//#define DEBUG_LUN
# ifdef DEBUG_LUN
#  define DEBUG_LUN_NUM 1
# endif
#endif /* DEBUG */

#define AC97_SSM_VERSION 1

#ifndef VBOX
# define SOFT_VOLUME
#else
# undef  SOFT_VOLUME
#endif
#define SR_FIFOE RT_BIT(4)          /* rwc, fifo error */
#define SR_BCIS  RT_BIT(3)          /* rwc, buffer completion interrupt status */
#define SR_LVBCI RT_BIT(2)          /* rwc, last valid buffer completion interrupt */
#define SR_CELV  RT_BIT(1)          /* ro, current equals last valid */
#define SR_DCH   RT_BIT(0)          /* ro, controller halted */
#define SR_VALID_MASK (RT_BIT(5) - 1)
#define SR_WCLEAR_MASK (SR_FIFOE | SR_BCIS | SR_LVBCI)
#define SR_RO_MASK (SR_DCH | SR_CELV)
#define SR_INT_MASK (SR_FIFOE | SR_BCIS | SR_LVBCI)

#define CR_IOCE  RT_BIT(4)         /* rw */
#define CR_FEIE  RT_BIT(3)         /* rw */
#define CR_LVBIE RT_BIT(2)         /* rw */
#define CR_RR    RT_BIT(1)         /* rw */
#define CR_RPBM  RT_BIT(0)         /* rw */
#define CR_VALID_MASK (RT_BIT(5) - 1)
#define CR_DONT_CLEAR_MASK (CR_IOCE | CR_FEIE | CR_LVBIE)

#define GC_WR    4              /* rw */
#define GC_CR    2              /* rw */
#define GC_VALID_MASK (RT_BIT(6) - 1)

#define GS_MD3   RT_BIT(17)        /* rw */
#define GS_AD3   RT_BIT(16)        /* rw */
#define GS_RCS   RT_BIT(15)        /* rwc */
#define GS_B3S12 RT_BIT(14)        /* ro */
#define GS_B2S12 RT_BIT(13)        /* ro */
#define GS_B1S12 RT_BIT(12)        /* ro */
#define GS_S1R1  RT_BIT(11)        /* rwc */
#define GS_S0R1  RT_BIT(10)        /* rwc */
#define GS_S1CR  RT_BIT(9)         /* ro */
#define GS_S0CR  RT_BIT(8)         /* ro */
#define GS_MINT  RT_BIT(7)         /* ro */
#define GS_POINT RT_BIT(6)         /* ro */
#define GS_PIINT RT_BIT(5)         /* ro */
#define GS_RSRVD (RT_BIT(4)|RT_BIT(3))
#define GS_MOINT RT_BIT(2)         /* ro */
#define GS_MIINT RT_BIT(1)         /* ro */
#define GS_GSCI  RT_BIT(0)         /* rwc */
#define GS_RO_MASK (GS_B3S12 |                   \
                    GS_B2S12 |                   \
                    GS_B1S12 |                   \
                    GS_S1CR |                    \
                    GS_S0CR |                    \
                    GS_MINT |                    \
                    GS_POINT |                   \
                    GS_PIINT |                   \
                    GS_RSRVD |                   \
                    GS_MOINT |                   \
                    GS_MIINT)
#define GS_VALID_MASK (RT_BIT(18) - 1)
#define GS_WCLEAR_MASK (GS_RCS|GS_S1R1|GS_S0R1|GS_GSCI)

/** @name Buffer Descriptor
 * @{ */
#define BD_IOC RT_BIT(31)          /**< Interrupt on Completion */
#define BD_BUP RT_BIT(30)          /**< Buffer Underrun Policy */
/** @} */

#define EACS_VRA 1
#define EACS_VRM 8

#define VOL_MASK 0x1f
#define MUTE_SHIFT 15

#define REC_MASK 7
enum
{
    REC_MIC = 0,
    REC_CD,
    REC_VIDEO,
    REC_AUX,
    REC_LINE_IN,
    REC_STEREO_MIX,
    REC_MONO_MIX,
    REC_PHONE
};

enum
{
    AC97_Reset                     = 0x00,
    AC97_Master_Volume_Mute        = 0x02,
    AC97_Headphone_Volume_Mute     = 0x04,
    AC97_Master_Volume_Mono_Mute   = 0x06,
    AC97_Master_Tone_RL            = 0x08,
    AC97_PC_BEEP_Volume_Mute       = 0x0A,
    AC97_Phone_Volume_Mute         = 0x0C,
    AC97_Mic_Volume_Mute           = 0x0E,
    AC97_Line_In_Volume_Mute       = 0x10,
    AC97_CD_Volume_Mute            = 0x12,
    AC97_Video_Volume_Mute         = 0x14,
    AC97_Aux_Volume_Mute           = 0x16,
    AC97_PCM_Out_Volume_Mute       = 0x18,
    AC97_Record_Select             = 0x1A,
    AC97_Record_Gain_Mute          = 0x1C,
    AC97_Record_Gain_Mic_Mute      = 0x1E,
    AC97_General_Purpose           = 0x20,
    AC97_3D_Control                = 0x22,
    AC97_AC_97_RESERVED            = 0x24,
    AC97_Powerdown_Ctrl_Stat       = 0x26,
    AC97_Extended_Audio_ID         = 0x28,
    AC97_Extended_Audio_Ctrl_Stat  = 0x2A,
    AC97_PCM_Front_DAC_Rate        = 0x2C,
    AC97_PCM_Surround_DAC_Rate     = 0x2E,
    AC97_PCM_LFE_DAC_Rate          = 0x30,
    AC97_PCM_LR_ADC_Rate           = 0x32,
    AC97_MIC_ADC_Rate              = 0x34,
    AC97_6Ch_Vol_C_LFE_Mute        = 0x36,
    AC97_6Ch_Vol_L_R_Surround_Mute = 0x38,
    AC97_Vendor_Reserved           = 0x58,
    AC97_Vendor_ID1                = 0x7c,
    AC97_Vendor_ID2                = 0x7e
};


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Buffer descriptor.
 */
typedef struct BD
{
    uint32_t addr;
    uint32_t ctl_len;
} BD;

typedef struct AC97BusMasterRegs
{
    uint32_t bdbar;             /**< rw 0, buffer descriptor list base address register */
    uint8_t  civ;               /**< ro 0, current index value */
    uint8_t  lvi;               /**< rw 0, last valid index */
    uint16_t sr;                /**< rw 1, status register */
    uint16_t picb;              /**< ro 0, position in current buffer */
    uint8_t  piv;               /**< ro 0, prefetched index value */
    uint8_t  cr;                /**< rw 0, control register */
    int      bd_valid;          /**< initialized? */
    BD       bd;                /**< buffer descriptor */
} AC97BusMasterRegs;
/** Pointer to a AC97 bus master register. */
typedef AC97BusMasterRegs *PAC97BMREG;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
typedef struct AC97INPUTSTREAM
{
    /** PCM line input stream. */
    R3PTRTYPE(PPDMAUDIOGSTSTRMIN)      pStrmIn;
    /** Mixer handle for line input stream. */
    R3PTRTYPE(PAUDMIXSTREAM)           phStrmIn;
} AC97INPUTSTREAM, *PAC97INPUTSTREAM;

typedef struct AC97OUTPUTSTREAM
{
    /** PCM output stream. */
    R3PTRTYPE(PPDMAUDIOGSTSTRMOUT)     pStrmOut;
} AC97OUTPUTSTREAM, *PAC97OUTPUTSTREAM;

/**
 * Struct for maintaining a host backend driver.
 */
typedef struct AC97STATE *PAC97STATE;
typedef struct AC97DRIVER
{
    union
    {
        /** Node for storing this driver in our device driver
         *  list of AC97STATE. */
        RTLISTNODE                     Node;
        struct
        {
            R3PTRTYPE(void *)          dummy1;
            R3PTRTYPE(void *)          dummy2;
        } dummy;
    };

    /** Pointer to AC97 controller (state). */
    R3PTRTYPE(PAC97STATE)              pAC97State;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                   Flags;
    uint32_t                           PaddingFlags;
    /** LUN # to which this driver has been assigned. */
    uint8_t                            uLUN;
    /** Audio connector interface to the underlying
     *  host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pConnector;
    /** Stream for line input. */
    AC97INPUTSTREAM                    LineIn;
    /** Stream for mic input. */
    AC97INPUTSTREAM                    MicIn;
    /** Stream for output. */
    AC97OUTPUTSTREAM                   Out;
    /** Number of samples to play (output), needed
     *  for the timer routine. */
    uint32_t                           cSamplesLive;
} AC97DRIVER, *PAC97DRIVER;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

typedef struct AC97STATE
{
    /** The PCI device state. */
    PCIDevice               PciDev;
    /** Global Control (Bus Master Control Register) */
    uint32_t                glob_cnt;
    /** Global Status (Bus Master Control Register) */
    uint32_t                glob_sta;
    /** Codec Access Semaphore Register (Bus Master Control Register) */
    uint32_t                cas;
    uint32_t                last_samp;
    /** Bus Master Control Registers for PCM in, PCM out, and Mic in */
    AC97BusMasterRegs       bm_regs[3];
    uint8_t                 mixer_data[256];
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    /** The emulation timer for handling the attached
     *  LUN drivers. */
    PTMTIMERR3              pTimer;
    /** Timer ticks for handling the LUN drivers. */
    uint64_t                uTicks;
# ifdef VBOX_WITH_STATISTICS
    STAMPROFILE             StatTimer;
    STAMCOUNTER             StatBytesRead;
    STAMCOUNTER             StatBytesWritten;
# endif
    /** List of associated LUN drivers. */
    RTLISTANCHOR            lstDrv;
    /** The device' software mixer. */
    R3PTRTYPE(PAUDIOMIXER)  pMixer;
    /** Audio sink for line input. */
    R3PTRTYPE(PAUDMIXSINK)  pSinkLineIn;
    /** Audio sink for microphone input. */
    R3PTRTYPE(PAUDMIXSINK)  pSinkMicIn;
#else
    QEMUSoundCard           card;
    /** PCM in */
    SWVoiceIn              *voice_pi;
    /** PCM out */
    SWVoiceOut             *voice_po;
    /** Mic in */
    SWVoiceIn              *voice_mc;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    uint8_t                 silence[128];
    int                     bup_flag;
    /** Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** Pointer to the attached audio driver. */
    PPDMIBASE               pDrvBase;
    /** The base interface for LUN\#0. */
    PDMIBASE                IBase;
    /** Base port of the I/O space region. */
    RTIOPORT                IOPortBase[2];
    /** Pointer to temporary scratch read/write buffer. */
    R3PTRTYPE(uint8_t *)    pvReadWriteBuf;
    /** Size of the temporary scratch read/write buffer. */
    uint32_t                cbReadWriteBuf;
} AC97STATE;
/** Pointer to the AC97 device state. */
typedef AC97STATE *PAC97STATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define ICHAC97STATE_2_DEVINS(a_pAC97)   ((a_pAC97)->pDevIns)

enum
{
    BUP_SET  = RT_BIT(0),
    BUP_LAST = RT_BIT(1)
};

#define MKREGS(prefix, start)                   \
    enum {                                      \
        prefix ## _BDBAR = start,               \
        prefix ## _CIV   = start + 4,           \
        prefix ## _LVI   = start + 5,           \
        prefix ## _SR    = start + 6,           \
        prefix ## _PICB  = start + 8,           \
        prefix ## _PIV   = start + 10,          \
        prefix ## _CR    = start + 11           \
    }

enum
{
    PI_INDEX = 0,    /* PCM in */
    PO_INDEX,        /* PCM out */
    MC_INDEX,        /* Mic in */
    LAST_INDEX
};

MKREGS (PI, PI_INDEX * 16);
MKREGS (PO, PO_INDEX * 16);
MKREGS (MC, MC_INDEX * 16);

enum
{
    GLOB_CNT = 0x2c,
    GLOB_STA = 0x30,
    CAS      = 0x34
};

#define GET_BM(a_idx)   ( ((a_idx) >> 4) & 3 )

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static DECLCALLBACK(void) ichac97Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);
static int ichac97TransferAudio(PAC97STATE pThis, int index, uint32_t cbElapsed);
#else
static void ichac97OutputCallback(void *pvContext, int cbFree);
static void ichac97InputCallback(void *pvContext, int cbAvail);
static void ichac97MicInCallback(void *pvContext, int cbAvail);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

static void ichac97WarmReset(PAC97STATE pThis)
{
    NOREF(pThis);
}

static void ichac97ColdReset(PAC97STATE pThis)
{
    NOREF(pThis);
}

/** Fetches the buffer descriptor at _CIV. */
static void ichac97FetchBufDesc(PAC97STATE pThis, PAC97BMREG pReg)
{
    PPDMDEVINS pDevIns = ICHAC97STATE_2_DEVINS(pThis);
    uint32_t u32[2];

    PDMDevHlpPhysRead(pDevIns, pReg->bdbar + pReg->civ * 8, &u32[0], sizeof(u32));
    pReg->bd_valid   = 1;
#if !defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)
# error Please adapt the code (audio buffers are little endian)!
#else
    pReg->bd.addr    = RT_H2LE_U32(u32[0] & ~3);
    pReg->bd.ctl_len = RT_H2LE_U32(u32[1]);
#endif
    pReg->picb       = pReg->bd.ctl_len & 0xffff;
    LogFlowFunc(("bd %2d addr=%#x ctl=%#06x len=%#x(%d bytes)\n",
                  pReg->civ, pReg->bd.addr, pReg->bd.ctl_len >> 16,
                  pReg->bd.ctl_len & 0xffff, (pReg->bd.ctl_len & 0xffff) << 1));
}

/**
 * Update the BM status register
 */
static void ichac97UpdateStatus(PAC97STATE pThis, PAC97BMREG pReg, uint32_t new_sr)
{
    PPDMDEVINS  pDevIns = ICHAC97STATE_2_DEVINS(pThis);
    int event = 0;
    int level = 0;
    uint32_t new_mask = new_sr & SR_INT_MASK;
    uint32_t old_mask = pReg->sr  & SR_INT_MASK;
    static uint32_t const masks[] = { GS_PIINT, GS_POINT, GS_MINT };

    if (new_mask ^ old_mask)
    {
        /** @todo is IRQ deasserted when only one of status bits is cleared? */
        if (!new_mask)
        {
            event = 1;
            level = 0;
        }
        else if ((new_mask & SR_LVBCI) && (pReg->cr & CR_LVBIE))
        {
            event = 1;
            level = 1;
        }
        else if ((new_mask & SR_BCIS) && (pReg->cr & CR_IOCE))
        {
            event = 1;
            level = 1;
        }
    }

    pReg->sr = new_sr;

    LogFlowFunc(("IOC%d LVB%d sr=%#x event=%d level=%d\n",
                 pReg->sr & SR_BCIS, pReg->sr & SR_LVBCI, pReg->sr, event, level));

    if (event)
    {
        if (level)
            pThis->glob_sta |= masks[pReg - pThis->bm_regs];
        else
            pThis->glob_sta &= ~masks[pReg - pThis->bm_regs];

        LogFlowFunc(("set irq level=%d\n", !!level));
        PDMDevHlpPCISetIrq(pDevIns, 0, !!level);
    }
}

static void ichac97StreamSetActive(PAC97STATE pThis, int bm_index, int on)
{
    AssertPtrReturnVoid(pThis);

    LogFlowFunc(("index=%d, on=%d\n", bm_index, on));

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    switch (bm_index)
    {
        case PI_INDEX:
             RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                pDrv->pConnector->pfnEnableIn(pDrv->pConnector,
                                              pDrv->LineIn.pStrmIn, RT_BOOL(on));
            break;

        case PO_INDEX:
            RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                pDrv->pConnector->pfnEnableOut(pDrv->pConnector,
                                               pDrv->Out.pStrmOut, RT_BOOL(on));
            break;

        case MC_INDEX:
            RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                pDrv->pConnector->pfnEnableIn(pDrv->pConnector,
                                              pDrv->MicIn.pStrmIn, RT_BOOL(on));
            break;

        default:
            AssertMsgFailed(("Wrong index %d\n", bm_index));
            break;
    }
#else
    switch (bm_index)
    {
        case PI_INDEX: AUD_set_active_in( pThis->voice_pi, on); break;
        case PO_INDEX: AUD_set_active_out(pThis->voice_po, on); break;
        case MC_INDEX: AUD_set_active_in( pThis->voice_mc, on); break;
        default:       AssertFailed (); break;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
}

static void ichac97ResetBMRegs(PAC97STATE pThis, PAC97BMREG pReg)
{
    LogFlowFunc(("reset_bm_regs\n"));
    pReg->bdbar    = 0;
    pReg->civ      = 0;
    pReg->lvi      = 0;
    /** @todo do we need to do that? */
    ichac97UpdateStatus(pThis, pReg, SR_DCH);
    pReg->picb     = 0;
    pReg->piv      = 0;
    pReg->cr       = pReg->cr & CR_DONT_CLEAR_MASK;
    pReg->bd_valid = 0;
    ichac97StreamSetActive(pThis, pReg - pThis->bm_regs, 0);
    RT_ZERO(pThis->silence);
}

static void ichac97MixerStore(PAC97STATE pThis, uint32_t i, uint16_t v)
{
    if (i + 2 > sizeof(pThis->mixer_data))
    {
        LogFlowFunc(("mixer_store: index %d out of bounds %d\n", i, sizeof(pThis->mixer_data)));
        return;
    }

    pThis->mixer_data[i + 0] = v & 0xff;
    pThis->mixer_data[i + 1] = v >> 8;
}

static uint16_t ichac97MixerLoad(PAC97STATE pThis, uint32_t i)
{
    uint16_t val;

    if (i + 2 > sizeof(pThis->mixer_data))
    {
        LogFlowFunc(("mixer_store: index %d out of bounds %d\n", i, sizeof(pThis->mixer_data)));
        val = 0xffff;
    }
    else
        val = pThis->mixer_data[i + 0] | (pThis->mixer_data[i + 1] << 8);

    return val;
}

static void ichac97OpenStream(PAC97STATE pThis, int index, uint16_t freq)
{
    LogFlowFunc(("index=%d, freq=%RU16\n", index, freq));

    int rc;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    uint8_t uLUN = 0;

    if (freq)
    {
        PDMAUDIOSTREAMCFG streamCfg;
        RT_ZERO(streamCfg);
        streamCfg.uHz           = freq;
        streamCfg.cChannels     = 2;
        streamCfg.enmFormat     = AUD_FMT_S16;
        streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

        char *pszDesc;

        switch (index)
        {
            case PI_INDEX: /* Line input. */
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] ac97.pi", uLUN) <= 0)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    rc = pDrv->pConnector->pfnOpenIn(pDrv->pConnector,
                                                     pszDesc, PDMAUDIORECSOURCE_LINE_IN,
                                                     NULL, pDrv /* pvContext */,
                                                     &streamCfg,
                                                     &pDrv->LineIn.pStrmIn);
                    LogFlowFunc(("LUN#%RU8: Opened line input with rc=%Rrc\n", uLUN, rc));
                    if (rc == VINF_SUCCESS) /* Note: Could return VWRN_ALREADY_EXISTS. */
                    {
                        audioMixerRemoveStream(pThis->pSinkLineIn, pDrv->LineIn.phStrmIn);
                        rc = audioMixerAddStreamIn(pThis->pSinkLineIn,
                                                   pDrv->pConnector, pDrv->LineIn.pStrmIn,
                                                   0 /* uFlags */,
                                                   &pDrv->LineIn.phStrmIn);
                    }

                    RTStrFree(pszDesc);
                    uLUN++;
                }
                break;
            }

            case PO_INDEX: /* Output. */
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] ac97.po", uLUN) <= 0)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    rc = pDrv->pConnector->pfnOpenOut(pDrv->pConnector, pszDesc,
                                                      NULL, pDrv /* pvContext */,
                                                      &streamCfg,
                                                      &pDrv->Out.pStrmOut);
                    LogFlowFunc(("LUN#%RU8: Opened output with rc=%Rrc\n", uLUN, rc));

                    RTStrFree(pszDesc);
                    uLUN++;
                }
                break;
            }

            case MC_INDEX: /* Mic in */
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] ac97.mc", uLUN) <= 0)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    rc = pDrv->pConnector->pfnOpenIn(pDrv->pConnector,
                                                     pszDesc, PDMAUDIORECSOURCE_MIC,
                                                     NULL, pDrv /* pvContext */,
                                                     &streamCfg,
                                                     &pDrv->MicIn.pStrmIn);
                    LogFlowFunc(("LUN#%RU8: Opened mic input with rc=%Rrc\n", uLUN, rc));
                    if (rc == VINF_SUCCESS) /* Note: Could return VWRN_ALREADY_EXISTS. */
                    {
                        audioMixerRemoveStream(pThis->pSinkMicIn, pDrv->MicIn.phStrmIn);
                        rc = audioMixerAddStreamIn(pThis->pSinkMicIn,
                                                   pDrv->pConnector, pDrv->MicIn.pStrmIn,
                                                   0 /* uFlags */,
                                                   &pDrv->MicIn.phStrmIn);
                    }

                    RTStrFree(pszDesc);
                    uLUN++;
                }
                break;
            }

            default:
                AssertMsgFailed(("Unsupported index %d\n", index));
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else
    {
        switch (index)
        {
            case PI_INDEX:
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    pDrv->pConnector->pfnCloseIn(pDrv->pConnector, pDrv->LineIn.pStrmIn);
                    audioMixerRemoveStream(pThis->pSinkLineIn,pDrv->LineIn.phStrmIn);

                    pDrv->LineIn.pStrmIn = NULL;
                    pDrv->LineIn.phStrmIn = NULL;
                }

                LogFlowFunc(("Closed line input\n"));
                break;
            }

            case PO_INDEX:
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    pDrv->pConnector->pfnCloseOut(pDrv->pConnector, pDrv->Out.pStrmOut);
                    pDrv->Out.pStrmOut = NULL;
                }

                LogFlowFunc(("Closed output\n"));
                break;
            }

            case MC_INDEX:
            {
                RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
                {
                    pDrv->pConnector->pfnCloseIn(pDrv->pConnector, pDrv->MicIn.pStrmIn);
                    audioMixerRemoveStream(pThis->pSinkMicIn, pDrv->MicIn.phStrmIn);

                    pDrv->MicIn.pStrmIn  = NULL;
                    pDrv->MicIn.phStrmIn = NULL;
                }

                LogFlowFunc(("Closed microphone input\n"));
                break;
            }

            default:
                AssertMsgFailed(("Unsupported index %d\n", index));
                break;
        }

        rc = VINF_SUCCESS;
    }
#else
    if (freq)
    {
        audsettings_t as;
        as.freq       = freq;
        as.nchannels  = 2;
        as.fmt        = AUD_FMT_S16;
        as.endianness = 0;

        switch (index)
        {
            case PI_INDEX: /* PCM in */
                pThis->voice_pi = AUD_open_in(&pThis->card, pThis->voice_pi, "ac97.pi", pThis, ichac97InputCallback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open PI freq=%d (%s)\n", freq, pThis->voice_pi ? "ok" : "FAIL"));
#endif
                rc = pThis->voice_pi ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
                break;

            case PO_INDEX: /* PCM out */
                pThis->voice_po = AUD_open_out(&pThis->card, pThis->voice_po, "ac97.po", pThis, ichac97OutputCallback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open PO freq=%d (%s)\n", freq, pThis->voice_po ? "ok" : "FAIL"));
#endif
                rc = pThis->voice_po ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
                break;

            case MC_INDEX: /* Mic in */
                pThis->voice_mc = AUD_open_in(&pThis->card, pThis->voice_mc, "ac97.mc", pThis, ichac97MicInCallback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open MC freq=%d (%s)\n", freq, pThis->voice_mc ? "ok" : "FAIL"));
#endif
                rc = pThis->voice_mc ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
                break;
        }
    }
    else
    {
        switch (index)
        {
            case PI_INDEX:
                AUD_close_in(&pThis->card, pThis->voice_pi);
                pThis->voice_pi = NULL;
#ifdef LOG_VOICES
                LogRel(("AC97: Closing PCM IN\n"));
#endif
                break;

            case PO_INDEX:
                AUD_close_out(&pThis->card, pThis->voice_po);
                pThis->voice_po = NULL;
#ifdef LOG_VOICES
                LogRel(("AC97: Closing PCM OUT\n"));
#endif
                break;

            case MC_INDEX:
                AUD_close_in(&pThis->card, pThis->voice_mc);
                pThis->voice_mc = NULL;
#ifdef LOG_VOICES
                LogRel(("AC97: Closing MIC IN\n"));
#endif
                break;
        }

        rc = VINF_SUCCESS;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    LogFlowFuncLeaveRC(rc);
}

/** @todo r=andy D'oh, pretty bad argument handling -- fix this! */
static void ichac97ResetStreams(PAC97STATE pThis, uint8_t active[LAST_INDEX])
{
    uint16_t uFreq = ichac97MixerLoad(pThis, AC97_PCM_LR_ADC_Rate);
    bool fEnable = RT_BOOL(active[PI_INDEX]);
    LogFlowFunc(("Input ADC uFreq=%RU16, fEnabled=%RTbool\n", uFreq, fEnable));

    ichac97OpenStream(pThis, PI_INDEX, uFreq);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->LineIn.pStrmIn, fEnable);
#else
    AUD_set_active_in(pThis->voice_pi, active[PI_INDEX]);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    uFreq = ichac97MixerLoad(pThis, AC97_PCM_Front_DAC_Rate);
    fEnable = RT_BOOL(active[PO_INDEX]);
    LogFlowFunc(("Output DAC uFreq=%RU16, fEnabled=%RTbool\n", uFreq, fEnable));

    ichac97OpenStream(pThis, PO_INDEX, uFreq);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        pDrv->pConnector->pfnEnableOut(pDrv->pConnector, pDrv->Out.pStrmOut, fEnable);
#else
    AUD_set_active_out(pThis->voice_po, active[PO_INDEX]);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    uFreq = ichac97MixerLoad(pThis, AC97_MIC_ADC_Rate);
    fEnable = RT_BOOL(active[MC_INDEX]);
    LogFlowFunc(("Mic ADC uFreq=%RU16, fEnabled=%RTbool\n", uFreq, fEnable));

    ichac97OpenStream(pThis, MC_INDEX, uFreq);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        pDrv->pConnector->pfnEnableIn(pDrv->pConnector, pDrv->MicIn.pStrmIn, fEnable);
#else
     AUD_set_active_in(pThis->voice_mc, active[MC_INDEX]);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
}

#ifdef USE_MIXER

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static void ichac97SetVolume(PAC97STATE pThis, int index, PDMAUDIOMIXERCTL mt, uint32_t val)
#else
static void ichac97SetVolume(PAC97STATE pThis, int index, audmixerctl_t mt, uint32_t val)
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
{
    int mute = (val >> MUTE_SHIFT) & 1;
    uint8_t rvol = VOL_MASK - (val & VOL_MASK);
    uint8_t lvol = VOL_MASK - ((val >> 8) & VOL_MASK);
    rvol = 255 * rvol / VOL_MASK;
    lvol = 255 * lvol / VOL_MASK;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
#endif

#ifdef SOFT_VOLUME
    if (index == AC97_Master_Volume_Mute)
    {
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
            pDrv->pConnector->pfnIsSetOutVolume(pDrv->pConnector, pDrv->Out.pStrmOut, RT_BOOL(mute), lvol, rvol);
# else
        AUD_set_volume_out(pThis->voice_po, mute, lvol, rvol);
# endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }
    else
    {
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        {
            /** @todo In SetVolume no passing audmixerctl_in as its not used in DrvAudio.cpp. */
            pDrv->pConnector->pfnSetVolume(pDrv->pConnector, RT_BOOL(mute), lvol, rvol);
        }
# else
        AUD_set_volume(mt, &mute, &lvol, &rvol);
# endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }
#else /* !SOFT_VOLUME */
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        pDrv->pConnector->pfnSetVolume(pDrv->pConnector, RT_BOOL(mute), lvol, rvol);
# else
    AUD_set_volume(mt, &mute, &lvol, &rvol);
# endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
#endif /* SOFT_VOLUME */

    rvol = VOL_MASK - ((VOL_MASK * rvol) / 255);
    lvol = VOL_MASK - ((VOL_MASK * lvol) / 255);

    /*
     * From AC'97 SoundMax Codec AD1981A: "Because AC '97 defines 6-bit volume registers, to
     * maintain compatibility whenever the D5 or D13 bits are set to `1,' their respective
     * lower five volume bits are automatically set to `1' by the Codec logic. On readback,
     * all lower 5 bits will read ones whenever these bits are set to `1.'"
     *
     *  Linux ALSA depends on this behavior.
     */
    if (val & RT_BIT(5))
        val |= RT_BIT(4) | RT_BIT(3) | RT_BIT(2) | RT_BIT(1) | RT_BIT(0);
    if (val & RT_BIT(13))
        val |= RT_BIT(12) | RT_BIT(11) | RT_BIT(10) | RT_BIT(9) | RT_BIT(8);

    ichac97MixerStore(pThis, index, val);
}

static PDMAUDIORECSOURCE ichac97IndextoRecSource(uint8_t i)
{
    switch (i)
    {
        case REC_MIC:     return PDMAUDIORECSOURCE_MIC;
        case REC_CD:      return PDMAUDIORECSOURCE_CD;
        case REC_VIDEO:   return PDMAUDIORECSOURCE_VIDEO;
        case REC_AUX:     return PDMAUDIORECSOURCE_AUX;
        case REC_LINE_IN: return PDMAUDIORECSOURCE_LINE_IN;
        case REC_PHONE:   return PDMAUDIORECSOURCE_PHONE;
        default:
            break;
    }

    LogFlowFunc(("Unknown record source %d, using MIC\n", i));
    return PDMAUDIORECSOURCE_MIC;
}

static uint8_t ichac97RecSourceToIndex(PDMAUDIORECSOURCE rs)
{
    switch (rs)
    {
        case PDMAUDIORECSOURCE_MIC:     return REC_MIC;
        case PDMAUDIORECSOURCE_CD:      return REC_CD;
        case PDMAUDIORECSOURCE_VIDEO:   return REC_VIDEO;
        case PDMAUDIORECSOURCE_AUX:     return REC_AUX;
        case PDMAUDIORECSOURCE_LINE_IN: return REC_LINE_IN;
        case PDMAUDIORECSOURCE_PHONE:   return REC_PHONE;
        default:
            break;
    }

    LogFlowFunc(("Unknown audio recording source %d using MIC\n", rs));
    return REC_MIC;
}

static void ichac97RecordSelect(PAC97STATE pThis, uint32_t val)
{
    uint8_t rs = val & REC_MASK;
    uint8_t ls = (val >> 8) & REC_MASK;
    PDMAUDIORECSOURCE ars = ichac97IndextoRecSource(rs);
    PDMAUDIORECSOURCE als = ichac97IndextoRecSource(ls);
    //AUD_set_record_source(&als, &ars);
    rs = ichac97RecSourceToIndex(ars);
    ls = ichac97RecSourceToIndex(als);
    ichac97MixerStore(pThis, AC97_Record_Select, rs | (ls << 8));
}

#endif /* USE_MIXER */

static void ichac97MixerReset(PAC97STATE pThis)
{
    LogFlowFuncEnter();

    RT_ZERO(pThis->mixer_data);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;

    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
    {
        pDrv->LineIn.phStrmIn = NULL;
        pDrv->MicIn.phStrmIn  = NULL;
    }

    pThis->pSinkLineIn = NULL;
    pThis->pSinkMicIn = NULL;

    if (pThis->pMixer)
    {
        audioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    ichac97MixerStore(pThis, AC97_Reset                   , 0x0000); /* 6940 */
    ichac97MixerStore(pThis, AC97_Master_Volume_Mono_Mute , 0x8000);
    ichac97MixerStore(pThis, AC97_PC_BEEP_Volume_Mute     , 0x0000);

    ichac97MixerStore(pThis, AC97_Phone_Volume_Mute       , 0x8008);
    ichac97MixerStore(pThis, AC97_Mic_Volume_Mute         , 0x8008);
    ichac97MixerStore(pThis, AC97_CD_Volume_Mute          , 0x8808);
    ichac97MixerStore(pThis, AC97_Aux_Volume_Mute         , 0x8808);
    ichac97MixerStore(pThis, AC97_Record_Gain_Mic_Mute    , 0x8000);
    ichac97MixerStore(pThis, AC97_General_Purpose         , 0x0000);
    ichac97MixerStore(pThis, AC97_3D_Control              , 0x0000);
    ichac97MixerStore(pThis, AC97_Powerdown_Ctrl_Stat     , 0x000f);

    /*
     * Sigmatel 9700 (STAC9700)
     */
    ichac97MixerStore(pThis, AC97_Vendor_ID1              , 0x8384);
    ichac97MixerStore(pThis, AC97_Vendor_ID2              , 0x7600); /* 7608 */

    ichac97MixerStore(pThis, AC97_Extended_Audio_ID       , 0x0809);
    ichac97MixerStore(pThis, AC97_Extended_Audio_Ctrl_Stat, 0x0009);
    ichac97MixerStore(pThis, AC97_PCM_Front_DAC_Rate      , 0xbb80);
    ichac97MixerStore(pThis, AC97_PCM_Surround_DAC_Rate   , 0xbb80);
    ichac97MixerStore(pThis, AC97_PCM_LFE_DAC_Rate        , 0xbb80);
    ichac97MixerStore(pThis, AC97_PCM_LR_ADC_Rate         , 0xbb80);
    ichac97MixerStore(pThis, AC97_MIC_ADC_Rate            , 0xbb80);

#ifdef USE_MIXER
    ichac97RecordSelect(pThis, 0);
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    ichac97SetVolume(pThis, AC97_Master_Volume_Mute,  PDMAUDIOMIXERCTL_VOLUME,  0x8000);
    ichac97SetVolume(pThis, AC97_PCM_Out_Volume_Mute, PDMAUDIOMIXERCTL_PCM,     0x8808);
    ichac97SetVolume(pThis, AC97_Line_In_Volume_Mute, PDMAUDIOMIXERCTL_LINE_IN, 0x8808);
# else
    ichac97SetVolume(pThis, AC97_Master_Volume_Mute,  AUD_MIXER_VOLUME,  0x8000);
    ichac97SetVolume(pThis, AC97_PCM_Out_Volume_Mute, AUD_MIXER_PCM,     0x8808);
    ichac97SetVolume(pThis, AC97_Line_In_Volume_Mute, AUD_MIXER_LINE_IN, 0x8808);
# endif
#else
    ichac97MixerStore(pThis, AC97_Record_Select, 0);
    ichac97MixerStore(pThis, AC97_Master_Volume_Mute,  0x8000);
    ichac97MixerStore(pThis, AC97_PCM_Out_Volume_Mute, 0x8808);
    ichac97MixerStore(pThis, AC97_Line_In_Volume_Mute, 0x8808);
#endif

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    int rc2 = audioMixerCreate("AC'97 Mixer", 0 /* uFlags */,
                               &pThis->pMixer);
    if (RT_SUCCESS(rc2))
    {
        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = 48000;
        streamCfg.cChannels     = 2;
        streamCfg.enmFormat     = AUD_FMT_S16;
        streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

        rc2 = audioMixerSetDeviceFormat(pThis->pMixer, &streamCfg);
        AssertRC(rc2);

        /* Add all required audio sinks. */
        rc2 = audioMixerAddSink(pThis->pMixer, "[Recording] Line In",
                                &pThis->pSinkLineIn);
        AssertRC(rc2);

        rc2 = audioMixerAddSink(pThis->pMixer, "[Recording] Microphone In",
                                &pThis->pSinkMicIn);
        AssertRC(rc2);
    }
#endif

    /* Reset all streams. */
    uint8_t active[LAST_INDEX] = { 0 };
    ichac97ResetStreams(pThis, active);
}

static int ichac97WriteAudio(PAC97STATE pThis, PAC97BMREG pReg, uint32_t cbMax, uint32_t *pcbWritten)
{
    PPDMDEVINS  pDevIns = ICHAC97STATE_2_DEVINS(pThis);

    uint32_t    addr           = pReg->bd.addr;
    uint32_t    cbWrittenTotal = 0;
    uint32_t    cbToRead;

    uint32_t cbToWrite = RT_MIN((uint32_t)(pReg->picb << 1), cbMax);
    if (!cbToWrite)
        return VERR_NO_DATA;

    int rc = VINF_SUCCESS;

    LogFlowFunc(("pReg=%p, cbMax=%RU32\n", pReg, cbMax));

    while (cbToWrite)
    {
        uint32_t cbWrittenMin = UINT32_MAX;

        cbToRead = RT_MIN(cbToWrite, pThis->cbReadWriteBuf);
        PDMDevHlpPhysRead(pDevIns, addr, pThis->pvReadWriteBuf, cbToRead); /** @todo Check rc? */

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        uint32_t cbWritten;

        /* Just multiplex the output to the connected backends.
         * No need to utilize the virtual mixer here (yet). */
        PAC97DRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        {
            int rc2 = pDrv->pConnector->pfnWrite(pDrv->pConnector, pDrv->Out.pStrmOut,
                                                 pThis->pvReadWriteBuf, cbToRead, &cbWritten);
            if (RT_FAILURE(rc2))
                continue;

            cbWrittenMin = RT_MIN(cbWrittenMin, cbWritten);
            LogFlowFunc(("\tLUN#%RU8: cbWritten=%RU32, cWrittenMin=%RU32\n", pDrv->uLUN, cbWritten, cbWrittenMin));
        }
#else
        cbWrittenMin = AUD_write(pThis->voice_po, pThis->pvReadWriteBuf, cbToRead);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
        LogFlowFunc(("\tcbToRead=%RU32, cbWrittenMin=%RU32, cbToWrite=%RU32, cbLeft=%RU32\n",
                     cbToRead, cbWrittenMin, cbToWrite, cbToWrite - cbWrittenMin));

        if (!cbWrittenMin)
        {
            rc = VERR_NO_DATA;
            break;
        }

        Assert(cbWrittenMin != UINT32_MAX);
        Assert(cbToWrite >= cbWrittenMin);
        cbToWrite      -= cbWrittenMin;
        addr           += cbWrittenMin;
        cbWrittenTotal += cbWrittenMin;
    }

    pReg->bd.addr = addr;

    if (RT_SUCCESS(rc))
    {
        if (!cbToWrite) /* All data written? */
        {
            if (cbToRead < 4)
            {
                AssertMsgFailed(("Unable to save last written sample, cbToRead < 4 (is %RU32)\n", cbToRead));
                pThis->last_samp = 0;
            }
            else
                pThis->last_samp = *(uint32_t *)&pThis->pvReadWriteBuf[cbToRead - 4];
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWrittenTotal;
    }

    LogFlowFunc(("cbWrittenTotal=%RU32, rc=%Rrc\n", cbWrittenTotal, rc));
    return rc;
}

static void ichac97WriteBUP(PAC97STATE pThis, uint32_t cbElapsed)
{
    if (!(pThis->bup_flag & BUP_SET))
    {
        if (pThis->bup_flag & BUP_LAST)
        {
            unsigned int i;
            uint32_t *p = (uint32_t*)pThis->silence;
            for (i = 0; i < sizeof(pThis->silence) / 4; i++)
                *p++ = pThis->last_samp;
        }
        else
            RT_ZERO(pThis->silence);

        pThis->bup_flag |= BUP_SET;
    }

    while (cbElapsed)
    {
        uint32_t cbWrittenMin = UINT32_MAX;

        uint32_t cbToWrite = RT_MIN(cbElapsed, (uint32_t)sizeof(pThis->silence));
        while (cbToWrite)
        {
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            PAC97DRIVER pDrv;
            uint32_t cbWritten;
            RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
            {
                int rc2 = pDrv->pConnector->pfnWrite(pDrv->pConnector, pDrv->Out.pStrmOut,
                                                     pThis->silence, cbToWrite, &cbWritten);
                if (RT_FAILURE(rc2))
                    continue;

                cbWrittenMin = RT_MIN(cbWrittenMin, cbWritten);
            }
#else
            cbWrittenMin = AUD_write(pThis->voice_po, pThis->silence, cbToWrite);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

            if (!cbWrittenMin)
                return;

            Assert(cbToWrite >= cbWrittenMin);
            cbToWrite -= cbWrittenMin;
            Assert(cbElapsed >= cbWrittenMin);
            cbElapsed -= cbWrittenMin;
        }
    }
}

static int ichac97ReadAudio(PAC97STATE pThis, PAC97BMREG pReg, uint32_t cbMax, uint32_t *pcbWritten)
{
    AssertReturn(cbMax, VERR_INVALID_PARAMETER);

    PPDMDEVINS pDevIns = ICHAC97STATE_2_DEVINS(pThis);

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    /* Select audio sink to process. */
    PAUDMIXSINK pSink = (pReg - pThis->bm_regs) == MC_INDEX
                      ? pThis->pSinkMicIn : pThis->pSinkLineIn;
    AssertPtr(pSink);

    uint32_t cbRead = 0;

    size_t cbMixBuf = cbMax;
    uint32_t cbToRead = RT_MIN(pReg->picb << 1, cbMixBuf);

    if (!cbToRead)
        return VERR_NO_DATA;

    uint8_t *pvMixBuf = (uint8_t *)RTMemAlloc(cbMixBuf);
    if (pvMixBuf)
    {
        rc = audioMixerProcessSinkIn(pSink, pvMixBuf, cbToRead, &cbRead);
        if (   RT_SUCCESS(rc)
            && cbRead)
        {
            PDMDevHlpPCIPhysWrite(pDevIns, pReg->bd.addr, pvMixBuf, cbRead);
            pReg->bd.addr += cbRead;
        }

        RTMemFree(pvMixBuf);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        Assert(cbRead);
        if (pcbWritten)
            *pcbWritten = cbRead;
    }

    return rc;
#else
    uint32_t    addr = pReg->bd.addr;
    uint32_t    temp = pReg->picb << 1;
    uint32_t    nread = 0;
    int         to_copy = 0;

    SWVoiceIn  *voice = (pReg - pThis->bm_regs) == MC_INDEX ? pThis->voice_mc : pThis->voice_pi;

    temp = audio_MIN(temp, (uint32_t)cbMax);
    if (!temp)
        return VERR_NO_DATA;

    uint8_t tmpbuf[4096];
    while (temp)
    {
        int acquired;
        to_copy = audio_MIN(temp, sizeof(tmpbuf));
        acquired = AUD_read(voice, tmpbuf, to_copy);
        if (!acquired)
        {
            rc = VERR_GENERAL_FAILURE; /* Not worth fixing anymore. */
            break;
        }
        PDMDevHlpPCIPhysWrite(pDevIns, addr, tmpbuf, acquired);
        temp  -= acquired;
        addr  += acquired;
        nread += acquired;
    }

    pReg->bd.addr = addr;

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = nread;
    }

    return rc;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
}

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static DECLCALLBACK(void) ichac97Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, PAC97STATE);
    AssertPtr(pThis);

    STAM_PROFILE_START(&pThis->StatTimer, a);

    int rc = VINF_SUCCESS;

    uint32_t cbInMax  = 0;
    uint32_t cbOutMin = UINT32_MAX;

    PAC97DRIVER pDrv;
    uint32_t cbIn, cbOut;

    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
    {
        if (!pDrv->pConnector->pfnIsOutputOK(pDrv->pConnector, pDrv->Out.pStrmOut))
            continue;

        rc = pDrv->pConnector->pfnQueryData(pDrv->pConnector, /** @todo Rename QueryStatus */
                                            &cbIn, &cbOut, &pDrv->cSamplesLive);
        if (RT_SUCCESS(rc))
        {
            if (cbIn || cbOut)
                LogFlowFunc(("\tLUN#%RU8: cbIn=%RU32, cbOut=%RU32\n", pDrv->uLUN, cbIn, cbOut));

            cbInMax  = RT_MAX(cbInMax, cbIn);
            cbOutMin = RT_MIN(cbOutMin, cbOut);
        }
        else
            pDrv->cSamplesLive = 0;
    }

    /*
     * Playback.
     */
    if (cbOutMin)
    {
        Assert(cbOutMin != UINT32_MAX);
        ichac97TransferAudio(pThis, PO_INDEX, cbOutMin); /** @todo Add rc! */
    }
    else
    {
        RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
        {
            if (pDrv->cSamplesLive)
                pDrv->pConnector->pfnPlayOut(pDrv->pConnector);
        }
    }

    /*
     * Recording.
     */
    if (cbInMax)
        ichac97TransferAudio(pThis, PI_INDEX, cbInMax); /** @todo Add rc! */

    TMTimerSet(pThis->pTimer, TMTimerGet(pThis->pTimer) + pThis->uTicks);

    STAM_PROFILE_STOP(&pThis->StatTimer, a);
}
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

static int ichac97TransferAudio(PAC97STATE pThis, int index, uint32_t cbElapsed)
{
    LogFlowFunc(("pThis=%p, index=%d, cbElapsed=%RU32\n", pThis, index, cbElapsed));

    PAC97BMREG pReg = &pThis->bm_regs[index];
    if (pReg->sr & SR_DCH) /* Controller halted? */
    {
        if (pReg->cr & CR_RPBM)
        {
            switch (index)
            {
                case PO_INDEX:
                    ichac97WriteBUP(pThis, cbElapsed);
                    break;

                default:
                    break;
            }
        }

        return VINF_SUCCESS;
    }

    int rc;
    uint32_t cbWrittenTotal = 0;

    while ((cbElapsed >> 1))
    {
        if (!pReg->bd_valid)
        {
            LogFlowFunc(("Invalid buffer descriptor, fetching next one ...\n"));
            ichac97FetchBufDesc(pThis, pReg);
        }

        if (!pReg->picb) /* Got a new buffer descriptor, that is, the position is 0? */
        {
            LogFlowFunc(("Fresh buffer descriptor %d is empty, addr=%#x, len=%#x, skipping\n",
                         pReg->civ, pReg->bd.addr, pReg->bd.ctl_len));
            if (pReg->civ == pReg->lvi)
            {
                pReg->sr |= SR_DCH; /* CELV? */
                pThis->bup_flag = 0;

                rc = VINF_SUCCESS;
                break;
            }

            pReg->sr &= ~SR_CELV;
            pReg->civ = pReg->piv;
            pReg->piv = (pReg->piv + 1) % 32;

            ichac97FetchBufDesc(pThis, pReg);
            continue;
        }

        uint32_t cbWritten;
        switch (index)
        {
            case PO_INDEX:
            {
                rc = ichac97WriteAudio(pThis, pReg, cbElapsed, &cbWritten);
                if (RT_SUCCESS(rc))
                {
                    cbWrittenTotal += cbWritten;
                    cbElapsed      -= cbWritten;
                    Assert((cbWritten & 1) == 0);    /* Else the following shift won't work */
                    pReg->picb     -= (cbWritten >> 1);
                }
                break;
            }

            case PI_INDEX:
            case MC_INDEX:
            {
                rc = ichac97ReadAudio(pThis, pReg, cbElapsed, &cbWritten);
                if (RT_SUCCESS(rc))
                {
                    cbElapsed  -= cbWritten;
                    Assert((cbWritten & 1) == 0);    /* Else the following shift won't work */
                    pReg->picb -= (cbWritten >> 1);
                }
                break;
            }

            default:
                AssertMsgFailed(("Index %ld not supported\n", index));
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        LogFlowFunc(("pReg->picb=%RU16, cbWrittenTotal=%RU32\n", pReg->picb, cbWrittenTotal));

        if (!pReg->picb)
        {
            uint32_t new_sr = pReg->sr & ~SR_CELV;

            if (pReg->bd.ctl_len & BD_IOC)
            {
                new_sr |= SR_BCIS;
            }

            if (pReg->civ == pReg->lvi)
            {
                LogFlowFunc(("Underrun civ (%d) == lvi (%d)\n", pReg->civ, pReg->lvi));
                new_sr |= SR_LVBCI | SR_DCH | SR_CELV;
                pThis->bup_flag = (pReg->bd.ctl_len & BD_BUP) ? BUP_LAST : 0;

                rc = VERR_NO_DATA;
            }
            else
            {
                pReg->civ = pReg->piv;
                pReg->piv = (pReg->piv + 1) % 32;
                ichac97FetchBufDesc(pThis, pReg);
            }

            ichac97UpdateStatus(pThis, pReg, new_sr);
        }

        if (RT_FAILURE(rc))
        {
            if (rc == VERR_NO_DATA)
                rc = VINF_SUCCESS;

            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#ifndef VBOX_WITH_PDM_AUDIO_DRIVER
static void ichac97InputCallback(void *pvContext, int cbAvail)
{
    ichac97TransferAudio((AC97STATE *)pvContext, PI_INDEX, cbAvail);
}

static void ichac97MicInCallback(void *pvContext, int cbAvail)
{
    ichac97TransferAudio((AC97STATE *)pvContext, MC_INDEX, cbAvail);
}

static void ichac97OutputCallback(void *pvContext, int cbFree)
{
    ichac97TransferAudio((AC97STATE *)pvContext, PO_INDEX, cbFree);
}
#endif

/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
static DECLCALLBACK(int) ichac97IOPortNABMRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PAC97STATE pThis = (PAC97STATE)pvUser;

    switch (cb)
    {
        case 1:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case CAS:
                    /* Codec Access Semaphore Register */
                    LogFlowFunc(("CAS %d\n", pThis->cas));
                    *pu32 = pThis->cas;
                    pThis->cas = 1;
                    break;
                case PI_CIV:
                case PO_CIV:
                case MC_CIV:
                    /* Current Index Value Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->civ;
                    LogFlowFunc(("CIV[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_LVI:
                case PO_LVI:
                case MC_LVI:
                    /* Last Valid Index Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->lvi;
                    LogFlowFunc(("LVI[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_PIV:
                case PO_PIV:
                case MC_PIV:
                    /* Prefetched Index Value Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->piv;
                    LogFlowFunc(("PIV[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_CR:
                case PO_CR:
                case MC_CR:
                    /* Control Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->cr;
                    LogFlowFunc(("CR[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register (lower part) */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->sr & 0xff;
                    LogFlowFunc(("SRb[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                default:
                    LogFlowFunc(("U nabm readb %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        case 2:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->sr;
                    LogFlowFunc(("SR[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_PICB:
                case PO_PICB:
                case MC_PICB:
                    /* Position in Current Buffer Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->picb;
                    LogFlowFunc(("PICB[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                default:
                    LogFlowFunc(("U nabm readw %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        case 4:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case PI_BDBAR:
                case PO_BDBAR:
                case MC_BDBAR:
                    /* Buffer Descriptor Base Address Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->bdbar;
                    LogFlowFunc(("BMADDR[%d] -> %#x\n", GET_BM(index), *pu32));
                    break;
                case PI_CIV:
                case PO_CIV:
                case MC_CIV:
                    /* 32-bit access: Current Index Value Register +
                     *                Last Valid Index Register +
                     *                Status Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->civ | (pReg->lvi << 8) | (pReg->sr << 16);
                    LogFlowFunc(("CIV LVI SR[%d] -> %#x, %#x, %#x\n", GET_BM(index), pReg->civ, pReg->lvi, pReg->sr));
                    break;
                case PI_PICB:
                case PO_PICB:
                case MC_PICB:
                    /* 32-bit access: Position in Current Buffer Register +
                     *                Prefetched Index Value Register +
                     *                Control Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    *pu32 = pReg->picb | (pReg->piv << 16) | (pReg->cr << 24);
                    LogFlowFunc(("PICB PIV CR[%d] -> %#x %#x %#x %#x\n", GET_BM(index), *pu32, pReg->picb, pReg->piv, pReg->cr));
                    break;
                case GLOB_CNT:
                    /* Global Control */
                    *pu32 = pThis->glob_cnt;
                    LogFlowFunc(("glob_cnt -> %#x\n", *pu32));
                    break;
                case GLOB_STA:
                    /* Global Status */
                    *pu32 = pThis->glob_sta | GS_S0CR;
                    LogFlowFunc(("glob_sta -> %#x\n", *pu32));
                    break;
                default:
                    LogFlowFunc(("U nabm readl %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
static DECLCALLBACK(int) ichac97IOPortNABMWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PAC97STATE pThis = (PAC97STATE)pvUser;

    switch (cb)
    {
        case 1:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            switch (index)
            {
                case PI_LVI:
                case PO_LVI:
                case MC_LVI:
                    /* Last Valid Index */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    if ((pReg->cr & CR_RPBM) && (pReg->sr & SR_DCH))
                    {
                        pReg->sr &= ~(SR_DCH | SR_CELV);
                        pReg->civ = pReg->piv;
                        pReg->piv = (pReg->piv + 1) % 32;
                        ichac97FetchBufDesc(pThis, pReg);
                    }
                    pReg->lvi = u32 % 32;
                    LogFlowFunc(("LVI[%d] <- %#x\n", GET_BM(index), u32));
                    break;
                case PI_CR:
                case PO_CR:
                case MC_CR:
                    /* Control Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    if (u32 & CR_RR)
                        ichac97ResetBMRegs(pThis, pReg);
                    else
                    {
                        pReg->cr = u32 & CR_VALID_MASK;
                        if (!(pReg->cr & CR_RPBM))
                        {
                            ichac97StreamSetActive(pThis, pReg - pThis->bm_regs, 0);
                            pReg->sr |= SR_DCH;
                        }
                        else
                        {
                            pReg->civ = pReg->piv;
                            pReg->piv = (pReg->piv + 1) % 32;
                            ichac97FetchBufDesc(pThis, pReg);
                            pReg->sr &= ~SR_DCH;
                            ichac97StreamSetActive(pThis, pReg - pThis->bm_regs, 1);
                        }
                    }
                    LogFlowFunc(("CR[%d] <- %#x (cr %#x)\n", GET_BM(index), u32, pReg->cr));
                    break;
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    pReg->sr |= u32 & ~(SR_RO_MASK | SR_WCLEAR_MASK);
                    ichac97UpdateStatus(pThis, pReg, pReg->sr & ~(u32 & SR_WCLEAR_MASK));
                    LogFlowFunc(("SR[%d] <- %#x (sr %#x)\n", GET_BM(index), u32, pReg->sr));
                    break;
                default:
                    LogFlowFunc(("U nabm writeb %#x <- %#x\n", Port, u32));
                    break;
            }
            break;
        }

        case 2:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            switch (index)
            {
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    pReg->sr |= u32 & ~(SR_RO_MASK | SR_WCLEAR_MASK);
                    ichac97UpdateStatus(pThis, pReg, pReg->sr & ~(u32 & SR_WCLEAR_MASK));
                    LogFlowFunc(("SR[%d] <- %#x (sr %#x)\n", GET_BM(index), u32, pReg->sr));
                    break;
                default:
                    LogFlowFunc(("U nabm writew %#x <- %#x\n", Port, u32));
                    break;
            }
            break;
        }

        case 4:
        {
            PAC97BMREG pReg = NULL;
            uint32_t index = Port - pThis->IOPortBase[1];
            switch (index)
            {
                case PI_BDBAR:
                case PO_BDBAR:
                case MC_BDBAR:
                    /* Buffer Descriptor list Base Address Register */
                    pReg = &pThis->bm_regs[GET_BM(index)];
                    pReg->bdbar = u32 & ~3;
                    LogFlowFunc(("BDBAR[%d] <- %#x (bdbar %#x)\n", GET_BM(index), u32, pReg->bdbar));
                    break;
                case GLOB_CNT:
                    /* Global Control */
                    if (u32 & GC_WR)
                        ichac97WarmReset(pThis);
                    if (u32 & GC_CR)
                        ichac97ColdReset(pThis);
                    if (!(u32 & (GC_WR | GC_CR)))
                        pThis->glob_cnt = u32 & GC_VALID_MASK;
                    LogFlowFunc(("glob_cnt <- %#x (glob_cnt %#x)\n", u32, pThis->glob_cnt));
                    break;
                case GLOB_STA:
                    /* Global Status */
                    pThis->glob_sta &= ~(u32 & GS_WCLEAR_MASK);
                    pThis->glob_sta |= (u32 & ~(GS_WCLEAR_MASK | GS_RO_MASK)) & GS_VALID_MASK;
                    LogFlowFunc(("glob_sta <- %#x (glob_sta %#x)\n", u32, pThis->glob_sta));
                    break;
                default:
                    LogFlowFunc(("U nabm writel %#x <- %#x\n", Port, u32));
                    break;
            }
            break;
        }

        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
static DECLCALLBACK(int) ichac97IOPortNAMRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PAC97STATE pThis = (PAC97STATE)pvUser;

    switch (cb)
    {
        case 1:
        {
            LogFlowFunc(("U nam readb %#x\n", Port));
            pThis->cas = 0;
            *pu32 = ~0U;
            break;
        }

        case 2:
        {
            uint32_t index = Port - pThis->IOPortBase[0];
            *pu32 = ~0U;
            pThis->cas = 0;
            switch (index)
            {
                default:
                    *pu32 = ichac97MixerLoad(pThis, index);
                    LogFlowFunc(("nam readw %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        case 4:
        {
            LogFlowFunc(("U nam readl %#x\n", Port));
            pThis->cas = 0;
            *pu32 = ~0U;
            break;
        }

        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
static DECLCALLBACK(int) ichac97IOPortNAMWrite(PPDMDEVINS pDevIns,
                                               void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PAC97STATE pThis = (PAC97STATE)pvUser;

    switch (cb)
    {
        case 1:
        {
            LogFlowFunc(("U nam writeb %#x <- %#x\n", Port, u32));
            pThis->cas = 0;
            break;
        }

        case 2:
        {
            uint32_t index = Port - pThis->IOPortBase[0];
            pThis->cas = 0;
            switch (index)
            {
                case AC97_Reset:
                    ichac97MixerReset(pThis);
                    break;
                case AC97_Powerdown_Ctrl_Stat:
                    u32 &= ~0xf;
                    u32 |= ichac97MixerLoad(pThis, index) & 0xf;
                    ichac97MixerStore(pThis, index, u32);
                    break;
#ifdef USE_MIXER
                case AC97_Master_Volume_Mute:
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
                    ichac97SetVolume(pThis, index, PDMAUDIOMIXERCTL_VOLUME, u32);
#else
                    ichac97SetVolume(pThis, index, AUD_MIXER_VOLUME, u32);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
                    break;
                case AC97_PCM_Out_Volume_Mute:
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
                    ichac97SetVolume(pThis, index, PDMAUDIOMIXERCTL_PCM, u32);
#else
                    ichac97SetVolume(pThis, index, AUD_MIXER_PCM, u32);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
                    break;
                case AC97_Line_In_Volume_Mute:
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
                    ichac97SetVolume(pThis, index, PDMAUDIOMIXERCTL_LINE_IN, u32);
#else
                    ichac97SetVolume(pThis, index, AUD_MIXER_LINE_IN, u32);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
                    break;
                case AC97_Record_Select:
                    ichac97RecordSelect(pThis, u32);
                    break;
#else  /* !USE_MIXER */
                case AC97_Master_Volume_Mute:
                case AC97_PCM_Out_Volume_Mute:
                case AC97_Line_In_Volume_Mute:
                case AC97_Record_Select:
                    ichac97MixerStore(pThis, index, u32);
                    break;
#endif /* !USE_MIXER */
                case AC97_Vendor_ID1:
                case AC97_Vendor_ID2:
                    LogFlowFunc(("Attempt to write vendor ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_ID:
                    LogFlowFunc(("Attempt to write extended audio ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_Ctrl_Stat:
                    if (!(u32 & EACS_VRA))
                    {
                        ichac97MixerStore(pThis, AC97_PCM_Front_DAC_Rate, 0xbb80);
                        ichac97MixerStore(pThis, AC97_PCM_LR_ADC_Rate,    0xbb80);
                        ichac97OpenStream(pThis, PI_INDEX, 48000);
                        ichac97OpenStream(pThis, PO_INDEX, 48000);
                    }
                    if (!(u32 & EACS_VRM))
                    {
                        ichac97MixerStore(pThis, AC97_MIC_ADC_Rate, 0xbb80);
                        ichac97OpenStream(pThis, MC_INDEX, 48000);
                    }
                    LogFlowFunc(("Setting extended audio control to %#x\n", u32));
                    ichac97MixerStore(pThis, AC97_Extended_Audio_Ctrl_Stat, u32);
                    break;
                case AC97_PCM_Front_DAC_Rate:
                    if (ichac97MixerLoad(pThis, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRA)
                    {
                        ichac97MixerStore(pThis, index, u32);
                        LogFlowFunc(("Set front DAC rate to %d\n", u32));
                        ichac97OpenStream(pThis, PO_INDEX, u32);
                    }
                    else
                        LogFlowFunc(("Attempt to set front DAC rate to %d, but VRA is not set\n", u32));
                    break;
                case AC97_MIC_ADC_Rate:
                    if (ichac97MixerLoad(pThis, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRM)
                    {
                        ichac97MixerStore(pThis, index, u32);
                        LogFlowFunc(("Set MIC ADC rate to %d\n", u32));
                        ichac97OpenStream(pThis, MC_INDEX, u32);
                    }
                    else
                        LogFlowFunc(("Attempt to set MIC ADC rate to %d, but VRM is not set\n", u32));
                    break;
                case AC97_PCM_LR_ADC_Rate:
                    if (ichac97MixerLoad(pThis, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRA)
                    {
                        ichac97MixerStore(pThis, index, u32);
                        LogFlowFunc(("Set front LR ADC rate to %d\n", u32));
                        ichac97OpenStream(pThis, PI_INDEX, u32);
                    }
                    else
                        LogFlowFunc(("Attempt to set LR ADC rate to %d, but VRA is not set\n", u32));
                    break;
                default:
                    LogFlowFunc(("U nam writew %#x <- %#x\n", Port, u32));
                    ichac97MixerStore(pThis, index, u32);
                    break;
            }
            break;
        }

        case 4:
        {
            LogFlowFunc(("U nam writel %#x <- %#x\n", Port, u32));
            pThis->cas = 0;
            break;
        }

        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) ichac97IOPortMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb,
                                          PCIADDRESSSPACE enmType)
{
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    PAC97STATE  pThis = RT_FROM_MEMBER(pPciDev, AC97STATE, PciDev);
    RTIOPORT    Port = (RTIOPORT)GCPhysAddress;
    int         rc;

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(cb >= 0x20);

    if (iRegion == 0)
        rc = PDMDevHlpIOPortRegister(pDevIns, Port, 256, pThis,
                                     ichac97IOPortNAMWrite, ichac97IOPortNAMRead,
                                     NULL, NULL, "ICHAC97 NAM");
    else
        rc = PDMDevHlpIOPortRegister(pDevIns, Port, 64, pThis,
                                     ichac97IOPortNABMWrite, ichac97IOPortNABMRead,
                                     NULL, NULL, "ICHAC97 NABM");
    if (RT_FAILURE(rc))
        return rc;

    pThis->IOPortBase[iRegion] = Port;
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) ichac97SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, AC97STATE *);

    SSMR3PutU32(pSSM, pThis->glob_cnt);
    SSMR3PutU32(pSSM, pThis->glob_sta);
    SSMR3PutU32(pSSM, pThis->cas);

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->bm_regs); i++)
    {
        PAC97BMREG pReg = &pThis->bm_regs[i];
        SSMR3PutU32(pSSM, pReg->bdbar);
        SSMR3PutU8( pSSM, pReg->civ);
        SSMR3PutU8( pSSM, pReg->lvi);
        SSMR3PutU16(pSSM, pReg->sr);
        SSMR3PutU16(pSSM, pReg->picb);
        SSMR3PutU8( pSSM, pReg->piv);
        SSMR3PutU8( pSSM, pReg->cr);
        SSMR3PutS32(pSSM, pReg->bd_valid);
        SSMR3PutU32(pSSM, pReg->bd.addr);
        SSMR3PutU32(pSSM, pReg->bd.ctl_len);
    }
    SSMR3PutMem(pSSM, pThis->mixer_data, sizeof(pThis->mixer_data));

    uint8_t active[LAST_INDEX];

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
    {
        PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
        AssertPtr(pCon);
        active[PI_INDEX] = pCon->pfnIsActiveIn (pCon, pDrv->LineIn.pStrmIn) ? 1 : 0;
        active[PO_INDEX] = pCon->pfnIsActiveOut(pCon, pDrv->Out.pStrmOut)   ? 1 : 0;
        active[MC_INDEX] = pCon->pfnIsActiveIn (pCon, pDrv->MicIn.pStrmIn)  ? 1 : 0;
    }
#else
    active[PI_INDEX] = AUD_is_active_in( pThis->voice_pi) ? 1 : 0;
    active[PO_INDEX] = AUD_is_active_out(pThis->voice_po) ? 1 : 0;
    active[MC_INDEX] = AUD_is_active_in( pThis->voice_mc) ? 1 : 0;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    SSMR3PutMem(pSSM, active, sizeof(active));

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) ichac97LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, AC97STATE *);

    AssertMsgReturn (uVersion == AC97_SSM_VERSION, ("%d\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    SSMR3GetU32(pSSM, &pThis->glob_cnt);
    SSMR3GetU32(pSSM, &pThis->glob_sta);
    SSMR3GetU32(pSSM, &pThis->cas);

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->bm_regs); i++)
    {
        PAC97BMREG pReg = &pThis->bm_regs[i];
        SSMR3GetU32(pSSM, &pReg->bdbar);
        SSMR3GetU8( pSSM, &pReg->civ);
        SSMR3GetU8( pSSM, &pReg->lvi);
        SSMR3GetU16(pSSM, &pReg->sr);
        SSMR3GetU16(pSSM, &pReg->picb);
        SSMR3GetU8( pSSM, &pReg->piv);
        SSMR3GetU8( pSSM, &pReg->cr);
        SSMR3GetS32(pSSM, &pReg->bd_valid);
        SSMR3GetU32(pSSM, &pReg->bd.addr);
        SSMR3GetU32(pSSM, &pReg->bd.ctl_len);
    }

    SSMR3GetMem(pSSM, pThis->mixer_data, sizeof(pThis->mixer_data));
    uint8_t active[LAST_INDEX];
    SSMR3GetMem(pSSM, active, sizeof(active));

#ifdef USE_MIXER
    ichac97RecordSelect(pThis, ichac97MixerLoad(pThis, AC97_Record_Select));
# define V_(a, b) ichac97SetVolume(pThis, a, b, ichac97MixerLoad(pThis, a))
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    V_(AC97_Master_Volume_Mute,  PDMAUDIOMIXERCTL_VOLUME);
    V_(AC97_PCM_Out_Volume_Mute, PDMAUDIOMIXERCTL_PCM);
    V_(AC97_Line_In_Volume_Mute, PDMAUDIOMIXERCTL_LINE_IN);
# else
    V_(AC97_Master_Volume_Mute,  AUD_MIXER_VOLUME);
    V_(AC97_PCM_Out_Volume_Mute, AUD_MIXER_PCM);
    V_(AC97_Line_In_Volume_Mute, AUD_MIXER_LINE_IN);
# endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
# undef V_
#endif /* USE_MIXER */
    ichac97ResetStreams(pThis, active);

    pThis->bup_flag = 0;
    pThis->last_samp = 0;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) ichac97QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PAC97STATE pThis = RT_FROM_MEMBER(pInterface, AC97STATE, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 *
 * @remarks The original sources didn't install a reset handler, but it seems to
 *          make sense to me so we'll do it.
 */
static DECLCALLBACK(void) ac97Reset(PPDMDEVINS pDevIns)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, AC97STATE *);

    /*
     * Reset the device state (will need pDrv later).
     */
    ichac97ResetBMRegs(pThis, &pThis->bm_regs[0]);
    ichac97ResetBMRegs(pThis, &pThis->bm_regs[1]);
    ichac97ResetBMRegs(pThis, &pThis->bm_regs[2]);

    /*
     * Reset the mixer too. The Windows XP driver seems to rely on
     * this. At least it wants to read the vendor id before it resets
     * the codec manually.
     */
    ichac97MixerReset(pThis);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) ichac97Destruct(PPDMDEVINS pDevIns)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, PAC97STATE);

    LogFlowFuncEnter();

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    while (!RTListIsEmpty(&pThis->lstDrv))
    {
        pDrv = RTListGetFirst(&pThis->lstDrv, AC97DRIVER, Node);

        RTListNodeRemove(&pDrv->Node);
        RTMemFree(pDrv);
    }

    if (pThis->pMixer)
    {
        audioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    if (pThis->pvReadWriteBuf)
    {
        RTMemFree(pThis->pvReadWriteBuf);
        pThis->pvReadWriteBuf = NULL;
        pThis->cbReadWriteBuf = 0;
    }

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
/**
 * Attach command.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor have to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int) ichac97Attach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, PAC97STATE);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("AC'97 device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /*
     * Attach driver.
     */
    char *pszDesc = NULL;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (AC'97) for LUN #%u", uLUN) <= 0)
        AssertMsgReturn(pszDesc,
                        ("Not enough memory for AC'97 driver port description of LUN #%u\n", uLUN),
                        VERR_NO_MEMORY);

    int rc = PDMDevHlpDriverAttach(pDevIns, uLUN,
                                   &pThis->IBase, &pThis->pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        PAC97DRIVER pDrv = (PAC97DRIVER)RTMemAllocZ(sizeof(AC97DRIVER));
        if (pDrv)
        {
            pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL,
                      ("Configuration error: LUN #%u has no host audio interface, rc=%Rrc\n",
                      uLUN, rc));
            pDrv->pAC97State = pThis;
            pDrv->uLUN = uLUN;

            /*
             * For now we always set the driver at LUN 0 as our primary
             * host backend. This might change in the future.
             */
            if (pDrv->uLUN == 0)
                pDrv->Flags |= PDMAUDIODRVFLAG_PRIMARY;

            LogFunc(("LUN#%RU8: pCon=%p, drvFlags=0x%x\n", uLUN, pDrv->pConnector, pDrv->Flags));

            /* Attach to driver list. */
            RTListAppend(&pThis->lstDrv, &pDrv->Node);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        LogFunc(("No attached driver for LUN #%u\n", uLUN));
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Failed to attach AC'97 LUN #%u (\"%s\"), rc=%Rrc\n",
                        uLUN, pszDesc, rc));

    RTStrFree(pszDesc);

    LogFunc(("iLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) ichac97Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PAC97STATE pThis = PDMINS_2_DATA(pDevIns, PAC97STATE);

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validations.
     */
    if (!CFGMR3AreValuesValid(pCfg, "\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for the AC97 device"));

    /*
     * Initialize data (most of it anyway).
     */
    pThis->pDevIns                  = pDevIns;
    /* IBase */
    pThis->IBase.pfnQueryInterface  = ichac97QueryInterface;

    /* PCI Device (the assertions will be removed later) */
    PCIDevSetVendorId         (&pThis->PciDev, 0x8086); /* 00 ro - intel. */               Assert(pThis->PciDev.config[0x00] == 0x86); Assert(pThis->PciDev.config[0x01] == 0x80);
    PCIDevSetDeviceId         (&pThis->PciDev, 0x2415); /* 02 ro - 82801 / 82801aa(?). */  Assert(pThis->PciDev.config[0x02] == 0x15); Assert(pThis->PciDev.config[0x03] == 0x24);
    PCIDevSetCommand          (&pThis->PciDev, 0x0000); /* 04 rw,ro - pcicmd. */           Assert(pThis->PciDev.config[0x04] == 0x00); Assert(pThis->PciDev.config[0x05] == 0x00);
    PCIDevSetStatus           (&pThis->PciDev, VBOX_PCI_STATUS_DEVSEL_MEDIUM |  VBOX_PCI_STATUS_FAST_BACK); /* 06 rwc?,ro? - pcists. */      Assert(pThis->PciDev.config[0x06] == 0x80); Assert(pThis->PciDev.config[0x07] == 0x02);
    PCIDevSetRevisionId       (&pThis->PciDev, 0x01);   /* 08 ro - rid. */                 Assert(pThis->PciDev.config[0x08] == 0x01);
    PCIDevSetClassProg        (&pThis->PciDev, 0x00);   /* 09 ro - pi. */                  Assert(pThis->PciDev.config[0x09] == 0x00);
    PCIDevSetClassSub         (&pThis->PciDev, 0x01);   /* 0a ro - scc; 01 == Audio. */    Assert(pThis->PciDev.config[0x0a] == 0x01);
    PCIDevSetClassBase        (&pThis->PciDev, 0x04);   /* 0b ro - bcc; 04 == multimedia. */ Assert(pThis->PciDev.config[0x0b] == 0x04);
    PCIDevSetHeaderType       (&pThis->PciDev, 0x00);   /* 0e ro - headtyp. */             Assert(pThis->PciDev.config[0x0e] == 0x00);
    PCIDevSetBaseAddress      (&pThis->PciDev, 0,       /* 10 rw - nambar - native audio mixer base. */
                               true /* fIoSpace */, false /* fPrefetchable */, false /* f64Bit */, 0x00000000); Assert(pThis->PciDev.config[0x10] == 0x01); Assert(pThis->PciDev.config[0x11] == 0x00); Assert(pThis->PciDev.config[0x12] == 0x00); Assert(pThis->PciDev.config[0x13] == 0x00);
    PCIDevSetBaseAddress      (&pThis->PciDev, 1,       /* 14 rw - nabmbar - native audio bus mastering. */
                               true /* fIoSpace */, false /* fPrefetchable */, false /* f64Bit */, 0x00000000); Assert(pThis->PciDev.config[0x14] == 0x01); Assert(pThis->PciDev.config[0x15] == 0x00); Assert(pThis->PciDev.config[0x16] == 0x00); Assert(pThis->PciDev.config[0x17] == 0x00);
    PCIDevSetSubSystemVendorId(&pThis->PciDev, 0x8086); /* 2c ro - intel.) */              Assert(pThis->PciDev.config[0x2c] == 0x86); Assert(pThis->PciDev.config[0x2d] == 0x80);
    PCIDevSetSubSystemId      (&pThis->PciDev, 0x0000); /* 2e ro. */                       Assert(pThis->PciDev.config[0x2e] == 0x00); Assert(pThis->PciDev.config[0x2f] == 0x00);
    PCIDevSetInterruptLine    (&pThis->PciDev, 0x00);   /* 3c rw. */                       Assert(pThis->PciDev.config[0x3c] == 0x00);
    PCIDevSetInterruptPin     (&pThis->PciDev, 0x01);   /* 3d ro - INTA#. */               Assert(pThis->PciDev.config[0x3d] == 0x01);

    /*
     * Register the PCI device, it's I/O regions, the timer and the
     * saved state item.
     */
    int rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE (rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 256, PCI_ADDRESS_SPACE_IO, ichac97IOPortMap);
    if (RT_FAILURE (rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, 64, PCI_ADDRESS_SPACE_IO, ichac97IOPortMap);
    if (RT_FAILURE (rc))
        return rc;

    rc = PDMDevHlpSSMRegister(pDevIns, AC97_SSM_VERSION, sizeof(*pThis), ichac97SaveExec, ichac97LoadExec);
    if (RT_FAILURE (rc))
        return rc;

    /*
     * Attach driver.
     */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTListInit(&pThis->lstDrv);

    uint8_t uLUN = 0;
    do
    {
        LogFunc(("Trying to attach driver for LUN #%RU8 ...\n", uLUN));
        rc = ichac97Attach(pDevIns, uLUN++, PDM_TACH_FLAGS_NOT_HOT_PLUG);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
                rc = VINF_SUCCESS;
            break;
        }

    } while (0);

    LogFunc(("cLUNs=%RU8, rc=%Rrc\n", uLUN, rc));
#else
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Audio Driver Port");
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        LogFunc(("ac97: No attached driver!\n"));
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach AC97 LUN #0! rc=%Rrc\n", rc));
        return rc;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

#ifndef VBOX_WITH_PDM_AUDIO_DRIVER
    AUD_register_card("ICH0", &pThis->card);
#endif
    ac97Reset(pDevIns);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PAC97DRIVER pDrv;
    uLUN = 0;
    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
    {
        if (!pDrv->pConnector->pfnIsInputOK(pDrv->pConnector, pDrv->LineIn.pStrmIn))
            LogRel(("AC97: WARNING: Unable to open PCM line input for LUN #%RU32!\n", uLUN));
        if (!pDrv->pConnector->pfnIsOutputOK(pDrv->pConnector, pDrv->Out.pStrmOut))
            LogRel(("AC97: WARNING: Unable to open PCM output for LUN #%RU32!\n", uLUN));
        if (!pDrv->pConnector->pfnIsInputOK(pDrv->pConnector, pDrv->MicIn.pStrmIn))
            LogRel(("AC97: WARNING: Unable to open PCM microphone input for LUN #%RU32!\n", uLUN));

        uLUN++;
    }

    RTListForEach(&pThis->lstDrv, pDrv, AC97DRIVER, Node)
    {
        /*
         * Only primary drivers are critical for the VM to run. Everything else
         * might not worth showing an own error message box in the GUI.
         */
        if (!(pDrv->Flags & PDMAUDIODRVFLAG_PRIMARY))
            continue;

        PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
        AssertPtr(pCon);
        if (   !pCon->pfnIsInputOK (pCon, pDrv->LineIn.pStrmIn)
            && !pCon->pfnIsOutputOK(pCon, pDrv->Out.pStrmOut)
            && !pCon->pfnIsInputOK (pCon, pDrv->MicIn.pStrmIn))
        {
            LogRel(("AC97: Falling back to NULL driver\n"));

            /* Was not able initialize *any* stream.
             * Select the NULL audio driver instead. */
            pCon->pfnCloseIn (pCon, pDrv->LineIn.pStrmIn);
            pCon->pfnCloseOut(pCon, pDrv->Out.pStrmOut);
            pCon->pfnCloseIn (pCon, pDrv->MicIn.pStrmIn);

            pDrv->Out.pStrmOut = NULL;
            pDrv->LineIn.pStrmIn = NULL;
            pDrv->MicIn.pStrmIn = NULL;

            pCon->pfnInitNull(pCon);
            ac97Reset(pDevIns);

            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                N_("No audio devices could be opened. Selecting the NULL audio backend "
                   "with the consequence that no sound is audible"));
        }
        else if (   !pCon->pfnIsInputOK (pCon, pDrv->LineIn.pStrmIn)
                 || !pCon->pfnIsOutputOK(pCon, pDrv->Out.pStrmOut)
                 || !pCon->pfnIsInputOK (pCon, pDrv->MicIn.pStrmIn))
        {
            char   szMissingStreams[255];
            size_t len = 0;
            if (!pCon->pfnIsInputOK (pCon, pDrv->LineIn.pStrmIn))
                len = RTStrPrintf(szMissingStreams,
                                  sizeof(szMissingStreams), "PCM Input");
            if (!pCon->pfnIsOutputOK(pCon, pDrv->Out.pStrmOut))
                len += RTStrPrintf(szMissingStreams + len,
                                   sizeof(szMissingStreams) - len, len ? ", PCM Output" : "PCM Output");
            if (!pCon->pfnIsInputOK (pCon, pDrv->MicIn.pStrmIn))
                len += RTStrPrintf(szMissingStreams + len,
                                   sizeof(szMissingStreams) - len, len ? ", PCM Microphone" : "PCM Microphone");

            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                N_("Some AC'97 audio streams (%s) could not be opened. Guest applications generating audio "
                "output or depending on audio input may hang. Make sure your host audio device "
                "is working properly. Check the logfile for error messages of the audio "
                "subsystem"), szMissingStreams);
        }
    }
#else
    if (!AUD_is_host_voice_in_ok(pThis->voice_pi))
        LogRel(("AC97: WARNING: Unable to open PCM IN!\n"));
    if (!AUD_is_host_voice_in_ok(pThis->voice_mc))
        LogRel(("AC97: WARNING: Unable to open PCM MC!\n"));
    if (!AUD_is_host_voice_out_ok(pThis->voice_po))
        LogRel(("AC97: WARNING: Unable to open PCM OUT!\n"));

    if (   !AUD_is_host_voice_in_ok( pThis->voice_pi)
        && !AUD_is_host_voice_out_ok(pThis->voice_po)
        && !AUD_is_host_voice_in_ok( pThis->voice_mc))
    {
        AUD_close_in(&pThis->card, pThis->voice_pi);
        AUD_close_out(&pThis->card, pThis->voice_po);
        AUD_close_in(&pThis->card, pThis->voice_mc);

        pThis->voice_po = NULL;
        pThis->voice_pi = NULL;
        pThis->voice_mc = NULL;
        AUD_init_null();
        ac97Reset(pDevIns);

        PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
            N_("No audio devices could be opened. Selecting the NULL audio backend "
               "with the consequence that no sound is audible"));
    }
    else if (   !AUD_is_host_voice_in_ok( pThis->voice_pi)
             || !AUD_is_host_voice_out_ok(pThis->voice_po)
             || !AUD_is_host_voice_in_ok( pThis->voice_mc))
    {
        char   szMissingVoices[128];
        size_t len = 0;
        if (!AUD_is_host_voice_in_ok(pThis->voice_pi))
            len = RTStrPrintf(szMissingVoices, sizeof(szMissingVoices), "PCM_in");
        if (!AUD_is_host_voice_out_ok(pThis->voice_po))
            len += RTStrPrintf(szMissingVoices + len, sizeof(szMissingVoices) - len, len ? ", PCM_out" : "PCM_out");
        if (!AUD_is_host_voice_in_ok(pThis->voice_mc))
            len += RTStrPrintf(szMissingVoices + len, sizeof(szMissingVoices) - len, len ? ", PCM_mic" : "PCM_mic");

        PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
            N_("Some audio devices (%s) could not be opened. Guest applications generating audio "
               "output or depending on audio input may hang. Make sure your host audio device "
               "is working properly. Check the logfile for error messages of the audio "
               "subsystem"), szMissingVoices);
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    if (RT_SUCCESS(rc))
    {
        pThis->cbReadWriteBuf = _4K; /** @todo Make this configurable. */
        pThis->pvReadWriteBuf = (uint8_t *)RTMemAlloc(pThis->cbReadWriteBuf);
        if (!pThis->pvReadWriteBuf)
            rc = VERR_NO_MEMORY;
    }

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    if (RT_SUCCESS(rc))
    {
        /* Start the emulation timer. */
        rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, ichac97Timer, pThis,
                                    TMTIMER_FLAGS_NO_CRIT_SECT, "DevIchAc97", &pThis->pTimer);
        AssertRCReturn(rc, rc);

        if (RT_SUCCESS(rc))
        {
            pThis->uTicks = PDMDevHlpTMTimeVirtGetFreq(pDevIns) / 200; /** Hz. @todo Make this configurable! */
            if (pThis->uTicks < 100)
                pThis->uTicks = 100;
            LogFunc(("Timer ticks=%RU64\n", pThis->uTicks));

            /* Fire off timer. */
            TMTimerSet(pThis->pTimer, TMTimerGet(pThis->pTimer) + pThis->uTicks);
        }
    }

# ifdef VBOX_WITH_STATISTICS
    if (RT_SUCCESS(rc))
    {
        /*
         * Register statistics.
         */
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTimer,            STAMTYPE_PROFILE, "/Devices/AC97/Timer",             STAMUNIT_TICKS_PER_CALL, "Profiling hdaTimer.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead,        STAMTYPE_COUNTER, "/Devices/AC97/BytesRead"   ,      STAMUNIT_BYTES,          "Bytes read from AC97 emulation.");
        PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesWritten,     STAMTYPE_COUNTER, "/Devices/AC97/BytesWritten",      STAMUNIT_BYTES,          "Bytes written to AC97 emulation.");
    }
# endif

#endif

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceICHAC97 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ichac97",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "ICH AC'97 Audio Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(AC97STATE),
    /* pfnConstruct */
    ichac97Construct,
    /* pfnDestruct */
    ichac97Destruct,
    /* pfnRelocate */
    NULL,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ac97Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* !IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

