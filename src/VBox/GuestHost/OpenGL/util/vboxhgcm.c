/* $Id$ */

/** @file
 * VBox HGCM connection
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifdef RT_OS_WINDOWS
    #include <windows.h>
    #include <ddraw.h>
#else
    #include <sys/ioctl.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <string.h>
    #include <unistd.h>
#endif

#include "cr_error.h"
#include "cr_net.h"
#include "cr_bufpool.h"
#include "cr_mem.h"
#include "cr_string.h"
#include "cr_endian.h"
#include "cr_threads.h"
#include "net_internals.h"

#include <VBox/VBoxGuest.h>
#include <VBox/HostServices/VBoxCrOpenGLSvc.h>

typedef struct {
    int                  initialized;
    int                  num_conns;
    CRConnection         **conns;
    CRBufferPool         *bufpool;
#ifdef CHROMIUM_THREADSAFE
    CRmutex              mutex;
    CRmutex              recvmutex;
#endif
    CRNetReceiveFuncList *recv_list;
    CRNetCloseFuncList   *close_list;
#ifdef RT_OS_WINDOWS
    HANDLE               hGuestDrv;
    LPDIRECTDRAW         pDirectDraw;
#else
    int                  iGuestDrv;
#endif
} CRVBOXHGCMDATA;

static CRVBOXHGCMDATA g_crvboxhgcm = {0,};

typedef enum {
    CR_VBOXHGCM_USERALLOCATED,
    CR_VBOXHGCM_MEMORY,
    CR_VBOXHGCM_MEMORY_BIG
#ifdef RT_OS_WINDOWS
    ,CR_VBOXHGCM_DDRAW_SURFACE
#endif
} CRVBOXHGCMBUFFERKIND;

#define CR_VBOXHGCM_BUFFER_MAGIC 0xABCDE321

typedef struct CRVBOXHGCMBUFFER {
    uint32_t             magic;
    CRVBOXHGCMBUFFERKIND kind;
    uint32_t             len;
    uint32_t             allocated;
#ifdef RT_OS_WINDOWS
    LPDIRECTDRAWSURFACE  pDDS;
#endif
} CRVBOXHGCMBUFFER;

#ifndef RT_OS_WINDOWS
    #define TRUE true
    #define FALSE false
    #define INVALID_HANDLE_VALUE (-1)
#endif

/* Some forward declarations */
static void crVBoxHGCMReceiveMessage(CRConnection *conn);

#ifndef IN_GUEST
static bool _crVBoxHGCMReadBytes(CRConnection *conn, void *buf, uint32_t len)
{
    CRASSERT(conn && buf);

    if (!conn->pBuffer || (conn->cbBuffer<len))
        return FALSE;

    crMemcpy(buf, conn->pBuffer, len);

    conn->cbBuffer -= len;
    conn->pBuffer = conn->cbBuffer>0 ? (uint8_t*)conn->pBuffer+len : NULL;

    return TRUE;
}
#endif

/*@todo get rid of it*/
static bool _crVBoxHGCMWriteBytes(CRConnection *conn, const void *buf, uint32_t len)
{
    CRASSERT(conn && buf);

    /* make sure there's host buffer and it's clear */
    CRASSERT(conn->pHostBuffer && !conn->cbHostBuffer);

    if (conn->cbHostBufferAllocated < len)
    {
        crDebug("Host buffer too small %d out of requsted %d bytes, reallocating", conn->cbHostBufferAllocated, len);
        crFree(conn->pHostBuffer);
        conn->pHostBuffer = crAlloc(len);
        if (!conn->pHostBuffer)
        {
            conn->cbHostBufferAllocated = 0;
            crError("OUT_OF_MEMORY trying to allocate %d bytes", len);
            return FALSE;
        }
        conn->cbHostBufferAllocated = len;
    }

    crMemcpy(conn->pHostBuffer, buf, len);
    conn->cbHostBuffer = len;

    return TRUE;
}

/**
 * Send an HGCM request
 *
 * @return VBox status code
 * @param   pvData      Data pointer
 * @param   cbData      Data size
 */
/*@todo use vbglR3DoIOCtl here instead */
static int crVBoxHGCMCall(void *pvData, unsigned cbData)
{
#ifdef IN_GUEST

#ifdef RT_OS_WINDOWS
    DWORD cbReturned;

    if (DeviceIoControl (g_crvboxhgcm.hGuestDrv,
                         VBOXGUEST_IOCTL_HGCM_CALL(cbData),
                         pvData, cbData,
                         pvData, cbData,
                         &cbReturned,
                         NULL))
    {
        return VINF_SUCCESS;
    }
    crDebug("vboxCall failed with %x\n", GetLastError());
    return VERR_NOT_SUPPORTED;
#else
    if (ioctl(g_crvboxhgcm.iGuestDrv, VBOXGUEST_IOCTL_HGCM_CALL(cbData), pvData) >= 0)
    {
        return VINF_SUCCESS;
    }
    crWarning("vboxCall failed with %x\n", errno);
    return VERR_NOT_SUPPORTED;
#endif /*#ifdef RT_OS_WINDOWS*/

#else /*#ifdef IN_GUEST*/
    crError("crVBoxHGCMCall called on host side!");
    CRASSERT(FALSE);
    return VERR_NOT_SUPPORTED;
#endif
}

