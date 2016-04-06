/* $Id$ */
/** @file
 * Windows host backend driver using DirectSound.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifdef VBOX_WITH_AUDIO_CALLBACKS
# define VBOX_DSOUND_MAX_EVENTS 3

typedef enum DSOUNDEVENT
{
    DSOUNDEVENT_NOTIFY = 0,
    DSOUNDEVENT_INPUT,
    DSOUNDEVENT_OUTPUT,
 } DSOUNDEVENT;
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

typedef struct DSOUNDHOSTCFG
{
    DWORD   cbBufferIn;
    DWORD   cbBufferOut;
    RTUUID  uuidPlay;
    LPCGUID pGuidPlay;
    RTUUID  uuidCapture;
    LPCGUID pGuidCapture;
} DSOUNDHOSTCFG, *PDSOUNDHOSTCFG;

typedef struct DSOUNDSTREAMOUT
{
    PDMAUDIOHSTSTRMOUT   strmOut; /* Always must come first! */
    LPDIRECTSOUND8       pDS;     /** @todo Move this out of this structure! Not required per-stream (e.g. for multi-channel). */
    LPDIRECTSOUNDBUFFER8 pDSB;
    DWORD                cbPlayWritePos;
    DWORD                csPlaybackBufferSize;
    bool                 fEnabled;
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
    bool                        fEnabled;
    PDMAUDIOSTREAMCFG           streamCfg;
} DSOUNDSTREAMIN, *PDSOUNDSTREAMIN;

typedef struct DRVHOSTDSOUND
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Our audio host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
    /** List of found host input devices. */
    RTLISTANCHOR        lstDevInput;
    /** List of found host output devices. */
    RTLISTANCHOR        lstDevOutput;
    /** DirectSound configuration options. */
    DSOUNDHOSTCFG       cfg;
    /** Whether this backend supports any audio input. */
    bool                fEnabledIn;
    /** Whether this backend supports any audio output. */
    bool                fEnabledOut;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /** Pointer to the audio connector interface of the driver/device above us. */
    PPDMIAUDIOCONNECTOR pUpIAudioConnector;
    /** Stopped indicator. */
    bool                fStopped;
    /** Shutdown indicator. */
    bool                fShutdown;
    /** Notification thread. */
    RTTHREAD            Thread;
    /** Array of events to wait for in notification thread. */
    HANDLE              aEvents[VBOX_DSOUND_MAX_EVENTS];
    /** Number of events to wait for in notification thread.
     *  Must not exceed VBOX_DSOUND_MAX_EVENTS. */
    uint8_t             cEvents;
    /** Pointer to the input stream. */
    PDSOUNDSTREAMIN     pDSStrmIn;
    /** Pointer to the output stream. */
    PDSOUNDSTREAMOUT    pDSStrmOut;
#endif
} DRVHOSTDSOUND, *PDRVHOSTDSOUND;

/** No flags specified. */
#define DSOUNDENUMCBFLAGS_NONE          0
/** (Release) log found devices. */
#define DSOUNDENUMCBFLAGS_LOG           RT_BIT(0)

/**
 * Callback context for enumeration callbacks
 */
typedef struct DSOUNDENUMCBCTX
{
    /** Pointer to host backend driver. */
    PDRVHOSTDSOUND      pDrv;
    /** Enumeration flags. */
    uint32_t            fFlags;
    /** Number of found input devices. */
    uint8_t             cDevIn;
    /** Number of found output devices. */
    uint8_t             cDevOut;
} DSOUNDENUMCBCTX, *PDSOUNDENUMCBCTX;

typedef struct DSOUNDDEV
{
    RTLISTNODE  Node;
    char       *pszName;
    GUID        Guid;
} DSOUNDDEV, *PDSOUNDDEV;

/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/

/** Maximum number of attempts to restore the sound buffer before giving up. */
#define DRV_DSOUND_RESTORE_ATTEMPTS_MAX         3

