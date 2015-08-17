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
    bool                 fReinitPlayPos;
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

/** Maximum number of release logging entries. */
static uint32_t s_cMaxRelLogEntries = 32;

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

    return RTStrDup("<GUID not found>");
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

static int dsoundPlayRestore(LPDIRECTSOUNDBUFFER8 pDSB)
{
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    LogRelMax(s_cMaxRelLogEntries, ("DSound: Error restoring playback buffer: %Rhrc\n", hr));
    return VERR_INVALID_STATE;
}

static int dsoundUnlockOutput(LPDIRECTSOUNDBUFFER8 pDSB,
                              LPVOID pv1, LPVOID pv2,
                              DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundBuffer8_Unlock(pDSB, pv1, cb1, pv2, cb2);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    LogRelMax(s_cMaxRelLogEntries, ("DSound: Error unlocking output buffer: %Rhrc\n", hr));
    return VERR_ACCESS_DENIED;
}

static int dsoundUnlockInput(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB,
                             LPVOID pv1, LPVOID pv2,
                             DWORD cb1, DWORD cb2)
{
    HRESULT hr = IDirectSoundCaptureBuffer8_Unlock(pDSCB, pv1, cb1, pv2, cb2);
    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    LogRelMax(s_cMaxRelLogEntries, ("DSound: Error unlocking input buffer: %Rhrc\n", hr));
    return VERR_ACCESS_DENIED;
}

static int dsoundLockOutput(LPDIRECTSOUNDBUFFER8 pDSB, PDMPCMPROPS *pProps,
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

    HRESULT hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (hr == DSERR_BUFFERLOST)
    {
        rc = dsoundPlayRestore(pDSB);
        if (RT_SUCCESS(rc))
        {
            hr = IDirectSoundBuffer8_Lock(pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
            if (FAILED(hr))
                rc = VERR_ACCESS_DENIED;
        }
    }

    if (RT_FAILURE(rc))
    {
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Error locking output buffer: %Rhrc\n", hr));
        return rc;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Locking playback buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                       cb1, cb2, pProps->uAlign));
        dsoundUnlockOutput(pDSB, pv1, pv2, cb1, cb2);
        return VERR_INVALID_STATE;
    }

    *ppv1 = pv1;
    *ppv2 = pv2;
    *pcb1 = cb1;
    *pcb2 = cb2;

    return VINF_SUCCESS;
}

static int dsoundLockInput(LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB, PPDMPCMPROPS pProps,
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
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Error locking capturing buffer: %Rhrc\n", hr));
        return VERR_ACCESS_DENIED;
    }

    if (   (pv1 && (cb1 & pProps->uAlign))
        || (pv2 && (cb2 & pProps->uAlign)))
    {
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Locking capture buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                       cb1, cb2, pProps->uAlign));
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
        IDirectSound8_Release(pDSoundStrmOut->pDS);
        pDSoundStrmOut->pDS = NULL;
    }
}

static int dsoundPlayInterfaceCreate(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    if (pDSoundStrmOut->pDS != NULL)
    {
        LogFlowFunc(("DirectSound instance already exists\n"));
        return VINF_SUCCESS;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_ALL,
                                  IID_IDirectSound8, (void **)&pDSoundStrmOut->pDS);
    if (FAILED(hr))
    {
        LogRel(("DSound: Error creating DirectSound instance: %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pDSoundStrmOut->pDS, pThis->cfg.pGuidPlay);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pDSoundStrmOut->pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                LogRel(("DSound: Error setting cooperative level for window %p: %Rhrc\n", hWnd, hr));
        }
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER)
                LogRel(("DSound: DirectSound playback is currently unavailable\n"));
            else
                LogRel(("DSound: Error initializing DirectSound: %Rhrc\n", hr));

            dsoundPlayInterfaceRelease(pDSoundStrmOut);
        }
    }

    return SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;
}

static void dsoundPlayClose(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    LogFlowFunc(("Closing playback stream %p (buffer %p)\n", pDSoundStrmOut, pDSoundStrmOut->pDSB));

    if (pDSoundStrmOut->pDSB)
    {
        HRESULT hr = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr))
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error closing playback stream %p: %Rhrc\n", pDSoundStrmOut, hr));

        IDirectSoundBuffer8_Release(pDSoundStrmOut->pDSB);
        pDSoundStrmOut->pDSB = NULL;
    }

    dsoundPlayInterfaceRelease(pDSoundStrmOut);
}