static void *crVBoxHGCMAlloc(CRConnection *conn)
{
    CRVBOXHGCMBUFFER *buf;

#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&g_crvboxhgcm.mutex);
#endif

    buf = (CRVBOXHGCMBUFFER *) crBufferPoolPop(g_crvboxhgcm.bufpool, conn->buffer_size);

    if (!buf)
    {
        crDebug("Buffer pool %p was empty; allocating new %d byte buffer.",
                        (void *) g_crvboxhgcm.bufpool,
                        (unsigned int)sizeof(CRVBOXHGCMBUFFER) + conn->buffer_size);

#if defined(IN_GUEST) && defined(RT_OS_WINDOWS)
        /* Try to start DDRAW on guest side */
        if (!g_crvboxhgcm.pDirectDraw && 0)
        {
            HRESULT hr;

            hr = DirectDrawCreate(NULL, &g_crvboxhgcm.pDirectDraw, NULL);
            if (hr != DD_OK)
            {
                crWarning("Failed to create DirectDraw interface (%x)\n", hr);
                g_crvboxhgcm.pDirectDraw = NULL;
            }
            else
            {
                hr = IDirectDraw_SetCooperativeLevel(g_crvboxhgcm.pDirectDraw, NULL, DDSCL_NORMAL);
                if (hr != DD_OK)
                {
                    crWarning("Failed to SetCooperativeLevel (%x)\n", hr);
                    IDirectDraw_Release(g_crvboxhgcm.pDirectDraw);
                    g_crvboxhgcm.pDirectDraw = NULL;
                }
                crDebug("Created DirectDraw and set CooperativeLevel successfully\n");
            }
        }

        /* Try to allocate buffer via DDRAW */
        if (g_crvboxhgcm.pDirectDraw)
        {
            DDSURFACEDESC       ddsd;
            HRESULT             hr;
            LPDIRECTDRAWSURFACE lpDDS;

            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwSize  = sizeof(ddsd);

            /* @todo DDSCAPS_VIDEOMEMORY ain't working for some reason
             * also, it would be better to request dwLinearSize but it fails too
             * ddsd.dwLinearSize = sizeof(CRVBOXHGCMBUFFER) + conn->buffer_size;
             */

            ddsd.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
            ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            /* use 1 byte per pixel format */
            ddsd.ddpfPixelFormat.dwSize = sizeof(ddsd.ddpfPixelFormat);
            ddsd.ddpfPixelFormat.dwFlags = DDPF_RGB;
            ddsd.ddpfPixelFormat.dwRGBBitCount = 8;
            ddsd.ddpfPixelFormat.dwRBitMask = 0xFF;
            ddsd.ddpfPixelFormat.dwGBitMask = 0;
            ddsd.ddpfPixelFormat.dwBBitMask = 0;
            /* request given buffer size, rounded to 1k */
            ddsd.dwWidth = 1024;
            ddsd.dwHeight = (sizeof(CRVBOXHGCMBUFFER) + conn->buffer_size + ddsd.dwWidth-1)/ddsd.dwWidth;

            hr = IDirectDraw_CreateSurface(g_crvboxhgcm.pDirectDraw, &ddsd, &lpDDS, NULL);
            if (hr != DD_OK)
            {
                crWarning("Failed to create DirectDraw surface (%x)\n", hr);
            }
            else
            {
                crDebug("Created DirectDraw surface (%x)\n", lpDDS);

                hr = IDirectDrawSurface_Lock(lpDDS, NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR, NULL);
                if (hr != DD_OK)
                {
                    crWarning("Failed to lock DirectDraw surface (%x)\n", hr);
                    IDirectDrawSurface_Release(lpDDS);
                }
                else
                {
                    uint32_t cbLocked;
                    cbLocked = (ddsd.dwFlags & DDSD_LINEARSIZE) ? ddsd.dwLinearSize : ddsd.lPitch*ddsd.dwHeight;

                    crDebug("Locked %d bytes DirectDraw surface\n", cbLocked);

                    buf = (CRVBOXHGCMBUFFER *) ddsd.lpSurface;
                    CRASSERT(buf);
                    buf->magic = CR_VBOXHGCM_BUFFER_MAGIC;
                    buf->kind  = CR_VBOXHGCM_DDRAW_SURFACE;
                    buf->allocated = cbLocked;
                    buf->pDDS = lpDDS;
                }
            }
        }
#endif

        /* We're either on host side, or we failed to allocate DDRAW buffer */
        if (!buf)
        {
            crDebug("Using system malloc\n");
            buf = (CRVBOXHGCMBUFFER *) crAlloc( sizeof(CRVBOXHGCMBUFFER) + conn->buffer_size );
            CRASSERT(buf);
            buf->magic = CR_VBOXHGCM_BUFFER_MAGIC;
            buf->kind  = CR_VBOXHGCM_MEMORY;
            buf->allocated = conn->buffer_size;
#ifdef RT_OS_WINDOWS
            buf->pDDS = NULL;
#endif
        }
    }

#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&g_crvboxhgcm.mutex);
#endif

    return (void *)( buf + 1 );

}

