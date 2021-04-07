/* $Id$ */
/** @file
 * Host audio driver - DirectSound (Windows).
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define INITGUID
#include <VBox/log.h>
#include <iprt/win/windows.h>
#include <dsound.h>
#include <Mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <iprt/alloc.h>
#include <iprt/system.h>
#include <iprt/uuid.h>
#include <iprt/utf16.h>

#include <VBox/vmm/pdmaudioinline.h>
#include <VBox/vmm/pdmaudiohostenuminline.h>

#include "VBoxDD.h"
#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
# include <new> /* For bad_alloc. */
# include "DrvHostAudioDSoundMMNotifClient.h"
#endif


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
/** General code behavior. */
#define DSLOG(a)    do { LogRel2(a); } while(0)
/** Something which produce a lot of logging during playback/recording. */
#define DSLOGF(a)   do { LogRel3(a); } while(0)
/** Important messages like errors. Limited in the default release log to avoid log flood. */
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
/** Default input latency (in ms). */
#define DRV_DSOUND_DEFAULT_LATENCY_MS_IN        50
/** Default output latency (in ms). */
#define DRV_DSOUND_DEFAULT_LATENCY_MS_OUT       50

/** Makes DRVHOSTDSOUND out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface) \
    ( (PDRVHOSTDSOUND)((uintptr_t)pInterface - RT_UOFFSETOF(DRVHOSTDSOUND, IHostAudio)) )


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/* Dynamically load dsound.dll. */
typedef HRESULT WINAPI FNDIRECTSOUNDENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
typedef FNDIRECTSOUNDENUMERATEW *PFNDIRECTSOUNDENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTUREENUMERATEW(LPDSENUMCALLBACKW pDSEnumCallback, PVOID pContext);
typedef FNDIRECTSOUNDCAPTUREENUMERATEW *PFNDIRECTSOUNDCAPTUREENUMERATEW;
typedef HRESULT WINAPI FNDIRECTSOUNDCAPTURECREATE8(LPCGUID lpcGUID, LPDIRECTSOUNDCAPTURE8 *lplpDSC, LPUNKNOWN pUnkOuter);
typedef FNDIRECTSOUNDCAPTURECREATE8 *PFNDIRECTSOUNDCAPTURECREATE8;

#define VBOX_DSOUND_MAX_EVENTS 3

typedef enum DSOUNDEVENT
{
    DSOUNDEVENT_NOTIFY = 0,
    DSOUNDEVENT_INPUT,
    DSOUNDEVENT_OUTPUT,
} DSOUNDEVENT;

typedef struct DSOUNDHOSTCFG
{
    RTUUID          uuidPlay;
    LPCGUID         pGuidPlay;
    RTUUID          uuidCapture;
    LPCGUID         pGuidCapture;
} DSOUNDHOSTCFG, *PDSOUNDHOSTCFG;

typedef struct DSOUNDSTREAM
{
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG   Cfg;
    /** Buffer alignment. */
    uint8_t             uAlign;
    /** Whether this stream is in an enable state on the DirectSound side. */
    bool                fEnabled;
    bool                afPadding[2];
    /** Size (in bytes) of the DirectSound buffer. */
    DWORD               cbBufSize;
    /** The stream's critical section for synchronizing access. */
    RTCRITSECT          CritSect;
    union
    {
        struct
        {
            /** The actual DirectSound Buffer (DSB) used for the capturing.
             *  This is a secondary buffer and is used as a streaming buffer. */
            LPDIRECTSOUNDCAPTUREBUFFER8 pDSCB;
            /** Current read offset (in bytes) within the DSB. */
            DWORD                       offReadPos;
            /** Number of buffer overruns happened. Used for logging. */
            uint8_t                     cOverruns;
        } In;
        struct
        {
            /** The actual DirectSound Buffer (DSB) used for playback.
             *  This is a secondary buffer and is used as a streaming buffer. */
            LPDIRECTSOUNDBUFFER8        pDSB;
            /** Current write offset (in bytes) within the DSB.
             * @note This is needed as the current write position as kept by direct sound
             *       will move ahead if we're too late. */
            DWORD                       offWritePos;
            /** Offset of last play cursor within the DSB when checked for pending. */
            DWORD                       offPlayCursorLastPending;
            /** Offset of last play cursor within the DSB when last played. */
            DWORD                       offPlayCursorLastPlayed;
            /** Total amount (in bytes) written to our internal ring buffer. */
            uint64_t                    cbWritten;
            /** Total amount (in bytes) played (to the DirectSound buffer). */
            uint64_t                    cbTransferred;
            /** Flag indicating whether playback was just (re)started. */
            bool                        fFirstTransfer;
            /** Flag indicating whether this stream is in draining mode, e.g. no new
             *  data is being written to it but DirectSound still needs to be able to
             *  play its remaining (buffered) data. */
            bool                        fDrain;
            /** How much (in bytes) the last transfer from the internal buffer
             *  to the DirectSound buffer was. */
            uint32_t                    cbLastTransferred;
            /** Timestamp (in ms) of the last transfer from the internal buffer
             *  to the DirectSound buffer. */
            uint64_t                    tsLastTransferredMs;
            /** Number of buffer underruns happened. Used for logging. */
            uint8_t                     cUnderruns;
        } Out;
    };
#ifdef LOG_ENABLED
    struct
    {
        uint64_t tsLastTransferredMs;
    } Dbg;
#endif
} DSOUNDSTREAM, *PDSOUNDSTREAM;

/**
 * DirectSound-specific device entry.
 */
typedef struct DSOUNDDEV
{
    PDMAUDIOHOSTDEV  Core;
    /** The GUID if handy. */
    GUID            Guid;
} DSOUNDDEV;
/** Pointer to a DirectSound device entry. */
typedef DSOUNDDEV *PDSOUNDDEV;

/**
 * Structure for holding a device enumeration context.
 */
typedef struct DSOUNDENUMCBCTX
{
    /** Enumeration flags. */
    uint32_t            fFlags;
    /** Pointer to device list to populate. */
    PPDMAUDIOHOSTENUM pDevEnm;
} DSOUNDENUMCBCTX, *PDSOUNDENUMCBCTX;

typedef struct DRVHOSTDSOUND
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Our audio host audio interface. */
    PDMIHOSTAUDIO               IHostAudio;
    /** Critical section to serialize access. */
    RTCRITSECT                  CritSect;
    /** DirectSound configuration options. */
    DSOUNDHOSTCFG               Cfg;
    /** List of devices of last enumeration. */
    PDMAUDIOHOSTENUM            DeviceEnum;
    /** Whether this backend supports any audio input.
     * @todo r=bird: This is not actually used for anything. */
    bool                        fEnabledIn;
    /** Whether this backend supports any audio output.
     * @todo r=bird: This is not actually used for anything. */
    bool                        fEnabledOut;
    /** The Direct Sound playback interface. */
    LPDIRECTSOUND8              pDS;
    /** The Direct Sound capturing interface. */
    LPDIRECTSOUNDCAPTURE8       pDSC;
#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    DrvHostAudioDSoundMMNotifClient *m_pNotificationClient;
#endif
    /** Pointer to the input stream. */
    PDSOUNDSTREAM               pDSStrmIn;
    /** Pointer to the output stream. */
    PDSOUNDSTREAM               pDSStrmOut;
} DRVHOSTDSOUND, *PDRVHOSTDSOUND;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static HRESULT  directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB);
static HRESULT  directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS);
static HRESULT  directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush);
static HRESULT  directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush);

static int      dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PDMAUDIOHOSTENUM pDevEnm, uint32_t fEnum);

static int      dsoundStreamEnable(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fEnable);
static void     dsoundStreamReset(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS);
static void     dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis);


static DWORD dsoundRingDistance(DWORD offEnd, DWORD offBegin, DWORD cSize)
{
    AssertReturn(offEnd <= cSize,   0);
    AssertReturn(offBegin <= cSize, 0);

    return offEnd >= offBegin ? offEnd - offBegin : cSize - offBegin + offEnd;
}

static int dsoundWaveFmtFromCfg(PPDMAUDIOSTREAMCFG pCfg, PWAVEFORMATEX pFmt)
{
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(pFmt, VERR_INVALID_POINTER);

    RT_BZERO(pFmt, sizeof(WAVEFORMATEX));

    pFmt->wFormatTag      = WAVE_FORMAT_PCM;
    pFmt->nChannels       = PDMAudioPropsChannels(&pCfg->Props);
    pFmt->wBitsPerSample  = PDMAudioPropsSampleBits(&pCfg->Props);
    pFmt->nSamplesPerSec  = PDMAudioPropsHz(&pCfg->Props);
    pFmt->nBlockAlign     = PDMAudioPropsFrameSize(&pCfg->Props);
    pFmt->nAvgBytesPerSec = PDMAudioPropsFramesToBytes(&pCfg->Props, PDMAudioPropsHz(&pCfg->Props));
    pFmt->cbSize          = 0; /* No extra data specified. */

    return VINF_SUCCESS;
}

/**
 * Retrieves the number of free bytes available for writing to a DirectSound output stream.
 *
 * @return  VBox status code. VERR_NOT_AVAILABLE if unable to determine or the
 *          buffer was not recoverable.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           DirectSound output stream to retrieve number for.
 * @param   pdwFree             Where to return the free amount on success.
 * @param   poffPlayCursor      Where to return the play cursor offset.
 */
