/* $Id$ */
/** @file
 * DevSB16 - VBox SB16 Audio Controller.
 *
 * @todo hiccups on NT4 and Win98.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: sb16.c from QEMU AUDIO subsystem (r3917).
 * QEMU Soundblaster 16 emulation
 *
 * Copyright (c) 2003-2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "vl_vbox.h"

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
# include "AudioMixer.h"
#else
 extern "C" {
  #include "audio.h"
 }
#endif

#ifndef VBOX
#define LENOFA(a) ((int) (sizeof(a)/sizeof(a[0])))
#else /* VBOX */
/** Current saved state version. */
#define SB16_SAVE_STATE_VERSION         2
/** The version used in VirtualBox version 3.0 and earlier. This didn't include
 * the config dump. */
#define SB16_SAVE_STATE_VERSION_VBOX_30 1
#endif /* VBOX */

#ifndef VBOX
#define IO_READ_PROTO(name)                             \
    uint32_t name (void *opaque, uint32_t nport)
#define IO_WRITE_PROTO(name)                                    \
    void name (void *opaque, uint32_t nport, uint32_t val)
#else  /* VBOX */
#define IO_READ_PROTO(name)                                             \
    DECLCALLBACK(int) name (PPDMDEVINS pDevIns, void *opaque,       \
                            RTIOPORT nport, uint32_t *pu32, unsigned cb)

#define IO_WRITE_PROTO(name)                                            \
    DECLCALLBACK(int) name (PPDMDEVINS pDevIns, void *opaque,       \
                            RTIOPORT nport, uint32_t val, unsigned cb)
#endif /* VBOX */

static const char e3[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

#ifndef VBOX
static struct {
    int ver_lo;
    int ver_hi;
    int irq;
    int dma;
    int hdma;
    int port;
} conf = {5, 4, 5, 1, 5, 0x220};
#endif /* !VBOX */

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
typedef struct SB16OUTPUTSTREAM
{
    /** PCM output stream. */
    R3PTRTYPE(PPDMAUDIOGSTSTRMOUT)     pStrmOut;
    /** Mixer handle for output stream. */
    R3PTRTYPE(PAUDMIXSTREAM)           phStrmOut;
} SB16OUTPUTSTREAM, *PSB16OUTPUTSTREAM;

/**
 * Struct for maintaining a host backend driver.
 */
typedef struct SB16STATE *PSB16STATE;
typedef struct SB16DRIVER
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

    /** Pointer to SB16 controller (state). */
    R3PTRTYPE(PSB16STATE)              pSB16State;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                   Flags;
    uint32_t                           PaddingFlags;
    /** LUN # to which this driver has been assigned. */
    uint8_t                            uLUN;
    uint8_t                            Padding[5];
    /** Audio connector interface to the underlying
     *  host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pConnector;
    /** Stream for output. */
    SB16OUTPUTSTREAM                   Out;
} SB16DRIVER, *PSB16DRIVER;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

