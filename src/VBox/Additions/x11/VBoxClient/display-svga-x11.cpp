/* $Id$ */
/** @file
 * X11 guest client - VMSVGA emulation resize event pass-through to X.Org
 * guest driver.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * Known things to test when changing this code.  All assume a guest with VMSVGA
 * active and controlled by X11 or Wayland, and Guest Additions installed and
 * running, unless otherwise stated.
 *  - On Linux 4.6 and later guests, VBoxClient --vmsvga should be running as
 *    root and not as the logged-in user.  Dynamic resizing should work for all
 *    screens in any environment which handles kernel resize notifications,
 *    including at log-in screens.  Test GNOME Shell Wayland and GNOME Shell
 *    under X.Org or Unity or KDE at the log-in screen and after log-in.
 *  - Linux 4.10 changed the user-kernel-ABI introduced in 4.6: test both.
 *  - On other guests (than Linux 4.6 or later) running X.Org Server 1.3 or
 *    later, VBoxClient --vmsvga should never be running as root, and should run
 *    (and dynamic resizing and screen enable/disable should work for all
 *    screens) whenever a user is logged in to a supported desktop environment.
 *  - On guests running X.Org Server 1.2 or older, VBoxClient --vmsvga should
 *    never run as root and should run whenever a user is logged in to a
 *    supported desktop environment.  Dynamic resizing should work for the first
 *    screen, and enabling others should not be possible.
 *  - When VMSVGA is not enabled, VBoxClient --vmsvga should never stay running.
 *  - The following assumptions are done and should be taken into account when reading/chaning the code:
 *    # The order of the outputs (monitors) is assumed to be the same in RANDROUTPUT array and
        XRRScreenResources.outputs array.
 *  - This code does 2 related but separate things: 1- It resizes and enables/disables monitors upon host's
      requests (see the infinite loop in run()). 2- it listens to RandR events (caused by this or any other X11 client)
      on a different thread and notifies host about the new monitor positions. See sendMonitorPositions(...). This is
      mainly a work around since we have realized that vmsvga does not convey correct monitor positions thru FIFO.
 */
#include <stdio.h>
#include <dlfcn.h>
/** For sleep(..) */
#include <unistd.h>
#include "VBoxClient.h"

#include <VBox/VBoxGuestLib.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include <X11/Xlibint.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/panoramiXproto.h>

#define MILLIS_PER_INCH (25.4)
#define DEFAULT_DPI (96.0)


/** Maximum number of supported screens.  DRM and X11 both limit this to 32. */
/** @todo if this ever changes, dynamically allocate resizeable arrays in the
 *  context structure. */
#define VMW_MAX_HEADS 32
/** Monitor positions array. Allocated here and deallocated in the class descructor. */
RTPOINT *mpMonitorPositions;
/** Thread to listen to some of the X server events. */
RTTHREAD mX11MonitorThread = NIL_RTTHREAD;
/** Shutdown indicator for the monitor thread. */
static bool g_fMonitorThreadShutdown = false;


typedef struct {
   CARD8  reqType;           /* always X_VMwareCtrlReqCode */
   CARD8  VMwareCtrlReqType; /* always X_VMwareCtrlSetTopology */
   CARD16 length B16;
   CARD32 screen B32;
   CARD32 number B32;
   CARD32 pad1   B32;
} xVMwareCtrlSetTopologyReq;
#define sz_xVMwareCtrlSetTopologyReq 16

#define X_VMwareCtrlSetTopology 2

typedef struct {
   BYTE   type; /* X_Reply */
   BYTE   pad1;
   CARD16 sequenceNumber B16;
   CARD32 length B32;
   CARD32 screen B32;
   CARD32 pad2   B32;
   CARD32 pad3   B32;
   CARD32 pad4   B32;
   CARD32 pad5   B32;
   CARD32 pad6   B32;
} xVMwareCtrlSetTopologyReply;
#define sz_xVMwareCtrlSetTopologyReply 32

struct X11VMWRECT
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
};
AssertCompileSize(struct X11VMWRECT, 8);

struct X11CONTEXT
{
    Display *pDisplay;
    /* We use a separate connection for randr event listening since sharing  a
       single display object with resizing (main) and event listening threads ends up having a deadlock.*/
    Display *pDisplayRandRMonitoring;
    Window rootWindow;
    int iDefaultScreen;
    XRRScreenResources *pScreenResources;
    int hRandRMajor;
    int hRandRMinor;
    int hRandREventBase;
    int hRandRErrorBase;
    int hEventMask;
    /** The number of outputs (monitors, including disconnect ones) xrandr reports. */
    int hOutputCount;
    void *pRandLibraryHandle;
    bool fWmwareCtrlExtention;
    int hVMWCtrlMajorOpCode;
    /** Function pointers we used if we dlopen libXrandr instead of linking. */
    void (*pXRRSelectInput) (Display *, Window, int);
    Bool (*pXRRQueryExtension) (Display *, int *, int *);
    Status (*pXRRQueryVersion) (Display *, int *, int*);
    XRRMonitorInfo* (*pXRRGetMonitors)(Display *, Window, Bool, int *);
    XRRScreenResources* (*pXRRGetScreenResources)(Display *, Window);
    Status (*pXRRSetCrtcConfig)(Display *, XRRScreenResources *, RRCrtc,
                                Time, int, int, RRMode, Rotation, RROutput *, int);
    void (*pXRRFreeMonitors)(XRRMonitorInfo *);
    void (*pXRRFreeScreenResources)(XRRScreenResources *);
    void (*pXRRFreeModeInfo)(XRRModeInfo *);
    void (*pXRRFreeOutputInfo)(XRROutputInfo *);
    void (*pXRRSetScreenSize)(Display *, Window, int, int, int, int);
    int (*pXRRUpdateConfiguration)(XEvent *event);
    XRRModeInfo* (*pXRRAllocModeInfo)(_Xconst char *, int);
    RRMode (*pXRRCreateMode) (Display *, Window, XRRModeInfo *);
    XRROutputInfo* (*pXRRGetOutputInfo) (Display *, XRRScreenResources *, RROutput);
    XRRCrtcInfo* (*pXRRGetCrtcInfo) (Display *, XRRScreenResources *, RRCrtc crtc);
    void (*pXRRAddOutputMode)(Display *, RROutput, RRMode);
};

static X11CONTEXT x11Context;

#define MAX_MODE_NAME_LEN 64
#define MAX_COMMAND_LINE_LEN 512
#define MAX_MODE_LINE_LEN 512

struct RANDROUTPUT
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    bool fEnabled;
};

struct DisplayModeR {

    /* These are the values that the user sees/provides */
    int Clock;                  /* pixel clock freq (kHz) */
    int HDisplay;               /* horizontal timing */
    int HSyncStart;
    int HSyncEnd;
    int HTotal;
    int HSkew;
    int VDisplay;               /* vertical timing */
    int VSyncStart;
    int VSyncEnd;
    int VTotal;
    int VScan;
    float HSync;
    float VRefresh;
};

