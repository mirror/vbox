/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBuffer class and subclasses implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

/* Local includes */
# include "UIMachineView.h"
# include "UIFrameBuffer.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#if defined (Q_OS_WIN32)
static CComModule _Module;
#else
NS_DECL_CLASSINFO (UIFrameBuffer)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI (UIFrameBuffer, IFramebuffer)
#endif

UIFrameBuffer::UIFrameBuffer(UIMachineView *pMachineView)
    : m_pMachineView(pMachineView)
    , m_width(0), m_height(0)
    , m_fIsDeleted(false)
#if defined (Q_OS_WIN32)
    , m_iRefCnt(0)
#endif
{
    AssertMsg(m_pMachineView, ("UIMachineView must not be null\n"));
    m_WinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;
    int rc = RTCritSectInit(&m_critSect);
    AssertRC(rc);
}

UIFrameBuffer::~UIFrameBuffer()
{
    RTCritSectDelete(&m_critSect);
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
    *pbUsesGuestVRAM = usesGuestVRAM();
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

/** @note This method is called on EMT from under this object's lock */
STDMETHODIMP UIFrameBuffer::RequestResize(ULONG uScreenId, ULONG uPixelFormat,
                                          BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                                          ULONG uWidth, ULONG uHeight,
                                          BOOL *pbFinished)
{
    if (m_fIsDeleted)
        return E_FAIL;

    NOREF(uScreenId);
    /* A resize event can happen while we are switching machine view classes,
     * but we synchronise afterwards so that shouldn't be a problem.  We must
     * temporarily remove the framebuffer in Display though while switching
     * to respect the thread synchronisation logic (see UIFrameBuffer.h). */
    if (m_pMachineView)
        QApplication::postEvent(m_pMachineView,
                                new UIResizeEvent(uPixelFormat, pVRAM,
                                                  uBitsPerPixel, uBytesPerLine,
                                                  uWidth, uHeight));
    else
    {
        /* Report to the VM thread that we finished resizing and rely on the
         * synchronisation when the new view is attached. */
        *pbFinished = TRUE;
        return S_OK;
    }

    *pbFinished = FALSE;
    return S_OK;
}

/**
 * Returns whether we like the given video mode.
 * @note We always like a mode smaller than the current framebuffer
 *       size.
 *
 * @returns COM status code
 * @param   width     video mode width in pixels
 * @param   height    video mode height in pixels
 * @param   bpp       video mode bit depth in bits per pixel
 * @param   supported pointer to result variable
 */
STDMETHODIMP UIFrameBuffer::VideoModeSupported(ULONG uWidth, ULONG uHeight, ULONG uBPP, BOOL *pbSupported)
{
    NOREF(uBPP);
    LogFlowThisFunc(("width=%lu, height=%lu, BPP=%lu\n",
                    (unsigned long)uWidth, (unsigned long)uHeight, (unsigned long)uBPP));
    if (!pbSupported)
        return E_POINTER;
    *pbSupported = TRUE;
    QSize screen = m_pMachineView->maxGuestSize();
    if (   (screen.width() != 0)
        && (uWidth > (ULONG)screen.width())
        && (uWidth > (ULONG)width()))
        *pbSupported = FALSE;
    if (   (screen.height() != 0)
        && (uHeight > (ULONG)screen.height())
        && (uHeight > (ULONG)height()))
        *pbSupported = FALSE;
    LogFlowThisFunc(("screenW=%lu, screenH=%lu -> aSupported=%s\n",
                    screen.width(), screen.height(), *pbSupported ? "TRUE" : "FALSE"));
    return S_OK;
}

STDMETHODIMP UIFrameBuffer::GetVisibleRegion(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied)
{
    PRTRECT rects = (PRTRECT)pRectangles;

    if (!rects)
        return E_POINTER;

    NOREF(uCount);
    NOREF(puCountCopied);

    return S_OK;
}

STDMETHODIMP UIFrameBuffer::SetVisibleRegion(BYTE *pRectangles, ULONG uCount)
{
    PRTRECT rects = (PRTRECT)pRectangles;

    if (!rects)
        return E_POINTER;

    QRegion reg;
    for (ULONG ind = 0; ind < uCount; ++ ind)
    {
        QRect rect;
        rect.setLeft(rects->xLeft);
        rect.setTop(rects->yTop);
        /* QRect are inclusive */
        rect.setRight(rects->xRight - 1);
        rect.setBottom(rects->yBottom - 1);
        reg += rect;
        ++ rects;
    }
    QApplication::postEvent(m_pMachineView, new UISetRegionEvent(reg));

    return S_OK;
}

STDMETHODIMP UIFrameBuffer::ProcessVHWACommand(BYTE *pCommand)
{
    Q_UNUSED(pCommand);
    return E_NOTIMPL;
}

#ifdef VBOX_WITH_VIDEOHWACCEL
void UIFrameBuffer::doProcessVHWACommand(QEvent *pEvent)
{
    Q_UNUSED(pEvent);
    /* should never be here */
    AssertBreakpoint();
}
#endif

void UIFrameBuffer::setView(UIMachineView * pView)
{
    m_pMachineView = pView;
    m_WinId = (m_pMachineView && m_pMachineView->viewport()) ? (LONG64)m_pMachineView->viewport()->winId() : 0;
}
