/* $Id$ */
/** @file
 * Intermedia audio driver..
 *
 * @remarks Intermediate audio driver having audio device as one of the sink and
 *          host backend as other..
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
 * hope that it will be1 useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: audio.c from QEMU AUDIO subsystem.
 *
 * QEMU Audio subsystem
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
#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <VBox/log.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include "VBoxDD.h"
#include "vl_vbox.h"

#include <ctype.h>
#include <stdlib.h>

#define AUDIO_CAP "audio"
#include "DrvAudio.h"

#ifdef RT_OS_WINDOWS
#define strcasecmp stricmp
#endif

static struct audio_driver *drvtab[] = {
#if defined (RT_OS_LINUX) || defined (RT_OS_FREEBSD) || defined(VBOX_WITH_SOLARIS_OSS)
    &oss_audio_driver,
#endif
#ifdef RT_OS_LINUX
# ifdef VBOX_WITH_PULSE
    &pulse_audio_driver,
# endif
# ifdef VBOX_WITH_ALSA
    &alsa_audio_driver,
# endif
#endif /* RT_OS_LINUX */
#ifdef RT_OS_FREEBSD
# ifdef VBOX_WITH_PULSE
    &pulse_audio_driver,
# endif
#endif
#ifdef RT_OS_DARWIN
    &coreaudio_audio_driver,
#endif
#ifdef RT_OS_WINDOWS
    &dsound_audio_driver,
#endif
#ifdef RT_OS_L4
    &oss_audio_driver,
#endif
#ifdef RT_OS_SOLARIS
    &solaudio_audio_driver,
#endif
    &no_audio_driver
};

extern t_sample *convAudio;
extern f_sample *clipAudio;

extern t_sample *convAudioIn;
extern f_sample *clipAudioIn;
static char *audio_streamname;

const char *audio_get_stream_name(void)
{
    return audio_streamname;
}

volume_t nominal_volume = {
    0,
#ifdef FLOAT_MIXENG
    1.0,
    1.0
#else
#ifndef VBOX
    UINT_MAX,
    UINT_MAX
#else
    INT_MAX,
    INT_MAX
#endif
#endif
};

volume_t sum_out_volume =
{
    0,
    INT_MAX,
    INT_MAX
};
volume_t master_out_volume =
{
    0,
    INT_MAX,
    INT_MAX
};
volume_t pcm_out_volume =
{
    0,
    INT_MAX,
    INT_MAX
};
volume_t pcm_in_volume =
{
    0,
    INT_MAX,
    INT_MAX
};


/***************** ring buffer Hanlding section **************/
static void IORingBufferCreate(PIORINGBUFFER *ppBuffer, uint32_t cSize)
{
    PIORINGBUFFER pTmpBuffer;

    AssertPtr(ppBuffer);

    *ppBuffer = NULL;
    pTmpBuffer = RTMemAllocZ(sizeof(IORINGBUFFER));
    if (pTmpBuffer)
    {
        pTmpBuffer->pBuffer = RTMemAlloc(cSize);
        if(pTmpBuffer->pBuffer)
        {
            pTmpBuffer->cBufSize = cSize;
            *ppBuffer = pTmpBuffer;
        }
        else
            RTMemFree(pTmpBuffer);
    }
}

static void IORingBufferDestroy(PIORINGBUFFER pBuffer)
{
    if (pBuffer)
    {
        if (pBuffer->pBuffer)
            RTMemFree(pBuffer->pBuffer);
        RTMemFree(pBuffer);
    }
}

DECL_FORCE_INLINE(void) IORingBufferReset(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);

    pBuffer->uReadPos = 0;
    pBuffer->uWritePos = 0;
    pBuffer->cBufferUsed = 0;
}

DECL_FORCE_INLINE(uint32_t) IORingBufferFree(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferUsed(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return ASMAtomicReadU32(&pBuffer->cBufferUsed);
}

DECL_FORCE_INLINE(uint32_t) IORingBufferSize(PIORINGBUFFER pBuffer)
{
    AssertPtr(pBuffer);
    return pBuffer->cBufSize;
}

static void IORingBufferAquireReadBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uUsed = 0;
    uint32_t uSize = 0;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is in use? */
    uUsed = ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uUsed > 0)
    {
        /* Get the size out of the requested size, the read block till the end
         * of the buffer & the currently used size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uReadPos, uUsed));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current read
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uReadPos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseReadBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uReadPos = (pBuffer->uReadPos + cSize) % pBuffer->cBufSize;
    ASMAtomicSubU32(&pBuffer->cBufferUsed, cSize);
}

static void IORingBufferAquireWriteBlock(PIORINGBUFFER pBuffer, uint32_t cReqSize, char **ppStart, uint32_t *pcSize)
{
    uint32_t uFree;
    uint32_t uSize;

    AssertPtr(pBuffer);

    *ppStart = 0;
    *pcSize = 0;

    /* How much is free? */
    uFree = pBuffer->cBufSize - ASMAtomicReadU32(&pBuffer->cBufferUsed);
    if (uFree > 0)
    {
        /* Get the size out of the requested size, the write block till the end
         * of the buffer & the currently free size. */
        uSize = RT_MIN(cReqSize, RT_MIN(pBuffer->cBufSize - pBuffer->uWritePos, uFree));
        if (uSize > 0)
        {
            /* Return the pointer address which point to the current write
             * position. */
            *ppStart = pBuffer->pBuffer + pBuffer->uWritePos;
            *pcSize = uSize;
        }
    }
}

DECL_FORCE_INLINE(void) IORingBufferReleaseWriteBlock(PIORINGBUFFER pBuffer, uint32_t cSize)
{
    AssertPtr(pBuffer);

    /* Split at the end of the buffer. */
    pBuffer->uWritePos = (pBuffer->uWritePos + cSize) % pBuffer->cBufSize;

    ASMAtomicAddU32(&pBuffer->cBufferUsed, cSize);
}

/****************** Ring Buffer Function Ends *****************/

/* http://www.df.lth.se/~john_e/gems/gem002d.html */
/* http://www.multi-platforms.com/Tips/PopCount.htm */
uint32_t popcount (uint32_t u)
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
    return popcount ((u&-u)-1);
}
uint64_t audio_get_clock (void)
{
    return PDMDrvHlpTMGetVirtualTime (gpDrvIns);
}
uint64_t audio_get_ticks_per_sec (void)
{
    return PDMDrvHlpTMGetVirtualFreq (gpDrvIns);
}

inline int audio_bits_to_index (int bits)
{
    switch (bits) {
    case 8:
        return 0;

    case 16:
        return 1;

    case 32:
        return 2;

    default:
        LogFlow(("invalid bits %d\n", bits));
        return 0;
    }
}

static const char *audio_audfmt_to_string (audfmt_e fmt)
{
    switch (fmt) {
    case AUD_FMT_U8:
        return "U8";

    case AUD_FMT_U16:
        return "U16";

    case AUD_FMT_U32:
        return "U32";

    case AUD_FMT_S8:
        return "S8";

    case AUD_FMT_S16:
        return "S16";

    case AUD_FMT_S32:
        return "S32";
    }

    LogFlow(("Bogus audfmt %d returning S16\n", fmt));
    return "S16";
}

static audfmt_e audio_string_to_audfmt (const char *s, audfmt_e defval, int *defaultp)
{
    if (!strcasecmp (s, "u8")) {
        *defaultp = 0;
        return AUD_FMT_U8;
    }
    else if (!strcasecmp (s, "u16")) {
        *defaultp = 0;
        return AUD_FMT_U16;
    }
    else if (!strcasecmp (s, "u32")) {
        *defaultp = 0;
        return AUD_FMT_U32;
    }
    else if (!strcasecmp (s, "s8")) {
        *defaultp = 0;
        return AUD_FMT_S8;
    }
    else if (!strcasecmp (s, "s16")) {
        *defaultp = 0;
        return AUD_FMT_S16;
    }
    else if (!strcasecmp (s, "s32")) {
        *defaultp = 0;
        return AUD_FMT_S32;
    }
    else {
        LogFlow(("Bogus audio format `%s' using %s\n",
                  s, audio_audfmt_to_string (defval)));
        *defaultp = 1;
        return defval;
    }
}

static audfmt_e drvAudioGetConfFormat(PCFGMNODE pCfgHandle, const char *envname, audfmt_e defval, int *defaultp)
{
    char *var = NULL;
    int rc;

    if(pCfgHandle == NULL || envname == NULL) {
        *defaultp = 1;
        return defval;
    }

    rc = CFGMR3QueryStringAlloc(pCfgHandle, envname, &var);
    if (RT_FAILURE (rc)) {
        *defaultp = 1;
        return defval;
    }
    return audio_string_to_audfmt (var, defval, defaultp);
}

static int drvAudioGetConfInt(PCFGMNODE pCfgHandle, const char *key, int defval, int *defaultp)
{
    int rc;
    uint64_t u64Data = 0;
    if(pCfgHandle == NULL || key == NULL) {
        *defaultp = 1;
        return defval;
    }

    *defaultp = 0;
    rc = CFGMR3QueryInteger(pCfgHandle, key, &u64Data);
    if (RT_FAILURE (rc))
    {
        *defaultp = 1;
        return defval;

    }
    else
    {
        LogFlow(("%s, Value = %d\n", key, u64Data));
        *defaultp = 0;
        return u64Data;
    }
}

static const char *drvAudioGetConfStr(PCFGMNODE  pCfgHandle, const char *key, const char *defval, int *defaultp)
{
    char *val = NULL;
    int rc;
    if(pCfgHandle == NULL || key == NULL) {
        *defaultp = 1;
        return defval;
    }

    rc = CFGMR3QueryStringAlloc(pCfgHandle, key, &val);
    if (RT_FAILURE (rc)) {
        *defaultp = 1;
        return defval;
    }
    else {
        *defaultp = 0;
        return val;
    }
}