/** Forward declarations. */
static void x11Connect();
static int determineOutputCount();

#define checkFunctionPtrReturn(pFunction)                               \
    do{                                                                 \
        if (!pFunction)                                                 \
        {                                                               \
            VBClLogFatalError("Could not find symbol address\n");       \
            dlclose(x11Context.pRandLibraryHandle);                     \
            x11Context.pRandLibraryHandle = NULL;                       \
            return VERR_NOT_FOUND;                                      \
        }                                                               \
    }while(0)

#define checkFunctionPtr(pFunction)                                     \
    do{                                                                 \
        if (!pFunction)                                                 \
        {                                                               \
            VBClLogFatalError("Could not find symbol address\n");       \
        }                                                               \
    }while(0)


/** A slightly modified version of the xf86CVTMode function from xf86cvt.c
  * from the xserver source code. Computes several parameters of a display mode
  * out of horizontal and vertical resolutions. Replicated here to avoid further
  * dependencies. */
DisplayModeR f86CVTMode(int HDisplay, int VDisplay, float VRefresh /* Herz */, Bool Reduced,
            Bool Interlaced)
{
    DisplayModeR Mode;

    /* 1) top/bottom margin size (% of height) - default: 1.8 */
#define CVT_MARGIN_PERCENTAGE 1.8

    /* 2) character cell horizontal granularity (pixels) - default 8 */
#define CVT_H_GRANULARITY 8

    /* 4) Minimum vertical porch (lines) - default 3 */
#define CVT_MIN_V_PORCH 3

    /* 4) Minimum number of vertical back porch lines - default 6 */
#define CVT_MIN_V_BPORCH 6

    /* Pixel Clock step (kHz) */
#define CVT_CLOCK_STEP 250

    Bool Margins = false;
    float VFieldRate, HPeriod;
    int HDisplayRnd, HMargin;
    int VDisplayRnd, VMargin, VSync;
    float Interlace;            /* Please rename this */

    /* CVT default is 60.0Hz */
    if (!VRefresh)
        VRefresh = 60.0;

    /* 1. Required field rate */
    if (Interlaced)
        VFieldRate = VRefresh * 2;
    else
        VFieldRate = VRefresh;

    /* 2. Horizontal pixels */
    HDisplayRnd = HDisplay - (HDisplay % CVT_H_GRANULARITY);

    /* 3. Determine left and right borders */
    if (Margins) {
        /* right margin is actually exactly the same as left */
        HMargin = (((float) HDisplayRnd) * CVT_MARGIN_PERCENTAGE / 100.0);
        HMargin -= HMargin % CVT_H_GRANULARITY;
    }
    else
        HMargin = 0;

    /* 4. Find total active pixels */
    Mode.HDisplay = HDisplayRnd + 2 * HMargin;

    /* 5. Find number of lines per field */
    if (Interlaced)
        VDisplayRnd = VDisplay / 2;
    else
        VDisplayRnd = VDisplay;

    /* 6. Find top and bottom margins */
    /* nope. */
    if (Margins)
        /* top and bottom margins are equal again. */
        VMargin = (((float) VDisplayRnd) * CVT_MARGIN_PERCENTAGE / 100.0);
    else
        VMargin = 0;

    Mode.VDisplay = VDisplay + 2 * VMargin;

    /* 7. Interlace */
    if (Interlaced)
        Interlace = 0.5;
    else
        Interlace = 0.0;

    /* Determine VSync Width from aspect ratio */
    if (!(VDisplay % 3) && ((VDisplay * 4 / 3) == HDisplay))
        VSync = 4;
    else if (!(VDisplay % 9) && ((VDisplay * 16 / 9) == HDisplay))
        VSync = 5;
    else if (!(VDisplay % 10) && ((VDisplay * 16 / 10) == HDisplay))
        VSync = 6;
    else if (!(VDisplay % 4) && ((VDisplay * 5 / 4) == HDisplay))
        VSync = 7;
    else if (!(VDisplay % 9) && ((VDisplay * 15 / 9) == HDisplay))
        VSync = 7;
    else                        /* Custom */
        VSync = 10;

    if (!Reduced) {             /* simplified GTF calculation */

        /* 4) Minimum time of vertical sync + back porch interval (µs)
         * default 550.0 */
#define CVT_MIN_VSYNC_BP 550.0

        /* 3) Nominal HSync width (% of line period) - default 8 */
#define CVT_HSYNC_PERCENTAGE 8

        float HBlankPercentage;
        int VSyncAndBackPorch, VBackPorch;
        int HBlank;

        /* 8. Estimated Horizontal period */
        HPeriod = ((float) (1000000.0 / VFieldRate - CVT_MIN_VSYNC_BP)) /
            (VDisplayRnd + 2 * VMargin + CVT_MIN_V_PORCH + Interlace);

        /* 9. Find number of lines in sync + backporch */
        if (((int) (CVT_MIN_VSYNC_BP / HPeriod) + 1) <
            (VSync + CVT_MIN_V_PORCH))
            VSyncAndBackPorch = VSync + CVT_MIN_V_PORCH;
        else
            VSyncAndBackPorch = (int) (CVT_MIN_VSYNC_BP / HPeriod) + 1;

        /* 10. Find number of lines in back porch */
        VBackPorch = VSyncAndBackPorch - VSync;
        (void) VBackPorch;

        /* 11. Find total number of lines in vertical field */
        Mode.VTotal = VDisplayRnd + 2 * VMargin + VSyncAndBackPorch + Interlace
            + CVT_MIN_V_PORCH;

        /* 5) Definition of Horizontal blanking time limitation */
        /* Gradient (%/kHz) - default 600 */
#define CVT_M_FACTOR 600

        /* Offset (%) - default 40 */
#define CVT_C_FACTOR 40

        /* Blanking time scaling factor - default 128 */
#define CVT_K_FACTOR 128

        /* Scaling factor weighting - default 20 */
#define CVT_J_FACTOR 20

#define CVT_M_PRIME CVT_M_FACTOR * CVT_K_FACTOR / 256
#define CVT_C_PRIME (CVT_C_FACTOR - CVT_J_FACTOR) * CVT_K_FACTOR / 256 + \
        CVT_J_FACTOR

        /* 12. Find ideal blanking duty cycle from formula */
        HBlankPercentage = CVT_C_PRIME - CVT_M_PRIME * HPeriod / 1000.0;

        /* 13. Blanking time */
        if (HBlankPercentage < 20)
            HBlankPercentage = 20;

        HBlank = Mode.HDisplay * HBlankPercentage / (100.0 - HBlankPercentage);
        HBlank -= HBlank % (2 * CVT_H_GRANULARITY);

        /* 14. Find total number of pixels in a line. */
        Mode.HTotal = Mode.HDisplay + HBlank;

        /* Fill in HSync values */
        Mode.HSyncEnd = Mode.HDisplay + HBlank / 2;

        Mode.HSyncStart = Mode.HSyncEnd -
            (Mode.HTotal * CVT_HSYNC_PERCENTAGE) / 100;
        Mode.HSyncStart += CVT_H_GRANULARITY -
            Mode.HSyncStart % CVT_H_GRANULARITY;

        /* Fill in VSync values */
        Mode.VSyncStart = Mode.VDisplay + CVT_MIN_V_PORCH;
        Mode.VSyncEnd = Mode.VSyncStart + VSync;

    }
    else {                      /* Reduced blanking */
        /* Minimum vertical blanking interval time (µs) - default 460 */
#define CVT_RB_MIN_VBLANK 460.0

        /* Fixed number of clocks for horizontal sync */
#define CVT_RB_H_SYNC 32.0

        /* Fixed number of clocks for horizontal blanking */
#define CVT_RB_H_BLANK 160.0

        /* Fixed number of lines for vertical front porch - default 3 */
#define CVT_RB_VFPORCH 3

        int VBILines;

        /* 8. Estimate Horizontal period. */
        HPeriod = ((float) (1000000.0 / VFieldRate - CVT_RB_MIN_VBLANK)) /
            (VDisplayRnd + 2 * VMargin);

        /* 9. Find number of lines in vertical blanking */
        VBILines = ((float) CVT_RB_MIN_VBLANK) / HPeriod + 1;

        /* 10. Check if vertical blanking is sufficient */
        if (VBILines < (CVT_RB_VFPORCH + VSync + CVT_MIN_V_BPORCH))
            VBILines = CVT_RB_VFPORCH + VSync + CVT_MIN_V_BPORCH;

        /* 11. Find total number of lines in vertical field */
        Mode.VTotal = VDisplayRnd + 2 * VMargin + Interlace + VBILines;

        /* 12. Find total number of pixels in a line */
        Mode.HTotal = Mode.HDisplay + CVT_RB_H_BLANK;

        /* Fill in HSync values */
        Mode.HSyncEnd = Mode.HDisplay + CVT_RB_H_BLANK / 2;
        Mode.HSyncStart = Mode.HSyncEnd - CVT_RB_H_SYNC;

        /* Fill in VSync values */
        Mode.VSyncStart = Mode.VDisplay + CVT_RB_VFPORCH;
        Mode.VSyncEnd = Mode.VSyncStart + VSync;
    }
    /* 15/13. Find pixel clock frequency (kHz for xf86) */
    Mode.Clock = Mode.HTotal * 1000.0 / HPeriod;
    Mode.Clock -= Mode.Clock % CVT_CLOCK_STEP;

    /* 16/14. Find actual Horizontal Frequency (kHz) */
    Mode.HSync = ((float) Mode.Clock) / ((float) Mode.HTotal);

    /* 17/15. Find actual Field rate */
    Mode.VRefresh = (1000.0 * ((float) Mode.Clock)) /
        ((float) (Mode.HTotal * Mode.VTotal));

    /* 18/16. Find actual vertical frame frequency */
    /* ignore - just set the mode flag for interlaced */
    if (Interlaced)
        Mode.VTotal *= 2;
    return Mode;
}

