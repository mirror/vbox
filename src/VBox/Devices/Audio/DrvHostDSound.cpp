/* $Id$ */
/** @file
 * Windows host backend driver using DirectSound.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
 * This code is based on: dsoundaudio.c
 *
 * QEMU DirectSound audio driver
 *
 * Copyright (c) 2005 Vassili Karpov (malc)
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

#include <dsound.h>

#include <iprt/alloc.h>
#include <iprt/uuid.h>

#include "AudioMixBuffer.h"
#include "DrvAudio.h"
#include "VBoxDD.h"

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

#define DSLOG(a) do { LogRel2(a); } while(0)
#define DSLOGREL(a)                 \
    do {                            \
        static int8_t scLogged = 0; \
        if (scLogged < 8) {         \
            ++scLogged;             \
            LogRel(a);              \
        }                           \
        else {                      \
            DSLOG(a);               \
        }                           \
    } while (0)

typedef struct DSOUNDHOSTCFG
{
    int     cbBufferIn;
    int     cbBufferOut;
    RTUUID  uuidPlay;
    LPCGUID pGuidPlay;
    RTUUID  uuidCapture;
    LPCGUID pGuidCapture;
} DSOUNDHOSTCFG, *PDSOUNDHOSTCFG;

typedef struct DRVHOSTDSOUND
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS    pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO IHostAudio;
    /** List of found host input devices. */
    RTLISTANCHOR  lstDevInput;
    /** List of found host output devices. */
    RTLISTANCHOR  lstDevOutput;
    /** Configuration options. */
    DSOUNDHOSTCFG cfg;
} DRVHOSTDSOUND, *PDRVHOSTDSOUND;

typedef struct DSOUNDSTREAMOUT
{
    PDMAUDIOHSTSTRMOUT  hw; /* Always must come first! */
    LPDIRECTSOUND       pDS;
    LPDIRECTSOUNDBUFFER pDSB;
    DWORD               cbPlayWritePos;
    DWORD               csPlaybackBufferSize;
    bool                fReinitPlayPos;
    PDMAUDIOSTREAMCFG   streamCfg;
} DSOUNDSTREAMOUT, *PDSOUNDSTREAMOUT;

typedef struct DSOUNDSTREAMIN
{
    PDMAUDIOHSTSTRMIN          hw; /* Always must come first! */
    LPDIRECTSOUNDCAPTURE       pDSC;
    LPDIRECTSOUNDCAPTUREBUFFER pDSCB;
    DWORD                      csCaptureReadPos;
    DWORD                      csCaptureBufferSize;
    HRESULT                    hrLastCaptureIn;
    PDMAUDIORECSOURCE          enmRecSource;
    PDMAUDIOSTREAMCFG          streamCfg;
} DSOUNDSTREAMIN, *PDSOUNDSTREAMIN;

/**
 * Callback context for enumeration callbacks
 */
typedef struct DSOUNDENUMCBCTX
{
    PDRVHOSTDSOUND      pDrv;
    PPDMAUDIOBACKENDCFG pCfg;
} DSOUNDENUMCBCTX, *PDSOUNDENUMCBCTX;

typedef struct DSOUNDDEV
{
    RTLISTNODE  Node;
    char       *pszName;
    GUID        Guid;
} DSOUNDDEV, *PDSOUNDDEV;

