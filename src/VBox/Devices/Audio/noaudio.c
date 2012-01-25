/*
 * QEMU Timer based audio emulation
 *
 * Copyright (c) 2004-2005 Vassili Karpov (malc)
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
#include "VBoxDD.h"
#include "vl_vbox.h"
#include "audio.h"
#include <iprt/alloc.h>

#define AUDIO_CAP "noaudio"
#include "audio_int.h"

typedef struct NoVoiceOut {
    HWVoiceOut hw;
    int64_t old_ticks;
} NoVoiceOut;

typedef struct NoVoiceIn {
    HWVoiceIn hw;
    int64_t old_ticks;
} NoVoiceIn;

static int no_run_out (HWVoiceOut *hw)
{
    NoVoiceOut *no = (NoVoiceOut *) hw;
    int live, decr, samples;
    int64_t now;
    int64_t ticks;
    int64_t ticks_per_second;

    live = audio_pcm_hw_get_live_out (&no->hw);
    if (!live) {
        return 0;
    }

    now = audio_get_clock ();
    ticks = now - no->old_ticks;

    ticks_per_second = audio_get_ticks_per_sec ();
    /* Minimize the rounding error: samples = int((ticks * freq) / ticks_per_second + 0.5). */
    samples = (2 * ticks * hw->info.freq + ticks_per_second) / ticks_per_second / 2;

    no->old_ticks = now;
    decr = audio_MIN (live, samples);
    hw->rpos = (hw->rpos + decr) % hw->samples;
    return decr;
}

static int no_write (SWVoiceOut *sw, void *buf, int len)
{
    return audio_pcm_sw_write (sw, buf, len);
}

static int no_init_out (HWVoiceOut *hw, audsettings_t *as)
{
    audio_pcm_init_info (&hw->info, as);
    hw->samples = 1024;
    return 0;
}

static void no_fini_out (HWVoiceOut *hw)
{
    (void) hw;
}

static int no_ctl_out (HWVoiceOut *hw, int cmd, ...)
{
    (void) hw;
    (void) cmd;
    return 0;
}

static int no_init_in (HWVoiceIn *hw, audsettings_t *as)
{
    audio_pcm_init_info (&hw->info, as);
    hw->samples = 1024;
    return 0;
}

static void no_fini_in (HWVoiceIn *hw)
{
    (void) hw;
}

static int no_run_in (HWVoiceIn *hw)
{
    NoVoiceIn *no = (NoVoiceIn *) hw;
    int live = audio_pcm_hw_get_live_in (hw);
    int dead = hw->samples - live;
    int samples = 0;

    if (dead) {
        int64_t now = audio_get_clock ();
        int64_t ticks = now - no->old_ticks;
        int64_t bytes = (ticks * hw->info.bytes_per_second)
            / audio_get_ticks_per_sec ();

        no->old_ticks = now;
        bytes = audio_MIN (bytes, INT_MAX);
        samples = bytes >> hw->info.shift;
        samples = audio_MIN (samples, dead);
        hw->wpos = (hw->wpos + samples) % hw->samples;
    }
    return samples;
}

static int no_read (SWVoiceIn *sw, void *buf, int size)
{
    return audio_pcm_sw_read (sw, buf, size);
}

static int no_ctl_in (HWVoiceIn *hw, int cmd, ...)
{
    (void) hw;
    (void) cmd;
    return 0;
}

static void *no_audio_init (void)
{
    return &no_audio_driver;
}

static void no_audio_fini (void *opaque)
{
    (void) opaque;
}

static struct audio_pcm_ops no_pcm_ops = {
    no_init_out,
    no_fini_out,
    no_run_out,
    no_write,
    no_ctl_out,

    no_init_in,
    no_fini_in,
    no_run_in,
    no_read,
    no_ctl_in
};

struct audio_driver no_audio_driver = {
    INIT_FIELD (name           = ) "null",
    INIT_FIELD (descr          = ) "Timer based audio emulation",
    INIT_FIELD (options        = ) NULL,
    INIT_FIELD (init           = ) no_audio_init,
    INIT_FIELD (fini           = ) no_audio_fini,
    INIT_FIELD (pcm_ops        = ) &no_pcm_ops,
    INIT_FIELD (can_be_default = ) 1,
    INIT_FIELD (max_voices_out = ) INT_MAX,
    INIT_FIELD (max_voices_in  = ) INT_MAX,
    INIT_FIELD (voice_size_out = ) sizeof (NoVoiceOut),
    INIT_FIELD (voice_size_in  = ) sizeof (NoVoiceIn)
};
