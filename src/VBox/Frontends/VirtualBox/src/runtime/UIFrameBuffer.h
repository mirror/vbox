/** @file
 * VBox Qt GUI - UIFrameBuffer class declaration.
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

#ifndef ___UIFrameBuffer_h___
#define ___UIFrameBuffer_h___

/* Qt includes: */
#include <QRegion>

/* GUI includes: */
#include "UIDefs.h"
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "CFramebuffer.h"
#include "CDisplaySourceBitmap.h"

/* Other VBox includes: */
#include <iprt/critsect.h>

/* Forward declarations: */
class QResizeEvent;
class QPaintEvent;
class UIMachineView;

/** Frame-buffer resize-event. */
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

/** Common IFramebuffer implementation used to maintain VM display video memory. */
class UIFrameBuffer : public QObject, VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
    Q_OBJECT;

signals:

    /** Notifies listener about guest-screen resolution changes. */
    void sigNotifyChange(ulong uScreenId, int iWidth, int iHeight);
    /** Notifies listener about guest-screen updates. */
    void sigNotifyUpdate(int iX, int iY, int iWidth, int iHeight);
    /** Notifies listener about guest-screen visible-region changes. */
    void sigSetVisibleRegion(QRegion region);
    /** Notifies listener about guest 3D capability changes. */
    void sigNotifyAbout3DOverlayVisibilityChange(bool fVisible);

public:

    /** Frame-buffer constructor.
      * @param pMachineView defines machine-view this frame-buffer is bounded to. */
    UIFrameBuffer(UIMachineView *pMachineView);
    /** Frame-buffer destructor. */
    ~UIFrameBuffer();

    /** Assigns machine-view frame-buffer will be bounded to.
      * @param pMachineView defines machine-view this frame-buffer is bounded to. */
    void setView(UIMachineView *pMachineView);

    /** Defines whether frame-buffer is <b>unused</b>.
      * @note Refer to m_fUnused for more information.
      * @note Calls to this and any other EMT callback are synchronized (from GUI side). */
    void setMarkAsUnused(bool fUnused);

    /** Returns whether frame-buffer is <b>auto-enabled</b>.
      * @note Refer to m_fAutoEnabled for more information. */
    bool isAutoEnabled() const { return m_fAutoEnabled; }
    /** Defines whether frame-buffer is <b>auto-enabled</b>.
      * @note Refer to m_fAutoEnabled for more information. */
    void setAutoEnabled(bool fAutoEnabled) { m_fAutoEnabled = fAutoEnabled; }

    NS_DECL_ISUPPORTS

#ifdef Q_OS_WIN
    /** Windows: Increments reference-counter. */
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement(&m_iRefCnt);
    }

    /** Windows: Decrements reference-counter. */
    STDMETHOD_(ULONG, Release)()
    {
        long iCounter = ::InterlockedDecrement(&m_iRefCnt);
        if (iCounter == 0)
            delete this;
        return iCounter;
    }