/** Makes DRVHOSTDSOUND out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface) \
    ( (PDRVHOSTDSOUND)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTDSOUND, IHostAudio)) )

static void dsoundDevRemove(PDSOUNDDEV pDev);

static size_t dsoundRingDistance(size_t offEnd, size_t offBegin, size_t cSize)
{
    return offEnd >= offBegin ? offEnd - offBegin: cSize - offBegin + offEnd;
}

static int dsoundWaveFmtFromCfg(PPDMAUDIOSTREAMCFG pCfg, PWAVEFORMATEX pFmt)
{
    RT_BZERO(pFmt, sizeof(WAVEFORMATEX));
    pFmt->wFormatTag      = WAVE_FORMAT_PCM;
    pFmt->nChannels       = pCfg->cChannels;
    pFmt->nSamplesPerSec  = pCfg->uHz;
    pFmt->nAvgBytesPerSec = pCfg->uHz << (pCfg->cChannels == 2 ? 1: 0);
    pFmt->nBlockAlign     = 1 << (pCfg->cChannels == 2 ? 1: 0);
    pFmt->cbSize          = 0; /* No extra data specified. */

    switch (pCfg->enmFormat)
    {
        case AUD_FMT_S8:
        case AUD_FMT_U8:
            pFmt->wBitsPerSample = 8;
            break;

        case AUD_FMT_S16:
        case AUD_FMT_U16:
            pFmt->wBitsPerSample = 16;
            pFmt->nAvgBytesPerSec <<= 1;
            pFmt->nBlockAlign <<= 1;
            break;

        case AUD_FMT_S32:
        case AUD_FMT_U32:
            pFmt->wBitsPerSample = 32;
            pFmt->nAvgBytesPerSec <<= 2;
            pFmt->nBlockAlign <<= 2;
            break;

        default:
            AssertMsgFailed(("Wave format %ld not supported\n", pCfg->enmFormat));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static void dsoundFreeDeviceLists(PDRVHOSTDSOUND pThis)
{
    PDSOUNDDEV pDev;
    while (!RTListIsEmpty(&pThis->lstDevInput))
    {
        pDev = RTListGetFirst(&pThis->lstDevInput, DSOUNDDEV, Node);
        dsoundDevRemove(pDev);
    }

    while (!RTListIsEmpty(&pThis->lstDevOutput))
    {
        pDev = RTListGetFirst(&pThis->lstDevOutput, DSOUNDDEV, Node);
        dsoundDevRemove(pDev);
    }
}

static int dsoundPlayRestore(LPDIRECTSOUNDBUFFER pDSB)
{
    HRESULT hr = IDirectSoundBuffer_Restore(pDSB);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    DSLOGREL(("DSound: restore playback buffer %Rhrc\n", hr));
    return VERR_INVALID_STATE;
}

static int dsoundUnlockOutput(LPDIRECTSOUNDBUFFER pDSB,
                              LPVOID pv1, LPVOID pv2,
                              DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundBuffer_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    DSLOG(("DSound: Unable to unlock output buffer, hr=%Rhrc\n", hr));
    return VERR_ACCESS_DENIED;
}

static int dsoundUnlockInput(LPDIRECTSOUNDCAPTUREBUFFER pDSCB,
                             LPVOID pv1, LPVOID pv2,
                             DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    DSLOG(("DSound: Unable to unlock input buffer, hr=%Rhrc\n", hr));
    return VERR_ACCESS_DENIED;
}

static int dsoundLockOutput(LPDIRECTSOUNDBUFFER pDSB, PDMPCMPROPS *pProps,
                            DWORD dwOffset, DWORD dwBytes,
                            LPVOID *ppv1, LPVOID *ppv2,
                            DWORD *pcb1, DWORD *pcb2,
                            DWORD dwFlags)
{
    int rc = VINF_SUCCESS;

    LPVOID pv1 = NULL;
    LPVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundBuffer_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (hr == DSERR_BUFFERLOST)
    {
        rc = dsoundPlayRestore(pDSB);
        if (RT_SUCCESS(rc))
        {
            hr = IDirectSoundBuffer_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
        }
    }

    if (FAILED(hr) || RT_FAILURE(rc))
    {
        DSLOG(("DSound: Unable to lock output buffer, hr=%Rhrc\n", hr));
        return RT_SUCCESS(rc) ? VERR_ACCESS_DENIED: rc;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        DSLOG(("DSound: lock output returned misaligned buffer: cb1=%d, cb2=%d\n", cb1, cb2));
        dsoundUnlockOutput(pDSB, pv1, pv2, cb1, cb2);
        return VERR_INVALID_STATE;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return VINF_SUCCESS;
}

static int dsoundLockInput(LPDIRECTSOUNDCAPTUREBUFFER pDSCB, PPDMPCMPROPS pProps,
                           DWORD dwOffset, DWORD dwBytes,
                           LPVOID *ppv1, LPVOID *ppv2,
                           DWORD *pcb1, DWORD *pcb2,
                           DWORD dwFlags)
{
    LPVOID pv1 = NULL;
    LPVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundCaptureBuffer_Lock(pDSCB, dwOffset, dwBytes,
                                            &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (FAILED(hr))
    {
        DSLOG(("DSound: Unable to lock capturing buffer, hr=%Rhrc\n", hr));
        return VERR_ACCESS_DENIED;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        DSLOG(("DSound: lock input returned misaligned buffer: cb1=%d, cb2=%d\n", cb1, cb2));
        dsoundUnlockInput(pDSCB, pv1, pv2, cb1, cb2);
        return VERR_INVALID_PARAMETER;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return VINF_SUCCESS;
}


/*
 * DirectSound playback
 */

static void dsoundPlayInterfaceRelease(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDS)
    {
        IDirectSound_Release(pDSoundStrmOut->pDS);
        pDSoundStrmOut->pDS = NULL;
    }
}

static int dsoundPlayInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDS != NULL)
    {
        DSLOG(("DSound: DirectSound instance already exists\n"));
        return VINF_SUCCESS;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_ALL,
                                  IID_IDirectSound, (void **)&pDSoundStrmOut->pDS);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: DirectSound create instance %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound_Initialize(pDSoundStrmOut->pDS, pThis->cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hwnd = GetDesktopWindow();
            hr = IDirectSound_SetCooperativeLevel(pDSoundStrmOut->pDS, hwnd, DSSCL_PRIORITY);
            if (FAILED (hr))
            {
                DSLOGREL(("DSound: set cooperative level for window %p %Rhrc\n", hwnd, hr));
            }
        }
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER)
            {
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            }
            else
            {
                DSLOGREL(("DSound: DirectSound initialize %Rhrc\n", hr));
            }
            dsoundPlayInterfaceRelease(pDSoundStrmOut);
        }
    }

    return SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;
}

static void dsoundPlayClose(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    DSLOG(("DSound: playback close %p buffer %p\n", pDSoundStrmOut, pDSoundStrmOut->pDSB));

    if (pDSoundStrmOut->pDSB)
    {
        HRESULT hr = IDirectSoundBuffer_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: playback close Stop %Rhrc\n", hr));
        }

        IDirectSoundBuffer_Release(pDSoundStrmOut->pDSB);
        pDSoundStrmOut->pDSB = NULL;
    }

    dsoundPlayInterfaceRelease(pDSoundStrmOut);
}

