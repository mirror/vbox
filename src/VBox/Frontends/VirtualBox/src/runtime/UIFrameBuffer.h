/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBuffer class and subclasses declarations
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIFrameBuffer_h___
#define ___UIFrameBuffer_h___

/* Qt includes: */
#include <QRegion>
#include <QPaintEvent>

/* GUI includes: */
#include "VBoxDefs.h"
#include "CFramebuffer.h"

/* Other VBox includes: */
#include <iprt/critsect.h>

/* Forward declarations: */
class UIMachineView;

/**
 *  Frame buffer resize event.
 */
class UIResizeEvent : public QEvent
{
public:

    UIResizeEvent(ulong uPixelFormat, uchar *pVRAM,
                  ulong uBitsPerPixel, ulong uBytesPerLine,
                  ulong uWidth, ulong uHeight)
        : QEvent((QEvent::Type)VBoxDefs::ResizeEventType)
        , m_uPixelFormat(uPixelFormat), m_pVRAM(pVRAM), m_uBitsPerPixel(uBitsPerPixel)
        , m_uBytesPerLine(uBytesPerLine), m_uWidth(uWidth), m_uHeight(uHeight) {}
    ulong pixelFormat() { return m_uPixelFormat; }
    uchar *VRAM() { return m_pVRAM; }
    ulong bitsPerPixel() { return m_uBitsPerPixel; }
    ulong bytesPerLine() { return m_uBytesPerLine; }
    ulong width() { return m_uWidth; }
    ulong height() { return m_uHeight; }

private:

    ulong m_uPixelFormat;
    uchar *m_pVRAM;
    ulong m_uBitsPerPixel;
    ulong m_uBytesPerLine;
    ulong m_uWidth;
    ulong m_uHeight;
};

/**
 *  Frame buffer repaint event.
 */
class UIRepaintEvent : public QEvent
{
public:

    UIRepaintEvent(int iX, int iY, int iW, int iH)
        : QEvent((QEvent::Type)VBoxDefs::RepaintEventType)
        , m_iX(iX), m_iY(iY), m_iW(iW), m_iH(iH) {}
    int x() { return m_iX; }
    int y() { return m_iY; }
    int width() { return m_iW; }
    int height() { return m_iH; }

private:

    int m_iX, m_iY, m_iW, m_iH;
};

/**
 *  Frame buffer set region event.
 */
class UISetRegionEvent : public QEvent
{
public:

    UISetRegionEvent(const QRegion &region)
        : QEvent((QEvent::Type)VBoxDefs::SetRegionEventType)
        , m_region(region) {}
    QRegion region() { return m_region; }

private:

    QRegion m_region;
};

/**
 *  Common IFramebuffer implementation for all methods used by GUI to maintain
 *  the VM display video memory.
 *
 *  Note that although this class can be called from multiple threads
 *  (in particular, the GUI thread and EMT) it doesn't protect access to every
 *  data field using its mutex lock. This is because all synchronization between
 *  the GUI and the EMT thread is supposed to be done using the
 *  IFramebuffer::NotifyUpdate() and IFramebuffer::RequestResize() methods
 *  (in particular, the \a aFinished parameter of these methods is responsible
 *  for the synchronization). These methods are always called on EMT and
 *  therefore always follow one another but never in parallel.
 *
 *  Using this object's mutex lock (exposed also in IFramebuffer::Lock() and
 *  IFramebuffer::Unlock() implementations) usually makes sense only if some
 *  third-party thread (i.e. other than GUI or EMT) needs to make sure that
 *  *no* VM display update or resize event can occur while it is accessing
 *  IFramebuffer properties or the underlying display memory storage area.
 *
 *  See IFramebuffer documentation for more info.
 */
class UIFrameBuffer : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:

    UIFrameBuffer(UIMachineView *aView);
    virtual ~UIFrameBuffer();

    void setDeleted(bool fIsDeleted) { m_fIsDeleted = fIsDeleted; }

    NS_DECL_ISUPPORTS