typedef struct SB16STATE
{
#ifdef VBOX
    /** Pointer to the device instance. */
    PPDMDEVINSR3        pDevIns;
    /** Pointer to the connector of the attached audio driver. */
    PPDMIAUDIOCONNECTOR pDrv;
    int irqCfg;
    int dmaCfg;
    int hdmaCfg;
    int portCfg;
    int verCfg;
#endif
    int irq;
    int dma;
    int hdma;
    int port;
    int ver;

    int in_index;
    int out_data_len;
    int fmt_stereo;
    int fmt_signed;
    int fmt_bits;
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PDMAUDIOFMT fmt;
#else
    audfmt_e fmt;
    QEMUSoundCard card;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    int dma_auto;
    int block_size;
    int fifo;
    int freq;
    int time_const;
    int speaker;
    int needed_bytes;
    int cmd;
    int use_hdma;
    int highspeed;
    int can_write; /** @todo Value never gets 0? */

    int v2x6;

    uint8_t csp_param;
    uint8_t csp_value;
    uint8_t csp_mode;
    uint8_t csp_regs[256];
    uint8_t csp_index;
    uint8_t csp_reg83[4];
    int csp_reg83r;
    int csp_reg83w;

    uint8_t in2_data[10];
    uint8_t out_data[50];
    uint8_t test_reg;
    uint8_t last_read_byte;
    int nzero;

    int left_till_irq;

    int dma_running;
    int bytes_per_second;
    int align;
    uint32_t audio_free;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTLISTANCHOR                   lstDrv;
    /** The device' software mixer. */
    R3PTRTYPE(PAUDIOMIXER)         pMixer;
    /** Audio sink for PCM output. */
    R3PTRTYPE(PAUDMIXSINK)         pSinkOutput;
    /** The emulation timer for handling I/O of the attached LUN drivers. */
    PTMTIMERR3                     pTimerIO;
    /** Timer ticks for handling the LUN drivers. */
    uint64_t                       uTicksIO;
#else
    SWVoiceOut *voice;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    PTMTIMER  pTimerIRQ;
    PPDMIBASE pDrvBase;
    /** LUN\#0: Base interface. */
    PDMIBASE  IBase;

    /* mixer state */
    int mixer_nreg;
    uint8_t mixer_regs[256];
} SB16STATE, *PSB16STATE;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static int sb16OpenOut(PSB16STATE pThis, PPDMAUDIOSTREAMCFG pCfg);
#endif

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
static DECLCALLBACK(int) sb16Attach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    PSB16STATE pThis = PDMINS_2_DATA(pDevIns, PSB16STATE);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("AC'97 device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /*
     * Attach driver.
     */
    char *pszDesc = NULL;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (SB16) for LUN #%u", uLUN) <= 0)
        AssertMsgReturn(pszDesc,
                        ("Not enough memory for SB16 driver port description of LUN #%u\n", uLUN),
                        VERR_NO_MEMORY);

    int rc = PDMDevHlpDriverAttach(pDevIns, uLUN,
                                   &pThis->IBase, &pThis->pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        PSB16DRIVER pDrv = (PSB16DRIVER)RTMemAllocZ(sizeof(SB16DRIVER));
        if (pDrv)
        {
            pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL,
                      ("Configuration error: LUN #%u has no host audio interface, rc=%Rrc\n",
                      uLUN, rc));
            pDrv->pSB16State = pThis;
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
        AssertMsgFailed(("Failed to attach SB16 LUN #%u (\"%s\"), rc=%Rrc\n",
                        uLUN, pszDesc, rc));

    RTStrFree(pszDesc);

    LogFunc(("iLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
static void sb16AudioCallback(void *pvContext, uint32_t cbFree);
#else
static void sb16AudioCallback(void *pvContext, int cbFree);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

static int magic_of_irq(int irq)
{
    switch (irq)
    {
        case 5:
            return 2;
        case 7:
            return 4;
        case 9:
            return 1;
        case 10:
            return 8;
        default:
            break;
    }

    LogFlowFunc(("bad irq %d\n", irq));
    return 2;
}

static int irq_of_magic(int magic)
{
    switch (magic)
    {
        case 1:
            return 9;
        case 2:
            return 5;
        case 4:
            return 7;
        case 8:
            return 10;
        default:
            break;
    }

    LogFlowFunc(("bad irq magic %d\n", magic));
    return -1;
}

#ifdef DEBUG
static inline void log_dsp(PSB16STATE pThis)
{
    LogFlowFunc(("%s:%s:%d:%s:dmasize=%d:freq=%d:const=%d:speaker=%d\n",
                 pThis->fmt_stereo ? "Stereo" : "Mono",
                 pThis->fmt_signed ? "Signed" : "Unsigned",
                 pThis->fmt_bits,
                 pThis->dma_auto ? "Auto" : "Single",
                 pThis->block_size,
                 pThis->freq,
                 pThis->time_const,
                 pThis->speaker));
}
#endif

static void sb16SpeakerControl(PSB16STATE pThis, int on)
{
    pThis->speaker = on;
    /* AUD_enable (pThis->voice, on); */
}

static void sb16Control(PSB16STATE pThis, int hold)
{
    int dma = pThis->use_hdma ? pThis->hdma : pThis->dma;
    pThis->dma_running = hold;

    LogFlowFunc(("hold %d high %d dma %d\n", hold, pThis->use_hdma, dma));

#ifndef VBOX
    if (hold) {
        DMA_hold_DREQ (dma);
        AUD_set_active_out (pThis->voice, 1);
    }
    else {
        DMA_release_DREQ (dma);
        AUD_set_active_out (pThis->voice, 0);
    }
#else  /* VBOX */
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PSB16DRIVER pDrv;
# endif
    if (hold)
    {
        PDMDevHlpDMASetDREQ (pThis->pDevIns, dma, 1);
        PDMDevHlpDMASchedule (pThis->pDevIns);
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
            pDrv->pConnector->pfnEnableOut(pDrv->pConnector,
                                           pDrv->Out.pStrmOut, true /* fEnable */);
# else
        AUD_set_active_out (pThis->voice, 1);
# endif
    }
    else
    {
        PDMDevHlpDMASetDREQ (pThis->pDevIns, dma, 0);
# ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
            pDrv->pConnector->pfnEnableOut(pDrv->pConnector,
                                           pDrv->Out.pStrmOut, false /* fEnable */);
# else
        AUD_set_active_out (pThis->voice, 0);
# endif
    }
#endif /* VBOX */
}

#ifndef VBOX
static void aux_timer (void *opaque)
{
    PSB16STATE pThis = opaque;
    pThis->can_write = 1;
    qemu_irq_raise (pThis->pic[pThis->irq]);
}
#else  /* VBOX */
static DECLCALLBACK(void) sb16TimerIRQ(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvThis)
{
    PSB16STATE pThis = (PSB16STATE)pvThis;
    pThis->can_write = 1;
    PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 1);
}
#endif /* VBOX */

#define DMA8_AUTO 1
#define DMA8_HIGH 2

static void continue_dma8(PSB16STATE pThis)
{
    if (pThis->freq > 0)
    {
        pThis->audio_free = 0;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = pThis->freq;
        streamCfg.cChannels     = 1 << pThis->fmt_stereo;
        streamCfg.enmFormat     = pThis->fmt;
        streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

        int rc = sb16OpenOut(pThis, &streamCfg);
        AssertRC(rc);
#else
        audsettings_t streamCfg;
        streamCfg.freq = pThis->freq;
        streamCfg.nchannels = 1 << pThis->fmt_stereo;
        streamCfg.fmt = pThis->fmt;
        streamCfg.endianness = 0;
        pThis->voice = AUD_open_out (
            &pThis->card,
            pThis->voice,
            "sb16",
            pThis,
            sb16AudioCallback,
            &streamCfg
            );
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }

    sb16Control(pThis, 1);
}

static void dma_cmd8(PSB16STATE pThis, int mask, int dma_len)
{
    pThis->fmt        = AUD_FMT_U8;
    pThis->use_hdma   = 0;
    pThis->fmt_bits   = 8;
    pThis->fmt_signed = 0;
    pThis->fmt_stereo = (pThis->mixer_regs[0x0e] & 2) != 0;

    if (-1 == pThis->time_const)
    {
        if (pThis->freq <= 0)
            pThis->freq = 11025;
    }
    else
    {
        int tmp = (256 - pThis->time_const);
        pThis->freq = (1000000 + (tmp / 2)) / tmp;
    }

    if (dma_len != -1)
    {
        pThis->block_size = dma_len << pThis->fmt_stereo;
    }
    else
    {
        /* This is apparently the only way to make both Act1/PL
           and SecondReality/FC work

           r=andy Wow, actually someone who remembers Future Crew :-)

           Act1 sets block size via command 0x48 and it's an odd number
           SR does the same with even number
           Both use stereo, and Creatives own documentation states that
           0x48 sets block size in bytes less one.. go figure */
        pThis->block_size &= ~pThis->fmt_stereo;
    }

    pThis->freq >>= pThis->fmt_stereo;
    pThis->left_till_irq = pThis->block_size;
    pThis->bytes_per_second = (pThis->freq << pThis->fmt_stereo);
    /* pThis->highspeed = (mask & DMA8_HIGH) != 0; */
    pThis->dma_auto = (mask & DMA8_AUTO) != 0;
    pThis->align = (1 << pThis->fmt_stereo) - 1;

    if (pThis->block_size & pThis->align) {
        LogFlowFunc(("warning: misaligned block size %d, alignment %d\n",
               pThis->block_size, pThis->align + 1));
    }

    LogFlowFunc(("freq %d, stereo %d, sign %d, bits %d, "
            "dma %d, auto %d, fifo %d, high %d\n",
            pThis->freq, pThis->fmt_stereo, pThis->fmt_signed, pThis->fmt_bits,
            pThis->block_size, pThis->dma_auto, pThis->fifo, pThis->highspeed));

    continue_dma8 (pThis);
    sb16SpeakerControl(pThis, 1);
}

static void dma_cmd (PSB16STATE pThis, uint8_t cmd, uint8_t d0, int dma_len)
{
    pThis->use_hdma   = cmd < 0xc0;
    pThis->fifo       = (cmd >> 1) & 1;
    pThis->dma_auto   = (cmd >> 2) & 1;
    pThis->fmt_signed = (d0 >> 4) & 1;
    pThis->fmt_stereo = (d0 >> 5) & 1;

    switch (cmd >> 4)
    {
        case 11:
            pThis->fmt_bits = 16;
            break;

        case 12:
            pThis->fmt_bits = 8;
            break;
    }

    if (-1 != pThis->time_const)
    {
#if 1
        int tmp = 256 - pThis->time_const;
        pThis->freq = (1000000 + (tmp / 2)) / tmp;
#else
        /* pThis->freq = 1000000 / ((255 - pThis->time_const) << pThis->fmt_stereo); */
        pThis->freq = 1000000 / ((255 - pThis->time_const));
#endif
        pThis->time_const = -1;
    }

    pThis->block_size = dma_len + 1;
    pThis->block_size <<= ((pThis->fmt_bits == 16) ? 1 : 0);
    if (!pThis->dma_auto)
    {
        /* It is clear that for DOOM and auto-init this value
           shouldn't take stereo into account, while Miles Sound Systems
           setsound.exe with single transfer mode wouldn't work without it
           wonders of SB16 yet again */
        pThis->block_size <<= pThis->fmt_stereo;
    }

    LogFlowFunc(("freq %d, stereo %d, sign %d, bits %d, dma %d, auto %d, fifo %d, high %d\n",
                 pThis->freq, pThis->fmt_stereo, pThis->fmt_signed, pThis->fmt_bits,
                 pThis->block_size, pThis->dma_auto, pThis->fifo, pThis->highspeed));

    if (16 == pThis->fmt_bits)
        pThis->fmt = pThis->fmt_signed ? AUD_FMT_S16 : AUD_FMT_U16;
    else
        pThis->fmt = pThis->fmt_signed ? AUD_FMT_S8 : AUD_FMT_U8;

    pThis->left_till_irq = pThis->block_size;

    pThis->bytes_per_second = (pThis->freq << pThis->fmt_stereo) << ((pThis->fmt_bits == 16) ? 1 : 0);
    pThis->highspeed = 0;
    pThis->align = (1 << (pThis->fmt_stereo + (pThis->fmt_bits == 16))) - 1;
    if (pThis->block_size & pThis->align)
    {
        LogFlowFunc(("warning: misaligned block size %d, alignment %d\n",
                     pThis->block_size, pThis->align + 1));
    }

    if (pThis->freq)
    {
        pThis->audio_free = 0;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = pThis->freq;
        streamCfg.cChannels     = 1 << pThis->fmt_stereo;
        streamCfg.enmFormat     = pThis->fmt;
        streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

        int rc = sb16OpenOut(pThis, &streamCfg);
        AssertRC(rc);
#else
        audsettings_t streamCfg;
        streamCfg.freq = pThis->freq;
        streamCfg.nchannels = 1 << pThis->fmt_stereo;
        streamCfg.fmt = pThis->fmt;
        streamCfg.endianness = 0;
        pThis->voice = AUD_open_out (
            &pThis->card,
            pThis->voice,
            "sb16",
            pThis,
            sb16AudioCallback,
            &streamCfg
            );
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }

    sb16Control(pThis, 1);
    sb16SpeakerControl(pThis, 1);
}

static inline void dsp_out_data (PSB16STATE pThis, uint8_t val)
{
    LogFlowFunc(("outdata %#x\n", val));
    if ((size_t) pThis->out_data_len < sizeof (pThis->out_data)) {
        pThis->out_data[pThis->out_data_len++] = val;
    }
}

static inline uint8_t dsp_get_data (PSB16STATE pThis)
{
    if (pThis->in_index) {
        return pThis->in2_data[--pThis->in_index];
    }
    else {
        LogFlowFunc(("buffer underflow\n"));
        return 0;
    }
}

static void command (PSB16STATE pThis, uint8_t cmd)
{
    LogFlowFunc(("command %#x\n", cmd));

    if (cmd > 0xaf && cmd < 0xd0) {
        if (cmd & 8) {
            LogFlowFunc(("ADC not yet supported (command %#x)\n", cmd));
        }

        switch (cmd >> 4) {
        case 11:
        case 12:
            break;
        default:
            LogFlowFunc(("%#x wrong bits\n", cmd));
        }
        pThis->needed_bytes = 3;
    }
    else {
        pThis->needed_bytes = 0;

        switch (cmd) {
        case 0x03:
            dsp_out_data (pThis, 0x10); /* pThis->csp_param); */
            goto warn;

        case 0x04:
            pThis->needed_bytes = 1;
            goto warn;

        case 0x05:
            pThis->needed_bytes = 2;
            goto warn;

        case 0x08:
            /* __asm__ ("int3"); */
            goto warn;

        case 0x0e:
            pThis->needed_bytes = 2;
            goto warn;

        case 0x09:
            dsp_out_data (pThis, 0xf8);
            goto warn;

        case 0x0f:
            pThis->needed_bytes = 1;
            goto warn;

        case 0x10:
            pThis->needed_bytes = 1;
            goto warn;

        case 0x14:
            pThis->needed_bytes = 2;
            pThis->block_size = 0;
            break;

        case 0x1c:              /* Auto-Initialize DMA DAC, 8-bit */
            dma_cmd8 (pThis, DMA8_AUTO, -1);
            break;

        case 0x20:              /* Direct ADC, Juice/PL */
            dsp_out_data (pThis, 0xff);
            goto warn;

        case 0x35:
            LogFlowFunc(("0x35 - MIDI command not implemented\n"));
            break;

        case 0x40:
            pThis->freq = -1;
            pThis->time_const = -1;
            pThis->needed_bytes = 1;
            break;

        case 0x41:
            pThis->freq = -1;
            pThis->time_const = -1;
            pThis->needed_bytes = 2;
            break;

        case 0x42:
            pThis->freq = -1;
            pThis->time_const = -1;
            pThis->needed_bytes = 2;
            goto warn;

        case 0x45:
            dsp_out_data (pThis, 0xaa);
            goto warn;

        case 0x47:                /* Continue Auto-Initialize DMA 16bit */
            break;

        case 0x48:
            pThis->needed_bytes = 2;
            break;

        case 0x74:
            pThis->needed_bytes = 2; /* DMA DAC, 4-bit ADPCM */
            LogFlowFunc(("0x75 - DMA DAC, 4-bit ADPCM not implemented\n"));
            break;

        case 0x75:              /* DMA DAC, 4-bit ADPCM Reference */
            pThis->needed_bytes = 2;
            LogFlowFunc(("0x74 - DMA DAC, 4-bit ADPCM Reference not implemented\n"));
            break;

        case 0x76:              /* DMA DAC, 2.6-bit ADPCM */
            pThis->needed_bytes = 2;
            LogFlowFunc(("0x74 - DMA DAC, 2.6-bit ADPCM not implemented\n"));
            break;

        case 0x77:              /* DMA DAC, 2.6-bit ADPCM Reference */
            pThis->needed_bytes = 2;
            LogFlowFunc(("0x74 - DMA DAC, 2.6-bit ADPCM Reference not implemented\n"));
            break;

        case 0x7d:
            LogFlowFunc(("0x7d - Autio-Initialize DMA DAC, 4-bit ADPCM Reference\n"));
            LogFlowFunc(("not implemented\n"));
            break;

        case 0x7f:
            LogFlowFunc(("0x7d - Autio-Initialize DMA DAC, 2.6-bit ADPCM Reference\n"));
            LogFlowFunc(("not implemented\n"));
            break;

        case 0x80:
            pThis->needed_bytes = 2;
            break;

        case 0x90:
        case 0x91:
            dma_cmd8(pThis, (((cmd & 1) == 0) ? 1 : 0) | DMA8_HIGH, -1);
            break;

        case 0xd0:              /* halt DMA operation. 8bit */
            sb16Control(pThis, 0);
            break;

        case 0xd1:              /* speaker on */
            sb16SpeakerControl(pThis, 1);
            break;

        case 0xd3:              /* speaker off */
            sb16SpeakerControl(pThis, 0);
            break;

        case 0xd4:              /* continue DMA operation. 8bit */
            /* KQ6 (or maybe Sierras audblst.drv in general) resets
               the frequency between halt/continue */
            continue_dma8 (pThis);
            break;

        case 0xd5:              /* halt DMA operation. 16bit */
            sb16Control(pThis, 0);
            break;

        case 0xd6:              /* continue DMA operation. 16bit */
            sb16Control(pThis, 1);
            break;

        case 0xd9:              /* exit auto-init DMA after this block. 16bit */
            pThis->dma_auto = 0;
            break;

        case 0xda:              /* exit auto-init DMA after this block. 8bit */
            pThis->dma_auto = 0;
            break;

        case 0xe0:              /* DSP identification */
            pThis->needed_bytes = 1;
            break;

        case 0xe1:
            dsp_out_data (pThis, pThis->ver & 0xff);
            dsp_out_data (pThis, pThis->ver >> 8);
            break;

        case 0xe2:
            pThis->needed_bytes = 1;
            goto warn;

        case 0xe3:
            {
                int i;
                for (i = sizeof (e3) - 1; i >= 0; --i)
                    dsp_out_data (pThis, e3[i]);
            }
            break;

        case 0xe4:              /* write test reg */
            pThis->needed_bytes = 1;
            break;

        case 0xe7:
            LogFlowFunc(("Attempt to probe for ESS (0xe7)?\n"));
            break;

        case 0xe8:              /* read test reg */
            dsp_out_data (pThis, pThis->test_reg);
            break;

        case 0xf2:
        case 0xf3:
            dsp_out_data (pThis, 0xaa);
            pThis->mixer_regs[0x82] |= (cmd == 0xf2) ? 1 : 2;
#ifndef VBOX
            qemu_irq_raise (pThis->pic[pThis->irq]);
#else
            PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 1);
#endif
            break;

        case 0xf9:
            pThis->needed_bytes = 1;
            goto warn;

        case 0xfa:
            dsp_out_data (pThis, 0);
            goto warn;

        case 0xfc:              /* FIXME */
            dsp_out_data (pThis, 0);
            goto warn;

        default:
            LogFlowFunc(("Unrecognized command %#x\n", cmd));
            break;
        }
    }

    if (!pThis->needed_bytes) {
        LogFlow(("\n"));
    }

 exit:
    if (!pThis->needed_bytes) {
        pThis->cmd = -1;
    }
    else {
        pThis->cmd = cmd;
    }
    return;

 warn:
    LogFlowFunc(("warning: command %#x,%d is not truly understood yet\n",
           cmd, pThis->needed_bytes));
    goto exit;

}

static uint16_t dsp_get_lohi (PSB16STATE pThis)
{
    uint8_t hi = dsp_get_data (pThis);
    uint8_t lo = dsp_get_data (pThis);
    return (hi << 8) | lo;
}

static uint16_t dsp_get_hilo (PSB16STATE pThis)
{
    uint8_t lo = dsp_get_data (pThis);
    uint8_t hi = dsp_get_data (pThis);
    return (hi << 8) | lo;
}

static void complete (PSB16STATE pThis)
{
    int d0, d1, d2;
    LogFlowFunc(("complete command %#x, in_index %d, needed_bytes %d\n",
            pThis->cmd, pThis->in_index, pThis->needed_bytes));

    if (pThis->cmd > 0xaf && pThis->cmd < 0xd0) {
        d2 = dsp_get_data (pThis);
        d1 = dsp_get_data (pThis);
        d0 = dsp_get_data (pThis);

        if (pThis->cmd & 8) {
            LogFlowFunc(("ADC params cmd = %#x d0 = %d, d1 = %d, d2 = %d\n",
                   pThis->cmd, d0, d1, d2));
        }
        else {
            LogFlowFunc(("cmd = %#x d0 = %d, d1 = %d, d2 = %d\n",
                    pThis->cmd, d0, d1, d2));
            dma_cmd (pThis, pThis->cmd, d0, d1 + (d2 << 8));
        }
    }
    else {
        switch (pThis->cmd) {
        case 0x04:
            pThis->csp_mode = dsp_get_data (pThis);
            pThis->csp_reg83r = 0;
            pThis->csp_reg83w = 0;
            LogFlowFunc(("CSP command 0x04: mode=%#x\n", pThis->csp_mode));
            break;

        case 0x05:
            pThis->csp_param = dsp_get_data (pThis);
            pThis->csp_value = dsp_get_data (pThis);
            LogFlowFunc(("CSP command 0x05: param=%#x value=%#x\n",
                    pThis->csp_param,
                    pThis->csp_value));
            break;

        case 0x0e:
            d0 = dsp_get_data (pThis);
            d1 = dsp_get_data (pThis);
            LogFlowFunc(("write CSP register %d <- %#x\n", d1, d0));
            if (d1 == 0x83) {
                LogFlowFunc(("0x83[%d] <- %#x\n", pThis->csp_reg83r, d0));
                pThis->csp_reg83[pThis->csp_reg83r % 4] = d0;
                pThis->csp_reg83r += 1;
            }
            else {
                pThis->csp_regs[d1] = d0;
            }
            break;

        case 0x0f:
            d0 = dsp_get_data (pThis);
            LogFlowFunc(("read CSP register %#x -> %#x, mode=%#x\n",
                    d0, pThis->csp_regs[d0], pThis->csp_mode));
            if (d0 == 0x83) {
                LogFlowFunc(("0x83[%d] -> %#x\n",
                        pThis->csp_reg83w,
                        pThis->csp_reg83[pThis->csp_reg83w % 4]));
                dsp_out_data (pThis, pThis->csp_reg83[pThis->csp_reg83w % 4]);
                pThis->csp_reg83w += 1;
            }
            else {
                dsp_out_data (pThis, pThis->csp_regs[d0]);
            }
            break;

        case 0x10:
            d0 = dsp_get_data (pThis);
            LogFlowFunc(("cmd 0x10 d0=%#x\n", d0));
            break;

        case 0x14:
            dma_cmd8(pThis, 0, dsp_get_lohi (pThis) + 1);
            break;

        case 0x40:
            pThis->time_const = dsp_get_data (pThis);
            LogFlowFunc(("set time const %d\n", pThis->time_const));
            break;

        case 0x42:              /* FT2 sets output freq with this, go figure */
#if 0
            LogFlowFunc(("cmd 0x42 might not do what it think it should\n"));
#endif
        case 0x41:
            pThis->freq = dsp_get_hilo (pThis);
            LogFlowFunc(("set freq %d\n", pThis->freq));
            break;

        case 0x48:
            pThis->block_size = dsp_get_lohi (pThis) + 1;
            LogFlowFunc(("set dma block len %d\n", pThis->block_size));
            break;

        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
            /* ADPCM stuff, ignore */
            break;

        case 0x80:
            {
                int freq, samples, bytes;
                uint64_t ticks;

                freq = pThis->freq > 0 ? pThis->freq : 11025;
                samples = dsp_get_lohi (pThis) + 1;
                bytes = samples << pThis->fmt_stereo << ((pThis->fmt_bits == 16) ? 1 : 0);
#ifndef VBOX
                ticks = (bytes * ticks_per_sec) / freq;
                if (ticks < ticks_per_sec / 1024) {
                    qemu_irq_raise (pThis->pic[pThis->irq]);
                }
                else {
                    if (pThis->aux_ts) {
                        qemu_mod_timer (
                            pThis->aux_ts,
                            qemu_get_clock (vm_clock) + ticks
                            );
                    }
                }
                LogFlowFunc(("mix silence %d %d %" PRId64 "\n", samples, bytes, ticks));
#else  /* VBOX */
                ticks = (bytes * TMTimerGetFreq(pThis->pTimerIRQ)) / freq;
                if (ticks < TMTimerGetFreq(pThis->pTimerIRQ) / 1024)
                    PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 1);
                else
                    TMTimerSet(pThis->pTimerIRQ, TMTimerGet(pThis->pTimerIRQ) + ticks);
                LogFlowFunc(("mix silence %d %d % %RU64\n", samples, bytes, ticks));
#endif /* VBOX */
            }
            break;

        case 0xe0:
            d0 = dsp_get_data (pThis);
            pThis->out_data_len = 0;
            LogFlowFunc(("E0 data = %#x\n", d0));
            dsp_out_data (pThis, ~d0);
            break;

        case 0xe2:
            d0 = dsp_get_data (pThis);
            LogFlow(("SB16:E2 = %#x\n", d0));
            break;

        case 0xe4:
            pThis->test_reg = dsp_get_data (pThis);
            break;

        case 0xf9:
            d0 = dsp_get_data (pThis);
            LogFlowFunc(("command 0xf9 with %#x\n", d0));
            switch (d0) {
            case 0x0e:
                dsp_out_data (pThis, 0xff);
                break;

            case 0x0f:
                dsp_out_data (pThis, 0x07);
                break;

            case 0x37:
                dsp_out_data (pThis, 0x38);
                break;

            default:
                dsp_out_data (pThis, 0x00);
                break;
            }
            break;

        default:
            LogFlowFunc(("complete: unrecognized command %#x\n", pThis->cmd));
            return;
        }
    }

    LogFlow(("\n"));
    pThis->cmd = -1;
    return;
}

static void legacy_reset(PSB16STATE pThis)
{
    pThis->freq       = 11025;
    pThis->fmt_signed = 0;
    pThis->fmt_bits   = 8;
    pThis->fmt_stereo = 0;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PDMAUDIOSTREAMCFG streamCfg;
    streamCfg.uHz           = pThis->freq;
    streamCfg.cChannels     = 1; /* Mono */
    streamCfg.enmFormat     = AUD_FMT_U8;
    streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

    int rc = sb16OpenOut(pThis, &streamCfg);
    AssertRC(rc);
#else
    audsettings_t streamCfg;
    streamCfg.freq = pThis->freq;
    streamCfg.nchannels = 1;
    streamCfg.fmt = AUD_FMT_U8;
    streamCfg.endianness = 0;
    pThis->voice = AUD_open_out (
        &pThis->card,
        pThis->voice,
        "sb16",
        pThis,
        sb16AudioCallback,
        &streamCfg
        );
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    /* Not sure about that... */
    /* AUD_set_active_out (pThis->voice, 1); */
}

static void sb16Reset(PSB16STATE pThis)
{
#ifndef VBOX
    qemu_irq_lower (pThis->pic[pThis->irq]);
    if (pThis->dma_auto) {
        qemu_irq_raise (pThis->pic[pThis->irq]);
        qemu_irq_lower (pThis->pic[pThis->irq]);
    }
#else  /* VBOX */
    PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 0);
    if (pThis->dma_auto) {
        PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 1);
        PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 0);
    }
