/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIFrameBuffer class and subclasses declarations
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef ___UIFrameBuffer_h___
#define ___UIFrameBuffer_h___

/* Global includes */
//#include <QImage>
#include <QPixmap>
//#include <QMutex>
#include <QPaintEvent>
//#include <QMoveEvent>
//#if defined (VBOX_GUI_USE_QGL)
//#include "VBoxFBOverlay.h"
//#endif

/* Local includes */
#include "COMDefs.h"
#include <iprt/critsect.h>

//#if defined (VBOX_GUI_USE_SDL)
//#include <SDL.h>
//#include <signal.h>
//#endif

//#if defined (Q_WS_WIN) && defined (VBOX_GUI_USE_DDRAW)
// VBox/cdefs.h defines these:
//#undef LOWORD
//#undef HIWORD
//#undef LOBYTE
//#undef HIBYTE
//#include <ddraw.h>
//#endif

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
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *pWinId);

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

protected:

    UIMachineView *m_pMachineView;
    RTCRITSECT m_critSect;
    int m_width;
    int m_height;
    uint64_t m_uWinId;

#if defined (Q_OS_WIN32)
private:

    long m_iRefCnt;
#endif
};

#if defined (VBOX_GUI_USE_QIMAGE)
class UIFrameBufferQImage : public UIFrameBuffer
{
public:

    UIFrameBufferQImage(UIMachineView *pMachineView);

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);

    ulong pixelFormat() { return m_uPixelFormat; }
    bool usesGuestVRAM() { return m_bUsesGuestVRAM; }

    uchar *address() { return m_img.bits(); }
    ulong bitsPerPixel() { return m_img.depth(); }
    ulong bytesPerLine() { return m_img.bytesPerLine(); }

    void paintEvent (QPaintEvent *pEvent);
    void resizeEvent (UIResizeEvent *pEvent);

private:

    QPixmap m_PM;
    QImage m_img;
    ulong m_uPixelFormat;
    bool m_bUsesGuestVRAM;
};
#endif

#if 0
#if defined (VBOX_GUI_USE_QGL)
class UIFrameBufferQGL : public UIFrameBuffer
{
public:

    UIFrameBufferQGL(UIMachineView *pMachineView);

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);
#ifdef VBOXQGL_PROF_BASE
    STDMETHOD(RequestResize) (ULONG uScreenId, ULONG uPixelFormat,
                              BYTE *pVRAM, ULONG uBitsPerPixel, ULONG uBytesPerLine,
                              ULONG uWidth, ULONG uHeight, BOOL *pbFinished);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
    STDMETHOD(ProcessVHWACommand)(BYTE *pbCommand);
#endif

    ulong pixelFormat() { return vboxWidget()->vboxPixelFormat(); }
    bool usesGuestVRAM() { return vboxWidget()->vboxUsesGuestVRAM(); }

    uchar *address() { return vboxWidget()->vboxAddress(); }
    ulong bitsPerPixel() { return vboxWidget()->vboxBitsPerPixel(); }
    ulong bytesPerLine() { return vboxWidget()->vboxBytesPerLine(); }

    void paintEvent (QPaintEvent *pEvent);
    void resizeEvent (UIResizeEvent *pEvent);
    void doProcessVHWACommand(QEvent *pEvent);

private:

    class VBoxGLWidget* vboxWidget();

    class VBoxVHWACommandElementProcessor m_cmdPipe;
};
#endif

#if defined (VBOX_GUI_USE_SDL)
class UIFrameBufferSDL : public UIFrameBuffer
{
public:

    UIFrameBufferSDL (VBoxConsoleView *pMachineView);
    virtual ~UIFrameBufferSDL();

    STDMETHOD(NotifyUpdate) (ULONG aX, ULONG aY, ULONG aW, ULONG aH);

    uchar* address()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? (uchar*) (uintptr_t) surf->pixels : 0;
    }

    ulong bitsPerPixel()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? surf->format->BitsPerPixel : 0;
    }

    ulong bytesPerLine()
    {
        SDL_Surface *surf = m_pSurfVRAM ? m_pSurfVRAM : m_pScreen;
        return surf ? surf->pitch : 0;
    }

    ulong pixelFormat()
    {
        return m_uPixelFormat;
    }

    bool usesGuestVRAM()
    {
        return m_pSurfVRAM != NULL;
    }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);

private:

    SDL_Surface *m_pScreen;
    SDL_Surface *m_pSurfVRAM;

    ulong m_uPixelFormat;
};
#endif

#if defined (VBOX_GUI_USE_DDRAW)
class UIFrameBufferDDRAW : public UIFrameBuffer
{
public:

    UIFrameBufferDDRAW(UIMachineView *pMachineView);
    virtual ~UIFrameBufferDDRAW();

    STDMETHOD(NotifyUpdate) (ULONG uX, ULONG uY, ULONG uW, ULONG uH);

    uchar* address() { return (uchar*) m_surfaceDesc.lpSurface; }
    ulong bitsPerPixel() { return m_surfaceDesc.ddpfPixelFormat.dwRGBBitCount; }
    ulong bytesPerLine() { return (ulong) m_surfaceDesc.lPitch; }

    ulong pixelFormat() { return m_uPixelFormat; };

    bool usesGuestVRAM() { return m_bUsesGuestVRAM; }

    void paintEvent(QPaintEvent *pEvent);
    void resizeEvent(UIResizeEvent *pEvent);
    void moveEvent(QMoveEvent *pEvent);

private:

    void releaseObjects();

    bool createSurface(ULONG uPixelFormat, uchar *pvVRAM,
                       ULONG uBitsPerPixel, ULONG uBytesPerLine,
                       ULONG uWidth, ULONG uHeight);
    void deleteSurface();
    void drawRect(ULONG uX, ULONG uY, ULONG uW, ULONG uH);
    void getWindowPosition(void);

    LPDIRECTDRAW7        m_DDRAW;
    LPDIRECTDRAWCLIPPER  m_clipper;
    LPDIRECTDRAWSURFACE7 m_surface;
    DDSURFACEDESC2       m_surfaceDesc;
    LPDIRECTDRAWSURFACE7 m_primarySurface;

    ulong m_uPixelFormat;

    bool m_bUsesGuestVRAM;

    int m_iWndX;
    int m_iWndY;

    bool m_bSynchronousUpdates;
};
#endif

#endif

#endif // !___UIFrameBuffer_h___

