/** @file
 *
 * VBox SB16 Audio Controller
 *
 * TODO: hiccups on NT4 and Win98.
 */

/*
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

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "../vl_vbox.h"

extern "C" {
#include "audio.h"
}

#define SB16_SSM_VERSION 1

#ifndef VBOX

#define LENOFA(a) ((int) (sizeof(a)/sizeof(a[0])))
#define dolog(...) AUD_log ("sb16", __VA_ARGS__)

/* #define DEBUG */
/* #define DEBUG_SB16_MOST */

#ifdef DEBUG
#define ldebug(...) dolog (__VA_ARGS__)
#else
#define ldebug(...)
#endif

#else

DECLINLINE(void) dolog (const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    AUD_vlog ("sb16", fmt, ap);
    va_end (ap);
}

#ifdef DEBUG
static void ldebug (const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    AUD_vlog ("sb16", fmt, ap);
    va_end (ap);
}
#else
DECLINLINE(void) ldebug (const char *fmt, ...)
{
    (void)fmt;
}
#endif

#endif /* VBOX */

#define IO_READ_PROTO(name)                                             \
        DECLCALLBACK(int) name (PPDMDEVINS pDevIns, void *opaque,       \
                                           RTIOPORT nport, uint32_t *pu32, unsigned cb)

#define IO_WRITE_PROTO(name)                                            \
        DECLCALLBACK(int) name (PPDMDEVINS pDevIns, void *opaque,       \
                                           RTIOPORT nport, uint32_t val, unsigned cb)

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
#endif

typedef struct SB16State {
#ifdef VBOX
    PPDMDEVINS pDevIns;
#endif
    QEMUSoundCard card;
#ifndef VBOX
    qemu_irq *pic;
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
    audfmt_e fmt;
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
    int can_write;

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
    int audio_free;
    SWVoiceOut *voice;

#ifndef VBOX
    QEMUTimer *aux_ts;
#else
    PTMTIMER  pTimer;
    PPDMIBASE pDrvBase;
    PDMIBASE  IBase;
#endif
    /* mixer state */
    int mixer_nreg;
    uint8_t mixer_regs[256];
} SB16State;

static void SB_audio_callback (void *opaque, int free);

static int magic_of_irq (int irq)
{
    switch (irq) {
    case 5:
        return 2;
    case 7:
        return 4;
    case 9:
        return 1;
    case 10:
        return 8;
    default:
        dolog ("bad irq %d\n", irq);
        return 2;
    }
}

static int irq_of_magic (int magic)
{
    switch (magic) {
    case 1:
        return 9;
    case 2:
        return 5;
    case 4:
        return 7;
    case 8:
        return 10;
    default:
        dolog ("bad irq magic %d\n", magic);
        return -1;
    }
}

#if 0
static void log_dsp (SB16State *dsp)
{
    ldebug ("%s:%s:%d:%s:dmasize=%d:freq=%d:const=%d:speaker=%d\n",
            dsp->fmt_stereo ? "Stereo" : "Mono",
            dsp->fmt_signed ? "Signed" : "Unsigned",
            dsp->fmt_bits,
            dsp->dma_auto ? "Auto" : "Single",
            dsp->block_size,
            dsp->freq,
            dsp->time_const,
            dsp->speaker);
}
#endif

static void speaker (SB16State *s, int on)
{
    s->speaker = on;
    /* AUD_enable (s->voice, on); */
}

static void control (SB16State *s, int hold)
{
    int dma = s->use_hdma ? s->hdma : s->dma;
    s->dma_running = hold;

    ldebug ("hold %d high %d dma %d\n", hold, s->use_hdma, dma);

#ifndef VBOX
    if (hold) {
        DMA_hold_DREQ (dma);
        AUD_set_active_out (s->voice, 1);
    }
    else {
        DMA_release_DREQ (dma);
        AUD_set_active_out (s->voice, 0);
    }
#else
    if (hold)
    {
        PDMDevHlpDMASetDREQ (s->pDevIns, dma, 1);
        PDMDevHlpDMASchedule (s->pDevIns);
        AUD_set_active_out (s->voice, 1);
    }
    else
    {
        PDMDevHlpDMASetDREQ (s->pDevIns, dma, 0);
        AUD_set_active_out (s->voice, 0);
    }
#endif
}

#ifndef VBOX
static void aux_timer (void *opaque)
{
    SB16State *s = (SB16State*)opaque;
    s->can_write = 1;
    qemu_irq_raise (s->pic[s->irq]);
}
#else
static DECLCALLBACK(void) sb16Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer)
{
    SB16State *s = PDMINS2DATA(pDevIns, SB16State *);
    s->can_write = 1;
    PDMDevHlpISASetIrq(s->pDevIns, s->irq, 1);
}
#endif

#define DMA8_AUTO 1
#define DMA8_HIGH 2

static void continue_dma8 (SB16State *s)
{
    if (s->freq > 0) {
        audsettings_t as;

        s->audio_free = 0;

        as.freq = s->freq;
        as.nchannels = 1 << s->fmt_stereo;
        as.fmt = s->fmt;
        as.endianness = 0;

        s->voice = AUD_open_out (
            &s->card,
            s->voice,
            "sb16",
            s,
            SB_audio_callback,
            &as
            );
    }

    control (s, 1);
}

