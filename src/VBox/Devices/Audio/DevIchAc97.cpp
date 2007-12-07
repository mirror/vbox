/** @file
 *
 * VBox ICH AC97 Audio Controller
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>

#include "Builtins.h"

extern "C" {
#include "audio.h"
}

#undef LOG_VOICES
#ifndef VBOX
//#define USE_MIXER
#else
#define USE_MIXER
#endif

#define AC97_SSM_VERSION 1

enum {
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

#ifndef VBOX
#define SOFT_VOLUME
#else
#undef  SOFT_VOLUME
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
#define GS_RO_MASK (GS_B3S12|                   \
                    GS_B2S12|                   \
                    GS_B1S12|                   \
                    GS_S1CR|                    \
                    GS_S0CR|                    \
                    GS_MINT|                    \
                    GS_POINT|                   \
                    GS_PIINT|                   \
                    GS_RSRVD|                   \
                    GS_MOINT|                   \
                    GS_MIINT)
#define GS_VALID_MASK (RT_BIT(18) - 1)
#define GS_WCLEAR_MASK (GS_RCS|GS_S1R1|GS_S0R1|GS_GSCI)

/** Buffer Descriptor */
#define BD_IOC RT_BIT(31)          /* Interrupt on Completion */
#define BD_BUP RT_BIT(30)          /* Buffer Underrun Policy */

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

typedef struct BD
{
    uint32_t addr;
    uint32_t ctl_len;
} BD;

typedef struct AC97BusMasterRegs
{
    uint32_t bdbar;             /* rw 0, buffer descriptor list base address register */
    uint8_t  civ;               /* ro 0, current index value */
    uint8_t  lvi;               /* rw 0, last valid index */
    uint16_t sr;                /* rw 1, status register */
    uint16_t picb;              /* ro 0, position in current buffer */
    uint8_t  piv;               /* ro 0, prefetched index value */
    uint8_t  cr;                /* rw 0, control register */
    int      bd_valid;          /* initialized? */
    BD       bd;
} AC97BusMasterRegs;

typedef struct AC97LinkState
{
    QEMUSoundCard           card;
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
    /** PCM in */
    SWVoiceIn               *voice_pi;
    /** PCM out */
    SWVoiceOut              *voice_po;
    /** Mic in */
    SWVoiceIn               *voice_mc;
    uint8_t                 silence[128];
    int                     bup_flag;
    /** Pointer to the device instance. */
    PPDMDEVINS              pDevIns;
    /** Pointer to the connector of the attached audio driver. */
    PPDMIAUDIOCONNECTOR     pDrv;
    /** Pointer to the attached audio driver. */
    PPDMIBASE               pDrvBase;
    /** The base interface. */
    PDMIBASE                IBase;
    /** Base port of the I/O space region. */
    RTIOPORT                IOPortBase[2];
} AC97LinkState;

#define ICHAC97STATE_2_DEVINS(pAC97)   ((pAC97)->pDevIns)
#define PCIDEV_2_ICHAC97STATE(pPciDev) ((PCIAC97LinkState *)(pPciDev))

enum
{
    BUP_SET  = RT_BIT(0),
    BUP_LAST = RT_BIT(1)
};

typedef struct PCIAC97LinkState
{
    PCIDevice dev;
    AC97LinkState ac97;
} PCIAC97LinkState;