static void crVBoxHGCMWriteExact(CRConnection *conn, const void *buf, unsigned int len)
{
    CRVBOXHGCMWRITE parms;
    int rc;

    parms.hdr.result      = VINF_SUCCESS;
    parms.hdr.u32ClientID = conn->u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_WRITE;
    parms.hdr.cParms      = SHCRGL_CPARMS_WRITE;

    parms.pBuffer.type                   = VMMDevHGCMParmType_LinAddr_In;
    parms.pBuffer.u.Pointer.size         = len;
    parms.pBuffer.u.Pointer.u.linearAddr = (VMMDEVHYPPTR) buf;

    rc = crVBoxHGCMCall(&parms, sizeof(parms));

    if (RT_FAILURE(rc) || RT_FAILURE(parms.hdr.result))
    {
        crWarning("SHCRGL_GUEST_FN_WRITE failed with %x %x\n", rc, parms.hdr.result);
    }
}

static void crVBoxHGCMReadExact( CRConnection *conn, const void *buf, unsigned int len )
{
    CRVBOXHGCMREAD parms;
    int rc;

    parms.hdr.result      = VINF_SUCCESS;
    parms.hdr.u32ClientID = conn->u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_READ;
    parms.hdr.cParms      = SHCRGL_CPARMS_READ;

    CRASSERT(!conn->pBuffer); //make sure there's no data to process
    parms.pBuffer.type                   = VMMDevHGCMParmType_LinAddr_Out;
    parms.pBuffer.u.Pointer.size         = conn->cbHostBufferAllocated;
    parms.pBuffer.u.Pointer.u.linearAddr = (VMMDEVHYPPTR) conn->pHostBuffer;

    parms.cbBuffer.type      = VMMDevHGCMParmType_32bit;
    parms.cbBuffer.u.value32 = 0;

    rc = crVBoxHGCMCall(&parms, sizeof(parms));

    if (RT_FAILURE(rc) || RT_FAILURE(parms.hdr.result))
    {
        crWarning("SHCRGL_GUEST_FN_READ failed with %x %x\n", rc, parms.hdr.result);
        return;
    }

    if (parms.cbBuffer.u.value32)
    {
        //conn->pBuffer  = (uint8_t*) parms.pBuffer.u.Pointer.u.linearAddr;
        conn->pBuffer  = conn->pHostBuffer;
        conn->cbBuffer = parms.cbBuffer.u.value32;
    }

    if (conn->cbBuffer)
        crVBoxHGCMReceiveMessage(conn);

}

/* Same as crVBoxHGCMWriteExact, but combined with read of writeback data.
 * This halves the number of HGCM calls we do,
 * most likely crVBoxHGCMPollHost shouldn't be called at all now.
 */