/** Makes a call to vmwarectrl extension. This updates the
 * connection information and possible resolutions (modes)
 * of each monitor on the driver. Also sets the preferred mode
 * of each output (monitor) to currently selected one. */
bool VMwareCtrlSetTopology(Display *dpy, int hExtensionMajorOpcode,
                            int screen, xXineramaScreenInfo extents[], int number)
{
   xVMwareCtrlSetTopologyReply rep;
   xVMwareCtrlSetTopologyReq *req;

   long len;

   LockDisplay(dpy);

   GetReq(VMwareCtrlSetTopology, req);
   req->reqType = hExtensionMajorOpcode;
   req->VMwareCtrlReqType = X_VMwareCtrlSetTopology;
   req->screen = screen;
   req->number = number;

   len = ((long) number) << 1;
   SetReqLen(req, len, len);
   len <<= 2;
   _XSend(dpy, (char *)extents, len);

   if (!_XReply(dpy, (xReply *)&rep,
                (SIZEOF(xVMwareCtrlSetTopologyReply) - SIZEOF(xReply)) >> 2,
                xFalse))
   {
       UnlockDisplay(dpy);
       SyncHandle();
       return false;
   }
   UnlockDisplay(dpy);
   SyncHandle();
   return true;
}

/** This function assumes monitors are named as from Virtual1 to VirtualX. */
static int getMonitorIdFromName(const char *sMonitorName)
{
    if (!sMonitorName)
        return -1;
    int iLen = strlen(sMonitorName);
    if (iLen <= 0)
        return -1;
    int iBase = 10;
    int iResult = 0;
    for (int i = iLen - 1; i >= 0; --i)
    {
        /* Stop upon seeing the first non-numeric char. */
        if (sMonitorName[i] < 48 || sMonitorName[i] > 57)
            break;
        iResult += (sMonitorName[i] - 48) * iBase / 10;
        iBase *= 10;
    }
    return iResult;
}

static void sendMonitorPositions(RTPOINT *pPositions, size_t cPositions)
{
    if (cPositions && !pPositions)
    {
        VBClLogError(("Monitor position update called with NULL pointer!\n"));
        return;
    }
    int rc = VbglR3SeamlessSendMonitorPositions(cPositions, pPositions);
    if (RT_SUCCESS(rc))
        VBClLogError("Sending monitor positions (%u of them)  to the host: %Rrc\n", cPositions, rc);
    else
        VBClLogError("Error during sending monitor positions (%u of them)  to the host: %Rrc\n", cPositions, rc);
}

static void queryMonitorPositions()
{
    static const int iSentinelPosition = -1;
    if (mpMonitorPositions)
    {
        free(mpMonitorPositions);
        mpMonitorPositions = NULL;
    }

    int iMonitorCount = 0;
    XRRMonitorInfo *pMonitorInfo = NULL;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    pMonitorInfo = XRRGetMonitors(x11Context.pDisplayRandRMonitoring,
                                  DefaultRootWindow(x11Context.pDisplayRandRMonitoring), true, &iMonitorCount);
#else
    if (x11Context.pXRRGetMonitors)
        pMonitorInfo = x11Context.pXRRGetMonitors(x11Context.pDisplayRandRMonitoring,
                                                  DefaultRootWindow(x11Context.pDisplayRandRMonitoring), true, &iMonitorCount);
#endif
    if (!pMonitorInfo)
        return;
    if (iMonitorCount == -1)
        VBClLogError("Could not get monitor info\n");
    else
    {
        mpMonitorPositions = (RTPOINT*)malloc(x11Context.hOutputCount * sizeof(RTPOINT));
        /** @todo memset? */
        for (int i = 0; i < x11Context.hOutputCount; ++i)
        {
            mpMonitorPositions[i].x = iSentinelPosition;
            mpMonitorPositions[i].y = iSentinelPosition;
        }
        for (int i = 0; i < iMonitorCount; ++i)
        {
            int iMonitorID = getMonitorIdFromName(XGetAtomName(x11Context.pDisplayRandRMonitoring, pMonitorInfo[i].name)) - 1;
            if (iMonitorID >= x11Context.hOutputCount || iMonitorID == -1)
                continue;
            VBClLogInfo("Monitor %d (w,h)=(%d,%d) (x,y)=(%d,%d)\n",
                        i,
                        pMonitorInfo[i].width, pMonitorInfo[i].height,
                        pMonitorInfo[i].x, pMonitorInfo[i].y);
            mpMonitorPositions[iMonitorID].x = pMonitorInfo[i].x;
            mpMonitorPositions[iMonitorID].y = pMonitorInfo[i].y;
        }
        if (iMonitorCount > 0)
            sendMonitorPositions(mpMonitorPositions, x11Context.hOutputCount);
    }
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRFreeMonitors(pMonitorInfo);
#else
    if (x11Context.pXRRFreeMonitors)
        x11Context.pXRRFreeMonitors(pMonitorInfo);
#endif
}