static void drvAudioProcessOptions(PCFGMNODE pCfgHandle, const char *prefix, struct audio_option *opt)
{
    int def;
    PCFGMNODE pCfgChildHandle = NULL;
    PCFGMNODE pCfgChildChildHandle = NULL;

    if (!prefix || !opt)
    {
        LogFlow(("prefix = NULL OR opt = NULL\n"));
        return;
    }

   /* if pCfgHandle is NULL, let NULL be passed to get int and get string functions..
    * The getter function will return default values.
    */
    if(pCfgHandle != NULL)
    {
       /* If its audio general setting, need to traverse to one child node.
        * /Devices/ichac97/0/LUN#0/Config/Audio
        */
       if(!strncmp(prefix, "AUDIO", 5)) {
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if(pCfgChildHandle) {
                pCfgHandle = pCfgChildHandle;
            }
        }
        else
        {
            /* If its driver specific configuration , then need to traverse two level deep child
             * child nodes. for eg. in case of DirectSoundConfiguration item
             * /Devices/ichac97/0/LUN#0/Config/Audio/DirectSoundConfig
             */
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if (pCfgChildHandle) {
                pCfgChildChildHandle = CFGMR3GetFirstChild(pCfgChildHandle);
                if(pCfgChildChildHandle) {
                   pCfgHandle = pCfgChildChildHandle;
                }
            }
        }
   }


    for (; opt->name; opt++) {
            LogFlow(("Option value pointer for `%s' is not set\n",
                   opt->name));
        if (!opt->valp) {
            LogFlow(("Option value pointer for `%s' is not set\n",
                   opt->name));
            continue;
        }
        def = 1;
        switch (opt->tag) {
        case AUD_OPT_BOOL:
        case AUD_OPT_INT:
            {
                int *intp = opt->valp;
                *intp =   drvAudioGetConfInt(pCfgHandle, opt->name, *intp, &def);
            }
            break;

        case AUD_OPT_FMT:
            {
                audfmt_e *fmtp = opt->valp;
                *fmtp = drvAudioGetConfFormat(pCfgHandle, opt->name, *fmtp, &def);
            }
            break;

        case AUD_OPT_STR:
            {
                const char **strp = opt->valp;
                *strp = drvAudioGetConfStr(pCfgHandle, opt->name, *strp, &def);
            }
            break;

        default:
            LogFlow(("Bad value tag for option `%s' - %d\n",
                   opt->name, opt->tag));
            break;
        }

        if (!opt->overridenp) {
            opt->overridenp = &opt->overriden;
        }
        *opt->overridenp = !def;
    }
}

static void drvAudioPrintSettings(audsettings_t *as)
{
    LogFlow(("frequency=%d nchannels=%d fmt=", as->freq, as->nchannels));

    switch (as->fmt) {
    case AUD_FMT_S8:
        LogFlow(("S8\n"));
        break;
    case AUD_FMT_U8:
        LogFlow(("U8\n"));
        break;
    case AUD_FMT_S16:
        LogFlow(("S16\n"));
        break;
    case AUD_FMT_U16:
        LogFlow(("U16\n"));
        break;
    case AUD_FMT_S32:
        LogFlow(("S32\n"));
        break;
    case AUD_FMT_U32:
        LogFlow(("U32\n"));
        break;
    default:
        LogFlow(("invalid(%d)\n", as->fmt));
        break;
    }

    LogFlow(("endianness=\n"));
    switch (as->endianness)
    {
    case 0:
        LogFlow(("little\n"));
        break;
    case 1:
        LogFlow(("big\n"));
        break;
    default:
        LogFlow(("invalid\n"));
        break;
    }
}

static int drvAudioValidateSettings(audsettings_t *as)
{
    int invalid;

    invalid = as->nchannels != 1 && as->nchannels != 2;
    invalid |= as->endianness != 0 && as->endianness != 1;

    switch (as->fmt) {
    case AUD_FMT_S8:
    case AUD_FMT_U8:
    case AUD_FMT_S16:
    case AUD_FMT_U16:
    case AUD_FMT_S32:
    case AUD_FMT_U32:
        break;
    default:
        invalid = 1;
        break;
    }

    invalid |= as->freq <= 0;
    return invalid ? -1 : 0;
}

int drvAudioPcmInfoEq(PDMPCMPROPERTIES * pProps, audsettings_t *as)
{
    int bits = 8, sign = 0;

    switch (as->fmt) {
    case AUD_FMT_S8:
        sign = 1;
    case AUD_FMT_U8:
        break;

    case AUD_FMT_S16:
        sign = 1;
    case AUD_FMT_U16:
        bits = 16;
        break;

    case AUD_FMT_S32:
        sign = 1;
    case AUD_FMT_U32:
        bits = 32;
        break;
    }
    return pProps->uFrequency == as->freq
        && pProps->cChannels == as->nchannels
        && pProps->fSigned == sign
        && pProps->cBits == bits
        && pProps->fSwapEndian == (as->endianness != AUDIO_HOST_ENDIANNESS);
}

void drvAudioPcmInitInfo(PDMPCMPROPERTIES * pProps, audsettings_t *as)
{
    int bits = 8, sign = 0, shift = 0;

    switch (as->fmt) {
    case AUD_FMT_S8:
        sign = 1;
    case AUD_FMT_U8:
        break;

    case AUD_FMT_S16:
        sign = 1;
    case AUD_FMT_U16:
        bits = 16;
        shift = 1;
        break;

    case AUD_FMT_S32:
        sign = 1;
    case AUD_FMT_U32:
        bits = 32;
        shift = 2;
        break;
    }

    pProps->uFrequency = as->freq;
    pProps->cBits = bits;
    pProps->fSigned = sign;
    pProps->cChannels = as->nchannels;
    pProps->cShift = (as->nchannels == 2) + shift;
    pProps->fAlign = (1 << pProps->cShift) - 1;
    pProps->cbPerSec = pProps->uFrequency << pProps->cShift;
    pProps->fSwapEndian = (as->endianness != AUDIO_HOST_ENDIANNESS);
}

void audio_pcm_info_clear_buf (struct audio_pcm_info *info, void *buf, int len)
{
    if (!len) {
        return;
    }

    if (info->sign) {
        memset (buf, 0x00, len << info->shift);
    }
    else {
        switch (info->bits) {
        case 8:
            memset (buf, 0x80, len << info->shift);
            break;

        case 16:
            {
                int i;
                uint16_t *p = buf;
                int shift = info->nchannels - 1;
                short s = INT16_MAX;

                if (info->swap_endianness) {
                    s = bswap16 (s);
                }

                for (i = 0; i < len << shift; i++) {
                    p[i] = s;
                }
            }
            break;

        case 32:
            {
                int i;
                uint32_t *p = buf;
                int shift = info->nchannels - 1;
                int32_t s = INT32_MAX;

                if (info->swap_endianness) {
                    s = bswap32 (s);
                }

                for (i = 0; i < len << shift; i++) {
                    p[i] = s;
                }
            }
            break;

        default:
            LogFlow(("audio_pcm_info_clear_buf: invalid bits %d\n", info->bits));
            break;
        }
    }
}

/*
 * Capture
 */
static void noop_conv (st_sample_t *dst, const void *src, int samples, volume_t *vol)
{
    (void) src;
    (void) dst;
    (void) samples;
    (void) vol;
}

static void audio_capture_maybe_changed (CaptureVoiceOut *cap, int enabled)
{
    if (cap->hw.fEnabled != enabled)
    {
        audcnotification_e cmd;
        cap->hw.fEnabled = enabled;
        cmd = enabled ? AUD_CNOTIFY_ENABLE : AUD_CNOTIFY_DISABLE;
    }
}

static void drvAudioRecalcAndNotifyCapture(CaptureVoiceOut *cap)
{
    PPDMHOSTVOICEOUT hw = &cap->hw;
    PPDMGSTVOICEOUT sw;
    PPDMGSTVOICEOUT pIter;
    int enabled = 0;
    LogFlow(("drvAudioRecalcAndNotifyCaptuer \n"));

    RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
    {
        sw = pIter;
        LogFlow(("1\n"));
        if (sw->State.fActive) {
            enabled = 1;
            break;
        }
    }
    audio_capture_maybe_changed (cap, enabled);
}

void drvAudioDetachCapture(PPDMHOSTVOICEOUT pHostVoiceOut)
{
    SWVoiceCap * sc;
    SWVoiceCap *pIter;
    LogFlow(("drvAudioDetachCapture \n"));
    RTListForEach(&pHostVoiceOut->HeadCapturedVoice, pIter, SWVoiceCap, ListCapturedVoice)
    {
        sc = pIter;
        PPDMGSTVOICEOUT sw = &sc->sw;
        CaptureVoiceOut *cap = sc->cap;
        int was_active = sw->State.fActive;

        if (sw->State.rate) {
            st_rate_stop (sw->State.rate);
            sw->State.rate = NULL;
        }

        while (!RTListIsEmpty(&sw->ListGstVoiceOut))
        {
            PPDMGSTVOICEOUT pIterTmp = RTListGetFirst(&sw->ListGstVoiceOut, PDMGSTVOICEOUT, ListGstVoiceOut);
            RTListNodeRemove(&pIterTmp->ListGstVoiceOut);
        }

        while (!RTListIsEmpty(&sc->ListCapturedVoice))
        {
            SWVoiceCap * pIterTmp = RTListGetFirst(&sc->ListCapturedVoice, SWVoiceCap, ListCapturedVoice);
            RTListNodeRemove(&pIterTmp->ListCapturedVoice);
        }

        if (was_active) {
            /* We have removed soft voice from the capture:
               this might have changed the overall status of the capture
               since this might have been the only active voice */
            drvAudioRecalcAndNotifyCapture(cap);
        }
    }
}

int drvAudioAttachCapture(PDRVAUDIO pDrvAudio, PPDMHOSTVOICEOUT pHostVoiceOut)
{
    CaptureVoiceOut *cap;
    CaptureVoiceOut *pIter;

    drvAudioDetachCapture(pHostVoiceOut);
    RTListForEach(&pDrvAudio->HeadCapturedVoice, pIter, CaptureVoiceOut, ListCapturedVoice)
    {
        SWVoiceCap *sc;
        PPDMGSTVOICEOUT pGstVoiceOut;
        cap = pIter;
        PPDMHOSTVOICEOUT hw_cap = &cap->hw;
        sc = (SWVoiceCap*) RTMemAllocZ(1 * sizeof (SWVoiceCap));
        if (!sc)
        {
            LogFlow(("Could not allocate soft capture voice (%u bytes)\n",
                   sizeof (*sc)));
            return -1;
        }

        sc->cap = cap;
        pGstVoiceOut = &sc->sw;
        pGstVoiceOut->pHostVoiceOut = hw_cap;
        pGstVoiceOut->Props = pHostVoiceOut->Props;
        pGstVoiceOut->State.fEmpty = 1;
        pGstVoiceOut->State.fActive = pHostVoiceOut->fEnabled;
        LogFlow(("DrvAudio: setting gstvoiceout ratio. Freq=%d and Frew=%d",
                 hw_cap->Props.uFrequency,  pGstVoiceOut->Props.uFrequency ));
        pGstVoiceOut->State.ratio = ((int64_t) hw_cap->Props.uFrequency << 32) / pGstVoiceOut->Props.uFrequency;
        pGstVoiceOut->State.rate = st_rate_start (pGstVoiceOut->Props.uFrequency, hw_cap->Props.uFrequency);
        if (!pGstVoiceOut->State.rate)
        {
            LogFlow(("Error Could not start rate conversion \n"));
            RTMemFree(pGstVoiceOut);
            return -1;
        }
        RTListPrepend(&hw_cap->HeadGstVoiceOut, &pGstVoiceOut->ListGstVoiceOut);
        RTListPrepend(&pHostVoiceOut->HeadCapturedVoice, &sc->ListCapturedVoice);
        if (pGstVoiceOut->State.fActive)
        {
            audio_capture_maybe_changed (cap, 1);
        }
    }
    return 0;
}

/*
 * Hard voice (capture)
 */
