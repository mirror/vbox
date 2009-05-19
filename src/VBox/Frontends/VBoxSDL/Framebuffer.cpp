/** @file
 *
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Implementation of VBoxSDLFB (SDL framebuffer) class
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

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/EventQueue.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/stream.h>
#include <iprt/env.h>

#ifdef RT_OS_OS2
# undef RT_MAX
// from <iprt/cdefs.h>
# define RT_MAX(Value1, Value2)  ((Value1) >= (Value2) ? (Value1) : (Value2))
#endif

using namespace com;

#define LOG_GROUP LOG_GROUP_GUI
#include <VBox/err.h>
#include <VBox/log.h>

#include "VBoxSDL.h"
#include "Framebuffer.h"
#include "Ico64x01.h"

#if defined(VBOX_WITH_XPCOM)
NS_IMPL_ISUPPORTS1_CI(VBoxSDLFB, IFramebuffer)
NS_DECL_CLASSINFO(VBoxSDLFB)
NS_IMPL_ISUPPORTS1_CI(VBoxSDLFBOverlay, IFramebufferOverlay)
NS_DECL_CLASSINFO(VBoxSDLFBOverlay)
#endif

#ifdef VBOX_SECURELABEL
/* function pointers */
extern "C"
{
DECLSPEC int (SDLCALL *pTTF_Init)(void);
DECLSPEC TTF_Font* (SDLCALL *pTTF_OpenFont)(const char *file, int ptsize);
DECLSPEC SDL_Surface* (SDLCALL *pTTF_RenderUTF8_Solid)(TTF_Font *font, const char *text, SDL_Color fg);
DECLSPEC SDL_Surface* (SDLCALL *pTTF_RenderUTF8_Blended)(TTF_Font *font, const char *text, SDL_Color fg);
DECLSPEC void (SDLCALL *pTTF_CloseFont)(TTF_Font *font);
DECLSPEC void (SDLCALL *pTTF_Quit)(void);
}
#endif /* VBOX_SECURELABEL */

//
// Constructor / destructor
//

/**
 * SDL framebuffer constructor. It is called from the main
 * (i.e. SDL) thread. Therefore it is safe to use SDL calls
 * here.
 * @param fFullscreen    flag whether we start in fullscreen mode
 * @param fResizable     flag whether the SDL window should be resizable
 * @param fShowSDLConfig flag whether we print out SDL settings
 * @param fKeepHostRes   flag whether we switch the host screen resolution
 *                       when switching to fullscreen or not
 * @param iFixedWidth    fixed SDL width (-1 means not set)
 * @param iFixedHeight   fixed SDL height (-1 means not set)
 */
VBoxSDLFB::VBoxSDLFB(bool fFullscreen, bool fResizable, bool fShowSDLConfig,
                     bool fKeepHostRes, uint32_t u32FixedWidth,
                     uint32_t u32FixedHeight, uint32_t u32FixedBPP)
{
    int rc;
    LogFlow(("VBoxSDLFB::VBoxSDLFB\n"));

#if defined (RT_OS_WINDOWS)
    refcnt = 0;
#endif

    mScreen         = NULL;
    mSurfVRAM       = NULL;
    mfInitialized   = false;
    mfFullscreen    = fFullscreen;
    mfKeepHostRes   = fKeepHostRes;
    mTopOffset      = 0;
    mfResizable     = fResizable;
    mfShowSDLConfig = fShowSDLConfig;
    mFixedSDLWidth  = u32FixedWidth;
    mFixedSDLHeight = u32FixedHeight;
    mFixedSDLBPP    = u32FixedBPP;
    mDefaultSDLBPP  = 32;
    mCenterXOffset  = 0;
    mCenterYOffset  = 0;
    /* Start with standard screen dimensions. */
    mGuestXRes      = 640;
    mGuestYRes      = 480;
    mPixelFormat    = FramebufferPixelFormat_Opaque;
    mUsesGuestVRAM  = FALSE;
    mPtrVRAM        = NULL;
    mBitsPerPixel   = 0;
    mBytesPerLine   = 0;
    mfSameSizeRequested = false;
#ifdef VBOX_SECURELABEL
    mLabelFont      = NULL;
    mLabelHeight    = 0;
    mLabelOffs      = 0;
#endif
    mWMIcon         = NULL;

    /* memorize the thread that inited us, that's the SDL thread */
    mSdlNativeThread = RTThreadNativeSelf();

    rc = RTCritSectInit(&mUpdateLock);
    AssertMsg(rc == VINF_SUCCESS, ("Error from RTCritSectInit!\n"));

#ifdef RT_OS_WINDOWS
    /* default to DirectX if nothing else set */
    if (!RTEnvGet("SDL_VIDEODRIVER"))
    {
        _putenv("SDL_VIDEODRIVER=directx");
//        _putenv("SDL_VIDEODRIVER=windib");
    }
#endif
#ifdef VBOXSDL_WITH_X11
    /* On some X servers the mouse is stuck inside the bottom right corner.
     * See http://wiki.clug.org.za/wiki/QEMU_mouse_not_working */
    RTEnvSet("SDL_VIDEO_X11_DGAMOUSE", "0");
#endif
    rc = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE);
    if (rc != 0)
    {
        RTPrintf("SDL Error: '%s'\n", SDL_GetError());
        return;
    }

    const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
    Assert(videoInfo);
    if (videoInfo)
    {
        switch (videoInfo->vfmt->BitsPerPixel)
        {
            case 16: mDefaultSDLBPP = 16; break;
            case 24: mDefaultSDLBPP = 24; break;
            default:
            case 32: mDefaultSDLBPP = 32; break;
        }

        /* output what SDL is capable of */
        if (mfShowSDLConfig)
            RTPrintf("SDL capabilities:\n"
                     "  Hardware surface support:                    %s\n"
                     "  Window manager available:                    %s\n"
                     "  Screen to screen blits accelerated:          %s\n"
                     "  Screen to screen colorkey blits accelerated: %s\n"
                     "  Screen to screen alpha blits accelerated:    %s\n"
                     "  Memory to screen blits accelerated:          %s\n"
                     "  Memory to screen colorkey blits accelerated: %s\n"
                     "  Memory to screen alpha blits accelerated:    %s\n"
                     "  Color fills accelerated:                     %s\n"
                     "  Video memory in kilobytes:                   %d\n"
                     "  Optimal bpp mode:                            %d\n"
                     "SDL video driver:                              %s\n",
                         videoInfo->hw_available ? "yes" : "no",
                         videoInfo->wm_available ? "yes" : "no",
                         videoInfo->blit_hw ? "yes" : "no",
                         videoInfo->blit_hw_CC ? "yes" : "no",
                         videoInfo->blit_hw_A ? "yes" : "no",
                         videoInfo->blit_sw ? "yes" : "no",
                         videoInfo->blit_sw_CC ? "yes" : "no",
                         videoInfo->blit_sw_A ? "yes" : "no",
                         videoInfo->blit_fill ? "yes" : "no",
                         videoInfo->video_mem,
                         videoInfo->vfmt->BitsPerPixel,
                         RTEnvGet("SDL_VIDEODRIVER"));
    }

    if (12320 == g_cbIco64x01)
    {
        mWMIcon = SDL_AllocSurface(SDL_SWSURFACE, 64, 64, 24, 0xff, 0xff00, 0xff0000, 0);
        /** @todo make it as simple as possible. No PNM interpreter here... */
        if (mWMIcon)
        {
            memcpy(mWMIcon->pixels, g_abIco64x01+32, g_cbIco64x01-32);
            SDL_WM_SetIcon(mWMIcon, NULL);
        }
    }

    resizeGuest();
    Assert(mScreen);
    mfInitialized = true;
}