#if defined (Q_OS_WIN32)
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&m_iRefCnt);
    }

    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement(&m_iRefCnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
#endif

    VBOX_SCRIPTABLE_DISPATCH_IMPL(IFramebuffer)

    /* IFramebuffer COM methods */
    STDMETHOD(COMGETTER(Address)) (BYTE **ppAddress);
    STDMETHOD(COMGETTER(Width)) (ULONG *puWidth);
    STDMETHOD(COMGETTER(Height)) (ULONG *puHeight);
    STDMETHOD(COMGETTER(BitsPerPixel)) (ULONG *puBitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine)) (ULONG *puBytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *puPixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *pbUsesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *puHeightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **ppOverlay);
    STDMETHOD(COMGETTER(WinId)) (LONG64 *pWinId);

    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();

    STDMETHOD(RequestResize) (ULONG uScreenId, ULONG uPixelFormat,
                              BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                              ULONG uWidth, ULONG uHeight,
                              BOOL *pbFinished);

    STDMETHOD(VideoModeSupported) (ULONG uWidth, ULONG uHeight, ULONG uBPP,
                                   BOOL *pbSupported);

    STDMETHOD(GetVisibleRegion)(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *pRectangles, ULONG uCount);

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

    ulong width() { return m_width; }
    ulong height() { return m_height; }

    inline int convertGuestXTo(int x) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.width() / m_width * x) : x; }
    inline int convertGuestYTo(int y) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.height() / m_height * y) : y; }
    inline int convertHostXTo(int x) const  { return m_scaledSize.isValid() ? qRound((double)m_width / m_scaledSize.width() * x) : x; }
    inline int convertHostYTo(int y) const  { return m_scaledSize.isValid() ? qRound((double)m_height / m_scaledSize.height() * y) : y; }

    virtual QSize scaledSize() const { return m_scaledSize; }
    virtual void setScaledSize(const QSize &size = QSize()) { m_scaledSize = size; }

    virtual ulong pixelFormat()
    {
        return FramebufferPixelFormat_FOURCC_RGB;
    }

    virtual bool usesGuestVRAM()
    {
        return false;
    }

    void lock() { RTCritSectEnter(&m_critSect); }
    void unlock() { RTCritSectLeave(&m_critSect); }

    virtual uchar *address() = 0;
    virtual ulong bitsPerPixel() = 0;
    virtual ulong bytesPerLine() = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when some part of the
     *  VM display viewport needs to be repainted on the host screen.
     */
    virtual void paintEvent(QPaintEvent *pEvent) = 0;

    /**
     *  Called on the GUI thread (from VBoxConsoleView) after it gets a
     *  UIResizeEvent posted from the RequestResize() method implementation.
     */
    virtual void resizeEvent(UIResizeEvent *pEvent)
    {
        m_width = pEvent->width();
        m_height = pEvent->height();
    }

    /**
     *  Called on the GUI thread (from VBoxConsoleView) when the VM console
     *  window is moved.
     */
    virtual void moveEvent(QMoveEvent * /* pEvent */) {}

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* this method is called from the GUI thread
     * to perform the actual Video HW Acceleration command processing
     * the event is framebuffer implementation specific */
    virtual void doProcessVHWACommand(QEvent * pEvent);

    virtual void viewportResized(QResizeEvent * /* pEvent */) {}

    virtual void viewportScrolled(int /* iX */, int /* iY */) {}
#endif

    virtual void setView(UIMachineView * pView);

protected:

    UIMachineView *m_pMachineView;
    RTCRITSECT m_critSect;
    ulong m_width;
    ulong m_height;
    QSize m_scaledSize;
    int64_t m_WinId;
    bool m_fIsDeleted;

#if defined (Q_OS_WIN32)
private:

    long m_iRefCnt;
#endif
};

#endif // !___UIFrameBuffer_h___
