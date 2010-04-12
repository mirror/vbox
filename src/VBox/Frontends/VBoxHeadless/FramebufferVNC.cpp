/* $Id$ */
/** @file
 * VBoxHeadless - VNC server implementation for VirtualBox.
 *
 * Uses LibVNCServer (http://sourceforge.net/projects/libvncserver/)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Contributed by Ivo Smits <Ivo@UFO-Net.nl>
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

#include "FramebufferVNC.h"

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <VBox/log.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/thread.h>

#include <png.h>
#include <rfb/rfb.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

/**
 * Perform parts of initialisation which are guaranteed not to fail
 * unless we run out of memory.  In this case, we just set the guest
 * buffer to 0 so that RequestResize() does not free it the first time
 * it is called.
 */
VNCFB::VNCFB(ComPtr <IConsole> console, int port, const char *password) :
    mPixelFormat(FramebufferPixelFormat_Opaque),
    mBitsPerPixel(0),
    mBytesPerLine(0),
    mRGBBuffer(0),
    mScreenBuffer(0),
    mVncPort(port),
    mConsole(console),
    mKeyboard(0),
    mMouse(0),
    mWidth(800), mHeight(600),
    vncServer(0),
    mVncPassword(password)
{
    LogFlow(("Creating VNC object %p, width=%u, height=%u, port=%u\n",
             this, mWidth,  mHeight, mVncPort));
}


VNCFB::~VNCFB() {
    LogFlow(("Destroying VNCFB object %p\n", this));
    RTCritSectDelete(&mCritSect);
    if (vncServer) {
    if (vncServer->authPasswdData) {
        char **papszPassword = (char **)vncServer->authPasswdData;
        vncServer->authPasswdData = NULL;
        RTMemFree(papszPassword[0]);
        RTMemFree(papszPassword);
    }
        rfbScreenCleanup(vncServer);
    }
    RTMemFree(mRGBBuffer);
    mRGBBuffer = NULL;
    RTMemFree(mScreenBuffer);
    mScreenBuffer = NULL;
}

HRESULT VNCFB::init() {
    LogFlow(("Initialising VNCFB object %p\n", this));
    int rc = RTCritSectInit(&mCritSect);
    AssertReturn(rc == VINF_SUCCESS, E_UNEXPECTED);

    vncServer = rfbGetScreen(0, NULL, mWidth, mHeight, 8, 3, 1);
    vncServer->screenData = (void*)this;
    if (mVncPort) vncServer->port = mVncPort;
    vncServer->desktopName = "VirtualBox";

    if (mVncPassword) {
    char **papszPasswords = (char **)RTMemAlloc(2 * sizeof(char **));
        papszPasswords[0] = RTStrDup(mVncPassword);
        papszPasswords[1] = NULL;
    vncServer->authPasswdData = papszPasswords;
        vncServer->passwordCheck = rfbCheckPasswordByList; //Password list based authentication function
    } else {
    vncServer->authPasswdData = NULL;
    }

    rfbInitServer(vncServer);
    vncServer->kbdAddEvent = vncKeyboardEvent;
    vncServer->kbdReleaseAllKeys = vncReleaseKeysEvent;
    vncServer->ptrAddEvent = vncMouseEvent;

    /* Set the initial framebuffer size */
    BOOL finished;
    RequestResize(0, FramebufferPixelFormat_Opaque, NULL, 0, 0, mWidth, mHeight, &finished);

    rc = RTThreadCreate(&mVncThread, vncThreadFn, vncServer,
                        0 /*cbStack*/, RTTHREADTYPE_GUI, 0 /*fFlags*/, "VNC");
    AssertRCReturn(rc, E_UNEXPECTED);

    return S_OK;
}

DECLCALLBACK(int) VNCFB::vncThreadFn(RTTHREAD hThreadSelf, void *pvUser)
{
    rfbRunEventLoop((rfbScreenInfoPtr)pvUser, -1, FALSE);
    return VINF_SUCCESS;
}

void VNCFB::vncMouseEvent(int buttonMask, int x, int y, rfbClientPtr cl) {
    ((VNCFB*)(cl->screen->screenData))->handleVncMouseEvent(buttonMask, x, y);
    rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
}

