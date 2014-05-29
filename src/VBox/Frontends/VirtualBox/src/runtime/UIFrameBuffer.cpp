/* $Id$ */
/** @file
 * VBox Qt GUI - UIFrameBuffer class implementation.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIMachineView.h"
# include "UIFrameBuffer.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# ifndef VBOX_WITH_TRANSLUCENT_SEAMLESS
#  include "UIMachineWindow.h"
# endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */

/* Other VBox includes: */
# include <VBox/VBoxVideo3D.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include "CConsole.h"
#include "CDisplay.h"

#if defined (Q_OS_WIN32)
static CComModule _Module;
#else
NS_DECL_CLASSINFO (UIFrameBuffer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (UIFrameBuffer, IFramebuffer)
#endif

UIFrameBuffer::UIFrameBuffer(UIMachineView *pMachineView)
    : m_pMachineView(pMachineView)
    , m_width(0), m_height(0)
    , m_fIsMarkedAsUnused(false)
    , m_fIsAutoEnabled(false)
    , m_fIsUpdatesAllowed(true)
#ifdef Q_OS_WIN
    , m_iRefCnt(0)
#endif /* Q_OS_WIN */
    , m_hiDPIOptimizationType(HiDPIOptimizationType_None)
    , m_dBackingScaleFactor(1.0)
{
    /* Assign mahine-view: */
    AssertMsg(m_pMachineView, ("UIMachineView must not be NULL\n"));
    m_WinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;

    /* Initialize critical-section: */
    int rc = RTCritSectInit(&m_critSect);
    AssertRC(rc);

    /* Connect handlers: */
    if (m_pMachineView)
        prepareConnections();
}

UIFrameBuffer::~UIFrameBuffer()
{
    /* Disconnect handlers: */
    if (m_pMachineView)
        cleanupConnections();

    /* Deinitialize critical-section: */
    RTCritSectDelete(&m_critSect);
}

/**
 * Sets the framebuffer <b>unused</b> status.
 * @param fIsMarkAsUnused determines whether framebuffer should ignore EMT events or not.
 * @note  Call to this (and any EMT callback) method is synchronized between calling threads (from GUI side).
 */
void UIFrameBuffer::setMarkAsUnused(bool fIsMarkAsUnused)
{
    lock();
    m_fIsMarkedAsUnused = fIsMarkAsUnused;
    unlock();
}

/**
 * Returns the framebuffer <b>auto-enabled</b> status.
 * @returns @c true if guest-screen corresponding to this framebuffer was automatically enabled by
            the auto-mount guest-screen auto-pilot, @c false otherwise.
 * @note    <i>Auto-enabled</i> status means the framebuffer was automatically enabled by the multi-screen layout
 *          and so have potentially incorrect guest size hint posted into guest event queue. Machine-view will try to
 *          automatically adjust guest-screen size as soon as possible.
 */
bool UIFrameBuffer::isAutoEnabled() const
{
    return m_fIsAutoEnabled;
}

/**
 * Sets the framebuffer <b>auto-enabled</b> status.
 * @param fIsAutoEnabled determines whether guest-screen corresponding to this framebuffer
 *        was automatically enabled by the auto-mount guest-screen auto-pilot.
 * @note  <i>Auto-enabled</i> status means the framebuffer was automatically enabled by the multi-screen layout
 *        and so have potentially incorrect guest size hint posted into guest event queue. Machine-view will try to
 *        automatically adjust guest-screen size as soon as possible.
 */
void UIFrameBuffer::setAutoEnabled(bool fIsAutoEnabled)
{
    m_fIsAutoEnabled = fIsAutoEnabled;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Address) (BYTE **ppAddress)
{
    if (!ppAddress)
        return E_POINTER;
    *ppAddress = address();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Width) (ULONG *puWidth)
{
    if (!puWidth)
        return E_POINTER;
    *puWidth = (ULONG)width();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Height) (ULONG *puHeight)
{
    if (!puHeight)
        return E_POINTER;
    *puHeight = (ULONG)height();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BitsPerPixel) (ULONG *puBitsPerPixel)
{
    if (!puBitsPerPixel)
        return E_POINTER;
    *puBitsPerPixel = bitsPerPixel();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BytesPerLine) (ULONG *puBytesPerLine)
{
    if (!puBytesPerLine)
        return E_POINTER;
    *puBytesPerLine = bytesPerLine();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(PixelFormat) (ULONG *puPixelFormat)
{
    if (!puPixelFormat)
        return E_POINTER;
    *puPixelFormat = pixelFormat();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(UsesGuestVRAM) (BOOL *pbUsesGuestVRAM)
{
    if (!pbUsesGuestVRAM)
        return E_POINTER;
    *pbUsesGuestVRAM = true;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(HeightReduction) (ULONG *puHeightReduction)
{
    if (!puHeightReduction)
        return E_POINTER;
    *puHeightReduction = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Overlay) (IFramebufferOverlay **ppOverlay)
{
    if (!ppOverlay)
        return E_POINTER;
    /* not yet implemented */
    *ppOverlay = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(WinId) (LONG64 *pWinId)
{
    if (!pWinId)
        return E_POINTER;
    *pWinId = m_WinId;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::Lock()
{
    this->lock();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::Unlock()
{
    this->unlock();
    return S_OK;
}

/**
 * EMT callback: Requests a size and pixel format change.
 * @param uScreenId     Guest screen number. Must be used in the corresponding call to CDisplay::ResizeCompleted if this call is made.
 * @param uPixelFormat  Pixel format of the memory buffer pointed to by @a pVRAM.
 * @param pVRAM         Pointer to the virtual video card's VRAM (may be <i>null</i>).
 * @param uBitsPerPixel Color depth, bits per pixel.
 * @param uBytesPerLine Size of one scan line, in bytes.
 * @param uWidth        Width of the guest display, in pixels.
 * @param uHeight       Height of the guest display, in pixels.
 * @param pfFinished    Can the VM start using the new frame buffer immediately after this method returns or it should wait for CDisplay::ResizeCompleted.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::RequestResize(ULONG uScreenId, ULONG uPixelFormat,
                                          BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                                          ULONG uWidth, ULONG uHeight,
                                          BOOL *pfFinished)
{
    /* Make sure result pointer is valid: */
    if (!pfFinished)
    {
        LogRel(("UIFrameBuffer::RequestResize: "
                "Screen=%lu, Format=%lu, "
                "BitsPerPixel=%lu, BytesPerLine=%lu, "
                "Size=%lux%lu, Invalid pfFinished pointer!\n",
                (unsigned long)uScreenId, (unsigned long)uPixelFormat,
                (unsigned long)uBitsPerPixel, (unsigned long)uBytesPerLine,
                (unsigned long)uWidth, (unsigned long)uHeight));

        return E_POINTER;
    }

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel(("UIFrameBuffer::RequestResize: "
                "Screen=%lu, Format=%lu, "
                "BitsPerPixel=%lu, BytesPerLine=%lu, "
                "Size=%lux%lu, Ignored!\n",
                (unsigned long)uScreenId, (unsigned long)uPixelFormat,
                (unsigned long)uBitsPerPixel, (unsigned long)uBytesPerLine,
                (unsigned long)uWidth, (unsigned long)uHeight));

        /* Mark request as finished.
         * It is required to report to the VM thread that we finished resizing and rely on the
         * later synchronisation when the new view is attached. */
        *pfFinished = TRUE;

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore RequestResize: */
        return E_FAIL;
    }

    /* Mark request as not-yet-finished: */
    *pfFinished = FALSE;

    /* Widget resize is NOT thread-safe and *probably* never will be,
     * We have to notify machine-view with the async-signal to perform resize operation. */
    LogRel(("UIFrameBuffer::RequestResize: "
            "Screen=%lu, Format=%lu, "
            "BitsPerPixel=%lu, BytesPerLine=%lu, "
            "Size=%lux%lu, Sending to async-handler..\n",
            (unsigned long)uScreenId, (unsigned long)uPixelFormat,
            (unsigned long)uBitsPerPixel, (unsigned long)uBytesPerLine,
            (unsigned long)uWidth, (unsigned long)uHeight));
    emit sigRequestResize(uPixelFormat, pVRAM, uBitsPerPixel, uBytesPerLine, uWidth, uHeight);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm RequestResize: */
    return S_OK;
}

void UIFrameBuffer::notifyChange()
{
    /* Lock access to frame-buffer: */
    lock();

    /* If there is NO pending source bitmap: */
    if (m_pendingSourceBitmap.isNull())
    {
        /* Do nothing, change-event already processed: */
        LogRelFlow(("UIFrameBuffer::notifyChange: Already processed.\n"));
        /* Unlock access to frame-buffer: */
        unlock();
        /* Return immediately: */
        return;
    }

    /* Disable screen updates: */
    m_fIsUpdatesAllowed = false;

    /* Release the current bitmap and keep the pending one: */
    m_sourceBitmap = m_pendingSourceBitmap;
    m_pendingSourceBitmap = 0;

    /* Unlock access to frame-buffer: */
    unlock();

    /* Acquire source bitmap: */
    BYTE *pAddress = NULL;
    ULONG ulWidth = 0;
    ULONG ulHeight = 0;
    ULONG ulBitsPerPixel = 0;
    ULONG ulBytesPerLine = 0;
    ULONG ulPixelFormat = 0;
    m_sourceBitmap.QueryBitmapInfo(pAddress,
                                   ulWidth,
                                   ulHeight,
                                   ulBitsPerPixel,
                                   ulBytesPerLine,
                                   ulPixelFormat);

    /* Perform frame-buffer resize: */
    UIResizeEvent e(FramebufferPixelFormat_Opaque, pAddress,
                    ulBitsPerPixel, ulBytesPerLine, ulWidth, ulHeight);
    resizeEvent(&e);
}

/**
 * EMT callback: Informs about source bitmap change.
 * @param uScreenId Guest screen number.
 * @param uX        Horizontal origin of the update rectangle, in pixels.
 * @param uY        Vertical origin of the update rectangle, in pixels.
 * @param uWidth    Width of the guest display, in pixels.
 * @param uHeight   Height of the guest display, in pixels.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::NotifyChange(ULONG uScreenId,
                                         ULONG uX,
                                         ULONG uY,
                                         ULONG uWidth,
                                         ULONG uHeight)
{
    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel(("UIFrameBuffer::NotifyChange: Screen=%lu, Origin=%lux%lu, Size=%lux%lu, Ignored!\n",
                (unsigned long)uScreenId,
                (unsigned long)uX, (unsigned long)uY,
                (unsigned long)uWidth, (unsigned long)uHeight));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore NotifyChange: */
        return E_FAIL;
    }

    /* Acquire new pending bitmap: */
    m_pendingSourceBitmap = 0;
    m_pMachineView->session().GetConsole().GetDisplay().QuerySourceBitmap(uScreenId, m_pendingSourceBitmap);

    /* Widget resize is NOT thread-safe and *probably* never will be,
     * We have to notify machine-view with the async-signal to perform resize operation. */
    LogRel(("UIFrameBuffer::NotifyChange: Screen=%lu, Origin=%lux%lu, Size=%lux%lu, Sending to async-handler..\n",
            (unsigned long)uScreenId,
            (unsigned long)uX, (unsigned long)uY,
            (unsigned long)uWidth, (unsigned long)uHeight));
    emit sigNotifyChange(uScreenId, uWidth, uHeight);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Give up control token to other thread: */
    RTThreadYield();

    /* Confirm NotifyChange: */
    return S_OK;
}

/**
 * EMT callback: Informs about an update.
 * @param uX      Horizontal origin of the update rectangle, in pixels.
 * @param uY      Vertical origin of the update rectangle, in pixels.
 * @param uWidth  Width of the update rectangle, in pixels.
 * @param uHeight Height of the update rectangle, in pixels.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::NotifyUpdate(ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight)
{
    /* Retina screens with physical-to-logical scaling requires
     * odd/even pixel updates to be taken into account,
     * otherwise we have artifacts on the borders of incoming rectangle. */
    uX = qMax(0, (int)uX - 1);
    uY = qMax(0, (int)uY - 1);
    uWidth = qMin((int)m_width, (int)uWidth + 2);
    uHeight = qMin((int)m_height, (int)uHeight + 2);

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel2(("UIFrameBuffer::NotifyUpdate: Origin=%lux%lu, Size=%lux%lu, Ignored!\n",
                 (unsigned long)uX, (unsigned long)uY,
                 (unsigned long)uWidth, (unsigned long)uHeight));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore NotifyUpdate: */
        return E_FAIL;
    }

    /* Widget update is NOT thread-safe and *seems* never will be,
     * We have to notify machine-view with the async-signal to perform update operation. */
    LogRel2(("UIFrameBuffer::NotifyUpdate: Origin=%lux%lu, Size=%lux%lu, Sending to async-handler..\n",
             (unsigned long)uX, (unsigned long)uY,
             (unsigned long)uWidth, (unsigned long)uHeight));
    emit sigNotifyUpdate(uX, uY, uWidth, uHeight);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm NotifyUpdate: */
    return S_OK;
}

/**
 * EMT callback: Returns whether the framebuffer implementation is willing to support a given video mode.
 * @param uWidth      Width of the guest display, in pixels.
 * @param uHeight     Height of the guest display, in pixels.
 * @param uBPP        Color depth, bits per pixel.
 * @param pfSupported Is framebuffer able/willing to render the video mode or not.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::VideoModeSupported(ULONG uWidth, ULONG uHeight, ULONG uBPP, BOOL *pfSupported)
{
    /* Make sure result pointer is valid: */
    if (!pfSupported)
    {
        LogRel2(("UIFrameBuffer::IsVideoModeSupported: Mode: BPP=%lu, Size=%lux%lu, Invalid pfSupported pointer!\n",
                 (unsigned long)uBPP, (unsigned long)uWidth, (unsigned long)uHeight));

        return E_POINTER;
    }

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel2(("UIFrameBuffer::IsVideoModeSupported: Mode: BPP=%lu, Size=%lux%lu, Ignored!\n",
                 (unsigned long)uBPP, (unsigned long)uWidth, (unsigned long)uHeight));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore VideoModeSupported: */
        return E_FAIL;
    }

    /* Determine if supported: */
    *pfSupported = TRUE;
    QSize screenSize = m_pMachineView->maxGuestSize();
    if (   (screenSize.width() != 0)
        && (uWidth > (ULONG)screenSize.width())
        && (uWidth > (ULONG)width()))
        *pfSupported = FALSE;
    if (   (screenSize.height() != 0)
        && (uHeight > (ULONG)screenSize.height())
        && (uHeight > (ULONG)height()))
        *pfSupported = FALSE;
    LogRel2(("UIFrameBuffer::IsVideoModeSupported: Mode: BPP=%lu, Size=%lux%lu, Supported=%s\n",
             (unsigned long)uBPP, (unsigned long)uWidth, (unsigned long)uHeight, *pfSupported ? "TRUE" : "FALSE"));

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm VideoModeSupported: */
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::GetVisibleRegion(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied)
{
    PRTRECT rects = (PRTRECT)pRectangles;

    if (!rects)
        return E_POINTER;

    Q_UNUSED(uCount);
    Q_UNUSED(puCountCopied);

    return S_OK;
}

