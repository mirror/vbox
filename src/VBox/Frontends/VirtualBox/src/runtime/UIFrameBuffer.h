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
#include <QImage>
#include <QTransform>

/* GUI includes: */
#include "UIDefs.h"
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "CDisplay.h"
#include "CDisplaySourceBitmap.h"

/* Other VBox includes: */
#include <iprt/critsect.h>

/* Forward declarations: */
class QResizeEvent;
class QPaintEvent;
class UIMachineView;

/** Common IFramebuffer implementation used to maintain VM display video memory. */
class ATL_NO_VTABLE UIFrameBuffer :
    public QObject,
    public CComObjectRootEx<CComMultiThreadModel>,
    VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
    Q_OBJECT;

signals:

    /** Notifies listener about guest-screen resolution changes. */
    void sigNotifyChange(int iWidth, int iHeight);
    /** Notifies listener about guest-screen updates. */
    void sigNotifyUpdate(int iX, int iY, int iWidth, int iHeight);
    /** Notifies listener about guest-screen visible-region changes. */
    void sigSetVisibleRegion(QRegion region);
    /** Notifies listener about guest 3D capability changes. */
    void sigNotifyAbout3DOverlayVisibilityChange(bool fVisible);

public:

    /** Frame-buffer constructor. */
    UIFrameBuffer();
    /** Frame-buffer destructor. */
    ~UIFrameBuffer();

    /** Frame-buffer initialization.
      * @param pMachineView defines machine-view this frame-buffer is bounded to. */
    HRESULT init(UIMachineView *pMachineView);

    /** Returns the copy of the IDisplay wrapper. */
    CDisplay display() const { return m_display; }

    /** Assigns machine-view frame-buffer will be bounded to.
      * @param pMachineView defines machine-view this frame-buffer is bounded to. */
    virtual void setView(UIMachineView *pMachineView);

    /** Defines whether frame-buffer is <b>unused</b>.
      * @note Refer to m_fUnused for more information.
      * @note Calls to this and any other EMT callback are synchronized (from GUI side). */
    void setMarkAsUnused(bool fUnused);

    /** Returns the visual-state this frame-buffer is used for. */
    UIVisualStateType visualState() const;

    /** Returns whether frame-buffer is <b>auto-enabled</b>.
      * @note Refer to m_fAutoEnabled for more information. */
    bool isAutoEnabled() const { return m_fAutoEnabled; }
    /** Defines whether frame-buffer is <b>auto-enabled</b>.
      * @note Refer to m_fAutoEnabled for more information. */
    void setAutoEnabled(bool fAutoEnabled) { m_fAutoEnabled = fAutoEnabled; }

    DECLARE_NOT_AGGREGATABLE(UIFrameBuffer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(UIFrameBuffer)
        COM_INTERFACE_ENTRY(IFramebuffer)
        COM_INTERFACE_ENTRY2(IDispatch,IFramebuffer)
        COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler.p)
    END_COM_MAP()

    HRESULT FinalConstruct();
    void FinalRelease();

    /* IFramebuffer COM methods: */
    STDMETHOD(COMGETTER(Width))(ULONG *puWidth);
    STDMETHOD(COMGETTER(Height))(ULONG *puHeight);
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *puBitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *puBytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat))(ULONG *puPixelFormat);
    STDMETHOD(COMGETTER(HeightReduction))(ULONG *puHeightReduction);
    STDMETHOD(COMGETTER(Overlay))(IFramebufferOverlay **ppOverlay);
    STDMETHOD(COMGETTER(WinId))(LONG64 *pWinId);
    STDMETHOD(COMGETTER(Capabilities))(ComSafeArrayOut(FramebufferCapabilities_T, aCapabilities));

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

    /** EMT callback: Notifies frame-buffer about guest-screen update.
      * @param        uX      Horizontal origin of the update rectangle, in pixels.
      * @param        uY      Vertical origin of the update rectangle, in pixels.
      * @param        uWidth  Width of the update rectangle, in pixels.
      * @param        uHeight Height of the update rectangle, in pixels.
      * @param        image   Brings image container which can be used to copy data from.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(NotifyUpdateImage)(ULONG uX, ULONG uY, ULONG uWidth, ULONG uHeight, ComSafeArrayIn(BYTE, image));

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
      * @param        aData Event-specific data, depends on the supplied event type.
      * @note         Any EMT callback is subsequent. No any other EMT callback can be called until this one processed.
      * @note         Calls to this and #setMarkAsUnused method are synchronized (from GUI side). */
    STDMETHOD(Notify3DEvent)(ULONG uType, ComSafeArrayIn(BYTE, data));

    /** Returns frame-buffer data address. */
    uchar *address() { return m_image.bits(); }
    /** Returns frame-buffer width. */
    ulong width() const { return m_iWidth; }
    /** Returns frame-buffer height. */
    ulong height() const { return m_iHeight; }
    /** Returns frame-buffer bits-per-pixel value. */
    ulong bitsPerPixel() const { return m_image.depth(); }
    /** Returns frame-buffer bytes-per-line value. */
    ulong bytesPerLine() const { return m_image.bytesPerLine(); }
    /** Returns default frame-buffer pixel-format. */
    ulong pixelFormat() const { return BitmapFormat_BGR; }

    /** Locks frame-buffer access. */
    void lock() const { RTCritSectEnter(&m_critSect); }
    /** Unlocks frame-buffer access. */
    void unlock() const { RTCritSectLeave(&m_critSect); }

    /** Returns the frame-buffer's scaled-size. */
    QSize scaledSize() const { return m_scaledSize; }
    /** Defines host-to-guest scale ratio as @a size. */
    void setScaledSize(const QSize &size = QSize()) { m_scaledSize = size; }
    /** Returns x-origin of the host (scaled) content corresponding to x-origin of guest (actual) content. */
    inline int convertGuestXTo(int x) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.width() / m_iWidth * x) : x; }
    /** Returns y-origin of the host (scaled) content corresponding to y-origin of guest (actual) content. */
    inline int convertGuestYTo(int y) const { return m_scaledSize.isValid() ? qRound((double)m_scaledSize.height() / m_iHeight * y) : y; }
    /** Returns x-origin of the guest (actual) content corresponding to x-origin of host (scaled) content. */
    inline int convertHostXTo(int x) const  { return m_scaledSize.isValid() ? qRound((double)m_iWidth / m_scaledSize.width() * x) : x; }
    /** Returns y-origin of the guest (actual) content corresponding to y-origin of host (scaled) content. */
    inline int convertHostYTo(int y) const  { return m_scaledSize.isValid() ? qRound((double)m_iHeight / m_scaledSize.height() * y) : y; }

    /** Handles frame-buffer notify-change-event. */
    virtual void handleNotifyChange(int iWidth, int iHeight);
    /** Handles frame-buffer paint-event. */
    virtual void handlePaintEvent(QPaintEvent *pEvent);
    /** Handles frame-buffer set-visible-region-event. */
    virtual void handleSetVisibleRegion(const QRegion &region);

    /** Performs frame-buffer resizing. */
    virtual void performResize(int iWidth, int iHeight);
    /** Performs frame-buffer rescaling. */
    virtual void performRescale();