#endif /* Q_OS_WIN */

    VBOX_SCRIPTABLE_DISPATCH_IMPL(IFramebuffer)

    /* IFramebuffer COM methods: */
    STDMETHOD(COMGETTER(Address))(BYTE **ppAddress);
    STDMETHOD(COMGETTER(Width))(ULONG *puWidth);
    STDMETHOD(COMGETTER(Height))(ULONG *puHeight);
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *puBitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *puBytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat))(ULONG *puPixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM))(BOOL *pbUsesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction))(ULONG *puHeightReduction);
    STDMETHOD(COMGETTER(Overlay))(IFramebufferOverlay **ppOverlay);
    STDMETHOD(COMGETTER(WinId))(LONG64 *pWinId);

    /* IFramebuffer sync methods: */
    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();

    /** EMT callback: Notifies frame-buffer about guest-screen size change.
      * @param        uScreenId Guest screen number.
      * @param        uX        Horizontal origin of the update rectangle, in pixels.
      * @param        uY        Vertical origin of the update rectangle, in pixels.
      * @param        uWidth    Width of the guest display, in pixels.
      * @param        uHeight   Height of the guest display, in pixels.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(NotifyChange)(ULONG uScreenId, ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight);

    /** EMT callback: Notifies frame-buffer about guest-screen update.
      * @param        uX      Horizontal origin of the update rectangle, in pixels.
      * @param        uY      Vertical origin of the update rectangle, in pixels.
      * @param        uWidth  Width of the update rectangle, in pixels.
      * @param        uHeight Height of the update rectangle, in pixels.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(NotifyUpdate)(ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight);

    /** EMT callback: Returns whether the frame-buffer implementation is willing to support a given video-mode.
      * @param        uWidth      Width of the guest display, in pixels.
      * @param        uHeight     Height of the guest display, in pixels.
      * @param        uBPP        Color depth, bits per pixel.
      * @param        pfSupported Is frame-buffer able/willing to render the video mode or not.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(VideoModeSupported)(ULONG uWidth, ULONG uHeight, ULONG uBPP, BOOL *pbSupported);

    /** EMT callback which is not used in current implementation. */
    STDMETHOD(GetVisibleRegion)(BYTE *pRectangles, ULONG uCount, ULONG *puCountCopied);
    /** EMT callback: Suggests new visible-region to this frame-buffer.
      * @param        pRectangles Pointer to the RTRECT array.
      * @param        uCount      Number of RTRECT elements in the rectangles array.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(SetVisibleRegion)(BYTE *pRectangles, ULONG uCount);

    /** EMT callback which is not used in current implementation. */
    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

    /** EMT callback: Notifies frame-buffer about 3D backend event.
      * @param        uType Event type. Currently only VBOX3D_NOTIFY_EVENT_TYPE_VISIBLE_3DDATA is supported.
      * @param        pData Event-specific data, depends on the supplied event type.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(Notify3DEvent)(ULONG uType, BYTE *pData);

    /** Returns frame-buffer data address. */
    virtual uchar *address() = 0;
    /** Returns frame-buffer width. */
    ulong width() { return m_width; }
    /** Returns frame-buffer height. */
    ulong height() { return m_height; }
    /** Returns frame-buffer bits-per-pixel value. */
    virtual ulong bitsPerPixel() = 0;
    /** Returns frame-buffer bytes-per-line value. */
    virtual ulong bytesPerLine() = 0;
    /** Returns default frame-buffer pixel-format. */
    virtual ulong pixelFormat() { return FramebufferPixelFormat_FOURCC_RGB; }

    /** Locks frame-buffer access. */
    void lock() const { RTCritSectEnter(&m_critSect); }
    /** Unlocks frame-buffer access. */
    void unlock() const { RTCritSectLeave(&m_critSect); }

    /** Returns host-to-guest scale ratio. */
    QSize scaledSize() const { return m_scaledSize; }
    /** Defines host-to-guest scale ratio as @a size. */
    void setScaledSize(const QSize &size = QSize()) { m_scaledSize = size; }
    /** Returns x-origin of the host (scaled) content corresponding to x-origin of guest (actual) content. */
    inline int convertGuestXTo(int x) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.width() / m_width * x) : x; }
    /** Returns y-origin of the host (scaled) content corresponding to y-origin of guest (actual) content. */
    inline int convertGuestYTo(int y) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.height() / m_height * y) : y; }
    /** Returns x-origin of the guest (actual) content corresponding to x-origin of host (scaled) content. */
    inline int convertHostXTo(int x) const  { return m_scaledSize.isValid() ? qRound((double)m_width / m_scaledSize.width() * x) : x; }
    /** Returns y-origin of the guest (actual) content corresponding to y-origin of host (scaled) content. */
    inline int convertHostYTo(int y) const  { return m_scaledSize.isValid() ? qRound((double)m_height / m_scaledSize.height() * y) : y; }

    /** Handles frame-buffer notify-change-event. */
    void notifyChange();
    /** Handles frame-buffer resize-event. */
    virtual void resizeEvent(UIResizeEvent *pEvent) = 0;
    /** Handles frame-buffer paint-event. */
    virtual void paintEvent(QPaintEvent *pEvent) = 0;
    /** Handles frame-buffer apply-visible-region-event. */
    void applyVisibleRegion(const QRegion &region);