static int dsoundGetFreeOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, DWORD *pdwFree, DWORD *poffPlayCursor)
{
    AssertPtrReturn(pThis,     VERR_INVALID_POINTER);
    AssertPtrReturn(pStreamDS, VERR_INVALID_POINTER);
    AssertPtrReturn(pdwFree,   VERR_INVALID_POINTER);
    AssertPtr(poffPlayCursor);

    Assert(pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT); /* Paranoia. */

    LPDIRECTSOUNDBUFFER8 pDSB = pStreamDS->Out.pDSB;
    if (!pDSB)
    {
        AssertPtr(pDSB);
        return VERR_INVALID_POINTER;
    }

    HRESULT hr = S_OK;

    /* Get the current play position which is used for calculating the free space in the buffer. */
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        DWORD offPlayCursor  = 0;
        DWORD offWriteCursor = 0;
        hr = IDirectSoundBuffer8_GetCurrentPosition(pDSB, &offPlayCursor, &offWriteCursor);
        if (SUCCEEDED(hr))
        {
            int32_t cbDiff = offWriteCursor - offPlayCursor;
            if (cbDiff < 0)
                cbDiff += pStreamDS->cbBufSize;

            int32_t cbFree = offPlayCursor - pStreamDS->Out.offWritePos;
            if (cbFree < 0)
                cbFree += pStreamDS->cbBufSize;

            if (cbFree > (int32_t)pStreamDS->cbBufSize - cbDiff)
            {
                /** @todo count/log these. */
                pStreamDS->Out.offWritePos = offWriteCursor;
                cbFree = pStreamDS->cbBufSize - cbDiff;
            }

            /* When starting to use a DirectSound buffer, offPlayCursor and offWriteCursor
             * both point at position 0, so we won't be able to detect how many bytes
             * are writable that way.
             *
             * So use our per-stream written indicator to see if we just started a stream. */
            if (pStreamDS->Out.cbWritten == 0)
                cbFree = pStreamDS->cbBufSize;

            DSLOGREL(("DSound: offPlayCursor=%RU32, offWriteCursor=%RU32, offWritePos=%RU32 -> cbFree=%RI32\n",
                      offPlayCursor, offWriteCursor, pStreamDS->Out.offWritePos, cbFree));

            *pdwFree = cbFree;
            *poffPlayCursor = offPlayCursor;
            return VINF_SUCCESS;
        }

        if (hr != DSERR_BUFFERLOST) /** @todo MSDN doesn't state this error for GetCurrentPosition(). */
            break;

        LogFunc(("Getting playing position failed due to lost buffer, restoring ...\n"));

        directSoundPlayRestore(pThis, pDSB);
    }

    if (hr != DSERR_BUFFERLOST) /* Avoid log flooding if the error is still there. */
        DSLOGREL(("DSound: Getting current playback position failed with %Rhrc\n", hr));

    LogFunc(("Failed with %Rhrc\n", hr));

    *poffPlayCursor = pStreamDS->cbBufSize;
    return VERR_NOT_AVAILABLE;
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

static HRESULT directSoundPlayRestore(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB)
{
    RT_NOREF(pThis);
    HRESULT hr = IDirectSoundBuffer8_Restore(pDSB);
    if (FAILED(hr))
        DSLOG(("DSound: Restoring playback buffer\n"));
    else
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

static HRESULT directSoundPlayLock(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                   DWORD dwOffset, DWORD dwBytes,
                                   PVOID *ppv1, PVOID *ppv2,
                                   DWORD *pcb1, DWORD *pcb2,
                                   DWORD dwFlags)
{
    AssertReturn(dwBytes, VERR_INVALID_PARAMETER);

    HRESULT hr = E_FAIL;
    AssertCompile(DRV_DSOUND_RESTORE_ATTEMPTS_MAX > 0);
    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        PVOID pv1, pv2;
        DWORD cb1, cb2;
        hr = IDirectSoundBuffer8_Lock(pStreamDS->Out.pDSB, dwOffset, dwBytes, &pv1, &cb1, &pv2, &cb2, dwFlags);
        if (SUCCEEDED(hr))
        {
            if (   (!pv1 || !(cb1 & pStreamDS->uAlign))
                && (!pv2 || !(cb2 & pStreamDS->uAlign)))
            {
                if (ppv1)
                    *ppv1 = pv1;
                if (ppv2)
                    *ppv2 = pv2;
                if (pcb1)
                    *pcb1 = cb1;
                if (pcb2)
                    *pcb2 = cb2;
                return S_OK;
            }
            DSLOGREL(("DSound: Locking playback buffer returned misaligned buffer: cb1=%#RX32, cb2=%#RX32 (alignment: %#RX32)\n",
                      *pcb1, *pcb2, pStreamDS->uAlign));
            directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, pv2, cb1, cb2);
            return E_FAIL;
        }

        if (hr != DSERR_BUFFERLOST)
            break;

        LogFlowFunc(("Locking failed due to lost buffer, restoring ...\n"));
        directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
    }

    DSLOGREL(("DSound: Locking playback buffer failed with %Rhrc (dwOff=%ld, dwBytes=%ld)\n", hr, dwOffset, dwBytes));
    return hr;
}

static HRESULT directSoundCaptureLock(PDSOUNDSTREAM pStreamDS,
                                      DWORD dwOffset, DWORD dwBytes,
                                      PVOID *ppv1, PVOID *ppv2,
                                      DWORD *pcb1, DWORD *pcb2,
                                      DWORD dwFlags)
{
    PVOID pv1 = NULL;
    PVOID pv2 = NULL;
    DWORD cb1 = 0;
    DWORD cb2 = 0;

    HRESULT hr = IDirectSoundCaptureBuffer8_Lock(pStreamDS->In.pDSCB, dwOffset, dwBytes,
                                                 &pv1, &cb1, &pv2, &cb2, dwFlags);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Locking capture buffer failed with %Rhrc\n", hr));
        return hr;
    }

    if (   (pv1 && (cb1 & pStreamDS->uAlign))
        || (pv2 && (cb2 & pStreamDS->uAlign)))
    {
        DSLOGREL(("DSound: Locking capture buffer returned misaligned buffer: cb1=%RI32, cb2=%RI32 (alignment: %RU32)\n",
                  cb1, cb2, pStreamDS->uAlign));
        directSoundCaptureUnlock(pStreamDS->In.pDSCB, pv1, pv2, cb1, cb2);
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

/**
 * Destroys a DirectSound playback interface.
 *
 * @param   pDS                 Playback interface to destroy.
 */
static void directSoundPlayInterfaceDestroy(LPDIRECTSOUND8 pDS)
{
    if (pDS)
    {
        LogFlowFuncEnter();

        IDirectSound8_Release(pDS);
        pDS = NULL;
    }
}

/**
 * Creates a DirectSound playback interface.
 *
 * @return  HRESULT
 * @param   pGUID               GUID of device to create the playback interface for.
 * @param   ppDS                Where to store the created interface. Optional.
 */
static HRESULT directSoundPlayInterfaceCreate(LPCGUID pGUID, LPDIRECTSOUND8 *ppDS)
{
    /* pGUID can be NULL, if this is the default device. */
    /* ppDS is optional. */

    LogFlowFuncEnter();

    LPDIRECTSOUND8 pDS;
    HRESULT hr = CoCreateInstance(CLSID_DirectSound8, NULL, CLSCTX_ALL,
                                  IID_IDirectSound8, (void **)&pDS);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating playback instance failed with %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSound8_Initialize(pDS, pGUID);
        if (SUCCEEDED(hr))
        {
            HWND hWnd = GetDesktopWindow();
            hr = IDirectSound8_SetCooperativeLevel(pDS, hWnd, DSSCL_PRIORITY);
            if (FAILED(hr))
                DSLOGREL(("DSound: Setting cooperative level for window %p failed with %Rhrc\n", hWnd, hr));
        }

        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no playback devices are attached. */
                DSLOGREL(("DSound: DirectSound playback is currently unavailable\n"));
            else
                DSLOGREL(("DSound: DirectSound playback initialization failed with %Rhrc\n", hr));

            directSoundPlayInterfaceDestroy(pDS);
        }
        else if (ppDS)
        {
            *ppDS = pDS;
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundPlayClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    LogFlowFuncEnter();

    HRESULT hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
    if (SUCCEEDED(hr))
    {
        DSLOG(("DSound: Closing playback stream\n"));
        RTCritSectEnter(&pThis->CritSect);

        if (pStreamDS->Out.pDSB)
        {
            IDirectSoundBuffer8_Release(pStreamDS->Out.pDSB);
            pStreamDS->Out.pDSB = NULL;
        }

        pThis->pDSStrmOut = NULL;

        RTCritSectLeave(&pThis->CritSect);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: Stopping playback stream %p failed with %Rhrc\n", pStreamDS, hr));

    return hr;
}

static HRESULT directSoundPlayOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                   PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);
    AssertPtrReturn(pCfgReq,   E_POINTER);
    AssertPtrReturn(pCfgAcq,   E_POINTER);

/** @todo r=bird: I cannot see any code populating pCfgAcq... */

    LogFlowFuncEnter();

    Assert(pStreamDS->Out.pDSB == NULL);

    DSLOG(("DSound: Opening playback stream (uHz=%RU32, cChannels=%RU8, cBits=%u, fSigned=%RTbool)\n", pCfgReq->Props.uHz,
           PDMAudioPropsChannels(&pCfgReq->Props), PDMAudioPropsSampleBits(&pCfgReq->Props), pCfgReq->Props.fSigned));

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(pCfgReq, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    DSLOG(("DSound: Requested playback format:\n"
           "  wFormatTag      = %RI16\n"
           "  nChannels       = %RI16\n"
           "  nSamplesPerSec  = %RU32\n"
           "  nAvgBytesPerSec = %RU32\n"
           "  nBlockAlign     = %RI16\n"
           "  wBitsPerSample  = %RI16\n"
           "  cbSize          = %RI16\n",
           wfx.wFormatTag,
           wfx.nChannels,
           wfx.nSamplesPerSec,
           wfx.nAvgBytesPerSec,
           wfx.nBlockAlign,
           wfx.wBitsPerSample,
           wfx.cbSize));

    /** @todo r=bird: Why is this called every time?  It triggers a device
     * enumeration. Andy claimed on IRC that enumeration was only done once...
     * It's generally a 'ing waste of time here too, as we dont really use any of
     * the information we gather there. */
    dsoundUpdateStatusInternal(pThis);

    HRESULT hr = directSoundPlayInterfaceCreate(pThis->Cfg.pGuidPlay, &pThis->pDS);
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
         * we're using this buffer as a so-called streaming buffer.
         *
         * See https://msdn.microsoft.com/en-us/library/windows/desktop/ee419014(v=vs.85).aspx
         *
         * However, as we do not want to use memory on the sound device directly
         * (as most modern audio hardware on the host doesn't have this anyway),
         * we're *not* going to use DSBCAPS_STATIC for that.
         *
         * Instead we're specifying DSBCAPS_LOCSOFTWARE, as this fits the bill
         * of copying own buffer data to our secondary's Direct Sound buffer.
         */
        bd.dwFlags       = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_LOCSOFTWARE;
        bd.dwBufferBytes = PDMAudioPropsFramesToBytes(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize);

        DSLOG(("DSound: Requested playback buffer is %RU64ms (%ld bytes)\n",
               PDMAudioPropsBytesToMilli(&pCfgReq->Props, bd.dwBufferBytes), bd.dwBufferBytes));

        hr = IDirectSound8_CreateSoundBuffer(pThis->pDS, &bd, &pDSB, NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Creating playback sound buffer failed with %Rhrc\n", hr));
            break;
        }

        /* "Upgrade" to IDirectSoundBuffer8 interface. */
        hr = IDirectSoundBuffer_QueryInterface(pDSB, IID_IDirectSoundBuffer8, (PVOID *)&pStreamDS->Out.pDSB);
        IDirectSoundBuffer_Release(pDSB);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Querying playback sound buffer interface failed with %Rhrc\n", hr));
            break;
        }

        /*
         * Query the actual parameters set for this stream.
         * Those might be different than the initially requested parameters.
         */
        RT_ZERO(wfx);
        hr = IDirectSoundBuffer8_GetFormat(pStreamDS->Out.pDSB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback format failed with %Rhrc\n", hr));
            break;
        }

        DSBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);

        hr = IDirectSoundBuffer8_GetCaps(pStreamDS->Out.pDSB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting playback capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Acquired playback buffer is %RU64ms (%ld bytes)\n",
               PDMAudioPropsBytesToMilli(&pCfgReq->Props, bc.dwBufferBytes), bc.dwBufferBytes));

        DSLOG(("DSound: Acquired playback format:\n"
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

        if (bc.dwBufferBytes & pStreamDS->uAlign)
            DSLOGREL(("DSound: Playback capabilities returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pStreamDS->uAlign + 1));

        /*
         * Initial state.
         * dsoundPlayStart initializes part of it to make sure that Stop/Start continues with a correct
         * playback buffer position.
         */
        pStreamDS->cbBufSize = bc.dwBufferBytes;

        pThis->pDSStrmOut = pStreamDS;

        const uint32_t cfBufSize = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, pStreamDS->cbBufSize);

        pCfgAcq->Backend.cFramesBufferSize    = cfBufSize;
        pCfgAcq->Backend.cFramesPeriod        = cfBufSize / 4;
        pCfgAcq->Backend.cFramesPreBuffering  = pCfgAcq->Backend.cFramesPeriod * 2;

    } while (0);

    if (FAILED(hr))
        directSoundPlayClose(pThis, pStreamDS);

    return hr;
}

static void dsoundPlayClearBuffer(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturnVoid(pStreamDS);

    PPDMAUDIOPCMPROPS pProps = &pStreamDS->Cfg.Props;

    HRESULT hr = IDirectSoundBuffer_SetCurrentPosition(pStreamDS->Out.pDSB, 0 /* Position */);
    if (FAILED(hr))
        DSLOGREL(("DSound: Setting current position to 0 when clearing buffer failed with %Rhrc\n", hr));

    PVOID pv1;
    hr = directSoundPlayLock(pThis, pStreamDS,
                             0 /* dwOffset */, pStreamDS->cbBufSize,
                             &pv1, NULL, 0, 0, DSBLOCK_ENTIREBUFFER);
    if (SUCCEEDED(hr))
    {
        PDMAudioPropsClearBuffer(pProps, pv1, pStreamDS->cbBufSize, PDMAUDIOPCMPROPS_B2F(pProps, pStreamDS->cbBufSize));

        directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, NULL, 0, 0);

        /* Make sure to get the last playback position and current write position from DirectSound again.
         * Those positions in theory could have changed, re-fetch them to be sure. */
        DWORD offPlay  = 0;
        DWORD offWrite = 0;
        hr = IDirectSoundBuffer_GetCurrentPosition(pStreamDS->Out.pDSB, &offPlay, &offWrite);
        if (SUCCEEDED(hr))
        {
            pStreamDS->Out.offPlayCursorLastPlayed = offPlay;
            pStreamDS->Out.offWritePos             = offWrite;
        }
        else
            DSLOGREL(("DSound: Re-fetching current position when clearing buffer failed with %Rhrc\n", hr));
    }
}