static void dma_cmd8 (SB16State *s, int mask, int dma_len)
{
    s->fmt = AUD_FMT_U8;
    s->use_hdma = 0;
    s->fmt_bits = 8;
    s->fmt_signed = 0;
    s->fmt_stereo = (s->mixer_regs[0x0e] & 2) != 0;
    if (-1 == s->time_const) {
        if (s->freq <= 0)
            s->freq = 11025;
    }
    else {
        int tmp = (256 - s->time_const);
        s->freq = (1000000 + (tmp / 2)) / tmp;
    }

    if (dma_len != -1) {
        s->block_size = dma_len << s->fmt_stereo;
    }
    else {
        /* This is apparently the only way to make both Act1/PL
           and SecondReality/FC work

           Act1 sets block size via command 0x48 and it's an odd number
           SR does the same with even number
           Both use stereo, and Creatives own documentation states that
           0x48 sets block size in bytes less one.. go figure */
        s->block_size &= ~s->fmt_stereo;
    }

    s->freq >>= s->fmt_stereo;
    s->left_till_irq = s->block_size;
    s->bytes_per_second = (s->freq << s->fmt_stereo);
    /* s->highspeed = (mask & DMA8_HIGH) != 0; */
    s->dma_auto = (mask & DMA8_AUTO) != 0;
    s->align = (1 << s->fmt_stereo) - 1;

    if (s->block_size & s->align) {
        dolog ("warning: misaligned block size %d, alignment %d\n",
               s->block_size, s->align + 1);
    }

    ldebug ("freq %d, stereo %d, sign %d, bits %d, "
            "dma %d, auto %d, fifo %d, high %d\n",
            s->freq, s->fmt_stereo, s->fmt_signed, s->fmt_bits,
            s->block_size, s->dma_auto, s->fifo, s->highspeed);

    continue_dma8 (s);
    speaker (s, 1);
}

static void dma_cmd (SB16State *s, uint8_t cmd, uint8_t d0, int dma_len)
{
    s->use_hdma = cmd < 0xc0;
    s->fifo = (cmd >> 1) & 1;
    s->dma_auto = (cmd >> 2) & 1;
    s->fmt_signed = (d0 >> 4) & 1;
    s->fmt_stereo = (d0 >> 5) & 1;

    switch (cmd >> 4) {
    case 11:
        s->fmt_bits = 16;
        break;

    case 12:
        s->fmt_bits = 8;
        break;
    }


    if (-1 != s->time_const) {
#if 1
        int tmp = 256 - s->time_const;
        s->freq = (1000000 + (tmp / 2)) / tmp;
#else
        /* s->freq = 1000000 / ((255 - s->time_const) << s->fmt_stereo); */
        s->freq = 1000000 / ((255 - s->time_const));
#endif
        s->time_const = -1;
    }

    s->block_size = dma_len + 1;
    s->block_size <<= (s->fmt_bits == 16);
    if (!s->dma_auto) {
        /* It is clear that for DOOM and auto-init this value
           shouldn't take stereo into account, while Miles Sound Systems
           setsound.exe with single transfer mode wouldn't work without it
           wonders of SB16 yet again */
        s->block_size <<= s->fmt_stereo;
    }

    ldebug ("freq %d, stereo %d, sign %d, bits %d, "
            "dma %d, auto %d, fifo %d, high %d\n",
            s->freq, s->fmt_stereo, s->fmt_signed, s->fmt_bits,
            s->block_size, s->dma_auto, s->fifo, s->highspeed);

    if (16 == s->fmt_bits) {
        if (s->fmt_signed) {
            s->fmt = AUD_FMT_S16;
        }
        else {
            s->fmt = AUD_FMT_U16;
        }
    }
    else {
        if (s->fmt_signed) {
            s->fmt = AUD_FMT_S8;
        }
        else {
            s->fmt = AUD_FMT_U8;
        }
    }

    s->left_till_irq = s->block_size;

    s->bytes_per_second = (s->freq << s->fmt_stereo) << (s->fmt_bits == 16);
    s->highspeed = 0;
    s->align = (1 << (s->fmt_stereo + (s->fmt_bits == 16))) - 1;
    if (s->block_size & s->align) {
        dolog ("warning: misaligned block size %d, alignment %d\n",
               s->block_size, s->align + 1);
    }

    if (s->freq) {
        audsettings_t as;

        s->audio_free = 0;

        as.freq = s->freq;
        as.nchannels = 1 << s->fmt_stereo;
        as.fmt = s->fmt;
        as.endianness = 0;

        s->voice = AUD_open_out (
            &s->card,
            s->voice,
            "sb16",
            s,
            SB_audio_callback,
            &as
            );
    }

    control (s, 1);
    speaker (s, 1);
}

static inline void dsp_out_data (SB16State *s, uint8_t val)
{
    ldebug ("outdata %#x\n", val);
    if ((size_t) s->out_data_len < sizeof (s->out_data)) {
        s->out_data[s->out_data_len++] = val;
    }
}

static inline uint8_t dsp_get_data (SB16State *s)
{
    if (s->in_index) {
        return s->in2_data[--s->in_index];
    }
    else {
        dolog ("buffer underflow\n");
        return 0;
    }
}