static void
crVBoxHGCMWriteReadExact(CRConnection *conn, const void *buf, unsigned int len, CRVBOXHGCMBUFFERKIND bufferKind)
{
    CRVBOXHGCMWRITEREAD parms;
    int rc;

    parms.hdr.result      = VINF_SUCCESS;
    parms.hdr.u32ClientID = conn->u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_WRITE_READ;
    parms.hdr.cParms      = SHCRGL_CPARMS_WRITE_READ;

    //if (bufferKind != CR_VBOXHGCM_DDRAW_SURFACE)
    {
        parms.pBuffer.type                   = VMMDevHGCMParmType_LinAddr_In;
        parms.pBuffer.u.Pointer.size         = len;
        parms.pBuffer.u.Pointer.u.linearAddr = (VMMDEVHYPPTR) buf;
    }
    /*else //@todo it fails badly, have to check why
    {
        parms.pBuffer.type                 = VMMDevHGCMParmType_PhysAddr;
        parms.pBuffer.u.Pointer.size       = len;
        parms.pBuffer.u.Pointer.u.physAddr = (VMMDEVHYPPHYS32) buf;
    }*/

    CRASSERT(!conn->pBuffer); //make sure there's no data to process
    parms.pWriteback.type                   = VMMDevHGCMParmType_LinAddr_Out;
    parms.pWriteback.u.Pointer.size         = conn->cbHostBufferAllocated;
    parms.pWriteback.u.Pointer.u.linearAddr = (VMMDEVHYPPTR) conn->pHostBuffer;

    parms.cbWriteback.type      = VMMDevHGCMParmType_32bit;
    parms.cbWriteback.u.value32 = 0;

    rc = crVBoxHGCMCall(&parms, sizeof(parms));

    if (RT_FAILURE(rc) || RT_FAILURE(parms.hdr.result))
    {

        if ((VERR_BUFFER_OVERFLOW == parms.hdr.result) && RT_SUCCESS(rc))
        {
            /* reallocate buffer and retry */

            CRASSERT(parms.cbWriteback.u.value32>conn->cbHostBufferAllocated);

            crDebug("Reallocating host buffer from %d to %d bytes", conn->cbHostBufferAllocated, parms.cbWriteback.u.value32);

            crFree(conn->pHostBuffer);
            conn->cbHostBufferAllocated = parms.cbWriteback.u.value32;
            conn->pHostBuffer = crAlloc(conn->cbHostBufferAllocated);

            crVBoxHGCMReadExact(conn, buf, len);

            return;
        }
        else
        {
            crWarning("SHCRGL_GUEST_FN_WRITE_READ (%i) failed with %x %x\n", len, rc, parms.hdr.result);
            return;
        }
    }

    if (parms.cbWriteback.u.value32)
    {
        //conn->pBuffer  = (uint8_t*) parms.pWriteback.u.Pointer.u.linearAddr;
        conn->pBuffer  = conn->pHostBuffer;
        conn->cbBuffer = parms.cbWriteback.u.value32;
    }

    if (conn->cbBuffer)
        crVBoxHGCMReceiveMessage(conn);
}

static void crVBoxHGCMSend(CRConnection *conn, void **bufp,
                           const void *start, unsigned int len)
{
    CRVBOXHGCMBUFFER *hgcm_buffer;

    if (!bufp) /* We're sending a user-allocated buffer. */
    {
#ifndef IN_GUEST
            //@todo remove temp buffer allocation in unpacker
            /* we're at the host side, so just store data until guest polls us */
            _crVBoxHGCMWriteBytes(conn, start, len);
#else
            crDebug("SHCRGL: sending userbuf with %d bytes\n", len);
            crVBoxHGCMWriteReadExact(conn, start, len, CR_VBOXHGCM_USERALLOCATED);
#endif
        return;
    }

    /* The region [start .. start + len + 1] lies within a buffer that
     * was allocated with crVBoxHGCMAlloc() and can be put into the free
     * buffer pool when we're done sending it.
     */

    hgcm_buffer = (CRVBOXHGCMBUFFER *)(*bufp) - 1;
    CRASSERT(hgcm_buffer->magic == CR_VBOXHGCM_BUFFER_MAGIC);

    /* Length would be passed as part of HGCM pointer description
     * No need to prepend it to the buffer
     */
    crVBoxHGCMWriteReadExact(conn, start, len, hgcm_buffer->kind);

    /* Reclaim this pointer for reuse */
#ifdef CHROMIUM_THREADSAFE
    crLockMutex(&g_crvboxhgcm.mutex);
#endif
    crBufferPoolPush(g_crvboxhgcm.bufpool, hgcm_buffer, hgcm_buffer->allocated);
#ifdef CHROMIUM_THREADSAFE
    crUnlockMutex(&g_crvboxhgcm.mutex);
#endif

    /* Since the buffer's now in the 'free' buffer pool, the caller can't
     * use it any more.  Setting bufp to NULL will make sure the caller
     * doesn't try to re-use the buffer.
     */
    *bufp = NULL;
}

static void crVBoxHGCMPollHost(CRConnection *conn)
{
    CRVBOXHGCMREAD parms;
    int rc;

    CRASSERT(!conn->pBuffer);

    parms.hdr.result      = VINF_SUCCESS;
    parms.hdr.u32ClientID = conn->u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_READ;
    parms.hdr.cParms      = SHCRGL_CPARMS_READ;

    parms.pBuffer.type                   = VMMDevHGCMParmType_LinAddr_Out;
    parms.pBuffer.u.Pointer.size         = conn->cbHostBufferAllocated;
    parms.pBuffer.u.Pointer.u.linearAddr = (VMMDEVHYPPTR) conn->pHostBuffer;

    parms.cbBuffer.type      = VMMDevHGCMParmType_32bit;
    parms.cbBuffer.u.value32 = 0;

    rc = crVBoxHGCMCall(&parms, sizeof(parms));

    if (RT_FAILURE(rc) || RT_FAILURE(parms.hdr.result))
    {
        crDebug("SHCRGL_GUEST_FN_READ failed with %x %x\n", rc, parms.hdr.result);
        return;
    }

    if (parms.cbBuffer.u.value32)
    {
        conn->pBuffer = (uint8_t*) parms.pBuffer.u.Pointer.u.linearAddr;
        conn->cbBuffer = parms.cbBuffer.u.value32;
    }
}