static HRESULT directSoundPlayGetStatus(PDRVHOSTDSOUND pThis, LPDIRECTSOUNDBUFFER8 pDSB, DWORD *pdwStatus)
{
    AssertPtrReturn(pThis, E_POINTER);
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

static HRESULT directSoundPlayStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    HRESULT hr = S_OK;

    if (pStreamDS->Out.pDSB)
    {
        DSLOG(("DSound: Stopping playback\n"));
        hr = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
        if (FAILED(hr))
        {
            hr = directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
            if (FAILED(hr)) /** @todo shouldn't this be a SUCCEEDED? */
                hr = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
        }
    }

    if (SUCCEEDED(hr))
    {
        if (fFlush)
            dsoundStreamReset(pThis, pStreamDS);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: %s playback failed with %Rhrc\n", fFlush ? "Stopping" : "Pausing", hr));

    return hr;
}

/**
 * Enables or disables a stream.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to enable / disable.
 * @param   fEnable             Whether to enable or disable the stream.
 */
static int dsoundStreamEnable(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fEnable)
{
    RT_NOREF(pThis);

    LogFunc(("%s %s\n",
             fEnable ? "Enabling" : "Disabling",
             pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN ? "capture" : "playback"));

    if (fEnable)
        dsoundStreamReset(pThis, pStreamDS);

    pStreamDS->fEnabled = fEnable;

    return VINF_SUCCESS;
}


/**
 * Resets the state of a DirectSound stream.
 *
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to reset state for.
 */
static void dsoundStreamReset(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    RT_NOREF(pThis);

    LogFunc(("Resetting %s\n",
             pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN ? "capture" : "playback"));

    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
    {
        pStreamDS->In.offReadPos = 0;
        pStreamDS->In.cOverruns  = 0;

        /* Also reset the DirectSound Capture Buffer (DSCB) by clearing all data to make sure
         * not stale audio data is left. */
        if (pStreamDS->In.pDSCB)
        {
            PVOID pv1; PVOID pv2; DWORD cb1; DWORD cb2;
            HRESULT hr = directSoundCaptureLock(pStreamDS, 0 /* Offset */, pStreamDS->cbBufSize, &pv1, &pv2, &cb1, &cb2,
                                                0 /* Flags */);
            if (SUCCEEDED(hr))
            {
                PDMAudioPropsClearBuffer(&pStreamDS->Cfg.Props, pv1, cb1, PDMAUDIOPCMPROPS_B2F(&pStreamDS->Cfg.Props, cb1));
                if (pv2 && cb2)
                    PDMAudioPropsClearBuffer(&pStreamDS->Cfg.Props, pv2, cb2, PDMAUDIOPCMPROPS_B2F(&pStreamDS->Cfg.Props, cb2));
                directSoundCaptureUnlock(pStreamDS->In.pDSCB, pv1, pv2, cb1, cb2);
            }
        }
    }
    else if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        /* If draining was enagaged, make sure dsound has stopped playing. */
        if (pStreamDS->Out.fDrain && pStreamDS->Out.pDSB)
            pStreamDS->Out.pDSB->Stop();

        pStreamDS->Out.fFirstTransfer = true;
        pStreamDS->Out.fDrain         = false;
        pStreamDS->Out.cUnderruns     = 0;

        pStreamDS->Out.cbLastTransferred   = 0;
        pStreamDS->Out.tsLastTransferredMs = 0;

        pStreamDS->Out.cbTransferred = 0;
        pStreamDS->Out.cbWritten = 0;

        pStreamDS->Out.offWritePos = 0;
        pStreamDS->Out.offPlayCursorLastPending = 0;
        pStreamDS->Out.offPlayCursorLastPlayed = 0;

        /* Also reset the DirectSound Buffer (DSB) by setting the position to 0 and clear all data to make sure
         * not stale audio data is left. */
        if (pStreamDS->Out.pDSB)
        {
            HRESULT hr = IDirectSoundBuffer8_SetCurrentPosition(pStreamDS->Out.pDSB, 0);
            if (SUCCEEDED(hr))
            {
                PVOID pv1; PVOID pv2; DWORD cb1; DWORD cb2;
                hr = directSoundPlayLock(pThis, pStreamDS, 0 /* Offset */, pStreamDS->cbBufSize, &pv1, &pv2, &cb1, &cb2,
                                         0 /* Flags */);
                if (SUCCEEDED(hr))
                {
                    PDMAudioPropsClearBuffer(&pStreamDS->Cfg.Props, pv1, cb1, PDMAUDIOPCMPROPS_B2F(&pStreamDS->Cfg.Props, cb1));
                    if (pv2 && cb2)
                        PDMAudioPropsClearBuffer(&pStreamDS->Cfg.Props, pv2, cb2, PDMAUDIOPCMPROPS_B2F(&pStreamDS->Cfg.Props, cb2));
                    directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, pv2, cb1, cb2);
                }
            }
        }
    }

#ifdef LOG_ENABLED
    pStreamDS->Dbg.tsLastTransferredMs = 0;
#endif
}


/**
 * Starts playing a DirectSound stream.
 *
 * @return  HRESULT
 * @param   pThis               Host audio driver instance.
 * @param   pStreamDS           Stream to start playing.
 */
static HRESULT directSoundPlayStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    HRESULT hr = S_OK;

    DWORD fFlags = DSCBSTART_LOOPING;

    for (unsigned i = 0; i < DRV_DSOUND_RESTORE_ATTEMPTS_MAX; i++)
    {
        DSLOG(("DSound: Starting playback\n"));
        hr = IDirectSoundBuffer8_Play(pStreamDS->Out.pDSB, 0, 0, fFlags);
        if (   SUCCEEDED(hr)
            || hr != DSERR_BUFFERLOST)
            break;
        LogFunc(("Restarting playback failed due to lost buffer, restoring ...\n"));
        directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
    }

    return hr;
}


/*
 * DirectSoundCapture
 */

#if 0 /* unused */
static LPCGUID dsoundCaptureSelectDevice(PDRVHOSTDSOUND pThis, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pThis, NULL);
    AssertPtrReturn(pCfg,  NULL);

    int rc = VINF_SUCCESS;

    LPCGUID pGUID = pThis->Cfg.pGuidCapture;
    if (!pGUID)
    {
        PDSOUNDDEV pDev = NULL;
        switch (pCfg->u.enmSrc)
        {
            case PDMAUDIORECSRC_LINE:
                /*
                 * At the moment we're only supporting line-in in the HDA emulation,
                 * and line-in + mic-in in the AC'97 emulation both are expected
                 * to use the host's mic-in as well.
                 *
                 * So the fall through here is intentional for now.
                 */
            case PDMAUDIORECSRC_MIC:
                pDev = (PDSOUNDDEV)DrvAudioHlpDeviceEnumGetDefaultDevice(&pThis->DeviceEnum, PDMAUDIODIR_IN);
                break;

            default:
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
                break;
        }

        if (   RT_SUCCESS(rc)
            && pDev)
        {
            DSLOG(("DSound: Guest source '%s' is using host recording device '%s'\n",
                   PDMAudioRecSrcGetName(pCfg->u.enmSrc), pDev->Core.szName));
            pGUID = &pDev->Guid;
        }
        if (RT_FAILURE(rc))
        {
            LogRel(("DSound: Selecting recording device failed with %Rrc\n", rc));
            return NULL;
        }
    }

    /* This always has to be in the release log. */
    char *pszGUID = dsoundGUIDToUtf8StrA(pGUID);
    LogRel(("DSound: Guest source '%s' is using host recording device with GUID '%s'\n",
            PDMAudioRecSrcGetName(pCfg->u.enmSrc), pszGUID ? pszGUID: "{?}"));
    RTStrFree(pszGUID);

    return pGUID;
}
#endif