static void command (SB16State *s, uint8_t cmd)
{
    ldebug ("command %#x\n", cmd);

    if (cmd > 0xaf && cmd < 0xd0) {
        if (cmd & 8) {
            dolog ("ADC not yet supported (command %#x)\n", cmd);
        }

        switch (cmd >> 4) {
        case 11:
        case 12:
            break;
        default:
            dolog ("%#x wrong bits\n", cmd);
        }
        s->needed_bytes = 3;
    }
    else {
        s->needed_bytes = 0;

        switch (cmd) {
        case 0x03:
            dsp_out_data (s, 0x10); /* s->csp_param); */
            goto warn;

        case 0x04:
            s->needed_bytes = 1;
            goto warn;

        case 0x05:
            s->needed_bytes = 2;
            goto warn;

        case 0x08:
            /* __asm__ ("int3"); */
            goto warn;

        case 0x0e:
            s->needed_bytes = 2;
            goto warn;

        case 0x09:
            dsp_out_data (s, 0xf8);
            goto warn;

        case 0x0f:
            s->needed_bytes = 1;
            goto warn;

        case 0x10:
            s->needed_bytes = 1;
            goto warn;

        case 0x14:
            s->needed_bytes = 2;
            s->block_size = 0;
            break;

        case 0x1c:              /* Auto-Initialize DMA DAC, 8-bit */
            dma_cmd8 (s, DMA8_AUTO, -1);
            break;

        case 0x20:              /* Direct ADC, Juice/PL */
            dsp_out_data (s, 0xff);
            goto warn;

        case 0x35:
            dolog ("0x35 - MIDI command not implemented\n");
            break;

        case 0x40:
            s->freq = -1;
            s->time_const = -1;
            s->needed_bytes = 1;
            break;

        case 0x41:
            s->freq = -1;
            s->time_const = -1;
            s->needed_bytes = 2;
            break;

        case 0x42:
            s->freq = -1;
            s->time_const = -1;
            s->needed_bytes = 2;
            goto warn;

        case 0x45:
            dsp_out_data (s, 0xaa);
            goto warn;

        case 0x47:                /* Continue Auto-Initialize DMA 16bit */
            break;

        case 0x48:
            s->needed_bytes = 2;
            break;

        case 0x74:
            s->needed_bytes = 2; /* DMA DAC, 4-bit ADPCM */
            dolog ("0x75 - DMA DAC, 4-bit ADPCM not implemented\n");
            break;

        case 0x75:              /* DMA DAC, 4-bit ADPCM Reference */
            s->needed_bytes = 2;
            dolog ("0x74 - DMA DAC, 4-bit ADPCM Reference not implemented\n");
            break;

        case 0x76:              /* DMA DAC, 2.6-bit ADPCM */
            s->needed_bytes = 2;
            dolog ("0x74 - DMA DAC, 2.6-bit ADPCM not implemented\n");
            break;

        case 0x77:              /* DMA DAC, 2.6-bit ADPCM Reference */
            s->needed_bytes = 2;
            dolog ("0x74 - DMA DAC, 2.6-bit ADPCM Reference not implemented\n");
            break;

        case 0x7d:
            dolog ("0x7d - Autio-Initialize DMA DAC, 4-bit ADPCM Reference\n");
            dolog ("not implemented\n");
            break;

        case 0x7f:
            dolog (
                "0x7d - Autio-Initialize DMA DAC, 2.6-bit ADPCM Reference\n"
                );
            dolog ("not implemented\n");
            break;

        case 0x80:
            s->needed_bytes = 2;
            break;

        case 0x90:
        case 0x91:
            dma_cmd8 (s, ((cmd & 1) == 0) | DMA8_HIGH, -1);
            break;

        case 0xd0:              /* halt DMA operation. 8bit */
            control (s, 0);
            break;

        case 0xd1:              /* speaker on */
            speaker (s, 1);
            break;

        case 0xd3:              /* speaker off */
            speaker (s, 0);
            break;

        case 0xd4:              /* continue DMA operation. 8bit */
            /* KQ6 (or maybe Sierras audblst.drv in general) resets
               the frequency between halt/continue */
            continue_dma8 (s);
            break;

        case 0xd5:              /* halt DMA operation. 16bit */
            control (s, 0);
            break;

        case 0xd6:              /* continue DMA operation. 16bit */
            control (s, 1);
            break;

        case 0xd9:              /* exit auto-init DMA after this block. 16bit */
            s->dma_auto = 0;
            break;

        case 0xda:              /* exit auto-init DMA after this block. 8bit */
            s->dma_auto = 0;
            break;

        case 0xe0:              /* DSP identification */
            s->needed_bytes = 1;
            break;

        case 0xe1:
            dsp_out_data (s, s->ver & 0xff);
            dsp_out_data (s, s->ver >> 8);
            break;

        case 0xe2:
            s->needed_bytes = 1;
            goto warn;

        case 0xe3:
            {
                int i;
                for (i = sizeof (e3) - 1; i >= 0; --i)
                    dsp_out_data (s, e3[i]);
            }
            break;

        case 0xe4:              /* write test reg */
            s->needed_bytes = 1;
            break;

        case 0xe7:
            dolog ("Attempt to probe for ESS (0xe7)?\n");
            break;

        case 0xe8:              /* read test reg */
            dsp_out_data (s, s->test_reg);
            break;

        case 0xf2:
        case 0xf3:
            dsp_out_data (s, 0xaa);
            s->mixer_regs[0x82] |= (cmd == 0xf2) ? 1 : 2;
#ifndef VBOX
            qemu_irq_raise (s->pic[s->irq]);
#else
            PDMDevHlpISASetIrq(s->pDevIns, s->irq, 1);
#endif
            break;

        case 0xf9:
            s->needed_bytes = 1;
            goto warn;

        case 0xfa:
            dsp_out_data (s, 0);
            goto warn;

        case 0xfc:              /* FIXME */
            dsp_out_data (s, 0);
            goto warn;

        default:
            dolog ("Unrecognized command %#x\n", cmd);
            break;
        }
    }

    if (!s->needed_bytes) {
        ldebug ("\n");
    }

 exit:
    if (!s->needed_bytes) {
        s->cmd = -1;
    }
    else {
        s->cmd = cmd;
    }
    return;

 warn:
    dolog ("warning: command %#x,%d is not truly understood yet\n",
           cmd, s->needed_bytes);
    goto exit;

}

static uint16_t dsp_get_lohi (SB16State *s)
{
    uint8_t hi = dsp_get_data (s);
    uint8_t lo = dsp_get_data (s);
    return (hi << 8) | lo;
}

static uint16_t dsp_get_hilo (SB16State *s)
{
    uint8_t lo = dsp_get_data (s);
    uint8_t hi = dsp_get_data (s);
    return (hi << 8) | lo;
}

