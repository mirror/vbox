/** @file
 *
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Declaration of VBoxSDLFB (SDL framebuffer) class
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __H_FRAMEBUFFER
#define __H_FRAMEBUFFER

#include "VBoxSDL.h"
#include <iprt/thread.h>

#include <iprt/critsect.h>

#ifdef VBOX_SECURELABEL
#include <SDL_ttf.h>
/* function pointers */
extern "C"
{
extern DECLSPEC int (SDLCALL *pTTF_Init)(void);
extern DECLSPEC TTF_Font* (SDLCALL *pTTF_OpenFont)(const char *file, int ptsize);
extern DECLSPEC SDL_Surface* (SDLCALL *pTTF_RenderUTF8_Solid)(TTF_Font *font, const char *text, SDL_Color fg);
extern DECLSPEC SDL_Surface* (SDLCALL *pTTF_RenderUTF8_Shaded)(TTF_Font *font, const char *text, SDL_Color fg, SDL_Color bg);
extern DECLSPEC SDL_Surface* (SDLCALL *pTTF_RenderUTF8_Blended)(TTF_Font *font, const char *text, SDL_Color fg);
extern DECLSPEC void (SDLCALL *pTTF_CloseFont)(TTF_Font *font);
extern DECLSPEC void (SDLCALL *pTTF_Quit)(void);
}
#endif /* VBOX_SECURELABEL */

class VBoxSDLFBOverlay;

class VBoxSDLFB :
    VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:
    VBoxSDLFB(bool fFullscreen = false, bool fResizable = true, bool fShowSDLConfig = false,
              bool fKeepHostRes = false, uint32_t u32FixedWidth = ~(uint32_t)0,
              uint32_t u32FixedHeight = ~(uint32_t)0, uint32_t u32FixedBPP = ~(uint32_t)0);
    virtual ~VBoxSDLFB();

#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IFramebuffer)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    STDMETHOD(COMGETTER(Width))(ULONG *width);
    STDMETHOD(COMGETTER(Height))(ULONG *height);
    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();
    STDMETHOD(COMGETTER(Address))(BYTE **address);
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *bitsPerPixel);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *bytesPerLine);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *pixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *usesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *heightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (uint64_t *winId);

    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y,
                            ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(RequestResize)(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                             ULONG bitsPerPixel, ULONG bytesPerLine,
                             ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(OperationSupported)(FramebufferAccelerationOperation_T operation, BOOL *supported);
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported);
    STDMETHOD(SolidFill)(ULONG x, ULONG y, ULONG width, ULONG height,
                         ULONG color, BOOL *handled);
    STDMETHOD(CopyScreenBits)(ULONG xDst, ULONG yDst, ULONG xSrc, ULONG ySrc,
                              ULONG width, ULONG height, BOOL *handled);

    STDMETHOD(GetVisibleRegion)(BYTE *aRectangles, ULONG aCount, ULONG *aCountCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *aRectangles, ULONG aCount);

    // internal public methods
    bool initialized() { return mfInitialized; }
    void resizeGuest();
    void resizeSDL();
    void update(int x, int y, int w, int h, bool fGuestRelative);
    void repaint();
    bool getFullscreen();
    void setFullscreen(bool fFullscreen);
    int  getXOffset();
    int  getYOffset();
    void getFullscreenGeometry(uint32_t *width, uint32_t *height);
    uint32_t getGuestXRes() { return mGuestXRes; }
    uint32_t getGuestYRes() { return mGuestYRes; }
#ifdef VBOX_SECURELABEL
    int  initSecureLabel(uint32_t height, char *font, uint32_t pointsize, uint32_t labeloffs);
    void setSecureLabelText(const char *text);
    void setSecureLabelColor(uint32_t colorFG, uint32_t colorBG);
    void paintSecureLabel(int x, int y, int w, int h, bool fForce);
#endif
    void uninit();
    void setWinId(uint64_t winId) { mWinId = winId; }

private:
    /** the sdl thread */
    RTNATIVETHREAD mSdlNativeThread;
    /** current SDL framebuffer pointer (also includes screen width/height) */
    SDL_Surface *mScreen;
    /** false if constructor failed */
    bool mfInitialized;
    /** maximum possible screen width in pixels (~0 = no restriction) */
    uint32_t mMaxScreenWidth;
    /** maximum possible screen height in pixels (~0 = no restriction) */
    uint32_t mMaxScreenHeight;
    /** current guest screen width in pixels */
    ULONG mGuestXRes;
    /** current guest screen height in pixels */
    ULONG mGuestYRes;
    /** fixed SDL screen width (~0 = not set) */
    uint32_t mFixedSDLWidth;
    /** fixed SDL screen height (~0 = not set) */
    uint32_t mFixedSDLHeight;
    /** fixed SDL bits per pixel (~0 = not set) */
    uint32_t mFixedSDLBPP;
    /** default BPP */
    uint32_t mDefaultSDLBPP;
    /** Y offset in pixels, i.e. guest-nondrawable area at the top */
    uint32_t mTopOffset;
    /** X offset for guest screen centering */
    uint32_t mCenterXOffset;
    /** Y offset for guest screen centering */
    uint32_t mCenterYOffset;
    /** flag whether we're in fullscreen mode */
    bool  mfFullscreen;
    /** flag wheter we keep the host screen resolution when switching to
     *  fullscreen or not */
    bool  mfKeepHostRes;
    /** framebuffer update semaphore */
    RTCRITSECT mUpdateLock;
    /** flag whether the SDL window should be resizable */
    bool mfResizable;
    /** flag whether we print out SDL information */
    bool mfShowSDLConfig;
    /** handle to window where framebuffer context is being drawn*/
    uint64_t mWinId;