static int audio_pcm_hw_find_min_in (PPDMHOSTVOICEIN hw)
{
    PPDMGSTVOICEIN pIter = NULL;
    PPDMGSTVOICEIN pGstVoiceIn = NULL;
    int m = hw->cSamplesCaptured;

    RTListForEach(&hw->HeadGstVoiceIn, pIter, PDMGSTVOICEIN, ListGstVoiceIn)
    {
        pGstVoiceIn = pIter;
        LogFlow(("DrvAudio: pGstVioceIn = %p\n", pGstVoiceIn));
        if (pGstVoiceIn->State.fActive)
        {
            m = audio_MIN (m, pGstVoiceIn->cHostSamplesAcquired);
        }
    }
    return m;
}

int audio_pcm_hw_get_live_in (PPDMHOSTVOICEIN hw)
{
    int live = hw->cSamplesCaptured - audio_pcm_hw_find_min_in (hw);
    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }
    return live;
}

/*
 * Soft voice (capture)
 */
static int audio_pcm_sw_get_rpos_in (PPDMGSTVOICEIN sw)
{
    PPDMHOSTVOICEIN hw = sw->hw;
    int live = hw->cSamplesCaptured - sw->cHostSamplesAcquired;
    int rpos;

    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }

    rpos = hw->offWrite - live;
    if (rpos >= 0) {
        return rpos;
    }
    else {
        return hw->cSamples + rpos;
    }
}

int audio_pcm_sw_read (PDRVAUDIO pThis, PPDMGSTVOICEIN sw, PPDMHOSTSTEREOSAMPLE buf, int size)
{
    PPDMHOSTVOICEIN hw = sw->hw;
    int samples, live, ret = 0, swlim, isamp, osamp, rpos, total = 0;
    PPDMHOSTSTEREOSAMPLE src, dst = sw->buf;
    PPDMHOSTSTEREOSAMPLE tmp = sw->buf;
    PPDMHOSTSTEREOSAMPLE pSampleBuf;
    char *pcDst = NULL;
    char *pcSrc = NULL;
    uint32_t cbToWrite;
    uint32_t csAvail = 0;
    uint32_t cSamplesRead = 0;
    uint32_t cbToRead = 0;
    uint32_t csWritten = 0;
    uint32_t cSamplesInRingBuffer = 0;
    uint32_t csToWrite;
    uint32_t csLimit;

    /* Max capacity to hold 1000 PDMHOSTSTERESAMPLE type samples. */
    pSampleBuf = (PPDMHOSTSTEREOSAMPLE) RTMemAlloc(1000 * sizeof(PDMHOSTSTEREOSAMPLE));
    rpos = audio_pcm_sw_get_rpos_in (sw) % hw->cSamples;

    /* difference between how many samples have be captured/read by the host and how
     * many have been transferred to guest (guest physical memory) to be utilized by the guest
     * eg skype on guest or saving samples in file in guest.
     */
    live = hw->cSamplesCaptured - sw->cHostSamplesAcquired;
    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live_in=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }

    samples = size >> sw->Props.cShift;
    if (!live)
    {
        LogFlow(("DrvAudio: Error: No Live data \n"));
        return 0;
    }

    swlim = (live * sw->State.ratio) >> 32;
    csLimit = swlim;
    swlim = audio_MIN (swlim, samples);
    while (swlim)
    {
        src = hw->pConversionBuf + rpos;
        isamp = hw->offWrite - rpos;
        /* XXX: <= ? */
        if (isamp <= 0)
            isamp = hw->cSamples - rpos;

        if (!isamp)
            break;

        osamp = swlim;
        if (osamp < 0)
        {
            LogFlow(("Error: osamp=%d\n", osamp));
            return 0;
        }

        if (ret + osamp > sw->buf_samples)
            Log(("audio_pcm_sw_read: buffer overflow!! ret = %d, osamp = %d, buf_samples = %d\n",
                  ret, osamp, sw->buf_samples));
        st_rate_flow (sw->State.rate, src, dst, &isamp, &osamp);
        swlim -= osamp;
        rpos = (rpos + isamp) % hw->cSamples;
        dst += osamp;
        ret += osamp;
        total += isamp;
    }
    sw->cHostSamplesAcquired = sw->cHostSamplesAcquired + total;
    if (csLimit >= samples)
    {
        /* read audio that is there in the ring buffer */
        cSamplesInRingBuffer = IORingBufferUsed(pThis->pAudioReadBuf) / sizeof(PDMHOSTSTEREOSAMPLE);
        if (cSamplesInRingBuffer >= 45000) /* 750 K Buffer / sizeof(PDMHOSTSTEREOSAMPLE)*/
        {
            IORingBufferReset(pThis->pAudioReadBuf);
            cSamplesInRingBuffer = 0;
        }
        if (cSamplesInRingBuffer > samples)
            cSamplesInRingBuffer = samples;
        cSamplesRead = 0, cbToRead = 0;
        while (cSamplesRead < cSamplesInRingBuffer)
        {
            cbToRead = cSamplesInRingBuffer * sizeof(PDMHOSTSTEREOSAMPLE);
            IORingBufferAquireReadBlock(pThis->pAudioReadBuf, cbToRead,
                                         &pcSrc, &cbToRead);
            if (!cbToRead)
                LogFlow(("DrvAudio: There are no audio in queue to be written. \n"));
            memcpy((uint8_t *)pSampleBuf, pcSrc, cbToRead);
            cSamplesRead = cSamplesRead + (cbToRead / sizeof(PDMHOSTSTEREOSAMPLE));
            /* Release the read buffer, so it could be used for new data. */
            IORingBufferReleaseReadBlock(pThis->pAudioReadBuf, cbToRead);
        }
        memcpy((uint8_t *)pSampleBuf + cbToRead, (uint8_t *)sw->buf,
               (samples - cSamplesRead) * sizeof(PDMHOSTSTEREOSAMPLE));

        memcpy((uint8_t *)buf, (uint8_t *)pSampleBuf, samples * sizeof(PDMHOSTSTEREOSAMPLE));

        csToWrite = ret - (samples - cSamplesRead);
        csAvail = IORingBufferFree(pThis->pAudioReadBuf) / sizeof(PDMHOSTSTEREOSAMPLE);
        csWritten = 0;
        while (csWritten < csToWrite)
        {
            cbToWrite = csToWrite * sizeof(PDMHOSTSTEREOSAMPLE);
            IORingBufferAquireWriteBlock(pThis->pAudioReadBuf, cbToWrite, &pcDst, &cbToWrite);

            csToWrite = cbToWrite / sizeof(PDMHOSTSTEREOSAMPLE);
            if (RT_UNLIKELY(csToWrite == 0))
            {
                IORingBufferReleaseWriteBlock(pThis->pAudioReadBuf, cbToWrite);
                IORingBufferReset(pThis->pAudioReadBuf);
                LogFlow(("DrvAudio: NO space in Ring buffer. Not doing anything, just discarding samples.\n"));
            }

            /* copy the audio data not accepted by the backend to the ring buffer */
            memcpy(pcDst, (uint8_t*)sw->buf + ((samples - cSamplesRead) * sizeof(PDMHOSTSTEREOSAMPLE)) +
                   (csWritten * sizeof(PDMHOSTSTEREOSAMPLE)), cbToWrite);
            IORingBufferReleaseWriteBlock(pThis->pAudioReadBuf, cbToWrite);
            csWritten += csToWrite;
        }
        return samples << sw->Props.cShift;
    }
    else if (csLimit < samples && csLimit != 0)
    {
        csAvail = IORingBufferFree(pThis->pAudioReadBuf) / sizeof(PDMHOSTSTEREOSAMPLE);
        csWritten = 0;
        while (csWritten < ret)
        {
            csToWrite = ret - csWritten;
            cbToWrite = csToWrite * sizeof(PDMHOSTSTEREOSAMPLE);
            IORingBufferAquireWriteBlock(pThis->pAudioReadBuf, cbToWrite, &pcDst, &cbToWrite);

            csToWrite = cbToWrite / sizeof(PDMHOSTSTEREOSAMPLE);
            if (RT_UNLIKELY(csToWrite == 0))
                LogFlow(("DrvAudio: NO space in Ring buffer. Not doing anything, just discarding samples.\n"));

            /* copy the audio data not accepted by the backend to the ring buffer */
            memcpy(pcDst, (uint8_t*)sw->buf + (csWritten * sizeof(PDMHOSTSTEREOSAMPLE)), cbToWrite);
            IORingBufferReleaseWriteBlock(pThis->pAudioReadBuf, cbToWrite);
            csWritten += csToWrite;
        }
        memcpy(buf, pSampleBuf, 0);
        return 0;
    }
    return 0;
}

/*
 * Hard voice (playback)
 */
static int audio_pcm_hw_find_min_out (PPDMHOSTVOICEOUT hw, int *nb_livep)
{
    PPDMGSTVOICEOUT sw;
    PPDMGSTVOICEOUT pIter;
    int m = INT_MAX;
    int nb_live = 0;
    LogFlow(("Hard Voice Playback \n"));

    RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
    {
        sw = pIter;
        if (sw->State.fActive || !sw->State.fEmpty)
        {
            m = audio_MIN (m, sw->cSamplesMixed);
            nb_live += 1;
        }
    }

    *nb_livep = nb_live;
    return m;
}

int audio_pcm_hw_get_live_out2 (PPDMHOSTVOICEOUT hw, int *nb_live)
{
    int smin;

    smin = audio_pcm_hw_find_min_out (hw, nb_live);

    if (!*nb_live) {
        return 0;
    }
    else
    {
        int live = smin;

        if (live < 0 || live > hw->cSamples)
        {
            LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
            return 0;
        }
        return live;
    }
}

int audio_pcm_hw_get_live_out (PPDMHOSTVOICEOUT hw)
{
    int nb_live;
    int live;

    live = audio_pcm_hw_get_live_out2 (hw, &nb_live);
    if (live < 0 || live > hw->cSamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hw->cSamples));
        return 0;
    }
    return live;
}

#if 0
/*
 * Soft voice (playback)
 */
