/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Declaration of Framebuffer class
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VBOX_INCLUDED_SRC_VBoxBFE_Framebuffer_h
#define VBOX_INCLUDED_SRC_VBoxBFE_Framebuffer_h

#include <VBox/types.h>
#include <iprt/critsect.h>

#include <SDL.h>

/** custom SDL event for display update handling */
#define SDL_USER_EVENT_UPDATERECT         (SDL_USEREVENT + 4)
/** custom SDL event for changing the guest resolution */
#define SDL_USER_EVENT_NOTIFYCHANGE       (SDL_USEREVENT + 5)
/** custom SDL for XPCOM event queue processing */
#define SDL_USER_EVENT_XPCOM_EVENTQUEUE   (SDL_USEREVENT + 6)
/** custom SDL event for updating the titlebar */
#define SDL_USER_EVENT_UPDATE_TITLEBAR    (SDL_USEREVENT + 7)
/** custom SDL user event for terminating the session */
#define SDL_USER_EVENT_TERMINATE          (SDL_USEREVENT + 8)
/** custom SDL user event for pointer shape change request */
#define SDL_USER_EVENT_POINTER_CHANGE     (SDL_USEREVENT + 9)
/** custom SDL user event for a regular timer */
#define SDL_USER_EVENT_TIMER              (SDL_USEREVENT + 10)
/** custom SDL user event for resetting mouse cursor */
#define SDL_USER_EVENT_GUEST_CAP_CHANGED  (SDL_USEREVENT + 11)
/** custom SDL user event for window resize done */
#define SDL_USER_EVENT_WINDOW_RESIZE_DONE (SDL_USEREVENT + 12)


/** The user.code field of the SDL_USER_EVENT_TERMINATE event.
 * @{
 */
/** Normal termination. */
#define VBOXSDL_TERM_NORMAL             0
/** Abnormal termination. */
#define VBOXSDL_TERM_ABEND              1
/** @} */

#if defined(VBOXSDL_WITH_X11) || defined(RT_OS_DARWIN)
void PushNotifyUpdateEvent(SDL_Event *event);
#endif
int  PushSDLEventForSure(SDL_Event *event);

class Display;

class Framebuffer
{
public:
    Framebuffer(Display *pDisplay, uint32_t uScreenId,
                bool fFullscreen, bool fResizable, bool fShowSDLConfig,
                bool fKeepHostRes, uint32_t u32FixedWidth,
                uint32_t u32FixedHeight, uint32_t u32FixedBPP,
                bool fUpdateImage);
    Framebuffer(bool fShowSDLConfig);
    virtual ~Framebuffer();

    static bool init(bool fShowSDLConfig);
    static void uninit();

    uint32_t getWidth();
    uint32_t getHeight();
    uint32_t getBitsPerPixel();
    uint32_t getBytesPerLine();
    uint8_t  *getPixelData();

    int      notifyUpdate(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    int      notifyChange(uint32_t idScreen, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    int      NotifyUpdateImage(uint32_t aX,
                               uint32_t aY,
                               uint32_t aWidth,
                               uint32_t aHeight,
                               void *pvImage);

    // internal public methods
    bool initialized() { return mfInitialized; }
    void notifyChange(uint32_t aScreenId);
    void resizeGuest();
    void resizeSDL();
    void update(int x, int y, int w, int h, bool fGuestRelative);
    void repaint();
    void setFullscreen(bool fFullscreen);
    void getFullscreenGeometry(uint32_t *width, uint32_t *height);
    uint32_t getScreenId() { return mScreenId; }
    uint32_t getGuestXRes() { return mGuestXRes; }
    uint32_t getGuestYRes() { return mGuestYRes; }
    int32_t getOriginX() { return mOriginX; }
    int32_t getOriginY() { return mOriginY; }
    int32_t getXOffset() { return mCenterXOffset; }
    int32_t getYOffset() { return mCenterYOffset; }
    SDL_Window *getWindow() { return mpWindow; }
    bool hasWindow(uint32_t id) { return SDL_GetWindowID(mpWindow) == id; }
    int setWindowTitle(const char *pcszTitle);
    void setWinId(int64_t winId) { mWinId = winId; }
    void setOrigin(int32_t axOrigin, int32_t ayOrigin) { mOriginX = axOrigin; mOriginY = ayOrigin; }
    bool getFullscreen() { return mfFullscreen; }

private:

    /** the SDL window */
    SDL_Window *mpWindow;
    /** the texture */
    SDL_Texture *mpTexture;
    /** renderer */
    SDL_Renderer *mpRenderer;
    /** render info */
    SDL_RendererInfo mRenderInfo;
    /** false if constructor failed */
    bool mfInitialized;
    /** the screen number of this framebuffer */
    uint32_t mScreenId;
    /** use NotifyUpdateImage */
    bool mfUpdateImage;
    /** maximum possible screen width in pixels (~0 = no restriction) */
    uint32_t mMaxScreenWidth;
    /** maximum possible screen height in pixels (~0 = no restriction) */
    uint32_t mMaxScreenHeight;
    /** current guest screen width in pixels */
    uint32_t mGuestXRes;
    /** current guest screen height in pixels */
    uint32_t mGuestYRes;
    int32_t mOriginX;
    int32_t mOriginY;
    /** fixed SDL screen width (~0 = not set) */
    uint32_t mFixedSDLWidth;
    /** fixed SDL screen height (~0 = not set) */
    uint32_t mFixedSDLHeight;
    /** fixed SDL bits per pixel (~0 = not set) */
    uint32_t mFixedSDLBPP;
    /** Y offset in pixels, i.e. guest-nondrawable area at the top */
    uint32_t mTopOffset;
    /** X offset for guest screen centering */
    uint32_t mCenterXOffset;
    /** Y offset for guest screen centering */
    uint32_t mCenterYOffset;
    /** flag whether we're in fullscreen mode */
    bool  mfFullscreen;
    /** flag whether we keep the host screen resolution when switching to
     *  fullscreen or not */
    bool  mfKeepHostRes;
    /** framebuffer update semaphore */
    RTCRITSECT mUpdateLock;
    /** flag whether the SDL window should be resizable */
    bool mfResizable;
    /** flag whether we print out SDL information */
    bool mfShowSDLConfig;
    /** handle to window where framebuffer context is being drawn*/
    int64_t mWinId;
    SDL_Surface *mSurfVRAM;
    bool mfUpdates;

    uint8_t *mPtrVRAM;
    uint32_t mBitsPerPixel;
    uint32_t mBytesPerLine;
    bool mfSameSizeRequested;
    Display *m_pDisplay;
};

#endif /* !VBOX_INCLUDED_SRC_VBoxBFE_Framebuffer_h */
