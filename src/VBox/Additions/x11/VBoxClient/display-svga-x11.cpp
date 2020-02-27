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
 */
#include <stdio.h>
#include "VBoxClient.h"

#include <VBox/VBoxGuestLib.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlibint.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>

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

#define OLD_JUNK

struct X11VMWRECT /* xXineramaScreenInfo in Xlib headers. */
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
    int hRandRMajor;
    int hVMWMajor;
    int hRandRMinor;
    int hRandREventBase;
    int hRandRErrorBase;
    int hEventMask;
    /** The number of outputs (monitors, including disconnect ones) xrandr reports. */
    int hOutputCount;
    Window rootWindow;
};

static X11CONTEXT x11Context;

#define MAX_MODE_NAME_LEN 64
#define MAX_COMMAND_LINE_LEN 512
#define MAX_MODE_LINE_LEN 512

static const char *szDefaultOutputNamePrefix = "Virtual";
static const char *pcszXrandr = "xrandr";
static const char *pcszCvt = "cvt";

struct DRMCONTEXT
{
    RTFILE hDevice;
};

struct RANDROUTPUT
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    bool fEnabled;
};

/** Forward declarations. */
static void x11Connect();
static int determineOutputCount();


#ifndef OLD_JUNK
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
    VbglR3SeamlessSendMonitorPositions(cPositions, pPositions);
}

static void queryMonitorPositions()
{
    static const int iSentinelPosition = -1;
    if (mpMonitorPositions)
    {
        free(mpMonitorPositions);
        mpMonitorPositions = NULL;
    }

    XRRScreenResources *pScreenResources = XRRGetScreenResources (x11Context.pDisplay, DefaultRootWindow(x11Context.pDisplay));
    AssertReturnVoid(pScreenResources);
    XRRFreeScreenResources (pScreenResources);

    int iMonitorCount = 0;
    XRRMonitorInfo *pMonitorInfo = XRRGetMonitors(x11Context.pDisplay, DefaultRootWindow(x11Context.pDisplay), true, &iMonitorCount);
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
            int iMonitorID = getMonitorIdFromName(XGetAtomName(x11Context.pDisplay, pMonitorInfo[i].name)) - 1;
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
    XRRFreeMonitors(pMonitorInfo);
}
#endif

static void monitorRandREvents()
{
#ifndef OLD_JUNK
    XEvent event;
    XNextEvent(x11Context.pDisplay, &event);
    int eventTypeOffset = event.type - x11Context.hRandREventBase;
    switch (eventTypeOffset)
    {
        case RRScreenChangeNotify:
            VBClLogInfo("RRScreenChangeNotify\n");
            queryMonitorPositions();
            break;
        case RRNotify:
            VBClLogInfo("RRNotify\n");
            break;
        default:
            VBClLogInfo("Unknown RR event: %d\n", event.type);
            break;
    }
#endif
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

static bool init()
{
    x11Connect();
    if (x11Context.pDisplay == NULL)
        return false;
    if (RT_FAILURE(startX11MonitorThread()))
        return false;
    XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, x11Context.hEventMask);
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
    XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, 0);
    XCloseDisplay(x11Context.pDisplay);
}

static void x11Connect()
{
    int dummy;
    if (x11Context.pDisplay != NULL)
        VBClLogFatalError("%s called with bad argument\n", __func__);
    x11Context.pDisplay = XOpenDisplay(NULL);
    if (x11Context.pDisplay == NULL)
        return;
    if (!XQueryExtension(x11Context.pDisplay, "VMWARE_CTRL",
                         &x11Context.hVMWMajor, &dummy, &dummy))
    {
        XCloseDisplay(x11Context.pDisplay);
        x11Context.pDisplay = NULL;
        return;
    }
    if (!XRRQueryExtension(x11Context.pDisplay, &x11Context.hRandREventBase, &x11Context.hRandRErrorBase))
    {
        XCloseDisplay(x11Context.pDisplay);
        x11Context.pDisplay = NULL;
        return;
    }
    if (!XRRQueryVersion(x11Context.pDisplay, &x11Context.hRandRMajor, &x11Context.hRandRMinor))
    {
        XCloseDisplay(x11Context.pDisplay);
        x11Context.pDisplay = NULL;
        return;
    }
    x11Context.hEventMask = 0;
#ifndef OLD_JUNK
    x11Context.hEventMask = RRScreenChangeNotifyMask;
    if (x11Context.hRandRMinor >= 2)
        x11Context.hEventMask |= RRCrtcChangeNotifyMask
                               | RROutputChangeNotifyMask
                               | RROutputPropertyNotifyMask;
#endif
    x11Context.rootWindow = DefaultRootWindow(x11Context.pDisplay);
    x11Context.hOutputCount = determineOutputCount();
}

/** run the xrandr command without options to get the total # of outputs (monitors) including
 *  disbaled ones. */
