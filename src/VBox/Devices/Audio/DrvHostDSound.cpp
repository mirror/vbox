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
 */
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <dsound.h>

#include <iprt/alloc.h>
#include <iprt/uuid.h>

#include "AudioMixBuffer.h"
#include "DrvAudio.h"
#include "VBoxDD.h"

/*
 * IDirectSound* interface uses HRESULT status codes and the driver callbacks use
 * the IPRT status codes. To minimize HRESULT->IPRT conversion most internal functions
 * in the driver return HRESULT and conversion is done in the driver callbacks.
 *
 * Naming convention:
 * 'dsound*' functions return IPRT status code;
 * 'directSound*' - return HRESULT.
 */

/*
 * Optional release logging, which a user can turn on with the
 * 'VBoxManage debugvm' command.
 * Debug logging still uses the common Log* macros from IPRT.
 * Messages which always should go to the release log use LogRel.
 */
/* General code behavior. */
#define DSLOG(a) do { LogRel2(a); } while(0)
/* Something which produce a lot of logging during playback/recording. */
#define DSLOGF(a) do { LogRel3(a); } while(0)
/* Important messages like errors. Limited in the default release log to avoid log flood. */
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

/* Dynamically load dsound.dll. */
typedef HRESULT WINAPI FNDIRECTSOUNDENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, LPVOID pContext);
typedef FNDIRECTSOUNDENUMERATEW *PFNDIRECTSOUNDENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTUREENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, LPVOID pContext);
typedef FNDIRECTSOUNDCAPTUREENUMERATEW *PFNDIRECTSOUNDCAPTUREENUMERATEW;

typedef struct DSOUNDHOSTCFG
{
    DWORD   cbBufferIn;
    DWORD   cbBufferOut;
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
    /** DirectSound configuration options. */
    DSOUNDHOSTCFG cfg;
} DRVHOSTDSOUND, *PDRVHOSTDSOUND;

typedef struct DSOUNDSTREAMOUT
{
    PDMAUDIOHSTSTRMOUT   strmOut; /* Always must come first! */
    LPDIRECTSOUND8       pDS;
    LPDIRECTSOUNDBUFFER8 pDSB;
    DWORD                cbPlayWritePos;
    DWORD                csPlaybackBufferSize;
    bool                 fRestartPlayback;
    PDMAUDIOSTREAMCFG    streamCfg;
} DSOUNDSTREAMOUT, *PDSOUNDSTREAMOUT;