static void complete (SB16State *s)
{
    int d0, d1, d2;
    ldebug ("complete command %#x, in_index %d, needed_bytes %d\n",
            s->cmd, s->in_index, s->needed_bytes);

    if (s->cmd > 0xaf && s->cmd < 0xd0) {
        d2 = dsp_get_data (s);
        d1 = dsp_get_data (s);
        d0 = dsp_get_data (s);

        if (s->cmd & 8) {
            dolog ("ADC params cmd = %#x d0 = %d, d1 = %d, d2 = %d\n",
                   s->cmd, d0, d1, d2);
        }
        else {
            ldebug ("cmd = %#x d0 = %d, d1 = %d, d2 = %d\n",
                    s->cmd, d0, d1, d2);
            dma_cmd (s, s->cmd, d0, d1 + (d2 << 8));
        }
    }
    else {
        switch (s->cmd) {
        case 0x04:
            s->csp_mode = dsp_get_data (s);
            s->csp_reg83r = 0;
            s->csp_reg83w = 0;
            ldebug ("CSP command 0x04: mode=%#x\n", s->csp_mode);
            break;

        case 0x05:
            s->csp_param = dsp_get_data (s);
            s->csp_value = dsp_get_data (s);
            ldebug ("CSP command 0x05: param=%#x value=%#x\n",
                    s->csp_param,
                    s->csp_value);
            break;

        case 0x0e:
            d0 = dsp_get_data (s);
            d1 = dsp_get_data (s);
            ldebug ("write CSP register %d <- %#x\n", d1, d0);
            if (d1 == 0x83) {
                ldebug ("0x83[%d] <- %#x\n", s->csp_reg83r, d0);
                s->csp_reg83[s->csp_reg83r % 4] = d0;
                s->csp_reg83r += 1;
            }
            else {
                s->csp_regs[d1] = d0;
            }
            break;

        case 0x0f:
            d0 = dsp_get_data (s);
            ldebug ("read CSP register %#x -> %#x, mode=%#x\n",
                    d0, s->csp_regs[d0], s->csp_mode);
            if (d0 == 0x83) {
                ldebug ("0x83[%d] -> %#x\n",
                        s->csp_reg83w,
                        s->csp_reg83[s->csp_reg83w % 4]);
                dsp_out_data (s, s->csp_reg83[s->csp_reg83w % 4]);
                s->csp_reg83w += 1;
            }
            else {
                dsp_out_data (s, s->csp_regs[d0]);
            }
            break;

        case 0x10:
            d0 = dsp_get_data (s);
            dolog ("cmd 0x10 d0=%#x\n", d0);
            break;

        case 0x14:
            dma_cmd8 (s, 0, dsp_get_lohi (s) + 1);
            break;

        case 0x40:
            s->time_const = dsp_get_data (s);
            ldebug ("set time const %d\n", s->time_const);
            break;

        case 0x42:              /* FT2 sets output freq with this, go figure */
#if 0
            dolog ("cmd 0x42 might not do what it think it should\n");
#endif
        case 0x41:
            s->freq = dsp_get_hilo (s);
            ldebug ("set freq %d\n", s->freq);
            break;

        case 0x48:
            s->block_size = dsp_get_lohi (s) + 1;
            ldebug ("set dma block len %d\n", s->block_size);
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

                freq = s->freq > 0 ? s->freq : 11025;
                samples = dsp_get_lohi (s) + 1;
                bytes = samples << s->fmt_stereo << (s->fmt_bits == 16);
#ifndef VBOX
                ticks = (bytes * ticks_per_sec) / freq;
                if (ticks < ticks_per_sec / 1024) {
                    qemu_irq_raise (s->pic[s->irq]);
                }
                else {
                    if (s->aux_ts) {
                        qemu_mod_timer (
                            s->aux_ts,
                            qemu_get_clock (vm_clock) + ticks
                            );
                    }
                }
#else
                ticks = (bytes * TMTimerGetFreq(s->pTimer)) / freq;
                if (ticks < TMTimerGetFreq(s->pTimer) / 1024)
                {
                    PDMDevHlpISASetIrq(s->pDevIns, s->irq, 1);
                }
                else
                    TMTimerSet(s->pTimer, TMTimerGet(s->pTimer) + ticks);
#endif
                ldebug ("mix silence %d %d % %RU64\n", samples, bytes, ticks);
            }
            break;

        case 0xe0:
            d0 = dsp_get_data (s);
            s->out_data_len = 0;
            ldebug ("E0 data = %#x\n", d0);
            dsp_out_data (s, ~d0);
            break;

        case 0xe2:
            d0 = dsp_get_data (s);
            ldebug ("E2 = %#x\n", d0);
            break;

        case 0xe4:
            s->test_reg = dsp_get_data (s);
            break;

        case 0xf9:
            d0 = dsp_get_data (s);
            ldebug ("command 0xf9 with %#x\n", d0);
            switch (d0) {
            case 0x0e:
                dsp_out_data (s, 0xff);
                break;

            case 0x0f:
                dsp_out_data (s, 0x07);
                break;

            case 0x37:
                dsp_out_data (s, 0x38);
                break;

            default:
                dsp_out_data (s, 0x00);
                break;
            }
            break;

        default:
            dolog ("complete: unrecognized command %#x\n", s->cmd);
            return;
        }
    }

    ldebug ("\n");
    s->cmd = -1;
    return;
}

static void legacy_reset (SB16State *s)
{
    audsettings_t as;

    s->freq = 11025;
    s->fmt_signed = 0;
    s->fmt_bits = 8;
    s->fmt_stereo = 0;

    as.freq = s->freq;
    as.nchannels = 1;
    as.fmt = AUD_FMT_U8;
    as.endianness = 0;

    s->voice = AUD_open_out (
        &s->card,
        s->voice,
        "sb16",
        s,
        SB_audio_callback,
        &as
        );

    /* Not sure about that... */
    /* AUD_set_active_out (s->voice, 1); */
}

static void reset (SB16State *s)
{
#ifndef VBOX
    qemu_irq_lower (s->pic[s->irq]);
    if (s->dma_auto) {
        qemu_irq_raise (s->pic[s->irq]);
        qemu_irq_lower (s->pic[s->irq]);
    }
#else
    PDMDevHlpISASetIrq(s->pDevIns, s->irq, 0);
    if (s->dma_auto) {
        PDMDevHlpISASetIrq(s->pDevIns, s->irq, 1);
        PDMDevHlpISASetIrq(s->pDevIns, s->irq, 0);
    }
#endif

    s->mixer_regs[0x82] = 0;
    s->dma_auto = 0;
    s->in_index = 0;
    s->out_data_len = 0;
    s->left_till_irq = 0;
    s->needed_bytes = 0;
    s->block_size = -1;
    s->nzero = 0;
    s->highspeed = 0;
    s->v2x6 = 0;
    s->cmd = -1;

    dsp_out_data(s, 0xaa);
    speaker (s, 0);
    control (s, 0);
    legacy_reset (s);
}