/**
 * Destroys a DirectSound capture interface.
 *
 * @param   pDSC                Capture interface to destroy.
 */
static void directSoundCaptureInterfaceDestroy(LPDIRECTSOUNDCAPTURE8 pDSC)
{
    if (pDSC)
    {
        LogFlowFuncEnter();

        IDirectSoundCapture_Release(pDSC);
        pDSC = NULL;
    }
}

/**
 * Creates a DirectSound capture interface.
 *
 * @return  HRESULT
 * @param   pGUID               GUID of device to create the capture interface for.
 * @param   ppDSC               Where to store the created interface. Optional.
 */
static HRESULT directSoundCaptureInterfaceCreate(LPCGUID pGUID, LPDIRECTSOUNDCAPTURE8 *ppDSC)
{
    /* pGUID can be NULL, if this is the default device. */
    /* ppDSC is optional. */

    LogFlowFuncEnter();

    LPDIRECTSOUNDCAPTURE8 pDSC;
    HRESULT hr = CoCreateInstance(CLSID_DirectSoundCapture8, NULL, CLSCTX_ALL,
                                  IID_IDirectSoundCapture8, (void **)&pDSC);
    if (FAILED(hr))
    {
        DSLOGREL(("DSound: Creating capture instance failed with %Rhrc\n", hr));
    }
    else
    {
        hr = IDirectSoundCapture_Initialize(pDSC, pGUID);
        if (FAILED(hr))
        {
            if (hr == DSERR_NODRIVER) /* Usually means that no capture devices are attached. */
                DSLOGREL(("DSound: Capture device currently is unavailable\n"));
            else
                DSLOGREL(("DSound: Initializing capturing device failed with %Rhrc\n", hr));

            directSoundCaptureInterfaceDestroy(pDSC);
        }
        else if (ppDSC)
        {
            *ppDSC = pDSC;
        }
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureClose(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    LogFlowFuncEnter();

    HRESULT hr = directSoundCaptureStop(pThis, pStreamDS, true /* fFlush */);
    if (FAILED(hr))
        return hr;

    if (   pStreamDS
        && pStreamDS->In.pDSCB)
    {
        DSLOG(("DSound: Closing capturing stream\n"));

        IDirectSoundCaptureBuffer8_Release(pStreamDS->In.pDSCB);
        pStreamDS->In.pDSCB = NULL;
    }

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureOpen(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                      PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);
    AssertPtrReturn(pCfgReq,   E_POINTER);
    AssertPtrReturn(pCfgAcq,   E_POINTER);

    /** @todo r=bird: I cannot see any code populating pCfgAcq... */

    LogFlowFuncEnter();

    Assert(pStreamDS->In.pDSCB == NULL);

    DSLOG(("DSound: Opening capturing stream (uHz=%RU32, cChannels=%RU8, cBits=%u, fSigned=%RTbool)\n", pCfgReq->Props.uHz,
           PDMAudioPropsChannels(&pCfgReq->Props), PDMAudioPropsSampleBits(&pCfgReq->Props), pCfgReq->Props.fSigned));

    WAVEFORMATEX wfx;
    int rc = dsoundWaveFmtFromCfg(pCfgReq, &wfx);
    if (RT_FAILURE(rc))
        return E_INVALIDARG;

    /** @todo r=bird: Why is this called every time?  It triggers a device
     * enumeration. Andy claimed on IRC that enumeration was only done once... */
    dsoundUpdateStatusInternal(pThis);

    HRESULT hr = directSoundCaptureInterfaceCreate(pThis->Cfg.pGuidCapture, &pThis->pDSC);
    if (FAILED(hr))
        return hr;

    do /* For readability breaks... */
    {
        DSCBUFFERDESC bd;
        RT_ZERO(bd);

        bd.dwSize        = sizeof(bd);
        bd.lpwfxFormat   = &wfx;
        bd.dwBufferBytes = PDMAudioPropsFramesToBytes(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize);

        DSLOG(("DSound: Requested capture buffer is %RU64ms (%ld bytes)\n",
               PDMAudioPropsBytesToMilli(&pCfgReq->Props, bd.dwBufferBytes), bd.dwBufferBytes));

        LPDIRECTSOUNDCAPTUREBUFFER pDSCB;
        hr = IDirectSoundCapture_CreateCaptureBuffer(pThis->pDSC, &bd, &pDSCB, NULL);
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

        hr = IDirectSoundCaptureBuffer_QueryInterface(pDSCB, IID_IDirectSoundCaptureBuffer8, (void **)&pStreamDS->In.pDSCB);
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
        hr = IDirectSoundCaptureBuffer8_GetCurrentPosition(pStreamDS->In.pDSCB, NULL, &offByteReadPos);
        if (FAILED(hr))
        {
            offByteReadPos = 0;
            DSLOGREL(("DSound: Getting capture position failed with %Rhrc\n", hr));
        }

        RT_ZERO(wfx);
        hr = IDirectSoundCaptureBuffer8_GetFormat(pStreamDS->In.pDSCB, &wfx, sizeof(wfx), NULL);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture format failed with %Rhrc\n", hr));
            break;
        }

        DSCBCAPS bc;
        RT_ZERO(bc);
        bc.dwSize = sizeof(bc);
        hr = IDirectSoundCaptureBuffer8_GetCaps(pStreamDS->In.pDSCB, &bc);
        if (FAILED(hr))
        {
            DSLOGREL(("DSound: Getting capture capabilities failed with %Rhrc\n", hr));
            break;
        }

        DSLOG(("DSound: Acquired capture buffer is %RU64ms (%ld bytes)\n",
               PDMAudioPropsBytesToMilli(&pCfgReq->Props, bc.dwBufferBytes), bc.dwBufferBytes));

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

        if (bc.dwBufferBytes & pStreamDS->uAlign)
            DSLOGREL(("DSound: Capture GetCaps returned misaligned buffer: size %RU32, alignment %RU32\n",
                      bc.dwBufferBytes, pStreamDS->uAlign + 1));

        /* Initial state: reading at the initial capture position, no error. */
        pStreamDS->In.offReadPos = 0;
        pStreamDS->cbBufSize     = bc.dwBufferBytes;

        pThis->pDSStrmIn = pStreamDS;

        pCfgAcq->Backend.cFramesBufferSize = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, pStreamDS->cbBufSize);

    } while (0);

    if (FAILED(hr))
        directSoundCaptureClose(pThis, pStreamDS);

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

static HRESULT directSoundCaptureStop(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, bool fFlush)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    RT_NOREF(pThis);

    HRESULT hr = S_OK;

    if (pStreamDS->In.pDSCB)
    {
        DSLOG(("DSound: Stopping capture\n"));
        hr = IDirectSoundCaptureBuffer_Stop(pStreamDS->In.pDSCB);
    }

    if (SUCCEEDED(hr))
    {
        if (fFlush)
             dsoundStreamReset(pThis, pStreamDS);
    }

    if (FAILED(hr))
        DSLOGREL(("DSound: Stopping capture buffer failed with %Rhrc\n", hr));

    return hr;
}

static HRESULT directSoundCaptureStart(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    AssertPtrReturn(pThis,     E_POINTER);
    AssertPtrReturn(pStreamDS, E_POINTER);

    HRESULT hr;
    if (pStreamDS->In.pDSCB)
    {
        DWORD dwStatus;
        hr = IDirectSoundCaptureBuffer8_GetStatus(pStreamDS->In.pDSCB, &dwStatus);
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
                const DWORD fFlags = DSCBSTART_LOOPING;

                DSLOG(("DSound: Starting to capture\n"));
                hr = IDirectSoundCaptureBuffer8_Start(pStreamDS->In.pDSCB, fFlags);
                if (FAILED(hr))
                    DSLOGREL(("DSound: Starting to capture failed with %Rhrc\n", hr));
            }
        }
    }
    else
        hr = E_UNEXPECTED;

    LogFlowFunc(("Returning %Rhrc\n", hr));
    return hr;
}

/**
 * Callback for the playback device enumeration.
 *
 * @return  TRUE if continuing enumeration, FALSE if not.
 * @param   pGUID               Pointer to GUID of enumerated device. Can be NULL.
 * @param   pwszDescription     Pointer to (friendly) description of enumerated device.
 * @param   pwszModule          Pointer to module name of enumerated device.
 * @param   lpContext           Pointer to PDSOUNDENUMCBCTX context for storing the enumerated information.
 *
 * @note    Carbon copy of dsoundDevicesEnumCbCapture with OUT direction.
 */
static BOOL CALLBACK dsoundDevicesEnumCbPlayback(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pEnumCtx = (PDSOUNDENUMCBCTX)lpContext;
    AssertPtrReturn(pEnumCtx , FALSE);

    PPDMAUDIOHOSTENUM pDevEnm = pEnumCtx->pDevEnm;
    AssertPtrReturn(pDevEnm, FALSE);

    AssertPtrNullReturn(pGUID, FALSE); /* pGUID can be NULL for default device(s). */
    AssertPtrReturn(pwszDescription, FALSE);
    RT_NOREF(pwszModule); /* Do not care about pwszModule. */

    int rc;
    PDSOUNDDEV pDev = (PDSOUNDDEV)PDMAudioHostDevAlloc(sizeof(DSOUNDDEV));
    if (pDev)
    {
        pDev->Core.enmUsage = PDMAUDIODIR_OUT;
        pDev->Core.enmType  = PDMAUDIODEVICETYPE_BUILTIN;

        if (pGUID == NULL)
            pDev->Core.fFlags = PDMAUDIOHOSTDEV_F_DEFAULT;

        char *pszName;
        rc = RTUtf16ToUtf8(pwszDescription, &pszName);
        if (RT_SUCCESS(rc))
        {
            RTStrCopy(pDev->Core.szName, sizeof(pDev->Core.szName), pszName);
            RTStrFree(pszName);

            if (pGUID) /* pGUID == NULL means default device. */
                memcpy(&pDev->Guid, pGUID, sizeof(pDev->Guid));

            PDMAudioHostEnumAppend(pDevEnm, &pDev->Core);

            /* Note: Querying the actual device information will be done at some
             *       later point in time outside this enumeration callback to prevent
             *       DSound hangs. */
            return TRUE;
        }
        PDMAudioHostDevFree(&pDev->Core);
    }
    else
        rc = VERR_NO_MEMORY;

    LogRel(("DSound: Error enumeration playback device '%ls': rc=%Rrc\n", pwszDescription, rc));
    return FALSE; /* Abort enumeration. */
}