/**
 * EMT callback: Suggests a new visible region to this framebuffer.
 * @param pRectangles Pointer to the RTRECT array.
 * @param uCount      Number of RTRECT elements in the rectangles array.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::SetVisibleRegion(BYTE *pRectangles, ULONG uCount)
{
    /* Make sure rectangles were passed: */
    if (!pRectangles)
    {
        LogRel2(("UIFrameBuffer::SetVisibleRegion: Rectangle count=%lu, Invalid pRectangles pointer!\n",
                 (unsigned long)uCount));

        return E_POINTER;
    }

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel2(("UIFrameBuffer::SetVisibleRegion: Rectangle count=%lu, Ignored!\n",
                 (unsigned long)uCount));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore SetVisibleRegion: */
        return E_FAIL;
    }

    /* Compose region: */
    QRegion region;
    PRTRECT rects = (PRTRECT)pRectangles;
    for (ULONG ind = 0; ind < uCount; ++ind)
    {
        /* Get current rectangle: */
        QRect rect;
        rect.setLeft(rects->xLeft);
        rect.setTop(rects->yTop);
        /* Which is inclusive: */
        rect.setRight(rects->xRight - 1);
        rect.setBottom(rects->yBottom - 1);
        /* Append region: */
        region += rect;
        ++rects;
    }

    /* We are directly updating synchronous visible-region: */
    m_syncVisibleRegion = region;
    /* And send async-signal to update asynchronous one: */
    LogRel2(("UIFrameBuffer::SetVisibleRegion: Rectangle count=%lu, Sending to async-handler..\n",
             (unsigned long)uCount));
    emit sigSetVisibleRegion(region);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm SetVisibleRegion: */
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::ProcessVHWACommand(BYTE *pCommand)
{
    Q_UNUSED(pCommand);
    return E_NOTIMPL;
}