VBoxSDLFB::~VBoxSDLFB()
{
    LogFlow(("VBoxSDLFB::~VBoxSDLFB\n"));
    RTCritSectDelete(&mUpdateLock);
}


/**
 * Returns the current framebuffer width in pixels.
 *
 * @returns COM status code
 * @param   width Address of result buffer.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(Width)(ULONG *width)
{
    LogFlow(("VBoxSDLFB::GetWidth\n"));
    if (!width)
        return E_INVALIDARG;
    *width = mGuestXRes;
    return S_OK;
}

/**
 * Returns the current framebuffer height in pixels.
 *
 * @returns COM status code
 * @param   height Address of result buffer.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(Height)(ULONG *height)
{
    LogFlow(("VBoxSDLFB::GetHeight\n"));
    if (!height)
        return E_INVALIDARG;
    *height = mGuestYRes;
    return S_OK;
}

/**
 * Lock the framebuffer (make its address immutable).
 *
 * @returns COM status code
 */
STDMETHODIMP VBoxSDLFB::Lock()
{
    LogFlow(("VBoxSDLFB::Lock\n"));
    RTCritSectEnter(&mUpdateLock);
    return S_OK;
}

/**
 * Unlock the framebuffer.
 *
 * @returns COM status code
 */
STDMETHODIMP VBoxSDLFB::Unlock()
{
    LogFlow(("VBoxSDLFB::Unlock\n"));
    RTCritSectLeave(&mUpdateLock);
    return S_OK;
}

/**
 * Return the framebuffer start address.
 *
 * @returns COM status code.
 * @param   address Pointer to result variable.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(Address)(BYTE **address)
{
    LogFlow(("VBoxSDLFB::GetAddress\n"));
    if (!address)
        return E_INVALIDARG;

    if (mSurfVRAM)
    {
        *address = (BYTE *) mSurfVRAM->pixels;
    }
    else
    {
        /* That's actually rather bad. */
        AssertMsgFailed(("mSurfVRAM is NULL!\n"));
        return E_FAIL;
    }
    LogFlow(("VBoxSDL::GetAddress returning %p\n", *address));
    return S_OK;
}

/**
 * Return the current framebuffer color depth.
 *
 * @returns COM status code
 * @param   bitsPerPixel Address of result variable
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(BitsPerPixel)(ULONG *bitsPerPixel)
{
    LogFlow(("VBoxSDLFB::GetBitsPerPixel\n"));
    if (!bitsPerPixel)
        return E_INVALIDARG;
    /* get the information directly from the surface in use */
    Assert(mSurfVRAM);
    *bitsPerPixel = (ULONG)(mSurfVRAM ? mSurfVRAM->format->BitsPerPixel : 0);
    return S_OK;
}

/**
 * Return the current framebuffer line size in bytes.
 *
 * @returns COM status code.
 * @param   lineSize Address of result variable.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(BytesPerLine)(ULONG *bytesPerLine)
{
    LogFlow(("VBoxSDLFB::GetBytesPerLine\n"));
    if (!bytesPerLine)
        return E_INVALIDARG;
    /* get the information directly from the surface */
    Assert(mSurfVRAM);
    *bytesPerLine = (ULONG)(mSurfVRAM ? mSurfVRAM->pitch : 0);
    return S_OK;
}

STDMETHODIMP VBoxSDLFB::COMGETTER(PixelFormat) (ULONG *pixelFormat)
{
    if (!pixelFormat)
        return E_POINTER;
    *pixelFormat = mPixelFormat;
    return S_OK;
}

STDMETHODIMP VBoxSDLFB::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
{
    if (!usesGuestVRAM)
        return E_POINTER;
    *usesGuestVRAM = mUsesGuestVRAM;
    return S_OK;
}

/**
 * Returns by how many pixels the guest should shrink its
 * video mode height values.
 *
 * @returns COM status code.
 * @param   heightReduction Address of result variable.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(HeightReduction)(ULONG *heightReduction)
{
    if (!heightReduction)
        return E_POINTER;
#ifdef VBOX_SECURELABEL
    *heightReduction = mLabelHeight;
#else
    *heightReduction = 0;
#endif
    return S_OK;
}

/**
 * Returns a pointer to an alpha-blended overlay used for displaying status
 * icons above the framebuffer.
 *
 * @returns COM status code.
 * @param   aOverlay The overlay framebuffer.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(Overlay)(IFramebufferOverlay **aOverlay)
{
    if (!aOverlay)
        return E_POINTER;
    /* Not yet implemented */
    *aOverlay = 0;
    return S_OK;
}