int audio_pcm_sw_write (PDRVAUDIO pThis,PPDMGSTVOICEOUT pGstVoiceOut, void *buf, int size)
{
    int hwsamples, samples, isamp, osamp, wpos, live, dead, left, swlim, blck;
    int ret = 0, pos = 0, total = 0;

    if (!pGstVoiceOut) {
        return size;
    }

    hwsamples = pGstVoiceOut->pHostVoiceOut->cSamples;
    LogFlow(("DrvAudio: hwSamples = %d\n", hwsamples));

    live = pGstVoiceOut->cSamplesMixed;
    if (live < 0 || live > hwsamples)
    {
        LogFlow(("Error: live=%d hw->samples=%d\n", live, hwsamples));
        return size;
    }

    if (live == hwsamples) {
        LogFlow(("DrvAudio: full %d\n", live));
        return size;
    }

    wpos = (pGstVoiceOut->pHostVoiceOut->offRead + live) % hwsamples;
    samples = size >> pGstVoiceOut->Props.cShift;

    dead = hwsamples - live;
    swlim = ((int64_t) dead << 32) / pGstVoiceOut->State.ratio;
    swlim = audio_MIN (swlim, samples);
    if (swlim > pGstVoiceOut->cSamples)
        Log(("audio_pcm_sw_write: buffer overflow!! swlim = %d, buf_samples = %d\n",
             swlim, pos, pGstVoiceOut->cSamples));
    //swlim = samples;
    if (swlim) {
        convAudio (pGstVoiceOut->buf, buf, swlim, &sum_out_volume);
    }

    while (swlim)
    {
        dead = hwsamples - live;
        left = hwsamples - wpos;
        blck = audio_MIN (dead, left);
        if (!blck) {
            break;
        }
        isamp = swlim;
        osamp = blck;
        //osamp = left;
        if (pos + isamp > pGstVoiceOut->cSamples)
            Log(("audio_pcm_sw_write: buffer overflow!! isamp = %d, pos = %d, buf_samples = %d\n",
                 isamp, pos, pGstVoiceOut->cSamples));
        st_rate_flow_mix (
            pGstVoiceOut->State.rate,
            pGstVoiceOut->buf + pos,
            pGstVoiceOut->pHostVoiceOut->pHostSterioSampleBuf + wpos,
            &isamp,
            &osamp
            );
        ret += isamp;
        swlim -= isamp;
        pos += isamp;
        live += osamp;
        wpos = (wpos + osamp) % hwsamples;
        total += osamp;
    }

    pGstVoiceOut->cSamplesMixed += total;
    pGstVoiceOut->State.fEmpty = pGstVoiceOut->cSamplesMixed == 0;

    LogFlow((
        "write size %d ret %d total sw %d\n",
        size >> pGstVoiceOut->Props.cShift,
        ret,
        pGstVoiceOut->cSamplesMixed
        ));

    //return ret << pGstVoiceOut->Props.cShift;
    return size;
}
#endif

/*
 * Soft voice (playback)
 */
int audio_pcm_sw_write(PDRVAUDIO pThis, PPDMGSTVOICEOUT pGstVoiceOut, void *buf, int size)
{
    int hwsamples, samples, isamp, osamp, wpos, live, dead, left, swlim, blck;
    int ret = 0, pos = 0, total = 0;
    uint8_t *pcDst = NULL;
    uint8_t *pcSrc = NULL;
    uint32_t cbToWrite;
    uint32_t cSamplesInQueue = 0;
    uint32_t cbSamplesInQueue = 0;
    uint32_t cbTotalSamples = 0;
    uint32_t cTotalSamples = 0;
    uint32_t cSamplesAccepted = 0;
    /* sample buf to hold the samples that have been passed from Device and to hold samples from the queue
     * 500 KB.
     */
    uint8_t SampleBuf[512000];
    PDMHOSTSTEREOSAMPLE pdmSampleBuf[5120];
    uint32_t cSamplesLeft;

    if (!pGstVoiceOut)
    {
        LogFlow(("DrvAudio: GstVoiceOut NULL \n"));
       return size;
    }
    LogFlow(("DrvAudio: size to write = %d\n", size));

    hwsamples = pGstVoiceOut->pHostVoiceOut->cSamples;

    live = pGstVoiceOut->cSamplesMixed;
    if (live < 0 || live > hwsamples)
    {
        /* save all the samples in the ring buffer, and return as if all the samples have been accepted.
         * Saving in ring buffer as host buffer is full and can't take any more samples.
         */
        cbToWrite = size;
        LogFlow(("DrvAudio: Error: live=%d hw->samples=%d\n", live, hwsamples));
        //return 0
        IORingBufferAquireWriteBlock(pThis->pAudioWriteBuf, cbToWrite, &pcDst, &cbToWrite);
        if (RT_UNLIKELY(cbToWrite == 0))
            LogFlow(("DrvAudio: NO space in Ring buffer. Not doing anything, just discarding samples.\n"));

        /* copy the audio data not accepted by the backend to the ring buffer */
        memcpy(pcDst, (uint8_t*)buf , cbToWrite);

        /* Rlease the ring buffer */
        IORingBufferReleaseWriteBlock(pThis->pAudioWriteBuf, cbToWrite);
        return size;
    }

    if (live == hwsamples)
    {
        /* save all the samples in the ring buffer, and return as if all the samples have been accepted.
         * Saving in ring buffer as host buffer is full and can't take any more samples.
         */
        LogFlow(("DrvAudio: full %d\n", live));
        //return 0;
        cbToWrite = size;
        IORingBufferAquireWriteBlock(pThis->pAudioWriteBuf, cbToWrite, &pcDst, &cbToWrite);
        if (RT_UNLIKELY(cbToWrite == 0))
            LogFlow(("DrvAudio: NO space in Ring buffer. Not doing anything, just discarding samples.\n"));

        /* copy the audio data not accepted by the backend to the ring buffer */
        memcpy(pcDst, (uint8_t*)buf, cbToWrite);

        /* Rlease the ring buffer */
        IORingBufferReleaseWriteBlock(pThis->pAudioWriteBuf, cbToWrite);
        return size;
    }

    wpos = (pGstVoiceOut->pHostVoiceOut->offRead + live) % hwsamples;
    /* @todo check it. to convert size to num of samples */
    //samples = cbTotalSamples >> pGstVoiceOut->Props.cShift;
    samples = size >> pGstVoiceOut->Props.cShift;

    dead = hwsamples - live;
    /* swlim is upper limit of max no. of samples that can be transferred in this cycle to backend */
    swlim = ((int64_t) dead << 32) / pGstVoiceOut->State.ratio;
    swlim = audio_MIN (swlim, samples);

    LogFlow(("DrvAudio: swlim = %d\n", swlim));

    if (swlim > pGstVoiceOut->cSamples)
        Log(("audio_pcm_sw_write: buffer overflow!! swlim = %d, buf_samples = %d\n",
             swlim, pos, pGstVoiceOut->cSamples));
    cSamplesAccepted = swlim;
     /* find out how much of the queue is full. */
    cbSamplesInQueue = IORingBufferUsed(pThis->pAudioWriteBuf);
    /* if ring buffer hold samples > 100 KB, discard samples */
    if (cbSamplesInQueue > 102400)
    {
        LogFlow(("DrvAudio: Samples in ring buffer > 300 KB. Discarding \n"));
        cbSamplesInQueue = 0;
        IORingBufferReset(pThis->pAudioWriteBuf);
    }
    /* read only that much samples as requried to be processed */
    if (cbSamplesInQueue > (cSamplesAccepted << pGstVoiceOut->Props.cShift))
        cbSamplesInQueue = (cSamplesAccepted << pGstVoiceOut->Props.cShift);

    LogFlow(("DrvAudio: Samples in queue =%d and its size=%d\n",
              cbSamplesInQueue >> pGstVoiceOut->Props.cShift, cbSamplesInQueue));

    /* read all the samples that are there in the queue and copy them to SampleBuf .
     * Reading all samples from the ring buffer as we need to send these samples first
     * and then add the samples received in this iteration to the ring buffer.
     */
    IORingBufferAquireReadBlock(pThis->pAudioWriteBuf, cbSamplesInQueue, &pcSrc, &cbSamplesInQueue);
    if (!cbSamplesInQueue)
        LogFlow(("DrvAudio: There are no audio in queue to be written. \n"));


    memcpy(SampleBuf, pcSrc, cbSamplesInQueue);
    /* Append the buf to samples that were there in the queue. SampBuf holds all the data to transfer */
    memcpy(SampleBuf + cbSamplesInQueue, buf, size);
    /* Release the read buffer, so it could be used for new data. */
    IORingBufferReleaseReadBlock(pThis->pAudioWriteBuf, cbSamplesInQueue);

    /* reset the ring buffer to accept new data. */

    /* cbTotalSamples = size of samples passed from the device + size of samples in the queue */
    cbTotalSamples = size + cbSamplesInQueue;
    LogFlow(("DrvAudio: size of samples read = %d and sz of samples recd = %d\n",
              cbSamplesInQueue, size));
    LogFlow(("DrvAudio: TotalSamples = %d\n", cbTotalSamples >> pGstVoiceOut->Props.cShift));
    if (swlim) {
        /* conversion engine: dest, source, num of samples, volume */
        convAudio (pGstVoiceOut->buf, SampleBuf, swlim, &sum_out_volume);
    }

    while (swlim)
    {
        dead = hwsamples - live;
        left = hwsamples - wpos;
        blck = audio_MIN (dead, left);
        if (!blck) {
            break;
        }
        isamp = swlim;
        osamp = blck;
        if (pos + isamp > pGstVoiceOut->cSamples)
            Log(("audio_pcm_sw_write: buffer overflow!! isamp = %d, pos = %d, buf_samples = %d\n",
                 isamp, pos, pGstVoiceOut->cSamples));
        /* mix and write to the host buffer to be read by the host during playout */
        st_rate_flow_mix (
            pGstVoiceOut->State.rate,
            pGstVoiceOut->buf + pos,
            pGstVoiceOut->pHostVoiceOut->pHostSterioSampleBuf + wpos,
            &isamp,
            &osamp
            );
        ret += isamp;
        swlim -= isamp;
        pos += isamp;
        live += osamp;
        wpos = (wpos + osamp) % hwsamples;
        total += osamp;
    }

    pGstVoiceOut->cSamplesMixed += total;
    pGstVoiceOut->State.fEmpty = pGstVoiceOut->cSamplesMixed == 0;

    cSamplesLeft = (cbTotalSamples >>  pGstVoiceOut->Props.cShift) - cSamplesAccepted ;
    LogFlow(("DrvAudio: cSamplesLeft = %d\n", cSamplesLeft));
    if (cSamplesLeft > 0)
    {
        cbToWrite = cSamplesLeft << pGstVoiceOut->Props.cShift;
        /* max capacity of ring buffer is 516196 */
        //cbToWrite = audio_MIN(cbToWrite, 516196);
        IORingBufferAquireWriteBlock(pThis->pAudioWriteBuf, cbToWrite, &pcDst, &cbToWrite);
        if (RT_UNLIKELY(cbToWrite == 0))
            LogFlow(("DrvAudio: NO space in Ring buffer. Not doing anything, just discarding samples.\n"));

        /* copy the audio data not accepted by the backend to the ring buffer */
        memcpy(pcDst, (uint8_t*)SampleBuf + (cSamplesAccepted << pGstVoiceOut->Props.cShift), cbToWrite);

        /* Rlease the ring buffer */
        IORingBufferReleaseWriteBlock(pThis->pAudioWriteBuf, cbToWrite);
    }
    /* convert no of samples to bytes */
    return size;
}

#ifdef DEBUG_AUDIO
static void audio_pcm_print_info (const char *cap, struct audio_pcm_info *info)
{
    LogFlow(("%s: bits %d, sign %d, freq %d, nchan %d\n",
           cap, info->bits, info->sign, info->freq, info->nchannels));
}
#endif

int AUD_get_buffer_size_out (PPDMGSTVOICEOUT sw)
{
    return sw->pHostVoiceOut->cSamples << sw->pHostVoiceOut->Props.cShift;
}