#endif /* VBOX */

    pThis->mixer_regs[0x82] = 0;
    pThis->dma_auto = 0;
    pThis->in_index = 0;
    pThis->out_data_len = 0;
    pThis->left_till_irq = 0;
    pThis->needed_bytes = 0;
    pThis->block_size = -1;
    pThis->nzero = 0;
    pThis->highspeed = 0;
    pThis->v2x6 = 0;
    pThis->cmd = -1;

    dsp_out_data(pThis, 0xaa);
    sb16SpeakerControl(pThis, 0);
    sb16Control(pThis, 0);
    legacy_reset (pThis);
}

static IO_WRITE_PROTO(dsp_write)
{
    PSB16STATE pThis = (PSB16STATE)opaque;
    int iport = nport - pThis->port;

    LogFlowFunc(("write %#x <- %#x\n", nport, val));
    switch (iport)
    {
        case 0x06:
            switch (val)
            {
                case 0x00:
                {
                    if (pThis->v2x6 == 1)
                    {
                        if (0 && pThis->highspeed)
                        {
                            pThis->highspeed = 0;
#ifndef VBOX
                            qemu_irq_lower (pThis->pic[pThis->irq]);
#else
                            PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 0);
#endif
                            sb16Control(pThis, 0);
                        }
                        else
                            sb16Reset(pThis);
                    }
                    pThis->v2x6 = 0;
                    break;
                }

                case 0x01:
                case 0x03:              /* FreeBSD kludge */
                    pThis->v2x6 = 1;
                    break;

                case 0xc6:
                    pThis->v2x6 = 0;    /* Prince of Persia, csp.sys, diagnose.exe */
                    break;

                case 0xb8:              /* Panic */
                    sb16Reset(pThis);
                    break;

                case 0x39:
                    dsp_out_data(pThis, 0x38);
                    sb16Reset(pThis);
                    pThis->v2x6 = 0x39;
                    break;

                default:
                    pThis->v2x6 = val;
                    break;
            }
            break;

        case 0x0c:                      /* Write data or command | write status */
#if 0
            if (pThis->highspeed)
                break;
#endif
            if (0 == pThis->needed_bytes)
            {
                command(pThis, val);
#if 0
                if (0 == pThis->needed_bytes) {
                    log_dsp (pThis);
                }
#endif
            }
            else
            {
                if (pThis->in_index == sizeof (pThis->in2_data))
                {
                    LogFlowFunc(("in data overrun\n"));
                }
                else
                {
                    pThis->in2_data[pThis->in_index++] = val;
                    if (pThis->in_index == pThis->needed_bytes)
                    {
                        pThis->needed_bytes = 0;
                        complete (pThis);
#if 0
                        log_dsp (pThis);
#endif
                    }
                }
            }
            break;

        default:
            LogFlowFunc(("nport=%#x, val=%#x)\n", nport, val));
            break;
    }