/**
 * EMT callback: Notifies framebuffer about 3D backend event.
 * @param uType Event type. Currently only VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA is supported.
 * @param pData Event-specific data, depends on the supplied event type.
 * @note  Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
 * @note  Calls to this and #setMarkAsUnused method are synchronized between calling threads (from GUI side).
 */
STDMETHODIMP UIFrameBuffer::Notify3DEvent(ULONG uType, BYTE *pData)
{
    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fIsMarkedAsUnused)
    {
        LogRel2(("UIFrameBuffer::Notify3DEvent: Ignored!\n"));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore Notify3DEvent: */
        return E_FAIL;
    }

    switch (uType)
    {
        case VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA:
        {
            /* Notify machine-view with the async-signal
             * about 3D overlay visibility change: */
            BOOL fVisible = !!pData;
            LogRel2(("UIFrameBuffer::Notify3DEvent: Sending to async-handler: "
                     "(VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA = %s)\n",
                     fVisible ? "TRUE" : "FALSE"));
            emit sigNotifyAbout3DOverlayVisibilityChange(fVisible);

            /* Unlock access to frame-buffer: */
            unlock();

            /* Confirm Notify3DEvent: */
            return S_OK;
        }

        case VBOX3D_NOTIFY_EVENT_TYPE_TEST_FUNCTIONAL:
        {
            HRESULT hr = m_fIsMarkedAsUnused ? E_FAIL : S_OK;
            unlock();
            return hr;
        }

        default:
            break;
    }

    /* Unlock access to frame-buffer: */
    unlock();

    /* Ignore Notify3DEvent: */
    return E_INVALIDARG;
}