static int audio_get_avail (PPDMGSTVOICEIN sw)
{
    int live;

    if (!sw)
    {
        LogFlow(("Error Voice Input Stream NUL \n"));
        return 0;
    }

    live = sw->hw->cSamplesCaptured - sw->cHostSamplesAcquired;
    if (live < 0 || live > sw->hw->cSamples)
    {
        LogFlow(("DrvAudio: Error, live=%d sw->hw->samples=%d\n", live, sw->hw->cSamples));
        return 0;
    }
    return (((int64_t) live << 32) / sw->State.ratio) << sw->Props.cShift;
}

static int audio_get_free(PPDMGSTVOICEOUT sw)
{
    int live, dead;

    if (!sw)
    {
        LogFlow(("DrvAudio: Error, guest voice output stream null \n"));
        return 0;
    }

    live = sw->cSamplesMixed;

    if (live < 0 || live > sw->pHostVoiceOut->cSamples)
    {
        LogFlow(("DrvAudio: Error, live=%d sw->hw->samples=%d\n", live, sw->pHostVoiceOut->cSamples));
        return 0;
    }
    dead = sw->pHostVoiceOut->cSamples - live;
    return (((int64_t) dead << 32) / sw->State.ratio) << sw->Props.cShift;
}

static void audio_capture_mix_and_clear (PDRVAUDIO pThis, PPDMHOSTVOICEOUT hw, int rpos, int samples)
{
    int n;
    LogFlow(("audio_capture_mix_and_clear \n"));
    if (hw->fEnabled)
    {
        SWVoiceCap *sc;
        SWVoiceCap *pIter;

        RTListForEach(&hw->HeadCapturedVoice, pIter, SWVoiceCap, ListCapturedVoice)
        {
            sc = pIter;
            PPDMGSTVOICEOUT sw = &sc->sw;
            int rpos2 = rpos;

            n = samples;
            while (n)
            {
                int till_end_of_hw = hw->cSamples - rpos2;
                int to_write = audio_MIN (till_end_of_hw, n);
                int bytes = to_write << hw->Props.cShift;
                int written;

                sw->buf = hw->pHostSterioSampleBuf + rpos2;
                written = audio_pcm_sw_write (pThis, sw, NULL, bytes);
                if (written - bytes) {
                    LogFlow(("Could not mix %d bytes into a capture "
                           "buffer, mixed %d\n",
                           bytes, written));
                    break;
                }
                n -= to_write;
                rpos2 = (rpos2 + to_write) % hw->cSamples;
            }
        }
    }

    n = audio_MIN (samples, hw->cSamples - rpos);
    mixeng_sniff_and_clear (hw, hw->pHostSterioSampleBuf + rpos, n);
    mixeng_sniff_and_clear (hw, hw->pHostSterioSampleBuf, samples - n);
}

static void drvAudioPlayOut(PDRVAUDIO pThis)
{
    PPDMHOSTVOICEOUT hw = NULL;
    PPDMGSTVOICEOUT sw;
    PPDMGSTVOICEOUT pIter;
    PPDMGSTVOICEOUT pIter1;

    while ((hw = drvAudioHlpPcmHwFindAnyEnabledOut(pThis, hw)))
    {
        int played;
        int live, myfree, nb_live, cleanup_required, prev_rpos;

        live = audio_pcm_hw_get_live_out2 (hw, &nb_live);
        if (!nb_live)
        {
            LogFlow(("DrvAudio: Live samples 0\n"));
            live = 0;
        }

        if (live < 0 || live > hw->cSamples)
        {
            LogFlow(("DrvAudio: live=%d hw->samples=%d\n", live, hw->cSamples));
            continue;
        }

        if (hw->pending_disable && !nb_live)
        {
            SWVoiceCap *sc;
            SWVoiceCap *pIter;
            hw->fEnabled = 0;
            hw->pending_disable = 0;
            pThis->pHostDrvAudio->pfnDisableEnableOut(pThis->pHostDrvAudio, hw, VOICE_DISABLE);
            RTListForEach(&hw->HeadCapturedVoice, pIter, SWVoiceCap , ListCapturedVoice)
            {
                sc = pIter;
                sc->sw.State.fActive = 0;
                drvAudioRecalcAndNotifyCapture(sc->cap);
            }
            continue;
        }

        if (!live)
        {
            RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
            {
                sw = pIter;
                if (sw->State.fActive)
                {
                    myfree = audio_get_free (sw);
                    if (myfree > 0)
                        sw->callback.fn (sw->callback.opaque, myfree);
                }
            }
            continue;
        }

        prev_rpos = hw->offRead;
        played = pThis->pHostDrvAudio->pfnPlayOut(pThis->pHostDrvAudio, hw);
        if (hw->offRead >= hw->cSamples)
        {
            LogFlow(("DrvAudio: hw->rpos=%d hw->samples=%d played=%d\n",
                   hw->offRead, hw->cSamples, played));
            hw->offRead = 0;
        }
        /* I think this code relates to the point to accomodate no of audio samples
         * that have got accumulated during the above call of pfnplayout. So , it basically
         * tries to mix the old samples that have already been played, new samples that have
         * been gathered in due time of playing. Mix them up and send it to audiosniffer for
         * to be sent to VRDP server to be played on the client.
         */
        if (played) {
            audio_capture_mix_and_clear (pThis, hw, prev_rpos, played);
        }

        cleanup_required = 0;
        RTListForEach(&hw->HeadGstVoiceOut, pIter1, PDMGSTVOICEOUT, ListGstVoiceOut)
        {
            sw = pIter1;
            if (!sw->State.fActive && sw->State.fEmpty)
                continue;

            if (played > sw->cSamplesMixed)
            {
                LogFlow(("DrvAudio: played=%d sw->total_hw_samples_mixed=%d\n",
                       played, sw->cSamplesMixed));
                played = sw->cSamplesMixed;
            }

            sw->cSamplesMixed -= played;

            if (!sw->cSamplesMixed)
            {
                sw->State.fEmpty = 1;
                cleanup_required |= !sw->State.fActive && !sw->callback.fn;
            }

            if (sw->State.fActive)
            {
                myfree = audio_get_free (sw);
                if (myfree > 0)
                    sw->callback.fn (sw->callback.opaque, myfree);
            }
        }

        if (cleanup_required)
        {
            PPDMGSTVOICEOUT sw1;
            RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
            {
                sw = pIter;
                if (!sw->State.fActive && !sw->callback.fn)
                    drvAudioHlpCloseOut(pThis, &pThis->qemuSoundCard, sw);
            }
        }
    }
}

static void drvAudioPlayIn(PDRVAUDIO pThis)
{
    PPDMHOSTVOICEIN hw = NULL;

    while ((hw = drvAudioHlpPcmHwFindAnyEnabledIn(pThis, hw)))
    {
        PPDMGSTVOICEIN pIter;
        int captured, min;
        captured = pThis->pHostDrvAudio->pfnPlayIn(pThis->pHostDrvAudio, hw);

        min = audio_pcm_hw_find_min_in (hw);
        hw->cSamplesCaptured += captured - min;
        hw->ts_helper += captured;

        RTListForEach(&hw->HeadGstVoiceIn, pIter, PDMGSTVOICEIN, ListGstVoiceIn)
        {
            pIter->cHostSamplesAcquired -= min;

            if (pIter->State.fActive)
            {
                int avail;
                avail = audio_get_avail (pIter);
                if (avail > 0)
                {
                    pIter->callback.fn(pIter->callback.opaque, avail);
                }
            }
        }
    }
}

static void drvAudioCapture(PDRVAUDIO pThis)
{
    CaptureVoiceOut *cap;
    CaptureVoiceOut *pIter;

    RTListForEach(&pThis->HeadCapturedVoice, pIter, CaptureVoiceOut, ListCapturedVoice)
    {
        cap = pIter;
        int live, rpos, captured;
        PPDMHOSTVOICEOUT hw = &cap->hw;
        PPDMGSTVOICEOUT sw;
        PPDMGSTVOICEOUT pIter;

        captured = live = audio_pcm_hw_get_live_out (hw);
        rpos = hw->offRead;
        while (live)
        {
            int left = hw->cSamples - rpos;
            int to_capture = audio_MIN (live, left);
            st_sample_t *src;
            struct capture_callback *cb;

            src = hw->pHostSterioSampleBuf + rpos;
            clipAudio (cap->buf, src, to_capture);
            mixeng_sniff_and_clear (hw, src, to_capture);

            rpos = (rpos + to_capture) % hw->cSamples;
            live -= to_capture;
        }
        hw->offRead = rpos;

        RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
        {
            sw = pIter;
            LogFlow(("6\n"));
            if (!sw->State.fActive && sw->State.fEmpty) {
                continue;
            }

            if (captured > sw->cSamplesMixed)
            {
                LogFlow(("DrvAudio: captured=%d sw->total_hw_samples_mixed=%d\n",
                       captured, sw->cSamplesMixed));
                captured = sw->cSamplesMixed;
            }

            sw->cSamplesMixed -= captured;
            sw->State.fEmpty = sw->cSamplesMixed == 0;
        }
    }
}
static void drvAudioTimer(PDRVAUDIO pThis)
{
        drvAudioPlayOut(pThis);
        drvAudioPlayIn(pThis);
        drvAudioCapture(pThis);
        TMTimerSet (pThis->pTimer, TMTimerGet (pThis->pTimer) + pThis->ticks);
}

static int audio_driver_init (PCFGMNODE pCfgHandle, PDRVAUDIO pThis, struct audio_driver *drv)
{
    int max_voices;
    int voice_size;
    if (drv->options)
    {
        drvAudioProcessOptions(pCfgHandle, drv->name, drv->options);
    }
    if (!pThis->pHostDrvAudio->pfnInit(pThis->pHostDrvAudio))
    {
        LogFlow(("DrvAudio: Could not init audio driver %p\n", pThis));
        return VERR_GENERAL_FAILURE;
    }

    max_voices = pThis->AudioConf.MaxHostVoicesOut;
    voice_size = pThis->AudioConf.szHostVoiceOut;
    pThis->cHostOutVoices = 1;
    if (pThis->cHostOutVoices > max_voices)
    {
        if (!max_voices)
            LogFlow(("Driver `%s' does not support \n", drv->name));
        else
            LogFlow(("Driver `%s' does not support %d   voices, max %d\n",
                   drv->name,
                   pThis->cHostOutVoices,
                   max_voices));
        pThis->cHostOutVoices = 1;//max_voices;
    }

    if (!voice_size && max_voices)
    {
        LogFlow(("drv=`%s' voice_size=0 max_voices=%d\n",
               drv->name, max_voices));
        pThis->cHostOutVoices = 0;
    }

    if (voice_size && !max_voices)
    {
        LogFlow(("drv=`%s' voice_size=%d max_voices=0\n",
               drv->name, voice_size));
    }


    max_voices = pThis->AudioConf.MaxHostVoicesIn;
    voice_size = pThis->AudioConf.szHostVoiceIn;

    LogFlow(("DrvAudio: voice_size =%d max_voices=%d cHostInVoices=%d\n", voice_size, max_voices, pThis->cHostInVoices));
    //pThis->cHostInVoices = 1; //@todo handle this
    if (pThis->cHostInVoices > max_voices)
    {
        if (!max_voices)
            LogFlow(("Driver `%s' does not support \n", drv->name));
        else
            LogFlow(("Driver `%s' does not support %d   voices, max %d\n",
                   drv->name,
                   pThis->cHostOutVoices,
                   max_voices));
        pThis->cHostInVoices = max_voices;
    }

    if (!voice_size && max_voices)
    {
        LogFlow(("drv=`%s' voice_size=0 max_voices=%d\n",
               drv->name, max_voices));
        //@todo in original code its 0, but I have made it 1 to avoid crash
        pThis->cHostInVoices = 0;
    }

    if (voice_size && !max_voices)
    {
        LogFlow(("drv=`%s' voice_size=%d max_voices=0\n",
               drv->name, voice_size));
    }
    return VINF_SUCCESS;
}