static int dsoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    DSLOG(("DSound: playback open %p size %d bytes, freq %d, chan %d, bits %d, sign %d\n",
           pDSoundStrmOut,
           pThis->cfg.cbBufferOut,
           pDSoundStrmOut->hw.Props.uHz,
           pDSoundStrmOut->hw.Props.cChannels,
           pDSoundStrmOut->hw.Props.cBits,
           pDSoundStrmOut->hw.Props.fSigned));

    if (pDSoundStrmOut->pDSB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundBuffer already exists\n"));
        dsoundPlayClose(pDSoundStrmOut);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStrmOut->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return rc;

    rc = dsoundPlayInterfaceCreate(pThis, pDSoundStrmOut);
    if (RT_FAILURE(rc))
        return rc;

    HRESULT hr = S_OK;

    do /* To use breaks. */
    {
        DSBUFFERDESC bd;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        bd.dwBufferBytes = pThis->cfg.cbBufferOut;
        hr = IDirectSound_CreateSoundBuffer(pDSoundStrmOut->pDS,
                                            &bd, &pDSoundStrmOut->pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: playback CreateSoundBuffer %Rhrc\n", hr));
            break;
        }

        /* Query the actual parameters. */

        hr = IDirectSoundBuffer_GetFormat(pDSoundStrmOut->pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: playback GetFormat %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundBuffer_GetCaps(pDSoundStrmOut->pDSB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: playback GetCaps %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: playback format: size %d bytes\n"
                "  tag             = %d\n"
                "  nChannels       = %d\n"
                "  nSamplesPerSec  = %d\n"
                "  nAvgBytesPerSec = %d\n"
                "  nBlockAlign     = %d\n"
                "  wBitsPerSample  = %d\n"
                "  cbSize          = %d\n",
                bc.dwBufferBytes,
                wfx.wFormatTag,
                wfx.nChannels,
                wfx.nSamplesPerSec,
                wfx.nAvgBytesPerSec,
                wfx.nBlockAlign,
                wfx.wBitsPerSample,
                wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmOut->hw.Props.uAlign)
        {
            DSLOGREL(("DSound: playback GetCaps returned misaligned buffer size %ld, alignment %d\n",
                      bc.dwBufferBytes, pDSoundStrmOut->hw.Props.uAlign + 1));
        }

        if (bc.dwBufferBytes != pThis->cfg.cbBufferOut)
        {
            DSLOGREL(("DSound: playback buffer size mismatch dsound %d, requested %d bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferOut));
        }

        /* Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pDSoundStrmOut->csPlaybackBufferSize = bc.dwBufferBytes >> pDSoundStrmOut->hw.Props.cShift;
        DSLOG(("DSound: playback open csPlaybackBufferSize %d samples\n", pDSoundStrmOut->csPlaybackBufferSize));
    } while (0);

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    dsoundPlayClose(pDSoundStrmOut);
    return VERR_NOT_SUPPORTED;
}

static void dsoundPlayClearSamples(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    int rc = dsoundLockOutput(pDSoundStrmOut->pDSB, &pDSoundStrmOut->hw.Props,
                              0, pDSoundStrmOut->csPlaybackBufferSize << pDSoundStrmOut->hw.Props.cShift,
                              &pv1, &pv2, &cb1, &cb2, DSBLOCK_ENTIREBUFFER);
    if (RT_SUCCESS(rc))
    {
        int len1 = cb1 >> pDSoundStrmOut->hw.Props.cShift;
        int len2 = cb2 >> pDSoundStrmOut->hw.Props.cShift;

        if (pv1 && len1)
            audio_pcm_info_clear_buf(&pDSoundStrmOut->hw.Props, pv1, len1);

        if (pv2 && len2)
            audio_pcm_info_clear_buf(&pDSoundStrmOut->hw.Props, pv2, len2);

        dsoundUnlockOutput(pDSoundStrmOut->pDSB, pv1, pv2, cb1, cb2);
    }
}

static int dsoundPlayGetStatus(LPDIRECTSOUNDBUFFER pDSB, DWORD *pStatus)
{
    int rc = VINF_SUCCESS;

    DWORD dwStatus = 0;
    HRESULT hr = IDirectSoundBuffer_GetStatus(pDSB, &dwStatus);
    if (SUCCEEDED(hr))
    {
        if ((dwStatus & DSBSTATUS_BUFFERLOST) != 0)
        {
            rc = dsoundPlayRestore(pDSB);
            if (RT_SUCCESS(rc))
            {
                hr = IDirectSoundBuffer_GetStatus(pDSB, &dwStatus);
            }
        }
    }

    if (FAILED(hr))
    {
        DSLOG(("DSound: playback GetStatus %Rhrc\n", hr));
        if (RT_SUCCESS(rc))
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }

    if (RT_SUCCESS(rc))
    {
        *pStatus = dwStatus;
    }

    return rc;
}

static void dsoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        /* This performs some restore, so call it anyway and ignore result. */
        dsoundPlayGetStatus(pDSoundStrmOut->pDSB, &dwStatus);

        DSLOG(("DSound: playback stop\n"));

        HRESULT hr = IDirectSoundBuffer_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr))
        {
            DSLOG(("DSound: stop playback buffer %Rhrc\n", hr));
        }
    }
}

static int dsoundPlayStart(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    int rc = VINF_SUCCESS;

    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        int rc = dsoundPlayGetStatus(pDSoundStrmOut->pDSB, &dwStatus);
        if (RT_FAILURE(rc))
        {
            DSLOG(("DSound: playback start GetStatus %Rrc\n", rc));
        }
        else
        {
            if (dwStatus & DSBSTATUS_PLAYING)
            {
                DSLOG(("DSound: already playing\n"));
            }
            else
            {
                dsoundPlayClearSamples(pDSoundStrmOut);

                pDSoundStrmOut->fReinitPlayPos = true;

                DSLOG(("DSound: playback start\n"));

                HRESULT hr = IDirectSoundBuffer_Play(pDSoundStrmOut->pDSB, 0, 0, DSBPLAY_LOOPING);
                if (FAILED(hr))
                {
                    DSLOGREL(("DSound: playback start %Rhrc\n", hr));
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        }
    }
    else
    {
        rc = VERR_INVALID_STATE;
    }

    return rc;
}

/*
 * DirectSoundCapture
 */

static LPCGUID dsoundCaptureSelectDevice(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    LPCGUID pGUID = pThis->cfg.pGuidCapture;

    if (!pGUID)
    {
        PDSOUNDDEV pDev = NULL;

        switch (pDSoundStrmIn->enmRecSource)
        {
            case PDMAUDIORECSOURCE_MIC:
            {
                RTListForEach(&pThis->lstDevInput, pDev, DSOUNDDEV, Node)
                {
                    if (RTStrIStr(pDev->pszName, "Mic") == 0) /** @todo what is with non en_us windows versions? */
                        break;
                }
            } break;

            case PDMAUDIORECSOURCE_LINE_IN:
            default:
                /* Try opening the default device (NULL). */
                break;
        }

        if (pDev)
        {
            DSLOG(("DSound: Guest \"%s\" is using host \"%s\"\n",
                   drvAudioRecSourceToString(pDSoundStrmIn->enmRecSource), pDev->pszName));

            pGUID = &pDev->Guid;
        }
    }

    return pGUID;
}

static void dsoundCaptureInterfaceRelease(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    if (pDSoundStrmIn->pDSC)
    {
        IDirectSoundCapture_Release(pDSoundStrmIn->pDSC);
        pDSoundStrmIn->pDSC = NULL;
    }
}

static int dsoundCaptureInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    if (pDSoundStrmIn->pDSC != NULL)
    {
        DSLOG(("DSound: DirectSoundCapture instance already exists\n"));
        return VINF_SUCCESS;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture, (void **)&pDSoundStrmIn->pDSC);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: DirectSoundCapture create instance %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pDSoundStrmIn);
        hr = IDirectSoundCapture_Initialize(pDSoundStrmIn->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER)
            {
                DSLOGREL(("DSound: DirectSound capture is currently unavailable\n"));
            }
            else
            {
                DSLOGREL(("DSound: DirectSoundCapture initialize %Rhrc\n", hr));
            }
            dsoundCaptureInterfaceRelease(pDSoundStrmIn);
        }
    }

    return SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;
}