/**
 * Returns handle of window where framebuffer context is being drawn
 *
 * @returns COM status code.
 * @param   winId Handle of associated window.
 */
STDMETHODIMP VBoxSDLFB::COMGETTER(WinId)(uint64_t *winId)
{
    if (!winId)
        return E_POINTER;
    *winId = mWinId;
    return S_OK;
}

/**
 * Notify framebuffer of an update.
 *
 * @returns COM status code
 * @param   x        Update region upper left corner x value.
 * @param   y        Update region upper left corner y value.
 * @param   w        Update region width in pixels.
 * @param   h        Update region height in pixels.
 * @param   finished Address of output flag whether the update
 *                   could be fully processed in this call (which
 *                   has to return immediately) or VBox should wait
 *                   for a call to the update complete API before
 *                   continuing with display updates.
 */
STDMETHODIMP VBoxSDLFB::NotifyUpdate(ULONG x, ULONG y,
                                     ULONG w, ULONG h)
{
    /*
     * The input values are in guest screen coordinates.
     */
    LogFlow(("VBoxSDLFB::NotifyUpdate: x = %d, y = %d, w = %d, h = %d\n",
             x, y, w, h));

#ifdef VBOXSDL_WITH_X11
    /*
     * SDL does not allow us to make this call from any other thread than
     * the main SDL thread (which initialized the video mode). So we have
     * to send an event to the main SDL thread and process it there. For
     * sake of simplicity, we encode all information in the event parameters.
     */
    SDL_Event event;
    event.type       = SDL_USEREVENT;
    event.user.type  = SDL_USER_EVENT_UPDATERECT;
    // 16 bit is enough for coordinates
    event.user.data1 = (void*)(x << 16 | y);
    event.user.data2 = (void*)(w << 16 | h);
    PushNotifyUpdateEvent(&event);
#else /* !VBOXSDL_WITH_X11 */
    update(x, y, w, h, true /* fGuestRelative */);
#endif /* !VBOXSDL_WITH_X11 */

    return S_OK;
}

/**
 * Request a display resize from the framebuffer.
 *
 * @returns COM status code.
 * @param   pixelFormat The requested pixel format.
 * @param   vram        Pointer to the guest VRAM buffer (can be NULL).
 * @param   bitsPerPixel Color depth in bits.
 * @param   bytesPerLine Size of a scanline in bytes.
 * @param   w           New display width in pixels.
 * @param   h           New display height in pixels.
 * @param   finished    Address of output flag whether the update
 *                      could be fully processed in this call (which
 *                      has to return immediately) or VBox should wait
 *                      for all call to the resize complete API before
 *                      continuing with display updates.
 */
STDMETHODIMP VBoxSDLFB::RequestResize(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                                      ULONG bitsPerPixel, ULONG bytesPerLine,
                                      ULONG w, ULONG h, BOOL *finished)
{
    LogFlowFunc (("w=%d, h=%d, pixelFormat=0x%08lX, vram=%p, "
                  "bpp=%d, bpl=%d\n",
                  w, h, pixelFormat, vram, bitsPerPixel, bytesPerLine));

    /*
     * SDL does not allow us to make this call from any other thread than
     * the main thread (the one which initialized the video mode). So we
     * have to send an event to the main SDL thread and tell VBox to wait.
     */
    if (!finished)
    {
        AssertMsgFailed(("RequestResize requires the finished flag!\n"));
        return E_FAIL;
    }

    /*
     * Optimize the case when the guest has changed only the VRAM ptr
     * and the framebuffer uses the guest VRAM as the source bitmap.
     */
    if (   mGuestXRes    == w
        && mGuestYRes    == h
        && mPixelFormat  == pixelFormat
        && mBitsPerPixel == bitsPerPixel
        && mBytesPerLine == bytesPerLine
        && mUsesGuestVRAM
       )
    {
        mfSameSizeRequested = true;
    }
    else
    {
        mfSameSizeRequested = false;
    }

    mGuestXRes   = w;
    mGuestYRes   = h;
    mPixelFormat = pixelFormat;
    mPtrVRAM     = vram;
    mBitsPerPixel = bitsPerPixel;
    mBytesPerLine = bytesPerLine;
    mUsesGuestVRAM = FALSE; /* yet */

    SDL_Event event;
    event.type       = SDL_USEREVENT;
    event.user.type  = SDL_USER_EVENT_RESIZE;

    /* Try multiple times if necessary */
    PushSDLEventForSure(&event);

    /* we want this request to be processed quickly, so yield the CPU */
    RTThreadYield();

    *finished = false;

    return S_OK;
}

/**
 * Returns whether we like the given video mode.
 *
 * @returns COM status code
 * @param   width     video mode width in pixels
 * @param   height    video mode height in pixels
 * @param   bpp       video mode bit depth in bits per pixel
 * @param   supported pointer to result variable
 */
STDMETHODIMP VBoxSDLFB::VideoModeSupported(ULONG width, ULONG height, ULONG bpp, BOOL *supported)
{
    if (!supported)
        return E_POINTER;

    /* are constraints set? */
    if (   (   (mMaxScreenWidth != ~(uint32_t)0)
            && (width > mMaxScreenWidth))
        || (   (mMaxScreenHeight != ~(uint32_t)0)
            && (height > mMaxScreenHeight)))
    {
        /* nope, we don't want that (but still don't freak out if it is set) */
#ifdef DEBUG
        printf("VBoxSDL::VideoModeSupported: we refused mode %dx%dx%d\n", width, height, bpp);
#endif
        *supported = false;
    }
    else
    {
        /* anything will do */
        *supported = true;
    }
    return S_OK;
}

STDMETHODIMP VBoxSDLFB::GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
                                         ULONG *aCountCopied)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

	NOREF(aCount);
	NOREF(aCountCopied);

    return S_OK;
}

STDMETHODIMP VBoxSDLFB::SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
{
    PRTRECT rects = (PRTRECT)aRectangles;

    if (!rects)
        return E_POINTER;

    /// @todo

	NOREF(aCount);

    return S_OK;
}