static void audio_vm_change_state_handler (PPDMDRVINS pDrvIns,/* void *opaque,*/ int running)
{
    PPDMHOSTVOICEOUT hwo = NULL;
    PPDMHOSTVOICEIN hwi = NULL;
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    int op = running ? VOICE_ENABLE : VOICE_DISABLE;

    while ((hwo = drvAudioHlpPcmHwFindAnyEnabledOut(pThis, hwo))) {
        pThis->pHostDrvAudio->pfnDisableEnableOut(pThis->pHostDrvAudio, hwo, op);
    }

    while ((hwi = drvAudioHlpPcmHwFindAnyEnabledIn(pThis, hwi))) {
        pThis->pHostDrvAudio->pfnDisableEnableIn(pThis->pHostDrvAudio, hwi, op);
    }
}

static void drvAudioExit(PPDMDRVINS pDrvIns)
{
    PPDMHOSTVOICEOUT hwo = NULL;
    PPDMHOSTVOICEIN hwi = NULL;
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    /* VBox change: audio_pcm_hw_find_any_enabled_out => audio_pcm_hw_find_any_out */
    while ((hwo = drvAudioHlpPcmHwFindAnyOut(pThis, hwo)))
    {
        SWVoiceCap *sc;
        SWVoiceCap *pIter;

        pThis->pHostDrvAudio->pfnDisableEnableOut(pThis->pHostDrvAudio, hwo, VOICE_DISABLE);
        pThis->pHostDrvAudio->pfnFiniOut(pThis->pHostDrvAudio, hwo);

        RTListForEach(&hwo->HeadCapturedVoice, pIter, SWVoiceCap, ListCapturedVoice)
        {
            sc = pIter;
            CaptureVoiceOut *cap = sc->cap;
            struct capture_callback *cb;
        }
    }

    /* VBox change: audio_pcm_hw_find_any_enabled_in => audio_pcm_hw_find_any_in */
    while ((hwi = drvAudioHlpPcmHwFindAnyIn(pThis, hwi)))
    {
        pThis->pHostDrvAudio->pfnDisableEnableIn(pThis->pHostDrvAudio, hwi, VOICE_DISABLE);
        pThis->pHostDrvAudio->pfnFiniIn(pThis->pHostDrvAudio, hwi);
    }
}

static DECLCALLBACK(void) audio_timer_helper (PPDMDRVINS pDrvIns, PTMTIMER pTimer, void *pvUser)
{
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    drvAudioTimer(pThis);
}

static struct audio_option audio_options[] =
{
    /* DAC */
    {"DACFixedSettings", AUD_OPT_BOOL, &conf.fixed_out.enabled,
     "Use fixed settings for host DAC", NULL, 0},

    {"DACFixedFreq", AUD_OPT_INT, &conf.fixed_out.settings.freq,
     "Frequency for fixed host DAC", NULL, 0},

    {"DACFixedFmt", AUD_OPT_FMT, &conf.fixed_out.settings.fmt,
     "Format for fixed host DAC", NULL, 0},

    {"DACFixedChannels", AUD_OPT_INT, &conf.fixed_out.settings.nchannels,
     "Number of channels for fixed DAC (1 - mono, 2 - stereo)", NULL, 0},

    {"DACVoices", AUD_OPT_INT, &conf.fixed_out.nb_voices,
     "Number of voices for DAC", NULL, 0},

    /* ADC */
    {"ADCFixedSettings", AUD_OPT_BOOL, &conf.fixed_in.enabled,
     "Use fixed settings for host ADC", NULL, 0},

    {"ADCFixedFreq", AUD_OPT_INT, &conf.fixed_in.settings.freq,
     "Frequency for fixed host ADC", NULL, 0},

    {"ADCFixedFmt", AUD_OPT_FMT, &conf.fixed_in.settings.fmt,
     "Format for fixed host ADC", NULL, 0},

    {"ADCFixedChannels", AUD_OPT_INT, &conf.fixed_in.settings.nchannels,
     "Number of channels for fixed ADC (1 - mono, 2 - stereo)", NULL, 0},

    {"ADCVoices", AUD_OPT_INT, &conf.fixed_in.nb_voices,
     "Number of voices for ADC", NULL, 0},

    /* Misc */
    {"TimerFreq", AUD_OPT_INT, &conf.period.hz,
     "Timer frequency in Hz (0 - use lowest possible)", NULL, 0},

    {"PLIVE", AUD_OPT_BOOL, &conf.plive,
     "(undocumented)", NULL, 0},

    {NULL, 0, NULL, NULL, NULL, 0}
};

static DECLCALLBACK (void) drvAudioRegisterCard(PPDMIAUDIOCONNECTOR pInterface, const char *name)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    LogFlow(("DrvAudio: drvAudioRegisterCard \n"));
    pThis->qemuSoundCard.name = RTStrDup(name);
}

static DECLCALLBACK(int) drvAudioInit(PCFGMNODE pCfgHandle, PPDMDRVINS pDrvIns, const char *drvname, PDRVAUDIO  pDrvAudio)
{
    size_t i;
    int done = 0;
    int rc;
    PDRVAUDIO  pThis  = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlow(("DrvAudio: drvAudioInit pDrvAudio=%p and pDrvIns=%p\n", pDrvAudio, pDrvIns));

    RTListInit(&pDrvAudio->HeadHostVoiceOut);
    RTListInit(&pDrvAudio->HeadHostVoiceIn);
    RTListInit(&pDrvAudio->HeadHostVoiceIn);
    RTListInit(&pDrvAudio->HeadCapturedVoice);

    /* get the configuration data from the backend */
    pDrvAudio->pHostDrvAudio->pfnGetConf(pDrvAudio->pHostDrvAudio, &pDrvAudio->AudioConf);

    rc = PDMDrvHlpTMTimerCreate (pDrvIns, TMCLOCK_VIRTUAL, audio_timer_helper,
                                 pThis, 0, "Audio timer", &pDrvAudio->pTimer);
    if (RT_FAILURE (rc))
    {
        LogRel(("DrvAudio: Failed to create timer \n"));
        return rc;
   }

    drvAudioProcessOptions(pCfgHandle, "AUDIO", audio_options);

    pDrvAudio->cHostOutVoices = conf.fixed_out.nb_voices;
    pDrvAudio->cHostInVoices = conf.fixed_in.nb_voices;

    pDrvAudio->ticks = 200; /* initialization of ticks */

    /* initialization of audio buffer. Create ring buffer of around 200 KB each . */
    IORingBufferCreate(&pDrvAudio->pAudioWriteBuf, 512000); /* 500 KB */

    /* allocating space for about 500 msec of audio data 48KHz, 128 bit sample
     * (guest format - PDMHOSTSTEREOSAMPLE) and dual channel
     */
    IORingBufferCreate(&pDrvAudio->pAudioReadBuf, 768000); /* 750 KB */


    if (pDrvAudio->cHostOutVoices <= 0)
    {
        LogFlow(("Bogus number of playback voices %d, setting to 1\n", pDrvAudio->cHostOutVoices));
        pDrvAudio->cHostOutVoices = 1;
    }

    if (pDrvAudio->cHostInVoices <= 0)
    {
        LogFlow(("Bogus number of capture voices %d, setting to 0\n", pDrvAudio->cHostInVoices));
        //@todo in original code its set to 0 currently being set to 1 to ensure atleas 1 voice IN
        pDrvAudio->cHostInVoices = 1;
    }
    LogFlow(("Audio: Trying driver '%s'.\n", drvname));

    if (drvname)
    {
        int found = 0;
        for (i = 0; i < sizeof (drvtab) / sizeof (drvtab[0]); i++)
        {
            /* @todo: audioVRDE name wont be visible here. So hardcoding */
            if (!strcmp (drvname, drvtab[i]->name) || !strcmp(drvname, "AudioVRDE"))
            {
                if (!strcmp(drvname, "AudioVRDE"))
                {
                    struct audio_driver vrde_audio_driver =
                    {
                        INIT_FIELD (name           = ) "AudioVRDE",
                        INIT_FIELD (descr          = ) "AudioVRDE http://www.pulseaudio.org",
                        INIT_FIELD (options        = ) NULL,
                    };
                    struct audio_driver *drvtabAudioVRDE = &vrde_audio_driver;
                        done = !audio_driver_init (pCfgHandle, pDrvAudio, drvtabAudioVRDE);
                }
                else
                {
                    done = !audio_driver_init (pCfgHandle, pDrvAudio, drvtab[i]);
                }
                found = 1;
                break;
            }
        }

        if (!found)
        {
            LogFlow(("Audio: Unknown audio driver `%s'\n", drvname));
        }
    }

    if (!done)
    {
        for (i = 0; !done && i < sizeof (drvtab) / sizeof (drvtab[0]); i++) {
            if (drvtab[i]->can_be_default) {
                LogRel(("Audio: Initialization of driver '%s' failed, trying '%s'.\n",drvname, drvtab[i]->name));
                drvname = drvtab[i]->name;
                done = !audio_driver_init (pCfgHandle, pDrvAudio, drvtab[i]);
            }
        }
    }

    if (!done) {
        done = !audio_driver_init (pCfgHandle, pThis, &no_audio_driver);
        if (!done) {
            LogFlow(("Could not initialize audio subsystem\n"));
        }
        else {
            LogRel(("Audio: Initialization of driver '%s' failed, using NULL driver.\n", drvname));
            LogFlow(("warning: Using timer based audio emulation\n"));
        }
    }
    if (done)
    {
        if (conf.period.hz <= 0)
        {
            if (conf.period.hz < 0)
            {
                LogFlow(("warning: Timer period is negative - %d "
                       "treating as zero\n",
                       conf.period.hz));
            }
            pDrvAudio->ticks = 1;
        }
        else
        {
            pDrvAudio->ticks =  PDMDrvHlpTMGetVirtualFreq(pDrvIns) / conf.period.hz;
        }
    }
    else {
        /* XXX */
        rc = TMR3TimerDestroy (pDrvAudio->pTimer);
        return rc;
    }
    TMTimerSet (pDrvAudio->pTimer, TMTimerGet (pDrvAudio->pTimer) + pDrvAudio->ticks);
    return VINF_SUCCESS;
}
static DECLCALLBACK(int) drvAudioInitNull(PPDMIAUDIOCONNECTOR pInterface)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    return audio_driver_init(NULL, pThis, &no_audio_driver);
}