/** Makes DRVHOSTDSOUND out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface) \
    ( (PDRVHOSTDSOUND)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTDSOUND, IHostAudio)) )

/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/

static HRESULT directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB);

static void dsoundDeviceRemove(PDSOUNDDEV pDev);
static int dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg);
#ifdef VBOX_WITH_AUDIO_CALLBACKS
static int dsoundNotifyThread(PDRVHOSTDSOUND pThis, bool fShutdown);
#endif

static DWORD dsoundRingDistance(DWORD offEnd, DWORD offBegin, DWORD cSize)
{
    return offEnd >= offBegin ? offEnd - offBegin : cSize - offBegin + offEnd;
}

static int dsoundWaveFmtFromCfg(PPDMAUDIOSTREAMCFG pCfg, PWAVEFORMATEX pFmt)
{
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(pFmt, VERR_INVALID_POINTER);

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

static int dsoundGetPosOut(PDRVHOSTDSOUND   pThis,
                           PDSOUNDSTREAMOUT pDSoundStrmOut, DWORD *pdwBuffer, DWORD *pdwFree, DWORD *pdwPlayPos)
{
    AssertPtrReturn(pThis,          VERR_INVALID_POINTER);
    AssertPtrReturn(pDSoundStrmOut, VERR_INVALID_POINTER);

    LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStrmOut->pDSB;
    if (!pDSB)
        return VERR_INVALID_POINTER;

    DWORD cbBuffer = AUDIOMIXBUF_S2B(&pDSoundStrmOut->strmOut.MixBuf, pDSoundStrmOut->csPlaybackBufferSize);

    /* Get the current play position which is used for calculating the free space in the buffer. */
    DWORD cbPlayPos;

    HRESULT hr;
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, NULL);
        if (   SUCCEEDED(hr)
            || hr != DSERR_BUFFERLOST) /** @todo: MSDN doesn't state this error for GetCurrentPosition(). */
        {
            break;
        }
        else
        {
            LogFlowFunc(("Getting playing position failed due to lost buffer, restoring ...\n"));
            directSoundPlayRestore(pThis, pDSB);
        }
    }

    int rc = VINF_SUCCESS;

    if (FAILED(hr))
    {
        if (hr != DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
            DSLOGREL(("DSound: Getting current playback position failed with %Rhrc\n", hr));
        LogFlowFunc(("Failed with %Rhrc\n", hr));

        rc = VERR_NOT_AVAILABLE;
    }
    else
    {
        if (pdwBuffer)
            *pdwBuffer = cbBuffer;

        if (pdwFree)
            *pdwFree = cbBuffer - dsoundRingDistance(pDSoundStrmOut->cbPlayWritePos, cbPlayPos, cbBuffer);

        if (pdwPlayPos)
            *pdwPlayPos = cbPlayPos;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
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

/**
 * Clears the list of the host's playback + capturing devices.
 *
 * @param   pThis               Host audio driver instance.
 */
static void dsoundDevicesClear(PDRVHOSTDSOUND pThis)
{
    AssertPtrReturnVoid(pThis);

    PDSOUNDDEV pDev;
    while (!RTListIsEmpty(&pThis->lstDevInput))
    {
        pDev = RTListGetFirst(&pThis->lstDevInput, DSOUNDDEV, Node);
        dsoundDeviceRemove(pDev);
    }

    while (!RTListIsEmpty(&pThis->lstDevOutput))
    {
        pDev = RTListGetFirst(&pThis->lstDevOutput, DSOUNDDEV, Node);
        dsoundDeviceRemove(pDev);
    }
}

static HRESULT directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB)
{
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (FAILED(hr))
        DSLOGREL(("DSound: Restoring playback buffer failed with %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayUnlock(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB,
                                     LPVOID pv1, LPVOID pv2,
                                     DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundBuffer8_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking playback buffer failed with %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureUnlock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB,
                                        LPVOID pv1, LPVOID pv2,
                                        DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer8_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking capture buffer failed with %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayLock(PDRVHOSTDSOUND pThis,
                                   LPDIRECTSOUNDBUFFER8 pDSB, PDMPCMPROPS *pProps,
                                   DWORD dwOffset, DWORD dwBytes,
                                   LPVOID *ppv1, LPVOID *ppv2,
                                   DWORD *pcb1, DWORD *pcb2,
                                   DWORD dwFlags)
{
    LPVOID pv1 = NULL;
    LPVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr;
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
        if (   SUCCEEDED(hr)
            || hr != DSERR_BUFFERLOST)
            break;
        else
        {
            LogFlowFunc(("Locking failed due to lost buffer, restoring ...\n"));
            directSoundPlayRestore(pThis, pDSB);
        }
    }

    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Locking playback buffer failed with %Rhrc\n", hr));
        return hr;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        DSLOGREL(("DSound: Locking playback buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                  cb1, cb2, pProps->uAlign));
        directSoundPlayUnlock(pThis, pDSB, pv1, pv2, cb1, cb2);
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
        DSLOGREL(("DSound: Locking capture buffer failed with %Rhrc\n", hr));
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
        DSLOGREL(("DSound: Creating playback instance failed with %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pDSoundStrmOut->pDS, pThis->cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pDSoundStrmOut->pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                DSLOGREL(("DSound: Setting cooperative level for window %p failed with %Rhrc\n", hWnd, hr));
        }

        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no playback devices are attached. */
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSound playback initialization failed with %Rhrc\n", hr));

            directSoundPlayInterfaceRelease(pDSoundStrmOut);
        }
    }

    return hr;
}

static HRESULT directSoundPlayClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStrmOut, E_POINTER);

    DSLOG(("DSound: Closing playback stream %p, buffer %p\n", pDSoundStrmOut, pDSoundStrmOut->pDSB));

    HRESULT hr = S_OK;

    if (pDSoundStrmOut->pDSB)
    {
        hr = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (SUCCEEDED(hr))
        {
#ifdef VBOX_WITH_AUDIO_CALLBACKS
            if (pThis->aEvents[DSOUNDEVENT_OUTPUT] != NULL)
            {
                CloseHandle(pThis->aEvents[DSOUNDEVENT_OUTPUT]);
                pThis->aEvents[DSOUNDEVENT_OUTPUT] = NULL;

                if (pThis->cEvents)
                    pThis->cEvents--;

                pThis->pDSStrmOut = NULL;
            }

            int rc2 = dsoundNotifyThread(pThis, false /* fShutdown */);
            AssertRC(rc2);
#endif
            IDirectSoundBuffer8_Release(pDSoundStrmOut->pDSB);
            pDSoundStrmOut->pDSB = NULL;
        }
        else
            DSLOGREL(("DSound: Stop playback stream %p when closing %Rhrc\n", pDSoundStrmOut, hr));
    }

    if (SUCCEEDED(hr))
        directSoundPlayInterfaceRelease(pDSoundStrmOut);

    return hr;
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
        DSLOGREL(("DSound: Playback buffer already exists\n"));
        directSoundPlayClose(pThis, pDSoundStrmOut);
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
        bd.dwSize      = sizeof(bd);
        bd.lpwfxFormat = &wfx;

        /*
         * As we reuse our (secondary) buffer for playing out data as it comes in,
         * we're using this buffer as a so-called static buffer.
         *
         * However, as we do not want to use memory on the sound device directly
         * (as most modern audio hardware on the host doesn't have this anyway),
         * we're *not* going to use DSBCAPS_STATIC for that.
         *
         * Instead we're specifying DSBCAPS_LOCSOFTWARE, as this fits the bill
         * of copying own buffer data (from AudioMixBuf) to our secondary's Direct Sound buffer.
         */
        bd.dwFlags     = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_LOCSOFTWARE;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
        bd.dwFlags    |= DSBCAPS_CTRLPOSITIONNOTIFY;
#endif
        bd.dwBufferBytes = pThis->cfg.cbBufferOut;

        hr = IDirectSound8_CreateSoundBuffer(pDSoundStrmOut->pDS, &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Creating playback sound buffer failed with %Rhrc\n", hr));
            break;
        }

        /* "Upgrade" to IDirectSoundBuffer8 interface. */
        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (LPVOID *)&pDSoundStrmOut->pDSB);
        IDirectSoundBuffer_Release(pDSB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying playback sound buffer interface failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        hr = IDirectSoundBuffer8_GetFormat(pDSoundStrmOut->pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback format failed with %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundBuffer8_GetCaps(pDSoundStrmOut->pDSB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Playback format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  dwFlags         = 0x%x\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               bc.dwFlags,
               wfx.wFormatTag,
               wfx.nChannels,
               wfx.nSamplesPerSec,
               wfx.nAvgBytesPerSec,
               wfx.nBlockAlign,
               wfx.wBitsPerSample,
               wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmOut->strmOut.Props.uAlign)
            DSLOGREL(("DSound: Playback capabilities returned misaligned buffer: size %RU32, alignment %RU32\n",
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

#ifdef VBOX_WITH_AUDIO_CALLBACKS
        /*
         * Install notification.
         */
        pThis->aEvents[DSOUNDEVENT_OUTPUT] = CreateEvent(NULL /* Security attribute */,
                                                         FALSE /* bManualReset */, FALSE /* bInitialState */,
                                                         NULL /* lpName */);
        if (pThis->aEvents[DSOUNDEVENT_OUTPUT] == NULL)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            DSLOGREL(("DSound: CreateEvent for output failed with %Rhrc\n", hr));
            break;
        }

        LPDIRECTSOUNDNOTIFY8 pNotify;
        hr = IDirectSoundNotify_QueryInterface(pDSoundStrmOut->pDSB, IID_IDirectSoundNotify8, (LPVOID *)&pNotify);
        if (SUCCEEDED(hr))
        {
            DSBPOSITIONNOTIFY dsBufPosNotify;
            RT_ZERO(dsBufPosNotify);
            dsBufPosNotify.dwOffset     = DSBPN_OFFSETSTOP;
            dsBufPosNotify.hEventNotify = pThis->aEvents[DSOUNDEVENT_OUTPUT];

            hr = IDirectSoundNotify_SetNotificationPositions(pNotify, 1 /* Count */, &dsBufPosNotify);
            if (FAILED(hr))
                DSLOGREL(("DSound: Setting playback position notification failed with %Rhrc\n", hr));

            IDirectSoundNotify_Release(pNotify);
        }
        else
            DSLOGREL(("DSound: Querying interface for position notification failed with %Rhrc\n", hr));

        if (FAILED(hr))
            break;

        pThis->pDSStrmOut = pDSoundStrmOut;

        Assert(pThis->cEvents < VBOX_DSOUND_MAX_EVENTS);
        pThis->cEvents++;

        /* Let the thread know. */
        dsoundNotifyThread(pThis, false /* fShutdown */);

        /* Trigger the just installed output notification. */
        hr = IDirectSoundBuffer8_Play(pDSoundStrmOut->pDSB, 0, 0, 0);

#endif /* VBOX_WITH_AUDIO_CALLBACKS */

    } while (0);

    if (FAILED(hr))
        directSoundPlayClose(pThis, pDSoundStrmOut);

    return hr;
}

static void dsoundPlayClearSamples(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturnVoid(pDSoundStrmOut);

    PPDMAUDIOHSTSTRMOUT pStrmOut = &pDSoundStrmOut->strmOut;

    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    HRESULT hr = directSoundPlayLock(pThis, pDSoundStrmOut->pDSB, &pDSoundStrmOut->strmOut.Props,
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

        directSoundPlayUnlock(pThis, pDSoundStrmOut->pDSB, pv1, pv2, cb1, cb2);
    }
}

static HRESULT directSoundPlayGetStatus(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pdwStatus)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSB,  E_POINTER);
    /* pdwStatus is optional. */

    DWORD dwStatus = 0;

    HRESULT hr;
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
        if (   hr == DSERR_BUFFERLOST
            || (   SUCCEEDED(hr)
                && (dwStatus & DSBSTATUS_BUFFERLOST)))
        {
            LogFlowFunc(("Getting status failed due to lost buffer, restoring ...\n"));
            directSoundPlayRestore(pThis, pDSB);
        }
        else
            break;
    }

    if (SUCCEEDED(hr))
    {
        if (pdwStatus)
            *pdwStatus = dwStatus;
    }
    else
        DSLOGREL(("DSound: Retrieving playback status failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pThis,          E_POINTER);
    AssertPtrReturn(pDSoundStrmOut, E_POINTER);

    HRESULT hr;

    if (pDSoundStrmOut->pDSB != NULL)
    {
        DSLOG(("DSound: Stopping playback\n"));

        HRESULT hr2 = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr2))
        {
            hr2 = directSoundPlayRestore(pThis, pDSoundStrmOut->pDSB);
            if (FAILED(hr2))
                hr2 = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        }

        if (FAILED(hr2))
            DSLOG(("DSound: Stopping playback failed with %Rhrc\n", hr2));

        hr = S_OK; /* Always report success here. */
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
    {
        dsoundPlayClearSamples(pThis, pDSoundStrmOut);
        pDSoundStrmOut->fEnabled = false;
    }
    else
        DSLOGREL(("DSound: Stopping playback failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pThis,          E_POINTER);
    AssertPtrReturn(pDSoundStrmOut, E_POINTER);

    HRESULT hr;
    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        hr = directSoundPlayGetStatus(pThis, pDSoundStrmOut->pDSB, &dwStatus);
        if (SUCCEEDED(hr))
        {
            if (dwStatus & DSBSTATUS_PLAYING)
            {
                DSLOG(("DSound: Already playing\n"));
            }
            else
            {
                dsoundPlayClearSamples(pThis, pDSoundStrmOut);

                pDSoundStrmOut->fRestartPlayback = true;
                pDSoundStrmOut->fEnabled         = true;

                DSLOG(("DSound: Playback started\n"));

                /*
                 * The actual IDirectSoundBuffer8_Play call will be made in drvHostDSoundPlayOut,
                 * because it is necessary to put some samples into the buffer first.
                 */
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    if (FAILED(hr))
        DSLOGREL(("DSound: Starting playback failed with %Rhrc\n", hr));

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

            case PDMAUDIORECSOURCE_LINE:
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
        LogFlowFuncEnter();
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
        DSLOGREL(("DSound: Creating capture instance failed with %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pDSoundStrmIn);
        hr = IDirectSoundCapture_Initialize(pDSoundStrmIn->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no capture devices are attached. */
                DSLOGREL(("DSound: Capture device currently is unavailable\n"));
            else
                DSLOGREL(("DSound: Initializing capturing device failed with %Rhrc\n", hr));

            directSoundCaptureInterfaceRelease(pDSoundStrmIn);
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureClose(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pDSoundStrmIn, E_POINTER);

    DSLOG(("DSound: pDSoundStrmIn=%p, pDSCB=%p\n", pDSoundStrmIn, pDSoundStrmIn->pDSCB));

    HRESULT hr = S_OK;

    if (pDSoundStrmIn->pDSCB)
    {
        hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (SUCCEEDED(hr))
        {
            IDirectSoundCaptureBuffer8_Release(pDSoundStrmIn->pDSCB);
            pDSoundStrmIn->pDSCB = NULL;
        }
        else
            DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));
    }

    if (SUCCEEDED(hr))
        directSoundCaptureInterfaceRelease(pDSoundStrmIn);

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
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
            if (hr == E_ACCESSDENIED)
            {
                DSLOGREL(("DSound: Capturing input from host not possible, access denied\n"));
            }
            else
                DSLOGREL(("DSound: Creating capture buffer failed with %Rhrc\n", hr));
            break;
        }

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pDSoundStrmIn->pDSCB);
        IDirectSoundCaptureBuffer_Release(pDSCB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying interface for capture buffer failed with %Rhrc\n", hr));
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
            DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pDSoundStrmIn->pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture format failed with %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pDSoundStrmIn->pDSCB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("Getting capture capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Capture format:\n"
               "  dwBufferBytes   = %RI32\n"
               "  dwFlags         = 0x%x\n"
               "  wFormatTag      = %RI16\n"
               "  nChannels       = %RI16\n"
               "  nSamplesPerSec  = %RU32\n"
               "  nAvgBytesPerSec = %RU32\n"
               "  nBlockAlign     = %RI16\n"
               "  wBitsPerSample  = %RI16\n"
               "  cbSize          = %RI16\n",
               bc.dwBufferBytes,
               bc.dwFlags,
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

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis        , E_POINTER);
    AssertPtrReturn(pDSoundStrmIn, E_POINTER);

    NOREF(pThis);

    HRESULT hr;

    if (pDSoundStrmIn->pDSCB)
    {
        DSLOG(("DSound: Stopping capture\n"));

        hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED(hr))
            DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
        pDSoundStrmIn->fEnabled = false;

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
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
            DSLOGREL(("DSound: Retrieving capture status failed with %Rhrc\n", hr));
        }
        else
        {
            if (dwStatus & DSCBSTATUS_CAPTURING)
            {
                DSLOG(("DSound: Already capturing\n"));
            }
            else
            {
                DWORD fFlags = 0;
#ifndef VBOX_WITH_AUDIO_CALLBACKS
                fFlags |= DSCBSTART_LOOPING;
#endif
                DSLOG(("DSound: Starting to capture\n"));
                hr = IDirectSoundCaptureBuffer8_Start(pDSoundStrmIn->pDSCB, fFlags);
                if (FAILED(hr))
                    DSLOGREL(("DSound: Starting to capture failed with %Rhrc\n", hr));
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
        pDSoundStrmIn->fEnabled = true;

    LogFlowFunc(("Returning %Rhrc\n", hr));
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

static void dsoundDeviceRemove(PDSOUNDDEV pDev)
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

static BOOL CALLBACK dsoundDevicesEnumCbPlayback(LPGUID lpGUID, LPCWSTR lpwstrDescription,
                                                 LPCWSTR lpwstrModule, LPVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!lpGUID)
        return TRUE;

    AssertPtrReturn(lpwstrDescription, FALSE);
    /* Do not care about lpwstrModule. */

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Output", lpGUID, lpwstrDescription, lpwstrModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevOutput,
                          lpGUID, lpwstrDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->cDevOut++;

    return TRUE;
}

static BOOL CALLBACK dsoundDevicesEnumCbCapture(LPGUID lpGUID, LPCWSTR lpwstrDescription,
                                                LPCWSTR lpwstrModule, LPVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!lpGUID)
        return TRUE;

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Input", lpGUID, lpwstrDescription, lpwstrModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevInput,
                          lpGUID, lpwstrDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->cDevIn++;

    return TRUE;
}

/**
 * Does a (Re-)enumeration of the host's playback + capturing devices.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pEnmCtx             Enumeration context to use.
 * @param   fEnum               Enumeration flags.
 */
static int dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PDSOUNDENUMCBCTX pEnmCtx, uint32_t fEnum)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pEnmCtx, VERR_INVALID_POINTER);

    dsoundDevicesClear(pThis);

    RTLDRMOD hDSound = NULL;
    int rc = RTLdrLoadSystem("dsound.dll", true /*fNoUnload*/, &hDSound);
    if (RT_SUCCESS(rc))
    {
        PFNDIRECTSOUNDENUMERATEW pfnDirectSoundEnumerateW = NULL;
        PFNDIRECTSOUNDCAPTUREENUMERATEW pfnDirectSoundCaptureEnumerateW = NULL;

        rc = RTLdrGetSymbol(hDSound, "DirectSoundEnumerateW", (void**)&pfnDirectSoundEnumerateW);
        if (RT_SUCCESS(rc))
            rc = RTLdrGetSymbol(hDSound, "DirectSoundCaptureEnumerateW", (void**)&pfnDirectSoundCaptureEnumerateW);

        if (RT_SUCCESS(rc))
        {
            HRESULT hr = pfnDirectSoundEnumerateW(&dsoundDevicesEnumCbPlayback, pEnmCtx);
            if (FAILED(hr))
                LogRel2(("DSound: Error enumerating host playback devices: %Rhrc\n", hr));

            hr = pfnDirectSoundCaptureEnumerateW(&dsoundDevicesEnumCbCapture, pEnmCtx);
            if (FAILED(hr))
                LogRel2(("DSound: Error enumerating host capturing devices: %Rhrc\n", hr));

            if (fEnum & DSOUNDENUMCBFLAGS_LOG)
            {
                LogRel2(("DSound: Found %RU8 host playback devices\n",  pEnmCtx->cDevOut));
                LogRel2(("DSound: Found %RU8 host capturing devices\n", pEnmCtx->cDevIn));
            }
        }

        RTLdrClose(hDSound);
    }
    else
    {
        /* No dsound.dll on this system. */
        LogRel2(("DSound: Could not load dsound.dll: %Rrc\n", rc));
    }

    return rc;
}

/**
 * Updates this host driver's internal status, according to the global, overall input/output
 * state and all connected (native) audio streams.
 *
 * @param   pThis               Host audio driver instance.
 * @param   pCfg                Where to store the backend configuration. Optional.
 * @param   fEnum               Enumeration flags.
 */
void dsoundUpdateStatusInternalEx(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum)
{
    AssertPtrReturnVoid(pThis);
    /* pCfg is optional. */

    PDMAUDIOBACKENDCFG Cfg;
    RT_ZERO(Cfg);

    Cfg.cbStreamOut     = sizeof(DSOUNDSTREAMOUT);
    Cfg.cbStreamIn      = sizeof(DSOUNDSTREAMIN);

    DSOUNDENUMCBCTX cbCtx = { pThis, fEnum, 0, 0 };

    int rc = dsoundDevicesEnumerate(pThis, &cbCtx, fEnum);
    if (RT_SUCCESS(rc))
    {
#ifdef VBOX_WITH_AUDIO_CALLBACKS
        if (   pThis->fEnabledOut != RT_BOOL(cbCtx.cDevOut)
            || pThis->fEnabledIn  != RT_BOOL(cbCtx.cDevIn))
        {
            /** @todo Use a registered callback to the audio connector (e.g "OnConfigurationChanged") to
             *        let the connector know that something has changed within the host backend. */
        }
#else
        pThis->fEnabledOut = RT_BOOL(cbCtx.cDevOut);
        pThis->fEnabledIn  = RT_BOOL(cbCtx.cDevIn);
#endif

        Cfg.cSources       = cbCtx.cDevIn;
        Cfg.cSinks         = cbCtx.cDevOut;
        Cfg.cMaxStreamsIn  = UINT32_MAX;
        Cfg.cMaxStreamsOut = UINT32_MAX;

        if (pCfg)
            memcpy(pCfg, &Cfg, sizeof(PDMAUDIOBACKENDCFG));
    }

    LogFlowFuncLeaveRC(rc);
}

void dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis)
{
    dsoundUpdateStatusInternalEx(pThis, NULL /* pCfg */, 0 /* fEnum */);
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

    PDRVHOSTDSOUND   pThis          = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_ENABLE\n"));
            /* Try to start playback. If it fails, then reopen and try again. */
            hr = directSoundPlayStart(pThis, pDSoundStrmOut);
            if (FAILED(hr))
            {
                hr = directSoundPlayClose(pThis, pDSoundStrmOut);
                if (SUCCEEDED(hr))
                    hr = directSoundPlayOpen(pThis, pDSoundStrmOut);
                if (SUCCEEDED(hr))
                    hr = directSoundPlayStart(pThis, pDSoundStrmOut);
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_DISABLE\n"));
            hr = directSoundPlayStop(pThis, pDSoundStrmOut);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
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

#ifdef DEBUG_andy
    LogFlowFuncEnter();
#endif

    do /* to use 'break' */
    {
        DWORD cbBuffer, cbFree, cbPlayPos;
        rc = dsoundGetPosOut(pThis, pDSoundStrmOut, &cbBuffer, &cbFree, &cbPlayPos);
        if (RT_FAILURE(rc))
            break;

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

        LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStrmOut->pDSB;
        AssertPtr(pDSB);

        LPVOID pv1, pv2;
        DWORD cb1, cb2;
        HRESULT hr = directSoundPlayLock(pThis, pDSB, &pHstStrmOut->Props, pDSoundStrmOut->cbPlayWritePos, cbLive,
                                         &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
        if (FAILED(hr))
        {
            rc = VERR_ACCESS_DENIED;
            break;
        }

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

        directSoundPlayUnlock(pThis, pDSB, pv1, pv2, cb1, cb2);

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
            /*
             * The playback has been just started.
             * Some samples of the new sound have been copied to the buffer
             * and it can start playing.
             */
            pDSoundStrmOut->fRestartPlayback = false;

            DWORD fFlags = 0;
#ifndef VBOX_WITH_AUDIO_CALLBACKS
            fFlags |= DSCBSTART_LOOPING;
#endif
            for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
            {
                hr = IDirectSoundBuffer8_Play(pDSoundStrmOut->pDSB, 0, 0, fFlags);
                if (   SUCCEEDED(hr)
                    || hr != DSERR_BUFFERLOST)
                    break;
                else
                {
                    LogFlowFunc(("Restarting playback failed due to lost buffer, restoring ...\n"));
                    directSoundPlayRestore(pThis, pDSoundStrmOut->pDSB);
                }
            }

            if (FAILED(hr))
            {
                DSLOGREL(("DSound: Starting playback failed with %Rhrc\n", hr));
                rc = VERR_NOT_SUPPORTED;
                break;
            }
        }

    } while (0);

    if (RT_FAILURE(rc))
    {
        dsoundUpdateStatusInternal(pThis);
    }
    else
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostDSoundFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStrmOut = (PDSOUNDSTREAMOUT)pHstStrmOut;

    directSoundPlayClose(pThis, pDSoundStrmOut);

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

    PDRVHOSTDSOUND  pThis         = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMIN pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Try to start capture. If it fails, then reopen and try again. */
            hr = directSoundCaptureStart(pThis, pDSoundStrmIn);
            if (FAILED(hr))
            {
                hr = directSoundCaptureClose(pDSoundStrmIn);
                if (SUCCEEDED(hr))
                {
                    hr = directSoundCaptureOpen(pThis, pDSoundStrmIn);
                    if (SUCCEEDED(hr))
                        hr = directSoundCaptureStart(pThis, pDSoundStrmIn);
                }
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            hr = directSoundCaptureStop(pThis, pDSoundStrmIn);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        default:
        {
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
        }
    }

    return rc;
}

static DECLCALLBACK(int) drvHostDSoundCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                uint32_t *pcSamplesCaptured)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    PDSOUNDSTREAMIN             pDSoundStrmIn = (PDSOUNDSTREAMIN)pHstStrmIn;
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB         = pDSoundStrmIn->pDSCB;

    int rc = VINF_SUCCESS;

    uint32_t cCaptured = 0;

    do
    {
        if (pDSCB == NULL)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        /* Get DirectSound capture position in bytes. */
        DWORD cbReadPos;
        HRESULT hr = IDirectSoundCaptureBuffer_GetCurrentPosition(pDSCB, NULL, &cbReadPos);
        if (FAILED(hr))
        {
            if (hr != pDSoundStrmIn->hrLastCaptureIn)
            {
                DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
                pDSoundStrmIn->hrLastCaptureIn = hr;
            }

            rc = VERR_NOT_AVAILABLE;
            break;
        }

        pDSoundStrmIn->hrLastCaptureIn = hr;

        if (cbReadPos & pHstStrmIn->Props.uAlign)
            DSLOGF(("DSound: Misaligned capture read position %ld (alignment: %RU32)\n", cbReadPos, pHstStrmIn->Props.uAlign));

        /* Capture position in samples. */
        DWORD csReadPos = cbReadPos >> pHstStrmIn->Props.cShift;

        /* Number of samples available in the DirectSound capture buffer. */
        DWORD csCaptured = dsoundRingDistance(csReadPos, pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize);
        if (csCaptured == 0)
            break;

        /* Using as an intermediate not circular buffer. */
        AudioMixBufReset(&pHstStrmIn->MixBuf);

        /* Get number of free samples in the mix buffer and check that is has free space */
        uint32_t csMixFree = AudioMixBufFree(&pHstStrmIn->MixBuf);
        if (csMixFree == 0)
        {
            DSLOGF(("DSound: Capture buffer full\n"));
            break;
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
            rc = VERR_ACCESS_DENIED;
            break;
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

        if (csWrittenTotal) /* Captured something? */
            rc = AudioMixBufMixToParent(&pHstStrmIn->MixBuf, csWrittenTotal, &cCaptured);

        if (RT_SUCCESS(rc))
        {
            pDSoundStrmIn->csCaptureReadPos = (pDSoundStrmIn->csCaptureReadPos + cCaptured) % pDSoundStrmIn->csCaptureBufferSize;
            DSLOGF(("DSound: Capture %ld (%ld+%ld), processed %RU32/%RU32\n",
                    csCaptured, len1, len2, cCaptured, csWrittenTotal));
        }

    } while (0);

    if (RT_FAILURE(rc))
    {
        dsoundUpdateStatusInternal(pThis);
    }
    else
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = cCaptured;
    }

    LogFlowFuncLeaveRC(rc);
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

/** @todo Replace PDMAUDIODIR with a (registered? unique) channel ID to provide multi-channel input/output. */
static DECLCALLBACK(bool) drvHostDSoundIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    AssertPtrReturn(pInterface, false);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    if (enmDir == PDMAUDIODIR_IN)
        return pThis->fEnabledIn;

    return pThis->fEnabledOut;
}