//
// Internal public methods
//

/**
 * Method that does the actual resize of the guest framebuffer and
 * then changes the SDL framebuffer setup.
 */
void VBoxSDLFB::resizeGuest()
{
    LogFlowFunc (("mGuestXRes: %d, mGuestYRes: %d\n", mGuestXRes, mGuestYRes));
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(),
              ("Wrong thread! SDL is not threadsafe!\n"));

    uint32_t Rmask, Gmask, Bmask, Amask = 0;

    mUsesGuestVRAM = FALSE;

    /* pixel characteristics. if we don't support the format directly, we will
     * fallback to the indirect 32bpp buffer (mUsesGuestVRAM will remain
     * FALSE) */
    if (mPixelFormat == FramebufferPixelFormat_FOURCC_RGB)
    {
        switch (mBitsPerPixel)
        {
            case 16:
            case 24:
            case 32:
                mUsesGuestVRAM = TRUE;
                break;
            default:
                /* the fallback buffer is always 32bpp */
                mBitsPerPixel = 32;
                mBytesPerLine = mGuestXRes * 4;
                break;
        }
    }
    else
    {
        /* the fallback buffer is always RGB, 32bpp */
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
        mBitsPerPixel = 32;
        mBytesPerLine = mGuestXRes * 4;
    }

    switch (mBitsPerPixel)
    {
        case 16: Rmask = 0x0000F800; Gmask = 0x000007E0; Bmask = 0x0000001F; break;
        default: Rmask = 0x00FF0000; Gmask = 0x0000FF00; Bmask = 0x000000FF; break;
    }

    /* first free the current surface */
    if (mSurfVRAM)
    {
        SDL_FreeSurface(mSurfVRAM);
        mSurfVRAM = NULL;
    }

    /* is the guest in a linear framebuffer mode we support? */
    if (mUsesGuestVRAM)
    {

        /* Create a source surface from guest VRAM. */
        mSurfVRAM = SDL_CreateRGBSurfaceFrom(mPtrVRAM, mGuestXRes, mGuestYRes, mBitsPerPixel,
                                             mBytesPerLine, Rmask, Gmask, Bmask, Amask);
    }
    else
    {
        /* Create a software surface for which SDL allocates the RAM */
        mSurfVRAM = SDL_CreateRGBSurface(SDL_SWSURFACE, mGuestXRes, mGuestYRes, mBitsPerPixel,
                                         Rmask, Gmask, Bmask, Amask);
    }
    LogFlow(("VBoxSDL:: created VRAM surface %p\n", mSurfVRAM));

    if (mfSameSizeRequested && mUsesGuestVRAM)
    {
        /*
         * Same size has been requested and the framebuffer still uses the guest VRAM.
         * Reset the condition and return.
         */
        mfSameSizeRequested = false;
        LogFlow(("VBoxSDL:: the same resolution requested, skipping the resize.\n"));
        return;
    }

    /* now adjust the SDL resolution */
    resizeSDL();
}

/**
 * Sets SDL video mode. This is independent from guest video
 * mode changes.
 *
 * @remarks Must be called from the SDL thread!
 */
void VBoxSDLFB::resizeSDL(void)
{
    LogFlow(("VBoxSDL:resizeSDL\n"));

    /*
     * We request a hardware surface from SDL so that we can perform
     * accelerated system memory to VRAM blits. The way video handling
     * works it that on the one hand we have the screen surface from SDL
     * and on the other hand we have a software surface that we create
     * using guest VRAM memory for linear modes and using SDL allocated
     * system memory for text and non linear graphics modes. We never
     * directly write to the screen surface but always use SDL blitting
     * functions to blit from our system memory surface to the VRAM.
     * Therefore, SDL can take advantage of hardware acceleration.
     */
    int sdlFlags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
#ifndef RT_OS_OS2 /* doesn't seem to work for some reason... */
    if (mfResizable)
        sdlFlags |= SDL_RESIZABLE;
#endif
    if (mfFullscreen)
        sdlFlags |= SDL_FULLSCREEN;

    /*
     * Now we have to check whether there are video mode restrictions
     */
    SDL_Rect **modes;
    /* Get available fullscreen/hardware modes */
    modes = SDL_ListModes(NULL, sdlFlags);
    Assert(modes != NULL);
    /* -1 means that any mode is possible (usually non fullscreen) */
    if (modes != (SDL_Rect **)-1)
    {
        /*
         * according to the SDL documentation, the API guarantees that
         * the modes are sorted from larger to smaller, so we just
         * take the first entry as the maximum.
         */
        mMaxScreenWidth  = modes[0]->w;
        mMaxScreenHeight = modes[0]->h;
    }
    else
    {
        /* no restriction */
        mMaxScreenWidth  = ~(uint32_t)0;
        mMaxScreenHeight = ~(uint32_t)0;
    }

    uint32_t newWidth;
    uint32_t newHeight;

    /* reset the centering offsets */
    mCenterXOffset = 0;
    mCenterYOffset = 0;

    /* we either have a fixed SDL resolution or we take the guest's */
    if (mFixedSDLWidth != ~(uint32_t)0)
    {
        newWidth  = mFixedSDLWidth;
        newHeight = mFixedSDLHeight;
    }
    else
    {
        newWidth  = RT_MIN(mGuestXRes, mMaxScreenWidth);
#ifdef VBOX_SECURELABEL
        newHeight = RT_MIN(mGuestYRes + mLabelHeight, mMaxScreenHeight);
#else
        newHeight = RT_MIN(mGuestYRes, mMaxScreenHeight);
#endif
    }

    /* we don't have any extra space by default */
    mTopOffset = 0;

    /*
     * Now set the screen resolution and get the surface pointer
     * @todo BPP is not supported!
     */
    mScreen = SDL_SetVideoMode(newWidth, newHeight, 0, sdlFlags);
#ifdef VBOX_SECURELABEL
    /*
     * For non fixed SDL resolution, the above call tried to add the label height
     * to the guest height. If it worked, we have an offset. If it didn't the below
     * code will try again with the original guest resolution.
     */
    if (mFixedSDLWidth == ~(uint32_t)0)
    {
        /* if it didn't work, then we have to go for the original resolution and paint over the guest */
        if (!mScreen)
        {
            mScreen = SDL_SetVideoMode(newWidth, newHeight - mLabelHeight, 0, sdlFlags);
        }
        else
        {
            /* we now have some extra space */
            mTopOffset = mLabelHeight;
        }
    }
    else
    {
        /* in case the guest resolution is small enough, we do have a top offset */
        if (mFixedSDLHeight - mGuestYRes >= mLabelHeight)
            mTopOffset = mLabelHeight;

        /* we also might have to center the guest picture */
        if (mFixedSDLWidth > mGuestXRes)
            mCenterXOffset = (mFixedSDLWidth - mGuestXRes) / 2;
        if (mFixedSDLHeight > mGuestYRes + mLabelHeight)
            mCenterYOffset = (mFixedSDLHeight - (mGuestYRes + mLabelHeight)) / 2;
    }
#endif
    AssertMsg(mScreen, ("Error: SDL_SetVideoMode failed!\n"));
    if (mScreen)
    {
#ifdef VBOX_WIN32_UI
        /* inform the UI code */
        resizeUI(mScreen->w, mScreen->h);
#endif
        if (mfShowSDLConfig)
            RTPrintf("Resized to %dx%d, screen surface type: %s\n", mScreen->w, mScreen->h,
                     ((mScreen->flags & SDL_HWSURFACE) == 0) ? "software" : "hardware");
    }
    repaint();
}