void UIFrameBuffer::applyVisibleRegion(const QRegion &region)
{
    /* Make sure async visible-region has changed: */
    if (m_asyncVisibleRegion == region)
        return;

    /* We are accounting async visible-regions one-by-one
     * to keep corresponding viewport area always updated! */
    if (!m_asyncVisibleRegion.isEmpty())
        m_pMachineView->viewport()->update(m_asyncVisibleRegion - region);

    /* Remember last visible region: */
    m_asyncVisibleRegion = region;

#ifndef VBOX_WITH_TRANSLUCENT_SEAMLESS
    /* We have to use async visible-region to apply to [Q]Widget [set]Mask: */
    m_pMachineView->machineWindow()->setMask(m_asyncVisibleRegion);
#endif /* !VBOX_WITH_TRANSLUCENT_SEAMLESS */
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void UIFrameBuffer::doProcessVHWACommand(QEvent *pEvent)
{
    Q_UNUSED(pEvent);
    /* should never be here */
    AssertBreakpoint();
}
#endif /* VBOX_WITH_VIDEOHWACCEL */

void UIFrameBuffer::setView(UIMachineView * pView)
{
    /* Disconnect handlers: */
    if (m_pMachineView)
        cleanupConnections();

    /* Reassign machine-view: */
    m_pMachineView = pView;
    m_WinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;

    /* Connect handlers: */
    if (m_pMachineView)
        prepareConnections();
}

void UIFrameBuffer::setHiDPIOptimizationType(HiDPIOptimizationType optimizationType)
{
    /* Make sure 'HiDPI optimization type' changed: */
    if (m_hiDPIOptimizationType == optimizationType)
        return;

    /* Update 'HiDPI optimization type': */
    m_hiDPIOptimizationType = optimizationType;
}

void UIFrameBuffer::setBackingScaleFactor(double dBackingScaleFactor)
{
    /* Make sure 'backing scale factor' changed: */
    if (m_dBackingScaleFactor == dBackingScaleFactor)
        return;

    /* Update 'backing scale factor': */
    m_dBackingScaleFactor = dBackingScaleFactor;
}

void UIFrameBuffer::prepareConnections()
{
    connect(this, SIGNAL(sigRequestResize(int, uchar*, int, int, int, int)),
            m_pMachineView, SLOT(sltHandleRequestResize(int, uchar*, int, int, int, int)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(sigNotifyChange(ulong, int, int)),
            m_pMachineView, SLOT(sltHandleNotifyChange(ulong, int, int)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(sigNotifyUpdate(int, int, int, int)),
            m_pMachineView, SLOT(sltHandleNotifyUpdate(int, int, int, int)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(sigSetVisibleRegion(QRegion)),
            m_pMachineView, SLOT(sltHandleSetVisibleRegion(QRegion)),
            Qt::QueuedConnection);
    connect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
            m_pMachineView, SLOT(sltHandle3DOverlayVisibilityChange(bool)),
            Qt::QueuedConnection);
}

void UIFrameBuffer::cleanupConnections()
{
    disconnect(this, SIGNAL(sigRequestResize(int, uchar*, int, int, int, int)),
               m_pMachineView, SLOT(sltHandleRequestResize(int, uchar*, int, int, int, int)));
    disconnect(this, SIGNAL(sigNotifyChange(ulong, int, int)),
               m_pMachineView, SLOT(sltHandleNotifyChange(ulong, int, int)));
    disconnect(this, SIGNAL(sigNotifyUpdate(int, int, int, int)),
               m_pMachineView, SLOT(sltHandleNotifyUpdate(int, int, int, int)));
    disconnect(this, SIGNAL(sigSetVisibleRegion(QRegion)),
               m_pMachineView, SLOT(sltHandleSetVisibleRegion(QRegion)));
    disconnect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
               m_pMachineView, SLOT(sltHandle3DOverlayVisibilityChange(bool)));
}