typedef struct DSOUNDSTREAMIN
{
    PDMAUDIOHSTSTRMIN           strmIn; /* Always must come first! */
    LPDIRECTSOUNDCAPTURE8       pDSC;
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB;
    DWORD                       csCaptureReadPos;
    DWORD                       csCaptureBufferSize;
    HRESULT                     hrLastCaptureIn;
    PDMAUDIORECSOURCE           enmRecSource;
    PDMAUDIOSTREAMCFG           streamCfg;
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

static DWORD dsoundRingDistance(DWORD offEnd, DWORD offBegin, DWORD cSize)
{
    return offEnd >= offBegin ? offEnd - offBegin : cSize - offBegin + offEnd;
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

static char *dsoundGUIDToUtf8StrA(LPCGUID lpGUID)
{
    if (lpGUID)
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
    }

    return RTStrDup("{Default device}");
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

static HRESULT directSoundPlayRestore(LPDIRECTSOUNDBUFFER8 pDSB)
{
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (FAILED(hr))
        DSLOGREL(("DSound: Restore playback buffer %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayUnlock(LPDIRECTSOUNDBUFFER8 pDSB,
                                     LPVOID pv1, LPVOID pv2,
                                     DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundBuffer8_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlock playback buffer %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureUnlock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB,
                                        LPVOID pv1, LPVOID pv2,
                                        DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer8_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlock capture buffer %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayLock(LPDIRECTSOUNDBUFFER8 pDSB, PDMPCMPROPS *pProps,
                                   DWORD dwOffset, DWORD dwBytes,
                                   LPVOID *ppv1, LPVOID *ppv2,
                                   DWORD *pcb1, DWORD *pcb2,
                                   DWORD dwFlags)
{
    LPVOID pv1 = NULL;
    LPVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (hr == DSERR_BUFFERLOST)
    {
        hr = directSoundPlayRestore(pDSB);
        if (SUCCEEDED(hr))
        {
            hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
        }
    }

    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Lock playback buffer %Rhrc\n", hr));
        return hr;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        DSLOGREL(("DSound: Locking playback buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                  cb1, cb2, pProps->uAlign));
        directSoundPlayUnlock(pDSB, pv1, pv2, cb1, cb2);
        return E_FAIL;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return S_OK;
}

static HRESULT directSoundCaptureLock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB, PPDMPCMPROPS pProps,
                                      DWORD dwOffset, DWORD dwBytes,
                                      LPVOID *ppv1, LPVOID *ppv2,
                                      DWORD *pcb1, DWORD *pcb2,
                                      DWORD dwFlags)
{
    LPVOID pv1 = NULL;
    LPVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundCaptureBuffer8_Lock(pDSCB, dwOffset, dwBytes,
                                                 &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Lock capture buffer %Rhrc\n", hr));
        return hr;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        DSLOGREL(("DSound: Locking capture buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                  cb1, cb2, pProps->uAlign));
        directSoundCaptureUnlock(pDSCB, pv1, pv2, cb1, cb2);
        return E_FAIL;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return S_OK;
}


/*
 * DirectSound playback
 */

static void directSoundPlayInterfaceRelease(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDS)
    {
        IDirectSound8_Release(pDSoundStrmOut->pDS);
        pDSoundStrmOut->pDS = NULL;
    }
}

static HRESULT directSoundPlayInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDS != NULL)
    {
        DSLOG(("DSound: DirectSound instance already exists\n"));
        return S_OK;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_ALL,
                                  IID_IDirectSound8, (void **)&pDSoundStrmOut->pDS);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Create DirectSound instance %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pDSoundStrmOut->pDS, pThis->cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pDSoundStrmOut->pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                DSLOGREL(("DSound: Set cooperative level for window %p %Rhrc\n", hWnd, hr));
        }

        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no playback devices are attached. */
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSound playback initialize %Rhrc\n", hr));

            directSoundPlayInterfaceRelease(pDSoundStrmOut);
        }
    }

    return hr;
}

static void directSoundPlayClose(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    DSLOG(("DSound: Closing playback stream %p, buffer %p\n", pDSoundStrmOut, pDSoundStrmOut->pDSB));

    if (pDSoundStrmOut->pDSB)
    {
        HRESULT hr = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr))
            DSLOGREL(("DSound: Stop playback stream %p when closing %Rhrc\n", pDSoundStrmOut, hr));

        IDirectSoundBuffer8_Release(pDSoundStrmOut->pDSB);
        pDSoundStrmOut->pDSB = NULL;
    }

    directSoundPlayInterfaceRelease(pDSoundStrmOut);
}

static HRESULT directSoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStrmOut, E_POINTER);

    DSLOG(("DSound: pDSoundStrmOut=%p, cbBufferOut=%ld, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
           pDSoundStrmOut,
           pThis->cfg.cbBufferOut,
           pDSoundStrmOut->strmOut.Props.uHz,
           pDSoundStrmOut->strmOut.Props.cChannels,
           pDSoundStrmOut->strmOut.Props.cBits,
           pDSoundStrmOut->strmOut.Props.fSigned));

    if (pDSoundStrmOut->pDSB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundBuffer already exists\n"));
        directSoundPlayClose(pDSoundStrmOut);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStrmOut->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    HRESULT hr = directSoundPlayInterfaceCreate(pThis, pDSoundStrmOut);
    if (FAILED(hr))
        return hr;

    do /* To use breaks. */
    {
        LPDIRECTSOUNDBUFFER pDSB = NULL;
        DSBUFFERDESC bd;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        bd.dwBufferBytes = pThis->cfg.cbBufferOut;
        hr = IDirectSound8_CreateSoundBuffer(pDSoundStrmOut->pDS,
                                             &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: CreateSoundBuffer %Rhrc\n", hr));
            break;
        }

        /* "Upgrade" to IDirectSoundBuffer8 intarface. */
        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (void **)&pDSoundStrmOut->pDSB);
        pDSB->Release();
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Query IDirectSoundBuffer8 %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        hr = IDirectSoundBuffer8_GetFormat(pDSoundStrmOut->pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Playback GetFormat %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundBuffer8_GetCaps(pDSoundStrmOut->pDSB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Playback GetCaps %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Playback format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               wfx.wFormatTag,
               wfx.nChannels,
               wfx.nSamplesPerSec,
               wfx.nAvgBytesPerSec,
               wfx.nBlockAlign,
               wfx.wBitsPerSample,
               wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmOut->strmOut.Props.uAlign)
            DSLOGREL(("DSound: Playback GetCaps returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pDSoundStrmOut->strmOut.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferOut)
            DSLOGREL(("DSound: Playback buffer size mismatched: DirectSound %RU32, requested %RU32 bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferOut));

        /*
         * Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pDSoundStrmOut->csPlaybackBufferSize = bc.dwBufferBytes >> pDSoundStrmOut->strmOut.Props.cShift;
        DSLOG(("DSound: csPlaybackBufferSize=%RU32\n", pDSoundStrmOut->csPlaybackBufferSize));

    } while (0);

    if (FAILED(hr))
        directSoundPlayClose(pDSoundStrmOut);

    return hr;
}

static void dsoundPlayClearSamples(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturnVoid(pDSoundStrmOut);

    PPDMAUDIOHSTSTRMOUT pStrmOut = &pDSoundStrmOut->strmOut;

    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    HRESULT hr = directSoundPlayLock(pDSoundStrmOut->pDSB, &pDSoundStrmOut->strmOut.Props,
                                     0 /* dwOffset */, AUDIOMIXBUF_S2B(&pStrmOut->MixBuf, pDSoundStrmOut->csPlaybackBufferSize),
                                     &pv1, &pv2, &cb1, &cb2, DSBLOCK_ENTIREBUFFER);
    if (SUCCEEDED(hr))
    {
        DWORD len1 = AUDIOMIXBUF_B2S(&pStrmOut->MixBuf, cb1);
        DWORD len2 = AUDIOMIXBUF_B2S(&pStrmOut->MixBuf, cb2);

        if (pv1 && len1)
            DrvAudioClearBuf(&pDSoundStrmOut->strmOut.Props, pv1, cb1, len1);

        if (pv2 && len2)
            DrvAudioClearBuf(&pDSoundStrmOut->strmOut.Props, pv2, cb2, len2);

        directSoundPlayUnlock(pDSoundStrmOut->pDSB, pv1, pv2, cb1, cb2);
    }
}

static HRESULT directSoundPlayGetStatus(LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pdwStatus)
{
    AssertPtrReturn(pDSB, E_POINTER);
    /* pdwStatus is optional. */

    DWORD dwStatus = 0;
    HRESULT hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
    if (SUCCEEDED(hr))
    {
        if ((dwStatus & DSBSTATUS_BUFFERLOST) != 0)
        {
            hr = directSoundPlayRestore(pDSB);
            if (SUCCEEDED(hr))
                hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
        }
    }

    if (SUCCEEDED(hr))
    {
        if (pdwStatus)
            *pdwStatus = dwStatus;
    }
    else
        DSLOGREL(("DSound: Playback GetStatus %Rhrc\n", hr));

    return hr;
}

static void directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pDSoundStrmOut);

    if (pDSoundStrmOut->pDSB != NULL)
    {
        /* This performs some restore, so call it anyway and ignore result. */
        directSoundPlayGetStatus(pDSoundStrmOut->pDSB, NULL /* Status */);

        DSLOG(("DSound: Stopping playback\n"));

        /* @todo Wait until all data in the buffer has been played. */
        HRESULT hr = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (SUCCEEDED(hr))
            dsoundPlayClearSamples(pDSoundStrmOut);
        else
            DSLOGREL(("DSound: Stop playback %Rhrc\n", hr));
    }
}

static HRESULT directSoundPlayStart(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pDSoundStrmOut, E_POINTER);

    HRESULT hr;
    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        hr = directSoundPlayGetStatus(pDSoundStrmOut->pDSB, &dwStatus);
        if (SUCCEEDED(hr))
        {
            if (dwStatus & DSBSTATUS_PLAYING)
            {
                DSLOG(("DSound: Already playing\n"));
            }
            else
            {
                dsoundPlayClearSamples(pDSoundStrmOut);

                pDSoundStrmOut->fRestartPlayback = true;

                DSLOG(("DSound: Playback start\n"));

                /* The actual IDirectSoundBuffer8_Play call will be made in drvHostDSoundPlayOut,
                 * because it is necessary to put some samples into the buffer first.
                 */
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    return hr;
}

/*
 * DirectSoundCapture
 */

static LPCGUID dsoundCaptureSelectDevice(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis, NULL);
    AssertPtrReturn(pDSoundStrmIn, NULL);

    LPCGUID pGUID = pThis->cfg.pGuidCapture;

    if (!pGUID)
    {
        PDSOUNDDEV  pDev = NULL;

        switch (pDSoundStrmIn->enmRecSource)
        {
            case PDMAUDIORECSOURCE_MIC:
            {
                RTListForEach(&pThis->lstDevInput, pDev, DSOUNDDEV, Node)
                {
                    if (RTStrIStr(pDev->pszName, "Mic")) /** @todo what is with non en_us windows versions? */
                        break;
                }
                if (RTListNodeIsDummy(&pThis->lstDevInput, pDev, DSOUNDDEV, Node))
                    pDev = NULL;    /* Found nothing. */

                break;
            }

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

    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);
    /* This always has to be in the release log. */
    LogRel(("DSound: Guest \"%s\" is using host device with GUID: %s\n",
            drvAudioRecSourceToString(pDSoundStrmIn->enmRecSource), pszGUID? pszGUID: "{?}"));
    RTStrFree(pszGUID);

    return pGUID;
}

static void directSoundCaptureInterfaceRelease(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    if (pDSoundStrmIn->pDSC)
    {
        IDirectSoundCapture_Release(pDSoundStrmIn->pDSC);
        pDSoundStrmIn->pDSC = NULL;
    }
}

static HRESULT directSoundCaptureInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    if (pDSoundStrmIn->pDSC != NULL)
    {
        DSLOG(("DSound: DirectSoundCapture instance already exists\n"));
        return S_OK;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture8, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture8, (void **)&pDSoundStrmIn->pDSC);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: DirectSoundCapture create %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pDSoundStrmIn);
        hr = IDirectSoundCapture_Initialize(pDSoundStrmIn->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no capture devices are attached. */
                DSLOGREL(("DSound: DirectSound capture is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSoundCapture initialize %Rhrc\n", hr));

            directSoundCaptureInterfaceRelease(pDSoundStrmIn);
        }
    }

    return hr;
}