#ifdef VBOX
    return VINF_SUCCESS;
#endif
}

static IO_READ_PROTO(dsp_read)
{
    PSB16STATE pThis = (PSB16STATE)opaque;
    int iport, retval, ack = 0;

    iport = nport - pThis->port;
#ifdef VBOX
    /** @todo reject non-byte access?
     *  The spec does not mention a non-byte access so we should check how real hardware behaves. */
#endif

    switch (iport)
    {
        case 0x06:                  /* reset */
            retval = 0xff;
            break;

        case 0x0a:                  /* read data */
            if (pThis->out_data_len)
            {
                retval = pThis->out_data[--pThis->out_data_len];
                pThis->last_read_byte = retval;
            }
            else
            {
                if (pThis->cmd != -1)
                    LogFlowFunc(("empty output buffer for command %#x\n", pThis->cmd));
                retval = pThis->last_read_byte;
                /* goto error; */
            }
            break;

        case 0x0c:                  /* 0 can write */
            retval = pThis->can_write ? 0 : 0x80;
            break;

        case 0x0d:                  /* timer interrupt clear */
            /* LogFlowFunc(("timer interrupt clear\n")); */
            retval = 0;
            break;

        case 0x0e:                  /* data available status | irq 8 ack */
            retval = (!pThis->out_data_len || pThis->highspeed) ? 0 : 0x80;
            if (pThis->mixer_regs[0x82] & 1)
            {
                ack = 1;
                pThis->mixer_regs[0x82] &= ~1;
#ifndef VBOX
                qemu_irq_lower (pThis->pic[pThis->irq]);
#else
                PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 0);
#endif
            }
            break;

        case 0x0f:                  /* irq 16 ack */
            retval = 0xff;
            if (pThis->mixer_regs[0x82] & 2)
            {
                ack = 1;
                pThis->mixer_regs[0x82] &= ~2;
#ifndef VBOX
               qemu_irq_lower (pThis->pic[pThis->irq]);
#else
               PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 0);
#endif
            }
            break;

        default:
            goto error;
    }

    if (!ack)
        LogFlowFunc(("read %#x -> %#x\n", nport, retval));

#ifndef VBOX
    return retval;
#else
    *pu32 = retval;
    return VINF_SUCCESS;
#endif

 error:
    LogFlowFunc(("warning: dsp_read %#x error\n", nport));
#ifndef VBOX
    return 0xff;
#else
    return VERR_IOM_IOPORT_UNUSED;
#endif
}