static void monitorRandREvents()
{
    XEvent event;
    XNextEvent(x11Context.pDisplayRandRMonitoring, &event);
    int eventTypeOffset = event.type - x11Context.hRandREventBase;
    switch (eventTypeOffset)
    {
        case RRScreenChangeNotify:
            VBClLogInfo("RRScreenChangeNotify event received\n");
            queryMonitorPositions();
            break;
        default:
            break;
    }
}

/**
 * @callback_method_impl{FNRTTHREAD}
 */
static DECLCALLBACK(int) x11MonitorThreadFunction(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf, pvUser);
    while (!ASMAtomicReadBool(&g_fMonitorThreadShutdown))
    {
        monitorRandREvents();
    }
    return 0;
}

static int startX11MonitorThread()
{
    int rc;
    Assert(g_fMonitorThreadShutdown == false);
    if (mX11MonitorThread == NIL_RTTHREAD)
    {
        rc = RTThreadCreate(&mX11MonitorThread, x11MonitorThreadFunction, NULL /*pvUser*/, 0 /*cbStack*/,
                            RTTHREADTYPE_MSG_PUMP, RTTHREADFLAGS_WAITABLE, "X11 events");
        if (RT_FAILURE(rc))
            VBClLogFatalError("Warning: failed to start X11 monitor thread (VBoxClient) rc=%Rrc!\n", rc);
    }
    else
        rc = VINF_ALREADY_INITIALIZED;
    return rc;
}

static int stopX11MonitorThread(void)
{
    int rc;
    if (mX11MonitorThread != NIL_RTTHREAD)
    {
        ASMAtomicWriteBool(&g_fMonitorThreadShutdown, true);
        /** @todo  Send event to thread to get it out of XNextEvent. */
        //????????
        //mX11Monitor.interruptEventWait();
        rc = RTThreadWait(mX11MonitorThread, RT_MS_1SEC, NULL /*prc*/);
        if (RT_SUCCESS(rc))
        {
            mX11MonitorThread = NIL_RTTHREAD;
            g_fMonitorThreadShutdown = false;
        }
        else
            VBClLogError("Failed to stop X11 monitor thread, rc=%Rrc!\n", rc);
    }
    return rc;
}

static bool callVMWCTRL(struct RANDROUTPUT *paOutputs)
{
    int hHeight = 600;
    int hWidth = 800;

    xXineramaScreenInfo *extents = (xXineramaScreenInfo *)malloc(x11Context.hOutputCount * sizeof(xXineramaScreenInfo));
    if (!extents)
        return false;
    int hRunningOffset = 0;
    for (int i = 0; i < x11Context.hOutputCount; ++i)
    {
        if (paOutputs[i].fEnabled)
        {
            hHeight = paOutputs[i].height;
            hWidth = paOutputs[i].width;
        }
        else
        {
            hHeight = 0;
            hWidth = 0;
        }
        extents[i].x_org = hRunningOffset;
        extents[i].y_org = 0;
        extents[i].width = hWidth;
        extents[i].height = hHeight;
        hRunningOffset += hWidth;
    }
    return VMwareCtrlSetTopology(x11Context.pDisplay, x11Context.hVMWCtrlMajorOpCode,
                                 DefaultScreen(x11Context.pDisplay),
                                 extents, x11Context.hOutputCount);
    free(extents);
}

/**
 * Tries to determine if the session parenting this process is of Xwayland.
 * NB: XDG_SESSION_TYPE is a systemd(1) environment variable and is unlikely
 * set in non-systemd environments or remote logins.
 * Therefore we check the Wayland specific display environment variable first.
 */
static bool isXwayland(void)
{
    const char *const pDisplayType = getenv("WAYLAND_DISPLAY");
    const char *pSessionType;

    if (pDisplayType != NULL) {
        return true;
    }
    pSessionType = getenv("XDG_SESSION_TYPE");
    if ((pSessionType != NULL) && (RTStrIStartsWith(pSessionType, "wayland"))) {
        return true;
    }
    return false;
}

static bool init()
{
    if (isXwayland())
    {
        VBClLogInfo("The parent session seems to be running on Wayland. Starting DRM client\n");
        char* argv[] = {NULL};
        char* env[] = {NULL};
        execve("./VBoxDRMClient", argv, env);
        perror("Could not start the DRM Client.");
        return false;
    }
    x11Connect();
    if (x11Context.pDisplay == NULL)
        return false;
    if (RT_FAILURE(startX11MonitorThread()))
        return false;
    return true;
}

static void cleanup()
{
    if (mpMonitorPositions)
    {
        free(mpMonitorPositions);
        mpMonitorPositions = NULL;
    }
    stopX11MonitorThread();
    if (x11Context.pRandLibraryHandle)
    {
        dlclose(x11Context.pRandLibraryHandle);
        x11Context.pRandLibraryHandle = NULL;
    }
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRSelectInput(x11Context.pDisplayRandRMonitoring, x11Context.rootWindow, 0);
    XRRFreeScreenResources(x11Context.pScreenResources);
#else
    if (x11Context.pXRRSelectInput)
        x11Context.pXRRSelectInput(x11Context.pDisplayRandRMonitoring, x11Context.rootWindow, 0);
    if (x11Context.pXRRFreeScreenResources)
        x11Context.pXRRFreeScreenResources(x11Context.pScreenResources);
#endif
    XCloseDisplay(x11Context.pDisplay);
    XCloseDisplay(x11Context.pDisplayRandRMonitoring);
}