static void directSoundCaptureClose(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturnVoid(pDSoundStrmIn);

    DSLOG(("DSound: pDSoundStrmIn=%p, pDSCB=%p\n", pDSoundStrmIn, pDSoundStrmIn->pDSCB));

    if (pDSoundStrmIn->pDSCB)
    {
        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED(hr))
            DSLOGREL(("DSound: Stop capture buffer %Rhrc\n", hr));

        IDirectSoundCaptureBuffer8_Release(pDSoundStrmIn->pDSCB);
        pDSoundStrmIn->pDSCB = NULL;
    }

    directSoundCaptureInterfaceRelease(pDSoundStrmIn);
}

static HRESULT directSoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStrmIn, E_POINTER);

    DSLOG(("DSound: pDSoundStrmIn=%p, cbBufferIn=%ld, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
           pDSoundStrmIn,
           pThis->cfg.cbBufferIn,
           pDSoundStrmIn->strmIn.Props.uHz,
           pDSoundStrmIn->strmIn.Props.cChannels,
           pDSoundStrmIn->strmIn.Props.cBits,
           pDSoundStrmIn->strmIn.Props.fSigned));

    if (pDSoundStrmIn->pDSCB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundCaptureBuffer already exists\n"));
        directSoundCaptureClose(pDSoundStrmIn);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStrmIn->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    HRESULT hr = directSoundCaptureInterfaceCreate(pThis, pDSoundStrmIn);
    if (FAILED(hr))
        return hr;

    do /* To use breaks. */
    {
        LPDIRECTSOUNDCAPTUREBUFFER pDSCB = NULL;
        DSCBUFFERDESC bd;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwBufferBytes = pThis->cfg.cbBufferIn;
        hr = IDirectSoundCapture_CreateCaptureBuffer(pDSoundStrmIn->pDSC,
                                                     &bd, &pDSCB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: CreateCaptureBuffer %Rhrc\n", hr));
            break;
        }

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pDSoundStrmIn->pDSCB);
        IDirectSoundCaptureBuffer_Release(pDSCB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Query IDirectSoundCaptureBuffer8 %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        DWORD cbReadPos = 0;
        hr = IDirectSoundCaptureBuffer8_GetCurrentPosition(pDSoundStrmIn->pDSCB, NULL, &cbReadPos);
        if (FAILED(hr))
        {
            cbReadPos = 0;
            DSLOGREL(("DSound: Capture (open) GetCurrentPosition %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pDSoundStrmIn->pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Capture GetFormat %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pDSoundStrmIn->pDSCB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Capture GetCaps %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Capture format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               wfx.wFormatTag,
               wfx.nChannels,
               wfx.nSamplesPerSec,
               wfx.nAvgBytesPerSec,
               wfx.nBlockAlign,
               wfx.wBitsPerSample,
               wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmIn->strmIn.Props.uAlign)
            DSLOGREL(("DSound: Capture GetCaps returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pDSoundStrmIn->strmIn.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferIn)
            DSLOGREL(("DSound: Capture buffer size mismatched: DirectSound %RU32, requested %RU32 bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferIn));

        /* Initial state: reading at the initial capture position, no error. */
        pDSoundStrmIn->csCaptureReadPos    = cbReadPos >> pDSoundStrmIn->strmIn.Props.cShift;
        pDSoundStrmIn->csCaptureBufferSize = bc.dwBufferBytes >> pDSoundStrmIn->strmIn.Props.cShift;
        pDSoundStrmIn->hrLastCaptureIn = S_OK;

        DSLOG(("DSound: csCaptureReadPos=%RU32, csCaptureBufferSize=%RU32\n",
                     pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize));

    } while (0);

    if (FAILED(hr))
        directSoundCaptureClose(pDSoundStrmIn);

    return hr;
}

static void directSoundCaptureStop(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturnVoid(pDSoundStrmIn);

    if (pDSoundStrmIn->pDSCB)
    {
        DSLOG(("DSound: Stopping capture\n"));

        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED(hr))
            DSLOGREL(("DSound: Capture buffer stop %Rhrc\n", hr));
    }
}

static HRESULT directSoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pDSoundStrmIn, VERR_INVALID_POINTER);

    HRESULT hr;
    if (pDSoundStrmIn->pDSCB != NULL)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer8_GetStatus(pDSoundStrmIn->pDSCB, &dwStatus);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Capture start GetStatus %Rhrc\n", hr));
        }
        else
        {
            if (dwStatus & DSCBSTATUS_CAPTURING)
            {
                DSLOG(("DSound: Already capturing\n"));
            }
            else
            {
                DSLOG(("DSound: Capture start\n"));
                hr = IDirectSoundCaptureBuffer8_Start(pDSoundStrmIn->pDSCB, DSCBSTART_LOOPING);
                if (FAILED(hr))
                    DSLOGREL(("DSound: Capture started %Rhrc\n", hr));
            }
        }
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    return hr;
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

static void dsoundLogDevice(const char *pszType, LPGUID lpGUID, LPCWSTR lpwstrDescription, LPCWSTR lpwstrModule)
{
    char *pszGUID = dsoundGUIDToUtf8StrA(lpGUID);
    /* This always has to be in the release log. */
    LogRel(("DSound: %s: GUID: %s [%ls] (Module: %ls)\n",
            pszType, pszGUID? pszGUID: "{?}", lpwstrDescription, lpwstrModule));
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

    AssertPtrReturn(lpwstrDescription, FALSE);
    /* Do not care about lpwstrModule */

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
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    /* pcSamples is optional. */

    LogFlowFunc(("pHstStrmOut=%p, pCfg=%p\n", pHstStrmOut, pCfg));

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    pDSoundStrmOut->streamCfg = *pCfg;
    pDSoundStrmOut->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

    int rc = DrvAudioStreamCfgToProps(&pDSoundStrmOut->streamCfg, &pDSoundStrmOut->strmOut.Props);
    if (RT_SUCCESS(rc))
    {
        pDSoundStrmOut->pDS = NULL;
        pDSoundStrmOut->pDSB = NULL;
        pDSoundStrmOut->cbPlayWritePos = 0;
        pDSoundStrmOut->fRestartPlayback = true;
        pDSoundStrmOut->csPlaybackBufferSize = 0;

        if (pcSamples)
            *pcSamples = pThis->cfg.cbBufferOut >> pHstStrmOut->Props.cShift;

        /* Try to open playback in case the device is already there. */
        directSoundPlayOpen(pThis, pDSoundStrmOut);
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
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_ENABLE\n"));
            /* Try to start playback. If it fails, then reopen and try again. */
            HRESULT hr = directSoundPlayStart(pDSoundStrmOut);
            if (FAILED(hr))
            {
                directSoundPlayClose(pDSoundStrmOut);
                directSoundPlayOpen(pThis, pDSoundStrmOut);

                hr = directSoundPlayStart(pDSoundStrmOut);
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_DISABLE\n"));
            directSoundPlayStop(pThis, pDSoundStrmOut);
            break;
        }

        default:
        {
            AssertMsgFailed(("Invalid command: %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostDSoundPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                              uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    int rc = VINF_SUCCESS;
    uint32_t cReadTotal = 0;

    do /* to use 'break' */
    {
        LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStrmOut->pDSB;
        if (!pDSB)
            break;

        int cShift = pHstStrmOut->Props.cShift;
        DWORD cbBuffer = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, pDSoundStrmOut->csPlaybackBufferSize);

        /* Get the current play position which is used for calculating the free space in the buffer. */
        DWORD cbPlayPos;
        HRESULT hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, NULL);
        if (hr == DSERR_BUFFERLOST)
        {
            hr = directSoundPlayRestore(pDSB);
            if (SUCCEEDED(hr))
            {
                hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, NULL);
            }
        }

        if (FAILED(hr))
        {
            if (hr != DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
                DSLOGREL(("DSound: Playback GetCurrentPosition %Rhrc\n", hr));
            break;
        }

        DWORD cbFree = cbBuffer - dsoundRingDistance(pDSoundStrmOut->cbPlayWritePos, cbPlayPos, cbBuffer);
        /*
         * Check for full buffer, do not allow the cbPlayWritePos to catch cbPlayPos during playback,
         * i.e. always leave a free space for 1 audio sample.
         */
        const DWORD cbSample = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, 1);
        if (cbFree <= cbSample)
            break;
        cbFree     -= cbSample;

        uint32_t csLive = AudioMixBufAvail(&pHstStrmOut->MixBuf);
        uint32_t cbLive = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, csLive);

        /* Do not write more than available space in the DirectSound playback buffer. */
        cbLive = RT_MIN(cbFree, cbLive);

        cbLive &= ~pHstStrmOut->Props.uAlign;
        if (cbLive == 0 || cbLive > cbBuffer)
        {
            DSLOG(("DSound: cbLive=%RU32, cbBuffer=%ld, cbPlayWritePos=%ld, cbPlayPos=%ld\n",
                   cbLive, cbBuffer, pDSoundStrmOut->cbPlayWritePos, cbPlayPos));
            break;
        }

        LPVOID pv1, pv2;
        DWORD cb1, cb2;
        hr = directSoundPlayLock(pDSB, &pHstStrmOut->Props, pDSoundStrmOut->cbPlayWritePos, cbLive,
                                 &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
        if (FAILED(hr))
            break;

        DWORD len1 = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cb1);
        DWORD len2 = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cb2);

        uint32_t cRead = 0;

        if (pv1 && cb1)
        {
            rc = AudioMixBufReadCirc(&pHstStrmOut->MixBuf, pv1, cb1, &cRead);
            if (RT_SUCCESS(rc))
                cReadTotal += cRead;
        }

        if (   RT_SUCCESS(rc)
            && cReadTotal == len1
            && pv2 && cb2)
        {
            rc = AudioMixBufReadCirc(&pHstStrmOut->MixBuf, pv2, cb2, &cRead);
            if (RT_SUCCESS(rc))
                cReadTotal += cRead;
        }

        directSoundPlayUnlock(pDSB, pv1, pv2, cb1, cb2);

        pDSoundStrmOut->cbPlayWritePos =
            (pDSoundStrmOut->cbPlayWritePos + AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal)) % cbBuffer;

        DSLOGF(("DSound: %RU32 (%RU32 samples) out of %RU32%s, buffer write pos %ld, rc=%Rrc\n",
                AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal), cReadTotal, cbLive,
                cbLive != AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal) ? " !!!": "",
                pDSoundStrmOut->cbPlayWritePos, rc));

        if (cReadTotal)
        {
            AudioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);
            rc = VINF_SUCCESS; /* Played something. */
        }

        if (RT_FAILURE(rc))
            break;

        if (pDSoundStrmOut->fRestartPlayback)
        {
            /* The playback has been just started.
             * Some samples of the new sound have been copied to the buffer
             * and it can start playing.
             */
            pDSoundStrmOut->fRestartPlayback = false;
            hr = IDirectSoundBuffer8_Play(pDSoundStrmOut->pDSB, 0, 0, DSBPLAY_LOOPING);
            if (FAILED(hr))
            {
                DSLOGREL(("DSound: Playback start %Rhrc\n", hr));
                rc = VERR_NOT_SUPPORTED;
            }
        }
    } while (0);

    if (pcSamplesPlayed)
        *pcSamplesPlayed = cReadTotal;

    return rc;
}