static IO_WRITE_PROTO (dsp_write)
{
    SB16State *s = (SB16State*)opaque;
    int iport = nport - s->port;

    ldebug ("write %#x <- %#x\n", nport, val);

    switch (iport) {
    case 0x06:
        switch (val) {
        case 0x00:
            if (s->v2x6 == 1) {
                if (0 && s->highspeed) {
                    s->highspeed = 0;
#ifndef VBOX
                    qemu_irq_lower (s->pic[s->irq]);
#else
                    PDMDevHlpISASetIrq(s->pDevIns, s->irq, 0);
#endif
                    control (s, 0);
                }
                else {
                    reset (s);
                }
            }
            s->v2x6 = 0;
            break;

        case 0x01:
        case 0x03:              /* FreeBSD kludge */
            s->v2x6 = 1;
            break;

        case 0xc6:
            s->v2x6 = 0;        /* Prince of Persia, csp.sys, diagnose.exe */
            break;

        case 0xb8:              /* Panic */
            reset (s);
            break;

        case 0x39:
            dsp_out_data (s, 0x38);
            reset (s);
            s->v2x6 = 0x39;
            break;

        default:
            s->v2x6 = val;
            break;
        }
        break;

    case 0x0c:                  /* write data or command | write status */
/*         if (s->highspeed) */
/*             break; */

        if (0 == s->needed_bytes) {
            command (s, val);
#if 0
            if (0 == s->needed_bytes) {
                log_dsp (s);
            }
#endif
        }
        else {
            if (s->in_index == sizeof (s->in2_data)) {
                dolog ("in data overrun\n");
            }
            else {
                s->in2_data[s->in_index++] = val;
                if (s->in_index == s->needed_bytes) {
                    s->needed_bytes = 0;
                    complete (s);
#if 0
                    log_dsp (s);
#endif
                }
            }
        }
        break;

    default:
        ldebug ("(nport=%#x, val=%#x)\n", nport, val);
        break;
    }

    return VINF_SUCCESS;
}

static IO_READ_PROTO (dsp_read)
{
    SB16State *s = (SB16State*)opaque;
    int iport, retval, ack = 0;

    iport = nport - s->port;

    switch (iport) {
    case 0x06:                  /* reset */
        retval = 0xff;
        break;

    case 0x0a:                  /* read data */
        if (s->out_data_len) {
            retval = s->out_data[--s->out_data_len];
            s->last_read_byte = retval;
        }
        else {
            if (s->cmd != -1) {
                dolog ("empty output buffer for command %#x\n",
                       s->cmd);
            }
            retval = s->last_read_byte;
            /* goto error; */
        }
        break;

    case 0x0c:                  /* 0 can write */
        retval = s->can_write ? 0 : 0x80;
        break;

    case 0x0d:                  /* timer interrupt clear */
        /* dolog ("timer interrupt clear\n"); */
        retval = 0;
        break;

    case 0x0e:                  /* data available status | irq 8 ack */
        retval = (!s->out_data_len || s->highspeed) ? 0 : 0x80;
        if (s->mixer_regs[0x82] & 1) {
            ack = 1;
            s->mixer_regs[0x82] &= 1;
#ifndef VBOX
            qemu_irq_lower (s->pic[s->irq]);
#else
            PDMDevHlpISASetIrq(s->pDevIns, s->irq, 0);
#endif
        }
        break;

    case 0x0f:                  /* irq 16 ack */
        retval = 0xff;
        if (s->mixer_regs[0x82] & 2) {
            ack = 1;
            s->mixer_regs[0x82] &= 2;
#ifndef VBOX
            qemu_irq_lower (s->pic[s->irq]);
#else
            PDMDevHlpISASetIrq(s->pDevIns, s->irq, 0);
#endif
        }
        break;

    default:
        goto error;
    }

    if (!ack) {
        ldebug ("read %#x -> %#x\n", nport, retval);
    }

#ifndef VBOX
    return retval;
#else
    *pu32 = retval;
    return VINF_SUCCESS;
#endif

 error:
    dolog ("warning: dsp_read %#x error\n", nport);
#ifndef VBOX
    return 0xff;
#else
    return VERR_IOM_IOPORT_UNUSED;
#endif
}

static void reset_mixer (SB16State *s)
{
    int i;

    memset (s->mixer_regs, 0xff, 0x7f);
    memset (s->mixer_regs + 0x83, 0xff, sizeof (s->mixer_regs) - 0x83);

    s->mixer_regs[0x02] = 4;    /* master volume 3bits */
    s->mixer_regs[0x06] = 4;    /* MIDI volume 3bits */
    s->mixer_regs[0x08] = 0;    /* CD volume 3bits */
    s->mixer_regs[0x0a] = 0;    /* voice volume 2bits */

    /* d5=input filt, d3=lowpass filt, d1,d2=input source */
    s->mixer_regs[0x0c] = 0;

    /* d5=output filt, d1=stereo switch */
    s->mixer_regs[0x0e] = 0;

    /* voice volume L d5,d7, R d1,d3 */
    s->mixer_regs[0x04] = (4 << 5) | (4 << 1);
    /* master ... */
    s->mixer_regs[0x22] = (4 << 5) | (4 << 1);
    /* MIDI ... */
    s->mixer_regs[0x26] = (4 << 5) | (4 << 1);

    for (i = 0x30; i < 0x48; i++) {
        s->mixer_regs[i] = 0x20;
    }
}

static IO_WRITE_PROTO(mixer_write_indexb)
{
    SB16State *s = (SB16State*)opaque;
    (void) nport;
    s->mixer_nreg = val;

    return VINF_SUCCESS;
}