#ifndef WITH_DISTRO_XRAND_XINERAMA
static int openLibRandR()
{
    x11Context.pRandLibraryHandle = dlopen("libXrandr.so", RTLD_LAZY /*| RTLD_LOCAL */);
    if (!x11Context.pRandLibraryHandle)
        x11Context.pRandLibraryHandle = dlopen("libXrandr.so.2", RTLD_LAZY /*| RTLD_LOCAL */);
    if (!x11Context.pRandLibraryHandle)
        x11Context.pRandLibraryHandle = dlopen("libXrandr.so.2.2.0", RTLD_LAZY /*| RTLD_LOCAL */);

    if (!x11Context.pRandLibraryHandle)
    {
        VBClLogFatalError("Could not locate libXrandr for dlopen\n");
        return VERR_NOT_FOUND;
    }

    *(void **)(&x11Context.pXRRSelectInput) = dlsym(x11Context.pRandLibraryHandle, "XRRSelectInput");
    checkFunctionPtrReturn(x11Context.pXRRSelectInput);

    *(void **)(&x11Context.pXRRQueryExtension) = dlsym(x11Context.pRandLibraryHandle, "XRRQueryExtension");
    checkFunctionPtrReturn(x11Context.pXRRQueryExtension);

    *(void **)(&x11Context.pXRRQueryVersion) = dlsym(x11Context.pRandLibraryHandle, "XRRQueryVersion");
    checkFunctionPtrReturn(x11Context.pXRRQueryVersion);

    /* Don't bail out when XRRGetMonitors XRRFreeMonitors are missing as in Oracle Solaris 10. It is not crucial esp. for single monitor. */
    *(void **)(&x11Context.pXRRGetMonitors) = dlsym(x11Context.pRandLibraryHandle, "XRRGetMonitors");
    checkFunctionPtr(x11Context.pXRRGetMonitors);

    *(void **)(&x11Context.pXRRFreeMonitors) = dlsym(x11Context.pRandLibraryHandle, "XRRFreeMonitors");
    checkFunctionPtr(x11Context.pXRRFreeMonitors);

    *(void **)(&x11Context.pXRRGetScreenResources) = dlsym(x11Context.pRandLibraryHandle, "XRRGetScreenResources");
    checkFunctionPtrReturn(x11Context.pXRRGetScreenResources);

    *(void **)(&x11Context.pXRRSetCrtcConfig) = dlsym(x11Context.pRandLibraryHandle, "XRRSetCrtcConfig");
    checkFunctionPtrReturn(x11Context.pXRRSetCrtcConfig);

    *(void **)(&x11Context.pXRRFreeScreenResources) = dlsym(x11Context.pRandLibraryHandle, "XRRFreeScreenResources");
    checkFunctionPtrReturn(x11Context.pXRRFreeScreenResources);

    *(void **)(&x11Context.pXRRFreeModeInfo) = dlsym(x11Context.pRandLibraryHandle, "XRRFreeModeInfo");
    checkFunctionPtrReturn(x11Context.pXRRFreeModeInfo);

    *(void **)(&x11Context.pXRRFreeOutputInfo) = dlsym(x11Context.pRandLibraryHandle, "XRRFreeOutputInfo");
    checkFunctionPtrReturn(x11Context.pXRRFreeOutputInfo);

    *(void **)(&x11Context.pXRRSetScreenSize) = dlsym(x11Context.pRandLibraryHandle, "XRRSetScreenSize");
    checkFunctionPtrReturn(x11Context.pXRRSetScreenSize);

    *(void **)(&x11Context.pXRRUpdateConfiguration) = dlsym(x11Context.pRandLibraryHandle, "XRRUpdateConfiguration");
    checkFunctionPtrReturn(x11Context.pXRRUpdateConfiguration);

    *(void **)(&x11Context.pXRRAllocModeInfo) = dlsym(x11Context.pRandLibraryHandle, "XRRAllocModeInfo");
    checkFunctionPtrReturn(x11Context.pXRRAllocModeInfo);

    *(void **)(&x11Context.pXRRCreateMode) = dlsym(x11Context.pRandLibraryHandle, "XRRCreateMode");
    checkFunctionPtrReturn(x11Context.pXRRCreateMode);

    *(void **)(&x11Context.pXRRGetOutputInfo) = dlsym(x11Context.pRandLibraryHandle, "XRRGetOutputInfo");
    checkFunctionPtrReturn(x11Context.pXRRGetOutputInfo);

    *(void **)(&x11Context.pXRRGetCrtcInfo) = dlsym(x11Context.pRandLibraryHandle, "XRRGetCrtcInfo");
    checkFunctionPtrReturn(x11Context.pXRRGetCrtcInfo);

    *(void **)(&x11Context.pXRRAddOutputMode) = dlsym(x11Context.pRandLibraryHandle, "XRRAddOutputMode");
    checkFunctionPtrReturn(x11Context.pXRRAddOutputMode);

    return VINF_SUCCESS;
}
#endif

static void x11Connect()
{
    x11Context.pScreenResources = NULL;
    x11Context.pXRRSelectInput = NULL;
    x11Context.pRandLibraryHandle = NULL;
    x11Context.pXRRQueryExtension = NULL;
    x11Context.pXRRQueryVersion = NULL;
    x11Context.pXRRGetMonitors = NULL;
    x11Context.pXRRGetScreenResources = NULL;
    x11Context.pXRRSetCrtcConfig = NULL;
    x11Context.pXRRFreeMonitors = NULL;
    x11Context.pXRRFreeScreenResources = NULL;
    x11Context.pXRRFreeOutputInfo = NULL;
    x11Context.pXRRFreeModeInfo = NULL;
    x11Context.pXRRSetScreenSize = NULL;
    x11Context.pXRRUpdateConfiguration = NULL;
    x11Context.pXRRAllocModeInfo = NULL;
    x11Context.pXRRCreateMode = NULL;
    x11Context.pXRRGetOutputInfo = NULL;
    x11Context.pXRRGetCrtcInfo = NULL;
    x11Context.pXRRAddOutputMode = NULL;
    x11Context.fWmwareCtrlExtention = false;

    int dummy;
    if (x11Context.pDisplay != NULL)
        VBClLogFatalError("%s called with bad argument\n", __func__);
    x11Context.pDisplay = XOpenDisplay(NULL);
    x11Context.pDisplayRandRMonitoring = XOpenDisplay(NULL);
    if (x11Context.pDisplay == NULL)
        return;
#ifndef WITH_DISTRO_XRAND_XINERAMA
    if (openLibRandR() != VINF_SUCCESS)
    {
        XCloseDisplay(x11Context.pDisplay);
        XCloseDisplay(x11Context.pDisplayRandRMonitoring);
        x11Context.pDisplay = NULL;
        x11Context.pDisplayRandRMonitoring = NULL;
        return;
    }
#endif

    x11Context.fWmwareCtrlExtention = XQueryExtension(x11Context.pDisplay, "VMWARE_CTRL",
                                                      &x11Context.hVMWCtrlMajorOpCode, &dummy, &dummy);
    if (!x11Context.fWmwareCtrlExtention)
        VBClLogError("VMWARE's ctrl extension is not available! Multi monitor management is not possible\n");
    else
        VBClLogInfo("VMWARE's ctrl extension is available. Major Opcode is %d.\n", x11Context.hVMWCtrlMajorOpCode);

    /* Check Xrandr stuff. */
    bool fSuccess = false;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    fSuccess = XRRQueryExtension(x11Context.pDisplay, &x11Context.hRandREventBase, &x11Context.hRandRErrorBase);
#else
    if (x11Context.pXRRQueryExtension)
        fSuccess = x11Context.pXRRQueryExtension(x11Context.pDisplay, &x11Context.hRandREventBase, &x11Context.hRandRErrorBase);
#endif
    if (fSuccess)
    {
        fSuccess = false;
#ifdef WITH_DISTRO_XRAND_XINERAMA
        fSuccess = XRRQueryVersion(x11Context.pDisplay, &x11Context.hRandRMajor, &x11Context.hRandRMinor);
#else
    if (x11Context.pXRRQueryVersion)
        fSuccess = x11Context.pXRRQueryVersion(x11Context.pDisplay, &x11Context.hRandRMajor, &x11Context.hRandRMinor);
#endif
        if (!fSuccess)
        {
            XCloseDisplay(x11Context.pDisplay);
            x11Context.pDisplay = NULL;
            return;
        }
    }
    x11Context.rootWindow = DefaultRootWindow(x11Context.pDisplay);
    x11Context.hEventMask = RRScreenChangeNotifyMask;

    /* Select the XEvent types we want to listen to. */
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRSelectInput(x11Context.pDisplayRandRMonitoring, x11Context.rootWindow, x11Context.hEventMask);
#else
    if (x11Context.pXRRSelectInput)
        x11Context.pXRRSelectInput(x11Context.pDisplayRandRMonitoring, x11Context.rootWindow, x11Context.hEventMask);
#endif
    x11Context.iDefaultScreen = DefaultScreen(x11Context.pDisplay);

#ifdef WITH_DISTRO_XRAND_XINERAMA
    x11Context.pScreenResources = XRRGetScreenResources(x11Context.pDisplay, x11Context.rootWindow);
#else
    if (x11Context.pXRRGetScreenResources)
        x11Context.pScreenResources = x11Context.pXRRGetScreenResources(x11Context.pDisplay, x11Context.rootWindow);
#endif
    /* Currently without the VMWARE_CTRL extension we cannot connect outputs and set outputs' preferred mode.
     * So we set the output count to 1 to get the 1st output position correct. */
    x11Context.hOutputCount = x11Context.fWmwareCtrlExtention ? determineOutputCount() : 1;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRFreeScreenResources(x11Context.pScreenResources);
#else
    if (x11Context.pXRRFreeScreenResources)
        x11Context.pXRRFreeScreenResources(x11Context.pScreenResources);
#endif
}