static DECLCALLBACK(int) drvHostDSoundGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    dsoundUpdateStatusInternalEx(pThis, pCfg, 0 /* fEnum */);

    return VINF_SUCCESS;
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS
static int dsoundNotifyThread(PDRVHOSTDSOUND pThis, bool fShutdown)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (fShutdown)
    {
        LogFlowFunc(("Shutting down thread ...\n"));
        pThis->fShutdown = fShutdown;
    }

    /* Set the notification event so that the thread is being notified. */
    BOOL fRc = SetEvent(pThis->aEvents[DSOUNDEVENT_NOTIFY]);
    Assert(fRc);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostDSoundThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PDRVHOSTDSOUND pThis = (PDRVHOSTDSOUND)pvUser;
    AssertPtr(pThis);

    LogFlowFuncEnter();

    /* Let caller know that we're done initializing, regardless of the result. */
    int rc = RTThreadUserSignal(hThreadSelf);
    AssertRC(rc);

    do
    {
        HANDLE aEvents[VBOX_DSOUND_MAX_EVENTS];
        DWORD  cEvents = 0;
        for (uint8_t i = 0; i < VBOX_DSOUND_MAX_EVENTS; i++)
        {
            if (pThis->aEvents[i])
                aEvents[cEvents++] = pThis->aEvents[i];
        }
        Assert(cEvents);

        LogFlowFunc(("Waiting: cEvents=%ld\n", cEvents));

        DWORD dwObj = WaitForMultipleObjects(cEvents, aEvents, FALSE /* bWaitAll */, INFINITE);
        switch (dwObj)
        {
            case WAIT_FAILED:
            {
                rc = VERR_CANCELLED;
                break;
            }

            case WAIT_TIMEOUT:
            {
                rc = VERR_TIMEOUT;
                break;
            }

            default:
            {
                dwObj = WAIT_OBJECT_0 + cEvents - 1;
                if (aEvents[dwObj] == pThis->aEvents[DSOUNDEVENT_NOTIFY])
                {
                    LogFlowFunc(("Notify\n"));
                }
                else if (aEvents[dwObj] == pThis->aEvents[DSOUNDEVENT_INPUT])
                {

                }
                else if (aEvents[dwObj] == pThis->aEvents[DSOUNDEVENT_OUTPUT])
                {
                    DWORD cbBuffer, cbFree, cbPlayPos;
                    rc = dsoundGetPosOut(pThis->pDSStrmOut, &cbBuffer, &cbFree, &cbPlayPos);
                    if (   RT_SUCCESS(rc)
                        && cbFree)
                    {
                        PDMAUDIOCALLBACKDATAOUT Out;
                        Out.cbInFree     = cbFree;
                        Out.cbOutWritten = 0;

                        while (!Out.cbOutWritten)
                        {
                            rc = pThis->pUpIAudioConnector->pfnCallback(pThis->pUpIAudioConnector,
                                                                        PDMAUDIOCALLBACKTYPE_OUTPUT, &Out, sizeof(Out));
                            if (RT_FAILURE(rc))
                                break;
                            RTThreadSleep(100);
                        }

                        LogFlowFunc(("Output: cbBuffer=%ld, cbFree=%ld, cbPlayPos=%ld, cbWritten=%RU32, rc=%Rrc\n",
                                     cbBuffer, cbFree, cbPlayPos, Out.cbOutWritten, rc));
                    }
                }
                break;
            }
        }

        if (pThis->fShutdown)
            break;

    } while (RT_SUCCESS(rc));

    pThis->fStopped = true;

    LogFlowFunc(("Exited with fShutdown=%RTbool, rc=%Rrc\n", pThis->fShutdown, rc));
    return rc;
}
#endif /* VBOX_WITH_AUDIO_CALLBACKS */