static IO_WRITE_PROTO(mixer_write_datab)
{
    SB16State *s = (SB16State*)opaque;

    (void) nport;
    ldebug ("mixer_write [%#x] <- %#x\n", s->mixer_nreg, val);

    switch (s->mixer_nreg) {
    case 0x00:
        reset_mixer (s);
        break;

    case 0x80:
        {
            int irq = irq_of_magic (val);
            ldebug ("setting irq to %d (val=%#x)\n", irq, val);
            if (irq > 0) {
                s->irq = irq;
            }
        }
        break;

    case 0x81:
        {
            int dma, hdma;

            dma = lsbindex (val & 0xf);
            hdma = lsbindex (val & 0xf0);
            if (dma != s->dma || hdma != s->hdma) {
                dolog (
                    "attempt to change DMA "
                    "8bit %d(%d), 16bit %d(%d) (val=%#x)\n",
                    dma, s->dma, hdma, s->hdma, val);
            }
#if 0
            s->dma = dma;
            s->hdma = hdma;
#endif
        }
        break;

    case 0x82:
        dolog ("attempt to write into IRQ status register (val=%#x)\n",
               val);
        return VINF_SUCCESS;

    default:
        if (s->mixer_nreg >= 0x80) {
            ldebug ("attempt to write mixer[%#x] <- %#x\n", s->mixer_nreg, val);
        }
        break;
    }

    s->mixer_regs[s->mixer_nreg] = val;

    if (s->mixer_nreg == 0x30 || s->mixer_nreg == 0x31)
    {
        int     mute = 0;
        uint8_t lvol = s->mixer_regs[0x30];
        uint8_t rvol = s->mixer_regs[0x31];
        AUD_set_volume (AUD_MIXER_VOLUME, &mute, &lvol, &rvol);
    }

    return VINF_SUCCESS;
}

static IO_WRITE_PROTO(mixer_write)
{
    SB16State *s = (SB16State*)opaque;
    int iport = nport - s->port;
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
}

static IO_READ_PROTO(mixer_read)
{
    SB16State *s = (SB16State*)opaque;

    (void) nport;
#ifndef DEBUG_SB16_MOST
    if (s->mixer_nreg != 0x82) {
        ldebug ("mixer_read[%#x] -> %#x\n",
                s->mixer_nreg, s->mixer_regs[s->mixer_nreg]);
    }
#else
    ldebug ("mixer_read[%#x] -> %#x\n",
            s->mixer_nreg, s->mixer_regs[s->mixer_nreg]);
#endif
#ifndef VBOX
    return s->mixer_regs[s->mixer_nreg];
#else
    *pu32 = s->mixer_regs[s->mixer_nreg];
    return VINF_SUCCESS;
#endif
}

static int write_audio (SB16State *s, int nchan, int dma_pos,
                        int dma_len, int len)
{
    int temp, net;
    uint8_t tmpbuf[4096];

    temp = len;
    net = 0;

    while (temp) {
        int left = dma_len - dma_pos;
#ifndef VBOX
        int copied;
#else
        uint32_t copied;
#endif
        size_t to_copy;

        to_copy = audio_MIN (temp, left);
        if (to_copy > sizeof (tmpbuf)) {
            to_copy = sizeof (tmpbuf);
        }

#ifndef VBOX
        copied = DMA_read_memory (nchan, tmpbuf, dma_pos, to_copy);
#else
        int rc = PDMDevHlpDMAReadMemory(s->pDevIns, nchan, tmpbuf, dma_pos,
                                        to_copy, &copied);
        AssertMsgRC (rc, ("DMAReadMemory -> %Vrc\n", rc));
#endif

        copied = AUD_write (s->voice, tmpbuf, copied);

        temp -= copied;
        dma_pos = (dma_pos + copied) % dma_len;
        net += copied;

        if (!copied) {
            break;
        }
    }

    return net;
}

#ifndef VBOX
static int SB_read_DMA (void *opaque, int nchan, int dma_pos, int dma_len)
#else
static DECLCALLBACK(uint32_t) SB_read_DMA (PPDMDEVINS pDevIns, void *opaque, unsigned nchan, uint32_t dma_pos, uint32_t dma_len)
#endif
{
    SB16State *s = (SB16State*)opaque;
    int till, copy, written, free;

    if (s->block_size <= 0) {
        dolog ("invalid block size=%d nchan=%d dma_pos=%d dma_len=%d\n",
               s->block_size, nchan, dma_pos, dma_len);
        return dma_pos;
    }

    if (s->left_till_irq < 0) {
        s->left_till_irq = s->block_size;
    }

    if (s->voice) {
        free = s->audio_free & ~s->align;
        if ((free <= 0) || !dma_len) {
            return dma_pos;
        }
    }
    else {
        free = dma_len;
    }

    copy = free;
    till = s->left_till_irq;

#ifdef DEBUG_SB16_MOST
    dolog ("pos:%06d %d till:%d len:%d\n",
           dma_pos, free, till, dma_len);
#endif

    if (till <= copy) {
        if (0 == s->dma_auto) {
            copy = till;
        }
    }

    written = write_audio (s, nchan, dma_pos, dma_len, copy);
    dma_pos = (dma_pos + written) % dma_len;
    s->left_till_irq -= written;

    if (s->left_till_irq <= 0) {
        s->mixer_regs[0x82] |= (nchan & 4) ? 2 : 1;
#ifndef VBOX
        qemu_irq_raise (s->pic[s->irq]);
#else
        PDMDevHlpISASetIrq(s->pDevIns, s->irq, 1);
#endif
        if (0 == s->dma_auto) {
            control (s, 0);
            speaker (s, 0);
        }
    }

#ifdef DEBUG_SB16_MOST
    ldebug ("pos %5d free %5d size %5d till % 5d copy %5d written %5d size %5d\n",
            dma_pos, free, dma_len, s->left_till_irq, copy, written,
            s->block_size);
#endif

    while (s->left_till_irq <= 0) {
        s->left_till_irq = s->block_size + s->left_till_irq;
    }

    return dma_pos;
}

static void SB_audio_callback (void *opaque, int free)
{
    SB16State *s = (SB16State*)opaque;
    s->audio_free = free;
#ifdef VBOX
    /* New space available, see if we can transfer more. There is no cyclic DMA timer in VBox. */
    PDMDevHlpDMASchedule (s->pDevIns);
#endif
}