static int determineOutputCount()
{
    if (!x11Context.pScreenResources)
        return 0;
    return x11Context.pScreenResources->noutput;
}

static int findExistingModeIndex(unsigned iXRes, unsigned iYRes)
{
    if (!x11Context.pScreenResources)
        return -1;
    for (int i = 0; i < x11Context.pScreenResources->nmode; ++i)
    {
        if (x11Context.pScreenResources->modes[i].width == iXRes && x11Context.pScreenResources->modes[i].height == iYRes)
            return i;
    }
    return -1;
}

static bool disableCRTC(RRCrtc crtcID)
{
    XRRCrtcInfo *pCrctInfo = NULL;

#ifdef WITH_DISTRO_XRAND_XINERAMA
    pCrctInfo = XRRGetCrtcInfo(x11Context.pDisplay, x11Context.pScreenResources, crtcID);
#else
    if (x11Context.pXRRGetCrtcInfo)
        pCrctInfo = x11Context.pXRRGetCrtcInfo(x11Context.pDisplay, x11Context.pScreenResources, crtcID);
#endif

    if (!pCrctInfo)
        return false;

    Status ret = Success;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    ret = XRRSetCrtcConfig(x11Context.pDisplay, x11Context.pScreenResources, crtcID,
                           CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
#else
    if (x11Context.pXRRSetCrtcConfig)
        ret = x11Context.pXRRSetCrtcConfig(x11Context.pDisplay, x11Context.pScreenResources, crtcID,
                                           CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
#endif
    if (ret == Success)
        return true;
    else
        return false;
}

static XRRScreenSize currentSize()
{
    XRRScreenSize cSize;
    cSize.width = DisplayWidth(x11Context.pDisplay, x11Context.iDefaultScreen);
    cSize.mwidth = DisplayWidthMM(x11Context.pDisplay, x11Context.iDefaultScreen);
    cSize.height = DisplayHeight(x11Context.pDisplay, x11Context.iDefaultScreen);
    cSize.mheight = DisplayHeightMM(x11Context.pDisplay, x11Context.iDefaultScreen);
    return cSize;
}

static unsigned int computeDpi(unsigned int pixels, unsigned int mm)
{
   unsigned int dpi = 0;
   if (mm > 0) {
      dpi = (unsigned int)((double)pixels * MILLIS_PER_INCH /
                           (double)mm + 0.5);
   }
   return (dpi > 0) ? dpi : DEFAULT_DPI;
}

static bool resizeFrameBuffer(struct RANDROUTPUT *paOutputs)
{
    unsigned int iXRes = 0;
    unsigned int iYRes = 0;
    /* Don't care about the output positions for now. */
    for (int i = 0; i < x11Context.hOutputCount; ++i)
    {
        if (!paOutputs[i].fEnabled)
            continue;
        iXRes += paOutputs[i].width;
        iYRes = iYRes < paOutputs[i].height ? paOutputs[i].height : iYRes;
    }
    XRRScreenSize cSize= currentSize();
    unsigned int xdpi = computeDpi(cSize.width, cSize.mwidth);
    unsigned int ydpi = computeDpi(cSize.height, cSize.mheight);
    unsigned int xmm;
    unsigned int ymm;
    xmm = (int)(MILLIS_PER_INCH * iXRes / ((double)xdpi) + 0.5);
    ymm = (int)(MILLIS_PER_INCH * iYRes / ((double)ydpi) + 0.5);
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, RRScreenChangeNotifyMask);
    XRRSetScreenSize(x11Context.pDisplay, x11Context.rootWindow, iXRes, iYRes, xmm, ymm);
#else
    if (x11Context.pXRRSelectInput)
        x11Context.pXRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, RRScreenChangeNotifyMask);
    if (x11Context.pXRRSetScreenSize)
        x11Context.pXRRSetScreenSize(x11Context.pDisplay, x11Context.rootWindow, iXRes, iYRes, xmm, ymm);
#endif
    XSync(x11Context.pDisplay, False);
    XEvent configEvent;
    bool event = false;
    while (XCheckTypedEvent(x11Context.pDisplay, RRScreenChangeNotify + x11Context.hRandREventBase, &configEvent))
    {
#ifdef WITH_DISTRO_XRAND_XINERAMA
        XRRUpdateConfiguration(&configEvent);
#else
        if (x11Context.pXRRUpdateConfiguration)
            x11Context.pXRRUpdateConfiguration(&configEvent);
#endif
        event = true;
    }
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, 0);
#else
    if (x11Context.pXRRSelectInput)
        x11Context.pXRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, 0);