static void crVBoxHGCMSingleRecv(CRConnection *conn, void *buf, unsigned int len)
{
    crVBoxHGCMReadExact(conn, buf, len);
}

static void crVBoxHGCMFree(CRConnection *conn, void *buf)
{
    CRVBOXHGCMBUFFER *hgcm_buffer = (CRVBOXHGCMBUFFER *) buf - 1;

    CRASSERT(hgcm_buffer->magic == CR_VBOXHGCM_BUFFER_MAGIC);

    /*@todo wrong len for redir buffers*/
    conn->recv_credits += hgcm_buffer->len;

    switch (hgcm_buffer->kind)
    {
        case CR_VBOXHGCM_MEMORY:
#ifdef RT_OS_WINDOWS
        case CR_VBOXHGCM_DDRAW_SURFACE:
#endif
#ifdef CHROMIUM_THREADSAFE
            crLockMutex(&g_crvboxhgcm.mutex);
#endif
            if (g_crvboxhgcm.bufpool) {
                //@todo o'rly?
                /* pool may have been deallocated just a bit earlier in response
                 * to a SIGPIPE (Broken Pipe) signal.
                 */
                crBufferPoolPush(g_crvboxhgcm.bufpool, hgcm_buffer, hgcm_buffer->allocated);
            }
#ifdef CHROMIUM_THREADSAFE
            crUnlockMutex(&g_crvboxhgcm.mutex);
#endif
            break;

        case CR_VBOXHGCM_MEMORY_BIG:
            crFree( hgcm_buffer );
            break;

        default:
            crError( "Weird buffer kind trying to free in crVBoxHGCMFree: %d", hgcm_buffer->kind );
    }
}

static void crVBoxHGCMReceiveMessage(CRConnection *conn)
{
    uint32_t len;
    CRVBOXHGCMBUFFER *hgcm_buffer;
    CRMessage *msg;
    CRMessageType cached_type;

    len = conn->cbBuffer;
    CRASSERT(len > 0);
    CRASSERT(conn->pBuffer);

#ifndef IN_GUEST
    if (conn->allow_redir_ptr)
    {
#endif //IN_GUEST
        CRASSERT(conn->buffer_size >= sizeof(CRMessageRedirPtr));

        hgcm_buffer = (CRVBOXHGCMBUFFER *) crVBoxHGCMAlloc( conn ) - 1;
        hgcm_buffer->len = sizeof(CRMessageRedirPtr);

        msg = (CRMessage *) (hgcm_buffer + 1);

        msg->header.type = CR_MESSAGE_REDIR_PTR;
        msg->redirptr.pMessage = (CRMessageHeader*) (conn->pBuffer);
        msg->header.conn_id = msg->redirptr.pMessage->conn_id;

        cached_type = msg->redirptr.pMessage->type;

        conn->cbBuffer = 0;
        conn->pBuffer  = NULL;
#ifndef IN_GUEST
    }
    else
    {
        if ( len <= conn->buffer_size )
        {
            /* put in pre-allocated buffer */
            hgcm_buffer = (CRVBOXHGCMBUFFER *) crVBoxHGCMAlloc( conn ) - 1;
        }
        else
        {
            /* allocate new buffer,
             * not using pool here as it's most likely one time transfer of huge texture
             */
            hgcm_buffer            = (CRVBOXHGCMBUFFER *) crAlloc( sizeof(CRVBOXHGCMBUFFER) + len );
            hgcm_buffer->magic     = CR_VBOXHGCM_BUFFER_MAGIC;
            hgcm_buffer->kind      = CR_VBOXHGCM_MEMORY_BIG;
            hgcm_buffer->allocated = sizeof(CRVBOXHGCMBUFFER) + len;
# ifdef RT_OS_WINDOWS
            hgcm_buffer->pDDS      = NULL;
# endif
        }

        hgcm_buffer->len = len;

        _crVBoxHGCMReadBytes(conn, hgcm_buffer + 1, len);

        msg = (CRMessage *) (hgcm_buffer + 1);
        cached_type = msg->header.type;
    }
#endif //IN_GUEST

    conn->recv_credits     -= len;
    conn->total_bytes_recv += len;

    crNetDispatchMessage( g_crvboxhgcm.recv_list, conn, msg, len );

    /* CR_MESSAGE_OPCODES is freed in crserverlib/server_stream.c with crNetFree.
     * OOB messages are the programmer's problem.  -- Humper 12/17/01
     */
    if (cached_type != CR_MESSAGE_OPCODES
        && cached_type != CR_MESSAGE_OOB
        && cached_type != CR_MESSAGE_GATHER)
    {
        crVBoxHGCMFree(conn, msg);
    }
}