#ifdef VBOX_SECURELABEL
    /** current secure label text */
    Utf8Str mSecureLabelText;
    /** current secure label foreground color (RGB) */
    uint32_t mSecureLabelColorFG;
    /** current secure label background color (RGB) */
    uint32_t mSecureLabelColorBG;
    /** secure label font handle */
    TTF_Font *mLabelFont;
    /** secure label height in pixels */
    uint32_t mLabelHeight;
    /** secure label offset from the top of the secure label */
    uint32_t mLabelOffs;
#endif
#ifdef RT_OS_WINDOWS
    long refcnt;
#endif
    SDL_Surface *mSurfVRAM;

    BYTE *mPtrVRAM;
    ULONG mBitsPerPixel;
    ULONG mBytesPerLine;
    ULONG mPixelFormat;
    BOOL mUsesGuestVRAM;
    BOOL mfSameSizeRequested;

    /** the application Icon */
    SDL_Surface *mWMIcon;
};

class VBoxSDLFBOverlay :
    public IFramebufferOverlay
{
public:
    VBoxSDLFBOverlay(ULONG x, ULONG y, ULONG width, ULONG height, BOOL visible,
                     VBoxSDLFB *aParent);
    virtual ~VBoxSDLFBOverlay();

#ifdef RT_OS_WINDOWS
    STDMETHOD_(ULONG, AddRef)()
    {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)()
    {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0)
            delete this;
        return cnt;
    }
    STDMETHOD(QueryInterface) (REFIID riid , void **ppObj)
    {
        if (riid == IID_IUnknown)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IFramebuffer)
        {
            *ppObj = this;
            AddRef();
            return S_OK;
        }
        *ppObj = NULL;
        return E_NOINTERFACE;
    }
#endif

    NS_DECL_ISUPPORTS

    STDMETHOD(COMGETTER(X))(ULONG *x);
    STDMETHOD(COMGETTER(Y))(ULONG *y);
    STDMETHOD(COMGETTER(Width))(ULONG *width);
    STDMETHOD(COMGETTER(Height))(ULONG *height);
    STDMETHOD(COMGETTER(Visible))(BOOL *visible);
    STDMETHOD(COMSETTER(Visible))(BOOL visible);
    STDMETHOD(COMGETTER(Alpha))(ULONG *alpha);
    STDMETHOD(COMSETTER(Alpha))(ULONG alpha);
    STDMETHOD(COMGETTER(Address))(ULONG *address);
    STDMETHOD(COMGETTER(BytesPerLine))(ULONG *bytesPerLine);

    /* These are not used, or return standard values. */
    STDMETHOD(COMGETTER(BitsPerPixel))(ULONG *bitsPerPixel);
    STDMETHOD(COMGETTER(PixelFormat)) (ULONG *pixelFormat);
    STDMETHOD(COMGETTER(UsesGuestVRAM)) (BOOL *usesGuestVRAM);
    STDMETHOD(COMGETTER(HeightReduction)) (ULONG *heightReduction);
    STDMETHOD(COMGETTER(Overlay)) (IFramebufferOverlay **aOverlay);
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId);

    STDMETHOD(Lock)();
    STDMETHOD(Unlock)();
    STDMETHOD(Move)(ULONG x, ULONG y);
    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y,
                            ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(RequestResize)(ULONG aScreenId, ULONG pixelFormat, ULONG vram,
                             ULONG bitsPerPixel, ULONG bytesPerLine,
                             ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(OperationSupported)(FramebufferAccelerationOperation_T operation,
                                  BOOL *supported);
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported);
    STDMETHOD(SolidFill)(ULONG x, ULONG y, ULONG width, ULONG height,
                         ULONG color, BOOL *handled);
    STDMETHOD(CopyScreenBits)(ULONG xDst, ULONG yDst, ULONG xSrc, ULONG ySrc,
                              ULONG width, ULONG height, BOOL *handled);

    // internal public methods
    HRESULT init();

private:
    /** Overlay X offset */
    ULONG mOverlayX;
    /** Overlay Y offset */
    ULONG mOverlayY;
    /** Overlay width */
    ULONG mOverlayWidth;
    /** Overlay height */
    ULONG mOverlayHeight;
    /** Whether the overlay is currently active */
    BOOL mOverlayVisible;
    /** The parent IFramebuffer */
    VBoxSDLFB *mParent;
    /** SDL surface containing the actual framebuffer bits */
    SDL_Surface *mOverlayBits;
    /** Additional SDL surface used for combining the framebuffer and the overlay */
    SDL_Surface *mBlendedBits;
#ifdef RT_OS_WINDOWS
    long refcnt;
#endif
};

#endif // __H_FRAMEBUFFER