static int determineOutputCount()
{
    int iCount = 0;
    char szCommand[MAX_COMMAND_LINE_LEN];
    RTStrPrintf(szCommand, sizeof(szCommand), "%s ", pcszXrandr);

    FILE *pFile;
    pFile = popen(szCommand, "r");
    if (pFile == NULL)
    {
        VBClLogError("Failed to run %s\n", szCommand);
        return VMW_MAX_HEADS;
    }
    char szModeLine[MAX_COMMAND_LINE_LEN];
    while (fgets(szModeLine, sizeof(szModeLine), pFile) != NULL)
    {
        if (RTStrIStr(szModeLine, szDefaultOutputNamePrefix))
            ++iCount;
    }
    if (iCount == 0)
        iCount = VMW_MAX_HEADS;
    return iCount;
}

/** Parse a single line of the output of xrandr command to extract Mode name.
 * Assumed to be null terminated and in following format:
 * e.g. 1016x559_vbox  59.70. where 1016x559_vbox is the mode name and at least one space in front
 * Set  mode name @p outPszModeName and return true if the name can be found, false otherwise.
 * outPszModeNameis assumed to be of length MAX_MODE_NAME_LEN. */
static bool parseModeLine(char *pszLine, char *outPszModeName)
{
    char *p = pszLine;
    (void*)outPszModeName;
    /* Copy chars to outPszModeName starting from the first non-space until outPszModeName ends with 'vbox'*/
    size_t iNameIndex = 0;
    bool fInitialSpace = true;

    while (*p)
    {
        if (*p != ' ')
            fInitialSpace = false;
        if (!fInitialSpace && iNameIndex < MAX_MODE_NAME_LEN)
        {
            outPszModeName[iNameIndex] = *p;
            ++iNameIndex;
            if (iNameIndex >= 4)
            {
                if (   outPszModeName[iNameIndex-1] == 'x'
                    && outPszModeName[iNameIndex-2] == 'o'
                    && outPszModeName[iNameIndex-3] == 'b')
                    break;
            }
        }
        ++p;
    }
    outPszModeName[iNameIndex] = '\0';
    return true;
}

/** Parse the output of the xrandr command to try to remove custom modes.
 * This function assumes all the outputs are named as VirtualX. */
static void removeCustomModesFromOutputs()
{
    char szCommand[MAX_COMMAND_LINE_LEN];
    RTStrPrintf(szCommand, sizeof(szCommand), "%s ", pcszXrandr);

    FILE *pFile;
    pFile = popen(szCommand, "r");
    if (pFile == NULL)
    {
        VBClLogError("Failed to run %s\n", szCommand);
        return;
    }
    char szModeLine[MAX_COMMAND_LINE_LEN];
    char szModeName[MAX_MODE_NAME_LEN];
    int iCount = 0;
    char szRmModeCommand[MAX_COMMAND_LINE_LEN];
    char szDelModeCommand[MAX_COMMAND_LINE_LEN];

    while (fgets(szModeLine, sizeof(szModeLine), pFile) != NULL)
    {
        if (RTStrIStr(szModeLine, szDefaultOutputNamePrefix))
        {
            ++iCount;
            continue;
        }
        if (iCount > 0 && RTStrIStr(szModeLine, "_vbox"))
        {
            parseModeLine(szModeLine, szModeName);
            if (strlen(szModeName) >= 4)
            {
                /* Delete the mode from the outout. this fails if the mode is currently in use. */
                RTStrPrintf(szDelModeCommand, sizeof(szDelModeCommand), "%s --delmode Virtual%d %s", pcszXrandr, iCount, szModeName);
                system(szDelModeCommand);
                /* Delete the mode from the xserver. note that this will fail if some output has still has this mode (even if unused).
                 * thus this will fail most of the time. */
                RTStrPrintf(szRmModeCommand, sizeof(szRmModeCommand), "%s --rmmode %s", pcszXrandr, szModeName);
                system(szRmModeCommand);
            }
        }
    }
}

static void getModeNameAndLineFromCVT(int iWidth, int iHeight, char *pszOutModeName, char *pszOutModeLine)
{
    char szCvtCommand[MAX_COMMAND_LINE_LEN];
    const int iFreq = 60;
    const int iMinNameLen = 4;
    /* Make release builds happy. */
    (void)iMinNameLen;
    RTStrPrintf(szCvtCommand, sizeof(szCvtCommand), "%s %d %d %d", pcszCvt, iWidth, iHeight, iFreq);
    FILE *pFile;
    pFile = popen(szCvtCommand, "r");
    if (pFile == NULL)
    {
        VBClLogError("Failed to run %s\n", szCvtCommand);
        return;
    }

    char szModeLine[MAX_COMMAND_LINE_LEN];
    while (fgets(szModeLine, sizeof(szModeLine), pFile) != NULL)
    {
        if (RTStrStr(szModeLine, "Modeline"))
        {
            if(szModeLine[strlen(szModeLine) - 1] == '\n')
                szModeLine[strlen(szModeLine) - 1] = '\0';
            size_t iFirstQu = RTStrOffCharOrTerm(szModeLine, '\"');
            size_t iModeLineLen = strlen(szModeLine);
            /* Make release builds happy. */
            (void)iModeLineLen;
            Assert(iFirstQu < iModeLineLen - iMinNameLen);

            char *p = &(szModeLine[iFirstQu + 1]);
            size_t iSecondQu = RTStrOffCharOrTerm(p, '_');
            Assert(iSecondQu > iMinNameLen);
            Assert(iSecondQu < MAX_MODE_NAME_LEN);
            Assert(iSecondQu < iModeLineLen);

            RTStrCopy(pszOutModeName, iSecondQu + 2, p);
            RTStrCat(pszOutModeName, MAX_MODE_NAME_LEN, "vbox");
            iSecondQu = RTStrOffCharOrTerm(p, '\"');
            RTStrCopy(pszOutModeLine, MAX_MODE_LINE_LEN, &(szModeLine[iFirstQu + iSecondQu + 2]));
            break;
        }
    }
}