/**
 * Update specified framebuffer area. The coordinates can either be
 * relative to the guest framebuffer or relative to the screen.
 *
 * @remarks Must be called from the SDL thread on Linux!
 * @param   x              left column
 * @param   y              top row
 * @param   w              width in pixels
 * @param   h              height in pixels
 * @param   fGuestRelative flag whether the above values are guest relative or screen relative;
 */
void VBoxSDLFB::update(int x, int y, int w, int h, bool fGuestRelative)
{
#ifdef VBOXSDL_WITH_X11
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
#endif
    Assert(mScreen);
    Assert(mSurfVRAM);
    if (!mScreen || !mSurfVRAM)
        return;

    /* the source and destination rectangles */
    SDL_Rect srcRect;
    SDL_Rect dstRect;

    /* this is how many pixels we have to cut off from the height for this specific blit */
    int yCutoffGuest = 0;

#ifdef VBOX_SECURELABEL
    bool fPaintLabel = false;
    /* if we have a label and no space for it, we have to cut off a bit */
    if (mLabelHeight && !mTopOffset)
    {
        if (y < (int)mLabelHeight)
            yCutoffGuest = mLabelHeight - y;
    }
#endif

    /**
     * If we get a SDL window relative update, we
     * just perform a full screen update to keep things simple.
     *
     * @todo improve
     */
    if (!fGuestRelative)
    {
#ifdef VBOX_SECURELABEL
        /* repaint the label if necessary */
        if (y < (int)mLabelHeight)
            fPaintLabel = true;
#endif
        x = 0;
        w = mGuestXRes;
        y = 0;
        h = mGuestYRes;
    }

    srcRect.x = x;
    srcRect.y = y + yCutoffGuest;
    srcRect.w = w;
    srcRect.h = RT_MAX(0, h - yCutoffGuest);

    /*
     * Destination rectangle is just offset by the label height.
     * There are two cases though: label height is added to the
     * guest resolution (mTopOffset == mLabelHeight; yCutoffGuest == 0)
     * or the label cuts off a portion of the guest screen (mTopOffset == 0;
     * yCutoffGuest >= 0)
     */
    dstRect.x = x + mCenterXOffset;
#ifdef VBOX_SECURELABEL
    dstRect.y = RT_MAX(mLabelHeight, y + yCutoffGuest + mTopOffset) + mCenterYOffset;
#else
    dstRect.y = y + yCutoffGuest + mTopOffset + mCenterYOffset;
#endif
    dstRect.w = w;
    dstRect.h = RT_MAX(0, h - yCutoffGuest);

    //RTPrintf("y = %d h = %d mapped to srcY %d srcH %d mapped to dstY = %d dstH %d (guestrel: %d, mLabelHeight: %d, mTopOffset: %d)\n",
    //         y, h, srcRect.y, srcRect.h, dstRect.y, dstRect.h, fGuestRelative, mLabelHeight, mTopOffset);

    /*
     * Now we just blit
     */
    SDL_BlitSurface(mSurfVRAM, &srcRect, mScreen, &dstRect);
    /* hardware surfaces don't need update notifications */
    if ((mScreen->flags & SDL_HWSURFACE) == 0)
        SDL_UpdateRect(mScreen, dstRect.x, dstRect.y, dstRect.w, dstRect.h);

#ifdef VBOX_SECURELABEL
    if (fPaintLabel)
        paintSecureLabel(0, 0, 0, 0, false);
#endif
}

/**
 * Repaint the whole framebuffer
 *
 * @remarks Must be called from the SDL thread!
 */
void VBoxSDLFB::repaint()
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    LogFlow(("VBoxSDLFB::repaint\n"));
    update(0, 0, mScreen->w, mScreen->h, false /* fGuestRelative */);
}

bool VBoxSDLFB::getFullscreen()
{
    LogFlow(("VBoxSDLFB::getFullscreen\n"));
    return mfFullscreen;
}

/**
 * Toggle fullscreen mode
 *
 * @remarks Must be called from the SDL thread!
 */
void VBoxSDLFB::setFullscreen(bool fFullscreen)
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    LogFlow(("VBoxSDLFB::SetFullscreen: fullscreen: %d\n", fFullscreen));
    mfFullscreen = fFullscreen;
    /* only change the SDL resolution, do not touch the guest framebuffer */
    resizeSDL();
}

/**
 * Return the geometry of the host. This isn't very well tested but it seems
 * to work at least on Linux hosts.
 */