/**
 * Callback for the capture device enumeration.
 *
 * @return  TRUE if continuing enumeration, FALSE if not.
 * @param   pGUID               Pointer to GUID of enumerated device. Can be NULL.
 * @param   pwszDescription     Pointer to (friendly) description of enumerated device.
 * @param   pwszModule          Pointer to module name of enumerated device.
 * @param   lpContext           Pointer to PDSOUNDENUMCBCTX context for storing the enumerated information.
 *
 * @note    Carbon copy of dsoundDevicesEnumCbPlayback with IN direction.
 */
static BOOL CALLBACK dsoundDevicesEnumCbCapture(LPGUID pGUID, LPCWSTR pwszDescription, LPCWSTR pwszModule, PVOID lpContext)
{
    PDSOUNDENUMCBCTX pEnumCtx = (PDSOUNDENUMCBCTX )lpContext;
    AssertPtrReturn(pEnumCtx , FALSE);

    PPDMAUDIOHOSTENUM pDevEnm = pEnumCtx->pDevEnm;
    AssertPtrReturn(pDevEnm, FALSE);

    AssertPtrNullReturn(pGUID, FALSE); /* pGUID can be NULL for default device(s). */
    AssertPtrReturn(pwszDescription, FALSE);
    RT_NOREF(pwszModule); /* Do not care about pwszModule. */

    int rc;
    PDSOUNDDEV pDev = (PDSOUNDDEV)PDMAudioHostDevAlloc(sizeof(DSOUNDDEV));
    if (pDev)
    {
        pDev->Core.enmUsage = PDMAUDIODIR_IN;
        pDev->Core.enmType  = PDMAUDIODEVICETYPE_BUILTIN;

        char *pszName;
        rc = RTUtf16ToUtf8(pwszDescription, &pszName);
        if (RT_SUCCESS(rc))
        {
            RTStrCopy(pDev->Core.szName, sizeof(pDev->Core.szName), pszName);
            RTStrFree(pszName);

            if (pGUID) /* pGUID == NULL means default capture device. */
                memcpy(&pDev->Guid, pGUID, sizeof(pDev->Guid));

            PDMAudioHostEnumAppend(pDevEnm, &pDev->Core);

            /* Note: Querying the actual device information will be done at some
             *       later point in time outside this enumeration callback to prevent
             *       DSound hangs. */
            return TRUE;
        }
        PDMAudioHostDevFree(&pDev->Core);
    }
    else
        rc = VERR_NO_MEMORY;

    LogRel(("DSound: Error enumeration capture device '%ls', rc=%Rrc\n", pwszDescription, rc));
    return FALSE; /* Abort enumeration. */
}

/**
 * Qqueries information for a given (DirectSound) device.
 *
 * @returns VBox status code.
 * @param   pThis               Host audio driver instance.
 * @param   pDev                Audio device to query information for.
 */