#ifdef VBOX_WITH_VIDEOHWACCEL
    /** Performs Video HW Acceleration command. */
    virtual void doProcessVHWACommand(QEvent *pEvent);
    /** Handles viewport resize-event. */
    virtual void viewportResized(QResizeEvent * /* pEvent */) {}
    /** Handles viewport scroll-event. */
    virtual void viewportScrolled(int /* iX */, int /* iY */) {}
#endif /* VBOX_WITH_VIDEOHWACCEL */

    /** Returns the scale-factor used by the frame-buffer. */
    double scaleFactor() const { return m_dScaleFactor; }
    /** Define the scale-factor used by the frame-buffer. */
    void setScaleFactor(double dScaleFactor) { m_dScaleFactor = dScaleFactor; }

    /** Returns backing-scale-factor used by HiDPI frame-buffer. */
    double backingScaleFactor() const { return m_dBackingScaleFactor; }
    /** Defines backing-scale-factor used by HiDPI frame-buffer. */
    void setBackingScaleFactor(double dBackingScaleFactor) { m_dBackingScaleFactor = dBackingScaleFactor; }

    /** Returns whether frame-buffer should use unscaled HiDPI output. */
    bool useUnscaledHiDPIOutput() const { return m_fUseUnscaledHiDPIOutput; }
    /** Defines whether frame-buffer should use unscaled HiDPI output. */
    void setUseUnscaledHiDPIOutput(bool fUseUnscaledHiDPIOutput) { m_fUseUnscaledHiDPIOutput = fUseUnscaledHiDPIOutput; }

    /** Return HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType hiDPIOptimizationType() const { return m_hiDPIOptimizationType; }
    /** Define HiDPI frame-buffer optimization type: */
    void setHiDPIOptimizationType(HiDPIOptimizationType optimizationType) { m_hiDPIOptimizationType = optimizationType; }