/** Add a new mode to xserver and to the output */
static void addMode(const char *pszModeName, const char *pszModeLine)
{
    char szNewModeCommand[MAX_COMMAND_LINE_LEN];
    RTStrPrintf(szNewModeCommand, sizeof(szNewModeCommand), "%s --newmode \"%s\" %s", pcszXrandr, pszModeName, pszModeLine);
    system(szNewModeCommand);

    char szAddModeCommand[1024];
    /* try to add the new mode to all possible outputs. we currently dont care if most the are disabled. */
    for(int i = 0; i < x11Context.hOutputCount; ++i)
    {
        RTStrPrintf(szAddModeCommand, sizeof(szAddModeCommand), "%s --addmode Virtual%d \"%s\"", pcszXrandr, i + 1, pszModeName);
        system(szAddModeCommand);
    }
}

static bool checkDefaultModes(struct RANDROUTPUT *pOutput, int iOutputIndex, char *pszModeName)
{
    const char szError[] = "cannot find mode";
    char szXranrCommand[MAX_COMMAND_LINE_LEN];
    RTStrPrintf(szXranrCommand, sizeof(szXranrCommand),
                "%s --dryrun --output Virtual%u --mode %dx%d --pos %dx%d 2>/dev/stdout", pcszXrandr, iOutputIndex,
                pOutput->width, pOutput->height, pOutput->x, pOutput->y);
    RTStrPrintf(pszModeName, MAX_MODE_NAME_LEN, "%dx%d", pOutput->width, pOutput->height);
    FILE *pFile;
    pFile = popen(szXranrCommand, "r");
    if (pFile == NULL)
    {
        VBClLogError("Failed to run %s\n", szXranrCommand);
        return false;
    }
    char szResult[64];
    if (fgets(szResult, sizeof(szResult), pFile) != NULL)
    {
        if (RTStrIStr(szResult, szError))
            return false;
    }
    return true;
}

/** Construct the xrandr command which sets the whole monitor topology each time. */
static void setXrandrModes(struct RANDROUTPUT *paOutputs)
{
    char szCommand[MAX_COMMAND_LINE_LEN];
    RTStrPrintf(szCommand, sizeof(szCommand), "%s ", pcszXrandr);

    for (int i = 0; i < x11Context.hOutputCount; ++i)
    {
        char line[64];
        if (!paOutputs[i].fEnabled)
            RTStrPrintf(line, sizeof(line), "--output Virtual%u --off ", i + 1);
        else
        {
            char szModeName[MAX_MODE_NAME_LEN];
            char szModeLine[MAX_MODE_LINE_LEN];
            /* Check if there is a default mode for the widthxheight and if not create and add a custom mode. */
            if (!checkDefaultModes(&(paOutputs[i]), i + 1, szModeName))
            {
                getModeNameAndLineFromCVT(paOutputs[i].width, paOutputs[i].height, szModeName, szModeLine);
                addMode(szModeName, szModeLine);
            }
            else
                RTStrPrintf(szModeName, sizeof(szModeName), "%dx%d ", paOutputs[i].width, paOutputs[i].height);

            RTStrPrintf(line, sizeof(line), "--output Virtual%u --mode %s --pos %dx%d ", i + 1,
                        szModeName, paOutputs[i].x, paOutputs[i].y);
        }
        RTStrCat(szCommand, sizeof(szCommand), line);
    }
    system(szCommand);
    VBClLogInfo("=======xrandr topology command:=====\n%s\n", szCommand);
    removeCustomModesFromOutputs();
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

    if (!init())
        return VINF_SUCCESS;
    static struct VMMDevDisplayDef aMonitors[VMW_MAX_HEADS];

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to request display change events, rc=%Rrc\n", rc);
    rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
    if (rc == VERR_RESOURCE_BUSY)  /* Someone else has already acquired it. */
        return VINF_SUCCESS;
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to register resizing support, rc=%Rrc\n", rc);

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
            setXrandrModes(aOutputs);
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