#ifdef VBOX_WITH_VIDEOHWACCEL
    /** Performs Video HW Acceleration command. */
    virtual void doProcessVHWACommand(QEvent *pEvent);
    /** Handles viewport resize-event. */
    virtual void viewportResized(QResizeEvent * /* pEvent */) {}
    /** Handles viewport scroll-event. */
    virtual void viewportScrolled(int /* iX */, int /* iY */) {}
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /** Return HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType hiDPIOptimizationType() const { return m_hiDPIOptimizationType; }
    /** Define HiDPI frame-buffer optimization type: */
    void setHiDPIOptimizationType(HiDPIOptimizationType optimizationType) { m_hiDPIOptimizationType = optimizationType; }

    /** Return backing scale factor used by HiDPI frame-buffer. */
    double backingScaleFactor() const { return m_dBackingScaleFactor; }
    /** Define backing scale factor used by HiDPI frame-buffer. */
    void setBackingScaleFactor(double dBackingScaleFactor) { m_dBackingScaleFactor = dBackingScaleFactor; }

protected:

    /** Machine-view this frame-buffer is bounded to. */
    UIMachineView *m_pMachineView;
    /** Cached window ID. */
    int64_t m_winId;
    /** RTCRITSECT object to protect frame-buffer access. */
    mutable RTCRITSECT m_critSect;
    /** Frame-buffer width. */
    ulong m_width;
    /** Frame-buffer height. */
    ulong m_height;
    /** Frame-buffer scaled size. */
    QSize m_scaledSize;
    /** Defines whether frame-buffer is <b>unused</b>.
      * <b>Unused</b> status determines whether frame-buffer should ignore EMT events or not. */
    bool m_fUnused;
    /** Defines whether frame-buffer is <b>auto-enabled</b>.
      * <b>Auto-enabled</b> status means that guest-screen corresponding to this frame-buffer
      * was automatically enabled by the multi-screen layout (auto-mount guest-screen) functionality,
      * so have potentially incorrect size-hint posted into guest event queue.
      * Machine-view will try to automatically adjust guest-screen size as soon as possible. */
    bool m_fAutoEnabled;

    /** Source bitmap from IDisplay. */
    CDisplaySourceBitmap m_sourceBitmap;
    /** Source bitmap from IDisplay (acquired but not yet applied). */
    CDisplaySourceBitmap m_pendingSourceBitmap;
    /** Reflects whether screen-updates are allowed. */
    bool m_fIsUpdatesAllowed;

    /* To avoid a seamless flicker which caused by the latency between
     * the initial visible-region arriving from EMT thread
     * and actual visible-region appliance on GUI thread
     * it was decided to use two visible-region instances: */
    /** Sync visible-region which being updated synchronously by locking EMT thread.
      * Used for immediate manual clipping of the painting operations. */
    QRegion m_syncVisibleRegion;
    /** Async visible-region which being updated asynchronously by posting async-event from EMT to GUI thread,
      * Used to update viewport parts for visible-region changes,
      * because NotifyUpdate doesn't take into account these changes. */
    QRegion m_asyncVisibleRegion;

private:

    /** Prepare connections routine. */
    void prepareConnections();
    /** Cleanup connections routine. */
    void cleanupConnections();

#ifdef Q_OS_WIN
    /** Windows: Holds the reference counter. */
    long m_iRefCnt;
#endif /* Q_OS_WIN */

    /** Holds HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType m_hiDPIOptimizationType;

    /** Holds backing scale factor used by HiDPI frame-buffer. */
    double m_dBackingScaleFactor;
};

#endif /* !___UIFrameBuffer_h___ */