static DECLCALLBACK(void) drvHostDSoundShutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    LogFlowFuncEnter();

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    int rc = dsoundNotifyThread(pThis, true /* fShutdown */);
    AssertRC(rc);

    int rcThread;
    rc = RTThreadWait(pThis->Thread,  15 * 1000 /* 15s timeout */, &rcThread);
    LogFlowFunc(("rc=%Rrc, rcThread=%Rrc\n", rc, rcThread));

    Assert(pThis->fStopped);

    if (pThis->aEvents[DSOUNDEVENT_NOTIFY])
    {
        CloseHandle(pThis->aEvents[DSOUNDEVENT_NOTIFY]);
        pThis->aEvents[DSOUNDEVENT_NOTIFY] = NULL;
}
#endif

    LogFlowFuncLeave();
}

static DECLCALLBACK(int) drvHostDSoundInit(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    LogFlowFuncEnter();

    int rc;

    /* Verify that IDirectSound is available. */
    LPDIRECTSOUND pDirectSound = NULL;
    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_ALL,
                                  IID_IDirectSound, (void **)&pDirectSound);
    if (SUCCEEDED(hr))
    {
        IDirectSound_Release(pDirectSound);

#ifdef VBOX_WITH_AUDIO_CALLBACKS
        /* Create notification event. */
        pThis->aEvents[DSOUNDEVENT_NOTIFY] = CreateEvent(NULL /* Security attribute */,
                                                         FALSE /* bManualReset */, FALSE /* bInitialState */,
                                                         NULL /* lpName */);
        Assert(pThis->aEvents[DSOUNDEVENT_NOTIFY] != NULL);

        /* Start notification thread. */
        rc = RTThreadCreate(&pThis->Thread, drvHostDSoundThread,
                            pThis /*pvUser*/, 0 /*cbStack*/,
                            RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "dSoundNtfy");
        if (RT_SUCCESS(rc))
        {
            /* Wait for the thread to initialize. */
            rc = RTThreadUserWait(pThis->Thread, RT_MS_1MIN);
            if (RT_FAILURE(rc))
                DSLOGREL(("DSound: Waiting for thread to initialize failed with rc=%Rrc\n", rc));
        }
    else
            DSLOGREL(("DSound: Creating thread failed with rc=%Rrc\n", rc));
#else
        rc = VINF_SUCCESS;
#endif

        dsoundUpdateStatusInternalEx(pThis, NULL /* pCfg */, DSOUNDENUMCBFLAGS_LOG /* fEnum */);
    }
    else
    {
        DSLOGREL(("DSound: DirectSound not available: %Rhrc\n", hr));
        rc = VERR_NOT_SUPPORTED;
    }

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
        DSLOGREL(("DSound: CoInitializeEx failed with %Rhrc\n", hr));
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Init basic data members and interfaces.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostDSoundQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostDSound);

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /*
     * Get the IAudioConnector interface of the above driver/device.
     */
    pThis->pUpIAudioConnector = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    if (!pThis->pUpIAudioConnector)
    {
        AssertMsgFailed(("Configuration error: No audio connector interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }
#endif

    /*
     * Init the static parts.
     */
    RTListInit(&pThis->lstDevInput);
    RTListInit(&pThis->lstDevOutput);

    pThis->fEnabledIn  = false;
    pThis->fEnabledOut = false;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    pThis->fStopped    = false;
    pThis->fShutdown   = false;

    RT_ZERO(pThis->aEvents);
    pThis->cEvents = 0;
#endif

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