void VNCFB::handleVncMouseEvent(int buttonMask, int x, int y) {
    //RTPrintf("VNC mouse: button=%d x=%d y=%d\n", buttonMask, x, y);
    if (!mMouse) {
        this->mConsole->COMGETTER(Mouse)(mMouse.asOutParam());
        if (!mMouse) {
            RTPrintf("Warning: could not get mouse object!\n");
            return;
        }
    }
    int dz = 0, buttons = 0;
    if (buttonMask & 16) dz = 1; else if (buttonMask & 8) dz = -1;
    if (buttonMask & 1) buttons |= 1;
    if (buttonMask & 2) buttons |= 4;
    if (buttonMask & 4) buttons |= 2;
    mMouse->PutMouseEvent(x - mouseX, y - mouseY, dz, 0, buttons);
    //mMouse->PutMouseEventAbsolute(x + 1, y + 1, dz, 0, buttonMask);
    mouseX = x;
    mouseY = y;
}

void VNCFB::kbdPutCode(int code) {
    mKeyboard->PutScancode(code);
}
void VNCFB::kbdSetShift(int state) {
    if (state && !kbdShiftState) {
        kbdPutCode(0x2a, 1);
        kbdShiftState = 1;
    } else if (!state && kbdShiftState) {
        kbdPutCode(0x2a, 0);
        kbdShiftState = 0;
    }
}
void VNCFB::kbdPutCode(int code, int down) {
    if (code & 0xff00) kbdPutCode((code >> 8) & 0xff);
    kbdPutCode((code & 0xff) | (down ? 0 : 0x80));
}
void VNCFB::kbdPutCodeShift(int shift, int code, int down) {
    if (shift != kbdShiftState) kbdPutCode(0x2a, shift);
    kbdPutCode(code, down);
    if (shift != kbdShiftState) kbdPutCode(0x2a, kbdShiftState);
}

/* Handle VNC keyboard code (X11 compatible?) to AT scancode conversion.
 * Have tried the code from the SDL frontend, but that didn't work.
 * Now we're using one lookup table for the lower X11 key codes (ASCII characters)
 * and a switch() block to handle some special keys. */
void VNCFB::handleVncKeyboardEvent(int down, int keycode) {
    //RTPrintf("VNC keyboard: down=%d code=%d -> ", down, keycode);
    if (mKeyboard == NULL) {
        this->mConsole->COMGETTER(Keyboard)(mKeyboard.asOutParam());
        if (!mKeyboard) {
            RTPrintf("Warning: could not get keyboard object!\n");
            return;
        }
    }
    /* Conversion table for key code range 32-127 (which happen to equal the ASCII codes)
     * The values in the table differ slightly from the actual scancode values that will be sent,
     * values 0xe0?? indicate that a 0xe0 scancode will be sent first (extended keys), then code ?? is sent
     * values 0x01?? indicate that the shift key must be 'down', then ?? is sent
     * values 0x00?? or 0x?? indicate that the shift key must be 'up', then ?? is sent
     * values 0x02?? indicate that the shift key can be ignored, and scancode ?? is sent
     * This is necessary because the VNC protocol sends a shift key sequence, but also
     * sends the 'shifted' version of the characters. */
    static int codes_low[] = { //Conversion table for VNC key code range 32-127
        0x0239, 0x0102, 0x0128, 0x0104, 0x0105, 0x0106, 0x0108, 0x0028, 0x010a, 0x010b, 0x0109, 0x010d, 0x0029, 0x000c, 0x0034, 0x0035, //space, !"#$%&'()*+`-./
        0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, //0123456789
        0x0127, 0x0027, 0x0133, 0x000d, 0x0134, 0x0135, 0x0103, //:;<=>?@
        0x11e, 0x130, 0x12e, 0x120, 0x112, 0x121, 0x122, 0x123, 0x117, 0x124, 0x125, 0x126, 0x132, 0x131, 0x118, 0x119, 0x110, 0x113, 0x11f, 0x114, 0x116, 0x12f, 0x111, 0x12d, 0x115, 0x12c, //A-Z
        0x001a, 0x002b, 0x001b, 0x0107, 0x010c, 0x0029, //[\]^_`
        0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, //a-z
        0x011a, 0x012b, 0x011b, 0x0129 //{|}~
    };
    int shift = -1, code = -1;
    if (keycode < 32) { //ASCII control codes.. unused..
    } else if (keycode < 127) { //DEL is in high area
        code = codes_low[keycode - 32];
        shift = (code >> 8) & 0x03; if (shift == 0x02 || code & 0xe000) shift = -1;
        code = code & 0xe0ff;
    } else if ((keycode & 0xFF00) != 0xFF00) {
    } else {
        switch(keycode) {
/*Numpad keys - these have to be implemented yet
Todo: numpad arrows, home, pageup, pagedown, end, insert, delete
65421 Numpad return

65450 Numpad *
65451 Numpad +
65453 Numpad -
65454 Numpad .
65455 Numpad /
65457 Numpad 1
65458 Numpad 2
65459 Numpad 3

65460 Numpad 4
65461 Numpad 5
65462 Numpad 6
65463 Numpad 7
65464 Numpad 8
65465 Numpad 9
65456 Numpad 0
*/
            case 65288: code =   0x0e; break; //Backspace
            case 65289: code =   0x0f; break; //Tab

            case 65293: code =   0x1c; break; //Return
            //case 65299: break; Pause/break
            case 65307: code =   0x01; break; //Escape

            case 65360: code = 0xe047; break; //Home
            case 65361: code = 0xe04b; break; //Left
            case 65362: code = 0xe048; break; //Up
            case 65363: code = 0xe04d; break; //Right
            case 65364: code = 0xe050; break; //Down
            case 65365: code = 0xe049; break; //Page up
            case 65366: code = 0xe051; break; //Page down
            case 65367: code = 0xe04f; break; //End

            //case 65377: break; //Print screen
            case 65379: code = 0xe052; break; //Insert

            case 65383: code = 0xe05d; break; //Menu

            case 65470: code =   0x3b; break; //F1
            case 65471: code =   0x3c; break; //F2
            case 65472: code =   0x3d; break; //F3
            case 65473: code =   0x3e; break; //F4
            case 65474: code =   0x3f; break; //F5
            case 65475: code =   0x40; break; //F6
            case 65476: code =   0x41; break; //F7
            case 65477: code =   0x42; break; //F8
            case 65478: code =   0x43; break; //F9
            case 65479: code =   0x44; break; //F10
            case 65480: code =   0x57; break; //F11
            case 65481: code =   0x58; break; //F12

            case 65505: shift =  down; break; //Shift (left + right)
            case 65507: code =   0x1d; break; //Left ctrl
            case 65508: code = 0xe01d; break; //Right ctrl
            case 65513: code =   0x38; break; //Left Alt
            case 65514: code = 0xe038; break; //Right Alt
            case 65515: code = 0xe05b; break; //Left windows key
            case 65516: code = 0xe05c; break; //Right windows key
            case 65535: code = 0xe053; break; //Delete
            default: RTPrintf("VNC unhandled keyboard code: down=%d code=%d\n", down, keycode); break;
        }
    }
    //RTPrintf("down=%d shift=%d code=%d\n", down, shift, code);
    if (shift != -1 && code != -1) {
        kbdPutCodeShift(shift, code, down);
    } else if (shift != -1) {
        kbdSetShift(shift);
    } else if (code != -1) {
        kbdPutCode(code, down);
    }
}
void VNCFB::handleVncKeyboardReleaseEvent() {
    kbdSetShift(0);
    kbdPutCode(0x1d, 0); //Left ctrl
    kbdPutCode(0xe01d, 0); //Right ctrl
    kbdPutCode(0x38, 0); //Left alt
    kbdPutCode(0xe038, 0); //Right alt
}