static void sb16MixerReset(PSB16STATE pThis)
{
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PSB16DRIVER pDrv;

    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
        pDrv->Out.phStrmOut   = NULL;

    pThis->pSinkOutput = NULL;

    if (pThis->pMixer)
    {
        audioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    memset (pThis->mixer_regs, 0xff, 0x7f);
    memset (pThis->mixer_regs + 0x83, 0xff, sizeof (pThis->mixer_regs) - 0x83);

    pThis->mixer_regs[0x02] = 4;    /* master volume 3bits */
    pThis->mixer_regs[0x06] = 4;    /* MIDI volume 3bits */
    pThis->mixer_regs[0x08] = 0;    /* CD volume 3bits */
    pThis->mixer_regs[0x0a] = 0;    /* voice volume 2bits */

    /* d5=input filt, d3=lowpass filt, d1,d2=input source */
    pThis->mixer_regs[0x0c] = 0;

    /* d5=output filt, d1=stereo switch */
    pThis->mixer_regs[0x0e] = 0;

    /* voice volume L d5,d7, R d1,d3 */
    pThis->mixer_regs[0x04] = (4 << 5) | (4 << 1);
    /* master ... */
    pThis->mixer_regs[0x22] = (4 << 5) | (4 << 1);
    /* MIDI ... */
    pThis->mixer_regs[0x26] = (4 << 5) | (4 << 1);

    for (int i = 0x30; i < 0x48; i++)
        pThis->mixer_regs[i] = 0x20;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    int rc2 = audioMixerCreate("SB16 Mixer", 0 /* uFlags */, &pThis->pMixer);
    if (RT_SUCCESS(rc2))
    {
        /* Set a default audio format for our mixer. */
        PDMAUDIOSTREAMCFG streamCfg;
        streamCfg.uHz           = 41000;
        streamCfg.cChannels     = 2;
        streamCfg.enmFormat     = AUD_FMT_S16;
        streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

        rc2 = audioMixerSetDeviceFormat(pThis->pMixer, &streamCfg);
        AssertRC(rc2);

        /* Add all required audio sinks. */
        rc2 = audioMixerAddSink(pThis->pMixer, "[Playback] PCM Output",
                                AUDMIXSINKDIR_OUTPUT, &pThis->pSinkOutput);
        AssertRC(rc2);
    }
#endif
}

static IO_WRITE_PROTO(mixer_write_indexb)
{
    PSB16STATE pThis = (PSB16STATE)opaque;
    (void) nport;
    pThis->mixer_nreg = val;

#ifdef VBOX
    return VINF_SUCCESS;
#endif
}

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
uint32_t popcount (uint32_t u) /** @todo r=andy WTF? */
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

uint32_t lsbindex (uint32_t u)
{
    return popcount ((u&-u)-1); /** @todo r=andy Un/signed mismatch? */
}
#endif

static IO_WRITE_PROTO(mixer_write_datab)
{
    PSB16STATE  pThis = (PSB16STATE)opaque;
    bool        fUpdateMaster = false;
    bool        fUpdateStream = false;

    (void) nport;
    LogFlowFunc(("mixer_write [%#x] <- %#x\n", pThis->mixer_nreg, val));

    switch (pThis->mixer_nreg)
    {
        case 0x00:
            sb16MixerReset(pThis);
            /* And update the actual volume, too. */
            fUpdateMaster = true;
            fUpdateStream  = true;
            break;

        case 0x04:
            /* Translate from old style stream volume (L/R). */
            pThis->mixer_regs[0x32] = val & 0xff;
            pThis->mixer_regs[0x33] = val << 4;
            fUpdateStream = true;
            break;

        case 0x22:
            /* Translate from old style master volume (L/R). */
            pThis->mixer_regs[0x30] = val & 0xff;
            pThis->mixer_regs[0x31] = val << 4;
            fUpdateMaster = true;
            break;

        case 0x30:
            /* Translate to old style master volume (L). */
            pThis->mixer_regs[0x22] = (pThis->mixer_regs[0x22] & 0x0f) | val;
            fUpdateMaster = true;
            break;

        case 0x31:
            /* Translate to old style master volume (R). */
            pThis->mixer_regs[0x22] = (pThis->mixer_regs[0x22] & 0xf0) | (val >> 4);
            fUpdateMaster = true;
            break;

        case 0x32:
            /* Translate to old style stream volume (L). */
            pThis->mixer_regs[0x04] = (pThis->mixer_regs[0x04] & 0x0f) | val;
            fUpdateStream = true;
            break;

        case 0x33:
            /* Translate to old style stream volume (R). */
            pThis->mixer_regs[0x04] = (pThis->mixer_regs[0x04] & 0xf0) | (val >> 4);
            fUpdateStream = true;
            break;

        case 0x80:
        {
            int irq = irq_of_magic(val);
            LogFlowFunc(("setting irq to %d (val=%#x)\n", irq, val));
            if (irq > 0)
                pThis->irq = irq;
            break;
        }

        case 0x81:
        {
            int dma, hdma;

            dma = lsbindex (val & 0xf);
            hdma = lsbindex (val & 0xf0);
            if (dma != pThis->dma || hdma != pThis->hdma)
                LogFlow(("SB16: attempt to change DMA 8bit %d(%d), 16bit %d(%d) (val=%#x)\n",
                         dma, pThis->dma, hdma, pThis->hdma, val));
#if 0
            pThis->dma = dma;
            pThis->hdma = hdma;
#endif
            break;
        }

        case 0x82:
            LogFlowFunc(("attempt to write into IRQ status register (val=%#x)\n", val));
#ifdef VBOX
            return VINF_SUCCESS;
#endif

        default:
            if (pThis->mixer_nreg >= 0x80)
                LogFlowFunc(("attempt to write mixer[%#x] <- %#x\n", pThis->mixer_nreg, val));
            break;
    }

    pThis->mixer_regs[pThis->mixer_nreg] = val;

#ifdef VBOX
    /* Update the master (mixer) volume. */
    if (fUpdateMaster)
    {
        int     mute = 0; /** @todo Handle (un)muting. */
        uint8_t lvol = pThis->mixer_regs[0x30];
        uint8_t rvol = pThis->mixer_regs[0x31];

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        PDMAUDIOVOLUME vol = { RT_BOOL(mute), lvol, rvol };
        audioMixerSetMasterVolume(pThis->pMixer, &vol);
#else
        AUD_set_volume(AUD_MIXER_VOLUME, &mute, &lvol, &rvol);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }

    /* Update the stream (PCM) volume. */
    if (fUpdateStream)
    {
        int     mute = 0; /** @todo Handle (un)muting. */
        uint8_t lvol = pThis->mixer_regs[0x32];
        uint8_t rvol = pThis->mixer_regs[0x33];

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        PDMAUDIOVOLUME vol = { RT_BOOL(mute), lvol, rvol };
        audioMixerSetSinkVolume(pThis->pSinkOutput, &vol);
#else
        AUD_set_volume(AUD_MIXER_PCM, &mute, &lvol, &rvol);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
    }
#endif /* VBOX */

#ifdef VBOX
    return VINF_SUCCESS;
#endif
}

static IO_WRITE_PROTO(mixer_write)
{
#ifndef VBOX
    mixer_write_indexb (opaque, nport, val & 0xff);
    mixer_write_datab (opaque, nport, (val >> 8) & 0xff);
#else  /* VBOX */
    PSB16STATE pThis = (PSB16STATE)opaque;
    int iport = nport - pThis->port;
    switch (cb)
    {
        case 1:
            switch (iport)
            {
                case 4:
                    mixer_write_indexb (pDevIns, opaque, nport, val, 1);
                    break;
                case 5:
                    mixer_write_datab (pDevIns, opaque, nport, val, 1);
                    break;
            }
            break;
        case 2:
            mixer_write_indexb (pDevIns, opaque, nport, val & 0xff, 1);
            mixer_write_datab (pDevIns, opaque, nport, (val >> 8) & 0xff, 1);
            break;
        default:
            AssertMsgFailed(("Port=%#x cb=%d u32=%#x\n", nport, cb, val));
            break;
    }
    return VINF_SUCCESS;
#endif /* VBOX */
}

static IO_READ_PROTO(mixer_read)
{
    PSB16STATE pThis = (PSB16STATE)opaque;

    (void) nport;
#ifndef DEBUG_SB16_MOST
    if (pThis->mixer_nreg != 0x82) {
        LogFlowFunc(("mixer_read[%#x] -> %#x\n",
                pThis->mixer_nreg, pThis->mixer_regs[pThis->mixer_nreg]));
    }
#else
    LogFlowFunc(("mixer_read[%#x] -> %#x\n",
            pThis->mixer_nreg, pThis->mixer_regs[pThis->mixer_nreg]));
#endif
#ifndef VBOX
    return pThis->mixer_regs[pThis->mixer_nreg];
#else
    *pu32 = pThis->mixer_regs[pThis->mixer_nreg];
    return VINF_SUCCESS;
#endif
}

static int sb16WriteAudio(PSB16STATE pThis, int nchan, int dma_pos,
                          int dma_len, int len)
{
    uint8_t tmpbuf[_4K]; /** @todo Have a buffer on the heap. */

    int cbToWrite = len;
    int cbWrittenTotal = 0;

    while (cbToWrite)
    {
#ifndef VBOX
        size_t cbToRead;
        int cbRead;
#else
        uint32_t cbWrittenMin = UINT32_MAX;
        size_t cbToRead;
        uint32_t cbRead;
#endif
        cbToRead = RT_MIN(dma_len - dma_pos, cbToWrite);
        if (cbToRead > sizeof(tmpbuf))
            cbToRead = sizeof(tmpbuf);

#ifndef VBOX
        cbRead = DMA_read_memory (nchan, tmpbuf, dma_pos, cbToRead);
#else
        int rc = PDMDevHlpDMAReadMemory(pThis->pDevIns, nchan, tmpbuf, dma_pos, cbToRead, &cbRead);
        AssertMsgRC (rc, ("DMAReadMemory -> %Rrc\n", rc));
#endif

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
        uint32_t cbWritten;

        /* Just multiplex the output to the connected backends.
         * No need to utilize the virtual mixer here (yet). */
        PSB16DRIVER pDrv;
        RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
        {
            int rc2 = pDrv->pConnector->pfnWrite(pDrv->pConnector, pDrv->Out.pStrmOut,
                                                 tmpbuf, cbToRead, &cbWritten);
            AssertRCBreak(rc);
            if (RT_FAILURE(rc2))
                continue;

            cbWrittenMin = RT_MIN(cbWrittenMin, cbWritten);
            LogFlowFunc(("\tLUN#%RU8: cbWritten=%RU32, cWrittenMin=%RU32\n", pDrv->uLUN, cbWritten, cbWrittenMin));
        }
#else
        cbWrittenMin = AUD_write (pThis->voice, tmpbuf, cbToRead);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

        cbToWrite      -= cbWrittenMin;
        dma_pos         = (dma_pos + cbWrittenMin) % dma_len;
        cbWrittenTotal += cbWrittenMin;

        if (!cbRead)
            break;
    }

    return cbWrittenTotal;
}

#ifndef VBOX
static int sb16DMARead(void *opaque, int nchan, int dma_pos, int dma_len)
#else
static DECLCALLBACK(uint32_t) sb16DMARead(PPDMDEVINS pDevIns, void *opaque, unsigned nchan, uint32_t dma_pos, uint32_t dma_len)
#endif
{
    PSB16STATE pThis = (PSB16STATE)opaque;
    int till, copy, written, free;

    if (pThis->block_size <= 0)
    {
        LogFlowFunc(("invalid block size=%d nchan=%d dma_pos=%d dma_len=%d\n",
               pThis->block_size, nchan, dma_pos, dma_len));
        return dma_pos;
    }

    if (pThis->left_till_irq < 0)
        pThis->left_till_irq = pThis->block_size;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PSB16DRIVER pDrv;

    uint32_t cbOutMin = UINT32_MAX;
    uint32_t cbOut;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        int rc2 = pDrv->pConnector->pfnQueryStatus(pDrv->pConnector,
                                                   NULL /* pcbIn */, &cbOut, NULL /* pcSamplesLive */);
        if (RT_SUCCESS(rc2))
            cbOutMin = RT_MIN(cbOutMin, cbOut);
    }

    LogFlowFunc(("cbOutMin=%RU32\n", cbOutMin));
    if (cbOutMin == UINT32_MAX)
    {
        free = dma_len;
    }
    else
    {
        free = cbOutMin & ~pThis->align; /** @todo int vs. uint32. */
        if ((free <= 0) || !dma_len)
            return dma_pos;
    }
#else
    if (pThis->voice)
    {
        free = pThis->audio_free & ~pThis->align;
        if ((free <= 0) || !dma_len)
            return dma_pos;
    }
    else
        free = dma_len;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    copy = free;
    till = pThis->left_till_irq;

#ifdef DEBUG_SB16_MOST
    LogFlowFunc(("pos:%06d %d till:%d len:%d\n", dma_pos, free, till, dma_len));
#endif

    if (copy >= till)
    {
        if (0 == pThis->dma_auto)
        {
            copy = till;
        }
        else
        {
            if (copy >= till + pThis->block_size)
                copy = till; /* Make sure we won't skip IRQs. */
        }
    }

    written = sb16WriteAudio(pThis, nchan, dma_pos, dma_len, copy);
    dma_pos = (dma_pos + written) % dma_len;
    Assert(pThis->left_till_irq >= written);
    pThis->left_till_irq -= written;

    if (pThis->left_till_irq <= 0)
    {
        pThis->mixer_regs[0x82] |= (nchan & 4) ? 2 : 1;
#ifndef VBOX
        qemu_irq_raise (pThis->pic[pThis->irq]);
#else
        PDMDevHlpISASetIrq(pThis->pDevIns, pThis->irq, 1);
#endif
        if (0 == pThis->dma_auto)
        {
            sb16Control(pThis, 0);
            sb16SpeakerControl(pThis, 0);
        }
    }

#ifdef DEBUG_SB16_MOST
    LogFlowFunc(("pos %5d free %5d size %5d till % 5d copy %5d written %5d size %5d\n",
                 dma_pos, free, dma_len, pThis->left_till_irq, copy, written,
                 pThis->block_size));
#endif

    while (pThis->left_till_irq <= 0)
        pThis->left_till_irq += pThis->block_size;

    return dma_pos;
}