static int dsoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pDSoundStrmOut, VERR_INVALID_POINTER);

    LogFlowFunc(("pDSoundStrmOut=%p, cbBufferOut=%ld, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
                 pDSoundStrmOut,
                 pThis->cfg.cbBufferOut,
                 pDSoundStrmOut->strmOut.Props.uHz,
                 pDSoundStrmOut->strmOut.Props.cChannels,
                 pDSoundStrmOut->strmOut.Props.cBits,
                 pDSoundStrmOut->strmOut.Props.fSigned));

    if (pDSoundStrmOut->pDSB != NULL)
    {
        /* Should not happen but be forgiving. */
        LogFlowFunc(("DirectSoundBuffer already exists\n"));
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
        LPDIRECTSOUNDBUFFER pDSB;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        bd.dwBufferBytes = pThis->cfg.cbBufferOut;
        hr = IDirectSound8_CreateSoundBuffer(pDSoundStrmOut->pDS,
                                            &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error creating playback stream: %Rhrc\n", hr));
            break;
        }

        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (void **)&pDSoundStrmOut->pDSB);
        pDSB->Release();
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying interface for playback stream: %Rhrc\n", hr));
            break;
        }

        /* Query the actual parameters. */
        hr = IDirectSoundBuffer8_GetFormat(pDSoundStrmOut->pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying format for playback stream: %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundBuffer8_GetCaps(pDSoundStrmOut->pDSB, &bc);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying capabilities for playback stream: %Rhrc\n", hr));
            break;
        }

        LogFunc(("Playback format:\n"
                 "\tdwBufferBytes   = %RI32\n"
                 "\twFormatTag      = %RI16\n"
                 "\tnChannels       = %RI16\n"
                 "\tnSamplesPerSec  = %RU32\n"
                 "\tnAvgBytesPerSec = %RU32\n"
                 "\tnBlockAlign     = %RI16\n"
                 "\twBitsPerSample  = %RI16\n"
                 "\tcbSize          = %RI16\n",
                bc.dwBufferBytes,
                wfx.wFormatTag,
                wfx.nChannels,
                wfx.nSamplesPerSec,
                wfx.nAvgBytesPerSec,
                wfx.nBlockAlign,
                wfx.wBitsPerSample,
                wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmOut->strmOut.Props.uAlign)
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Playback capabilities returned misaligned buffer (size %ld, alignment %RU32)\n",
                           bc.dwBufferBytes, pDSoundStrmOut->strmOut.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferOut)
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Playback buffer size mismatched (DirectSound %ld, requested %ld bytes)\n",
                           bc.dwBufferBytes, pThis->cfg.cbBufferOut));

        /*
         * Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pDSoundStrmOut->csPlaybackBufferSize = bc.dwBufferBytes >> pDSoundStrmOut->strmOut.Props.cShift;
        LogFlowFunc(("csPlaybackBufferSize=%ld\n", pDSoundStrmOut->csPlaybackBufferSize));

    } while (0);

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    dsoundPlayClose(pDSoundStrmOut);
    return VERR_NOT_SUPPORTED;
}

static void dsoundPlayClearSamples(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturnVoid(pDSoundStrmOut);

    LPVOID pv1, pv2;
    DWORD cb1, cb2;
    int rc = dsoundLockOutput(pDSoundStrmOut->pDSB, &pDSoundStrmOut->strmOut.Props,
                              0, pDSoundStrmOut->csPlaybackBufferSize << pDSoundStrmOut->strmOut.Props.cShift,
                              &pv1, &pv2, &cb1, &cb2, DSBLOCK_ENTIREBUFFER);
    if (RT_SUCCESS(rc))
    {
        int len1 = cb1 >> pDSoundStrmOut->strmOut.Props.cShift;
        int len2 = cb2 >> pDSoundStrmOut->strmOut.Props.cShift;

        if (pv1 && len1)
            drvAudioClearBuf(&pDSoundStrmOut->strmOut.Props, pv1, len1);

        if (pv2 && len2)
            drvAudioClearBuf(&pDSoundStrmOut->strmOut.Props, pv2, len2);

        dsoundUnlockOutput(pDSoundStrmOut->pDSB, pv1, pv2, cb1, cb2);
    }
}

static int dsoundPlayGetStatus(LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pStatus)
{
    AssertPtrReturn(pDSB, VERR_INVALID_POINTER);
    /* pStatus is optional. */

    int rc = VINF_SUCCESS;

    DWORD dwStatus = 0;
    HRESULT hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
    if (SUCCEEDED(hr))
    {
        if ((dwStatus & DSBSTATUS_BUFFERLOST) != 0)
        {
            rc = dsoundPlayRestore(pDSB);
            if (RT_SUCCESS(rc))
                hr = IDirectSoundBuffer8_GetStatus(pDSB, &dwStatus);
        }
    }

    if (FAILED(hr))
    {
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Error getting playback status: %Rhrc\n", hr));
        if (RT_SUCCESS(rc))
            rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        if (pStatus)
            *pStatus = dwStatus;
    }

    return rc;
}