void VNCFB::vncKeyboardEvent(rfbBool down, rfbKeySym keySym, rfbClientPtr cl) {
    ((VNCFB*)(cl->screen->screenData))->handleVncKeyboardEvent(down, keySym);
}
void VNCFB::vncReleaseKeysEvent(rfbClientPtr cl) { //Release modifier keys
    ((VNCFB*)(cl->screen->screenData))->handleVncKeyboardReleaseEvent();
}

// IFramebuffer properties
/////////////////////////////////////////////////////////////////////////////
/**
 * Requests a resize of our "screen".
 *
 * @returns COM status code
 * @param   pixelFormat Layout of the guest video RAM (i.e. 16, 24,
 *                      32 bpp)
 * @param   vram        host context pointer to the guest video RAM,
 *                      in case we can cope with the format
 * @param   bitsPerPixel color depth of the guest video RAM
 * @param   bytesPerLine length of a screen line in the guest video RAM
 * @param   w           video mode width in pixels
 * @param   h           video mode height in pixels
 * @retval  finished    set to true if the method is synchronous and
 *                      to false otherwise
 *
 * This method is called when the guest attempts to resize the virtual
 * screen.  The pointer to the guest's video RAM is supplied in case
 * the framebuffer can handle the pixel format.  If it can't, it should
 * allocate a memory buffer itself, and the virtual VGA device will copy
 * the guest VRAM to that in a format we can handle.  The
 * COMGETTER(UsesGuestVRAM) method is used to tell the VGA device which method
 * we have chosen, and the other COMGETTER methods tell the device about
 * the layout of our buffer.  We currently handle all VRAM layouts except
 * FramebufferPixelFormat_Opaque (which cannot be handled by
 * definition).
 */