#ifndef VBOX_WITH_PDM_AUDIO_DRIVER
static void sb16AudioCallback(void *pvContext, int cbFree)
{
    PSB16STATE pState = (PSB16STATE)pvContext;
    AssertPtrReturnVoid(pState);
    pState->audio_free = cbFree;
#ifdef VBOX
    /* New space available, see if we can transfer more. There is no cyclic DMA timer in VBox. */
    PDMDevHlpDMASchedule(pState->pDevIns);
#endif
}
#else
static DECLCALLBACK(void) sb16TimerIO(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PSB16STATE pThis = PDMINS_2_DATA(pDevIns, PSB16STATE);
    AssertPtrReturnVoid(pThis);

    int rc = VINF_SUCCESS;

    uint32_t cbInMax  = 0;
    uint32_t cbOutMin = UINT32_MAX;

    PSB16DRIVER pDrv;

    uint32_t cbIn, cbOut, cSamplesLive;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        rc = pDrv->pConnector->pfnQueryStatus(pDrv->pConnector,
                                              &cbIn, &cbOut, &cSamplesLive);
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("\tLUN#%RU8: [1] cbIn=%RU32, cbOut=%RU32\n", pDrv->uLUN, cbIn, cbOut));

            if (cSamplesLive)
            {
                uint32_t cSamplesPlayed;
                int rc2 = pDrv->pConnector->pfnPlayOut(pDrv->pConnector, &cSamplesPlayed);
                if (RT_SUCCESS(rc2))
                    LogFlowFunc(("LUN#%RU8: cSamplesLive=%RU32, cSamplesPlayed=%RU32\n",
                                 pDrv->uLUN, cSamplesLive, cSamplesPlayed));

                if (cSamplesPlayed)
                {
                    rc = pDrv->pConnector->pfnQueryStatus(pDrv->pConnector,
                                                          &cbIn, &cbOut, &cSamplesLive);
                    if (RT_SUCCESS(rc))
                        LogFlowFunc(("\tLUN#%RU8: [2] cbIn=%RU32, cbOut=%RU32\n", pDrv->uLUN, cbIn, cbOut));
                }
            }

            cbInMax  = RT_MAX(cbInMax, cbIn);
            cbOutMin = RT_MIN(cbOutMin, cbOut);
        }
    }

    LogFlowFunc(("cbInMax=%RU32, cbOutMin=%RU32\n", cbInMax, cbOutMin));

    if (cbOutMin == UINT32_MAX)
        cbOutMin = 0;

    /*
     * Playback.
     */
    if (cbOutMin)
    {
        Assert(cbOutMin != UINT32_MAX);

        /* New space available, see if we can transfer more. */
        PDMDevHlpDMASchedule(pThis->pDevIns);
    }

    /*
     * Recording.
     */
    /** @todo Implement recording. */

    TMTimerSet(pThis->pTimerIO, TMTimerGet(pThis->pTimerIO) + pThis->uTicksIO);
}
#endif /* !VBOX_WITH_PDM_AUDIO_DRIVER */