static int dsoundDeviceQueryInfo(PDRVHOSTDSOUND pThis, PDSOUNDDEV pDev)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pDev,  VERR_INVALID_POINTER);
    int rc;

    if (pDev->Core.enmUsage == PDMAUDIODIR_OUT)
    {
        LPDIRECTSOUND8 pDS;
        HRESULT hr = directSoundPlayInterfaceCreate(&pDev->Guid, &pDS);
        if (SUCCEEDED(hr))
        {
            DSCAPS DSCaps;
            RT_ZERO(DSCaps);
            DSCaps.dwSize = sizeof(DSCAPS);
            hr = IDirectSound_GetCaps(pDS, &DSCaps);
            if (SUCCEEDED(hr))
            {
                pDev->Core.cMaxOutputChannels = DSCaps.dwFlags & DSCAPS_PRIMARYSTEREO ? 2 : 1;

                DWORD dwSpeakerCfg;
                hr = IDirectSound_GetSpeakerConfig(pDS, &dwSpeakerCfg);
                if (SUCCEEDED(hr))
                {
                    unsigned uSpeakerCount = 0;
                    switch (DSSPEAKER_CONFIG(dwSpeakerCfg))
                    {
                        case DSSPEAKER_MONO:             uSpeakerCount = 1; break;
                        case DSSPEAKER_HEADPHONE:        uSpeakerCount = 2; break;
                        case DSSPEAKER_STEREO:           uSpeakerCount = 2; break;
                        case DSSPEAKER_QUAD:             uSpeakerCount = 4; break;
                        case DSSPEAKER_SURROUND:         uSpeakerCount = 4; break;
                        case DSSPEAKER_5POINT1:          uSpeakerCount = 6; break;
                        case DSSPEAKER_5POINT1_SURROUND: uSpeakerCount = 6; break;
                        case DSSPEAKER_7POINT1:          uSpeakerCount = 8; break;
                        case DSSPEAKER_7POINT1_SURROUND: uSpeakerCount = 8; break;
                        default:                                            break;
                    }

                    if (uSpeakerCount) /* Do we need to update the channel count? */
                        pDev->Core.cMaxOutputChannels = uSpeakerCount;

                    rc = VINF_SUCCESS;
                }
                else
                {
                    LogRel(("DSound: Error retrieving playback device speaker config, hr=%Rhrc\n", hr));
                    rc = VERR_ACCESS_DENIED; /** @todo Fudge! */
                }
            }
            else
            {
                LogRel(("DSound: Error retrieving playback device capabilities, hr=%Rhrc\n", hr));
                rc = VERR_ACCESS_DENIED; /** @todo Fudge! */
            }

            directSoundPlayInterfaceDestroy(pDS);
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    else if (pDev->Core.enmUsage == PDMAUDIODIR_IN)
    {
        LPDIRECTSOUNDCAPTURE8 pDSC;
        HRESULT hr = directSoundCaptureInterfaceCreate(&pDev->Guid, &pDSC);
        if (SUCCEEDED(hr))
        {
            DSCCAPS DSCCaps;
            RT_ZERO(DSCCaps);
            DSCCaps.dwSize = sizeof(DSCCAPS);
            hr = IDirectSoundCapture_GetCaps(pDSC, &DSCCaps);
            if (SUCCEEDED(hr))
            {
                pDev->Core.cMaxInputChannels = DSCCaps.dwChannels;
                rc = VINF_SUCCESS;
            }
            else
            {
                LogRel(("DSound: Error retrieving capture device capabilities, hr=%Rhrc\n", hr));
                rc = VERR_ACCESS_DENIED; /** @todo Fudge! */
            }

            directSoundCaptureInterfaceDestroy(pDSC);
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    else
        AssertFailedStmt(rc = VERR_NOT_SUPPORTED);

    return rc;
}


/**
 * Queries information for @a pDevice and adds an entry to the enumeration.
 *
 * @returns VBox status code.
 * @param   pDevEnm     The enumeration to add the device to.
 * @param   pDevice     The device.
 * @param   enmType     The type of device.
 * @param   fDefault    Whether it's the default device.
 */
static int dsoundDeviceNewStyleAdd(PPDMAUDIOHOSTENUM pDevEnm, IMMDevice *pDevice, EDataFlow enmType, bool fDefault)
{
    int rc = VINF_SUCCESS; /* ignore most errors */

    /*
     * Gather the necessary properties.
     */
    IPropertyStore *pProperties = NULL;
    HRESULT hrc = pDevice->OpenPropertyStore(STGM_READ, &pProperties);
    if (SUCCEEDED(hrc))
    {
        /* Get the friendly name. */
        PROPVARIANT VarName;
        PropVariantInit(&VarName);
        hrc = pProperties->GetValue(PKEY_Device_FriendlyName, &VarName);
        if (SUCCEEDED(hrc))
        {
            /* Get the DirectSound GUID. */
            PROPVARIANT VarGUID;
            PropVariantInit(&VarGUID);
            hrc = pProperties->GetValue(PKEY_AudioEndpoint_GUID, &VarGUID);
            if (SUCCEEDED(hrc))
            {
                /* Get the device format. */
                PROPVARIANT VarFormat;
                PropVariantInit(&VarFormat);
                hrc = pProperties->GetValue(PKEY_AudioEngine_DeviceFormat, &VarFormat);
                if (SUCCEEDED(hrc))
                {
                    WAVEFORMATEX const * const pFormat = (WAVEFORMATEX const *)VarFormat.blob.pBlobData;
                    AssertPtr(pFormat);

                    /*
                     * Create a enumeration entry for it.
                     */
                    PDSOUNDDEV pDev = (PDSOUNDDEV)PDMAudioHostDevAlloc(sizeof(DSOUNDDEV));
                    if (pDev)
                    {
                        pDev->Core.enmUsage = enmType == eRender ? PDMAUDIODIR_OUT : PDMAUDIODIR_IN;
                        pDev->Core.enmType  = PDMAUDIODEVICETYPE_BUILTIN;
                        if (enmType == eRender)
                            pDev->Core.cMaxOutputChannels = pFormat->nChannels;
                        else
                            pDev->Core.cMaxInputChannels  = pFormat->nChannels;

                        RT_NOREF(fDefault);
                        //if (fDefault)
                            hrc = UuidFromStringW(VarGUID.pwszVal, &pDev->Guid);
                        if (SUCCEEDED(hrc))
                        {
                            char *pszName;
                            rc = RTUtf16ToUtf8(VarName.pwszVal, &pszName);
                            if (RT_SUCCESS(rc))
                            {
                                RTStrCopy(pDev->Core.szName, sizeof(pDev->Core.szName), pszName);
                                RTStrFree(pszName);

                                PDMAudioHostEnumAppend(pDevEnm, &pDev->Core);
                            }
                            else
                                PDMAudioHostDevFree(&pDev->Core);
                        }
                        else
                        {
                            LogFunc(("UuidFromStringW(%ls): %Rhrc\n", VarGUID.pwszVal, hrc));
                            PDMAudioHostDevFree(&pDev->Core);
                        }
                    }
                    else
                        rc = VERR_NO_MEMORY;
                    PropVariantClear(&VarFormat);
                }
                else
                    LogFunc(("Failed to get PKEY_AudioEngine_DeviceFormat: %Rhrc\n", hrc));
                PropVariantClear(&VarGUID);
            }
            else
                LogFunc(("Failed to get PKEY_AudioEndpoint_GUID: %Rhrc\n", hrc));
            PropVariantClear(&VarName);
        }
        else
            LogFunc(("Failed to get PKEY_Device_FriendlyName: %Rhrc\n", hrc));
        pProperties->Release();
    }
    else
        LogFunc(("OpenPropertyStore failed: %Rhrc\n", hrc));

    if (hrc == E_OUTOFMEMORY && RT_SUCCESS_NP(rc))
        rc = VERR_NO_MEMORY;
    return rc;
}

/**
 * Does a (Re-)enumeration of the host's playback + capturing devices.
 *
 * @return  IPRT status code.
 * @param   pThis               Host audio driver instance.
 * @param   pDevEnm             Where to store the enumerated devices.
 */
static int dsoundDevicesEnumerate(PDRVHOSTDSOUND pThis, PPDMAUDIOHOSTENUM pDevEnm)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    DSLOG(("DSound: Enumerating devices ...\n"));

    /*
     * Use the Vista+ API.
     */
    IMMDeviceEnumerator *pEnumerator;
    HRESULT hrc = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);
    if (SUCCEEDED(hrc))
    {
        int rc = VINF_SUCCESS;
        for (unsigned idxPass = 0; idxPass < 2 && RT_SUCCESS(rc); idxPass++)
        {
            EDataFlow const enmType = idxPass == 0 ? EDataFlow::eRender : EDataFlow::eCapture;

            /* Get the default device first. */
            IMMDevice *pDefaultDevice = NULL;
            hrc = pEnumerator->GetDefaultAudioEndpoint(enmType, eMultimedia, &pDefaultDevice);
            if (SUCCEEDED(hrc))
                rc = dsoundDeviceNewStyleAdd(pDevEnm, pDefaultDevice, enmType, true);
            else
                pDefaultDevice = NULL;

            /* Enumerate the devices. */
            IMMDeviceCollection *pCollection = NULL;
            hrc = pEnumerator->EnumAudioEndpoints(enmType, DEVICE_STATE_ACTIVE /*| DEVICE_STATE_UNPLUGGED?*/, &pCollection);
            if (SUCCEEDED(hrc) && pCollection != NULL)
            {
                UINT cDevices = 0;
                hrc = pCollection->GetCount(&cDevices);
                if (SUCCEEDED(hrc))
                {
                    for (UINT idxDevice = 0; idxDevice < cDevices && RT_SUCCESS(rc); idxDevice++)
                    {
                        IMMDevice *pDevice = NULL;
                        hrc = pCollection->Item(idxDevice, &pDevice);
                        if (SUCCEEDED(hrc) && pDevice)
                        {
                            if (pDevice != pDefaultDevice)
                                rc = dsoundDeviceNewStyleAdd(pDevEnm, pDevice, enmType, false);
                            pDevice->Release();
                        }
                    }
                }
                pCollection->Release();
            }
            else
                LogRelMax(10, ("EnumAudioEndpoints(%s) failed: %Rhrc\n", idxPass == 0 ? "output" : "input", hrc));

            if (pDefaultDevice)
                pDefaultDevice->Release();
        }
        pEnumerator->Release();
        if (pDevEnm->cDevices > 0 || RT_FAILURE(rc))
        {
            DSLOG(("DSound: Enumerating devices done - %u device (%Rrc)\n", pDevEnm->cDevices, rc));
            return rc;
        }
    }

    /*
     * Fall back on dsound.
     */
    RTLDRMOD hDSound = NULL;
    int rc = RTLdrLoadSystem("dsound.dll", true /*fNoUnload*/, &hDSound);
    if (RT_SUCCESS(rc))
    {
        DSOUNDENUMCBCTX EnumCtx;
        EnumCtx.fFlags  = 0;
        EnumCtx.pDevEnm = pDevEnm;

        /*
         * Enumerate playback devices.
         */
        PFNDIRECTSOUNDENUMERATEW pfnDirectSoundEnumerateW = NULL;
        rc = RTLdrGetSymbol(hDSound, "DirectSoundEnumerateW", (void**)&pfnDirectSoundEnumerateW);
        if (RT_SUCCESS(rc))
        {
            DSLOG(("DSound: Enumerating playback devices ...\n"));

            HRESULT hr = pfnDirectSoundEnumerateW(&dsoundDevicesEnumCbPlayback, &EnumCtx);
            if (FAILED(hr))
                LogRel(("DSound: Error enumerating host playback devices: %Rhrc\n", hr));
        }
        else
            LogRel(("DSound: Error starting to enumerate host playback devices: %Rrc\n", rc));

        /*
         * Enumerate capture devices.
         */
        PFNDIRECTSOUNDCAPTUREENUMERATEW pfnDirectSoundCaptureEnumerateW = NULL;
        rc = RTLdrGetSymbol(hDSound, "DirectSoundCaptureEnumerateW", (void**)&pfnDirectSoundCaptureEnumerateW);
        if (RT_SUCCESS(rc))
        {
            DSLOG(("DSound: Enumerating capture devices ...\n"));

            HRESULT hr = pfnDirectSoundCaptureEnumerateW(&dsoundDevicesEnumCbCapture, &EnumCtx);
            if (FAILED(hr))
                LogRel(("DSound: Error enumerating host capture devices: %Rhrc\n", hr));
        }
        else
            LogRel(("DSound: Error starting to enumerate host capture devices: %Rrc\n", rc));

        /*
         * Query Information from all enumerated devices.
         */
        PDSOUNDDEV pDev;
        RTListForEach(&pDevEnm->LstDevices, pDev, DSOUNDDEV, Core.ListEntry)
        {
            dsoundDeviceQueryInfo(pThis, pDev); /* ignore rc */
        }

        RTLdrClose(hDSound);
    }
    else
    {
        /* No dsound.dll on this system. */
        LogRel(("DSound: Could not load dsound.dll for enumerating devices: %Rrc\n", rc));
    }

    DSLOG(("DSound: Enumerating devices done\n"));

    return rc;
}

/**
 * Updates this host driver's internal status, according to the global, overall input/output
 * state and all connected (native) audio streams.
 *
 * @todo r=bird: This is a 'ing waste of 'ing time!  We're doing this everytime
 *       an 'ing stream is created and we doesn't 'ing use the information here
 *       for any darn thing!  Given the reported slowness of enumeration and
 *       issues with eh 'ing code the only appropriate response is:
 *       AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAARG!!!!!!!
 *
 * @param   pThis               Host audio driver instance.
 */
static void dsoundUpdateStatusInternal(PDRVHOSTDSOUND pThis)
{
#if 0 /** @todo r=bird: This isn't doing *ANYTHING* useful. So, I've just disabled it.  */
    AssertPtrReturnVoid(pThis);
    LogFlowFuncEnter();

    PDMAudioHostEnumDelete(&pThis->DeviceEnum);
    int rc = dsoundDevicesEnumerate(pThis, &pThis->DeviceEnum);
    if (RT_SUCCESS(rc))
    {
#if 0
        if (   pThis->fEnabledOut != RT_BOOL(cbCtx.cDevOut)
            || pThis->fEnabledIn  != RT_BOOL(cbCtx.cDevIn))
        {
            /** @todo Use a registered callback to the audio connector (e.g "OnConfigurationChanged") to
             *        let the connector know that something has changed within the host backend. */
        }
#endif
        pThis->fEnabledIn  = PDMAudioHostEnumCountMatching(&pThis->DeviceEnum, PDMAUDIODIR_IN)  != 0;
        pThis->fEnabledOut = PDMAudioHostEnumCountMatching(&pThis->DeviceEnum, PDMAUDIODIR_OUT) != 0;
    }

    LogFlowFuncLeaveRC(rc);
#else
    RT_NOREF(pThis);
#endif
}

static int dsoundCreateStreamOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                 PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    LogFlowFunc(("pStreamDS=%p, pCfgReq=%p\n", pStreamDS, pCfgReq));

    int rc;

    /* Try to open playback in case the device is already there. */
    HRESULT hr = directSoundPlayOpen(pThis, pStreamDS, pCfgReq, pCfgAcq);
    if (SUCCEEDED(hr))
    {
        rc = PDMAudioStrmCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            dsoundStreamReset(pThis, pStreamDS);
    }
    else
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int dsoundControlStreamOut(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStreamDS=%p, cmd=%d\n", pStreamDS, enmStreamCmd));

    HRESULT hr;
    int     rc = VINF_SUCCESS;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            dsoundStreamEnable(pThis, pStreamDS, true /* fEnable */);
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Only resume if the stream is enabled.
               Note! This used to always just resume the stream playback regardless of state,
                     and instead rely on DISABLE filling the buffer with silence. */
            if (pStreamDS->fEnabled)
            {
                hr = directSoundPlayStart(pThis, pStreamDS);
                if (FAILED(hr))
                    rc = VERR_NOT_SUPPORTED; /** @todo Fix this. */
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            dsoundStreamEnable(pThis, pStreamDS, false /* fEnable */);

            /* Don't stop draining buffers. They'll stop by themselves. */
            if (pStreamDS->Cfg.enmDir != PDMAUDIODIR_OUT || !pStreamDS->Out.fDrain)
            {
                hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
                if (FAILED(hr))
                    rc = VERR_NOT_SUPPORTED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            hr = directSoundPlayStop(pThis, pStreamDS, false /* fFlush */);
            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DRAIN:
        {
            /* Make sure we transferred everything. */
            pStreamDS->fEnabled = true; /** @todo r=bird: ??? */

            /*
             * We've started the buffer in looping mode, try switch to non-looping...
             */
            if (pStreamDS->Out.pDSB)
            {
                Log2Func(("drain: Switching playback to non-looping mode...\n"));
                HRESULT hrc = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
                if (SUCCEEDED(hrc))
                {
                    hrc = IDirectSoundBuffer8_Play(pStreamDS->Out.pDSB, 0, 0, 0);
                    if (SUCCEEDED(hrc))
                        pStreamDS->Out.fDrain = true;
                    else
                        Log2Func(("drain: IDirectSoundBuffer8_Play(,,,0) failed: %Rhrc\n", hrc));
                }
                else
                {
                    Log2Func(("drain: IDirectSoundBuffer8_Stop failed: %Rhrc\n", hrc));
                    hrc = directSoundPlayRestore(pThis, pStreamDS->Out.pDSB);
                    if (SUCCEEDED(hrc))
                    {
                        hrc = IDirectSoundBuffer8_Stop(pStreamDS->Out.pDSB);
                        Log2Func(("drain: IDirectSoundBuffer8_Stop failed: %Rhrc\n", hrc));
                    }
                }
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DROP:
        {
            pStreamDS->Out.cbLastTransferred   = 0;
            pStreamDS->Out.tsLastTransferredMs = 0;
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                    const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVHOSTDSOUND  pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTDSOUND, IHostAudio);
    PDSOUNDSTREAM   pStreamDS = (PDSOUNDSTREAM)pStream;
    AssertPtrReturn(pStreamDS, 0);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);

    if (pStreamDS->fEnabled)
        AssertReturn(pStreamDS->cbBufSize, VERR_INTERNAL_ERROR_2);
    else
    {
        Log2Func(("Stream disabled, skipping\n"));
        return VINF_SUCCESS;
    }

/** @todo Any condition under which we should call dsoundUpdateStatusInternal(pThis) here?
 * The old code thought it did so in case of failure, only it couldn't ever fails, so it never did. */

    /*
     * Transfer loop.
     */
    uint32_t cbWritten = 0;
    while (cbBuf > 0)
    {
        /*
         * Figure out how much we can possibly write.
         */
        DWORD offPlayCursor = 0;
        DWORD cbWritable    = 0;
        int rc = dsoundGetFreeOut(pThis, pStreamDS, &cbWritable, &offPlayCursor);
        AssertRCReturn(rc, rc);
        if (cbWritable < pStreamDS->Cfg.Props.cbFrame)
            break;

        uint32_t const cbToWrite = RT_MIN(cbWritable, cbBuf);
        Log3Func(("offPlay=%#x offWritePos=%#x -> cbWritable=%#x cbToWrite=%#x%s%s\n", offPlayCursor, pStreamDS->Out.offWritePos,
                  cbWritable, cbToWrite, pStreamDS->Out.fFirstTransfer ? " first" : "", pStreamDS->Out.fDrain ? " drain" : ""));

        /*
         * Lock that amount of buffer.
         */
        PVOID pv1 = NULL;
        DWORD cb1 = 0;
        PVOID pv2 = NULL;
        DWORD cb2 = 0;
        HRESULT hrc = directSoundPlayLock(pThis, pStreamDS, pStreamDS->Out.offWritePos, cbToWrite,
                                          &pv1, &pv2, &cb1, &cb2, 0 /*dwFlags*/);
        AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), VERR_ACCESS_DENIED); /** @todo translate these status codes already! */
        //AssertMsg(cb1 + cb2 == cbToWrite, ("%#x + %#x vs %#x\n", cb1, cb2, cbToWrite));

        /*
         * Copy over the data.
         */
        memcpy(pv1, pvBuf, cb1);
        pvBuf      = (uint8_t *)pvBuf + cb1;
        cbBuf     -= cb1;
        cbWritten += cb1;

        if (pv2)
        {
            memcpy(pv2, pvBuf, cb2);
            pvBuf      = (uint8_t *)pvBuf + cb2;
            cbBuf     -= cb2;
            cbWritten += cb2;
        }

        /*
         * Unlock and update the write position.
         */
        directSoundPlayUnlock(pThis, pStreamDS->Out.pDSB, pv1, pv2, cb1, cb2); /** @todo r=bird: pThis + pDSB parameters here for Unlock, but only pThis for Lock. Why? */
        pStreamDS->Out.offWritePos = (pStreamDS->Out.offWritePos + cb1 + cb2) % pStreamDS->cbBufSize;

        /*
         * If this was the first chunk, kick off playing.
         */
        if (!pStreamDS->Out.fFirstTransfer)
        { /* likely */ }
        else
        {
            *pcbWritten = cbWritten;
            hrc = directSoundPlayStart(pThis, pStreamDS);
            AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), VERR_ACCESS_DENIED); /** @todo translate these status codes already! */
            pStreamDS->Out.fFirstTransfer = false;
        }
    }

    /*
     * Done.
     */
    *pcbWritten = cbWritten;

    pStreamDS->Out.cbTransferred += cbWritten;
    if (cbWritten)
    {
        uint64_t const msPrev = pStreamDS->Out.tsLastTransferredMs;
        pStreamDS->Out.cbLastTransferred   = cbWritten;
        pStreamDS->Out.tsLastTransferredMs = RTTimeMilliTS();
        LogFlowFunc(("cbLastTransferred=%RU32, tsLastTransferredMs=%RU64 cMsDelta=%RU64\n",
                     cbWritten, pStreamDS->Out.tsLastTransferredMs, msPrev ? pStreamDS->Out.tsLastTransferredMs - msPrev : 0));
    }
    return VINF_SUCCESS;
}