static void dsoundCaptureClose(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    DSLOG(("DSound: capture close %p buffer %p\n", pDSoundStrmIn, pDSoundStrmIn->pDSCB));

    if (pDSoundStrmIn->pDSCB)
    {
        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED (hr))
        {
            DSLOG(("DSound: close capture buffer stop %Rhrc\n", hr));
        }

        IDirectSoundCaptureBuffer_Release(pDSoundStrmIn->pDSCB);
        pDSoundStrmIn->pDSCB = NULL;
    }

    dsoundCaptureInterfaceRelease(pDSoundStrmIn);
}

static int dsoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    DSLOG(("DSound: capture open %p size %d bytes freq %d, chan %d, bits %d, sign %d\n",
           pDSoundStrmIn,
           pThis->cfg.cbBufferIn,
           pDSoundStrmIn->hw.Props.uHz,
           pDSoundStrmIn->hw.Props.cChannels,
           pDSoundStrmIn->hw.Props.cBits,
           pDSoundStrmIn->hw.Props.fSigned));

    if (pDSoundStrmIn->pDSCB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundCaptureBuffer already exists\n"));
        dsoundCaptureClose(pDSoundStrmIn);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStrmIn->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return rc;

    rc = dsoundCaptureInterfaceCreate(pThis, pDSoundStrmIn);
    if (RT_FAILURE(rc))
        return rc;

    HRESULT hr = S_OK;

    do /* To use breaks. */
    {
        DSCBUFFERDESC bd;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwBufferBytes = pThis->cfg.cbBufferIn;
        hr = IDirectSoundCapture_CreateCaptureBuffer(pDSoundStrmIn->pDSC,
                                                     &bd, &pDSoundStrmIn->pDSCB, NULL);

        if (FAILED(hr))
        {
            DSLOGREL(("DSound: create capture buffer %Rhrc\n", hr));
            pDSoundStrmIn->pDSCB = NULL;
            break;
        }

        /* Query the actual parameters. */

        DWORD cbReadPos = 0;
        hr = IDirectSoundCaptureBuffer_GetCurrentPosition(pDSoundStrmIn->pDSCB, NULL, &cbReadPos);
        if (FAILED(hr))
        {
            cbReadPos = 0;
            DSLOG(("DSound: open GetCurrentPosition %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer_GetFormat(pDSoundStrmIn->pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: capture buffer GetFormat %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer_GetCaps(pDSoundStrmIn->pDSCB, &bc);
        if (FAILED (hr))
        {
            DSLOGREL(("DSound: capture buffer GetCaps %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: capture buffer format: size %d bytes\n"
                "  tag             = %d\n"
                "  nChannels       = %d\n"
                "  nSamplesPerSec  = %d\n"
                "  nAvgBytesPerSec = %d\n"
                "  nBlockAlign     = %d\n"
                "  wBitsPerSample  = %d\n"
                "  cbSize          = %d\n",
                bc.dwBufferBytes,
                wfx.wFormatTag,
                wfx.nChannels,
                wfx.nSamplesPerSec,
                wfx.nAvgBytesPerSec,
                wfx.nBlockAlign,
                wfx.wBitsPerSample,
                wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmIn->hw.Props.uAlign)
        {
            DSLOGREL(("DSound: GetCaps returned misaligned buffer size %ld, alignment %d\n",
                      bc.dwBufferBytes, pDSoundStrmIn->hw.Props.uAlign + 1));
        }

        if (bc.dwBufferBytes != pThis->cfg.cbBufferIn)
        {
            DSLOGREL(("DSound: buffer size mismatch dsound %d, requested %d bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferIn));
        }

        /* Initial state: reading at the initial capture position. */
        pDSoundStrmIn->csCaptureReadPos = cbReadPos >> pDSoundStrmIn->hw.Props.cShift;
        pDSoundStrmIn->csCaptureBufferSize = bc.dwBufferBytes >> pDSoundStrmIn->hw.Props.cShift;
        DSLOG(("DSound: capture open csCaptureReadPos %d, csCaptureBufferSize %d samples\n",
               pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize));

        pDSoundStrmIn->hrLastCaptureIn = S_OK;
    } while (0);

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    dsoundCaptureClose(pDSoundStrmIn);
    return VERR_NOT_SUPPORTED;
}

static void dsoundCaptureStop(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    if (pDSoundStrmIn->pDSCB)
    {
        DSLOG(("DSound: capture stop\n"));

        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED(hr))
        {
            DSLOG(("DSound: stop capture buffer %Rhrc\n", hr));
        }
    }
}

static int dsoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    HRESULT hr;

    if (pDSoundStrmIn->pDSCB != NULL)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer_GetStatus(pDSoundStrmIn->pDSCB, &dwStatus);
        if (FAILED(hr))
        {
            DSLOG(("DSound: start GetStatus %Rhrc\n", hr));
        }
        else
        {
            if (dwStatus & DSCBSTATUS_CAPTURING)
            {
                DSLOG(("DSound: already capturing\n"));
            }
            else
            {
                DSLOG(("DSound: capture start\n"));

                hr = IDirectSoundCaptureBuffer_Start(pDSoundStrmIn->pDSCB, DSCBSTART_LOOPING);
                if (FAILED (hr))
                {
                    DSLOGREL(("DSound: start %Rhrc\n", hr));
                }
            }
        }
    }
    else
    {
        hr = E_FAIL;
    }

    return SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;
}

static int dsoundDevAdd(PRTLISTANCHOR pList, LPGUID lpGUID,
                        LPCWSTR lpwstrDescription, PDSOUNDDEV *ppDev)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(lpGUID, VERR_INVALID_POINTER);
    AssertPtrReturn(lpwstrDescription, VERR_INVALID_POINTER);

    PDSOUNDDEV pDev = (PDSOUNDDEV)RTMemAlloc(sizeof(DSOUNDDEV));
    if (!pDev)
        return VERR_NO_MEMORY;

    int rc = RTUtf16ToUtf8(lpwstrDescription, &pDev->pszName);
    if (RT_SUCCESS(rc))
        memcpy(&pDev->Guid, lpGUID, sizeof(GUID));

    if (RT_SUCCESS(rc))
        RTListAppend(pList, &pDev->Node);

    if (ppDev)
        *ppDev = pDev;

    return rc;
}

static void dsoundDevRemove(PDSOUNDDEV pDev)
{
    if (pDev)
    {
        RTStrFree(pDev->pszName);
        pDev->pszName = NULL;

        RTListNodeRemove(&pDev->Node);

        RTMemFree(pDev);
    }
}

static char *dsoundGUIDToUtf8StrA(LPCGUID lpGUID)
{
    LPOLESTR lpOLEStr;
    HRESULT hr = StringFromCLSID(*lpGUID, &lpOLEStr);
    if (SUCCEEDED(hr))
    {
        char *pszGUID;
        int rc = RTUtf16ToUtf8(lpOLEStr, &pszGUID);
        CoTaskMemFree(lpOLEStr);

        return RT_SUCCESS(rc) ? pszGUID : NULL;
    }

    return NULL;
}

static void dsoundLogDevice(const char *pszType, LPGUID lpGUID, LPCWSTR lpwstrDescription, LPCWSTR lpwstrModule)
{
    char *pszGUID = dsoundGUIDToUtf8StrA(lpGUID);

    DSLOG(("DSound: %s: GUID: %s [%ls] (Module: %ls)\n",
           pszType, pszGUID? pszGUID: "no GUID", lpwstrDescription, lpwstrModule));

    RTStrFree(pszGUID);
}

static BOOL CALLBACK dsoundEnumCallback(LPGUID lpGUID, LPCWSTR lpwstrDescription,
                                        LPCWSTR lpwstrModule, LPVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);
    AssertPtrReturn(pCtx->pCfg, FALSE);

    if (!lpGUID)
        return TRUE;

    dsoundLogDevice("Output", lpGUID, lpwstrDescription, lpwstrModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevOutput,
                          lpGUID, lpwstrDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->pCfg->cMaxHstStrmsOut++;

    return TRUE;
}

static BOOL CALLBACK dsoundCaptureEnumCallback(LPGUID lpGUID, LPCWSTR lpwstrDescription,
                                               LPCWSTR lpwstrModule, LPVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);
    AssertPtrReturn(pCtx->pCfg, FALSE);

    if (!lpGUID)
        return TRUE;

    dsoundLogDevice("Input", lpGUID, lpwstrDescription, lpwstrModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevInput,
                                 lpGUID, lpwstrDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->pCfg->cMaxHstStrmsIn++;

    return TRUE;
}


/*
 * PDMIHOSTAUDIO
 */

static DECLCALLBACK(int) drvHostDSoundInitOut(PPDMIHOSTAUDIO pInterface,
                                              PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                              uint32_t *pcSamples)
{
    LogFlowFunc(("pHstStrmOut=%p, pCfg=%p\n", pHstStrmOut, pCfg));

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    pDSoundStrmOut->streamCfg = *pCfg;
    pDSoundStrmOut->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

    int rc = drvAudioStreamCfgToProps(&pDSoundStrmOut->streamCfg, &pDSoundStrmOut->hw.Props);
    if (RT_SUCCESS(rc))
    {
        pDSoundStrmOut->pDS = NULL;
        pDSoundStrmOut->pDSB = NULL;
        pDSoundStrmOut->cbPlayWritePos = 0;
        pDSoundStrmOut->fReinitPlayPos = true;
        pDSoundStrmOut->csPlaybackBufferSize = 0;

        if (pcSamples)
            *pcSamples = pThis->cfg.cbBufferOut >> pHstStrmOut->Props.cShift;

        /* Try to open playback in case the device is already there. */
        dsoundPlayOpen(pThis, pDSoundStrmOut);
    }
    else
    {
        RT_ZERO(pDSoundStrmOut->streamCfg);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostDSoundControlOut(PPDMIHOSTAUDIO pInterface,
                                                 PPDMAUDIOHSTSTRMOUT pHstStrmOut, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    LogFlowFunc(("pHstStrmOut=%p, cmd=%d\n", pHstStrmOut, enmStreamCmd));

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    int rc = VINF_SUCCESS;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            /* Try to start playback. If it fails, then reopen and try again. */
            rc = dsoundPlayStart(pDSoundStrmOut);
            if (RT_FAILURE(rc))
            {
                dsoundPlayClose(pDSoundStrmOut);
                dsoundPlayOpen(pThis, pDSoundStrmOut);

                rc = dsoundPlayStart(pDSoundStrmOut);
            }
        } break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            dsoundPlayStop(pThis, pDSoundStrmOut);
        } break;

        default:
        {
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
        } break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static DECLCALLBACK(int) drvHostDSoundPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                              uint32_t *pcSamplesPlayed)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;
    LPDIRECTSOUNDBUFFER pDSB = pDSoundStrmOut->pDSB;

    int rc = VINF_SUCCESS;

    if (!pDSB)
    {
        if (pcSamplesPlayed) /** @todo single point of return */
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    int cShift = pHstStrmOut->Props.cShift;
    int cbBuffer = pDSoundStrmOut->csPlaybackBufferSize << cShift;

    DWORD cbPlayPos, cbWritePos;
    HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(pDSB, &cbPlayPos, &cbWritePos);
    if (hr == DSERR_BUFFERLOST)
    {
        rc = dsoundPlayRestore(pDSB);
        if (RT_FAILURE(rc))
        {
            if (pcSamplesPlayed)
                *pcSamplesPlayed = 0;
            return VINF_SUCCESS;
        }

        hr = IDirectSoundBuffer_GetCurrentPosition(pDSB, &cbPlayPos, &cbWritePos);
        if (hr == DSERR_BUFFERLOST)
        {
            /* Avoid log flooding if the error is still there. */
            if (pcSamplesPlayed)
                *pcSamplesPlayed = 0;
            return VINF_SUCCESS;
        }
    }
    if (FAILED (hr))
    {
        DSLOG(("DSound: get playback buffer position %Rhrc\n", hr));
        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    int cbFree;
    DWORD cbPlayWritePos;
    if (pDSoundStrmOut->fReinitPlayPos)
    {
        pDSoundStrmOut->fReinitPlayPos = false;

        pDSoundStrmOut->cbPlayWritePos = cbWritePos;

        cbPlayWritePos = pDSoundStrmOut->cbPlayWritePos;
        cbFree = cbBuffer - (int)dsoundRingDistance(cbWritePos, cbPlayPos, cbBuffer);
    }
    else
    {
        if (pDSoundStrmOut->cbPlayWritePos == cbPlayPos)
        {
            /* Full buffer. */
            if (pcSamplesPlayed)
                *pcSamplesPlayed = 0;
            return VINF_SUCCESS;
        }

        cbPlayWritePos = pDSoundStrmOut->cbPlayWritePos;
        cbFree = (int)dsoundRingDistance(cbPlayPos, cbPlayWritePos, cbBuffer);
    }

    uint32_t csLive = drvAudioHstOutSamplesLive(pHstStrmOut, NULL /* pcStreamsLive */);
    int cbLive = csLive << cShift;

    /* Do not write more than available space in the DirectSound playback buffer. */
    cbLive = RT_MIN(cbFree, cbLive);

    cbLive &= ~pHstStrmOut->Props.uAlign;
    if (cbLive <= 0 || cbLive > cbBuffer)
    {
        DSLOG(("DSound: cbLive=%d cbBuffer=%d cbPlayWritePos=%d cbPlayPos=%d\n",
              cbLive, cbBuffer, cbPlayWritePos, cbPlayPos));
        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    rc = dsoundLockOutput(pDSB, &pHstStrmOut->Props, cbPlayWritePos, cbLive,
                          &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
    if (RT_FAILURE(rc))
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = 0;
        return VINF_SUCCESS;
    }

    DWORD len1 = cb1 >> cShift;
    DWORD len2 = cb2 >> cShift;

    uint32_t cReadTotal = 0;
    uint32_t cRead = 0;

    if (pv1 && cb1)
    {
        rc = audioMixBufReadCirc(&pHstStrmOut->MixBuf, pv1, cb1, &cRead);
        if (RT_SUCCESS(rc))
            cReadTotal += cRead;
    }

    if (   RT_SUCCESS(rc)
        && cReadTotal == len1
        && pv2 && cb2)
    {
        rc = audioMixBufReadCirc(&pHstStrmOut->MixBuf, pv2, cb2, &cRead);
        if (RT_SUCCESS(rc))
            cReadTotal += cRead;
    }

    dsoundUnlockOutput(pDSB, pv1, pv2, cb1, cb2);

    pDSoundStrmOut->cbPlayWritePos = (cbPlayWritePos + (cReadTotal << cShift)) % cbBuffer;

    LogFlow(("DSound: PlayOut %RU32 (%RU32 samples) out of %d%s, ds write pos %d -> %d, rc=%Rrc\n",
             AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal), cReadTotal, cbLive,
             cbLive != AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal) ? " !!!": "",
             cbPlayWritePos, pDSoundStrmOut->cbPlayWritePos, rc));

    if (cReadTotal)
    {
        audioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);
        rc = VINF_SUCCESS; /* Played something. */
    }

    if (pcSamplesPlayed)
        *pcSamplesPlayed = cReadTotal;

    return rc;
}

static DECLCALLBACK(int) drvHostDSoundFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    dsoundPlayClose(pDSoundStrmOut);

    pDSoundStrmOut->cbPlayWritePos = 0;
    pDSoundStrmOut->fReinitPlayPos = true;
    pDSoundStrmOut->csPlaybackBufferSize = 0;
    RT_ZERO(pDSoundStrmOut->streamCfg);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostDSoundInitIn(PPDMIHOSTAUDIO pInterface,
                                             PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                             PDMAUDIORECSOURCE enmRecSource,
                                             uint32_t *pcSamples)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMIN pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;

    LogFlowFunc(("pHstStrmIn=%p, pAudioSettings=%p, enmRecSource=%ld\n",
                 pHstStrmIn, pCfg, enmRecSource));

    pDSoundStrmIn->streamCfg = *pCfg;
    pDSoundStrmIn->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANESS;

    /** @todo caller should already init Props? */
    int rc = drvAudioStreamCfgToProps(&pDSoundStrmIn->streamCfg, &pHstStrmIn->Props);
    if (RT_SUCCESS(rc))
    {
        /* Init the stream structure and save relevant information to it. */
        pDSoundStrmIn->csCaptureReadPos = 0;
        pDSoundStrmIn->csCaptureBufferSize = 0;
        pDSoundStrmIn->pDSC = NULL;
        pDSoundStrmIn->pDSCB = NULL;
        pDSoundStrmIn->enmRecSource = enmRecSource;
        pDSoundStrmIn->hrLastCaptureIn = S_OK;

        if (pcSamples)
            *pcSamples = pThis->cfg.cbBufferIn >> pHstStrmIn->Props.cShift;

        /* Try to open capture in case the device is already there. */
        dsoundCaptureOpen(pThis, pDSoundStrmIn);
    }
    else
    {
        RT_ZERO(pDSoundStrmIn->streamCfg);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostDSoundControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    LogFlowFunc(("pHstStrmIn=%p, enmStreamCmd=%ld\n", pHstStrmIn, enmStreamCmd));

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMIN pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            /* Try to start capture. If it fails, then reopen and try again. */
            rc = dsoundCaptureStart(pThis, pDSoundStrmIn);
            if (RT_FAILURE(rc))
            {
                dsoundCaptureClose(pDSoundStrmIn);
                dsoundCaptureOpen(pThis, pDSoundStrmIn);

                rc = dsoundCaptureStart(pThis, pDSoundStrmIn);
            }
        } break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            dsoundCaptureStop(pDSoundStrmIn);
        } break;

        default:
        {
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
        } break;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostDSoundCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                uint32_t *pcSamplesCaptured)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMIN pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;
    LPDIRECTSOUNDCAPTUREBUFFER pDSCB = pDSoundStrmIn->pDSCB;

    int rc;

    if (pDSCB == NULL)
    {
        if (pcSamplesCaptured) /** @todo single point of return */
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    /* Get DirectSound capture position in bytes. */
    DWORD cbReadPos;
    HRESULT hr = IDirectSoundCaptureBuffer_GetCurrentPosition(pDSCB, NULL, &cbReadPos);
    if (FAILED(hr))
    {
        if (hr != pDSoundStrmIn->hrLastCaptureIn)
        {
            DSLOGREL(("DSound: CaptureIn GetCurrentPosition %Rhrc\n", hr));
            pDSoundStrmIn->hrLastCaptureIn = hr;
        }

        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }
    pDSoundStrmIn->hrLastCaptureIn = hr;

    if (cbReadPos & pHstStrmIn->Props.uAlign)
    {
        DSLOG(("DSound: CaptureIn misaligned read position %d(%d)\n", cbReadPos, pHstStrmIn->Props.uAlign));
    }

    /* Capture position in samples. */
    int csReadPos = cbReadPos >> pHstStrmIn->Props.cShift;

    /* Number of samples available in the DirectSound capture buffer. */
    DWORD csCaptured = dsoundRingDistance(csReadPos, pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize);
    if (csCaptured == 0)
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    /* Using as an intermediate not circular buffer. */
    audioMixBufReset(&pHstStrmIn->MixBuf);

    /* Get number of free samples in the mix buffer and check that is has free space */
    size_t csMixFree = audioMixBufFree(&pHstStrmIn->MixBuf);
    if (csMixFree == 0)
    {
        DSLOG(("DSound: capture mix buffer full\n"));
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    LogFlow(("DSound: CaptureIn csMixFree = %d, csReadPos = %d, csCaptureReadPos = %d, csCaptured = %d\n",
             csMixFree, csReadPos, pDSoundStrmIn->csCaptureReadPos, csCaptured));

    /* No need to fetch more samples than mix buffer can receive. */
    csCaptured = RT_MIN(csCaptured, csMixFree);

    /* Lock relevant range in the DirectSound capture buffer. */
    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    rc = dsoundLockInput(pDSCB, &pHstStrmIn->Props,
                         pDSoundStrmIn->csCaptureReadPos << pHstStrmIn->Props.cShift,
                         csCaptured << pHstStrmIn->Props.cShift,
                         &pv1, &pv2, &cb1, &cb2,
                         0 /* dwFlags */);
    if (RT_FAILURE(rc))
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    DWORD len1 = cb1 >> pHstStrmIn->Props.cShift;
    DWORD len2 = cb2 >> pHstStrmIn->Props.cShift;

    uint32_t csWrittenTotal = 0;
    uint32_t csWritten;
    if (pv1 && len1)
    {
        rc = audioMixBufWriteAt(&pHstStrmIn->MixBuf, 0 /* offWrite */,
                                pv1, cb1, &csWritten);
        if (RT_SUCCESS(rc))
            csWrittenTotal += csWritten;
    }

    if (   RT_SUCCESS(rc)
        && csWrittenTotal == len1
        && pv2 && len2)
    {
        rc = audioMixBufWriteAt(&pHstStrmIn->MixBuf, csWrittenTotal,
                                pv2, cb2, &csWritten);
        if (RT_SUCCESS(rc))
            csWrittenTotal += csWritten;
    }

    dsoundUnlockInput(pDSCB, pv1, pv2, cb1, cb2);

    uint32_t csProcessed = 0;
    if (csWrittenTotal != 0)
    {
        /* Captured something. */
        rc = audioMixBufMixToParent(&pHstStrmIn->MixBuf, csWrittenTotal,
                                    &csProcessed);
    }

    if (RT_SUCCESS(rc))
    {
        pDSoundStrmIn->csCaptureReadPos = (pDSoundStrmIn->csCaptureReadPos + csProcessed) % pDSoundStrmIn->csCaptureBufferSize;
        LogFlow(("DSound: CaptureIn %d (%d+%d), processed %d/%d\n",
                 csCaptured, len1, len2, csProcessed, csWrittenTotal));
    }

    if (pcSamplesCaptured)
        *pcSamplesCaptured = csProcessed;

    return rc;
}

static DECLCALLBACK(int) drvHostDSoundFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMIN pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;

    dsoundCaptureClose(pDSoundStrmIn);

    pDSoundStrmIn->csCaptureReadPos = 0;
    pDSoundStrmIn->csCaptureBufferSize = 0;
    RT_ZERO(pDSoundStrmIn->streamCfg);

    return VINF_SUCCESS;
}