protected:

    /** Prepare connections routine. */
    void prepareConnections();
    /** Cleanup connections routine. */
    void cleanupConnections();

    /** Updates coordinate-system: */
    void updateCoordinateSystem();

    /** Default paint routine. */
    void paintDefault(QPaintEvent *pEvent);
    /** Paint routine for seamless mode. */
    void paintSeamless(QPaintEvent *pEvent);

    /** Erases corresponding @a rect with @a painter. */
    static void eraseImageRect(QPainter &painter, const QRect &rect,
                               bool fUseUnscaledHiDPIOutput,
                               HiDPIOptimizationType hiDPIOptimizationType,
                               double dBackingScaleFactor);
    /** Draws corresponding @a rect of passed @a image with @a painter. */
    static void drawImageRect(QPainter &painter, const QImage &image, const QRect &rect,
                              int iContentsShiftX, int iContentsShiftY,
                              bool fUseUnscaledHiDPIOutput,
                              HiDPIOptimizationType hiDPIOptimizationType,
                              double dBackingScaleFactor);

    /** Holds the QImage buffer. */
    QImage m_image;
    /** Holds frame-buffer width. */
    int m_iWidth;
    /** Holds frame-buffer height. */
    int m_iHeight;

    /** Holds the copy of the IDisplay wrapper. */
    CDisplay m_display;
    /** Source bitmap from IDisplay. */
    CDisplaySourceBitmap m_sourceBitmap;
    /** Source bitmap from IDisplay (acquired but not yet applied). */
    CDisplaySourceBitmap m_pendingSourceBitmap;
    /** There is a pending source bitmap which must be applied. */
    bool m_fPendingSourceBitmap;

    /** Holds machine-view this frame-buffer is bounded to. */
    UIMachineView *m_pMachineView;
    /** Holds window ID this frame-buffer referring to. */
    int64_t m_iWinId;

    /** Reflects whether screen-updates are allowed. */
    bool m_fUpdatesAllowed;

    /** Defines whether frame-buffer is <b>unused</b>.
      * <b>Unused</b> status determines whether frame-buffer should ignore EMT events or not. */
    bool m_fUnused;

    /** Defines whether frame-buffer is <b>auto-enabled</b>.
      * <b>Auto-enabled</b> status means that guest-screen corresponding to this frame-buffer
      * was automatically enabled by the multi-screen layout (auto-mount guest-screen) functionality,
      * so have potentially incorrect size-hint posted into guest event queue.
      * Machine-view will try to automatically adjust guest-screen size as soon as possible. */
    bool m_fAutoEnabled;

    /** RTCRITSECT object to protect frame-buffer access. */
    mutable RTCRITSECT m_critSect;

    /** @name Scale-factor related variables.
     * @{ */
    /** Holds the scale-factor used by the scaled-size. */
    double m_dScaleFactor;
    /** Holds the coordinate-system for the scale-factor above. */
    QTransform m_transform;
    /** Holds the frame-buffer's scaled-size. */
    QSize m_scaledSize;
    /** @} */

    /** @name Seamless mode related variables.
     * @{ */
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
    /** When the framebuffer is being resized, visible region is saved here.
      * The saved region is applied when updates are enabled again. */
    QRegion m_pendingSyncVisibleRegion;
    /** @} */

    /** @name HiDPI screens related variables.
     * @{ */
    /** Holds backing-scale-factor used by HiDPI frame-buffer. */
    double m_dBackingScaleFactor;
    /** Holds whether frame-buffer should use unscaled HiDPI output. */
    bool m_fUseUnscaledHiDPIOutput;
    /** Holds HiDPI frame-buffer optimization type. */
    HiDPIOptimizationType m_hiDPIOptimizationType;
    /** @} */

private:

#ifdef Q_OS_WIN
     CComPtr <IUnknown> m_pUnkMarshaler;
#endif /* Q_OS_WIN */
};

#endif /* !___UIFrameBuffer_h___ */