void VBoxSDLFB::getFullscreenGeometry(uint32_t *width, uint32_t *height)
{
    SDL_Rect **modes;

    /* Get available fullscreen/hardware modes */
    modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
    Assert(modes != NULL);
    /* -1 means that any mode is possible (usually non fullscreen) */
    if (modes != (SDL_Rect **)-1)
    {
        /*
         * According to the SDL documentation, the API guarantees that the modes
         * are sorted from larger to smaller, so we just take the first entry as
         * the maximum.
         *
         * XXX Crude Xinerama hack :-/
         */
        if (   modes[0]->w > (16*modes[0]->h/9)
            && modes[1]
            && modes[1]->h == modes[0]->h)
        {
            *width  = modes[1]->w;
            *height = modes[1]->h;
        }
        else
        {
            *width  = modes[0]->w;
            *height = modes[0]->w;
        }
    }
}

/**
 * Returns the current x offset of the start of the guest screen
 *
 * @returns current x offset in pixels
 */
int VBoxSDLFB::getXOffset()
{
    /* there can only be an offset for centering */
    return mCenterXOffset;
}

/**
 * Returns the current y offset of the start of the guest screen
 *
 * @returns current y offset in pixels
 */
int VBoxSDLFB::getYOffset()
{
    /* we might have a top offset and a center offset */
    return mTopOffset + mCenterYOffset;
}

#ifdef VBOX_SECURELABEL
/**
 * Setup the secure labeling parameters
 *
 * @returns         VBox status code
 * @param height    height of the secure label area in pixels
 * @param font      file path fo the TrueType font file
 * @param pointsize font size in points
 */
int VBoxSDLFB::initSecureLabel(uint32_t height, char *font, uint32_t pointsize, uint32_t labeloffs)
{
    LogFlow(("VBoxSDLFB:initSecureLabel: new offset: %d pixels, new font: %s, new pointsize: %d\n",
              height, font, pointsize));
    mLabelHeight = height;
    mLabelOffs = labeloffs;
    Assert(font);
    pTTF_Init();
    mLabelFont = pTTF_OpenFont(font, pointsize);
    if (!mLabelFont)
    {
        AssertMsgFailed(("Failed to open TTF font file %s\n", font));
        return VERR_OPEN_FAILED;
    }
    mSecureLabelColorFG = 0x0000FF00;
    mSecureLabelColorBG = 0x00FFFF00;
    repaint();
    return VINF_SUCCESS;
}

/**
 * Set the secure label text and repaint the label
 *
 * @param   text UTF-8 string of new label
 * @remarks must be called from the SDL thread!
 */
void VBoxSDLFB::setSecureLabelText(const char *text)
{
    mSecureLabelText = text;
    paintSecureLabel(0, 0, 0, 0, true);
}

/**
 * Sets the secure label background color.
 *
 * @param   colorFG encoded RGB value for text
 * @param   colorBG encored RGB value for background
 * @remarks must be called from the SDL thread!
 */
void VBoxSDLFB::setSecureLabelColor(uint32_t colorFG, uint32_t colorBG)
{
    mSecureLabelColorFG = colorFG;
    mSecureLabelColorBG = colorBG;
    paintSecureLabel(0, 0, 0, 0, true);
}

/**
 * Paint the secure label if required
 *
 * @param   fForce Force the repaint
 * @remarks must be called from the SDL thread!
 */
void VBoxSDLFB::paintSecureLabel(int x, int y, int w, int h, bool fForce)
{
#ifdef VBOXSDL_WITH_X11
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
#endif
    /* only when the function is present */
    if (!pTTF_RenderUTF8_Solid)
        return;
    /* check if we can skip the paint */
    if (!fForce && ((uint32_t)y > mLabelHeight))
    {
        return;
    }
    /* first fill the background */
    SDL_Rect rect = {0, 0, (Uint16)mScreen->w, (Uint16)mLabelHeight};
    SDL_FillRect(mScreen, &rect, SDL_MapRGB(mScreen->format,
                                            (mSecureLabelColorBG & 0x00FF0000) >> 16,   /* red   */
                                            (mSecureLabelColorBG & 0x0000FF00) >> 8,   /* green */
                                            mSecureLabelColorBG & 0x000000FF)); /* blue  */

    /* now the text */
    if (mLabelFont != NULL && mSecureLabelText)
    {
        SDL_Color clrFg = {(mSecureLabelColorFG & 0x00FF0000) >> 16,
                           (mSecureLabelColorFG & 0x0000FF00) >> 8,
                           mSecureLabelColorFG & 0x000000FF, 0};
        SDL_Surface *sText = (pTTF_RenderUTF8_Blended != NULL)
                                 ? pTTF_RenderUTF8_Blended(mLabelFont, mSecureLabelText.raw(), clrFg)
                                 : pTTF_RenderUTF8_Solid(mLabelFont, mSecureLabelText.raw(), clrFg);
        rect.x = 10;
        rect.y = mLabelOffs;
        SDL_BlitSurface(sText, NULL, mScreen, &rect);
        SDL_FreeSurface(sText);
    }
    /* make sure to update the screen */
    SDL_UpdateRect(mScreen, 0, 0, mScreen->w, mLabelHeight);
}
#endif /* VBOX_SECURELABEL */

/**
 * Terminate SDL
 *
 * @remarks must be called from the SDL thread!
 */