STDMETHODIMP VNCFB::RequestResize(ULONG aScreenId, ULONG pixelFormat,
                                  BYTE *vram, ULONG bitsPerPixel,
                                  ULONG bytesPerLine,
                                  ULONG w, ULONG h, BOOL *finished) {
    NOREF(aScreenId);
    if (!finished) return E_POINTER;

    /* For now, we are doing things synchronously */
    *finished = true;

    if (mRGBBuffer) RTMemFree(mRGBBuffer);

    mWidth = w;
    mHeight = h;

    if (pixelFormat == FramebufferPixelFormat_FOURCC_RGB && bitsPerPixel == 32) {
        mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            mBufferAddress = reinterpret_cast<uint8_t *>(vram);
            mBytesPerLine = bytesPerLine;
            mBitsPerPixel = bitsPerPixel;
            mRGBBuffer = NULL;
    } else {
            mPixelFormat = FramebufferPixelFormat_FOURCC_RGB;
            mBytesPerLine = w * 4;
            mBitsPerPixel = 32;
            mRGBBuffer = reinterpret_cast<uint8_t *>(RTMemAlloc(mBytesPerLine * h));
            AssertReturn(mRGBBuffer != 0, E_OUTOFMEMORY);
            mBufferAddress = mRGBBuffer;
    }

    uint8_t *oldBuffer = mScreenBuffer;
    mScreenBuffer = reinterpret_cast<uint8_t *>(RTMemAlloc(mBytesPerLine * h));
        AssertReturn(mScreenBuffer != 0, E_OUTOFMEMORY);

    for (ULONG i = 0; i < mBytesPerLine * h; i += 4) {
        mScreenBuffer[i]   = mBufferAddress[i+2];
        mScreenBuffer[i+1] = mBufferAddress[i+1];
        mScreenBuffer[i+2] = mBufferAddress[i];
    }

    RTPrintf("Set framebuffer: buffer=%d w=%lu h=%lu bpp=%d\n", mBufferAddress, mWidth, mHeight, (int)mBitsPerPixel);
    rfbNewFramebuffer(vncServer, (char*)mScreenBuffer, mWidth, mHeight, 8, 3, mBitsPerPixel / 8);
    if (oldBuffer) RTMemFree(oldBuffer);
    return S_OK;
}

//Guest framebuffer update notification
STDMETHODIMP VNCFB::NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h) {
    if (!mBufferAddress || !mScreenBuffer) return S_OK;
    ULONG joff = y * mBytesPerLine + x * 4;
    for (ULONG j = joff; j < joff + h * mBytesPerLine; j += mBytesPerLine)
    for (ULONG i = j; i < j + w * 4; i += 4) {
        mScreenBuffer[i]   = mBufferAddress[i+2];
        mScreenBuffer[i+1] = mBufferAddress[i+1];
        mScreenBuffer[i+2] = mBufferAddress[i];
    }
    rfbMarkRectAsModified(vncServer, x, y, x+w, y+h);
    return S_OK;
}




/**
 * Return the address of the frame buffer for the virtual VGA device to
 * write to.  If COMGETTER(UsesGuestVRAM) returns FLASE (or if this address
 * is not the same as the guests VRAM buffer), the device will perform
 * translation.
 *
 * @returns         COM status code
 * @retval  address The address of the buffer
 */
STDMETHODIMP VNCFB::COMGETTER(Address) (BYTE **address) {
    if (!address) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Address): returning address %p\n", mBufferAddress));
    *address = mBufferAddress;
    return S_OK;
}

/**
 * Return the width of our frame buffer.
 *
 * @returns       COM status code
 * @retval  width The width of the frame buffer
 */
STDMETHODIMP VNCFB::COMGETTER(Width) (ULONG *width) {
    if (!width) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Width): returning width %lu\n", (unsigned long) mWidth));
    *width = mWidth;
    return S_OK;
}

/**
 * Return the height of our frame buffer.
 *
 * @returns        COM status code
 * @retval  height The height of the frame buffer
 */
STDMETHODIMP VNCFB::COMGETTER(Height) (ULONG *height) {
    if (!height) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(Height): returning height %lu\n", (unsigned long) mHeight));
    *height = mHeight;
    return S_OK;
}

/**
 * Return the colour depth of our frame buffer.  Note that we actually
 * store the pixel format, not the colour depth internally, since
 * when display sets FramebufferPixelFormat_Opaque, it
 * wants to retreive FramebufferPixelFormat_Opaque and
 * nothing else.
 *
 * @returns            COM status code
 * @retval  bitsPerPixel The colour depth of the frame buffer
 */
