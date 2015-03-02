/* $Id$ */
/** @file
 * VBox Qt GUI - UIFrameBuffer class implementation.
 */

/*
 * Copyright (C) 2010-2014 Oracle Corporation
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
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Qt includes: */
# include <QPainter>
/* GUI includes: */
# include "UIFrameBuffer.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMachineView.h"
# include "UIPopupCenter.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WITH_MASKED_SEAMLESS
#  include "UIMachineWindow.h"
# endif /* VBOX_WITH_MASKED_SEAMLESS */
/* COM includes: */
# include "CConsole.h"
# include "CDisplay.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Other VBox includes: */
#include <VBox/VBoxVideo3D.h>


/* COM stuff: */
#ifdef Q_WS_WIN
static CComModule _Module;
#else /* !Q_WS_WIN */
NS_DECL_CLASSINFO(UIFrameBuffer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(UIFrameBuffer, IFramebuffer)
#endif /* !Q_WS_WIN */

#ifdef Q_WS_X11
# include <QX11Info>
# include <X11/Xlib.h>
#endif /* Q_WS_X11 */

UIFrameBuffer::UIFrameBuffer()
    : m_iWidth(0), m_iHeight(0)
    , m_fPendingSourceBitmap(false)
    , m_pMachineView(NULL)
    , m_iWinId(0)
    , m_fUpdatesAllowed(true)
    , m_fUnused(false)
    , m_fAutoEnabled(false)
    , m_dScaleFactor(1.0)
    , m_dBackingScaleFactor(1.0)
    , m_fUseUnscaledHiDPIOutput(false)
    , m_hiDPIOptimizationType(HiDPIOptimizationType_None)
{
    /* Update coordinate-system: */
    updateCoordinateSystem();
}

HRESULT UIFrameBuffer::init(UIMachineView *pMachineView)
{
    LogRel2(("UIFrameBuffer::init %p\n", this));

    /* Assign mahine-view: */
    m_pMachineView = pMachineView;
    /* Cache window ID: */
    m_iWinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;

    /* Assign display: */
    m_display = m_pMachineView->uisession()->display();

    /* Initialize critical-section: */
    int rc = RTCritSectInit(&m_critSect);
    AssertRC(rc);

    /* Connect handlers: */
    if (m_pMachineView)
        prepareConnections();

    /* Resize/rescale frame-buffer to the default size: */
    performResize(640, 480);
    performRescale();

#ifdef Q_OS_WIN
    CoCreateFreeThreadedMarshaler(this, &m_pUnkMarshaler.p);
#endif /* Q_OS_WIN */
    return S_OK;
}

UIFrameBuffer::~UIFrameBuffer()
{
    LogRel2(("UIFrameBuffer::~UIFrameBuffer %p\n", this));

    /* Disconnect handlers: */
    if (m_pMachineView)
        cleanupConnections();

    /* Deinitialize critical-section: */
    RTCritSectDelete(&m_critSect);
}

void UIFrameBuffer::setView(UIMachineView *pMachineView)
{
    /* Disconnect old handlers: */
    if (m_pMachineView)
        cleanupConnections();

    /* Reassign machine-view: */
    m_pMachineView = pMachineView;
    /* Recache window ID: */
    m_iWinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;

#ifdef Q_WS_X11
    /* Sync Qt and X11 Server (see xTracker #7547). */
    XSync(QX11Info::display(), false);
#endif

    /* Connect new handlers: */
    if (m_pMachineView)
        prepareConnections();
}

UIVisualStateType UIFrameBuffer::visualState() const
{
    return m_pMachineView ? m_pMachineView->visualStateType() : UIVisualStateType_Invalid;
}

void UIFrameBuffer::setMarkAsUnused(bool fUnused)
{
    lock();
    m_fUnused = fUnused;
    unlock();
}

HRESULT UIFrameBuffer::FinalConstruct()
{
    return 0;
}

void UIFrameBuffer::FinalRelease()
{
    return;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Width)(ULONG *puWidth)
{
    if (!puWidth)
        return E_POINTER;
    *puWidth = (ULONG)width();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Height)(ULONG *puHeight)
{
    if (!puHeight)
        return E_POINTER;
    *puHeight = (ULONG)height();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BitsPerPixel)(ULONG *puBitsPerPixel)
{
    if (!puBitsPerPixel)
        return E_POINTER;
    *puBitsPerPixel = bitsPerPixel();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(BytesPerLine)(ULONG *puBytesPerLine)
{
    if (!puBytesPerLine)
        return E_POINTER;
    *puBytesPerLine = bytesPerLine();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(PixelFormat)(ULONG *puPixelFormat)
{
    if (!puPixelFormat)
        return E_POINTER;
    *puPixelFormat = pixelFormat();
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(HeightReduction)(ULONG *puHeightReduction)
{
    if (!puHeightReduction)
        return E_POINTER;
    *puHeightReduction = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Overlay)(IFramebufferOverlay **ppOverlay)
{
    if (!ppOverlay)
        return E_POINTER;
    *ppOverlay = 0;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(WinId)(LONG64 *pWinId)
{
    if (!pWinId)
        return E_POINTER;
    *pWinId = m_iWinId;
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::COMGETTER(Capabilities)(ComSafeArrayOut(FramebufferCapabilities_T, enmCapabilities))
{
    if (ComSafeArrayOutIsNull(enmCapabilities))
        return E_POINTER;

    com::SafeArray<FramebufferCapabilities_T> caps;
    if (vboxGlobal().isSeparateProcess())
    {
       caps.resize(1);
       caps[0] = FramebufferCapabilities_UpdateImage;
    }
    else
    {
       caps.resize(2);
       caps[0] = FramebufferCapabilities_VHWA;
       caps[1] = FramebufferCapabilities_VisibleRegion;
    }

    caps.detachTo(ComSafeArrayOutArg(enmCapabilities));
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::NotifyChange(ULONG uScreenId, ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight)
{
    CDisplaySourceBitmap sourceBitmap;
    if (!vboxGlobal().isSeparateProcess())
        display().QuerySourceBitmap(uScreenId, sourceBitmap);

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fUnused)
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

    /* Disable screen updates: */
    m_fUpdatesAllowed = false;

    /* While updates are disabled, visible region will be saved:  */
    m_pendingSyncVisibleRegion = QRegion();

    if (!vboxGlobal().isSeparateProcess())
    {
       /* Acquire new pending bitmap: */
       m_pendingSourceBitmap = sourceBitmap;
       m_fPendingSourceBitmap = true;
    }

    /* Widget resize is NOT thread-safe and *probably* never will be,
     * We have to notify machine-view with the async-signal to perform resize operation. */
    LogRel(("UIFrameBuffer::NotifyChange: Screen=%lu, Origin=%lux%lu, Size=%lux%lu, Sending to async-handler\n",
            (unsigned long)uScreenId,
            (unsigned long)uX, (unsigned long)uY,
            (unsigned long)uWidth, (unsigned long)uHeight));
    emit sigNotifyChange(uWidth, uHeight);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Give up control token to other thread: */
    RTThreadYield();

    /* Confirm NotifyChange: */
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::NotifyUpdate(ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight)
{
    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fUnused)
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
    LogRel2(("UIFrameBuffer::NotifyUpdate: Origin=%lux%lu, Size=%lux%lu, Sending to async-handler\n",
             (unsigned long)uX, (unsigned long)uY,
             (unsigned long)uWidth, (unsigned long)uHeight));
    emit sigNotifyUpdate(uX, uY, uWidth, uHeight);

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm NotifyUpdate: */
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::NotifyUpdateImage(ULONG uX, ULONG uY,
                                              ULONG uWidth, ULONG uHeight,
                                              ComSafeArrayIn(BYTE, image))
{
    /* Wrapping received data: */
    com::SafeArray<BYTE> imageData(ComSafeArrayInArg(image));

    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fUnused)
    {
        LogRel2(("UIFrameBuffer::NotifyUpdateImage: Origin=%lux%lu, Size=%lux%lu, Ignored!\n",
                 (unsigned long)uX, (unsigned long)uY,
                 (unsigned long)uWidth, (unsigned long)uHeight));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore NotifyUpdate: */
        return E_FAIL;
    }

    /* Directly update m_image: */
    if (m_fUpdatesAllowed)
    {
        /* Copy to m_image: */
        uchar *pu8Dst = m_image.bits() + uY * m_image.bytesPerLine() + uX * 4;
        uchar *pu8Src = imageData.raw();
        ULONG h;
        for (h = 0; h < uHeight; ++h)
        {
            memcpy(pu8Dst, pu8Src, uWidth * 4);
            pu8Dst += m_image.bytesPerLine();
            pu8Src += uWidth * 4;
        }

        /* Widget update is NOT thread-safe and *seems* never will be,
         * We have to notify machine-view with the async-signal to perform update operation. */
        LogRel2(("UIFrameBuffer::NotifyUpdateImage: Origin=%lux%lu, Size=%lux%lu, Sending to async-handler\n",
                 (unsigned long)uX, (unsigned long)uY,
                 (unsigned long)uWidth, (unsigned long)uHeight));
        emit sigNotifyUpdate(uX, uY, uWidth, uHeight);
    }

    /* Unlock access to frame-buffer: */
    unlock();

    /* Confirm NotifyUpdateImage: */
    return S_OK;
}

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
    if (m_fUnused)
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
    if (m_fUnused)
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
    for (ULONG uIndex = 0; uIndex < uCount; ++uIndex)
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
    /* Tune according scale-factor: */
    if (scaleFactor() != 1.0 || backingScaleFactor() > 1.0)
        region = m_transform.map(region);

    if (m_fUpdatesAllowed)
    {
        /* We are directly updating synchronous visible-region: */
        m_syncVisibleRegion = region;
        /* And send async-signal to update asynchronous one: */
        LogRel2(("UIFrameBuffer::SetVisibleRegion: Rectangle count=%lu, Sending to async-handler\n",
                 (unsigned long)uCount));
        emit sigSetVisibleRegion(region);
    }
    else
    {
        /* Save the region. */
        m_pendingSyncVisibleRegion = region;
        LogRel2(("UIFrameBuffer::SetVisibleRegion: Rectangle count=%lu, Saved\n",
                 (unsigned long)uCount));
    }

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

STDMETHODIMP UIFrameBuffer::Notify3DEvent(ULONG uType, ComSafeArrayIn(BYTE, data))
{
    /* Lock access to frame-buffer: */
    lock();

    /* Make sure frame-buffer is used: */
    if (m_fUnused)
    {
        LogRel2(("UIFrameBuffer::Notify3DEvent: Ignored!\n"));

        /* Unlock access to frame-buffer: */
        unlock();

        /* Ignore Notify3DEvent: */
        return E_FAIL;
    }

    com::SafeArray<BYTE> eventData(ComSafeArrayInArg(data));
    switch (uType)
    {
        case VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA:
        {
            /* Notify machine-view with the async-signal
             * about 3D overlay visibility change: */
            BOOL fVisible = eventData[0];
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
            HRESULT hr = m_fUnused ? E_FAIL : S_OK;
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

void UIFrameBuffer::handleNotifyChange(int iWidth, int iHeight)
{
    LogRel(("UIFrameBuffer::handleNotifyChange: Size=%dx%d\n", iWidth, iHeight));

    /* Make sure machine-view is assigned: */
    AssertPtrReturnVoid(m_pMachineView);

    /* Lock access to frame-buffer: */
    lock();

    /* If there is NO pending source-bitmap: */
    if (!vboxGlobal().isSeparateProcess() && !m_fPendingSourceBitmap)
    {
        /* Do nothing, change-event already processed: */
        LogRel2(("UIFrameBuffer::handleNotifyChange: Already processed.\n"));
        /* Unlock access to frame-buffer: */
        unlock();
        /* Return immediately: */
        return;
    }

    /* Release the current bitmap and keep the pending one: */
    m_sourceBitmap = m_pendingSourceBitmap;
    m_pendingSourceBitmap = 0;
    m_fPendingSourceBitmap = false;

    /* Unlock access to frame-buffer: */
    unlock();

    /* Perform frame-buffer resize: */
    performResize(iWidth, iHeight);
}

void UIFrameBuffer::handlePaintEvent(QPaintEvent *pEvent)
{
    LogRel2(("UIFrameBuffer::handlePaintEvent: Origin=%lux%lu, Size=%dx%d\n",
             pEvent->rect().x(), pEvent->rect().y(),
             pEvent->rect().width(), pEvent->rect().height()));

    /* On mode switch the enqueued paint-event may still come
     * while the machine-view is already null (before the new machine-view set),
     * ignore paint-event in that case. */
    if (!m_pMachineView)
        return;

    /* Lock access to frame-buffer: */
    lock();

    /* But if updates disabled: */
    if (!m_fUpdatesAllowed)
    {
        /* Unlock access to frame-buffer: */
        unlock();
        /* And return immediately: */
        return;
    }

    /* Depending on visual-state type: */
    switch (m_pMachineView->machineLogic()->visualStateType())
    {
        case UIVisualStateType_Seamless:
            paintSeamless(pEvent);
            break;
        default:
            paintDefault(pEvent);
            break;
    }

    /* Unlock access to frame-buffer: */
    unlock();
}

void UIFrameBuffer::handleSetVisibleRegion(const QRegion &region)
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

#ifdef VBOX_WITH_MASKED_SEAMLESS
    /* We have to use async visible-region to apply to [Q]Widget [set]Mask: */
    m_pMachineView->machineWindow()->setMask(m_asyncVisibleRegion);
#endif /* VBOX_WITH_MASKED_SEAMLESS */
}

void UIFrameBuffer::performResize(int iWidth, int iHeight)
{
    LogRel(("UIFrameBuffer::performResize: Size=%dx%d\n", iWidth, iHeight));

    /* Make sure machine-view is assigned: */
    AssertPtrReturnVoid(m_pMachineView);

    /* Invalidate visible-region (if necessary): */
    if (m_pMachineView->machineLogic()->visualStateType() == UIVisualStateType_Seamless &&
        (m_iWidth != iWidth || m_iHeight != iHeight))
    {
        lock();
        m_syncVisibleRegion = QRegion();
        m_asyncVisibleRegion = QRegion();
        unlock();
    }

    /* If source-bitmap invalid: */
    if (m_sourceBitmap.isNull())
    {
        LogRel(("UIFrameBuffer::performResize: "
                "Using FALLBACK buffer due to source-bitmap is not provided..\n"));

        /* Remember new size came from hint: */
        m_iWidth = iWidth;
        m_iHeight = iHeight;

        /* And recreate fallback buffer: */
        m_image = QImage(m_iWidth, m_iHeight, QImage::Format_RGB32);
        m_image.fill(0);
    }
    /* If source-bitmap valid: */
    else
    {
        LogRel(("UIFrameBuffer::performResize: "
                "Directly using source-bitmap content\n"));

        /* Acquire source-bitmap attributes: */
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
        Assert(ulBitsPerPixel == 32);

        /* Remember new actual size: */
        m_iWidth = (int)ulWidth;
        m_iHeight = (int)ulHeight;

        /* Recreate QImage on the basis of source-bitmap content: */
        m_image = QImage(pAddress, m_iWidth, m_iHeight, ulBytesPerLine, QImage::Format_RGB32);

        /* Check whether guest color depth differs from the bitmap color depth: */
        ULONG ulGuestBitsPerPixel = 0;
        LONG xOrigin = 0;
        LONG yOrigin = 0;
        KGuestMonitorStatus monitorStatus = KGuestMonitorStatus_Enabled;
        display().GetScreenResolution(m_pMachineView->screenId(),
                                      ulWidth, ulHeight, ulGuestBitsPerPixel, xOrigin, yOrigin, monitorStatus);

        /* Remind user if necessary, ignore text and VGA modes: */
        /* This check (supports graphics) is not quite right due to past mistakes
         * in the Guest Additions protocol, but in practice it should be fine. */
        if (   ulGuestBitsPerPixel != ulBitsPerPixel
            && ulGuestBitsPerPixel != 0
            && m_pMachineView->uisession()->isGuestSupportsGraphics())
            popupCenter().remindAboutWrongColorDepth(m_pMachineView->machineWindow(),
                                                     ulGuestBitsPerPixel, ulBitsPerPixel);
        else
            popupCenter().forgetAboutWrongColorDepth(m_pMachineView->machineWindow());
    }

    lock();

    /* Enable screen updates: */
    m_fUpdatesAllowed = true;

    if (!m_pendingSyncVisibleRegion.isEmpty())
    {
        /* Directly update synchronous visible-region: */
        m_syncVisibleRegion = m_pendingSyncVisibleRegion;
        m_pendingSyncVisibleRegion = QRegion();

        /* And send async-signal to update asynchronous one: */
        LogRel2(("UIFrameBuffer::performResize: Rectangle count=%lu, Sending to async-handler\n",
                 (unsigned long)m_syncVisibleRegion.rectCount()));
        emit sigSetVisibleRegion(m_syncVisibleRegion);
    }

    unlock();
}

void UIFrameBuffer::performRescale()
{
//    printf("UIFrameBuffer::performRescale\n");

    /* Make sure machine-view is assigned: */
    AssertPtrReturnVoid(m_pMachineView);

    /* Depending on current visual state: */
    switch (m_pMachineView->machineLogic()->visualStateType())
    {
        case UIVisualStateType_Scale:
            m_scaledSize = m_scaledSize.width() == m_iWidth && m_scaledSize.height() == m_iHeight ? QSize() : m_scaledSize;
            break;
        default:
            m_scaledSize = scaleFactor() == 1.0 ? QSize() : QSize((int)(m_iWidth * scaleFactor()), (int)(m_iHeight * scaleFactor()));
            break;
    }

    /* Update coordinate-system: */
    updateCoordinateSystem();

//    printf("UIFrameBuffer::performRescale: Complete: Scale-factor=%f, Scaled-size=%dx%d\n",
//           scaleFactor(), scaledSize().width(), scaledSize().height());
}

void UIFrameBuffer::prepareConnections()
{
    /* Attach EMT connections: */
    connect(this, SIGNAL(sigNotifyChange(int, int)),
            m_pMachineView, SLOT(sltHandleNotifyChange(int, int)),
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
    /* Detach EMT connections: */
    disconnect(this, SIGNAL(sigNotifyChange(int, int)),
               m_pMachineView, SLOT(sltHandleNotifyChange(int, int)));
    disconnect(this, SIGNAL(sigNotifyUpdate(int, int, int, int)),
               m_pMachineView, SLOT(sltHandleNotifyUpdate(int, int, int, int)));
    disconnect(this, SIGNAL(sigSetVisibleRegion(QRegion)),
               m_pMachineView, SLOT(sltHandleSetVisibleRegion(QRegion)));
    disconnect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
               m_pMachineView, SLOT(sltHandle3DOverlayVisibilityChange(bool)));
}

void UIFrameBuffer::updateCoordinateSystem()
{
    /* Reset to default: */
    m_transform = QTransform();

    /* Apply the scale-factor if necessary: */
    if (scaleFactor() != 1.0)
        m_transform = m_transform.scale(scaleFactor(), scaleFactor());

    /* Apply the backing-scale-factor if necessary: */
    if (useUnscaledHiDPIOutput() && backingScaleFactor() > 1.0)
        m_transform = m_transform.scale(1.0 / backingScaleFactor(), 1.0 / backingScaleFactor());
}

void UIFrameBuffer::paintDefault(QPaintEvent *pEvent)
{
    /* Scaled image is NULL by default: */
    QImage scaledImage;
    /* But if scaled-factor is set and current image is NOT null: */
    if (m_scaledSize.isValid() && !m_image.isNull())
    {
        /* We are doing a deep copy of the image to make sure it will not be
         * detached during scale process, otherwise we can get a frozen frame-buffer. */
        scaledImage = m_image.copy();
        /* And scaling the image to predefined scaled-factor: */
        scaledImage = scaledImage.scaled(m_scaledSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    /* Finally we are choosing image to paint from: */
    const QImage &sourceImage = scaledImage.isNull() ? m_image : scaledImage;

    /* Prepare the 'paint' rectangle: */
    QRect paintRect = pEvent->rect();

    /* Take the backing-scale-factor into account: */
    if (useUnscaledHiDPIOutput() && backingScaleFactor() > 1.0)
    {
        paintRect.moveTo(paintRect.topLeft() * backingScaleFactor());
        paintRect.setSize(paintRect.size() * backingScaleFactor());
    }

    /* Make sure paint-rectangle is within the image boundary: */
    paintRect = paintRect.intersected(sourceImage.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Draw image rectangle: */
    drawImageRect(painter, sourceImage, paintRect,
                  m_pMachineView->contentsX(), m_pMachineView->contentsY(),
                  useUnscaledHiDPIOutput(), hiDPIOptimizationType(), backingScaleFactor());
}

void UIFrameBuffer::paintSeamless(QPaintEvent *pEvent)
{
    /* Scaled image is NULL by default: */
    QImage scaledImage;
    /* But if scaled-factor is set and current image is NOT null: */
    if (m_scaledSize.isValid() && !m_image.isNull())
    {
        /* We are doing a deep copy of the image to make sure it will not be
         * detached during scale process, otherwise we can get a frozen frame-buffer. */
        scaledImage = m_image.copy();
        /* And scaling the image to predefined scaled-factor: */
        scaledImage = scaledImage.scaled(m_scaledSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    /* Finally we are choosing image to paint from: */
    const QImage &sourceImage = scaledImage.isNull() ? m_image : scaledImage;

    /* Prepare the 'paint' rectangle: */
    QRect paintRect = pEvent->rect();

    /* Prepare seamless regions to erase/paint: */
    lock();
    const QRegion eraseRegion = QRegion(paintRect) - m_syncVisibleRegion;
    const QRegion paintRegion = QRegion(paintRect) & m_syncVisibleRegion;
    unlock();

    /* Take the backing-scale-factor into account: */
    if (useUnscaledHiDPIOutput() && backingScaleFactor() > 1.0)
    {
        paintRect.moveTo(paintRect.topLeft() * backingScaleFactor());
        paintRect.setSize(paintRect.size() * backingScaleFactor());
    }

    /* Make sure paint-rectangle is within the image boundary: */
    paintRect = paintRect.intersected(sourceImage.rect());
    if (paintRect.isEmpty())
        return;

    /* Create painter: */
    QPainter painter(m_pMachineView->viewport());

    /* Apply painter clipping for erasing: */
    painter.setClipRegion(eraseRegion);
    /* Set composition-mode to erase: */
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    /* Erase rectangle: */
    eraseImageRect(painter, paintRect,
                   useUnscaledHiDPIOutput(), hiDPIOptimizationType(), backingScaleFactor());

    /* Apply painter clipping for painting: */
    painter.setClipRegion(paintRegion);
    /* Set composition-mode to paint: */
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
#if defined(VBOX_WITH_TRANSLUCENT_SEAMLESS)
# if defined(Q_WS_WIN) || defined(Q_WS_X11)
    /* Replace translucent background with black one: */
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(paintRect, QColor(Qt::black));
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
# endif /* Q_WS_WIN || Q_WS_X11 */
#endif /* VBOX_WITH_TRANSLUCENT_SEAMLESS */
    /* Paint rectangle: */
    drawImageRect(painter, sourceImage, paintRect,
                  m_pMachineView->contentsX(), m_pMachineView->contentsY(),
                  useUnscaledHiDPIOutput(), hiDPIOptimizationType(), backingScaleFactor());
}

/* static */
void UIFrameBuffer::eraseImageRect(QPainter &painter, const QRect &rect,
                                   bool fUseUnscaledHiDPIOutput,
                                   HiDPIOptimizationType hiDPIOptimizationType,
                                   double dBackingScaleFactor)
{
    /* Prepare sub-pixmap: */
    QPixmap subPixmap = QPixmap(rect.width(), rect.height());

    /* If HiDPI 'backing-scale-factor' defined: */
    if (dBackingScaleFactor > 1.0)
    {
        /* Should we
         * perform logical HiDPI scaling and optimize it for performance? */
        if (!fUseUnscaledHiDPIOutput && hiDPIOptimizationType == HiDPIOptimizationType_Performance)
        {
            /* Adjust sub-pixmap: */
            subPixmap = QPixmap((int)(rect.width() * dBackingScaleFactor),
                                (int)(rect.height() * dBackingScaleFactor));
        }

#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
        /* Should we
         * do not perform logical HiDPI scaling or
         * perform logical HiDPI scaling and optimize it for performance? */
        if (fUseUnscaledHiDPIOutput || hiDPIOptimizationType == HiDPIOptimizationType_Performance)
        {
            /* Mark sub-pixmap as HiDPI: */
            subPixmap.setDevicePixelRatio(dBackingScaleFactor);
        }
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */
    }

    /* Which point we should draw corresponding sub-pixmap? */
    QPointF paintPoint = rect.topLeft();

    /* Take the backing-scale-factor into account: */
    if (fUseUnscaledHiDPIOutput && dBackingScaleFactor > 1.0)
        paintPoint /= dBackingScaleFactor;

    /* Draw sub-pixmap: */
    painter.drawPixmap(paintPoint, subPixmap);
}

/* static */
void UIFrameBuffer::drawImageRect(QPainter &painter, const QImage &image, const QRect &rect,
                                  int iContentsShiftX, int iContentsShiftY,
                                  bool fUseUnscaledHiDPIOutput,
                                  HiDPIOptimizationType hiDPIOptimizationType,
                                  double dBackingScaleFactor)
{
    /* Calculate offset: */
    size_t offset = (rect.x() + iContentsShiftX) * image.depth() / 8 +
                    (rect.y() + iContentsShiftY) * image.bytesPerLine();

    /* Restrain boundaries: */
    int iSubImageWidth = qMin(rect.width(), image.width() - rect.x() - iContentsShiftX);
    int iSubImageHeight = qMin(rect.height(), image.height() - rect.y() - iContentsShiftY);

    /* Create sub-image (no copy involved): */
    QImage subImage = QImage(image.bits() + offset,
                             iSubImageWidth, iSubImageHeight,
                             image.bytesPerLine(), image.format());

    /* Create sub-pixmap on the basis of sub-image above (1st copy involved): */
    QPixmap subPixmap = QPixmap::fromImage(subImage);

    /* If HiDPI 'backing-scale-factor' defined: */
    if (dBackingScaleFactor > 1.0)
    {
        /* Should we
         * perform logical HiDPI scaling and optimize it for performance? */
        if (!fUseUnscaledHiDPIOutput && hiDPIOptimizationType == HiDPIOptimizationType_Performance)
        {
            /* Fast scale sub-pixmap (2nd copy involved): */
            subPixmap = subPixmap.scaled(subPixmap.size() * dBackingScaleFactor,
                                         Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }

#ifdef Q_WS_MAC
# ifdef VBOX_GUI_WITH_HIDPI
        /* Should we
         * do not perform logical HiDPI scaling or
         * perform logical HiDPI scaling and optimize it for performance? */
        if (fUseUnscaledHiDPIOutput || hiDPIOptimizationType == HiDPIOptimizationType_Performance)
        {
            /* Mark sub-pixmap as HiDPI: */
            subPixmap.setDevicePixelRatio(dBackingScaleFactor);
        }
# endif /* VBOX_GUI_WITH_HIDPI */
#endif /* Q_WS_MAC */
    }

    /* Which point we should draw corresponding sub-pixmap? */
    QPointF paintPoint = rect.topLeft();

    /* Take the backing-scale-factor into account: */
    if (fUseUnscaledHiDPIOutput && dBackingScaleFactor > 1.0)
        paintPoint /= dBackingScaleFactor;

    /* Draw sub-pixmap: */
    painter.drawPixmap(paintPoint, subPixmap);
}