static DECLCALLBACK(int) SaveExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    SB16State *s = PDMINS2DATA (pDevIns, SB16State *);
    QEMUFile *f = pSSMHandle;

    qemu_put_be32 (f, s->irq);
    qemu_put_be32 (f, s->dma);
    qemu_put_be32 (f, s->hdma);
    qemu_put_be32 (f, s->port);
    qemu_put_be32 (f, s->ver);
    qemu_put_be32 (f, s->in_index);
    qemu_put_be32 (f, s->out_data_len);
    qemu_put_be32 (f, s->fmt_stereo);
    qemu_put_be32 (f, s->fmt_signed);
    qemu_put_be32 (f, s->fmt_bits);
    qemu_put_be32s (f, &s->fmt);
    qemu_put_be32 (f, s->dma_auto);
    qemu_put_be32 (f, s->block_size);
    qemu_put_be32 (f, s->fifo);
    qemu_put_be32 (f, s->freq);
    qemu_put_be32 (f, s->time_const);
    qemu_put_be32 (f, s->speaker);
    qemu_put_be32 (f, s->needed_bytes);
    qemu_put_be32 (f, s->cmd);
    qemu_put_be32 (f, s->use_hdma);
    qemu_put_be32 (f, s->highspeed);
    qemu_put_be32 (f, s->can_write);
    qemu_put_be32 (f, s->v2x6);

    qemu_put_8s (f, &s->csp_param);
    qemu_put_8s (f, &s->csp_value);
    qemu_put_8s (f, &s->csp_mode);
    qemu_put_8s (f, &s->csp_param);
    qemu_put_buffer (f, s->csp_regs, 256);
    qemu_put_8s (f, &s->csp_index);
    qemu_put_buffer (f, s->csp_reg83, 4);
    qemu_put_be32 (f, s->csp_reg83r);
    qemu_put_be32 (f, s->csp_reg83w);

    qemu_put_buffer (f, s->in2_data, sizeof (s->in2_data));
    qemu_put_buffer (f, s->out_data, sizeof (s->out_data));
    qemu_put_8s (f, &s->test_reg);
    qemu_put_8s (f, &s->last_read_byte);

    qemu_put_be32 (f, s->nzero);
    qemu_put_be32 (f, s->left_till_irq);
    qemu_put_be32 (f, s->dma_running);
    qemu_put_be32 (f, s->bytes_per_second);
    qemu_put_be32 (f, s->align);

    qemu_put_be32 (f, s->mixer_nreg);
    qemu_put_buffer (f, s->mixer_regs, 256);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) LoadExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle,
                                   uint32_t u32Version)
{
    SB16State *s = PDMINS2DATA (pDevIns, SB16State *);
    QEMUFile *f = pSSMHandle;

    if (u32Version != SB16_SSM_VERSION)
    {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    s->irq=qemu_get_be32 (f);
    s->dma=qemu_get_be32 (f);
    s->hdma=qemu_get_be32 (f);
    s->port=qemu_get_be32 (f);
    s->ver=qemu_get_be32 (f);
    s->in_index=qemu_get_be32 (f);
    s->out_data_len=qemu_get_be32 (f);
    s->fmt_stereo=qemu_get_be32 (f);
    s->fmt_signed=qemu_get_be32 (f);
    s->fmt_bits=qemu_get_be32 (f);
    qemu_get_be32s (f, (uint32_t*)&s->fmt);
    s->dma_auto=qemu_get_be32 (f);
    s->block_size=qemu_get_be32 (f);
    s->fifo=qemu_get_be32 (f);
    s->freq=qemu_get_be32 (f);
    s->time_const=qemu_get_be32 (f);
    s->speaker=qemu_get_be32 (f);
    s->needed_bytes=qemu_get_be32 (f);
    s->cmd=qemu_get_be32 (f);
    s->use_hdma=qemu_get_be32 (f);
    s->highspeed=qemu_get_be32 (f);
    s->can_write=qemu_get_be32 (f);
    s->v2x6=qemu_get_be32 (f);

    qemu_get_8s (f, &s->csp_param);
    qemu_get_8s (f, &s->csp_value);
    qemu_get_8s (f, &s->csp_mode);
    qemu_get_8s (f, &s->csp_param);
    qemu_get_buffer (f, s->csp_regs, 256);
    qemu_get_8s (f, &s->csp_index);
    qemu_get_buffer (f, s->csp_reg83, 4);
    s->csp_reg83r=qemu_get_be32 (f);
    s->csp_reg83w=qemu_get_be32 (f);

    qemu_get_buffer (f, s->in2_data, sizeof (s->in2_data));
    qemu_get_buffer (f, s->out_data, sizeof (s->out_data));
    qemu_get_8s (f, &s->test_reg);
    qemu_get_8s (f, &s->last_read_byte);

    s->nzero=qemu_get_be32 (f);
    s->left_till_irq=qemu_get_be32 (f);
    s->dma_running=qemu_get_be32 (f);
    s->bytes_per_second=qemu_get_be32 (f);
    s->align=qemu_get_be32 (f);

    s->mixer_nreg=qemu_get_be32 (f);
    qemu_get_buffer (f, s->mixer_regs, 256);

    if (s->voice) {
        AUD_close_out (&s->card, s->voice);
        s->voice = NULL;
    }

    if (s->dma_running) {
        if (s->freq) {
            audsettings_t as;

            s->audio_free = 0;

            as.freq = s->freq;
            as.nchannels = 1 << s->fmt_stereo;
            as.fmt = s->fmt;
            as.endianness = 0;

            s->voice = AUD_open_out (
                &s->card,
                s->voice,
                "sb16",
                s,
                SB_audio_callback,
                &as
                );
        }

        control (s, 1);
        speaker (s, s->speaker);
    }

    return VINF_SUCCESS;
}

#ifndef VBOX
int SB16_init (AudioState *audio, qemu_irq *pic)
{
    SB16State *s;
    int i;
    static const uint8_t dsp_write_ports[] = {0x6, 0xc};
    static const uint8_t dsp_read_ports[] = {0x6, 0xa, 0xc, 0xd, 0xe, 0xf};

    if (!audio) {
        dolog ("No audio state\n");
        return -1;
    }

    s = qemu_mallocz (sizeof (*s));
    if (!s) {
        dolog ("Could not allocate memory for SB16 (%zu bytes)\n",
               sizeof (*s));
        return -1;
    }

    s->cmd = -1;
    s->pic = pic;
    s->irq = conf.irq;
    s->dma = conf.dma;
    s->hdma = conf.hdma;
    s->port = conf.port;
    s->ver = conf.ver_lo | (conf.ver_hi << 8);

    s->mixer_regs[0x80] = magic_of_irq (s->irq);
    s->mixer_regs[0x81] = (1 << s->dma) | (1 << s->hdma);
    s->mixer_regs[0x82] = 2 << 5;

    s->csp_regs[5] = 1;
    s->csp_regs[9] = 0xf8;

    reset_mixer (s);
    s->aux_ts = qemu_new_timer (vm_clock, aux_timer, s);
    if (!s->aux_ts) {
        dolog ("warning: Could not create auxiliary timer\n");
    }

    for (i = 0; i < LENOFA (dsp_write_ports); i++) {
        register_ioport_write (s->port + dsp_write_ports[i], 1, 1, dsp_write, s);
    }

    for (i = 0; i < LENOFA (dsp_read_ports); i++) {
        register_ioport_read (s->port + dsp_read_ports[i], 1, 1, dsp_read, s);
    }

    register_ioport_write (s->port + 0x4, 1, 1, mixer_write_indexb, s);
    register_ioport_write (s->port + 0x4, 1, 2, mixer_write_indexw, s);
    register_ioport_read (s->port + 0x5, 1, 1, mixer_read, s);
    register_ioport_write (s->port + 0x5, 1, 1, mixer_write_datab, s);

    DMA_register_channel (s->hdma, SB_read_DMA, s);
    DMA_register_channel (s->dma, SB_read_DMA, s);
    s->can_write = 1;

    register_savevm ("sb16", 0, 1, SB_save, SB_load, s);
    AUD_register_card (audio, "sb16", &s->card);
    return 0;
}
#endif

static DECLCALLBACK(void *) sb16QueryInterface (struct PDMIBASE *pInterface,
                                                PDMINTERFACE enmInterface)
{
    SB16State *pData = (SB16State *)((uintptr_t)pInterface
                     - RT_OFFSETOF(SB16State, IBase));
    Assert(&pData->IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        default:
            return NULL;
    }
}

static DECLCALLBACK(int) sb16Construct (PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    SB16State *s = PDMINS2DATA(pDevIns, SB16State *);
    int rc;

    Assert(iInstance == 0);

    uint32_t value;
    if (!CFGMR3AreValuesValid(pCfgHandle, "IRQ\0DMA\0DMA16\0Port\0Version\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Invalid configuraton for sb16 device"));
    rc = CFGMR3QueryU32(pCfgHandle, "IRQ", &value);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->irq = 5;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"IRQ\" value"));
    else s->irq = value;
    rc = CFGMR3QueryU32(pCfgHandle, "DMA", &value);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->dma = 1;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DMA\" value"));
    else
        s->dma = value;
    rc = CFGMR3QueryU32(pCfgHandle, "DMA16", &value);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->hdma = 5;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DMA16\" value"));
    else
        s->hdma = value;
    rc = CFGMR3QueryU32(pCfgHandle, "Port", &value);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->port = 0x220;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Port\" value"));
    else
        s->port = value;
    uint16_t version;
    rc = CFGMR3QueryU16(pCfgHandle, "Version", &version);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        s->ver = 0x0405;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"Version\" value"));
    else
        s->ver = version;

    s->pDevIns                 = pDevIns;
    s->IBase.pfnQueryInterface = sb16QueryInterface;
    s->cmd                     = -1;

    s->mixer_regs[0x80]        = magic_of_irq (s->irq);
    s->mixer_regs[0x81]        = (1 << s->dma) | (1 << s->hdma);
    s->mixer_regs[0x82]        = 2 << 5;

    s->csp_regs[5]             = 1;
    s->csp_regs[9]             = 0xf8;

    reset_mixer (s);

    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, sb16Timer, "SB16 timer", &s->pTimer);
    if (VBOX_FAILURE(rc))
        AssertMsgFailedReturn(("pfnTMTimerCreate -> %Vrc\n", rc), rc);

    rc = PDMDevHlpIOPortRegister (pDevIns, s->port + 0x04,  2, s,
                                  mixer_write, mixer_read, NULL, NULL, "SB16");
    if (VBOX_FAILURE (rc))
        return rc;
    rc = PDMDevHlpIOPortRegister (pDevIns, s->port + 0x06, 10, s,
                                  dsp_write, dsp_read, NULL, NULL, "SB16");
    if (VBOX_FAILURE (rc))
        return rc;

    rc = PDMDevHlpDMARegister (pDevIns, s->hdma, SB_read_DMA, s);
    if (VBOX_FAILURE (rc))
        return rc;
    rc = PDMDevHlpDMARegister (pDevIns, s->dma, SB_read_DMA, s);
    if (VBOX_FAILURE (rc))
        return rc;

    s->can_write = 1;

    rc = PDMDevHlpSSMRegister (pDevIns, pDevIns->pDevReg->szDeviceName, iInstance, SB16_SSM_VERSION,
                               sizeof (*s), NULL, SaveExec, NULL, NULL, LoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    rc = PDMDevHlpDriverAttach (pDevIns, 0, &s->IBase, &s->pDrvBase, "Audio Driver Port");
    if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log (("sb16: No attached driver!\n"));
    else if (VBOX_FAILURE(rc))
        AssertMsgFailedReturn(("Failed to attach SB16 LUN #0! rc=%Vrc\n", rc), rc);

    AUD_register_card ("sb16", &s->card);
    legacy_reset (s);

    if (!s->voice)
    {
        AUD_close_out(&s->card, s->voice);
        s->voice = NULL;
        AUD_init_null();
        PDMDevHlpVMSetRuntimeError(pDevIns, false, "HostAudioNotResponding",
                N_("No audio devices could be opened. Selecting the NULL audio backend "
                   "with the consequence that no sound is audible."));
    }
    LogRel(("SB16\n"));
    return 0;
}

const PDMDEVREG g_DeviceSB16 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "sb16",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Sound Blaster 16 Controller",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* bInstance */
    sizeof(SB16State),
    /* pfnConstruct */
    sb16Construct,
    /* pfnDesctruct */
    NULL,
    /* pfnIOCtl */
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
    /* pfnQueryInterface. */
    NULL
};