static int dsoundDestroyStreamOut(PDRVHOSTDSOUND pThis, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    LogFlowFuncEnter();

    HRESULT hr = directSoundPlayStop(pThis, pStreamDS, true /* fFlush */);
    if (SUCCEEDED(hr))
    {
        hr = directSoundPlayClose(pThis, pStreamDS);
        if (FAILED(hr))
            return VERR_GENERAL_FAILURE; /** @todo Fix. */
    }

    return VINF_SUCCESS;
}

static int dsoundCreateStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS,
                                PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    LogFunc(("pStreamDS=%p, pCfgReq=%p, enmRecSource=%s\n",
             pStreamDS, pCfgReq, PDMAudioRecSrcGetName(pCfgReq->u.enmSrc)));


    /* Try to open capture in case the device is already there. */
    int rc;
    HRESULT hr = directSoundCaptureOpen(pThis, pStreamDS, pCfgReq, pCfgAcq);
    if (SUCCEEDED(hr))
    {
        rc = PDMAudioStrmCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            dsoundStreamReset(pThis, pStreamDS);
    }
    else
        rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;

    return rc;
}

static int dsoundControlStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("pStreamDS=%p, enmStreamCmd=%ld\n", pStreamDS, enmStreamCmd));

    int rc = VINF_SUCCESS;

    HRESULT hr;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            dsoundStreamEnable(pThis, pStreamDS, true /* fEnable */);
            RT_FALL_THROUGH();
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Try to start capture. If it fails, then reopen and try again. */
            hr = directSoundCaptureStart(pThis, pStreamDS);
            if (FAILED(hr))
            {
                hr = directSoundCaptureClose(pThis, pStreamDS);
                if (SUCCEEDED(hr))
                {
                    PDMAUDIOSTREAMCFG CfgAcq;
                    hr = directSoundCaptureOpen(pThis, pStreamDS, &pStreamDS->Cfg /* pCfgReq */, &CfgAcq);
                    if (SUCCEEDED(hr))
                    {
                        rc = PDMAudioStrmCfgCopy(&pStreamDS->Cfg, &CfgAcq);
                        if (RT_FAILURE(rc))
                            break;

                        /** @todo What to do if the format has changed? */

                        hr = directSoundCaptureStart(pThis, pStreamDS);
                    }
                }
            }

            if (FAILED(hr))
                rc = VERR_NOT_SUPPORTED;
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
            dsoundStreamEnable(pThis, pStreamDS, false /* fEnable */);
            RT_FALL_THROUGH();
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            directSoundCaptureStop(pThis, pStreamDS,
                                   enmStreamCmd == PDMAUDIOSTREAMCMD_DISABLE /* fFlush */);

            /* Return success in any case, as stopping the capture can fail if
             * the capture buffer is not around anymore.
             *
             * This can happen if the host's capturing device has been changed suddenly. */
            rc = VINF_SUCCESS;
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    /*PDRVHOSTDSOUND  pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTDSOUND, IHostAudio);*/ RT_NOREF(pInterface);
    PDSOUNDSTREAM   pStreamDS = (PDSOUNDSTREAM)pStream;
    AssertPtrReturn(pStreamDS, 0);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbRead, VERR_INVALID_POINTER);

#if 0 /** @todo r=bird: shouldn't we do the same check as for output streams? */
    if (pStreamDS->fEnabled)
        AssertReturn(pStreamDS->cbBufSize, VERR_INTERNAL_ERROR_2);
    else
    {
        Log2Func(("Stream disabled, skipping\n"));
        return VINF_SUCCESS;
    }
#endif

    /*
     * Read loop.
     */
    uint32_t cbRead = 0;
    while (cbBuf > 0)
    {
        /*
         * Figure out how much we can read.
         */
        DWORD   offCaptureCursor = 0;
        DWORD   offReadCursor    = 0;
        HRESULT hrc = IDirectSoundCaptureBuffer_GetCurrentPosition(pStreamDS->In.pDSCB, &offCaptureCursor, &offReadCursor);
        AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), VERR_ACCESS_DENIED); /** @todo translate these status codes already! */
        //AssertMsg(offReadCursor == pStreamDS->In.offReadPos, ("%#x %#x\n", offReadCursor, pStreamDS->In.offReadPos));

        uint32_t const cbReadable = dsoundRingDistance(offCaptureCursor, pStreamDS->In.offReadPos, pStreamDS->cbBufSize);

        if (cbReadable >= pStreamDS->Cfg.Props.cbFrame)
        { /* likely */ }
        else
        {
            if (cbRead > 0)
            { /* likely */ }
            else if (pStreamDS->In.cOverruns < 32)
            {
                pStreamDS->In.cOverruns++;
                DSLOG(("DSound: Warning: Buffer full (size is %zu bytes), skipping to record data (overflow #%RU32)\n",
                       pStreamDS->cbBufSize, pStreamDS->In.cOverruns));
            }
            break;
        }

        uint32_t const cbToRead = RT_MIN(cbReadable, cbBuf);
        Log3Func(("offCapture=%#x offRead=%#x/%#x -> cbWritable=%#x cbToWrite=%#x\n",
                  offCaptureCursor, offReadCursor, pStreamDS->In.offReadPos, cbReadable, cbToRead));

        /*
         * Lock that amount of buffer.
         */
        PVOID pv1 = NULL;
        DWORD cb1 = 0;
        PVOID pv2 = NULL;
        DWORD cb2 = 0;
        hrc = directSoundCaptureLock(pStreamDS, pStreamDS->In.offReadPos, cbToRead, &pv1, &pv2, &cb1, &cb2, 0 /*dwFlags*/);
        AssertMsgReturn(SUCCEEDED(hrc), ("%Rhrc\n", hrc), VERR_ACCESS_DENIED); /** @todo translate these status codes already! */
        AssertMsg(cb1 + cb2 == cbToRead, ("%#x + %#x vs %#x\n", cb1, cb2, cbToRead));

        /*
         * Copy over the data.
         */
        memcpy(pvBuf, pv1, cb1);
        pvBuf   = (uint8_t *)pvBuf + cb1;
        cbBuf  -= cb1;
        cbRead += cb1;

        if (pv2)
        {
            memcpy(pvBuf, pv2, cb2);
            pvBuf   = (uint8_t *)pvBuf + cb2;
            cbBuf  -= cb2;
            cbRead += cb2;
        }

        /*
         * Unlock and update the write position.
         */
        directSoundCaptureUnlock(pStreamDS->In.pDSCB, pv1, pv2, cb1, cb2); /** @todo r=bird: pDSB parameter here for Unlock, but pStreamDS for Lock. Why? */
        pStreamDS->In.offReadPos = (pStreamDS->In.offReadPos + cb1 + cb2) % pStreamDS->cbBufSize;
    }

    /*
     * Done.
     */
    if (pcbRead)
        *pcbRead = cbRead;

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
    if (cbRead)
    {
        RTFILE hFile;
        int rc2 = RTFileOpen(&hFile, VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "dsoundCapture.pcm",
                             RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc2))
        {
            RTFileWrite(hFile, (uint8_t *)pvBuf - cbRead, cbRead, NULL);
            RTFileClose(hFile);
        }
    }
