/** @file
 * VBox Qt GUI - UIFrameBuffer class declaration.
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

#ifndef ___UIFrameBuffer_h___
#define ___UIFrameBuffer_h___

/* Qt includes: */
#include <QRegion>
#include <QPaintEvent>

/* GUI includes: */
#include "UIDefs.h"

/* COM includes: */
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
        : QEvent((QEvent::Type)ResizeEventType)
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
class UIFrameBuffer : public QObject, VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
    Q_OBJECT;

signals:

    /* Notifiers: EMT<->GUI interthread stuff: */
    void sigRequestResize(int iPixelFormat, uchar *pVRAM,
                          int iBitsPerPixel, int iBytesPerLine,
                          int iWidth, int iHeight);
    void sigNotifyUpdate(int iX, int iY, int iWidth, int iHeight);
    void sigSetVisibleRegion(QRegion region);
    void sigNotifyAbout3DOverlayVisibilityChange(bool fVisible);

public:

    UIFrameBuffer(UIMachineView *aView);
    virtual ~UIFrameBuffer();

    /* API: [Un]used status stuff: */
    void setMarkAsUnused(bool fIsMarkAsUnused);

    /* API: Auto-enabled stuff: */
    bool isAutoEnabled() const;
    void setAutoEnabled(bool fIsAutoEnabled);

    NS_DECL_ISUPPORTS

#ifdef Q_OS_WIN
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
#endif /* Q_OS_WIN */

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

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight);

    STDMETHOD(VideoModeSupported) (ULONG uWidth, ULONG uHeight, ULONG uBPP,
                                   BOOL *pbSupported);

    STDMETHOD(GetVisibleRegion)(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *pRectangles, ULONG uCount);

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

    STDMETHOD(Notify3DEvent)(ULONG uType, BYTE *pData);

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

    void lock() const { RTCritSectEnter(&m_critSect); }
    void unlock() const { RTCritSectLeave(&m_critSect); }

    virtual uchar *address() = 0;
    virtual ulong bitsPerPixel() = 0;
    virtual ulong bytesPerLine() = 0;

    /* API: Event-delegate stuff: */
    virtual void moveEvent(QMoveEvent* /*pEvent*/) {}
    virtual void resizeEvent(UIResizeEvent *pEvent) = 0;
    virtual void paintEvent(QPaintEvent *pEvent) = 0;
    virtual void applyVisibleRegion(const QRegion &region);

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* this method is called from the GUI thread
     * to perform the actual Video HW Acceleration command processing
     * the event is framebuffer implementation specific */
    virtual void doProcessVHWACommand(QEvent * pEvent);

    virtual void viewportResized(QResizeEvent * /* pEvent */) {}

    virtual void viewportScrolled(int /* iX */, int /* iY */) {}
#endif /* VBOX_WITH_VIDEOHWACCEL */

    virtual void setView(UIMachineView * pView);

    /** Return HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType hiDPIOptimizationType() const { return m_hiDPIOptimizationType; }
    /** Define HiDPI frame-buffer optimization type: */
    void setHiDPIOptimizationType(HiDPIOptimizationType optimizationType);

    /** Return backing scale factor used by HiDPI frame-buffer. */
    double backingScaleFactor() const { return m_dBackingScaleFactor; }
    /** Define backing scale factor used by HiDPI frame-buffer. */
    void setBackingScaleFactor(double dBackingScaleFactor);

protected:

    UIMachineView *m_pMachineView;
    mutable RTCRITSECT m_critSect;
    ulong m_width;
    ulong m_height;
    QSize m_scaledSize;
    int64_t m_WinId;
    bool m_fIsMarkedAsUnused;
    bool m_fIsAutoEnabled;

    /* To avoid a seamless flicker,
     * which caused by the latency between the
     * initial visible-region arriving from EMT thread
     * and actual visible-region application on GUI thread
     * it was decided to use two visible-region instances:
     * 1. 'Sync-one' which being updated synchronously by locking EMT thread,
     *               and used for immediate manual clipping of the painting operations.
     * 2. 'Async-one' which updated asynchronously by posting async-event from EMT to GUI thread,
                      which is used to update viewport parts for visible-region changes,
                      because NotifyUpdate doesn't take into account these changes. */
    QRegion m_syncVisibleRegion;
    QRegion m_asyncVisibleRegion;

private:

    /* Helpers: Prepare/cleanup stuff: */
    void prepareConnections();
    void cleanupConnections();

#ifdef Q_OS_WIN
    long m_iRefCnt;
#endif /* Q_OS_WIN */

    /** Holds HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType m_hiDPIOptimizationType;

    /** Holds backing scale factor used by HiDPI frame-buffer. */
    double m_dBackingScaleFactor;
};

#endif // !___UIFrameBuffer_h___