static void dsoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturnVoid(pThis);
    AssertPtrReturnVoid(pDSoundStrmOut);

    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        /* This performs some restore, so call it anyway and ignore result. */
        dsoundPlayGetStatus(pDSoundStrmOut->pDSB, &dwStatus);

        LogFlowFunc(("Playback stopped\n"));

        HRESULT hr = IDirectSoundBuffer8_Stop(pDSoundStrmOut->pDSB);
        if (FAILED(hr))
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Errpor stopping playback buffer: %Rhrc\n", hr));
    }
}

static int dsoundPlayStart(PDSOUNDSTREAMOUT pDSoundStrmOut)
{
    AssertPtrReturn(pDSoundStrmOut, VERR_INVALID_POINTER);

    int rc;

    if (pDSoundStrmOut->pDSB != NULL)
    {
        DWORD dwStatus;
        rc = dsoundPlayGetStatus(pDSoundStrmOut->pDSB, &dwStatus);
        if (RT_SUCCESS(rc))
        {
            if (dwStatus & DSBSTATUS_PLAYING)
            {
                LogFlowFunc(("Already playing\n"));
            }
            else
            {
                dsoundPlayClearSamples(pDSoundStrmOut);

                pDSoundStrmOut->fReinitPlayPos = true;

                LogFlowFunc(("Playback started\n"));

                HRESULT hr = IDirectSoundBuffer8_Play(pDSoundStrmOut->pDSB, 0, 0, DSBPLAY_LOOPING);
                if (FAILED(hr))
                {
                    LogRelMax(s_cMaxRelLogEntries, ("DSound: Error starting playback: %Rhrc\n", hr));
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        }
    }
    else
        rc = VERR_INVALID_STATE;

    return rc;
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
            LogRel2(("DSound: Guest \"%s\" is using host \"%s\"\n",
                     drvAudioRecSourceToString(pDSoundStrmIn->enmRecSource), pDev->pszName));

            pGUID = &pDev->Guid;
        }
    }

    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);
    if (pszGUID)
    {
        LogRel(("DSound: Guest \"%s\" is using host device with GUID: %s\n",
                drvAudioRecSourceToString(pDSoundStrmIn->enmRecSource), pszGUID));
        RTStrFree(pszGUID);
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
        LogFunc(("DSound: DirectSoundCapture instance already exists\n"));
        return VINF_SUCCESS;
    }

    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture8, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture8, (void **)&pDSoundStrmIn->pDSC);
    if (FAILED(hr))
    {
        LogRel(("DSound: Error creating capture instance: %Rhrc\n", hr));
    }
    else
    {
        LPCGUID pGUID = dsoundCaptureSelectDevice(pThis, pDSoundStrmIn);
        hr = IDirectSoundCapture_Initialize(pDSoundStrmIn->pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER)
                LogRel(("DSound: DirectSound capture is currently unavailable\n"));
            else
                LogRel(("DSound: Error initializing capture: %Rhrc\n", hr));
            dsoundCaptureInterfaceRelease(pDSoundStrmIn);
        }
    }

    return SUCCEEDED(hr) ? VINF_SUCCESS: VERR_NOT_SUPPORTED;
}

static void dsoundCaptureClose(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturnVoid(pDSoundStrmIn);

    LogFlowFunc(("pDSoundStrmIn=%p, pDSCB=%p\n", pDSoundStrmIn, pDSoundStrmIn->pDSCB));

    if (pDSoundStrmIn->pDSCB)
    {
        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED (hr))
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error stopping capture buffer: %Rhrc\n", hr));

        IDirectSoundCaptureBuffer8_Release(pDSoundStrmIn->pDSCB);
        pDSoundStrmIn->pDSCB = NULL;
    }

    dsoundCaptureInterfaceRelease(pDSoundStrmIn);
}