#endif
    XRRScreenSize newSize = currentSize();
    /** @todo  In case of unsuccesful frame buffer resize we have to revert frame buffer size and crtc sizes. */
    if (!event || newSize.width != (int)iXRes || newSize.height != (int)iYRes)
    {
        VBClLogError("Resizing frame buffer to %d %d has failed\n", iXRes, iYRes);
        return false;
    }
    return true;
}

static XRRModeInfo *createMode(int iXRes, int iYRes)
{
    XRRModeInfo *pModeInfo = NULL;
    char sModeName[126];
    sprintf(sModeName, "%dx%d_vbox", iXRes, iYRes);
#ifdef WITH_DISTRO_XRAND_XINERAMA
    pModeInfo = XRRAllocModeInfo(sModeName, strlen(sModeName));
#else
    if (x11Context.pXRRAllocModeInfo)
        pModeInfo = x11Context.pXRRAllocModeInfo(sModeName, strlen(sModeName));
#endif
    pModeInfo->width = iXRes;
    pModeInfo->height = iYRes;

    DisplayModeR mode = f86CVTMode(iXRes, iYRes, 60 /*VRefresh */, true /*Reduced */, false  /* Interlaced */);

    pModeInfo->dotClock = mode.Clock;
    pModeInfo->hSyncStart = mode.HSyncStart;
    pModeInfo->hSyncEnd = mode.HSyncEnd;
    pModeInfo->hTotal = mode.HTotal;
    pModeInfo->hSkew = mode.HSkew;
    pModeInfo->vSyncStart = mode.VSyncStart;
    pModeInfo->vSyncEnd = mode.VSyncEnd;
    pModeInfo->vTotal = mode.VTotal;

    RRMode newMode = None;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    newMode = XRRCreateMode(x11Context.pDisplay, x11Context.rootWindow, pModeInfo);
#else
    if (x11Context.pXRRCreateMode)
        newMode = x11Context.pXRRCreateMode(x11Context.pDisplay, x11Context.rootWindow, pModeInfo);
#endif
    if (newMode == None)
    {
#ifdef WITH_DISTRO_XRAND_XINERAMA
        XRRFreeModeInfo(pModeInfo);
#else
        if (x11Context.pXRRFreeModeInfo)
            x11Context.pXRRFreeModeInfo(pModeInfo);
#endif
        return NULL;
    }
    pModeInfo->id = newMode;
    return pModeInfo;
}

static bool configureOutput(int iOutputIndex, struct RANDROUTPUT *paOutputs)
{
    if (iOutputIndex >= x11Context.hOutputCount)
    {
        VBClLogError("Output index %d is greater than # of oputputs %d\n", iOutputIndex, x11Context.hOutputCount);
        return false;
    }
    RROutput outputId = x11Context.pScreenResources->outputs[iOutputIndex];
    XRROutputInfo *pOutputInfo = NULL;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    pOutputInfo = XRRGetOutputInfo(x11Context.pDisplay, x11Context.pScreenResources, outputId);
#else
    if (x11Context.pXRRGetOutputInfo)
        pOutputInfo = x11Context.pXRRGetOutputInfo(x11Context.pDisplay, x11Context.pScreenResources, outputId);
#endif
    if (!pOutputInfo)
        return false;
    XRRModeInfo *pModeInfo = NULL;
    bool fNewMode = false;
    /* Index of the mode within the XRRScreenResources.modes array. -1 if such a mode with required resolution does not exists*/
    int iModeIndex = findExistingModeIndex(paOutputs[iOutputIndex].width, paOutputs[iOutputIndex].height);
    if (iModeIndex != -1 && iModeIndex < x11Context.pScreenResources->nmode)
        pModeInfo = &(x11Context.pScreenResources->modes[iModeIndex]);
    else
    {
        /* A mode with required size was not found. Create a new one. */
        pModeInfo = createMode(paOutputs[iOutputIndex].width, paOutputs[iOutputIndex].height);
        fNewMode = true;
    }
    if (!pModeInfo)
    {
        VBClLogError("Could not create mode for the resolution (%d, %d)\n",
                     paOutputs[iOutputIndex].width, paOutputs[iOutputIndex].height);
        return false;
    }

#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRAddOutputMode(x11Context.pDisplay, outputId, pModeInfo->id);
#else
    if (x11Context.pXRRAddOutputMode)
        x11Context.pXRRAddOutputMode(x11Context.pDisplay, outputId, pModeInfo->id);
#endif
    /* Make sure outputs crtc is set. */
    pOutputInfo->crtc = pOutputInfo->crtcs[0];

    RRCrtc crtcId = pOutputInfo->crtcs[0];
    Status ret = Success;
#ifdef WITH_DISTRO_XRAND_XINERAMA
    ret = XRRSetCrtcConfig(x11Context.pDisplay, x11Context.pScreenResources, crtcId, CurrentTime,
                           paOutputs[iOutputIndex].x, paOutputs[iOutputIndex].y,
                           pModeInfo->id, RR_Rotate_0, &(outputId), 1 /*int noutputs*/);
#else
    if (x11Context.pXRRSetCrtcConfig)
        ret = x11Context.pXRRSetCrtcConfig(x11Context.pDisplay, x11Context.pScreenResources, crtcId, CurrentTime,
                                           paOutputs[iOutputIndex].x, paOutputs[iOutputIndex].y,
                                           pModeInfo->id, RR_Rotate_0, &(outputId), 1 /*int noutputs*/);
#endif
    if (ret != Success)
        VBClLogError("crtc set config failed for output %d\n", iOutputIndex);

#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRFreeOutputInfo(pOutputInfo);
#else
    if (x11Context.pXRRFreeOutputInfo)
        x11Context.pXRRFreeOutputInfo(pOutputInfo);
#endif

    if (fNewMode)
    {
#ifdef WITH_DISTRO_XRAND_XINERAMA
        XRRFreeModeInfo(pModeInfo);
#else
        if (x11Context.pXRRFreeModeInfo)
            x11Context.pXRRFreeModeInfo(pModeInfo);
#endif
    }
    return true;
}