static void SB_save (QEMUFile *f, void *opaque)
{
#ifndef VBOX
    PSB16STATE pThis = opaque;
#else
    PSB16STATE pThis = (PSB16STATE)opaque;
#endif

    qemu_put_be32 (f, pThis->irq);
    qemu_put_be32 (f, pThis->dma);
    qemu_put_be32 (f, pThis->hdma);
    qemu_put_be32 (f, pThis->port);
    qemu_put_be32 (f, pThis->ver);
    qemu_put_be32 (f, pThis->in_index);
    qemu_put_be32 (f, pThis->out_data_len);
    qemu_put_be32 (f, pThis->fmt_stereo);
    qemu_put_be32 (f, pThis->fmt_signed);
    qemu_put_be32 (f, pThis->fmt_bits);
    qemu_put_be32s (f, &pThis->fmt);
    qemu_put_be32 (f, pThis->dma_auto);
    qemu_put_be32 (f, pThis->block_size);
    qemu_put_be32 (f, pThis->fifo);
    qemu_put_be32 (f, pThis->freq);
    qemu_put_be32 (f, pThis->time_const);
    qemu_put_be32 (f, pThis->speaker);
    qemu_put_be32 (f, pThis->needed_bytes);
    qemu_put_be32 (f, pThis->cmd);
    qemu_put_be32 (f, pThis->use_hdma);
    qemu_put_be32 (f, pThis->highspeed);
    qemu_put_be32 (f, pThis->can_write);
    qemu_put_be32 (f, pThis->v2x6);

    qemu_put_8s (f, &pThis->csp_param);
    qemu_put_8s (f, &pThis->csp_value);
    qemu_put_8s (f, &pThis->csp_mode);
    qemu_put_8s (f, &pThis->csp_param);
    qemu_put_buffer (f, pThis->csp_regs, 256);
    qemu_put_8s (f, &pThis->csp_index);
    qemu_put_buffer (f, pThis->csp_reg83, 4);
    qemu_put_be32 (f, pThis->csp_reg83r);
    qemu_put_be32 (f, pThis->csp_reg83w);

    qemu_put_buffer (f, pThis->in2_data, sizeof (pThis->in2_data));
    qemu_put_buffer (f, pThis->out_data, sizeof (pThis->out_data));
    qemu_put_8s (f, &pThis->test_reg);
    qemu_put_8s (f, &pThis->last_read_byte);

    qemu_put_be32 (f, pThis->nzero);
    qemu_put_be32 (f, pThis->left_till_irq);
    qemu_put_be32 (f, pThis->dma_running);
    qemu_put_be32 (f, pThis->bytes_per_second);
    qemu_put_be32 (f, pThis->align);

    qemu_put_be32 (f, pThis->mixer_nreg);
    qemu_put_buffer (f, pThis->mixer_regs, 256);
}

static int SB_load (QEMUFile *f, void *opaque, int version_id)
{
#ifndef VBOX
    PSB16STATE pThis = opaque;

    if (version_id != 1) {
        return -EINVAL;
    }
#else
    PSB16STATE pThis = (PSB16STATE)opaque;
#endif

    pThis->irq=qemu_get_be32 (f);
    pThis->dma=qemu_get_be32 (f);
    pThis->hdma=qemu_get_be32 (f);
    pThis->port=qemu_get_be32 (f);
    pThis->ver=qemu_get_be32 (f);
    pThis->in_index=qemu_get_be32 (f);
    pThis->out_data_len=qemu_get_be32 (f);
    pThis->fmt_stereo=qemu_get_be32 (f);
    pThis->fmt_signed=qemu_get_be32 (f);
    pThis->fmt_bits=qemu_get_be32 (f);
    qemu_get_be32s (f, (uint32_t*)&pThis->fmt);
    pThis->dma_auto=qemu_get_be32 (f);
    pThis->block_size=qemu_get_be32 (f);
    pThis->fifo=qemu_get_be32 (f);
    pThis->freq=qemu_get_be32 (f);
    pThis->time_const=qemu_get_be32 (f);
    pThis->speaker=qemu_get_be32 (f);
    pThis->needed_bytes=qemu_get_be32 (f);
    pThis->cmd=qemu_get_be32 (f);
    pThis->use_hdma=qemu_get_be32 (f);
    pThis->highspeed=qemu_get_be32 (f);
    pThis->can_write=qemu_get_be32 (f);
    pThis->v2x6=qemu_get_be32 (f);

    qemu_get_8s (f, &pThis->csp_param);
    qemu_get_8s (f, &pThis->csp_value);
    qemu_get_8s (f, &pThis->csp_mode);
    qemu_get_8s (f, &pThis->csp_param);
    qemu_get_buffer (f, pThis->csp_regs, 256);
    qemu_get_8s (f, &pThis->csp_index);
    qemu_get_buffer (f, pThis->csp_reg83, 4);
    pThis->csp_reg83r=qemu_get_be32 (f);
    pThis->csp_reg83w=qemu_get_be32 (f);

    qemu_get_buffer (f, pThis->in2_data, sizeof (pThis->in2_data));
    qemu_get_buffer (f, pThis->out_data, sizeof (pThis->out_data));
    qemu_get_8s (f, &pThis->test_reg);
    qemu_get_8s (f, &pThis->last_read_byte);

    pThis->nzero=qemu_get_be32 (f);
    pThis->left_till_irq=qemu_get_be32 (f);
    pThis->dma_running=qemu_get_be32 (f);
    pThis->bytes_per_second=qemu_get_be32 (f);
    pThis->align=qemu_get_be32 (f);

    pThis->mixer_nreg=qemu_get_be32 (f);
    qemu_get_buffer (f, pThis->mixer_regs, 256);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PSB16DRIVER pDrv;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        if (pDrv->Out.pStrmOut)
        {
            pDrv->pConnector->pfnCloseOut(pThis->pDrv, pDrv->Out.pStrmOut);
            pDrv->Out.pStrmOut = NULL;
        }
    }
#else
    AUD_close_out (&pThis->card, pThis->voice);
    pThis->voice = NULL;
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    if (pThis->dma_running)
    {
        if (pThis->freq)
        {
            pThis->audio_free = 0;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.uHz           = pThis->freq;
            streamCfg.cChannels     = 1 << pThis->fmt_stereo;
            streamCfg.enmFormat     = pThis->fmt;
            streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

            int rc = sb16OpenOut(pThis, &streamCfg);
            AssertRC(rc);
#else
            audsettings_t as;
            as.freq = pThis->freq;
            as.nchannels = 1 << pThis->fmt_stereo;
            as.fmt = pThis->fmt;
            as.endianness = 0;
            pThis->voice = AUD_open_out (
                &pThis->card,
                pThis->voice,
                "sb16",
                pThis,
                sb16AudioCallback,
                &as
                );
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */
        }

        sb16Control(pThis, 1);
        sb16SpeakerControl(pThis, pThis->speaker);
    }

#ifdef VBOX
    return VINF_SUCCESS;
#endif
}

#ifndef VBOX
int SB16_init (AudioState *audio, qemu_irq *pic)
{
    PSB16STATE pThis;
    int i;
    static const uint8_t dsp_write_ports[] = {0x6, 0xc};
    static const uint8_t dsp_read_ports[] = {0x6, 0xa, 0xc, 0xd, 0xe, 0xf};

    if (!audio) {
        LogFlowFunc(("No audio state\n"));
        return -1;
    }

    s = qemu_mallocz (sizeof (*s));
    if (!s) {
        LogFlowFunc(("Could not allocate memory for SB16 (%zu bytes)\n",
               sizeof (*s));
        return -1;
    }

    pThis->cmd = -1;
    pThis->pic = pic;
    pThis->irq = conf.irq;
    pThis->dma = conf.dma;
    pThis->hdma = conf.hdma;
    pThis->port = conf.port;
    pThis->ver = conf.ver_lo | (conf.ver_hi << 8);

    pThis->mixer_regs[0x80] = magic_of_irq (pThis->irq);
    pThis->mixer_regs[0x81] = (1 << pThis->dma) | (1 << pThis->hdma);
    pThis->mixer_regs[0x82] = 2 << 5;

    pThis->csp_regs[5] = 1;
    pThis->csp_regs[9] = 0xf8;

    sb16MixerReset(pThis);
    pThis->aux_ts = qemu_new_timer (vm_clock, aux_timer, s);
    if (!pThis->aux_ts) {
        LogFlowFunc(("warning: Could not create auxiliary timer\n"));
    }

    for (i = 0; i < LENOFA (dsp_write_ports); i++) {
        register_ioport_write (pThis->port + dsp_write_ports[i], 1, 1, dsp_write, s);
    }

    for (i = 0; i < LENOFA (dsp_read_ports); i++) {
        register_ioport_read (pThis->port + dsp_read_ports[i], 1, 1, dsp_read, s);
    }

    register_ioport_write (pThis->port + 0x4, 1, 1, mixer_write_indexb, s);
    register_ioport_write (pThis->port + 0x4, 1, 2, mixer_write_indexw, s);
    register_ioport_read (pThis->port + 0x5, 1, 1, mixer_read, s);
    register_ioport_write (pThis->port + 0x5, 1, 1, mixer_write_datab, s);

    DMA_register_channel (pThis->hdma, sb16DMARead, s);
    DMA_register_channel (pThis->dma, sb16DMARead, s);
    pThis->can_write = 1;

    register_savevm ("sb16", 0, 1, SB_save, SB_load, s);
    AUD_register_card (audio, "sb16", &pThis->card);
    return 0;
}

#else /* VBOX */

static DECLCALLBACK(int) sb16LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PSB16STATE pThis = PDMINS_2_DATA (pDevIns, PSB16STATE);

    SSMR3PutS32(pSSM, pThis->irqCfg);
    SSMR3PutS32(pSSM, pThis->dmaCfg);
    SSMR3PutS32(pSSM, pThis->hdmaCfg);
    SSMR3PutS32(pSSM, pThis->portCfg);
    SSMR3PutS32(pSSM, pThis->verCfg);
    return VINF_SSM_DONT_CALL_AGAIN;
}