static DECLCALLBACK(bool) drvHostDSoundIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostDSoundGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    dsoundFreeDeviceLists(pThis);

    pCfg->cbStreamOut = sizeof(DSOUNDSTREAMOUT);
    pCfg->cbStreamIn  = sizeof(DSOUNDSTREAMIN);

    pCfg->cMaxHstStrmsOut = 0;
    pCfg->cMaxHstStrmsIn  = 0;

    DSOUNDENUMCBCTX ctx = { pThis, pCfg };

    HRESULT hr = DirectSoundEnumerateW(&dsoundEnumCallback, &ctx);
    if (FAILED(hr))
        DSLOG(("DSound: Enumerating host playback devices failed %Rhrc\n", hr));

    DSLOGREL(("DSound: Found %RU32 host playback devices\n", pCfg->cMaxHstStrmsOut));
    if (pCfg->cMaxHstStrmsOut == 0)
        pCfg->cMaxHstStrmsOut = 1; /* Support at least one stream. */

    hr = DirectSoundCaptureEnumerateW(&dsoundCaptureEnumCallback, &ctx);
    if (FAILED(hr))
        DSLOG(("DSound: Enumerating host capturing devices failed %Rhrc\n", hr));

    DSLOGREL(("DSound: Found %RU32 host capturing devices\n", pCfg->cMaxHstStrmsIn));
    if (pCfg->cMaxHstStrmsIn == 0)
        pCfg->cMaxHstStrmsIn = 1; /* Support at least one stream. */

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) drvHostDSoundShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