static DECLCALLBACK(int) drvAudioWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT sw, void *buf, int size)
{
    int bytes;
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (!sw || size < 0)
    {
        LogFlow(("DrvAudio: GstVoiceOut is NULL \n"));
        /* XXX: Consider options */
        return size;
    }

    if (!sw->pHostVoiceOut->fEnabled) {
        return 0;
    }
    bytes = audio_pcm_sw_write(pThis, sw, buf, size);
    return bytes;
}

static DECLCALLBACK(int) drvAudioRead(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN sw, void *buf, int size)
{
    int bytes;
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (!sw) {
        /* XXX: Consider options */
        return size;
    }

    if (!sw->hw->enabled) {
        LogFlow(("DrvAudio: Reading from disabled voice \n"));
        return 0;
    }
    bytes = audio_pcm_sw_read(pThis, sw, (PPDMHOSTSTEREOSAMPLE)buf, size);
    return bytes;
}

static DECLCALLBACK(void) drvAudioIsSetOutVolume (PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT sw, int mute, uint8_t lvol, uint8_t rvol)
{
    if (sw)
    {
        sw->State.uVolumeMute = mute;
        sw->State.uVolumeLeft    = (uint32_t)lvol * 0x808080; /* maximum is INT_MAX = 0x7fffffff */
        sw->State.uVolumeRight    = (uint32_t)rvol * 0x808080; /* maximum is INT_MAX = 0x7fffffff */
    }
}


static DECLCALLBACK(void) drvAudioSetVolume (PPDMIAUDIOCONNECTOR pInterface, int *mute, uint8_t *lvol, uint8_t *rvol)
{
    volume_t vol;
    const char *name;

    uint32_t u32VolumeLeft  = (uint32_t)*lvol;
    uint32_t u32VolumeRight = (uint32_t)*rvol;
    /* 0x00..0xff => 0x01..0x100 */
    if (u32VolumeLeft)
        u32VolumeLeft++;
    if (u32VolumeRight)
        u32VolumeRight++;
    vol.mute  = *mute;
    vol.l     = u32VolumeLeft  * 0x800000; /* maximum is 0x80000000 */
    vol.r     = u32VolumeRight * 0x800000; /* maximum is 0x80000000 */
    sum_out_volume.mute = 0;
    sum_out_volume.l    = ASMMultU64ByU32DivByU32(INT_MAX, INT_MAX, 0x80000000U);
    sum_out_volume.r    = ASMMultU64ByU32DivByU32(INT_MAX, INT_MAX, 0x80000000U);
}

static DECLCALLBACK(void) drvAudioEnableOut (PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT sw, int on)
{
    PPDMHOSTVOICEOUT hw;
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (!sw)
        return;

    hw = sw->pHostVoiceOut;
    if (sw->State.fActive != on)
    {
        PPDMGSTVOICEOUT pIter;
        SWVoiceCap *sc;
        SWVoiceCap *pIterCap;

        if (on)
        {
            hw->pending_disable = 0;
            if (!hw->fEnabled)
            {
                hw->fEnabled = 1;
                pThis->pHostDrvAudio->pfnDisableEnableOut(pThis->pHostDrvAudio, hw, VOICE_ENABLE);
            }
        }
        else
        {
            if (hw->fEnabled)
            {
                int nb_active = 0;

                RTListForEach(&hw->HeadGstVoiceOut, pIter, PDMGSTVOICEOUT, ListGstVoiceOut)
                {
                    nb_active += pIter->State.fActive != 0;
                }

                hw->pending_disable = nb_active == 1;
            }
        }

        RTListForEach(&hw->HeadCapturedVoice, pIterCap, SWVoiceCap, ListCapturedVoice)
        {
            sc = pIterCap;
            sc->sw.State.fActive = hw->fEnabled;
            if (hw->fEnabled) {
                audio_capture_maybe_changed (sc->cap, 1);
            }
        }
        sw->State.fActive = on;
    }
}

static DECLCALLBACK(void) drvAudioEnableIn(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN sw , int on)
{
    PPDMHOSTVOICEIN hw;
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (!sw) {
        LogFlow(("DrvAudio: NO GuestVoiceIn. Returning 0\n"));
        return;
    }

    hw = sw->hw;
    if (sw->State.fActive != on)
    {
        PPDMGSTVOICEIN pIter;

        if (on)
        {
            if (!hw->enabled)
            {
                hw->enabled = 1;
                pThis->pHostDrvAudio->pfnDisableEnableIn(pThis->pHostDrvAudio, hw, VOICE_ENABLE);
            }
            sw->cHostSamplesAcquired = hw->cSamplesCaptured;
        }
        else
        {
            if (hw->enabled)
            {
                int nb_active = 0;

                RTListForEach(&hw->HeadGstVoiceIn, pIter, PDMGSTVOICEIN, ListGstVoiceIn)
                {
                    nb_active += pIter->State.fActive != 0;
                }

                if (nb_active == 1) {
                    hw->enabled = 0;
                    pThis->pHostDrvAudio->pfnDisableEnableIn(pThis->pHostDrvAudio, hw, VOICE_DISABLE);
                }
            }
        }
        sw->State.fActive = on;
    }
}

static DECLCALLBACK(int) drvAudioIsHostVoiceInOK(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN sw)
{
    if (sw == NULL) {
        return 0;
    }
    return 1;
}

static DECLCALLBACK(int) drvAudioIsHostVoiceOutOK(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT sw)
{
    if (sw == NULL) {
        return 0;
    }
    return 1;
}