static DECLCALLBACK(int) drvHostDSoundFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    directSoundPlayClose(pDSoundStrmOut);

    pDSoundStrmOut->cbPlayWritePos = 0;
    pDSoundStrmOut->fRestartPlayback = true;
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
    pDSoundStrmIn->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

    /** @todo caller should already init Props? */
    int rc = DrvAudioStreamCfgToProps(&pDSoundStrmIn->streamCfg, &pHstStrmIn->Props);
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
        directSoundCaptureOpen(pThis, pDSoundStrmIn);
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
            HRESULT hr = directSoundCaptureStart(pThis, pDSoundStrmIn);
            if (FAILED(hr))
            {
                directSoundCaptureClose(pDSoundStrmIn);
                directSoundCaptureOpen(pThis, pDSoundStrmIn);

                hr = directSoundCaptureStart(pThis, pDSoundStrmIn);
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
        } break;

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            directSoundCaptureStop(pDSoundStrmIn);
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
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB = pDSoundStrmIn->pDSCB;

    int rc = VINF_SUCCESS;

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
            DSLOGREL(("DSound: Capture GetCurrentPosition %Rhrc\n", hr));
            pDSoundStrmIn->hrLastCaptureIn = hr;
        }

        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }
    pDSoundStrmIn->hrLastCaptureIn = hr;

    if (cbReadPos & pHstStrmIn->Props.uAlign)
        DSLOGF(("DSound: Misaligned capture read position %ld (alignment: %RU32)\n", cbReadPos, pHstStrmIn->Props.uAlign));

    /* Capture position in samples. */
    DWORD csReadPos = cbReadPos >> pHstStrmIn->Props.cShift;

    /* Number of samples available in the DirectSound capture buffer. */
    DWORD csCaptured = dsoundRingDistance(csReadPos, pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize);
    if (csCaptured == 0)
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    /* Using as an intermediate not circular buffer. */
    AudioMixBufReset(&pHstStrmIn->MixBuf);

    /* Get number of free samples in the mix buffer and check that is has free space */
    uint32_t csMixFree = AudioMixBufFree(&pHstStrmIn->MixBuf);
    if (csMixFree == 0)
    {
        DSLOGF(("DSound: Capture buffer full\n"));
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    DSLOGF(("DSound: Capture csMixFree=%RU32, csReadPos=%ld, csCaptureReadPos=%ld, csCaptured=%ld\n",
            csMixFree, csReadPos, pDSoundStrmIn->csCaptureReadPos, csCaptured));

    /* No need to fetch more samples than mix buffer can receive. */
    csCaptured = RT_MIN(csCaptured, csMixFree);

    /* Lock relevant range in the DirectSound capture buffer. */
    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    hr = directSoundCaptureLock(pDSCB, &pHstStrmIn->Props,
                                AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, pDSoundStrmIn->csCaptureReadPos), /* dwOffset */
                                AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, csCaptured),                      /* dwBytes */
                                &pv1, &pv2, &cb1, &cb2,
                                0 /* dwFlags */);
    if (FAILED(hr))
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    DWORD len1 = AUDIOMIXBUF_B2S(&pHstStrmIn->MixBuf, cb1);
    DWORD len2 = AUDIOMIXBUF_B2S(&pHstStrmIn->MixBuf, cb2);

    uint32_t csWrittenTotal = 0;
    uint32_t csWritten;
    if (pv1 && len1)
    {
        rc = AudioMixBufWriteAt(&pHstStrmIn->MixBuf, 0 /* offWrite */,
                                pv1, cb1, &csWritten);
        if (RT_SUCCESS(rc))
            csWrittenTotal += csWritten;
    }

    if (   RT_SUCCESS(rc)
        && csWrittenTotal == len1
        && pv2 && len2)
    {
        rc = AudioMixBufWriteAt(&pHstStrmIn->MixBuf, csWrittenTotal,
                                pv2, cb2, &csWritten);
        if (RT_SUCCESS(rc))
            csWrittenTotal += csWritten;
    }

    directSoundCaptureUnlock(pDSCB, pv1, pv2, cb1, cb2);

    uint32_t csProcessed = 0;
    if (csWrittenTotal != 0)
    {
        /* Captured something. */
        rc = AudioMixBufMixToParent(&pHstStrmIn->MixBuf, csWrittenTotal,
                                    &csProcessed);
    }

    if (RT_SUCCESS(rc))
    {
        pDSoundStrmIn->csCaptureReadPos = (pDSoundStrmIn->csCaptureReadPos + csProcessed) % pDSoundStrmIn->csCaptureBufferSize;
        DSLOGF(("DSound: Capture %ld (%ld+%ld), processed %RU32/%RU32\n",
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

    directSoundCaptureClose(pDSoundStrmIn);

    pDSoundStrmIn->csCaptureReadPos    = 0;
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

    RTLDRMOD hDSound = NULL;
    int rc = RTLdrLoadSystem("dsound.dll", true /*fNoUnload*/, &hDSound);
    if (RT_SUCCESS(rc))
    {
        PFNDIRECTSOUNDENUMERATEW pfnDirectSoundEnumerateW = NULL;
        PFNDIRECTSOUNDCAPTUREENUMERATEW pfnDirectSoundCaptureEnumerateW = NULL;

        rc = RTLdrGetSymbol(hDSound, "DirectSoundEnumerateW", (void**)&pfnDirectSoundEnumerateW);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(hDSound, "DirectSoundCaptureEnumerateW", (void**)&pfnDirectSoundCaptureEnumerateW);
        }

        if (RT_SUCCESS(rc))
        {
            DSOUNDENUMCBCTX ctx = { pThis, pCfg };

            HRESULT hr = pfnDirectSoundEnumerateW(&dsoundEnumCallback, &ctx);
            if (FAILED(hr))
                LogRel(("DSound: Error enumerating host playback devices: %Rhrc\n", hr));

            LogRel(("DSound: Found %RU32 host playback devices\n", pCfg->cMaxHstStrmsOut));

            hr = pfnDirectSoundCaptureEnumerateW(&dsoundCaptureEnumCallback, &ctx);
            if (FAILED(hr))
                LogRel(("DSound: Error enumerating host capturing devices: %Rhrc\n", hr));

            LogRel(("DSound: Found %RU32 host capturing devices\n", pCfg->cMaxHstStrmsIn));
        }

        RTLdrClose(hDSound);
    }
    else
    {
        /* No dsound.dll on this system.  */
        LogRel(("DSound: Could not load dsound.dll %Rrc\n", rc));
    }

    /* Always return success and at least default values to make the caller happy. */
    if (pCfg->cMaxHstStrmsOut == 0)
    {
        LogRel(("DSound: Adjusting the number of host playback devices to 1\n"));
        pCfg->cMaxHstStrmsOut = 1; /* Support at least one stream. */
    }

    if (pCfg->cMaxHstStrmsIn < 2)
    {
        LogRel(("DSound: Adjusting the number of host capturing devices from %RU32 to 2\n", pCfg->cMaxHstStrmsIn));
        pCfg->cMaxHstStrmsIn = 2; /* Support at least two streams (line in + mic). */
    }

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
        DSLOGREL(("DSound: DirectSound not available %Rhrc\n", hr));

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

static LPCGUID dsoundConfigQueryGUID(PCFGMNODE pCfg, const char *pszName, RTUUID *pUuid)
{
    LPCGUID pGuid = NULL;

    char *pszGuid = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfg, pszName, &pszGuid);
    if (RT_SUCCESS(rc))
    {
        rc = RTUuidFromStr(pUuid, pszGuid);
        if (RT_SUCCESS(rc))
            pGuid = (LPCGUID)&pUuid;
        else
            DSLOGREL(("DSound: Error parsing device GUID for device '%s': %Rrc\n", pszName, rc));

        RTStrFree(pszGuid);
    }

    return pGuid;
}

static void dSoundConfigInit(PDRVHOSTDSOUND pThis, PCFGMNODE pCfg)
{
    unsigned int uBufsizeOut, uBufsizeIn;

    CFGMR3QueryUIntDef(pCfg, "BufsizeOut", &uBufsizeOut, _16K);
    CFGMR3QueryUIntDef(pCfg, "BufsizeIn",  &uBufsizeIn,  _16K);
    pThis->cfg.cbBufferOut = uBufsizeOut;
    pThis->cfg.cbBufferIn  = uBufsizeIn;

    pThis->cfg.pGuidPlay    = dsoundConfigQueryGUID(pCfg, "DeviceGuidOut", &pThis->cfg.uuidPlay);
    pThis->cfg.pGuidCapture = dsoundConfigQueryGUID(pCfg, "DeviceGuidIn",  &pThis->cfg.uuidCapture);

    DSLOG(("DSound: BufsizeOut %u, BufsizeIn %u, DeviceGuidOut {%RTuuid}, DeviceGuidIn {%RTuuid}\n",
           pThis->cfg.cbBufferOut,
           pThis->cfg.cbBufferIn,
           &pThis->cfg.uuidPlay,
           &pThis->cfg.uuidCapture));
}

static DECLCALLBACK(void) drvHostDSoundDestruct(PPDMDRVINS pDrvIns)
{
    PDRVHOSTDSOUND pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    LogFlowFuncEnter();

    if (pThis->pDrvIns)
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

    LogRel(("Audio: Initializing DirectSound audio driver\n"));

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: CoInitializeEx %Rhrc\n", hr));
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
