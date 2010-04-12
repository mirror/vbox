/* $Id$ */
/** @file
 * VBox Remote Desktop Protocol - VNC server interface.
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

#include <VBox/com/VirtualBox.h>

#include <iprt/uuid.h>

#include <VBox/com/com.h>
#include <VBox/com/string.h>

#include <iprt/initterm.h>
#include <iprt/critsect.h>

#include <rfb/rfb.h>
#include <pthread.h>

class VNCFB : VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
public:
    VNCFB(ComPtr <IConsole> console, int port, char const *password);
    virtual ~VNCFB();

#ifndef VBOX_WITH_XPCOM
    STDMETHOD_(ULONG, AddRef)() {
        return ::InterlockedIncrement (&refcnt);
    }
    STDMETHOD_(ULONG, Release)() {
        long cnt = ::InterlockedDecrement (&refcnt);
        if (cnt == 0) delete this;
        return cnt;
    }
#endif
    VBOX_SCRIPTABLE_DISPATCH_IMPL(IFramebuffer)

    NS_DECL_ISUPPORTS

    // public methods only for internal purposes
    HRESULT init ();

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
    STDMETHOD(COMGETTER(WinId)) (ULONG64 *winId);

    STDMETHOD(NotifyUpdate)(ULONG x, ULONG y, ULONG w, ULONG h);
    STDMETHOD(RequestResize)(ULONG aScreenId, ULONG pixelFormat, BYTE *vram,
                             ULONG bitsPerPixel, ULONG bytesPerLine,
                             ULONG w, ULONG h, BOOL *finished);
    STDMETHOD(VideoModeSupported)(ULONG width, ULONG height, ULONG bpp, BOOL *supported);
    STDMETHOD(GetVisibleRegion)(BYTE *rectangles, ULONG count, ULONG *countCopied);
    STDMETHOD(SetVisibleRegion)(BYTE *rectangles, ULONG count);

    STDMETHOD(ProcessVHWACommand)(BYTE *pCommand);

private:
    /** Guest framebuffer pixel format */
    ULONG mPixelFormat;
    /** Guest framebuffer color depth */
    ULONG mBitsPerPixel;
    /** Guest framebuffer line length */
    ULONG mBytesPerLine;

    //Our own framebuffer, in case we can't use the VRAM
    uint8_t *mRGBBuffer;
    //The source framebuffer (either our own mRGBBuffer or the guest VRAM)
    uint8_t *mBufferAddress;
    //VNC display framebuffer (RGB -> BGR converted)
    uint8_t *mScreenBuffer;

    int mVncPort;

    ComPtr<IConsole> mConsole;
    ComPtr<IKeyboard> mKeyboard;
    ComPtr<IMouse> mMouse;

    int kbdShiftState;
    void kbdSetShift(int state);
    void kbdPutCode(int code);
    void kbdPutCode(int code, int down);
    void kbdPutCodeShift(int shift, int code, int down);

    ULONG mWidth, mHeight;

    RTCRITSECT mCritSect;

    rfbScreenInfoPtr vncServer;
    RTTHREAD mVncThread;
        static DECLCALLBACK(int) vncThreadFn(RTTHREAD hThreadSelf, void *pvUser);
        /** The password that was passed to the constructor.  NULL if no
         * authentication required. */
    char const *mVncPassword;

    static void vncKeyboardEvent(rfbBool down, rfbKeySym keySym, rfbClientPtr cl);
    static void vncMouseEvent(int buttonMask, int x, int y, rfbClientPtr cl);
    static void vncReleaseKeysEvent(rfbClientPtr cl);

    void handleVncKeyboardEvent(int down, int keySym);
    void handleVncMouseEvent(int buttonMask, int x, int y);
    void handleVncKeyboardReleaseEvent();

    int mouseX, mouseY;

#ifndef VBOX_WITH_XPCOM
    long refcnt;
#endif
};