static DECLCALLBACK(int) drvAudioOpenIn(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN *ppGstVoiceIn,
                                        const char *name, void *callback_opaque , audio_callback_fn_t callback_fn,
                                        uint32_t uFrequency, uint32_t cChannels, audfmt_e Format, uint32_t Endian)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    audsettings_t AudioSettings;
    AudioSettings.freq = uFrequency;
    AudioSettings.nchannels = cChannels;
    AudioSettings.fmt = Format;
    AudioSettings.endianness = Endian;
    LogFlow(("DrvAudio: open IN %s, freq %d, nchannels %d, fmt %d\n",
             name, AudioSettings.freq, AudioSettings.nchannels, AudioSettings.fmt));

    if (drvAudioValidateSettings(&AudioSettings))
    {
        LogRel(("Audio: Audio Settings Validation failed \n"));
        drvAudioPrintSettings(&AudioSettings);
        drvAudioHlpCloseIn(pThis, &pThis->qemuSoundCard, *ppGstVoiceIn);
        *ppGstVoiceIn = (PPDMGSTVOICEIN)NULL;
        return VERR_GENERAL_FAILURE;
    }

    if (*ppGstVoiceIn && drvAudioPcmInfoEq(&(*ppGstVoiceIn)->Props, &AudioSettings))
        return VINF_SUCCESS;

    if (!conf.fixed_in.enabled && *ppGstVoiceIn)
    {
        drvAudioHlpCloseIn(pThis, &pThis->qemuSoundCard, *ppGstVoiceIn);
        *ppGstVoiceIn = (PPDMGSTVOICEIN )NULL;
    }

    if (*ppGstVoiceIn)
    {
        PPDMHOSTVOICEIN pHostVoiceIn = (*ppGstVoiceIn)->hw;

        if (!pHostVoiceIn)
        {
            LogFlow(("Internal logic error voice has no hardware store\n"));
            drvAudioHlpCloseIn(pThis, &pThis->qemuSoundCard, *ppGstVoiceIn);
            *ppGstVoiceIn = (PPDMGSTVOICEIN)NULL;
            return VERR_GENERAL_FAILURE;
        }

        drvAudioHlpPcmSwFinishedIn(*ppGstVoiceIn);
        if (drvAudioHlpPcmSwInitIn(*ppGstVoiceIn, pHostVoiceIn, name, &AudioSettings))
        {
            drvAudioHlpCloseIn(pThis, &pThis->qemuSoundCard, *ppGstVoiceIn);
            *ppGstVoiceIn = (PPDMGSTVOICEIN)NULL;
            return VERR_GENERAL_FAILURE;
        }
    }
    else
    {
        *ppGstVoiceIn = drvAudioHlpPcmCreateVoicePairIn(pThis, name, &AudioSettings);
        if (!(*ppGstVoiceIn))
        {
            LogFlow(("Failed to create voice `%s'\n", name));
            *ppGstVoiceIn = (PPDMGSTVOICEIN)NULL;
            return VERR_GENERAL_FAILURE;
        }
    }

    if (*ppGstVoiceIn)
    {
        (*ppGstVoiceIn)->State.uVolumeLeft = nominal_volume.l;
        (*ppGstVoiceIn)->State.uVolumeRight = nominal_volume.r;
        (*ppGstVoiceIn)->State.uVolumeMute = nominal_volume.mute;
        (*ppGstVoiceIn)->callback.fn = callback_fn;
        (*ppGstVoiceIn)->callback.opaque = callback_opaque;
    }
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) drvAudioOpenOut(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT *ppGstVoiceOut, const char *name,
                                         void *callback_opaque, audio_callback_fn_t callback_fn,
                                         uint32_t uFrequency, uint32_t cChannels, audfmt_e Format, uint32_t Endian)
{
    int cLiveSamples = 0;
    AudioState *s;
    int rc;
    PPDMGSTVOICEOUT pOldGstVoiceOut;
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    audsettings_t AudioSettings;
    AudioSettings.freq = uFrequency;
    AudioSettings.nchannels = cChannels;
    AudioSettings.fmt = Format;
    AudioSettings.endianness = Endian;

    LogFlow(("DrvAudio: open OUT %s, freq %d, nchannels %d, fmt %d\n",
            name, AudioSettings.freq, AudioSettings.nchannels, AudioSettings.fmt));

    if (drvAudioValidateSettings(&AudioSettings))
    {
        LogRel(("DrvAudio: Audio Settings Validation failed \n"));
        drvAudioPrintSettings(&AudioSettings);
        drvAudioHlpCloseOut(pThis, &pThis->qemuSoundCard, *ppGstVoiceOut);
        *ppGstVoiceOut = (PPDMGSTVOICEOUT)NULL;
        return VERR_GENERAL_FAILURE;
    }

    if (*ppGstVoiceOut && drvAudioPcmInfoEq(&(*ppGstVoiceOut)->Props, &AudioSettings))
        return VINF_SUCCESS;

    if (  conf.plive && *ppGstVoiceOut
        && (!(*ppGstVoiceOut)->State.fActive
        && !(*ppGstVoiceOut)->State.fEmpty)
       )
    {
        cLiveSamples = (*ppGstVoiceOut)->cSamplesMixed;
        if(cLiveSamples)
        {
            pOldGstVoiceOut = *ppGstVoiceOut;
            *ppGstVoiceOut = NULL;
        }
    }

    if (!conf.fixed_out.enabled && *ppGstVoiceOut)
    {
        drvAudioHlpCloseOut(pThis, &pThis->qemuSoundCard, *ppGstVoiceOut);
        *ppGstVoiceOut = (PPDMGSTVOICEOUT *)NULL;
    }

    if (*ppGstVoiceOut)
    {
        PPDMHOSTVOICEOUT pHostVoiceOut = (*ppGstVoiceOut)->pHostVoiceOut;
        if (!pHostVoiceOut)
        {
            LogFlow(("Guest Voice Stream has no Host voice stream in store\n"));
            return VERR_GENERAL_FAILURE;
        }

        drvAudioHlpPcmSwFinishedOut(*ppGstVoiceOut);
        rc = drvAudioHlpPcmSwInitOut(*ppGstVoiceOut, pHostVoiceOut, name, &AudioSettings);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        (*ppGstVoiceOut) = drvAudioHlpPcmCreateVoicePairOut(pThis, s, name, &AudioSettings);
        if (!(*ppGstVoiceOut))
        {
            LogFlow(("Failed to create voice `%s'\n", name));
            *ppGstVoiceOut = (PPDMGSTVOICEOUT)NULL;
            return VERR_GENERAL_FAILURE;
        }
    }

    if (*ppGstVoiceOut)
    {
        (*ppGstVoiceOut)->State.uVolumeLeft = nominal_volume.l;
        (*ppGstVoiceOut)->State.uVolumeRight = nominal_volume.r;
        (*ppGstVoiceOut)->State.uVolumeMute = nominal_volume.mute;
        (*ppGstVoiceOut)->callback.fn = callback_fn;
        (*ppGstVoiceOut)->callback.opaque = callback_opaque;
        if (cLiveSamples)
        {
            int mixed =
                (cLiveSamples << pOldGstVoiceOut->Props.cShift)
                * pOldGstVoiceOut->Props.cbPerSec
                / (*ppGstVoiceOut)->Props.cbPerSec;

            (*ppGstVoiceOut)->cSamplesMixed += mixed;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioIsActiveIn(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN pGstVoiceIn)
{
    return pGstVoiceIn ? pGstVoiceIn->State.fActive : 0;
}

static DECLCALLBACK(int) drvAudioIsActiveOut(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT pGstVoiceOut)
{
    return pGstVoiceOut ? pGstVoiceOut->State.fActive : 0;
}

static DECLCALLBACK(t_sample *)drvAudioConvDevFmtToStSample(PPDMIAUDIOCONNECTOR pInterface, uint32_t cChannels,
                                                     uint32_t fSign, uint32_t uEndian, uint32_t ubitIdx)
{
    LogFlow(("DrvAudio: drvAudioConvStSamplToFmt \n"));
    return   mixeng_conv[cChannels] /* stereo */
                         [fSign]     /* sign */
                         [uEndian]   /* big endian */
                         [ubitIdx];  /* bits */

}

static DECLCALLBACK(f_sample *)drvAudioConvStSampleToDevFmt(PPDMIAUDIOCONNECTOR pInterface, void *buf,
                                                            PPDMHOSTSTEREOSAMPLE pSampleBuf, uint32_t samples)
{
    /*@todo handle ths properly*/
    clipAudioIn (buf, pSampleBuf, samples);
}


static DECLCALLBACK(void *)drvAudioPrepareAudioConversion(PPDMIAUDIOCONNECTOR pInterface, uint32_t uSampleFreq, uint32_t uTgtFreq)
{
    return st_rate_start (uSampleFreq, uTgtFreq);
}

static DECLCALLBACK(void)drvAudioEndAudioConversion(PPDMIAUDIOCONNECTOR pInterface, void * pRate)
{
    AssertPtr(pRate);
    st_rate_stop (pRate);
}

static DECLCALLBACK(void)drvAudioDoRateConversion(PPDMIAUDIOCONNECTOR pInterface, void * pRate,
                                                  PPDMHOSTSTEREOSAMPLE pHostStereoSampleBuf,
                                                  PPDMHOSTSTEREOSAMPLE pConvertedSampleBuf,
                                                  uint32_t * pcSampleSrc, uint32_t *pcConvertedSamples)
{
    AssertPtr(pRate);
    st_rate_flow(pRate, pHostStereoSampleBuf, pConvertedSampleBuf, pcSampleSrc, pcConvertedSamples);

}

static DECLCALLBACK(void)drvAudioDoRateConvAndMix(PPDMIAUDIOCONNECTOR pInterface, void * pRate,
                                                  PPDMHOSTSTEREOSAMPLE pSourceSampleBuf,
                                                  PPDMHOSTSTEREOSAMPLE pTargetMixedSampleBuf,
                                                  uint32_t * pcSampleSrc, uint32_t *pcMixedSamples)
{
    st_rate_flow_mix(pRate, pSourceSampleBuf, pTargetMixedSampleBuf, pcSampleSrc, pcMixedSamples);

}

/****************************************************************/

static DECLCALLBACK(void) drvAudioCloseIn(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEIN pGstVoiceIn)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (pGstVoiceIn)
    {
        if (!&pThis->qemuSoundCard || !pThis->qemuSoundCard.audio)
        {
            LogFlow(("DrvAudio: Error, card=%p card->audio=%p\n",
                   (void *) &pThis->qemuSoundCard, &pThis->qemuSoundCard ? (void *) pThis->qemuSoundCard.audio : NULL));
            return;
        }
        drvAudioHlpCloseIn(pThis, pThis->qemuSoundCard.audio, pGstVoiceIn);
    }
}

DECLCALLBACK(void) drvAudioCloseOut(PPDMIAUDIOCONNECTOR pInterface, PPDMGSTVOICEOUT pGstVoiceOut)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (pGstVoiceOut) {
        if (!&pThis->qemuSoundCard || !pThis->qemuSoundCard.audio)
        {
            LogFlow(("DrvAudio: Error, card=%p card->audio=%p\n",
                   (void *) &pThis->qemuSoundCard, &pThis->qemuSoundCard ? (void *) pThis->qemuSoundCard.audio : NULL));
            return;
        }

        drvAudioHlpCloseOut(pThis, pThis->qemuSoundCard.audio, pGstVoiceOut);
    }
}


/********************************************************************/



/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOCONNECTOR, &pThis->IAudioConnector);
    return NULL;
}

/**
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioPowerOff(PPDMDRVINS pDrvIns)
{
    audio_vm_change_state_handler (pDrvIns, 0);
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("drvAUDIODestruct:\n"));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (audio_streamname)
    {
        MMR3HeapFree(audio_streamname);
        audio_streamname = NULL;
    }
    drvAudioExit(pDrvIns);
}


/**
 * Construct an AUDIO driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVAUDIO   pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    char       *drvname;
    int         rc = 0;
    PPDMIBASE pBase;

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                    = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface  = drvAudioQueryInterface;
    /* IAudio */
    pThis->IAudioConnector.pfnRead             = drvAudioRead;
    pThis->IAudioConnector.pfnWrite            = drvAudioWrite;
    pThis->IAudioConnector.pfnRegisterCard     = drvAudioRegisterCard;
    pThis->IAudioConnector.pfnIsHostVoiceInOK  = drvAudioIsHostVoiceInOK;
    pThis->IAudioConnector.pfnIsHostVoiceOutOK = drvAudioIsHostVoiceOutOK;
    pThis->IAudioConnector.pfnInitNull         = drvAudioInitNull;
    pThis->IAudioConnector.pfnIsSetOutVolume   = drvAudioIsSetOutVolume;
    pThis->IAudioConnector.pfnSetVolume        = drvAudioSetVolume;
    pThis->IAudioConnector.pfnEnableOut        = drvAudioEnableOut;
    pThis->IAudioConnector.pfnEnableIn         = drvAudioEnableIn;
    pThis->IAudioConnector.pfnCloseIn          = drvAudioCloseIn;
    pThis->IAudioConnector.pfnCloseOut         = drvAudioCloseOut;
    pThis->IAudioConnector.pfnOpenIn           = drvAudioOpenIn;
    pThis->IAudioConnector.pfnOpenOut          = drvAudioOpenOut;
    pThis->IAudioConnector.pfnIsActiveIn       = drvAudioIsActiveIn;
    pThis->IAudioConnector.pfnIsActiveOut      = drvAudioIsActiveOut;
    /* Mixer/Conversion */
    pThis->IAudioConnector.pfnConvDevFmtToStSample   = drvAudioConvDevFmtToStSample;
    pThis->IAudioConnector.pfnConvStSampleToDevFmt   = drvAudioConvStSampleToDevFmt;
    pThis->IAudioConnector.pfnPrepareAudioConversion = drvAudioPrepareAudioConversion;
    pThis->IAudioConnector.pfnEndAudioConversion     = drvAudioEndAudioConversion;
    pThis->IAudioConnector.pfnDoRateConversion       = drvAudioDoRateConversion;
    pThis->IAudioConnector.pfnDoRateConvAndMix       = drvAudioDoRateConvAndMix;

    rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pBase);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to attach  the audio device rc = %d \n", rc));
        return rc;
    }
    pThis->pHostDrvAudio = PDMIBASE_QUERY_INTERFACE(pBase, PDMIHOSTAUDIO);
    if (!pThis->pHostDrvAudio)
    {
        LogRel(("Audio: Failed to attach to underlying host driver \n"));
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("No media or async media interface below"));
    }

    pThis->pDrvIns = pDrvIns;
    gpDrvIns = pThis->pDrvIns;
    rc = CFGMR3QueryStringAlloc (pCfgHandle, "AudioDriver", &drvname);
    if (RT_FAILURE (rc))
    {
        LogFlow(("Failed to get AudioDriver from CFGM\n"));
        return rc;
    }

    rc = CFGMR3QueryStringAlloc (pCfgHandle, "StreamName", &audio_streamname);
    if (RT_FAILURE (rc))
    {
        LogFlow(("Failed to get SteamName from CFGM \n"));
        audio_streamname = NULL;
    }
    rc = drvAudioInit(pCfgHandle, pDrvIns, drvname, pThis);
    if (RT_FAILURE (rc))
        return rc;

    MMR3HeapFree (drvname);

    return VINF_SUCCESS;
}

/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioSuspend(PPDMDRVINS pDrvIns)
{
    audio_vm_change_state_handler(pDrvIns, 0);
}

/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) audioResume(PPDMDRVINS pDrvIns)
{
    audio_vm_change_state_handler(pDrvIns, 1);
}

/**
 * Audio driver registration record.
 */
const PDMDRVREG g_DrvAUDIO =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "AUDIO",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "AUDIO Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    2,
    /* cbInstance */
    sizeof(DRVAUDIO),
    /* pfnConstruct */
    drvAudioConstruct,
    /* pfnDestruct */
    drvAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvAudioSuspend,
    /* pfnResume */
    audioResume,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvAudioPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};