static DECLCALLBACK(int) drvHostDSoundInit(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    LogFlowFuncEnter();

    /* Verify that IDirectSound is available. */
    LPDIRECTSOUND pDirectSound = NULL;
    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_ALL,
                                  IID_IDirectSound, (void **)&pDirectSound);
    if (SUCCEEDED(hr))
        IDirectSound_Release(pDirectSound);
    else
        DSLOGREL(("DSound: not available %Rhrc\n", hr));

    int rc = SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void *) drvHostDSoundQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS     pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTDSOUND pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}

static int dsoundConfigQueryStringAlloc(PCFGMNODE pNode, const char *pszName, char **ppszString)
{
    size_t cbString;
    int rc = CFGMR3QuerySize(pNode, pszName, &cbString);
    if (RT_SUCCESS(rc))
    {
        char *pszString = RTStrAlloc(cbString);
        if (pszString)
        {
            rc = CFGMR3QueryString(pNode, pszName, pszString, cbString);
            if (RT_SUCCESS(rc))
                *ppszString = pszString;
            else
                RTStrFree(pszString);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}

static LPCGUID dsoundConfigQueryGUID(PCFGMNODE pCfg, const char *pszName, RTUUID *pUuid)
{
    LPCGUID pGuid = NULL;

    char *pszGuid = NULL;
    dsoundConfigQueryStringAlloc(pCfg, pszName, &pszGuid);
    if (pszGuid)
    {
        int rc = RTUuidFromStr(pUuid, pszGuid);
        if (RT_SUCCESS(rc))
            pGuid = (LPCGUID)&pUuid;
        else
            DSLOGREL(("DSound: Parse DirectSound %s %Rrc\n", pszName, rc));

        RTStrFree(pszGuid);
    }

    return pGuid;
}

static void dSoundConfigInit(PDRVHOSTDSOUND pThis, PCFGMNODE pCfg)
{
    CFGMR3QuerySIntDef(pCfg, "BufsizeOut",       &pThis->cfg.cbBufferOut,       _16K);
    CFGMR3QuerySIntDef(pCfg, "BufsizeIn",        &pThis->cfg.cbBufferIn,        _16K);

    pThis->cfg.pGuidPlay    = dsoundConfigQueryGUID(pCfg, "DeviceGuidOut", &pThis->cfg.uuidPlay);
    pThis->cfg.pGuidCapture = dsoundConfigQueryGUID(pCfg, "DeviceGuidIn",  &pThis->cfg.uuidCapture);
}

static DECLCALLBACK(void) drvHostDSoundDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTDSOUND pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);

    LogFlowFuncEnter();

    CoUninitialize();

    LogFlowFuncLeave();
}

/**
 * Construct a DirectSound Audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostDSoundConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTDSOUND pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);
    DSLOGREL(("Audio: Initializing DirectSound audio driver\n"));

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: COM initialize %Rhrc\n", hr));
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                    = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface  = drvHostDSoundQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostDSound);

    RTListInit(&pThis->lstDevInput);
    RTListInit(&pThis->lstDevOutput);

    /*
     * Initialize configuration values.
     */
    dSoundConfigInit(pThis, pCfg);

    return VINF_SUCCESS;
}

/**
 * PDM driver registration.
 */
const PDMDRVREG g_DrvHostDSound =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "DSoundAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "DirectSound Audio host driver",
    /* fFlags */
     PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTDSOUND),
    /* pfnConstruct */
    drvHostDSoundConstruct,
    /* pfnDestruct */
    drvHostDSoundDestruct,
    /* pfnRelocate */
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
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