/*
 * Called on host side only, to "accept" client connection
 */
static void crVBoxHGCMAccept( CRConnection *conn, const char *hostname, unsigned short port )
{
    CRASSERT(conn && conn->pHostBuffer);
#ifdef IN_GUEST
    CRASSERT(FALSE);
#endif
}

/**
 * The function that actually connects.  This should only be called by clients,
 * guests in vbox case.
 * Servers go through crVBoxHGCMAccept;
 */
/*@todo use vbglR3Something here */
static int crVBoxHGCMDoConnect( CRConnection *conn )
{
#ifdef IN_GUEST
    VBoxGuestHGCMConnectInfo info;

#ifdef RT_OS_WINDOWS
    DWORD cbReturned;

    if (g_crvboxhgcm.hGuestDrv == INVALID_HANDLE_VALUE)
    {
        /* open VBox guest driver */
        g_crvboxhgcm.hGuestDrv = CreateFile(VBOXGUEST_DEVICE_NAME,
                                        GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL);

        /* @todo check if we could rollback to softwareopengl */
        if (g_crvboxhgcm.hGuestDrv == INVALID_HANDLE_VALUE)
        {
            crDebug("could not open VBox Guest Additions driver! rc = %d\n", GetLastError());
            return FALSE;
        }
    }
#else
    if (g_crvboxhgcm.iGuestDrv == INVALID_HANDLE_VALUE)
    {
        g_crvboxhgcm.iGuestDrv = open(VBOXGUEST_USER_DEVICE_NAME, O_RDWR, 0);
        if (g_crvboxhgcm.iGuestDrv == INVALID_HANDLE_VALUE)
        {
            crDebug("could not open Guest Additions kernel module! rc = %d\n", errno);
            return FALSE;
        }
    }
#endif

    memset (&info, 0, sizeof (info));
    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    strcpy (info.Loc.u.host.achName, "VBoxSharedCrOpenGL");

#ifdef RT_OS_WINDOWS
    if (DeviceIoControl(g_crvboxhgcm.hGuestDrv,
                        VBOXGUEST_IOCTL_HGCM_CONNECT,
                        &info, sizeof (info),
                        &info, sizeof (info),
                        &cbReturned,
                        NULL))
#else
    /*@todo it'd fail */
    if (ioctl(g_crvboxhgcm.iGuestDrv, VBOXGUEST_IOCTL_HGCM_CONNECT, &info, sizeof (info)) >= 0)
#endif
    {
        if (info.result == VINF_SUCCESS)
        {
            conn->u32ClientID = info.u32ClientID;
            crDebug("HGCM connect was successful: client id =%x\n", conn->u32ClientID);
        }
        else
        {
            crDebug("HGCM connect failed with rc=%x\n", info.result);
            return FALSE;
        }
    }
    else
    {
#ifdef RT_OS_WINDOWS
        crDebug("HGCM connect failed with rc=%x\n", GetLastError());
#else
        crDebug("HGCM connect failed with rc=%x\n", errno);
#endif
        return FALSE;
    }

    return TRUE;

#else /*#ifdef IN_GUEST*/
    crError("crVBoxHGCMDoConnect called on host side!");
    CRASSERT(FALSE);
    return FALSE;
#endif
}