#define MKREGS(prefix, start)                   \
enum {                                          \
    prefix ## _BDBAR = start,                   \
    prefix ## _CIV   = start + 4,               \
    prefix ## _LVI   = start + 5,               \
    prefix ## _SR    = start + 6,               \
    prefix ## _PICB  = start + 8,               \
    prefix ## _PIV   = start + 10,              \
    prefix ## _CR    = start + 11               \
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

#define GET_BM(index) (((index) >> 4) & 3)

static void po_callback (void *opaque, int free);
static void pi_callback (void *opaque, int avail);
static void mc_callback (void *opaque, int avail);

static void warm_reset (AC97LinkState *s)
{
    (void) s;
}

static void cold_reset (AC97LinkState * s)
{
    (void) s;
}

/** Fetch Buffer Descriptor at _CIV */
static void fetch_bd (AC97LinkState *s, AC97BusMasterRegs *r)
{
    PPDMDEVINS pDevIns = ICHAC97STATE_2_DEVINS(s);
    uint8_t b[8];

    PDMDevHlpPhysRead (pDevIns, r->bdbar + r->civ * 8, b, sizeof(b));
    r->bd_valid   = 1;
#if !defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)
#error Please adapt the code (audio buffers are little endian)!
#else
    r->bd.addr    = (*(uint32_t *) &b[0]) & ~3;
    r->bd.ctl_len = (*(uint32_t *) &b[4]);
#endif
    r->picb       = r->bd.ctl_len & 0xffff;
    Log (("ac97: bd %2d addr=%#x ctl=%#06x len=%#x(%d bytes)\n",
          r->civ, r->bd.addr, r->bd.ctl_len >> 16,
          r->bd.ctl_len & 0xffff, (r->bd.ctl_len & 0xffff) << 1));
}

/**
 * Update the BM status register
 */
static void update_sr (AC97LinkState *s, AC97BusMasterRegs *r, uint32_t new_sr)
{
    PPDMDEVINS  pDevIns = ICHAC97STATE_2_DEVINS(s);
    int event = 0;
    int level = 0;
    uint32_t new_mask = new_sr & SR_INT_MASK;
    uint32_t old_mask = r->sr  & SR_INT_MASK;
    uint32_t masks[] = {GS_PIINT, GS_POINT, GS_MINT};

    if (new_mask ^ old_mask)
    {
        /** @todo is IRQ deasserted when only one of status bits is cleared? */
        if (!new_mask)
        {
            event = 1;
            level = 0;
        }
        else if ((new_mask & SR_LVBCI) && (r->cr & CR_LVBIE))
        {
            event = 1;
            level = 1;
        }
        else if ((new_mask & SR_BCIS) && (r->cr & CR_IOCE))
        {
            event = 1;
            level = 1;
        }
    }

    r->sr = new_sr;

    Log (("ac97: IOC%d LVB%d sr=%#x event=%d level=%d\n",
          r->sr & SR_BCIS, r->sr & SR_LVBCI, r->sr, event, level));

    if (event)
    {
        if (level)
            s->glob_sta |= masks[r - s->bm_regs];
        else
            s->glob_sta &= ~masks[r - s->bm_regs];

        Log (("ac97: set irq level=%d\n", !!level));
        PDMDevHlpPCISetIrq (pDevIns, 0, !!level);
    }
}

static void voice_set_active (AC97LinkState *s, int bm_index, int on)
{
    switch (bm_index)
    {
    case PI_INDEX: AUD_set_active_in (s->voice_pi, on); break;
    case PO_INDEX: AUD_set_active_out(s->voice_po, on); break;
    case MC_INDEX: AUD_set_active_in (s->voice_mc, on); break;
    default:       AssertFailed ();
                   break;
    }
}

static void reset_bm_regs (AC97LinkState *s, AC97BusMasterRegs *r)
{
    Log (("ac97: reset_bm_regs\n"));
    r->bdbar    = 0;
    r->civ      = 0;
    r->lvi      = 0;
    /** @todo do we need to do that? */
    update_sr (s, r, SR_DCH);
    r->picb     = 0;
    r->piv      = 0;
    r->cr       = r->cr & CR_DONT_CLEAR_MASK;
    r->bd_valid = 0;

    voice_set_active (s, r - s->bm_regs, 0);
    memset (s->silence, 0, sizeof (s->silence));
}

static void mixer_store (AC97LinkState *s, uint32_t i, uint16_t v)
{
    if (i + 2 > sizeof (s->mixer_data))
    {
        Log (("ac97: mixer_store: index %d out of bounds %d\n",
              i, sizeof (s->mixer_data)));
        return;
    }

    s->mixer_data[i + 0] = v & 0xff;
    s->mixer_data[i + 1] = v >> 8;
}

static uint16_t mixer_load (AC97LinkState *s, uint32_t i)
{
    uint16_t val;

    if (i + 2 > sizeof (s->mixer_data))
    {
        Log (("ac97: mixer_store: index %d out of bounds %d\n",
              i, sizeof (s->mixer_data)));
        val = 0xffff;
    }
    else
        val = s->mixer_data[i + 0] | (s->mixer_data[i + 1] << 8);

    return val;
}

static void open_voice (AC97LinkState *s, int index, int freq)
{
    audsettings_t as;

    if (freq)
    {
        as.freq       = freq;
        as.nchannels  = 2;
        as.fmt        = AUD_FMT_S16;
        as.endianness = 0;

        switch (index)
        {
            case PI_INDEX: /* PCM in */
                s->voice_pi = AUD_open_in (&s->card, s->voice_pi, "ac97.pi",
                                           s, pi_callback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open PI freq=%d (%s)\n", freq, s->voice_pi ? "ok" : "FAIL"));
#endif
                break;

            case PO_INDEX: /* PCM out */
                s->voice_po = AUD_open_out (&s->card, s->voice_po, "ac97.po",
                                            s, po_callback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open PO freq=%d (%s)\n", freq, s->voice_po ? "ok" : "FAIL"));
#endif
                break;

            case MC_INDEX: /* Mic in */
                s->voice_mc = AUD_open_in (&s->card, s->voice_mc, "ac97.mc",
                                           s, mc_callback, &as);
#ifdef LOG_VOICES
                LogRel(("AC97: open MC freq=%d (%s)\n", freq, s->voice_mc ? "ok" : "FAIL"));
#endif
                break;
        }
    }
    else
    {
        switch (index)
        {
            case PI_INDEX:
                AUD_close_in (&s->card, s->voice_pi);
#ifdef LOG_VOICES
                LogRel(("AC97: Closing PCM IN\n"));
#endif
                s->voice_pi = NULL;
                break;

            case PO_INDEX:
                AUD_close_out (&s->card, s->voice_po);
#ifdef LOG_VOICES
                LogRel(("AC97: Closing PCM OUT\n"));
#endif
                s->voice_po = NULL;
                break;

            case MC_INDEX:
                AUD_close_in (&s->card, s->voice_mc);
#ifdef LOG_VOICES
                LogRel(("AC97: Closing MIC IN\n"));
#endif
                s->voice_mc = NULL;
                break;
        }
    }
}

static void reset_voices (AC97LinkState *s, uint8_t active[LAST_INDEX])
{
    uint16_t freq;

    freq = mixer_load (s, AC97_PCM_LR_ADC_Rate);
    open_voice (s, PI_INDEX, freq);
    AUD_set_active_in (s->voice_pi, active[PI_INDEX]);

    freq = mixer_load (s, AC97_PCM_Front_DAC_Rate);
    open_voice (s, PO_INDEX, freq);
    AUD_set_active_out (s->voice_po, active[PO_INDEX]);

    freq = mixer_load (s, AC97_MIC_ADC_Rate);
    open_voice (s, MC_INDEX, freq);
    AUD_set_active_in (s->voice_mc, active[MC_INDEX]);
}

#ifdef USE_MIXER
static void set_volume (AC97LinkState *s, int index,
                        audmixerctl_t mt, uint32_t val)
{
    int mute = (val >> MUTE_SHIFT) & 1;
    uint8_t rvol = VOL_MASK - (val & VOL_MASK);
    uint8_t lvol = VOL_MASK - ((val >> 8) & VOL_MASK);
    rvol = 255 * rvol / VOL_MASK;
    lvol = 255 * lvol / VOL_MASK;

#ifdef SOFT_VOLUME
    if (index == AC97_Master_Volume_Mute)
        AUD_set_volume_out (s->voice_po, mute, lvol, rvol);
    else
        AUD_set_volume (mt, &mute, &lvol, &rvol);
#else
    AUD_set_volume (mt, &mute, &lvol, &rvol);
#endif

    rvol = VOL_MASK - ((VOL_MASK * rvol) / 255);
    lvol = VOL_MASK - ((VOL_MASK * lvol) / 255);

#ifdef VBOX
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
#endif

    mixer_store (s, index, val);
}

static audrecsource_t ac97_to_aud_record_source (uint8_t i)
{
    switch (i)
    {
        case REC_MIC:     return AUD_REC_MIC;
        case REC_CD:      return AUD_REC_CD;
        case REC_VIDEO:   return AUD_REC_VIDEO;
        case REC_AUX:     return AUD_REC_AUX;
        case REC_LINE_IN: return AUD_REC_LINE_IN;
        case REC_PHONE:   return AUD_REC_PHONE;
        default:          Log (("ac97: Unknown record source %d, using MIC\n", i));
                          return AUD_REC_MIC;
    }
}

static uint8_t aud_to_ac97_record_source (audrecsource_t rs)
{
    switch (rs)
    {
        case AUD_REC_MIC:     return REC_MIC;
        case AUD_REC_CD:      return REC_CD;
        case AUD_REC_VIDEO:   return REC_VIDEO;
        case AUD_REC_AUX:     return REC_AUX;
        case AUD_REC_LINE_IN: return REC_LINE_IN;
        case AUD_REC_PHONE:   return REC_PHONE;
        default:              Log (("ac97: Unknown audio recording source %d using MIC\n", rs));
                              return REC_MIC;
    }
}

static void record_select (AC97LinkState *s, uint32_t val)
{
    uint8_t rs = val & REC_MASK;
    uint8_t ls = (val >> 8) & REC_MASK;
    audrecsource_t ars = ac97_to_aud_record_source (rs);
    audrecsource_t als = ac97_to_aud_record_source (ls);
    AUD_set_record_source (&als, &ars);
    rs = aud_to_ac97_record_source (ars);
    ls = aud_to_ac97_record_source (als);
    mixer_store (s, AC97_Record_Select, rs | (ls << 8));
}
#endif

static void mixer_reset (AC97LinkState *s)
{
    uint8_t active[LAST_INDEX];

    Log (("ac97: mixer_reset\n"));
    memset (s->mixer_data, 0, sizeof (s->mixer_data));
    memset (active, 0, sizeof (active));
    mixer_store (s, AC97_Reset                   , 0x0000); /* 6940 */
    mixer_store (s, AC97_Master_Volume_Mono_Mute , 0x8000);
    mixer_store (s, AC97_PC_BEEP_Volume_Mute     , 0x0000);

    mixer_store (s, AC97_Phone_Volume_Mute       , 0x8008);
    mixer_store (s, AC97_Mic_Volume_Mute         , 0x8008);
    mixer_store (s, AC97_CD_Volume_Mute          , 0x8808);
    mixer_store (s, AC97_Aux_Volume_Mute         , 0x8808);
    mixer_store (s, AC97_Record_Gain_Mic_Mute    , 0x8000);
    mixer_store (s, AC97_General_Purpose         , 0x0000);
    mixer_store (s, AC97_3D_Control              , 0x0000);
    mixer_store (s, AC97_Powerdown_Ctrl_Stat     , 0x000f);

    /*
     * Sigmatel 9700 (STAC9700)
     */
    mixer_store (s, AC97_Vendor_ID1              , 0x8384);
    mixer_store (s, AC97_Vendor_ID2              , 0x7600); /* 7608 */

    mixer_store (s, AC97_Extended_Audio_ID       , 0x0809);
    mixer_store (s, AC97_Extended_Audio_Ctrl_Stat, 0x0009);
    mixer_store (s, AC97_PCM_Front_DAC_Rate      , 0xbb80);
    mixer_store (s, AC97_PCM_Surround_DAC_Rate   , 0xbb80);
    mixer_store (s, AC97_PCM_LFE_DAC_Rate        , 0xbb80);
    mixer_store (s, AC97_PCM_LR_ADC_Rate         , 0xbb80);
    mixer_store (s, AC97_MIC_ADC_Rate            , 0xbb80);

#ifdef USE_MIXER
    record_select (s, 0);
    set_volume (s, AC97_Master_Volume_Mute,  AUD_MIXER_VOLUME,  0x8000);
    set_volume (s, AC97_PCM_Out_Volume_Mute, AUD_MIXER_PCM,     0x8808);
    set_volume (s, AC97_Line_In_Volume_Mute, AUD_MIXER_LINE_IN, 0x8808);
#else
    mixer_store (s, AC97_Record_Select, 0);
    mixer_store (s, AC97_Master_Volume_Mute,  0x8000);
    mixer_store (s, AC97_PCM_Out_Volume_Mute, 0x8808);
    mixer_store (s, AC97_Line_In_Volume_Mute, 0x8808);
#endif

    reset_voices (s, active);
}

static int write_audio (AC97LinkState *s, AC97BusMasterRegs *r,
                        int max, int *stop)
{
    PPDMDEVINS  pDevIns = ICHAC97STATE_2_DEVINS(s);
    uint8_t  tmpbuf[4096];
    uint32_t addr = r->bd.addr;
    uint32_t temp = r->picb << 1;
    uint32_t written = 0;
    int to_copy = 0;

    temp = audio_MIN (temp, (uint32_t) max);
    if (!temp)
    {
        *stop = 1;
        return 0;
    }

    while (temp)
    {
        int copied;
        to_copy = audio_MIN (temp, sizeof (tmpbuf));
        PDMDevHlpPhysRead (pDevIns, addr, tmpbuf, to_copy);
        copied = AUD_write (s->voice_po, tmpbuf, to_copy);
        Log (("ac97: write_audio max=%x to_copy=%x copied=%x\n",
              max, to_copy, copied));
        if (!copied)
        {
            *stop = 1;
            break;
        }
        temp    -= copied;
        addr    += copied;
        written += copied;
    }

    if (!temp)
    {
        if (to_copy < 4)
        {
            Log (("ac97: whoops\n"));
            s->last_samp = 0;
        }
        else
            s->last_samp = *(uint32_t *) &tmpbuf[to_copy - 4];
    }

    r->bd.addr = addr;
    return written;
}

static void write_bup (AC97LinkState *s, int elapsed)
{
    int written = 0;

    Log (("ac97: write_bup\n"));
    if (!(s->bup_flag & BUP_SET))
    {
        if (s->bup_flag & BUP_LAST)
        {
            unsigned int i;
            uint32_t *p = (uint32_t*)s->silence;
            for (i = 0; i < sizeof (s->silence) / 4; i++)
                *p++ = s->last_samp;
        }
        else
            memset (s->silence, 0, sizeof (s->silence));

        s->bup_flag |= BUP_SET;
    }

    while (elapsed)
    {
        unsigned int temp = audio_MIN ((unsigned int)elapsed, sizeof (s->silence));
        while (temp)
        {
            int copied = AUD_write (s->voice_po, s->silence, temp);
            if (!copied)
                return;
            temp    -= copied;
            elapsed -= copied;
            written += copied;
        }
    }
}

static int read_audio (AC97LinkState *s, AC97BusMasterRegs *r,
                       int max, int *stop)
{
    PPDMDEVINS  pDevIns = ICHAC97STATE_2_DEVINS(s);
    uint8_t  tmpbuf[4096];
    uint32_t addr = r->bd.addr;
    uint32_t temp = r->picb << 1;
    uint32_t nread = 0;
    int      to_copy = 0;
    SWVoiceIn *voice = (r - s->bm_regs) == MC_INDEX ? s->voice_mc : s->voice_pi;

    temp = audio_MIN (temp, (uint32_t) max);
    if (!temp)
    {
        *stop = 1;
        return 0;
    }

    while (temp)
    {
        int acquired;
        to_copy = audio_MIN (temp, sizeof (tmpbuf));
        acquired = AUD_read (voice, tmpbuf, to_copy);
        if (!acquired)
        {
            *stop = 1;
            break;
        }
        PDMDevHlpPhysWrite (pDevIns, addr, tmpbuf, acquired);
        temp  -= acquired;
        addr  += acquired;
        nread += acquired;
    }

    r->bd.addr = addr;
    return nread;
}

static void transfer_audio (AC97LinkState *s, int index, int elapsed)
{
    AC97BusMasterRegs *r = &s->bm_regs[index];
    int written = 0, stop = 0;

    if (r->sr & SR_DCH)
    {
        if (r->cr & CR_RPBM)
        {
            switch (index)
            {
            case PO_INDEX:
                write_bup (s, elapsed);
                break;
            }
        }
        return;
    }

    while ((elapsed >> 1) && !stop)
    {
        int temp;

        if (!r->bd_valid)
        {
            Log (("ac97: invalid bd\n"));
            fetch_bd (s, r);
        }

        if (!r->picb)
        {
            Log (("ac97: fresh bd %d is empty %#x %#x\n",
                  r->civ, r->bd.addr, r->bd.ctl_len));
            if (r->civ == r->lvi)
            {
                r->sr |= SR_DCH; /* CELV? */
                s->bup_flag = 0;
                break;
            }
            r->sr &= ~SR_CELV;
            r->civ = r->piv;
            r->piv = (r->piv + 1) % 32;
            fetch_bd (s, r);
            return;
        }

        switch (index)
        {
            case PO_INDEX:
                temp = write_audio (s, r, elapsed, &stop);
                written += temp;
                elapsed -= temp;
                r->picb -= (temp >> 1);
                break;

            case PI_INDEX:
            case MC_INDEX:
                temp = read_audio (s, r, elapsed, &stop);
                elapsed -= temp;
                r->picb -= (temp >> 1);
                break;
        }

        Log(("r->picb = %d\n", r->picb));

        if (!r->picb)
        {
            uint32_t new_sr = r->sr & ~SR_CELV;

            if (r->bd.ctl_len & BD_IOC)
                new_sr |= SR_BCIS;

            if (r->civ == r->lvi)
            {
                Log (("ac97: Underrun civ (%d) == lvi (%d)\n", r->civ, r->lvi));
                new_sr |= SR_LVBCI | SR_DCH | SR_CELV;
                stop = 1;
                s->bup_flag = (r->bd.ctl_len & BD_BUP) ? BUP_LAST : 0;
            }
            else
            {
                r->civ = r->piv;
                r->piv = (r->piv + 1) % 32;
                fetch_bd (s, r);
            }
            update_sr (s, r, new_sr);
        }
    }
}

static void pi_callback (void *opaque, int avail)
{
    transfer_audio ((AC97LinkState*)opaque, PI_INDEX, avail);
}

static void mc_callback (void *opaque, int avail)
{
    transfer_audio ((AC97LinkState*)opaque, MC_INDEX, avail);
}

static void po_callback (void *opaque, int free)
{
    transfer_audio ((AC97LinkState*)opaque, PO_INDEX, free);
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) ichac97IOPortNABMRead (PPDMDEVINS pDevIns, void *pvUser,
                                                RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PCIAC97LinkState *d = (PCIAC97LinkState*)pvUser;
    AC97LinkState *s = &d->ac97;

    switch (cb)
    {
        case 1:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case CAS:
                    /* Codec Access Semaphore Register */
                    Log (("ac97: CAS %d\n", s->cas));
                    *pu32 = s->cas;
                    s->cas = 1;
                    break;
                case PI_CIV:
                case PO_CIV:
                case MC_CIV:
                    /* Current Index Value Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->civ;
                    Log (("ac97: CIV[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_LVI:
                case PO_LVI:
                case MC_LVI:
                    /* Last Valid Index Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->lvi;
                    Log (("ac97: LVI[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_PIV:
                case PO_PIV:
                case MC_PIV:
                    /* Prefetched Index Value Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->piv;
                    Log (("ac97: PIV[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_CR:
                case PO_CR:
                case MC_CR:
                    /* Control Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->cr;
                    Log (("ac97: CR[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register (lower part) */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->sr & 0xff;
                    Log (("ac97: SRb[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                default:
                    Log (("ac97: U nabm readb %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        case 2:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->sr;
                    Log (("ac97: SR[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_PICB:
                case PO_PICB:
                case MC_PICB:
                    /* Position in Current Buffer Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->picb;
                    Log (("ac97: PICB[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                default:
                    Log (("ac97: U nabm readw %#x -> %#x\n", Port, *pu32));
                    break;
            }
            break;
        }

        case 4:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            *pu32 = ~0U;

            switch (index)
            {
                case PI_BDBAR:
                case PO_BDBAR:
                case MC_BDBAR:
                    /* Buffer Descriptor Base Address Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->bdbar;
                    Log (("ac97: BMADDR[%d] -> %#x\n", GET_BM (index), *pu32));
                    break;
                case PI_CIV:
                case PO_CIV:
                case MC_CIV:
                    /* 32-bit access: Current Index Value Register +
                     *                Last Valid Index Register +
                     *                Status Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->civ | (r->lvi << 8) | (r->sr << 16);
                    Log (("ac97: CIV LVI SR[%d] -> %#x, %#x, %#x\n", GET_BM (index),
                                r->civ, r->lvi, r->sr));
                    break;
                case PI_PICB:
                case PO_PICB:
                case MC_PICB:
                    /* 32-bit access: Position in Current Buffer Register +
                     *                Prefetched Index Value Register +
                     *                Control Register */
                    r = &s->bm_regs[GET_BM (index)];
                    *pu32 = r->picb | (r->piv << 16) | (r->cr << 24);
                    Log (("ac97: PICB PIV CR[%d] -> %#x %#x %#x %#x\n", GET_BM (index),
                                *pu32, r->picb, r->piv, r->cr));
                    break;
                case GLOB_CNT:
                    /* Global Control */
                    *pu32 = s->glob_cnt;
                    Log (("ac97: glob_cnt -> %#x\n", *pu32));
                    break;
                case GLOB_STA:
                    /* Global Status */
                    *pu32 = s->glob_sta | GS_S0CR;
                    Log (("ac97: glob_sta -> %#x\n", *pu32));
                    break;
                default:
                    Log (("ac97: U nabm readl %#x -> %#x\n", Port, *pu32));
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
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) ichac97IOPortNABMWrite (PPDMDEVINS pDevIns, void *pvUser,
                                                 RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PCIAC97LinkState *d = (PCIAC97LinkState*)pvUser;
    AC97LinkState *s = &d->ac97;

    switch (cb)
    {
        case 1:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            switch (index)
            {
                case PI_LVI:
                case PO_LVI:
                case MC_LVI:
                    /* Last Valid Index */
                    r = &s->bm_regs[GET_BM (index)];
                    if ((r->cr & CR_RPBM) && (r->sr & SR_DCH)) {
                        r->sr &= ~(SR_DCH | SR_CELV);
                        r->civ = r->piv;
                        r->piv = (r->piv + 1) % 32;
                        fetch_bd (s, r);
                    }
                    r->lvi = u32 % 32;
                    Log (("ac97: LVI[%d] <- %#x\n", GET_BM (index), u32));
                    break;
                case PI_CR:
                case PO_CR:
                case MC_CR:
                    /* Control Register */
                    r = &s->bm_regs[GET_BM (index)];
                    if (u32 & CR_RR)
                        reset_bm_regs (s, r);
                    else
                    {
                        r->cr = u32 & CR_VALID_MASK;
                        if (!(r->cr & CR_RPBM))
                        {
                            voice_set_active (s, r - s->bm_regs, 0);
                            r->sr |= SR_DCH;
                        }
                        else
                        {
                            r->civ = r->piv;
                            r->piv = (r->piv + 1) % 32;
                            fetch_bd (s, r);
                            r->sr &= ~SR_DCH;
                            voice_set_active (s, r - s->bm_regs, 1);
                        }
                    }
                    Log (("ac97: CR[%d] <- %#x (cr %#x)\n", GET_BM (index), u32, r->cr));
                    break;
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    r = &s->bm_regs[GET_BM (index)];
                    r->sr |= u32 & ~(SR_RO_MASK | SR_WCLEAR_MASK);
                    update_sr (s, r, r->sr & ~(u32 & SR_WCLEAR_MASK));
                    Log (("ac97: SR[%d] <- %#x (sr %#x)\n", GET_BM (index), u32, r->sr));
                    break;
                default:
                    Log (("ac97: U nabm writeb %#x <- %#x\n", Port, u32));
                    break;
            }
            break;
        }

        case 2:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            switch (index)
            {
                case PI_SR:
                case PO_SR:
                case MC_SR:
                    /* Status Register */
                    r = &s->bm_regs[GET_BM (index)];
                    r->sr |= u32 & ~(SR_RO_MASK | SR_WCLEAR_MASK);
                    update_sr (s, r, r->sr & ~(u32 & SR_WCLEAR_MASK));
                    Log (("ac97: SR[%d] <- %#x (sr %#x)\n", GET_BM (index), u32, r->sr));
                    break;
                default:
                    Log (("ac97: U nabm writew %#x <- %#x\n", Port, u32));
                    break;
            }
            break;
        }

        case 4:
        {
            AC97BusMasterRegs *r = NULL;
            uint32_t index = Port - d->ac97.IOPortBase[1];
            switch (index)
            {
                case PI_BDBAR:
                case PO_BDBAR:
                case MC_BDBAR:
                    /* Buffer Descriptor list Base Address Register */
                    r = &s->bm_regs[GET_BM (index)];
                    r->bdbar = u32 & ~3;
                    Log (("ac97: BDBAR[%d] <- %#x (bdbar %#x)\n",
                                GET_BM (index), u32, r->bdbar));
                    break;
                case GLOB_CNT:
                    /* Global Control */
                    if (u32 & GC_WR)
                        warm_reset (s);
                    if (u32 & GC_CR)
                        cold_reset (s);
                    if (!(u32 & (GC_WR | GC_CR)))
                        s->glob_cnt = u32 & GC_VALID_MASK;
                    Log (("ac97: glob_cnt <- %#x (glob_cnt %#x)\n", u32, s->glob_cnt));
                    break;
                case GLOB_STA:
                    /* Global Status */
                    s->glob_sta &= ~(u32 & GS_WCLEAR_MASK);
                    s->glob_sta |= (u32 & ~(GS_WCLEAR_MASK | GS_RO_MASK)) & GS_VALID_MASK;
                    Log (("ac97: glob_sta <- %#x (glob_sta %#x)\n", u32, s->glob_sta));
                    break;
                default:
                    Log (("ac97: U nabm writel %#x <- %#x\n", Port, u32));
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
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) ichac97IOPortNAMRead (PPDMDEVINS pDevIns, void *pvUser,
                                               RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PCIAC97LinkState *d = (PCIAC97LinkState*)pvUser;
    AC97LinkState *s = &d->ac97;

    switch (cb)
    {
        case 1:
        {
            Log (("ac97: U nam readb %#x\n", Port));
            s->cas = 0;
            *pu32 = ~0U;
            break;
        }

        case 2:
        {
            uint32_t index = Port - d->ac97.IOPortBase[0];
            *pu32 = ~0U;
            s->cas = 0;
            switch (index)
            {
            default:
                *pu32 = mixer_load (s, index);
                Log (("ac97: nam readw %#x -> %#x\n", Port, *pu32));
                break;
            }
            break;
        }

        case 4:
        {
            Log (("ac97: U nam readl %#x\n", Port));
            s->cas = 0;
            *pu32 = ~0U;
            break;
        }

        default:
            return VERR_IOM_IOPORT_UNUSED;
    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) ichac97IOPortNAMWrite (PPDMDEVINS pDevIns, void *pvUser,
                                                RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PCIAC97LinkState *d = (PCIAC97LinkState*)pvUser;
    AC97LinkState *s = &d->ac97;

    switch (cb)
    {
        case 1:
        {
            Log (("ac97: U nam writeb %#x <- %#x\n", Port, u32));
            s->cas = 0;
            break;
        }

        case 2:
        {
            uint32_t index = Port - d->ac97.IOPortBase[0];
            s->cas = 0;
            switch (index)
            {
                case AC97_Reset:
                    mixer_reset (s);
                    break;
                case AC97_Powerdown_Ctrl_Stat:
                    u32 &= ~0xf;
                    u32 |= mixer_load (s, index) & 0xf;
                    mixer_store (s, index, u32);
                    break;
#ifdef USE_MIXER
                case AC97_Master_Volume_Mute:
                    set_volume (s, index, AUD_MIXER_VOLUME, u32);
                    break;
                case AC97_PCM_Out_Volume_Mute:
                    set_volume (s, index, AUD_MIXER_PCM, u32);
                    break;
                case AC97_Line_In_Volume_Mute:
                    set_volume (s, index, AUD_MIXER_LINE_IN, u32);
                    break;
                case AC97_Record_Select:
                    record_select (s, u32);
                    break;
#else
                case AC97_Master_Volume_Mute:
                case AC97_PCM_Out_Volume_Mute:
                case AC97_Line_In_Volume_Mute:
                case AC97_Record_Select:
                    mixer_store (s, index, u32);
                    break;
#endif
                case AC97_Vendor_ID1:
                case AC97_Vendor_ID2:
                    Log (("ac97: Attempt to write vendor ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_ID:
                    Log (("ac97: Attempt to write extended audio ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_Ctrl_Stat:
                    if (!(u32 & EACS_VRA))
                    {
                        mixer_store (s, AC97_PCM_Front_DAC_Rate, 0xbb80);
                        mixer_store (s, AC97_PCM_LR_ADC_Rate,    0xbb80);
                        open_voice (s, PI_INDEX, 48000);
                        open_voice (s, PO_INDEX, 48000);
                    }
                    if (!(u32 & EACS_VRM))
                    {
                        mixer_store (s, AC97_MIC_ADC_Rate, 0xbb80);
                        open_voice (s, MC_INDEX, 48000);
                    }
                    Log (("ac97: Setting extended audio control to %#x\n", u32));
                    mixer_store (s, AC97_Extended_Audio_Ctrl_Stat, u32);
                    break;
                case AC97_PCM_Front_DAC_Rate:
                    if (mixer_load (s, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRA)
                    {
                        mixer_store (s, index, u32);
                        Log(("ac97: Set front DAC rate to %d\n", u32));
                        open_voice (s, PO_INDEX, u32);
                    }
                    else
                    {
                        Log (("ac97: Attempt to set front DAC rate to %d, "
                                    "but VRA is not set\n",
                                    u32));
                    }
                    break;
                case AC97_MIC_ADC_Rate:
                    if (mixer_load (s, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRM)
                    {
                        mixer_store (s, index, u32);
                        Log (("ac97: Set MIC ADC rate to %d\n", u32));
                        open_voice (s, MC_INDEX, u32);
                    }
                    else
                    {
                        Log (("ac97: Attempt to set MIC ADC rate to %d, "
                                    "but VRM is not set\n",
                                    u32));
                    }
                    break;
                case AC97_PCM_LR_ADC_Rate:
                    if (mixer_load (s, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRA)
                    {
                        mixer_store (s, index, u32);
                        Log (("ac97: Set front LR ADC rate to %d\n", u32));
                        open_voice (s, PI_INDEX, u32);
                    }
                    else
                    {
                        Log (("ac97: Attempt to set LR ADC rate to %d, but VRA is not set\n",
                                    u32));
                    }
                    break;
                default:
                    Log (("ac97: U nam writew %#x <- %#x\n", Port, u32));
                    mixer_store (s, index, u32);
                    break;
            }
            break;
        }

        case 4:
        {
            Log (("ac97: U nam writel %#x <- %#x\n", Port, u32));
            s->cas = 0;
            break;
        }

        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", Port, cb, u32));
            break;
    }
    return VINF_SUCCESS;
}

/**
 * Callback function for mapping a PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device.
 *                          Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region.
 *                          If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative
 *                          to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) ichac97IOPortMap (PPCIDEVICE pPciDev, int iRegion,
                                           RTGCPHYS GCPhysAddress, uint32_t cb,
                                           PCIADDRESSSPACE enmType)
{
    int         rc;
    PPDMDEVINS  pDevIns = pPciDev->pDevIns;
    RTIOPORT    Port = (RTIOPORT)GCPhysAddress;
    PCIAC97LinkState *pData = PCIDEV_2_ICHAC97STATE(pPciDev);

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(cb >= 0x20);

    if (iRegion == 0)
        rc = PDMDevHlpIOPortRegister (pDevIns, Port, 256, pData,
                                      ichac97IOPortNAMWrite, ichac97IOPortNAMRead,
                                      NULL, NULL, "ICHAC97 NAM");
    else
        rc = PDMDevHlpIOPortRegister (pDevIns, Port, 64, pData,
                                      ichac97IOPortNABMWrite, ichac97IOPortNABMRead,
                                      NULL, NULL, "ICHAC97 NABM");
    if (VBOX_FAILURE(rc))
        return rc;

    pData->ac97.IOPortBase[iRegion] = Port;
    return VINF_SUCCESS;
}

/**
 * Saves a state of the AC'97 device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) ichac97SaveExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    PCIAC97LinkState *pData = PDMINS2DATA(pDevIns, PCIAC97LinkState *);
    size_t  i;
    uint8_t active[LAST_INDEX];
    AC97LinkState *s = &pData->ac97;

    SSMR3PutU32 (pSSMHandle, s->glob_cnt);
    SSMR3PutU32 (pSSMHandle, s->glob_sta);
    SSMR3PutU32 (pSSMHandle, s->cas);

    for (i = 0; i < sizeof (s->bm_regs) / sizeof (s->bm_regs[0]); ++i)
    {
        AC97BusMasterRegs *r = &s->bm_regs[i];
        SSMR3PutU32 (pSSMHandle, r->bdbar);
        SSMR3PutU8  (pSSMHandle, r->civ);
        SSMR3PutU8  (pSSMHandle, r->lvi);
        SSMR3PutU16 (pSSMHandle, r->sr);
        SSMR3PutU16 (pSSMHandle, r->picb);
        SSMR3PutU8  (pSSMHandle, r->piv);
        SSMR3PutU8  (pSSMHandle, r->cr);
        SSMR3PutS32 (pSSMHandle, r->bd_valid);
        SSMR3PutU32 (pSSMHandle, r->bd.addr);
        SSMR3PutU32 (pSSMHandle, r->bd.ctl_len);
    }
    SSMR3PutMem (pSSMHandle, s->mixer_data, sizeof (s->mixer_data));

    active[PI_INDEX] = AUD_is_active_in  (s->voice_pi) ? 1 : 0;
    active[PO_INDEX] = AUD_is_active_out (s->voice_po) ? 1 : 0;
    active[MC_INDEX] = AUD_is_active_in  (s->voice_mc) ? 1 : 0;
    SSMR3PutMem (pSSMHandle, active, sizeof (active));

    return VINF_SUCCESS;
}

/**
 * Loads a saved AC'97 device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) ichac97LoadExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle,
                                          uint32_t u32Version)
{
    PCIAC97LinkState *pData = PDMINS2DATA(pDevIns, PCIAC97LinkState *);
    size_t  i;
    uint8_t active[LAST_INDEX];
    AC97LinkState *s = &pData->ac97;

    if (u32Version != AC97_SSM_VERSION)
    {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    SSMR3GetU32 (pSSMHandle, &s->glob_cnt);
    SSMR3GetU32 (pSSMHandle, &s->glob_sta);
    SSMR3GetU32 (pSSMHandle, &s->cas);

    for (i = 0; i < sizeof (s->bm_regs) / sizeof (s->bm_regs[0]); ++i)
    {
        AC97BusMasterRegs *r = &s->bm_regs[i];
        SSMR3GetU32 (pSSMHandle, &r->bdbar);
        SSMR3GetU8  (pSSMHandle, &r->civ);
        SSMR3GetU8  (pSSMHandle, &r->lvi);
        SSMR3GetU16 (pSSMHandle, &r->sr);
        SSMR3GetU16 (pSSMHandle, &r->picb);
        SSMR3GetU8  (pSSMHandle, &r->piv);
        SSMR3GetU8  (pSSMHandle, &r->cr);
        SSMR3GetS32 (pSSMHandle, &r->bd_valid);
        SSMR3GetU32 (pSSMHandle, &r->bd.addr);
        SSMR3GetU32 (pSSMHandle, &r->bd.ctl_len);
    }
    SSMR3GetMem (pSSMHandle, s->mixer_data, sizeof (s->mixer_data));
    SSMR3GetMem (pSSMHandle, active, sizeof (active));

#ifdef USE_MIXER
    record_select (s, mixer_load (s, AC97_Record_Select));
#define V_(a, b) set_volume (s, a, b, mixer_load (s, a))
    V_ (AC97_Master_Volume_Mute,  AUD_MIXER_VOLUME);
    V_ (AC97_PCM_Out_Volume_Mute, AUD_MIXER_PCM);
    V_ (AC97_Line_In_Volume_Mute, AUD_MIXER_LINE_IN);
#undef V_
#endif
    reset_voices (s, active);

    s->bup_flag = 0;
    s->last_samp = 0;

    return VINF_SUCCESS;
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *
 * @remark  The original sources didn't install a reset handler, but it seems to
 *          make sense to me so we'll do it.
 */
static DECLCALLBACK(void)  ac97Reset(PPDMDEVINS pDevIns)
{
    PCIAC97LinkState *pData = PDMINS2DATA(pDevIns, PCIAC97LinkState *);

    /*
     * Reset the device state (will need pDrv later).
     */
    reset_bm_regs (&pData->ac97, &pData->ac97.bm_regs[0]);
    reset_bm_regs (&pData->ac97, &pData->ac97.bm_regs[1]);
    reset_bm_regs (&pData->ac97, &pData->ac97.bm_regs[2]);

    /*
     * Reset the mixer too. The Windows XP driver seems to rely on
     * this. At least it wants to read the vendor id before it resets
     * the codec manually.
     */
    mixer_reset(&pData->ac97);
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) ichac97QueryInterface (struct PDMIBASE *pInterface,
                                                   PDMINTERFACE enmInterface)
{
    PCIAC97LinkState *pData =
        (PCIAC97LinkState *)((uintptr_t)pInterface
                             - RT_OFFSETOF(PCIAC97LinkState, ac97.IBase));
    Assert(&pData->ac97.IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->ac97.IBase;
        default:
            return NULL;
    }
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed,
 *                      pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which
 *                      registers and such to use.
 *                      The device number is also found in pDevIns->iInstance,
 *                      but since it's likely to be freqently used PDM passes
 *                      it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device.
 *                      Use this to obtain the configuration
 *                      of the device instance. It's also found in
 *                      pDevIns->pCfgHandle, but like iInstance it's expected
 *                      to be used a bit in this function.
 */
static DECLCALLBACK(int) ichac97Construct (PPDMDEVINS pDevIns, int iInstance,
                                           PCFGMNODE pCfgHandle)
{
    PCIAC97LinkState *pData = PDMINS2DATA(pDevIns, PCIAC97LinkState *);
    AC97LinkState    *s     = &pData->ac97;
    int               rc;

    Assert(iInstance == 0);

    /*
     * Initialize data (most of it anyway).
     */
    s->pDevIns                  = pDevIns;
    /* IBase */
    s->IBase.pfnQueryInterface  = ichac97QueryInterface;

    /* PCI Device */
    pData->dev.config[0x00] = 0x86;      /* vid vendor id intel ro */
    pData->dev.config[0x01] = 0x80;      /* intel */

    pData->dev.config[0x02] = 0x15;      /* did device id 82801 ro */
    pData->dev.config[0x03] = 0x24;      /* 82801aa */

    pData->dev.config[0x04] = 0x00;      /* pcicmd pci command rw, ro */
    pData->dev.config[0x05] = 0x00;

    pData->dev.config[0x06] = 0x80;      /* pcists pci status rwc, ro */
    pData->dev.config[0x07] = 0x02;

    pData->dev.config[0x08] = 0x01;      /* rid revision ro */
    pData->dev.config[0x09] = 0x00;      /* pi programming interface ro */
    pData->dev.config[0x0a] = 0x01;      /* scc sub class code ro */
    pData->dev.config[0x0b] = 0x04;      /* bcc base class code ro */
    pData->dev.config[0x0e] = 0x00;      /* headtyp header type ro */

    pData->dev.config[0x10] = 0x01;      /* nambar native audio mixer base
                                          * address rw */
    pData->dev.config[0x11] = 0x00;
    pData->dev.config[0x12] = 0x00;
    pData->dev.config[0x13] = 0x00;

    pData->dev.config[0x14] = 0x01;      /* nabmbar native audio bus mastering
                                          * base address rw */
    pData->dev.config[0x15] = 0x00;
    pData->dev.config[0x16] = 0x00;
    pData->dev.config[0x17] = 0x00;

    pData->dev.config[0x2c] = 0x86;      /* svid subsystem vendor id rwo */
    pData->dev.config[0x2d] = 0x80;

    pData->dev.config[0x2e] = 0x00;      /* sid subsystem id rwo */
    pData->dev.config[0x2f] = 0x00;

    pData->dev.config[0x3c] = 0x00;      /* intr_ln interrupt line rw */
    pData->dev.config[0x3d] = 0x01;      /* intr_pn interrupt pin ro */

    /*
     * Register the PCI device, it's I/O regions, the timer and the
     * saved state item.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pData->dev);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister (pDevIns, 0, 256, PCI_ADDRESS_SPACE_IO,
                                       ichac97IOPortMap);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister (pDevIns, 1, 64, PCI_ADDRESS_SPACE_IO,
                                       ichac97IOPortMap);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = PDMDevHlpSSMRegister (pDevIns, pDevIns->pDevReg->szDeviceName,
                               iInstance, AC97_SSM_VERSION, sizeof(*pData),
                               NULL, ichac97SaveExec, NULL,
                               NULL, ichac97LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Attach driver.
     */
    rc = PDMDevHlpDriverAttach (pDevIns, 0, &s->IBase,
                                &s->pDrvBase, "Audio Driver Port");
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log (("ac97: No attached driver!\n"));
    else if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to attach AC97 LUN #0! rc=%Vrc\n", rc));
        return rc;
    }

    AUD_register_card ("ICH0", &s->card);

    ac97Reset(pDevIns);

#ifndef RT_OS_DARWIN /* coreaudio doesn't supply these. */
    if (!s->voice_pi)
        LogRel(("AC97: WARNING: Unable to open PCM IN!\n"));
    if (!s->voice_mc)
        LogRel(("AC97: WARNING: Unable to open PCM MC!\n"));
#endif
    if (!s->voice_po)
        LogRel(("AC97: WARNING: Unable to open PCM OUT!\n"));

    if (!s->voice_pi && !s->voice_po && !s->voice_mc)
    {
        /* Was not able initialize *any* voice. Select the NULL audio driver instead */
        AUD_close_in  (&s->card, s->voice_pi);
        AUD_close_out (&s->card, s->voice_po);
        AUD_close_in  (&s->card, s->voice_mc);
        s->voice_po = NULL;
        s->voice_pi = NULL;
        s->voice_mc = NULL;
        AUD_init_null();
        ac97Reset(pDevIns);

        PDMDevHlpVMSetRuntimeError(pDevIns, false, "HostAudioNotResponding",
             N_("No audio devices could not be opened. Selecting the NULL audio backend "
                "with the consequence that no sound is audible."));
    }
#ifndef RT_OS_DARWIN
    else if (!s->voice_pi || !s->voice_po || !s->voice_mc)
    {
        char   szMissingVoices[128];
        size_t len = 0;
        bool   fComma = false;
        if (!s->voice_pi)
        {
            len = RTStrPrintf(szMissingVoices, sizeof(szMissingVoices), "PCM_in");
            fComma = true;
        }
        if (!s->voice_po)
        {
            len = RTStrPrintf(szMissingVoices + len, sizeof(szMissingVoices)-len, "%sPCM_out",
                              fComma ? ", " : "");
            fComma = true;
        }
        if (!s->voice_mc)
        {
            len = RTStrPrintf(szMissingVoices + len, sizeof(szMissingVoices)-len, "%sPCM_mic",
                              fComma ? ", " : "");
        }

        PDMDevHlpVMSetRuntimeError(pDevIns, false, "HostAudioNotResponding",
             N_("Some audio devices (%s) could not be opened. Guest applications generating audio "
                "output or depending on audio input may hang. Make sure your host audio device "
                "is working properly. Check the logfile for error messages of the audio "
                "subsystem."), szMissingVoices);
    }
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
    /* szDeviceName */
    "ichac97",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "ICH AC'97 Audio Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIAC97LinkState),
    /* pfnConstruct */
    ichac97Construct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
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
    NULL
};