static DECLCALLBACK(int) sb16SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PSB16STATE pThis = PDMINS_2_DATA (pDevIns, PSB16STATE);

    sb16LiveExec (pDevIns, pSSM, 0);
    SB_save (pSSM, pThis);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) sb16LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PSB16STATE pThis = PDMINS_2_DATA (pDevIns, PSB16STATE);

    AssertMsgReturn(    uVersion == SB16_SAVE_STATE_VERSION
                    ||  uVersion == SB16_SAVE_STATE_VERSION_VBOX_30,
                    ("%u\n", uVersion),
                    VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    if (uVersion > SB16_SAVE_STATE_VERSION_VBOX_30)
    {
        int32_t irq;
        SSMR3GetS32 (pSSM, &irq);
        int32_t dma;
        SSMR3GetS32 (pSSM, &dma);
        int32_t hdma;
        SSMR3GetS32 (pSSM, &hdma);
        int32_t port;
        SSMR3GetS32 (pSSM, &port);
        int32_t ver;
        int rc = SSMR3GetS32 (pSSM, &ver);
        AssertRCReturn (rc, rc);

        if (   irq  != pThis->irqCfg
            || dma  != pThis->dmaCfg
            || hdma != pThis->hdmaCfg
            || port != pThis->portCfg
            || ver  != pThis->verCfg )
            return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                    N_("config changed: irq=%x/%x dma=%x/%x hdma=%x/%x port=%x/%x ver=%x/%x (saved/config)"),
                                    irq,  pThis->irqCfg,
                                    dma,  pThis->dmaCfg,
                                    hdma, pThis->hdmaCfg,
                                    port, pThis->portCfg,
                                    ver,  pThis->verCfg);
    }
    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    SB_load(pSSM, pThis, uVersion);
    return VINF_SUCCESS;
}

static int sb16OpenOut(PSB16STATE pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PSB16DRIVER pDrv;
    uint8_t uLUN = 0;
    char *pszDesc;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        if (RTStrAPrintf(&pszDesc, "[LUN#%RU8] sb16.po", uLUN) <= 0)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        int rc2 = pDrv->pConnector->pfnOpenOut(pDrv->pConnector, pszDesc, pCfg, &pDrv->Out.pStrmOut);
        LogFlowFunc(("LUN#%RU8: Opened output with rc=%Rrc\n", uLUN, rc));
        if (rc2 == VINF_SUCCESS) /* Note: Could return VWRN_ALREADY_EXISTS. */
        {
            audioMixerRemoveStream(pThis->pSinkOutput, pDrv->Out.phStrmOut);
            rc = audioMixerAddStreamOut(pThis->pSinkOutput,
                                        pDrv->pConnector, pDrv->Out.pStrmOut,
                                        0 /* uFlags */,
                                        &pDrv->Out.phStrmOut);
        }

        RTStrFree(pszDesc);

        if (RT_FAILURE(rc2))
        {
            if (RT_SUCCESS(rc))
                rc = rc2;
            break;
        }

        uLUN++;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) sb16QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PSB16STATE pThis = RT_FROM_MEMBER(pInterface, SB16STATE, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) sb16Destruct(PPDMDEVINS pDevIns)
{
    PSB16STATE pThis = PDMINS_2_DATA(pDevIns, PSB16STATE);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    PSB16DRIVER pDrv;

    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
        pDrv->Out.phStrmOut = NULL;

    pThis->pSinkOutput = NULL;

    if (pThis->pMixer)
    {
        audioMixerDestroy(pThis->pMixer);
        pThis->pMixer = NULL;
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) sb16Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PSB16STATE pThis = PDMINS_2_DATA(pDevIns, PSB16STATE);
    int rc;

    /*
     * Validations.
     */
    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "IRQ\0"
                              "DMA\0"
                              "DMA16\0"
                              "Port\0"
                              "Version\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuration for sb16 device"));

    /*
     * Read config data.
     */
    rc = CFGMR3QuerySIntDef(pCfgHandle, "IRQ", &pThis->irq, 5);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));
    pThis->irqCfg  = pThis->irq;

    rc = CFGMR3QuerySIntDef(pCfgHandle, "DMA", &pThis->dma, 1);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DMA\" value"));
    pThis->dmaCfg  = pThis->dma;

    rc = CFGMR3QuerySIntDef(pCfgHandle, "DMA16", &pThis->hdma, 5);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DMA16\" value"));
    pThis->hdmaCfg = pThis->hdma;

    RTIOPORT Port;
    rc = CFGMR3QueryPortDef(pCfgHandle, "Port", &Port, 0x220);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Port\" value"));
    pThis->port    = Port;
    pThis->portCfg = Port;

    uint16_t u16Version;
    rc = CFGMR3QueryU16Def(pCfgHandle, "Version", &u16Version, 0x0405);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Version\" value"));
    pThis->ver     = u16Version;
    pThis->verCfg  = u16Version;

    /*
     * Init instance data.
     */
    pThis->pDevIns                 = pDevIns;
    pThis->IBase.pfnQueryInterface = sb16QueryInterface;
    pThis->cmd                     = -1;

    pThis->mixer_regs[0x80]        = magic_of_irq (pThis->irq);
    pThis->mixer_regs[0x81]        = (1 << pThis->dma) | (1 << pThis->hdma);
    pThis->mixer_regs[0x82]        = 2 << 5;

    pThis->csp_regs[5]             = 1;
    pThis->csp_regs[9]             = 0xf8;

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    RTListInit(&pThis->lstDrv);
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    sb16MixerReset(pThis);

    /*
     * Create timer(s), register & attach stuff.
     */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, sb16TimerIRQ, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "SB16 IRQ timer", &pThis->pTimerIRQ);
    if (RT_FAILURE(rc))
        AssertMsgFailedReturn(("Error creating IRQ timer, rc=%Rrc\n", rc), rc);

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, sb16TimerIO, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "SB16 IO timer", &pThis->pTimerIO);
    if (RT_FAILURE(rc))
        AssertMsgFailedReturn(("Error creating I/O timer, rc=%Rrc\n", rc), rc);
    else
    {
        pThis->uTicksIO = PDMDevHlpTMTimeVirtGetFreq(pDevIns) / 200; /** Hz. @todo Make this configurable! */
        if (pThis->uTicksIO < 100)
            pThis->uTicksIO = 100;
        LogFunc(("I/O timer ticks=%RU64\n", pThis->uTicksIO));

        /* Fire off timer. */
        TMTimerSet(pThis->pTimerIO, TMTimerGet(pThis->pTimerIO) + pThis->uTicksIO);
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    rc = PDMDevHlpIOPortRegister(pDevIns, pThis->port + 0x04,  2, pThis,
                                 mixer_write, mixer_read, NULL, NULL, "SB16");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, pThis->port + 0x06, 10, pThis,
                                 dsp_write, dsp_read, NULL, NULL, "SB16");
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpDMARegister(pDevIns, pThis->hdma, sb16DMARead, pThis);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpDMARegister(pDevIns, pThis->dma, sb16DMARead, pThis);
    if (RT_FAILURE(rc))
        return rc;

    pThis->can_write = 1;

    rc = PDMDevHlpSSMRegister3(pDevIns, SB16_SAVE_STATE_VERSION, sizeof(SB16STATE), sb16LiveExec, sb16SaveExec, sb16LoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Attach driver.
     */
#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    uint8_t uLUN;
    for (uLUN = 0; uLUN < UINT8_MAX; uLUN)
    {
        LogFunc(("Trying to attach driver for LUN #%RU8 ...\n", uLUN));
        rc = sb16Attach(pDevIns, uLUN, PDM_TACH_FLAGS_NOT_HOT_PLUG);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
                rc = VINF_SUCCESS;
            break;
        }

        uLUN++;
    }

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

#ifdef VBOX_WITH_PDM_AUDIO_DRIVER
    legacy_reset(pThis);

    PSB16DRIVER pDrv;
    uLUN = 0;
    RTListForEach(&pThis->lstDrv, pDrv, SB16DRIVER, Node)
    {
        /*
         * Only primary drivers are critical for the VM to run. Everything else
         * might not worth showing an own error message box in the GUI.
         */
        if (!(pDrv->Flags & PDMAUDIODRVFLAG_PRIMARY))
            continue;

        PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
        AssertPtr(pCon);
        if (!pCon->pfnIsOutputOK(pCon, pDrv->Out.pStrmOut))
        {
            LogRel(("SB16: WARNING: Unable to open PCM OUT!\n"));

            pCon->pfnCloseOut(pCon, pDrv->Out.pStrmOut);
            pDrv->Out.pStrmOut = NULL;

            pThis->pDrv->pfnInitNull(pThis->pDrv);

            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                N_("No audio devices could be opened. Selecting the NULL audio backend "
                   "with the consequence that no sound is audible"));
        }
    }
#else
    AUD_register_card("sb16", &pThis->card);
    legacy_reset(pThis);

    if (!AUD_is_host_voice_out_ok(pThis->voice))
    {
        LogRel (("SB16: WARNING: Unable to open PCM OUT!\n"));
        AUD_close_out (&pThis->card, pThis->voice);
        pThis->voice = NULL;

        AUD_init_null();

        PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
            N_("No audio devices could be opened. Selecting the NULL audio backend "
               "with the consequence that no sound is audible"));
    }
#endif /* VBOX_WITH_PDM_AUDIO_DRIVER */

    return VINF_SUCCESS;
}

const PDMDEVREG g_DeviceSB16 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "sb16",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Sound Blaster 16 Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(SB16STATE),
    /* pfnConstruct */
    sb16Construct,
    /* pfnDestruct */
    sb16Destruct,
    /* pfnRelocate */
    NULL,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface */
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
#endif /* VBOX */