static int dsoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDSoundStrmIn, VERR_INVALID_PARAMETER);

    LogFlowFunc(("pDSoundStrmIn=%p, cbBufferIn=%ld, uHz=%RU32, cChannels=%RU8, cBits=%RU8, fSigned=%RTbool\n",
                 pDSoundStrmIn,
                 pThis->cfg.cbBufferIn,
                 pDSoundStrmIn->strmIn.Props.uHz,
                 pDSoundStrmIn->strmIn.Props.cChannels,
                 pDSoundStrmIn->strmIn.Props.cBits,
                 pDSoundStrmIn->strmIn.Props.fSigned));

    if (pDSoundStrmIn->pDSCB != NULL)
    {
        /* Should not happen but be forgiving. */
        LogFlowFunc(("Capture buffer already exists\n"));
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
        LPDIRECTSOUNDCAPTUREBUFFER pDSCB = NULL;
        RT_ZERO(bd);
        bd.dwSize = sizeof(bd);
        bd.lpwfxFormat = &wfx;
        bd.dwBufferBytes = pThis->cfg.cbBufferIn;
        hr = IDirectSoundCapture_CreateCaptureBuffer(pDSoundStrmIn->pDSC,
                                                     &bd, &pDSCB, NULL);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error creating capture buffer: %Rhrc\n", hr));
            pDSoundStrmIn->pDSCB = NULL;
            break;
        }

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pDSoundStrmIn->pDSCB);
        IDirectSoundCaptureBuffer_Release(pDSCB);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying for capture buffer interface: %Rhrc\n", hr));
            break;
        }

        /* Query the actual parameters. */
        DWORD cbReadPos = 0;
        hr = IDirectSoundCaptureBuffer8_GetCurrentPosition(pDSoundStrmIn->pDSCB, NULL, &cbReadPos);
        if (FAILED(hr))
        {
            cbReadPos = 0;
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error retrieving current position for capture stream: %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pDSoundStrmIn->pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying format for capture stream: %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pDSoundStrmIn->pDSCB, &bc);
        if (FAILED (hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error querying capabilities for capture stream: %Rhrc\n", hr));
            break;
        }

        LogFunc(("Capture format:\n"
                 "\tdwBufferBytes   = %RI32\n"
                 "\twFormatTag      = %RI16\n"
                 "\tnChannels       = %RI16\n"
                 "\tnSamplesPerSec  = %RU32\n"
                 "\tnAvgBytesPerSec = %RU32\n"
                 "\tnBlockAlign     = %RI16\n"
                 "\twBitsPerSample  = %RI16\n"
                 "\tcbSize          = %RI16\n",
                bc.dwBufferBytes,
                wfx.wFormatTag,
                wfx.nChannels,
                wfx.nSamplesPerSec,
                wfx.nAvgBytesPerSec,
                wfx.nBlockAlign,
                wfx.wBitsPerSample,
                wfx.cbSize));

        if (bc.dwBufferBytes & pDSoundStrmIn->strmIn.Props.uAlign)
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Capture capabilities returned misaligned buffer (size %ld, alignment %RU32)\n",
                      bc.dwBufferBytes, pDSoundStrmIn->strmIn.Props.uAlign + 1));

        if (bc.dwBufferBytes != pThis->cfg.cbBufferIn)
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Capture buffer size mismatched (DirectSound %ld, requested %ld bytes)\n",
                      bc.dwBufferBytes, pThis->cfg.cbBufferIn));

        /* Initial state: reading at the initial capture position. */
        pDSoundStrmIn->csCaptureReadPos    = cbReadPos >> pDSoundStrmIn->strmIn.Props.cShift;
        pDSoundStrmIn->csCaptureBufferSize = bc.dwBufferBytes >> pDSoundStrmIn->strmIn.Props.cShift;

        LogFlowFunc(("csCaptureReadPos=%ld, csCaptureBufferSize=%ld\n",
                     pDSoundStrmIn->csCaptureReadPos, pDSoundStrmIn->csCaptureBufferSize));

        /* Update status. */
        pDSoundStrmIn->hrLastCaptureIn = S_OK;

    } while (0);

    if (SUCCEEDED(hr))
        return VINF_SUCCESS;

    dsoundCaptureClose(pDSoundStrmIn);
    return VERR_NOT_SUPPORTED;
}

static void dsoundCaptureStop(PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturnVoid(pDSoundStrmIn);

    if (pDSoundStrmIn->pDSCB)
    {
        LogFlowFunc(("Capturing stopped\n"));

        HRESULT hr = IDirectSoundCaptureBuffer_Stop(pDSoundStrmIn->pDSCB);
        if (FAILED(hr))
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error stopping capture buffer: %Rhrc\n", hr));
    }
}