#endif
    return VINF_SUCCESS;
}

static int dsoundDestroyStreamIn(PDRVHOSTDSOUND pThis, PDSOUNDSTREAM pStreamDS)
{
    LogFlowFuncEnter();

    directSoundCaptureClose(pThis, pStreamDS);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostDSoundHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RT_BZERO(pBackendCfg, sizeof(PPDMAUDIOBACKENDCFG));

    pBackendCfg->cbStreamOut = sizeof(DSOUNDSTREAM);
    pBackendCfg->cbStreamIn  = sizeof(DSOUNDSTREAM);

    RTStrPrintf2(pBackendCfg->szName, sizeof(pBackendCfg->szName), "DirectSound");

    pBackendCfg->cMaxStreamsIn  = UINT32_MAX;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetDevices}
 */
static DECLCALLBACK(int) drvHostDSoundHA_GetDevices(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHOSTENUM pDeviceEnum)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pDeviceEnum, VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        PDMAudioHostEnumInit(pDeviceEnum);
        rc = dsoundDevicesEnumerate(pThis, pDeviceEnum);
        if (RT_FAILURE(rc))
            PDMAudioHostEnumDelete(pDeviceEnum);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostDSoundHA_Shutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVHOSTDSOUND pThis = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);

    LogFlowFuncEnter();

    RT_NOREF(pThis);

    LogFlowFuncLeave();
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostDSoundHA_Init(PPDMIHOSTAUDIO pInterface)
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

        rc = VINF_SUCCESS;

        dsoundUpdateStatusInternal(pThis);
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

static int dsoundConfigInit(PDRVHOSTDSOUND pThis, PCFGMNODE pCfg)
{
    pThis->Cfg.pGuidPlay    = dsoundConfigQueryGUID(pCfg, "DeviceGuidOut", &pThis->Cfg.uuidPlay);
    pThis->Cfg.pGuidCapture = dsoundConfigQueryGUID(pCfg, "DeviceGuidIn",  &pThis->Cfg.uuidCapture);

    DSLOG(("DSound: Configuration: DeviceGuidOut {%RTuuid}, DeviceGuidIn {%RTuuid}\n",
           &pThis->Cfg.uuidPlay,
           &pThis->Cfg.uuidCapture));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostDSoundHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                      PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = dsoundCreateStreamIn(pThis,  pStreamDS, pCfgReq, pCfgAcq);
    else
        rc = dsoundCreateStreamOut(pThis, pStreamDS, pCfgReq, pCfgAcq);

    if (RT_SUCCESS(rc))
    {
        /** @todo already copied   */
        rc = PDMAudioStrmCfgCopy(&pStreamDS->Cfg, pCfgAcq);
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&pStreamDS->CritSect);
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = dsoundDestroyStreamIn(pThis, pStreamDS);
    else
        rc = dsoundDestroyStreamOut(pThis, pStreamDS);

    if (RT_SUCCESS(rc))
    {
        if (RTCritSectIsInitialized(&pStreamDS->CritSect))
            rc = RTCritSectDelete(&pStreamDS->CritSect);
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVHOSTDSOUND pThis     = PDMIHOSTAUDIO_2_DRVHOSTDSOUND(pInterface);
    PDSOUNDSTREAM  pStreamDS = (PDSOUNDSTREAM)pStream;

    int rc;
    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = dsoundControlStreamIn(pThis,  pStreamDS, enmStreamCmd);
    else
        rc = dsoundControlStreamOut(pThis, pStreamDS, enmStreamCmd);

    return rc;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    /*PDRVHOSTDSOUND  pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTDSOUND, IHostAudio); */ RT_NOREF(pInterface);
    PDSOUNDSTREAM   pStreamDS = (PDSOUNDSTREAM)pStream;
    AssertPtrReturn(pStreamDS, 0);
    Assert(pStreamDS->Cfg.enmDir == PDMAUDIODIR_IN);

    if (pStreamDS->fEnabled)
    {
        /* This is the same calculation as for StreamGetPending. */
        AssertPtr(pStreamDS->In.pDSCB);
        DWORD   offCaptureCursor = 0;
        DWORD   offReadCursor    = 0;
        HRESULT hrc = IDirectSoundCaptureBuffer_GetCurrentPosition(pStreamDS->In.pDSCB, &offCaptureCursor, &offReadCursor);
        if (SUCCEEDED(hrc))
        {
            uint32_t cbPending = dsoundRingDistance(offCaptureCursor, offReadCursor, pStreamDS->cbBufSize);
            Log3Func(("cbPending=%RU32\n", cbPending));
            return cbPending;
        }
        AssertMsgFailed(("hrc=%Rhrc\n", hrc));
    }

    return 0;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVHOSTDSOUND  pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTDSOUND, IHostAudio);
    PDSOUNDSTREAM   pStreamDS = (PDSOUNDSTREAM)pStream;
    AssertPtrReturn(pStreamDS, 0);

    DWORD           cbFree    = 0;
    DWORD           offIgn    = 0;
    int rc = dsoundGetFreeOut(pThis, pStreamDS, &cbFree, &offIgn);
    AssertRCReturn(rc, 0);

    return cbFree;
}

#if 0 /* This isn't working as the write cursor is more a function of time than what we do.
         Previously we only reported the pre-buffering status anyway, so no harm. */
/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetPending}
 */
static DECLCALLBACK(uint32_t) drvHostDSoundHA_StreamGetPending(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    /*PDRVHOSTDSOUND  pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTDSOUND, IHostAudio); */ RT_NOREF(pInterface);
    PDSOUNDSTREAM   pStreamDS = (PDSOUNDSTREAM)pStream;
    AssertPtrReturn(pStreamDS, 0);

    if (pStreamDS->Cfg.enmDir == PDMAUDIODIR_OUT)
    {
        /* This is a similar calculation as for StreamGetReadable, only for an output buffer. */
        AssertPtr(pStreamDS->In.pDSCB);
        DWORD   offPlayCursor  = 0;
        DWORD   offWriteCursor = 0;
        HRESULT hrc = IDirectSoundBuffer8_GetCurrentPosition(pStreamDS->Out.pDSB, &offPlayCursor, &offWriteCursor);
        if (SUCCEEDED(hrc))
        {
            uint32_t cbPending = dsoundRingDistance(offWriteCursor, offPlayCursor, pStreamDS->cbBufSize);
            Log3Func(("cbPending=%RU32\n", cbPending));
            return cbPending;
        }
        AssertMsgFailed(("hrc=%Rhrc\n", hrc));
    }
    /* else: For input streams we never have any pending data. */

    return 0;
}
#endif

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostDSoundHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMAUDIOSTREAMSTS_FLAGS_NONE);

    PDSOUNDSTREAM pStreamDS = (PDSOUNDSTREAM)pStream;

    PDMAUDIOSTREAMSTS fStrmStatus = PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED;

    if (pStreamDS->fEnabled)
        fStrmStatus |= PDMAUDIOSTREAMSTS_FLAGS_ENABLED;

    return fStrmStatus;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostDSoundHA_StreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
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

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    if (pThis->m_pNotificationClient)
    {
        pThis->m_pNotificationClient->Unregister();
        pThis->m_pNotificationClient->Release();

        pThis->m_pNotificationClient = NULL;
    }
#endif

    PDMAudioHostEnumDelete(&pThis->DeviceEnum);

    if (pThis->pDrvIns)
        CoUninitialize();

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

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
    pThis->IHostAudio.pfnInit               = drvHostDSoundHA_Init;
    pThis->IHostAudio.pfnShutdown           = drvHostDSoundHA_Shutdown;
    pThis->IHostAudio.pfnGetConfig          = drvHostDSoundHA_GetConfig;
    pThis->IHostAudio.pfnGetStatus          = drvHostDSoundHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostDSoundHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostDSoundHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostDSoundHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostDSoundHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostDSoundHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostDSoundHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamIterate      = drvHostDSoundHA_StreamIterate;
    pThis->IHostAudio.pfnStreamPlay         = drvHostDSoundHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostDSoundHA_StreamCapture;
    pThis->IHostAudio.pfnGetDevices         = drvHostDSoundHA_GetDevices;
    pThis->IHostAudio.pfnStreamGetPending   = NULL;

    /*
     * Init the static parts.
     */
    PDMAudioHostEnumInit(&pThis->DeviceEnum);

    pThis->fEnabledIn  = false;
    pThis->fEnabledOut = false;

    int rc = VINF_SUCCESS;

#ifdef VBOX_WITH_AUDIO_MMNOTIFICATION_CLIENT
    bool fUseNotificationClient = false;

    char szOSVersion[32];
    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szOSVersion, sizeof(szOSVersion));
    if (RT_SUCCESS(rc))
    {
        /* IMMNotificationClient is available starting at Windows Vista. */
        if (RTStrVersionCompare(szOSVersion, "6.0") >= 0)
            fUseNotificationClient = true;
    }

    if (fUseNotificationClient)
    {
        /* Get the notification interface. */
# ifdef VBOX_WITH_AUDIO_CALLBACKS
        PPDMIAUDIONOTIFYFROMHOST pIAudioNotifyFromHost = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIONOTIFYFROMHOST);
        Assert(pIAudioNotifyFromHost);
# else
        PPDMIAUDIONOTIFYFROMHOST pIAudioNotifyFromHost = NULL;
# endif

        try
        {
            pThis->m_pNotificationClient = new DrvHostAudioDSoundMMNotifClient(pIAudioNotifyFromHost);

            hr = pThis->m_pNotificationClient->Initialize();
            if (SUCCEEDED(hr))
                hr = pThis->m_pNotificationClient->Register();

            if (FAILED(hr))
                rc = VERR_AUDIO_BACKEND_INIT_FAILED;
        }
        catch (std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    LogRel2(("DSound: Notification client is %s\n", fUseNotificationClient ? "enabled" : "disabled"));
#endif

    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize configuration values.
         */
        rc = dsoundConfigInit(pThis, pCfg);
        if (RT_SUCCESS(rc))
            rc = RTCritSectInit(&pThis->CritSect);
    }

    return rc;
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