/*@todo same, replace DeviceIoControl with vbglR3DoIOCtl */
static void crVBoxHGCMDoDisconnect( CRConnection *conn )
{
    VBoxGuestHGCMDisconnectInfo info;
#ifdef RT_OS_WINDOWS
    DWORD cbReturned;
#endif
    int i;

    if (conn->pHostBuffer)
    {
        crFree(conn->pHostBuffer);
        conn->pHostBuffer = NULL;
        conn->cbHostBuffer = 0;
        conn->cbHostBufferAllocated = 0;
    }

    conn->pBuffer = NULL;
    conn->cbBuffer = 0;

    //@todo hold lock here?
    if (conn->type == CR_VBOXHGCM)
    {
        --g_crvboxhgcm.num_conns;

        if (conn->index < g_crvboxhgcm.num_conns)
        {
            g_crvboxhgcm.conns[conn->index] = g_crvboxhgcm.conns[g_crvboxhgcm.num_conns];
            g_crvboxhgcm.conns[conn->index]->index = conn->index;
        }
        else g_crvboxhgcm.conns[conn->index] = NULL;

        conn->type = CR_NO_CONNECTION;
    }

#ifndef IN_GUEST
#else
    if (conn->u32ClientID)
    {
        memset (&info, 0, sizeof (info));
        info.u32ClientID = conn->u32ClientID;

#ifdef RT_OS_WINDOWS
        if ( !DeviceIoControl(g_crvboxhgcm.hGuestDrv,
                               VBOXGUEST_IOCTL_HGCM_DISCONNECT,
                               &info, sizeof (info),
                               &info, sizeof (info),
                               &cbReturned,
                               NULL) )
        {
            crDebug("Disconnect failed with %x\n", GetLastError());
        }
#else
        if (ioctl(g_crvboxhgcm.iGuestDrv, VBOXGUEST_IOCTL_HGCM_DISCONNECT, &info, sizeof (info)) < 0)
        {
            crDebug("Disconnect failed with %x\n", errno);
        }
#endif

        conn->u32ClientID = 0;
    }

    /* see if any connections remain */
    for (i = 0; i < g_crvboxhgcm.num_conns; i++)
        if (g_crvboxhgcm.conns[i] && g_crvboxhgcm.conns[i]->type != CR_NO_CONNECTION)
            break;

    /* close guest additions driver*/
    if (i>=g_crvboxhgcm.num_conns)
    {
#ifdef RT_OS_WINDOWS
        CloseHandle(g_crvboxhgcm.hGuestDrv);
        g_crvboxhgcm.hGuestDrv = INVALID_HANDLE_VALUE;
#else
        close(g_crvboxhgcm.iGuestDrv);
        g_crvboxhgcm.iGuestDrv = INVALID_HANDLE_VALUE;
#endif
    }
#endif
}

static void crVBoxHGCMInstantReclaim(CRConnection *conn, CRMessage *mess)
{
    crVBoxHGCMFree(conn, mess);
    CRASSERT(FALSE);
}

static void crVBoxHGCMHandleNewMessage( CRConnection *conn, CRMessage *msg, unsigned int len )
{
    CRASSERT(FALSE);
}

void crVBoxHGCMInit(CRNetReceiveFuncList *rfl, CRNetCloseFuncList *cfl, unsigned int mtu)
{
    (void) mtu;

    g_crvboxhgcm.recv_list = rfl;
    g_crvboxhgcm.close_list = cfl;
    if (g_crvboxhgcm.initialized)
    {
        return;
    }

    g_crvboxhgcm.initialized = 1;

    g_crvboxhgcm.num_conns = 0;
    g_crvboxhgcm.conns     = NULL;

    /* Can't open VBox guest driver here, because it gets called for host side as well */
    /*@todo as we have 2 dll versions, can do it now.*/

#ifdef RT_OS_WINDOWS
    g_crvboxhgcm.hGuestDrv = INVALID_HANDLE_VALUE;
    g_crvboxhgcm.pDirectDraw = NULL;
#else
    g_crvboxhgcm.iGuestDrv = INVALID_HANDLE_VALUE;
#endif

#ifdef CHROMIUM_THREADSAFE
    crInitMutex(&g_crvboxhgcm.mutex);
    crInitMutex(&g_crvboxhgcm.recvmutex);
#endif
    g_crvboxhgcm.bufpool = crBufferPoolInit(16);
}

/* Callback function used to free buffer pool entries */
void crVBoxHGCMBufferFree(void *data)
{
#ifdef RT_OS_WINDOWS
    LPDIRECTDRAWSURFACE lpDDS;
#endif
    CRVBOXHGCMBUFFER *hgcm_buffer = (CRVBOXHGCMBUFFER *) data;

    CRASSERT(hgcm_buffer->magic == CR_VBOXHGCM_BUFFER_MAGIC);

    switch (hgcm_buffer->kind)
    {
        case CR_VBOXHGCM_MEMORY:
            crFree( hgcm_buffer );
            break;
#ifdef RT_OS_WINDOWS
        case CR_VBOXHGCM_DDRAW_SURFACE:
            lpDDS = hgcm_buffer->pDDS;
            CRASSERT(lpDDS);
            IDirectDrawSurface_Unlock(lpDDS, NULL);
            IDirectDrawSurface_Release(lpDDS);
            crDebug("DDraw surface freed (%x)\n", lpDDS);
            break;
#endif
        case CR_VBOXHGCM_MEMORY_BIG:
            crFree( hgcm_buffer );
            break;

        default:
            crError( "Weird buffer kind trying to free in crVBoxHGCMBufferFree: %d", hgcm_buffer->kind );
    }
}