static int dsoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAMIN pDSoundStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pDSoundStrmIn, VERR_INVALID_PARAMETER);

    HRESULT hr;

    if (pDSoundStrmIn->pDSCB != NULL)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer8_GetStatus(pDSoundStrmIn->pDSCB, &dwStatus);
        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error getting capture buffer status: %Rhrc\n", hr));
        }
        else
        {
            if (dwStatus & DSCBSTATUS_CAPTURING)
            {
                LogFlowFunc(("Already capturing\n"));
            }
            else
            {
                LogFlowFunc(("Capturig started\n"));
                hr = IDirectSoundCaptureBuffer8_Start(pDSoundStrmIn->pDSCB, DSCBSTART_LOOPING);
                if (FAILED (hr))
                    LogRelMax(s_cMaxRelLogEntries, ("DSound: Error starting capture: %Rhrc\n", hr));
            }
        }
    }
    else
    {
        AssertMsgFailed(("No/invalid capture buffer\n"));
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

static void dsoundLogDevice(const char *pszType, LPGUID lpGUID, LPCWSTR lpwstrDescription, LPCWSTR lpwstrModule)
{
    AssertPtrReturnVoid(pszType);
    AssertPtrReturnVoid(lpGUID);
    AssertPtrReturnVoid(lpwstrDescription);
    AssertPtrReturnVoid(lpwstrModule);

    char *pszGUID = dsoundGUIDToUtf8StrA(lpGUID);
    if (pszGUID)
    {
        LogRel(("DSound: %s: GUID: %s [%ls] (Module: %ls)\n",
                pszType, pszGUID, lpwstrDescription, lpwstrModule));

        RTStrFree(pszGUID);
    }
}

static BOOL CALLBACK dsoundEnumCallback(LPGUID lpGUID, LPCWSTR lpwstrDescription,
                                        LPCWSTR lpwstrModule, LPVOID lpContext)
{
   AssertPtrReturn(lpContext, FALSE);

    PDSOUNDENUMCBCTX pCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pCtx, FALSE);
    AssertPtrReturn(pCtx->pDrv, FALSE);
    AssertPtrReturn(pCtx->pCfg, FALSE);

    if (!lpGUID)
        return TRUE;

    AssertPtrReturn(lpwstrDescription, FALSE);
    AssertPtrReturn(lpwstrModule, FALSE);

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

    int rc = drvAudioStreamCfgToProps(&pDSoundStrmOut->streamCfg, &pDSoundStrmOut->strmOut.Props);
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

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            dsoundPlayStop(pThis, pDSoundStrmOut);
            break;
        }

        default:
        {
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
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

    do
    {
        LPDIRECTSOUNDBUFFER8 pDSB = pDSoundStrmOut->pDSB;
        if (!pDSB)
            break;

        int cShift = pHstStrmOut->Props.cShift;
        DWORD cbBuffer = pDSoundStrmOut->csPlaybackBufferSize << cShift;

        DWORD cbPlayPos, cbWritePos;
        HRESULT hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, &cbWritePos);
        if (hr == DSERR_BUFFERLOST)
        {
            rc = dsoundPlayRestore(pDSB);
            if (RT_FAILURE(rc))
                break;

            hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &cbPlayPos, &cbWritePos);
            if (hr == DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
                break;
        }

        if (FAILED(hr))
        {
            LogRelMax(s_cMaxRelLogEntries, ("Error retrieving current playback position: %Rhrc\n", hr));
            break;
        }

        DWORD cbFree;
        DWORD cbPlayWritePos;
        if (pDSoundStrmOut->fReinitPlayPos)
        {
            pDSoundStrmOut->fReinitPlayPos = false;

            pDSoundStrmOut->cbPlayWritePos = cbWritePos;

            cbPlayWritePos = pDSoundStrmOut->cbPlayWritePos;
            cbFree = cbBuffer - dsoundRingDistance(cbWritePos, cbPlayPos, cbBuffer);
        }
        else
        {
            /* Full buffer? */
            if (pDSoundStrmOut->cbPlayWritePos == cbPlayPos)
                break;

            cbPlayWritePos = pDSoundStrmOut->cbPlayWritePos;
            cbFree         = dsoundRingDistance(cbPlayPos, cbPlayWritePos, cbBuffer);
        }

        uint32_t csLive = drvAudioHstOutSamplesLive(pHstStrmOut, NULL /* pcStreamsLive */);
        uint32_t cbLive = csLive << cShift;

        /* Do not write more than available space in the DirectSound playback buffer. */
        cbLive = RT_MIN(cbFree, cbLive);

        cbLive &= ~pHstStrmOut->Props.uAlign;
        if (cbLive == 0 || cbLive > cbBuffer)
        {
            LogFlowFunc(("cbLive=%RU32, cbBuffer=%ld, cbPlayWritePos=%ld, cbPlayPos=%ld\n",
                         cbLive, cbBuffer, cbPlayWritePos, cbPlayPos));
            break;
        }

        LPVOID pv1, pv2;
        DWORD cb1, cb2;
        rc = dsoundLockOutput(pDSB, &pHstStrmOut->Props, cbPlayWritePos, cbLive,
                              &pv1, &pv2, &cb1, &cb2, 0 /* dwFlags */);
        if (RT_FAILURE(rc))
            break;

        DWORD len1 = cb1 >> cShift;
        DWORD len2 = cb2 >> cShift;

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

        dsoundUnlockOutput(pDSB, pv1, pv2, cb1, cb2);

        pDSoundStrmOut->cbPlayWritePos = (cbPlayWritePos + (cReadTotal << cShift)) % cbBuffer;

        LogFlowFunc(("%RU32 (%RU32 samples) out of %RU32%s, buffer write pos %ld -> %ld, rc=%Rrc\n",
                     AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal), cReadTotal, cbLive,
                     cbLive != AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cReadTotal) ? " !!!": "",
                     cbPlayWritePos, pDSoundStrmOut->cbPlayWritePos, rc));

        if (cReadTotal)
        {
            AudioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);
            rc = VINF_SUCCESS; /* Played something. */
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
    pDSoundStrmIn->streamCfg.enmEndianness = PDMAUDIOHOSTENDIANNESS;

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
    LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB = pDSoundStrmIn->pDSCB;

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
            LogRelMax(s_cMaxRelLogEntries, ("DSound: Error retrieving current capture position: %Rhrc\n", hr));
            pDSoundStrmIn->hrLastCaptureIn = hr;
        }

        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }
    pDSoundStrmIn->hrLastCaptureIn = hr;

    if (cbReadPos & pHstStrmIn->Props.uAlign)
        LogFunc(("Misaligned read position %ld (alignment: %RU32)\n", cbReadPos, pHstStrmIn->Props.uAlign));

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
        LogRelMax(s_cMaxRelLogEntries, ("DSound: Capture buffer full\n"));
        if (pcSamplesCaptured)
            *pcSamplesCaptured = 0;
        return VINF_SUCCESS;
    }

    LogFlowFunc(("csMixFree=%RU32, csReadPos=%ld, csCaptureReadPos=%ld, csCaptured=%ld\n",
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

    dsoundUnlockInput(pDSCB, pv1, pv2, cb1, cb2);

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
        LogFlowFunc(("%ld (%ld+%ld), processed %RU32/%RU32\n",
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

    DSOUNDENUMCBCTX ctx = { pThis, pCfg };

    HRESULT hr = DirectSoundEnumerateW(&dsoundEnumCallback, &ctx);
    if (FAILED(hr))
        LogRel(("DSound: Error enumerating host playback devices: %Rhrc\n", hr));

    LogRel(("DSound: Found %RU32 host playback devices\n", pCfg->cMaxHstStrmsOut));
    if (pCfg->cMaxHstStrmsOut == 0)
        pCfg->cMaxHstStrmsOut = 1; /* Support at least one stream. */

    hr = DirectSoundCaptureEnumerateW(&dsoundCaptureEnumCallback, &ctx);
    if (FAILED(hr))
        LogRel(("DSound: Error nnumerating host capturing devices: %Rhrc\n", hr));

    LogRel(("DSound: Found %RU32 host capturing devices\n", pCfg->cMaxHstStrmsIn));
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
        LogRel(("DSound: DirectSound not available: %Rhrc\n", hr));

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
    /** @todo r=bird: What's wrong with CFGMR3QueryStringAlloc ??   */
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
            LogRel(("DSound: Error parsing device GUID for device '%s': %Rrc\n", pszName, rc));

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

    LogFlowFunc(("BufsizeOut %u, BufsizeIn %u, DeviceGuidOut {%RTuuid}, DeviceGuidIn {%RTuuid}\n",
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
        LogRel(("DSound: Error initializing COM: %Rhrc\n", hr));
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