STDMETHODIMP VNCFB::COMGETTER(BitsPerPixel) (ULONG *bitsPerPixel) {
    if (!bitsPerPixel) return E_POINTER;
    *bitsPerPixel = mBitsPerPixel;
    LogFlow(("FFmpeg::COMGETTER(BitsPerPixel): returning depth %lu\n",
              (unsigned long) *bitsPerPixel));
    return S_OK;
}

/**
 * Return the number of bytes per line in our frame buffer.
 *
 * @returns          COM status code
 * @retval  bytesPerLine The number of bytes per line
 */
STDMETHODIMP VNCFB::COMGETTER(BytesPerLine) (ULONG *bytesPerLine) {
    if (!bytesPerLine) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(BytesPerLine): returning line size %lu\n", (unsigned long) mBytesPerLine));
    *bytesPerLine = mBytesPerLine;
    return S_OK;
}

/**
 * Return the pixel layout of our frame buffer.
 *
 * @returns             COM status code
 * @retval  pixelFormat The pixel layout
 */
STDMETHODIMP VNCFB::COMGETTER(PixelFormat) (ULONG *pixelFormat) {
    if (!pixelFormat) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(PixelFormat): returning pixel format: %lu\n", (unsigned long) mPixelFormat));
    *pixelFormat = mPixelFormat;
    return S_OK;
}

/**
 * Return whether we use the guest VRAM directly.
 *
 * @returns             COM status code
 * @retval  pixelFormat The pixel layout
 */
STDMETHODIMP VNCFB::COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM) {
    if (!usesGuestVRAM) return E_POINTER;
    LogFlow(("FFmpeg::COMGETTER(UsesGuestVRAM): uses guest VRAM? %d\n", mRGBBuffer == NULL));
    *usesGuestVRAM = (mRGBBuffer == NULL);
    return S_OK;
}

/**
 * Return the number of lines of our frame buffer which can not be used
 * (e.g. for status lines etc?).
 *
 * @returns                 COM status code
 * @retval  heightReduction The number of unused lines
 */
STDMETHODIMP VNCFB::COMGETTER(HeightReduction) (ULONG *heightReduction) {
    if (!heightReduction) return E_POINTER;
    /* no reduction */
    *heightReduction = 0;
    LogFlow(("FFmpeg::COMGETTER(HeightReduction): returning 0\n"));
    return S_OK;
}

/**
 * Return a pointer to the alpha-blended overlay used to render status icons
 * etc above the framebuffer.
 *
 * @returns          COM status code
 * @retval  aOverlay The overlay framebuffer
 */
STDMETHODIMP VNCFB::COMGETTER(Overlay) (IFramebufferOverlay **aOverlay) {
    if (!aOverlay) return E_POINTER;
    /* not yet implemented */
    *aOverlay = 0;
    LogFlow(("FFmpeg::COMGETTER(Overlay): returning 0\n"));
    return S_OK;
}

/**
 * Return id of associated window
 *
 * @returns          COM status code
 * @retval  winId Associated window id
 */
STDMETHODIMP VNCFB::COMGETTER(WinId) (ULONG64 *winId) {
    if (!winId) return E_POINTER;
    *winId = 0;
    return S_OK;
}

// IFramebuffer methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP VNCFB::Lock() {
    LogFlow(("VNCFB::Lock: called\n"));
    int rc = RTCritSectEnter(&mCritSect);
    AssertRC(rc);
    if (rc == VINF_SUCCESS) return S_OK;
    return E_UNEXPECTED;
}

STDMETHODIMP VNCFB::Unlock() {
    LogFlow(("VNCFB::Unlock: called\n"));
    RTCritSectLeave(&mCritSect);
    return S_OK;
}


/**
 * Returns whether we like the given video mode.
 *
 * @returns COM status code
 */
STDMETHODIMP VNCFB::VideoModeSupported(ULONG width, ULONG height, ULONG bpp, BOOL *supported) {
    if (!supported) return E_POINTER;
    *supported = true;
    return S_OK;
}

/** Stubbed */
STDMETHODIMP VNCFB::GetVisibleRegion(BYTE *rectangles, ULONG /* count */, ULONG * /* countCopied */) {
    if (!rectangles) return E_POINTER;
    *rectangles = 0;
    return S_OK;
}

/** Stubbed */
STDMETHODIMP VNCFB::SetVisibleRegion(BYTE *rectangles, ULONG /* count */) {
    if (!rectangles) return E_POINTER;
    return S_OK;
}

STDMETHODIMP VNCFB::ProcessVHWACommand(BYTE *pCommand) {
    return E_NOTIMPL;
}

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(VNCFB)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VNCFB, IFramebuffer)
#endif