/** Construct the xrandr command which sets the whole monitor topology each time. */
static void setXrandrTopology(struct RANDROUTPUT *paOutputs)
{
    XGrabServer(x11Context.pDisplay);
    if (x11Context.fWmwareCtrlExtention)
        callVMWCTRL(paOutputs);

#ifdef WITH_DISTRO_XRAND_XINERAMA
    x11Context.pScreenResources = XRRGetScreenResources(x11Context.pDisplay, x11Context.rootWindow);
#else
    if (x11Context.pXRRGetScreenResources)
        x11Context.pScreenResources = x11Context.pXRRGetScreenResources(x11Context.pDisplay, x11Context.rootWindow);
#endif
    x11Context.hOutputCount = x11Context.fWmwareCtrlExtention ? determineOutputCount() : 1;

    if (!x11Context.pScreenResources)
    {
        XUngrabServer(x11Context.pDisplay);
        return;
    }

    /* Disable crtcs. */
    for (int i = 0; i < x11Context.pScreenResources->noutput; ++i)
    {
        XRROutputInfo *pOutputInfo = NULL;
#ifdef WITH_DISTRO_XRAND_XINERAMA
        pOutputInfo = XRRGetOutputInfo(x11Context.pDisplay, x11Context.pScreenResources, x11Context.pScreenResources->outputs[i]);
#else
        if (x11Context.pXRRGetOutputInfo)
            pOutputInfo = x11Context.pXRRGetOutputInfo(x11Context.pDisplay, x11Context.pScreenResources, x11Context.pScreenResources->outputs[i]);
#endif
        if (!pOutputInfo)
            continue;
        if (pOutputInfo->crtc == None)
            continue;

        if (!disableCRTC(pOutputInfo->crtc))
        {
            VBClLogFatalError("Crtc disable failed %lu\n", pOutputInfo->crtc);
            XUngrabServer(x11Context.pDisplay);
#ifdef WITH_DISTRO_XRAND_XINERAMA
            XRRFreeScreenResources(x11Context.pScreenResources);
#else
            if (x11Context.pXRRFreeScreenResources)
                x11Context.pXRRFreeScreenResources(x11Context.pScreenResources);
#endif
            return;
        }
#ifdef WITH_DISTRO_XRAND_XINERAMA
        XRRFreeOutputInfo(pOutputInfo);
#else
        if (x11Context.pXRRFreeOutputInfo)
            x11Context.pXRRFreeOutputInfo(pOutputInfo);
#endif
    }
    /* Resize the frame buffer. */
    if (!resizeFrameBuffer(paOutputs))
    {
        XUngrabServer(x11Context.pDisplay);
#ifdef WITH_DISTRO_XRAND_XINERAMA
        XRRFreeScreenResources(x11Context.pScreenResources);
#else
        if (x11Context.pXRRFreeScreenResources)
            x11Context.pXRRFreeScreenResources(x11Context.pScreenResources);
#endif
        return;
    }

    /* Configure the outputs. */
    for (int i = 0; i < x11Context.hOutputCount; ++i)
    {
        /* be paranoid. */
        if (i >= x11Context.pScreenResources->noutput)
            break;
        if (!paOutputs[i].fEnabled)
            continue;
        configureOutput(i, paOutputs);
    }
    XSync(x11Context.pDisplay, False);
#ifdef WITH_DISTRO_XRAND_XINERAMA
    XRRFreeScreenResources(x11Context.pScreenResources);
#else
    if (x11Context.pXRRFreeScreenResources)
        x11Context.pXRRFreeScreenResources(x11Context.pScreenResources);
#endif
    XUngrabServer(x11Context.pDisplay);
    XFlush(x11Context.pDisplay);
}

static const char *getName()
{
    return "Display SVGA X11";
}

static const char *getPidFilePath()
{
    return ".vboxclient-display-svga-x11.pid";
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    RT_NOREF(ppInterface, fDaemonised);
    int rc;
    uint32_t events;
    /* Do not acknowledge the first event we query for to pick up old events,
     * e.g. from before a guest reboot. */
    bool fAck = false;
    bool fFirstRun = true;
    if (!init())
        return VERR_NOT_AVAILABLE;
    static struct VMMDevDisplayDef aMonitors[VMW_MAX_HEADS];

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to request display change events, rc=%Rrc\n", rc);
    rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to register resizing support, rc=%Rrc\n", rc);
    if (rc == VERR_RESOURCE_BUSY)  /* Someone else has already acquired it. */
        return VERR_RESOURCE_BUSY;
    for (;;)
    {
        struct VMMDevDisplayDef aDisplays[VMW_MAX_HEADS];
        uint32_t cDisplaysOut;
        /* Query the first size without waiting.  This lets us e.g. pick up
         * the last event before a guest reboot when we start again after. */
        rc = VbglR3GetDisplayChangeRequestMulti(VMW_MAX_HEADS, &cDisplaysOut, aDisplays, fAck);
        fAck = true;
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failed to get display change request, rc=%Rrc\n", rc);
        if (cDisplaysOut > VMW_MAX_HEADS)
            VBClLogFatalError("Display change request contained, rc=%Rrc\n", rc);
        if (cDisplaysOut > 0)
        {
            for (unsigned i = 0; i < cDisplaysOut && i < VMW_MAX_HEADS; ++i)
            {
                uint32_t idDisplay = aDisplays[i].idDisplay;
                if (idDisplay >= VMW_MAX_HEADS)
                    continue;
                aMonitors[idDisplay].fDisplayFlags = aDisplays[i].fDisplayFlags;
                if (!(aDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
                {
                    if (idDisplay == 0 || (aDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_ORIGIN))
                    {
                        aMonitors[idDisplay].xOrigin = aDisplays[i].xOrigin;
                        aMonitors[idDisplay].yOrigin = aDisplays[i].yOrigin;
                    } else {
                        aMonitors[idDisplay].xOrigin = aMonitors[idDisplay - 1].xOrigin + aMonitors[idDisplay - 1].cx;
                        aMonitors[idDisplay].yOrigin = aMonitors[idDisplay - 1].yOrigin;
                    }
                    aMonitors[idDisplay].cx = aDisplays[i].cx;
                    aMonitors[idDisplay].cy = aDisplays[i].cy;
                }
            }
            /* Create a whole topology and send it to xrandr. */
            struct RANDROUTPUT aOutputs[VMW_MAX_HEADS];
            int iRunningX = 0;
            for (int j = 0; j < x11Context.hOutputCount; ++j)
            {
                aOutputs[j].x = iRunningX;
                aOutputs[j].y = aMonitors[j].yOrigin;
                aOutputs[j].width = aMonitors[j].cx;
                aOutputs[j].height = aMonitors[j].cy;
                aOutputs[j].fEnabled = !(aMonitors[j].fDisplayFlags & VMMDEV_DISPLAY_DISABLED);
                if (aOutputs[j].fEnabled)
                    iRunningX += aOutputs[j].width;
            }
            setXrandrTopology(aOutputs);
            /* Wait for some seconds and set toplogy again after the boot. In some desktop environments (cinnamon) where
               DE get into our resizing our first resize is reverted by the DE. Sleeping for some secs. helps. Setting
               topology a 2nd time resolves the black screen I get after resizing.*/
            if (fFirstRun)
            {
                sleep(4);
                setXrandrTopology(aOutputs);
                fFirstRun = false;
            }
        }
        do
        {
            rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, RT_INDEFINITE_WAIT, &events);
        } while (rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failure waiting for event, rc=%Rrc\n", rc);
    }
    cleanup();
}

static struct VBCLSERVICE interface =
{
    getName,
    getPidFilePath,
    VBClServiceDefaultHandler, /* Init */
    run,
    VBClServiceDefaultCleanup
}, *pInterface = &interface;

struct VBCLSERVICE **VBClDisplaySVGAX11Service()
{
    return &pInterface;
}