void crVBoxHGCMTearDown(void)
{
    int32_t i, cCons;

    if (!g_crvboxhgcm.initialized) return;

    /* Connection count would be changed in calls to crNetDisconnect, so we have to store original value.
     * Walking array backwards is not a good idea as it could cause some issues if we'd disconnect clients not in the
     * order of their connection.
     */
    cCons = g_crvboxhgcm.num_conns;
    for (i=0; i<cCons; i++)
    {
        /* Note that [0] is intended, as the connections array would be shifted in each call to crNetDisconnect */
        crNetDisconnect(g_crvboxhgcm.conns[0]);
    }
    CRASSERT(0==g_crvboxhgcm.num_conns);

#ifdef CHROMIUM_THREADSAFE
    crFreeMutex(&g_crvboxhgcm.mutex);
    crFreeMutex(&g_crvboxhgcm.recvmutex);
#endif

    if (g_crvboxhgcm.bufpool)
        crBufferPoolCallbackFree(g_crvboxhgcm.bufpool, crVBoxHGCMBufferFree);
    g_crvboxhgcm.bufpool = NULL;

    g_crvboxhgcm.initialized = 0;

    crFree(g_crvboxhgcm.conns);
    g_crvboxhgcm.conns = NULL;

#ifdef RT_OS_WINDOWS
    if (g_crvboxhgcm.pDirectDraw)
    {
        IDirectDraw_Release(g_crvboxhgcm.pDirectDraw);
        g_crvboxhgcm.pDirectDraw = NULL;
        crDebug("DirectDraw released\n");
    }
#endif
}

void crVBoxHGCMConnection(CRConnection *conn)
{
    int i, found = 0;
    int n_bytes;

    CRASSERT(g_crvboxhgcm.initialized);

    conn->type = CR_VBOXHGCM;
    conn->Alloc = crVBoxHGCMAlloc;
    conn->Send = crVBoxHGCMSend;
    conn->SendExact = crVBoxHGCMWriteExact;
    conn->Recv = crVBoxHGCMSingleRecv;
    conn->RecvMsg = crVBoxHGCMReceiveMessage;
    conn->Free = crVBoxHGCMFree;
    conn->Accept = crVBoxHGCMAccept;
    conn->Connect = crVBoxHGCMDoConnect;
    conn->Disconnect = crVBoxHGCMDoDisconnect;
    conn->InstantReclaim = crVBoxHGCMInstantReclaim;
    conn->HandleNewMessage = crVBoxHGCMHandleNewMessage;
    conn->index = g_crvboxhgcm.num_conns;
    conn->sizeof_buffer_header = sizeof(CRVBOXHGCMBUFFER);
    conn->actual_network = 1;

    conn->krecv_buf_size = 0;

    conn->pBuffer = NULL;
    conn->cbBuffer = 0;
    conn->allow_redir_ptr = 1;

    //@todo remove this crap at all later
    conn->cbHostBufferAllocated = 2*1024;
    conn->pHostBuffer = (uint8_t*) crAlloc(conn->cbHostBufferAllocated);
    CRASSERT(conn->pHostBuffer);
    conn->cbHostBuffer = 0;

    /* Find a free slot */
    for (i = 0; i < g_crvboxhgcm.num_conns; i++) {
        if (g_crvboxhgcm.conns[i] == NULL) {
            conn->index = i;
            g_crvboxhgcm.conns[i] = conn;
            found = 1;
            break;
        }
    }

    /* Realloc connection stack if we couldn't find a free slot */
    if (found == 0) {
        n_bytes = ( g_crvboxhgcm.num_conns + 1 ) * sizeof(*g_crvboxhgcm.conns);
        crRealloc( (void **) &g_crvboxhgcm.conns, n_bytes );
        g_crvboxhgcm.conns[g_crvboxhgcm.num_conns++] = conn;
    }
}

int crVBoxHGCMRecv(void)
{
    int32_t i;

#ifdef IN_GUEST
    /* we're on guest side, poll host if it got something for us */
    for (i=0; i<g_crvboxhgcm.num_conns; i++)
    {
        CRConnection *conn = g_crvboxhgcm.conns[i];

        if ( !conn || conn->type == CR_NO_CONNECTION )
            continue;

        if (!conn->pBuffer)
        {
            crVBoxHGCMPollHost(conn);
        }
    }
#endif

    for (i=0; i<g_crvboxhgcm.num_conns; i++)
    {
        CRConnection *conn = g_crvboxhgcm.conns[i];

        if ( !conn || conn->type == CR_NO_CONNECTION )
            continue;

        if (conn->cbBuffer>0)
        {
            crVBoxHGCMReceiveMessage(conn);
        }
    }

    return 0;
}

CRConnection** crVBoxHGCMDump( int *num )
{
    *num = g_crvboxhgcm.num_conns;

    return g_crvboxhgcm.conns;
}
