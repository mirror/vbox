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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <iprt/win/windows.h>
#include <dsound.h>

#include <iprt/alloc.h>
#include <iprt/uuid.h>

#include "AudioMixBuffer.h"
#include "DrvAudio.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
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
#define DSLOG(a)    do { LogRel2(a); } while(0)
/* Something which produce a lot of logging during playback/recording. */
#define DSLOGF(a)   do { LogRel3(a); } while(0)
/* Important messages like errors. Limited in the default release log to avoid log flood. */
#define DSLOGREL(a) \
    do {  \
        static int8_t s_cLogged = 0; \
        if (s_cLogged < 8) { \
            ++s_cLogged; \
            LogRel(a); \
        } else DSLOG(a); \
    } while (0)


/** Maximum number of attempts to restore the sound buffer before giving up. */
#define DRV_DSOUND_RESTORE_ATTEMPTS_MAX         3

/** Makes DRVHOSTDSOUND out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface) \
    ( (PDRVHOSTDSOUND)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTDSOUND, IHostAudio)) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/* Dynamically load dsound.dll. */
typedef HRESULT WINAPI FNDIRECTSOUNDENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
typedef FNDIRECTSOUNDENUMERATEW *PFNDIRECTSOUNDENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTUREENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
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
    /** Associated host output stream.
     *  Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    LPDIRECTSOUND8       pDS;     /** @todo Move this out of this structure! Not required per-stream (e.g. for multi-channel). */
    LPDIRECTSOUNDBUFFER8 pDSB;
    DWORD                offPlayWritePos;
    DWORD                cMaxSamplesInBuffer;
    bool                 fEnabled;
    bool                 fRestartPlayback;
    PDMAUDIOSTREAMCFG    streamCfg;
} DSOUNDSTREAMOUT, *PDSOUNDSTREAMOUT;

typedef struct DSOUNDSTREAMIN
{
    /** Associated host input stream.
     *  Note: Always must come first! */
    PDMAUDIOSTREAM              Stream;
    LPDIRECTSOUNDCAPTURE8       pDSC;
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB;
    DWORD                       idxSampleCaptureReadPos;
    DWORD                       cMaxSamplesInBuffer;
    HRESULT                     hrLastCapture;
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
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static HRESULT  directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB);
static void     dsoundDeviceRemove(PDSOUNDDEV pDev);
static int      dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg);
#ifdef VBOX_WITH_AUDIO_CALLBACKS
static int      dsoundNotifyThread(PDRVHOSTDSOUND pThis, bool fShutdown);
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
        case PDMAUDIOFMT_S8:
        case PDMAUDIOFMT_U8:
            pFmt->wBitsPerSample = 8;
            break;

        case PDMAUDIOFMT_S16:
        case PDMAUDIOFMT_U16:
            pFmt->wBitsPerSample = 16;
            pFmt->nAvgBytesPerSec <<= 1;
            pFmt->nBlockAlign <<= 1;
            break;

        case PDMAUDIOFMT_S32:
        case PDMAUDIOFMT_U32:
            pFmt->wBitsPerSample = 32;
            pFmt->nAvgBytesPerSec <<= 2;
            pFmt->nBlockAlign <<= 2;
            break;

        default:
            AssertMsgFailed(("Wave format %d not supported\n", pCfg->enmFormat));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}


static int dsoundGetPosOut(PDRVHOSTDSOUND   pThis,
                           PDSOUNDSTREAMOUT pDSoundStream, DWORD *pdwBuffer, DWORD *pdwFree, DWORD *pdwPlayPos)
{
    AssertPtr(pThis);
    AssertPtrReturn(pDSoundStream, VERR_INVALID_POINTER);
    AssertPtrNull(pdwBuffer);
    AssertPtrNull(pdwFree);
    AssertPtrNull(pdwPlayPos);

    LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStream->pDSB;
    if (!pDSB)
        return VERR_INVALID_POINTER;

    /* Get the current play position which is used for calculating the free space in the buffer. */
    DWORD cbPlayPos;
    HRESULT hr = E_FAIL;
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, NULL);
        if (SUCCEEDED(hr))
        {
            DWORD const cbBuffer = AUDIOMIXBUF_S2B(&pDSoundStream->Stream.MixBuf, pDSoundStream->cMaxSamplesInBuffer);
            if (pdwBuffer)
                *pdwBuffer  = cbBuffer;
            if (pdwFree)
                *pdwFree    = cbBuffer - dsoundRingDistance(pDSoundStream->offPlayWritePos, cbPlayPos, cbBuffer);
            if (pdwPlayPos)
                *pdwPlayPos = cbPlayPos;
            return VINF_SUCCESS;
        }
        if (hr != DSERR_BUFFERLOST) /** @todo MSDN doesn't state this error for GetCurrentPosition(). */
            break;
        LogFlowFunc(("Getting playing position failed due to lost buffer, restoring ...\n"));
        directSoundPlayRestore(pThis, pDSB);
    }

    if (hr != DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
        DSLOGREL(("DSound: Getting current playback position failed with %Rhrc\n", hr));
    LogFlowFunc(("Failed with %Rhrc\n", hr));

    int rc = VERR_NOT_AVAILABLE;
    LogFlowFuncLeaveRC(rc);
    return rc;
}