void VBoxSDLFB::uninit()
{
    AssertMsg(mSdlNativeThread == RTThreadNativeSelf(), ("Wrong thread! SDL is not threadsafe!\n"));
    if (mSurfVRAM)
    {
        SDL_FreeSurface(mSurfVRAM);
        mSurfVRAM = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#ifdef VBOX_SECURELABEL
    if (mLabelFont)
        pTTF_CloseFont(mLabelFont);
    if (pTTF_Quit)
        pTTF_Quit();
#endif
    mScreen = NULL;
    if (mWMIcon)
    {
        SDL_FreeSurface(mWMIcon);
        mWMIcon = NULL;
    }
}

// IFramebufferOverlay
///////////////////////////////////////////////////////////////////////////////////

/**
 * Constructor for the VBoxSDLFBOverlay class (IFramebufferOverlay implementation)
 *
 * @param x       Initial X offset for the overlay
 * @param y       Initial Y offset for the overlay
 * @param width   Initial width for the overlay
 * @param height  Initial height for the overlay
 * @param visible Whether the overlay is initially visible
 * @param alpha   Initial alpha channel value for the overlay
 */
VBoxSDLFBOverlay::VBoxSDLFBOverlay(ULONG x, ULONG y, ULONG width, ULONG height,
                                   BOOL visible, VBoxSDLFB *aParent) :
                                   mOverlayX(x), mOverlayY(y), mOverlayWidth(width),
                                   mOverlayHeight(height), mOverlayVisible(visible),
                                   mParent(aParent)
{}

/**
 * Destructor for the VBoxSDLFBOverlay class.
 */
VBoxSDLFBOverlay::~VBoxSDLFBOverlay()
{
    SDL_FreeSurface(mBlendedBits);
    SDL_FreeSurface(mOverlayBits);
}

/**
 * Perform any initialisation of the overlay that can potentially fail
 *
 * @returns S_OK on success or the reason for the failure
 */
HRESULT VBoxSDLFBOverlay::init()
{
    mBlendedBits = SDL_CreateRGBSurface(SDL_ANYFORMAT, mOverlayWidth, mOverlayHeight, 32,
                                        0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    AssertMsgReturn(mBlendedBits != NULL, ("Failed to create an SDL surface\n"),
                    E_OUTOFMEMORY);
    mOverlayBits = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, mOverlayWidth,
                                        mOverlayHeight, 32, 0x00ff0000, 0x0000ff00,
                                        0x000000ff, 0xff000000);
    AssertMsgReturn(mOverlayBits != NULL, ("Failed to create an SDL surface\n"),
                    E_OUTOFMEMORY);
    return S_OK;
}

/**
 * Returns the current overlay X offset in pixels.
 *
 * @returns COM status code
 * @param   x Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(X)(ULONG *x)
{
    LogFlow(("VBoxSDLFBOverlay::GetX\n"));
    if (!x)
        return E_INVALIDARG;
    *x = mOverlayX;
    return S_OK;
}

/**
 * Returns the current overlay height in pixels.
 *
 * @returns COM status code
 * @param   height Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Y)(ULONG *y)
{
    LogFlow(("VBoxSDLFBOverlay::GetY\n"));
    if (!y)
        return E_INVALIDARG;
    *y = mOverlayY;
    return S_OK;
}

/**
 * Returns the current overlay width in pixels.  In fact, this returns the line size.
 *
 * @returns COM status code
 * @param   width Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Width)(ULONG *width)
{
    LogFlow(("VBoxSDLFBOverlay::GetWidth\n"));
    if (!width)
        return E_INVALIDARG;
    *width = mOverlayBits->pitch;
    return S_OK;
}

/**
 * Returns the current overlay line size in pixels.
 *
 * @returns COM status code
 * @param   lineSize Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(BytesPerLine)(ULONG *bytesPerLine)
{
    LogFlow(("VBoxSDLFBOverlay::GetBytesPerLine\n"));
    if (!bytesPerLine)
        return E_INVALIDARG;
    *bytesPerLine = mOverlayBits->pitch;
    return S_OK;
}

/**
 * Returns the current overlay height in pixels.
 *
 * @returns COM status code
 * @param   height Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Height)(ULONG *height)
{
    LogFlow(("VBoxSDLFBOverlay::GetHeight\n"));
    if (!height)
        return E_INVALIDARG;
    *height = mOverlayHeight;
    return S_OK;
}

/**
 * Returns whether the overlay is currently visible.
 *
 * @returns COM status code
 * @param   visible Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Visible)(BOOL *visible)
{
    LogFlow(("VBoxSDLFBOverlay::GetVisible\n"));
    if (!visible)
        return E_INVALIDARG;
    *visible = mOverlayVisible;
    return S_OK;
}

/**
 * Sets whether the overlay is currently visible.
 *
 * @returns COM status code
 * @param   visible New value.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMSETTER(Visible)(BOOL visible)
{
    LogFlow(("VBoxSDLFBOverlay::SetVisible\n"));
    mOverlayVisible = visible;
    return S_OK;
}

/**
 * Returns the value of the global alpha channel.
 *
 * @returns COM status code
 * @param   alpha Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Alpha)(ULONG *alpha)
{
    LogFlow(("VBoxSDLFBOverlay::GetAlpha\n"));
    return E_NOTIMPL;
}

/**
 * Sets whether the overlay is currently visible.
 *
 * @returns COM status code
 * @param   alpha new value.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMSETTER(Alpha)(ULONG alpha)
{
    LogFlow(("VBoxSDLFBOverlay::SetAlpha\n"));
    return E_NOTIMPL;
}

/**
 * Returns the address of the framebuffer bits for writing to.
 *
 * @returns COM status code
 * @param   alpha Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Address)(ULONG *address)
{
    LogFlow(("VBoxSDLFBOverlay::GetAddress\n"));
    if (!address)
        return E_INVALIDARG;
    *address = (uintptr_t) mOverlayBits->pixels;
    return S_OK;
}

/**
 * Returns the current colour depth.  In fact, this is always 32bpp.
 *
 * @returns COM status code
 * @param   bitsPerPixel Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(BitsPerPixel)(ULONG *bitsPerPixel)
{
    LogFlow(("VBoxSDLFBOverlay::GetBitsPerPixel\n"));
    if (!bitsPerPixel)
        return E_INVALIDARG;
    *bitsPerPixel = 32;
    return S_OK;
}

/**
 * Returns the current pixel format.  In fact, this is always RGB.
 *
 * @returns COM status code
 * @param   pixelFormat Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(PixelFormat)(ULONG *pixelFormat)
{
    LogFlow(("VBoxSDLFBOverlay::GetPixelFormat\n"));
    if (!pixelFormat)
        return E_INVALIDARG;
    *pixelFormat = FramebufferPixelFormat_FOURCC_RGB;
    return S_OK;
}

/**
 * Returns whether the guest VRAM is used directly.  In fact, this is always FALSE.
 *
 * @returns COM status code
 * @param   usesGuestVRAM Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(UsesGuestVRAM)(BOOL *usesGuestVRAM)
{
    LogFlow(("VBoxSDLFBOverlay::GetUsesGuestVRAM\n"));
    if (!usesGuestVRAM)
        return E_INVALIDARG;
    *usesGuestVRAM = FALSE;
    return S_OK;
}

/**
 * Returns the height reduction.  In fact, this is always 0.
 *
 * @returns COM status code
 * @param   heightReduction Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(HeightReduction)(ULONG *heightReduction)
{
    LogFlow(("VBoxSDLFBOverlay::GetHeightReduction\n"));
    if (!heightReduction)
        return E_INVALIDARG;
    *heightReduction = 0;
    return S_OK;
}

/**
 * Returns the overlay for this framebuffer.  Obviously, we return NULL here.
 *
 * @returns COM status code
 * @param   overlay Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(Overlay)(IFramebufferOverlay **aOverlay)
{
    LogFlow(("VBoxSDLFBOverlay::GetOverlay\n"));
    if (!aOverlay)
        return E_INVALIDARG;
    *aOverlay = 0;
    return S_OK;
}

/**
 * Returns associated window handle. We return NULL here.
 *
 * @returns COM status code
 * @param   winId Address of result buffer.
 */
STDMETHODIMP VBoxSDLFBOverlay::COMGETTER(WinId)(ULONG64 *winId)
{
    LogFlow(("VBoxSDLFBOverlay::GetWinId\n"));
    if (!winId)
        return E_INVALIDARG;
    *winId = 0;
    return S_OK;
}


/**
 * Lock the overlay.  This should not be used - lock the parent IFramebuffer instead.
 *
 * @returns COM status code
 */
STDMETHODIMP VBoxSDLFBOverlay::Lock()
{
    LogFlow(("VBoxSDLFBOverlay::Lock\n"));
    AssertMsgFailed(("You should not attempt to lock an IFramebufferOverlay object -\n"
                     "lock the parent IFramebuffer object instead.\n"));
    return E_NOTIMPL;
}

/**
 * Unlock the overlay.
 *
 * @returns COM status code
 */
STDMETHODIMP VBoxSDLFBOverlay::Unlock()
{
    LogFlow(("VBoxSDLFBOverlay::Unlock\n"));
    AssertMsgFailed(("You should not attempt to lock an IFramebufferOverlay object -\n"
                     "lock the parent IFramebuffer object instead.\n"));
    return E_NOTIMPL;
}

/**
 * Change the X and Y co-ordinates of the overlay area.
 *
 * @returns COM status code
 * @param   x New X co-ordinate.
 * @param   y New Y co-ordinate.
 */
STDMETHODIMP VBoxSDLFBOverlay::Move(ULONG x, ULONG y)
{
    mOverlayX = x;
    mOverlayY = y;
    return S_OK;
}

/**
 * Notify the overlay that a section of the framebuffer has been redrawn.
 *
 * @returns COM status code
 * @param   x        X co-ordinate of upper left corner of modified area.
 * @param   y        Y co-ordinate of upper left corner of modified area.
 * @param   w        Width of modified area.
 * @param   h        Height of modified area.
 * @retval  finished Set if the operation has completed.
 *
 * All we do here is to send a request to the parent to update the affected area,
 * translating between our co-ordinate system and the parent's.  It would be have
 * been better to call the parent directly, but such is life.  We leave bounds
 * checking to the parent.
 */
STDMETHODIMP VBoxSDLFBOverlay::NotifyUpdate(ULONG x, ULONG y,
                            ULONG w, ULONG h)
{
    return mParent->NotifyUpdate(x + mOverlayX, y + mOverlayY, w, h);
}

/**
 * Change the dimensions of the overlay.
 *
 * @returns COM status code
 * @param   pixelFormat Must be FramebufferPixelFormat_PixelFormatRGB32.
 * @param   vram        Must be NULL.
 * @param   lineSize    Ignored.
 * @param   w           New overlay width.
 * @param   h           New overlay height.
 * @retval  finished    Set if the operation has completed.
 */
STDMETHODIMP VBoxSDLFBOverlay::RequestResize(ULONG aScreenId, ULONG pixelFormat, ULONG vram,
                                             ULONG bitsPerPixel, ULONG bytesPerLine,
                                             ULONG w, ULONG h, BOOL *finished)
{
    AssertReturn(pixelFormat == FramebufferPixelFormat_FOURCC_RGB, E_INVALIDARG);
    AssertReturn(vram == 0, E_INVALIDARG);
    AssertReturn(bitsPerPixel == 32, E_INVALIDARG);
    mOverlayWidth = w;
    mOverlayHeight = h;
    SDL_FreeSurface(mOverlayBits);
    mBlendedBits = SDL_CreateRGBSurface(SDL_ANYFORMAT, mOverlayWidth, mOverlayHeight, 32,
                                        0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    AssertMsgReturn(mBlendedBits != NULL, ("Failed to create an SDL surface\n"),
                    E_OUTOFMEMORY);
    mOverlayBits = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, mOverlayWidth,
                                        mOverlayHeight, 32, 0x00ff0000, 0x0000ff00,
                                        0x000000ff, 0xff000000);
    AssertMsgReturn(mOverlayBits != NULL, ("Failed to create an SDL surface\n"),
                    E_OUTOFMEMORY);
    return S_OK;
}

/**
 * Returns whether we like the given video mode.
 *
 * @returns COM status code
 * @param   width     video mode width in pixels
 * @param   height    video mode height in pixels
 * @param   bpp       video mode bit depth in bits per pixel
 * @retval  supported pointer to result variable
 *
 * Basically, we support anything with 32bpp.
 */
STDMETHODIMP VBoxSDLFBOverlay::VideoModeSupported(ULONG width, ULONG height, ULONG bpp,
                                               BOOL *supported)
{
    if (!supported)
        return E_POINTER;
    if (bpp == 32)
        *supported = true;
    else
        *supported = false;
    return S_OK;
}