static char *dsoundGUIDToUtf8StrA(LPCGUID pGUID)
{
    if (pGUID)
    {
        LPOLESTR lpOLEStr;
        HRESULT hr = StringFromCLSID(*pGUID, &lpOLEStr);
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
    RT_NOREF(pThis);
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (FAILED(hr))
        DSLOGREL(("DSound: Restoring playback buffer failed with %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundPlayUnlock(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB,
                                     PVOID pv1, PVOID pv2,
                                     DWORD cb1, DWORD cb2)
{
    RT_NOREF(pThis);
    HRESULT hr = IDirectSoundBuffer8_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking playback buffer failed with %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureUnlock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB,
                                        PVOID pv1, PVOID pv2,
                                        DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer8_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (FAILED(hr))
        DSLOGREL(("DSound: Unlocking capture buffer failed with %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundPlayLock(PDRVHOSTDSOUND pThis,
                                   LPDIRECTSOUNDBUFFER8 pDSB, PPDMAUDIOPCMPROPS pProps,
                                   DWORD dwOffset, DWORD dwBytes,
                                   PVOID *ppv1, PVOID *ppv2,
                                   DWORD *pcb1, DWORD *pcb2,
                                   DWORD dwFlags)
{
    HRESULT hr = E_FAIL;
    AssertCompile(DRV_DSOUND_RESTORE_ATTEMPTS_MAX > 0);
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        *ppv1 = *ppv2 = NULL;
        *pcb1 = *pcb2 = 0;
        hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, ppv1, pcb1, ppv2, pcb2, dwFlags);
        if (SUCCEEDED(hr))
        {
            if (   (!*ppv1 || !(*pcb1 & pProps->uAlign))
                && (!*ppv2 || !(*pcb2 & pProps->uAlign)) )
                return S_OK;
            DSLOGREL(("DSound: Locking playback buffer returned misaligned buffer: cb1=%#RX32, cb2=%#RX32 (alignment: %#RX32)\n",
                      *pcb1, *pcb2, pProps->uAlign));
            directSoundPlayUnlock(pThis, pDSB, *ppv1, *ppv2, *pcb1, *pcb2);
            return E_FAIL;
        }

        if (hr != DSERR_BUFFERLOST)
            break;

        LogFlowFunc(("Locking failed due to lost buffer, restoring ...\n"));
        directSoundPlayRestore(pThis, pDSB);
    }

    DSLOGREL(("DSound: Locking playback buffer failed with %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureLock(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB, PPDMAUDIOPCMPROPS pProps,
                                      DWORD dwOffset, DWORD dwBytes,
                                      PVOID *ppv1, PVOID *ppv2,
                                      DWORD *pcb1, DWORD *pcb2,
                                      DWORD dwFlags)
{
    PVOID pv1 = NULL;
    PVOID pv2 = NULL;
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

static void directSoundPlayInterfaceRelease(PDSOUNDSTREAMOUT pDSoundStream)
{
    if (pDSoundStream->pDS)
    {
        IDirectSound8_Release(pDSoundStream->pDS);
        pDSoundStream->pDS = NULL;
    }
}


static HRESULT directSoundPlayInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    if (pDSoundStream->pDS != NULL)
    {
        DSLOG(("DSound: DirectSound instance already exists\n"));
        return S_OK;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_ALL,
                                  IID_IDirectSound8, (void **)&pDSoundStream->pDS);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating playback instance failed with %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pDSoundStream->pDS, pThis->cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pDSoundStream->pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                DSLOGREL(("DSound: Setting cooperative level for window %p failed with %Rhrc\n", hWnd, hr));
        }

        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no playback devices are attached. */
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSound playback initialization failed with %Rhrc\n", hr));

            directSoundPlayInterfaceRelease(pDSoundStream);
        }
    }

    return hr;
}


static HRESULT directSoundPlayClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    DSLOG(("DSound: Closing playback stream %p, buffer %p\n", pDSoundStream, pDSoundStream->pDSB));

    HRESULT hr = S_OK;

    if (pDSoundStream->pDSB)
    {
        hr = IDirectSoundBuffer8_Stop(pDSoundStream->pDSB);
        if (SUCCEEDED(hr))
        {
#ifdef VBOX_WITH_AUDIO_CALLBACKS
            if (pThis->aEvents[DSOUNDEVENT_OUTPUT] != NULL)
            {
                CloseHandle(pThis->aEvents[DSOUNDEVENT_OUTPUT]);
                pThis->aEvents[DSOUNDEVENT_OUTPUT] = NULL;

                if (pThis->cEvents)
                    pThis->cEvents--;

                pThis->pDSStream = NULL;
            }

            int rc2 = dsoundNotifyThread(pThis, false /* fShutdown */);
            AssertRC(rc2);
#endif
            IDirectSoundBuffer8_Release(pDSoundStream->pDSB);
            pDSoundStream->pDSB = NULL;
        }
        else
            DSLOGREL(("DSound: Stop playback stream %p when closing %Rhrc\n", pDSoundStream, hr));
    }

    if (SUCCEEDED(hr))
        directSoundPlayInterfaceRelease(pDSoundStream);

    return hr;
}


static HRESULT directSoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    DSLOG(("DSound: pDSoundStream=%p, cbBufferOut=%RU32, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
           pDSoundStream,
           pThis->cfg.cbBufferOut,
           pDSoundStream->Stream.Props.uHz,
           pDSoundStream->Stream.Props.cChannels,
           pDSoundStream->Stream.Props.cBits,
           pDSoundStream->Stream.Props.fSigned));

    if (pDSoundStream->pDSB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: Playback buffer already exists\n"));
        directSoundPlayClose(pThis, pDSoundStream);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStream->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    HRESULT hr = directSoundPlayInterfaceCreate(pThis, pDSoundStream);
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

        hr = IDirectSound8_CreateSoundBuffer(pDSoundStream->pDS, &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Creating playback sound buffer failed with %Rhrc\n", hr));
            break;
        }

        /* "Upgrade" to IDirectSoundBuffer8 interface. */
        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (PVOID *)&pDSoundStream->pDSB);
        IDirectSoundBuffer_Release(pDSB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying playback sound buffer interface failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        hr = IDirectSoundBuffer8_GetFormat(pDSoundStream->pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback format failed with %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundBuffer8_GetCaps(pDSoundStream->pDSB, &bc);
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

        if (bc.dwBufferBytes & pDSoundStream->Stream.Props.uAlign)
            DSLOGREL(("DSound: Playback capabilities returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pDSoundStream->Stream.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferOut)
            DSLOGREL(("DSound: Playback buffer size mismatched: DirectSound %RU32, requested %RU32 bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferOut));

        /*
         * Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pDSoundStream->cMaxSamplesInBuffer = bc.dwBufferBytes >> pDSoundStream->Stream.Props.cShift;
        DSLOG(("DSound: cMaxSamplesInBuffer=%RU32\n", pDSoundStream->cMaxSamplesInBuffer));

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
        hr = IDirectSoundNotify_QueryInterface(pDSoundStream->pDSB, IID_IDirectSoundNotify8, (PVOID *)&pNotify);
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

        pThis->pDSStreamOut = pDSoundStream;

        Assert(pThis->cEvents < VBOX_DSOUND_MAX_EVENTS);
        pThis->cEvents++;

        /* Let the thread know. */
        dsoundNotifyThread(pThis, false /* fShutdown */);

        /* Trigger the just installed output notification. */
        hr = IDirectSoundBuffer8_Play(pDSoundStream->pDSB, 0, 0, 0);

#endif /* VBOX_WITH_AUDIO_CALLBACKS */

    } while (0);

    if (FAILED(hr))
        directSoundPlayClose(pThis, pDSoundStream);

    return hr;
}


static void dsoundPlayClearSamples(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    AssertPtrReturnVoid(pDSoundStream);

    PPDMAUDIOSTREAM pStream = &pDSoundStream->Stream;

    PVOID pv1, pv2;
    DWORD cb1, cb2;
    HRESULT hr = directSoundPlayLock(pThis, pDSoundStream->pDSB, &pDSoundStream->Stream.Props,
                                     0 /* dwOffset */, AUDIOMIXBUF_S2B(&pStream->MixBuf, pDSoundStream->cMaxSamplesInBuffer),
                                     &pv1, &pv2, &cb1, &cb2, DSBLOCK_ENTIREBUFFER);
    if (SUCCEEDED(hr))
    {
        DWORD len1 = AUDIOMIXBUF_B2S(&pStream->MixBuf, cb1);
        DWORD len2 = AUDIOMIXBUF_B2S(&pStream->MixBuf, cb2);

        if (pv1 && len1)
            DrvAudioHlpClearBuf(&pDSoundStream->Stream.Props, pv1, cb1, len1);

        if (pv2 && len2)
            DrvAudioHlpClearBuf(&pDSoundStream->Stream.Props, pv2, cb2, len2);

        directSoundPlayUnlock(pThis, pDSoundStream->pDSB, pv1, pv2, cb1, cb2);
    }
}


static HRESULT directSoundPlayGetStatus(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pdwStatus)
{
    AssertPtr(pThis);
    AssertPtrReturn(pDSB,  E_POINTER);
    AssertPtrNull(pdwStatus);

    DWORD dwStatus = 0;
    HRESULT hr = E_FAIL;
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


static HRESULT directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    AssertPtrReturn(pThis,         E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    HRESULT hr;

    if (pDSoundStream->pDSB != NULL)
    {
        DSLOG(("DSound: Stopping playback\n"));

        HRESULT hr2 = IDirectSoundBuffer8_Stop(pDSoundStream->pDSB);
        if (FAILED(hr2))
        {
            hr2 = directSoundPlayRestore(pThis, pDSoundStream->pDSB);
            if (FAILED(hr2))
                hr2 = IDirectSoundBuffer8_Stop(pDSoundStream->pDSB);
        }

        if (FAILED(hr2))
            DSLOG(("DSound: Stopping playback failed with %Rhrc\n", hr2));

        hr = S_OK; /* Always report success here. */
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
    {
        dsoundPlayClearSamples(pThis, pDSoundStream);
        pDSoundStream->fEnabled = false;
    }
    else
        DSLOGREL(("DSound: Stopping playback failed with %Rhrc\n", hr));

    return hr;
}


static HRESULT directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStream)
{
    AssertPtrReturn(pThis,         E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    HRESULT hr;
    if (pDSoundStream->pDSB != NULL)
    {
        DWORD dwStatus;
        hr = directSoundPlayGetStatus(pThis, pDSoundStream->pDSB, &dwStatus);
        if (SUCCEEDED(hr))
        {
            if (dwStatus & DSBSTATUS_PLAYING)
            {
                DSLOG(("DSound: Already playing\n"));
            }
            else
            {
                dsoundPlayClearSamples(pThis, pDSoundStream);

                pDSoundStream->fRestartPlayback = true;
                pDSoundStream->fEnabled         = true;

                DSLOG(("DSound: Playback started\n"));

                /*
                 * The actual IDirectSoundBuffer8_Play call will be made in drvHostDSoundPlay,
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

static LPCGUID dsoundCaptureSelectDevice(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStream)
{
    AssertPtrReturn(pThis, NULL);
    AssertPtrReturn(pDSoundStream, NULL);

    int rc = VINF_SUCCESS;

    LPCGUID pGUID = pThis->cfg.pGuidCapture;
    if (!pGUID)
    {
        PDSOUNDDEV pDev = NULL;

        switch (pDSoundStream->enmRecSource)
        {
            case PDMAUDIORECSOURCE_LINE:
                /*
                 * At the moment we're only supporting line-in in the HDA emulation,
                 * and line-in + mic-in in the AC'97 emulation both are expected
                 * to use the host's mic-in as well.
                 *
                 * So the fall through here is intentional for now.
                 */
            case PDMAUDIORECSOURCE_MIC:
            {
                RTListForEach(&pThis->lstDevInput, pDev, DSOUNDDEV, Node)
                {
                    if (RTStrIStr(pDev->pszName, "Mic")) /** @todo what is with non en_us windows versions? */
                        break;
                }

                if (RTListNodeIsDummy(&pThis->lstDevInput, pDev, DSOUNDDEV, Node))
                    pDev = NULL; /* Found nothing. */

                break;
            }

            default:
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
                break;
        }

        if (   RT_SUCCESS(rc)
            && pDev)
        {
            DSLOG(("DSound: Guest source '%s' is using host recording device '%s'\n",
                   DrvAudioHlpRecSrcToStr(pDSoundStream->enmRecSource), pDev->pszName));

            pGUID = &pDev->Guid;
        }
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("DSound: Selecting recording device failed with %Rrc\n", rc));
        return NULL;
    }

    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);

    /* This always has to be in the release log. */
    LogRel(("DSound: Guest source '%s' is using host recording device with GUID '%s'\n",
            DrvAudioHlpRecSrcToStr(pDSoundStream->enmRecSource), pszGUID ? pszGUID: "{?}"));

    if (pszGUID)
    {
        RTStrFree(pszGUID);
        pszGUID = NULL;
    }

    return pGUID;
}


static void directSoundCaptureInterfaceRelease(PDSOUNDSTREAMIN pDSoundStream)
{
    if (pDSoundStream->pDSC)
    {
        LogFlowFuncEnter();
        IDirectSoundCapture_Release(pDSoundStream->pDSC);
        pDSoundStream->pDSC = NULL;
    }
}


static HRESULT directSoundCaptureInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStream)
{
    if (pDSoundStream->pDSC != NULL)
    {
        DSLOG(("DSound: DirectSoundCapture instance already exists\n"));
        return S_OK;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture8, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture8, (void **)&pDSoundStream->pDSC);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating capture instance failed with %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pDSoundStream);
        hr = IDirectSoundCapture_Initialize(pDSoundStream->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no capture devices are attached. */
                DSLOGREL(("DSound: Capture device currently is unavailable\n"));
            else
                DSLOGREL(("DSound: Initializing capturing device failed with %Rhrc\n", hr));

            directSoundCaptureInterfaceRelease(pDSoundStream);
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureClose(PDSOUNDSTREAMIN pDSoundStream)
{
    AssertPtrReturn(pDSoundStream, E_POINTER);

    DSLOG(("DSound: pDSoundStream=%p, pDSCB=%p\n", pDSoundStream, pDSoundStream->pDSCB));

    HRESULT hr = S_OK;

    if (pDSoundStream->pDSCB)
    {
        hr = IDirectSoundCaptureBuffer_Stop(pDSoundStream->pDSCB);
        if (SUCCEEDED(hr))
        {
            IDirectSoundCaptureBuffer8_Release(pDSoundStream->pDSCB);
            pDSoundStream->pDSCB = NULL;
        }
        else
            DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));
    }

    if (SUCCEEDED(hr))
        directSoundCaptureInterfaceRelease(pDSoundStream);

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStream)
{
    AssertPtrReturn(pThis, E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    DSLOG(("DSound: pDSoundStream=%p, cbBufferIn=%RU32, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
           pDSoundStream,
           pThis->cfg.cbBufferIn,
           pDSoundStream->Stream.Props.uHz,
           pDSoundStream->Stream.Props.cChannels,
           pDSoundStream->Stream.Props.cBits,
           pDSoundStream->Stream.Props.fSigned));

    if (pDSoundStream->pDSCB != NULL)
    {
        /* Should not happen but be forgiving. */
        DSLOGREL(("DSound: DirectSoundCaptureBuffer already exists\n"));
        directSoundCaptureClose(pDSoundStream);
    }

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(&pDSoundStream->streamCfg, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    HRESULT hr = directSoundCaptureInterfaceCreate(pThis, pDSoundStream);
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
        hr = IDirectSoundCapture_CreateCaptureBuffer(pDSoundStream->pDSC,
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

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pDSoundStream->pDSCB);
        IDirectSoundCaptureBuffer_Release(pDSCB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying interface for capture buffer failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters.
         */
        DWORD offByteReadPos = 0;
        hr = IDirectSoundCaptureBuffer8_GetCurrentPosition(pDSoundStream->pDSCB, NULL, &offByteReadPos);
        if (FAILED(hr))
        {
            offByteReadPos = 0;
            DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pDSoundStream->pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture format failed with %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pDSoundStream->pDSCB, &bc);
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

        if (bc.dwBufferBytes & pDSoundStream->Stream.Props.uAlign)
            DSLOGREL(("DSound: Capture GetCaps returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pDSoundStream->Stream.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferIn)
            DSLOGREL(("DSound: Capture buffer size mismatched: DirectSound %RU32, requested %RU32 bytes\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferIn));

        /* Initial state: reading at the initial capture position, no error. */
        pDSoundStream->idxSampleCaptureReadPos    = offByteReadPos >> pDSoundStream->Stream.Props.cShift;
        pDSoundStream->cMaxSamplesInBuffer = bc.dwBufferBytes >> pDSoundStream->Stream.Props.cShift;
        pDSoundStream->hrLastCapture       = S_OK;

        DSLOG(("DSound: idxSampleCaptureReadPos=%RU32, cMaxSamplesInBuffer=%RU32\n",
                     pDSoundStream->idxSampleCaptureReadPos, pDSoundStream->cMaxSamplesInBuffer));

    } while (0);

    if (FAILED(hr))
        directSoundCaptureClose(pDSoundStream);

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStream)
{
    AssertPtrReturn(pThis        , E_POINTER);
    AssertPtrReturn(pDSoundStream, E_POINTER);

    NOREF(pThis);

    HRESULT hr;

    if (pDSoundStream->pDSCB)
    {
        DSLOG(("DSound: Stopping capture\n"));

        hr = IDirectSoundCaptureBuffer_Stop(pDSoundStream->pDSCB);
        if (FAILED(hr))
            DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
        pDSoundStream->fEnabled = false;

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}


static HRESULT directSoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStream)
{
    AssertPtrReturn(pThis,         VERR_INVALID_POINTER);
    AssertPtrReturn(pDSoundStream, VERR_INVALID_POINTER);

    HRESULT hr;
    if (pDSoundStream->pDSCB != NULL)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer8_GetStatus(pDSoundStream->pDSCB, &dwStatus);
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
                hr = IDirectSoundCaptureBuffer8_Start(pDSoundStream->pDSCB, fFlags);
                if (FAILED(hr))
                    DSLOGREL(("DSound: Starting to capture failed with %Rhrc\n", hr));
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    if (SUCCEEDED(hr))
        pDSoundStream->fEnabled = true;

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}


static int dsoundDevAdd(PRTLISTANCHOR pList, LPGUID pGUID, LPCWSTR pwszDescription, PDSOUNDDEV *ppDev)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pGUID, VERR_INVALID_POINTER);
    AssertPtrReturn(pwszDescription, VERR_INVALID_POINTER);

    PDSOUNDDEV pDev = (PDSOUNDDEV)RTMemAlloc(sizeof(DSOUNDDEV));
    if (!pDev)
        return VERR_NO_MEMORY;

    int rc = RTUtf16ToUtf8(pwszDescription, &pDev->pszName);
    if (RT_SUCCESS(rc))
        memcpy(&pDev->Guid, pGUID, sizeof(GUID));

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


static void dsoundLogDevice(const char *pszType, LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule)
{
    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);
    /* This always has to be in the release log. */
    LogRel(("DSound: %s: GUID: %s [%ls] (Module: %ls)\n", pszType, pszGUID ? pszGUID : "{?}", pwszDescription, pwszModule));
    RTStrFree(pszGUID);
}


static BOOL CALLBACK dsoundDevicesEnumCbPlayback(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!pGUID)
        return TRUE;

    AssertPtrReturn(pwszDescription, FALSE);
    /* Do not care about pwszModule. */

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Output", pGUID, pwszDescription, pwszModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevOutput,
                          pGUID, pwszDescription, NULL /* ppDev */);
    if (RT_FAILURE(rc))
        return FALSE; /* Abort enumeration. */

    pCtx->cDevOut++;

    return TRUE;
}


static BOOL CALLBACK dsoundDevicesEnumCbCapture(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);

    if (!pGUID)
        return TRUE;

    if (pCtx->fFlags & DSOUNDENUMCBFLAGS_LOG)
        dsoundLogDevice("Input", pGUID, pwszDescription, pwszModule);

    int rc = dsoundDevAdd(&pCtx->pDrv->lstDevInput,
                          pGUID, pwszDescription, NULL /* ppDev */);
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
static void dsoundUpdateStatusInternalEx(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDCFG pCfg, uint32_t fEnum)
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


static void dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis)
{
    dsoundUpdateStatusInternalEx(pThis, NULL /* pCfg */, 0 /* fEnum */);
}


static int dsoundCreateStreamOut(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAM pStream,
                                 PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    LogFlowFunc(("pStream=%p, pCfg=%p\n", pStream, pCfgReq));
    PDSOUNDSTREAMOUT pDSoundStream = (PDSOUNDSTREAMOUT)pStream;

    pDSoundStream->streamCfg = *pCfgReq;
    pDSoundStream->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

    int rc = DrvAudioHlpStreamCfgToProps(&pDSoundStream->streamCfg, &pDSoundStream->Stream.Props);
    if (RT_SUCCESS(rc))
    {
        pDSoundStream->pDS = NULL;
        pDSoundStream->pDSB = NULL;
        pDSoundStream->offPlayWritePos = 0;
        pDSoundStream->fRestartPlayback = true;
        pDSoundStream->cMaxSamplesInBuffer = 0;

        if (pCfgAcq)
            pCfgAcq->cSamples = pThis->cfg.cbBufferOut >> pStream->Props.cShift; /** @todo Get rid of using Props! */

        /* Try to open playback in case the device is already there. */
        directSoundPlayOpen(pThis, pDSoundStream);
    }
    else
        RT_ZERO(pDSoundStream->streamCfg);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int dsoundControlStreamOut(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStream=%p, cmd=%d\n", pStream, enmStreamCmd));
    PDSOUNDSTREAMOUT pDSoundStream = (PDSOUNDSTREAMOUT)pStream;

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_ENABLE\n"));
            /* Try to start playback. If it fails, then reopen and try again. */
            hr = directSoundPlayStart(pThis, pDSoundStream);
            if (FAILED(hr))
            {
                hr = directSoundPlayClose(pThis, pDSoundStream);
                if (SUCCEEDED(hr))
                    hr = directSoundPlayOpen(pThis, pDSoundStream);
                if (SUCCEEDED(hr))
                    hr = directSoundPlayStart(pThis, pDSoundStream);
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            DSLOG(("DSound: Playback PDMAUDIOSTREAMCMD_DISABLE\n"));
            hr = directSoundPlayStop(pThis, pDSoundStream);
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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
int drvHostDSoundStreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF2(pvBuf, cbBuf);

    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    PDRVHOSTDSOUND   pThis         = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAMOUT pDSoundStream = (PDSOUNDSTREAMOUT)pStream;

    int rc = VINF_SUCCESS;
    uint32_t cReadTotal = 0;

#ifdef DEBUG_andy
    LogFlowFuncEnter();
#endif

    do /* to use 'break' */
    {
        DWORD cbBuffer, cbFree, cbPlayPos;
        rc = dsoundGetPosOut(pThis, pDSoundStream, &cbBuffer, &cbFree, &cbPlayPos);
        if (RT_FAILURE(rc))
            break;

        /*
         * Check for full buffer, do not allow the offPlayWritePos to catch cbPlayPos during playback,
         * i.e. always leave a free space for 1 audio sample.
         */
        const DWORD cbSample = AUDIOMIXBUF_S2B(&pStream->MixBuf, 1);
        if (cbFree <= cbSample)
            break;
        cbFree     -= cbSample;

        uint32_t cLive  = AudioMixBufLive(&pStream->MixBuf);
        uint32_t cbLive = AUDIOMIXBUF_S2B(&pStream->MixBuf, cLive);

        /* Do not write more than available space in the DirectSound playback buffer. */
        cbLive = RT_MIN(cbFree, cbLive);
        cbLive &= ~pStream->Props.uAlign;
        if (cbLive == 0 || cbLive > cbBuffer)
        {
            DSLOG(("DSound: cbLive=%RU32, cbBuffer=%ld, offPlayWritePos=%ld, cbPlayPos=%ld\n",
                   cbLive, cbBuffer, pDSoundStream->offPlayWritePos, cbPlayPos));
            break;
        }

        LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStream->pDSB;
        AssertPtr(pDSB);

        PVOID pv1, pv2;
        DWORD cb1, cb2;
        HRESULT hr = directSoundPlayLock(pThis, pDSB, &pStream->Props, pDSoundStream->offPlayWritePos, cbLive,
                                         &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
        if (FAILED(hr))
        {
            rc = VERR_ACCESS_DENIED;
            break;
        }

        /** @todo r=bird: Can pv1/cb1 really be NULL? Docs says they're always set
         *        and pv2/cb2 only used when there is a buffer wrap araound. */

        DWORD cSamplesIn1 = AUDIOMIXBUF_B2S(&pStream->MixBuf, cb1);
        uint32_t cRead = 0;

        if (pv1 && cb1)
        {
            rc = AudioMixBufReadCirc(&pStream->MixBuf, pv1, cb1, &cRead);
            if (RT_SUCCESS(rc))
                cReadTotal += cRead;
        }

        if (   RT_SUCCESS(rc)
            && cReadTotal == cSamplesIn1
            && pv2 && cb2)
        {
            rc = AudioMixBufReadCirc(&pStream->MixBuf, pv2, cb2, &cRead);
            if (RT_SUCCESS(rc))
                cReadTotal += cRead;
        }

        directSoundPlayUnlock(pThis, pDSB, pv1, pv2, cb1, cb2);

        pDSoundStream->offPlayWritePos = (pDSoundStream->offPlayWritePos + AUDIOMIXBUF_S2B(&pStream->MixBuf, cReadTotal))
                                       % cbBuffer;

        DSLOGF(("DSound: %RU32 (%RU32 samples) out of %RU32%s, buffer write pos %ld, rc=%Rrc\n",
                AUDIOMIXBUF_S2B(&pStream->MixBuf, cReadTotal), cReadTotal, cbLive,
                cbLive != AUDIOMIXBUF_S2B(&pStream->MixBuf, cReadTotal) ? " !!!": "",
                pDSoundStream->offPlayWritePos, rc));

        if (cReadTotal)
        {
            AudioMixBufFinish(&pStream->MixBuf, cReadTotal);
            rc = VINF_SUCCESS; /* Played something. */
        }

        if (RT_FAILURE(rc))
            break;

        if (pDSoundStream->fRestartPlayback)
        {
            /*
             * The playback has been just started.
             * Some samples of the new sound have been copied to the buffer
             * and it can start playing.
             */
            pDSoundStream->fRestartPlayback = false;

            DWORD fFlags = 0;
#ifndef VBOX_WITH_AUDIO_CALLBACKS
            fFlags |= DSCBSTART_LOOPING;
#endif
            for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
            {
                hr = IDirectSoundBuffer8_Play(pDSoundStream->pDSB, 0, 0, fFlags);
                if (   SUCCEEDED(hr)
                    || hr != DSERR_BUFFERLOST)
                    break;
                else
                {
                    LogFlowFunc(("Restarting playback failed due to lost buffer, restoring ...\n"));
                    directSoundPlayRestore(pThis, pDSoundStream->pDSB);
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
        dsoundUpdateStatusInternal(pThis);
    else if (pcbWritten)
        *pcbWritten = cReadTotal;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int dsoundDestroyStreamOut(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAM pStream)
{
    PDSOUNDSTREAMOUT pDSoundStream = (PDSOUNDSTREAMOUT)pStream;

    directSoundPlayClose(pThis, pDSoundStream);

    pDSoundStream->offPlayWritePos      = 0;
    pDSoundStream->fRestartPlayback     = true;
    pDSoundStream->cMaxSamplesInBuffer  = 0;

    RT_ZERO(pDSoundStream->streamCfg);

    return VINF_SUCCESS;
}

static int dsoundCreateStreamIn(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAM pStream,
                                PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDSOUNDSTREAMIN pDSoundStream = (PDSOUNDSTREAMIN)pStream;

    LogFlowFunc(("pStream=%p, pCfg=%p, enmRecSource=%ld\n", pStream, pCfgReq, pCfgReq->DestSource.Source));

    memcpy(&pDSoundStream->streamCfg, pCfgReq, sizeof(PDMAUDIOSTREAMCFG));

    /** @todo caller should already init Props? */
    int rc = DrvAudioHlpStreamCfgToProps(&pDSoundStream->streamCfg, &pStream->Props); /** @todo Get rid of using Props! */
    if (RT_SUCCESS(rc))
    {
        /* Init the stream structure and save relevant information to it. */
        pDSoundStream->idxSampleCaptureReadPos    = 0;
        pDSoundStream->cMaxSamplesInBuffer = 0;
        pDSoundStream->pDSC                = NULL;
        pDSoundStream->pDSCB               = NULL;
        pDSoundStream->enmRecSource        = pCfgReq->DestSource.Source;
        pDSoundStream->hrLastCapture       = S_OK;

        if (pCfgAcq)
            pCfgAcq->cSamples = pThis->cfg.cbBufferIn >> pStream->Props.cShift; /** @todo Get rid of using Props! */

        /* Try to open capture in case the device is already there. */
        directSoundCaptureOpen(pThis, pDSoundStream); /** @todo r=andy Why not checking the result here?? */
    }
    else
    {
        RT_ZERO(pDSoundStream->streamCfg);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int dsoundControlStreamIn(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStream=%p, enmStreamCmd=%ld\n", pStream, enmStreamCmd));
    PDSOUNDSTREAMIN pDSoundStream = (PDSOUNDSTREAMIN)pStream;

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Try to start capture. If it fails, then reopen and try again. */
            hr = directSoundCaptureStart(pThis, pDSoundStream);
            if (FAILED(hr))
            {
                hr = directSoundCaptureClose(pDSoundStream);
                if (SUCCEEDED(hr))
                {
                    hr = directSoundCaptureOpen(pThis, pDSoundStream);
                    if (SUCCEEDED(hr))
                        hr = directSoundCaptureStart(pThis, pDSoundStream);
                }
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            hr = directSoundCaptureStop(pThis, pDSoundStream);
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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
int drvHostDSoundStreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF2(pvBuf, cbBuf);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    PDSOUNDSTREAMIN             pDSoundStream = (PDSOUNDSTREAMIN)pStream;
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB         = pDSoundStream->pDSCB;

    int rc = VINF_SUCCESS;

    uint32_t cSamplesProcessed = 0;

    do
    {
        if (pDSCB == NULL)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        /* Get DirectSound capture position in bytes. */
        DWORD offByteReadPos;
        HRESULT hr = IDirectSoundCaptureBuffer_GetCurrentPosition(pDSCB, NULL, &offByteReadPos);
        if (FAILED(hr))
        {
            if (hr != pDSoundStream->hrLastCapture)
            {
                DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
                pDSoundStream->hrLastCapture = hr;
            }

            rc = VERR_NOT_AVAILABLE;
            break;
        }

        pDSoundStream->hrLastCapture = hr;

        if (offByteReadPos & pStream->Props.uAlign)
            DSLOGF(("DSound: Misaligned capture read position %ld (alignment: %RU32)\n", offByteReadPos, pStream->Props.uAlign));

        /* Capture position in samples. */
        DWORD idxSampleReadPos = offByteReadPos >> pStream->Props.cShift;

        /* Number of samples available in the DirectSound capture buffer. */
        DWORD cSamplesToCapture = dsoundRingDistance(idxSampleReadPos, pDSoundStream->idxSampleCaptureReadPos,
                                                     pDSoundStream->cMaxSamplesInBuffer);
        if (cSamplesToCapture == 0)
            break;

        /* Get number of free samples in the mix buffer and check that is has free space */
        uint32_t cFreeMixSamples = AudioMixBufFree(&pStream->MixBuf);
        if (cFreeMixSamples == 0)
        {
            DSLOGF(("DSound: Capture buffer full\n"));
            break;
        }

        DSLOGF(("DSound: Capture cFreeMixSamples=%RU32, idxSampleReadPos=%u, idxSampleCaptureReadPos=%u, cSamplesToCapture=%u\n",
                cFreeMixSamples, idxSampleReadPos, pDSoundStream->idxSampleCaptureReadPos, cSamplesToCapture));

        /* No need to fetch more samples than mix buffer can receive. */
        cSamplesToCapture = RT_MIN(cSamplesToCapture, cFreeMixSamples);

        /* Lock relevant range in the DirectSound capture buffer. */
        PVOID pv1, pv2;
        DWORD cb1, cb2;
        hr = directSoundCaptureLock(pDSCB, &pStream->Props,
                                    AUDIOMIXBUF_S2B(&pStream->MixBuf, pDSoundStream->idxSampleCaptureReadPos), /* dwOffset */
                                    AUDIOMIXBUF_S2B(&pStream->MixBuf, cSamplesToCapture),                      /* dwBytes */
                                    &pv1, &pv2, &cb1, &cb2,
                                    0 /* dwFlags */);
        if (FAILED(hr))
        {
            rc = VERR_ACCESS_DENIED;
            break;
        }

        DWORD len1 = AUDIOMIXBUF_B2S(&pStream->MixBuf, cb1);
        DWORD len2 = AUDIOMIXBUF_B2S(&pStream->MixBuf, cb2);

        uint32_t cSamplesWrittenTotal = 0;
        uint32_t cSamplesWritten;
        if (pv1 && len1)
        {
            rc = AudioMixBufWriteCirc(&pStream->MixBuf, pv1, cb1, &cSamplesWritten);
            if (RT_SUCCESS(rc))
                cSamplesWrittenTotal += cSamplesWritten;
        }

        if (   RT_SUCCESS(rc)
            && cSamplesWrittenTotal == len1
            && pv2 && len2)
        {
            rc = AudioMixBufWriteCirc(&pStream->MixBuf, pv2, cb2, &cSamplesWritten);
            if (RT_SUCCESS(rc))
                cSamplesWrittenTotal += cSamplesWritten;
        }

        directSoundCaptureUnlock(pDSCB, pv1, pv2, cb1, cb2);

        if (cSamplesWrittenTotal) /* Captured something? */
            rc = AudioMixBufMixToParent(&pStream->MixBuf, cSamplesWrittenTotal, &cSamplesProcessed);

        if (RT_SUCCESS(rc))
        {
            pDSoundStream->idxSampleCaptureReadPos = (pDSoundStream->idxSampleCaptureReadPos + cSamplesProcessed)
                                                   % pDSoundStream->cMaxSamplesInBuffer;
            DSLOGF(("DSound: Capture %u (%u+%u), processed %RU32/%RU32\n",
                    cSamplesToCapture, len1, len2, cSamplesProcessed, cSamplesWrittenTotal));
        }

    } while (0);

    if (RT_FAILURE(rc))
        dsoundUpdateStatusInternal(pThis);
    else if (pcbRead)
        *pcbRead = cSamplesProcessed;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int dsoundDestroyStreamIn(PPDMAUDIOSTREAM pStream)
{
    PDSOUNDSTREAMIN pDSoundStream = (PDSOUNDSTREAMIN)pStream;

    directSoundCaptureClose(pDSoundStream);

    pDSoundStream->idxSampleCaptureReadPos = 0;
    pDSoundStream->cMaxSamplesInBuffer = 0;
    RT_ZERO(pDSoundStream->streamCfg);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
int drvHostDSoundGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    dsoundUpdateStatusInternalEx(pThis, pBackendCfg, 0 /* fEnum */);

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


static DECLCALLBACK(int) dsoundNotificationThread(RTTHREAD hThreadSelf, void *pvUser)
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
                    rc = dsoundGetPosOut(pThis->pDSStream, &cbBuffer, &cbFree, &cbPlayPos);
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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
void drvHostDSoundShutdown(PPDMIHOSTAUDIO pInterface)
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
#else
    RT_NOREF_PV(pThis);
#endif

    LogFlowFuncLeave();
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostDSoundInit(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    LogFlowFuncEnter();

    int rc;

    /* Verify that IDirectSound is available. */
    LPDIRECTSOUND pDirectSound = NULL;
    HRESULT hr = CoCreateInstance(CLSID_DirectSound, NULL, CLSCTX_ALL, IID_IDirectSound, (void **)&pDirectSound);
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
        rc = RTThreadCreate(&pThis->Thread, dsoundNotificationThread,
                            pThis /*pvUser*/, 0 /*cbStack*/,
                            RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "dsoundNtfy");
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


static void dsoundConfigInit(PDRVHOSTDSOUND pThis, PCFGMNODE pCfg)
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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostDSoundGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostDSoundStreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = dsoundCreateStreamIn(pThis,  pStream, pCfgReq, pCfgAcq);
    else
        rc = dsoundCreateStreamOut(pThis, pStream, pCfgReq, pCfgAcq);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostDSoundStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = dsoundDestroyStreamIn(pStream);
    else
        rc = dsoundDestroyStreamOut(pThis, pStream);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostDSoundStreamControl(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = dsoundControlStreamIn(pThis,  pStream, enmStreamCmd);
    else
        rc = dsoundControlStreamOut(pThis, pStream, enmStreamCmd);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostDSoundStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, PDMAUDIOSTRMSTS_FLAG_NONE);
    AssertPtrReturn(pStream,    PDMAUDIOSTRMSTS_FLAG_NONE);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    PDMAUDIOSTRMSTS strmSts = PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
        PDSOUNDSTREAMIN pDSoundStream = (PDSOUNDSTREAMIN)pStream;
        if (pDSoundStream->fEnabled)
            strmSts |= PDMAUDIOSTRMSTS_FLAG_ENABLED | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE;
    }
    else
    {
        PDSOUNDSTREAMOUT pDSoundStream = (PDSOUNDSTREAMOUT)pStream;
        if (pDSoundStream->fEnabled)
        {
            strmSts |= PDMAUDIOSTRMSTS_FLAG_ENABLED;

            DWORD cbFree;
            int rc = dsoundGetPosOut(pThis, pDSoundStream, NULL /* cbBuffer */, &cbFree, NULL /* cbPlayPos */);
            if (   RT_SUCCESS(rc)
                && cbFree)
            {
                LogFlowFunc(("cbFree=%ld\n", cbFree));
                strmSts |= PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE;
            }
        }
    }

    return strmSts;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostDSoundStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for DSound. */
    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   PDMDRVINS::IBase Interface                                                                                                   *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostDSoundQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS     pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTDSOUND pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTDSOUND);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/*********************************************************************************************************************************
*   PDMDRVREG Interface                                                                                                          *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNPDMDRVDESTRUCT, pfnDestruct}
 */
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
 * @callback_method_impl{FNPDMDRVCONSTRUCT,
 *      Construct a DirectSound Audio driver instance.}
 */
static DECLCALLBACK(int) drvHostDSoundConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
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
    dsoundConfigInit(pThis, pCfg);

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
