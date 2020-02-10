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
#include "stdio.h"
#include "VBoxClient.h"

#include <VBox/VBoxGuestLib.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include <sys/utsname.h>

/** Maximum number of supported screens.  DRM and X11 both limit this to 32. */
/** @todo if this ever changes, dynamically allocate resizeable arrays in the
 *  context structure. */
#define VMW_MAX_HEADS 32

/* VMWare X.Org driver control parts definitions. */

#include <X11/Xlibint.h>
#include <X11/extensions/Xrandr.h>

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
#else  /* Solaris and BSDs, in case they ever adopt the DRM driver. */
# include <sys/ioccom.h>
#endif

#define USE_XRANDR_BIN


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
    Window rootWindow;
};

static void x11Connect(struct X11CONTEXT *pContext)
{
    int dummy;

    if (pContext->pDisplay != NULL)
        VBClLogFatalError("%s called with bad argument\n", __func__);
    pContext->pDisplay = XOpenDisplay(NULL);
    if (pContext->pDisplay == NULL)
        return;
    // if (   !XQueryExtension(pContext->pDisplay, "RANDR",
    //                         &pContext->hRandRMajor, &dummy, &dummy)
    if(!XQueryExtension(pContext->pDisplay, "VMWARE_CTRL",
                        &pContext->hVMWMajor, &dummy, &dummy))
    {
        XCloseDisplay(pContext->pDisplay);
        pContext->pDisplay = NULL;
    }
    if (!XRRQueryExtension(pContext->pDisplay, &pContext->hRandREventBase, &pContext->hRandRErrorBase))
    {
        XCloseDisplay(pContext->pDisplay);
        pContext->pDisplay = NULL;
    }
    if (!XRRQueryVersion(pContext->pDisplay, &pContext->hRandRMajor, &pContext->hRandRMinor))
    {
        XCloseDisplay(pContext->pDisplay);
        pContext->pDisplay = NULL;
    }
    pContext->rootWindow = DefaultRootWindow(pContext->pDisplay);
}

#ifndef USE_XRANDR_BIN

static bool checkRecentLinuxKernel(void)
{
    struct utsname name;

    if (uname(&name) == -1)
        VBClLogFatalError("Failed to get kernel name\n");
    if (strcmp(name.sysname, "Linux"))
        return false;
    return (RTStrVersionCompare(name.release, "4.6") >= 0);
}


#define X11_VMW_TOPOLOGY_REQ 2

struct X11REQHEADER
{
    uint8_t hMajor;
    uint8_t idType;
    uint16_t cd;
};

struct X11VMWTOPOLOGYREQ
{
    struct X11REQHEADER header;
    uint32_t idX11Screen;
    uint32_t cScreens;
    uint32_t u32Pad;
    struct X11VMWRECT aRects[1];
};
AssertCompileSize(struct X11VMWTOPOLOGYREQ, 24);

#define X11_VMW_TOPOLOGY_REPLY_SIZE 32

#define X11_VMW_RESOLUTION_REQUEST 1
struct X11VMWRESOLUTIONREQ
{
    struct X11REQHEADER header;
    uint32_t idX11Screen;
    uint32_t w;
    uint32_t h;
};
AssertCompileSize(struct X11VMWRESOLUTIONREQ, 16);

#define X11_VMW_RESOLUTION_REPLY_SIZE 32

#define X11_RANDR_GET_SCREEN_REQUEST 5
struct X11RANDRGETSCREENREQ
{
    struct X11REQHEADER header;
    uint32_t hWindow;
};
AssertCompileSize(struct X11RANDRGETSCREENREQ, 8);

#define X11_RANDR_GET_SCREEN_REPLY_SIZE 32

/* This was a macro in old Xlib versions and a function in newer ones; the
 * display members touched by the macro were declared as ABI for compatibility
 * reasons.  To simplify building with different generations, we duplicate the
 * code. */
static void x11GetRequest(struct X11CONTEXT *pContext, uint8_t hMajor,
                          uint8_t idType, size_t cb, struct X11REQHEADER **ppReq)
{
    if (pContext->pDisplay->bufptr + cb > pContext->pDisplay->bufmax)
        _XFlush(pContext->pDisplay);
    if (pContext->pDisplay->bufptr + cb > pContext->pDisplay->bufmax)
        VBClLogFatalError("%s display buffer overflow\n", __func__);
    if (cb % 4 != 0)
        VBClLogFatalError("%s bad parameter\n", __func__);
    pContext->pDisplay->last_req = pContext->pDisplay->bufptr;
    *ppReq = (struct X11REQHEADER *)pContext->pDisplay->bufptr;
    (*ppReq)->hMajor = hMajor;
    (*ppReq)->idType = idType;
    (*ppReq)->cd = cb / 4;
    pContext->pDisplay->bufptr += cb;
    pContext->pDisplay->request++;
}

static void x11SendHints(struct X11CONTEXT *pContext, struct X11VMWRECT *pRects,
                         unsigned cRects)
{
    unsigned i;
    struct X11VMWTOPOLOGYREQ     *pReqTopology;
    uint8_t                       repTopology[X11_VMW_TOPOLOGY_REPLY_SIZE];
    struct X11VMWRESOLUTIONREQ   *pReqResolution;
    uint8_t                       repResolution[X11_VMW_RESOLUTION_REPLY_SIZE];

    if (!VALID_PTR(pContext->pDisplay))
        VBClLogFatalError("%s bad display argument\n", __func__);
    if (cRects == 0)
        return;
    /* Try a topology (multiple screen) request. */
    x11GetRequest(pContext, pContext->hVMWMajor, X11_VMW_TOPOLOGY_REQ,
                    sizeof(struct X11VMWTOPOLOGYREQ)
                  + sizeof(struct X11VMWRECT) * (cRects - 1),
                  (struct X11REQHEADER **)&pReqTopology);
    pReqTopology->idX11Screen = DefaultScreen(pContext->pDisplay);
    pReqTopology->cScreens = cRects;
    for (i = 0; i < cRects; ++i)
        pReqTopology->aRects[i] = pRects[i];
    _XSend(pContext->pDisplay, NULL, 0);
    if (_XReply(pContext->pDisplay, (xReply *)&repTopology, 0, xTrue))
        return;
    /* That failed, so try the old single screen set resolution.  We prefer
     * simpler code to negligeably improved efficiency, so we just always try
     * both requests instead of doing version checks or caching. */
    x11GetRequest(pContext, pContext->hVMWMajor, X11_VMW_RESOLUTION_REQUEST,
                  sizeof(struct X11VMWRESOLUTIONREQ),
                  (struct X11REQHEADER **)&pReqResolution);
    pReqResolution->idX11Screen = DefaultScreen(pContext->pDisplay);
    pReqResolution->w = pRects[0].w;
    pReqResolution->h = pRects[0].h;
    if (_XReply(pContext->pDisplay, (xReply *)&repResolution, 0, xTrue))
        return;
    /* What now? */
    VBClLogFatalError("%s failed to set resolution\n", __func__);
}

/** Call RRGetScreenInfo to wake up the server to the new modes. */
static void x11GetScreenInfo(struct X11CONTEXT *pContext)
{
    struct X11RANDRGETSCREENREQ *pReqGetScreen;
    uint8_t                      repGetScreen[X11_RANDR_GET_SCREEN_REPLY_SIZE];

    if (!VALID_PTR(pContext->pDisplay))
        VBClLogFatalError("%s bad display argument\n", __func__);
    x11GetRequest(pContext, pContext->hRandRMajor, X11_RANDR_GET_SCREEN_REQUEST,
                    sizeof(struct X11RANDRGETSCREENREQ),
                  (struct X11REQHEADER **)&pReqGetScreen);
    pReqGetScreen->hWindow = DefaultRootWindow(pContext->pDisplay);
    _XSend(pContext->pDisplay, NULL, 0);
    if (!_XReply(pContext->pDisplay, (xReply *)&repGetScreen, 0, xTrue))
        VBClLogFatalError("%s failed to set resolution\n", __func__);
}

#else //#fndef USE_XRANDR_BIN

#define MAX_MODE_NAME_LEN 64
#define MAX_COMMAND_LINE_LEN 512
#define MAX_MODE_LINE_LEN 512

static const char *szDefaultOutputNamePrefix = "Virtual";
static const char *pcszXrandr = "xrandr";
static const char *pcszCvt = "cvt";
/** The number of outputs (monitors, including disconnect ones) xrandr reports. */
static int iOutputCount = 0;

struct DRMCONTEXT
{
    RTFILE hDevice;
};

#define DRM_DRIVER_NAME "vmwgfx"

/** DRM version structure. */
struct DRMVERSION
{
    int cMajor;
    int cMinor;
    int cPatchLevel;
    size_t cbName;
    char *pszName;
    size_t cbDate;
    char *pszDate;
    size_t cbDescription;
    char *pszDescription;
};
AssertCompileSize(struct DRMVERSION, 8 + 7 * sizeof(void *));

#define DRM_IOCTL_VERSION _IOWR('d', 0x00, struct DRMVERSION)

/** Rectangle structure for geometry of a single screen. */
struct DRMVMWRECT
{
        int32_t x;
        int32_t y;
        uint32_t w;
        uint32_t h;
};
AssertCompileSize(struct DRMVMWRECT, 16);

struct DRMVMWUPDATELAYOUT {
        uint32_t cOutputs;
        uint32_t u32Pad;
        uint64_t ptrRects;
};
AssertCompileSize(struct DRMVMWUPDATELAYOUT, 16);

#define DRM_IOCTL_VMW_UPDATE_LAYOUT \
        _IOW('d', 0x40 + 20, struct DRMVMWUPDATELAYOUT)

struct RANDROUTPUT
{
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    bool fEnabled;
};

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

    while(*p)
    {
        if (*p != ' ')
            fInitialSpace = false;
        if (!fInitialSpace && iNameIndex < MAX_MODE_NAME_LEN)
        {
            outPszModeName[iNameIndex] = *p;
            ++iNameIndex;
            if (iNameIndex >= 4)
            {
                if (outPszModeName[iNameIndex-1] == 'x' &&
                    outPszModeName[iNameIndex-2] == 'o' &&
                    outPszModeName[iNameIndex-3] == 'b')
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
            /* Some gcc's complain about unused variable. */
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
    for(int i = 0; i < iOutputCount; ++i)
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

    for (int i = 0; i < iOutputCount; ++i)
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

static void drmConnect(struct DRMCONTEXT *pContext)
{
    unsigned i;
    RTFILE hDevice;

    if (pContext->hDevice != NIL_RTFILE)
        VBClLogFatalError("%s called with bad argument\n", __func__);
    /* Try to open the SVGA DRM device. */
    for (i = 0; i < 128; ++i)
    {
        char szPath[64];
        struct DRMVERSION version;
        char szName[sizeof(DRM_DRIVER_NAME)];
        int rc;

        /* Control devices for drm graphics driver control devices go from
         * controlD64 to controlD127.  Render node devices go from renderD128
         * to renderD192.  The driver takes resize hints via the control device
         * on pre-4.10 kernels and on the render device on newer ones.  Try
         * both types. */
        if (i % 2 == 0)
            rc = RTStrPrintf(szPath, sizeof(szPath), "/dev/dri/renderD%u", i / 2 + 128);
        else
            rc = RTStrPrintf(szPath, sizeof(szPath), "/dev/dri/controlD%u", i / 2 + 64);
        if (RT_FAILURE(rc))
            VBClLogFatalError("RTStrPrintf of device path failed, rc=%Rrc\n", rc);
        rc = RTFileOpen(&hDevice, szPath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_FAILURE(rc))
            continue;
        RT_ZERO(version);
        version.cbName = sizeof(szName);
        version.pszName = szName;
        rc = RTFileIoCtl(hDevice, DRM_IOCTL_VERSION, &version, sizeof(version), NULL);
        if (   RT_SUCCESS(rc)
            && !strncmp(szName, DRM_DRIVER_NAME, sizeof(DRM_DRIVER_NAME) - 1)
            && (   version.cMajor > 2
                || (version.cMajor == 2 && version.cMinor >= 9)))
            break;
        hDevice = NIL_RTFILE;
    }
    pContext->hDevice = hDevice;
}

static void drmSendHints(struct DRMCONTEXT *pContext, struct DRMVMWRECT *paRects,
                         unsigned cHeads)
{
    int rc;
    struct DRMVMWUPDATELAYOUT ioctlLayout;

    if (pContext->hDevice == NIL_RTFILE)
        VBClLogFatalError("%s bad device argument\n", __func__);
    ioctlLayout.cOutputs = cHeads;
    ioctlLayout.ptrRects = (uint64_t)paRects;
    rc = RTFileIoCtl(pContext->hDevice, DRM_IOCTL_VMW_UPDATE_LAYOUT,
                     &ioctlLayout, sizeof(ioctlLayout), NULL);
    if (RT_FAILURE(rc) && rc != VERR_INVALID_PARAMETER)
        VBClLogFatalError("Failure updating layout, rc=%Rrc\n", rc);
}

#endif /* #ifndef USE_XRANDR_BIN */

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
    iOutputCount = determineOutputCount();
    (void)ppInterface;
    (void)fDaemonised;
    int rc;
    /* Do not acknowledge the first event we query for to pick up old events,
     * e.g. from before a guest reboot. */
    bool fAck = false;
    struct X11CONTEXT x11Context = { NULL };
    x11Connect(&x11Context);
    if (x11Context.pDisplay == NULL)
        return VINF_SUCCESS;

#ifndef USE_XRANDR_BIN
    unsigned cHeads;
    struct X11VMWRECT aRects[VMW_MAX_HEADS];
    if (checkRecentLinuxKernel())
        return VINF_SUCCESS;
#else //#ifndef USE_XRANDR_BIN
    static struct VMMDevDisplayDef aMonitors[VMW_MAX_HEADS];
    struct DRMCONTEXT drmContext = { NIL_RTFILE };
    drmConnect(&drmContext);
    if (drmContext.hDevice == NIL_RTFILE)
        return VINF_SUCCESS;
    unsigned cEnabledMonitors = 0;
#endif

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to request display change events, rc=%Rrc\n", rc);
    rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
    if (rc == VERR_RESOURCE_BUSY)  /* Someone else has already acquired it. */
        return VINF_SUCCESS;
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failed to register resizing support, rc=%Rrc\n", rc);

    int eventMask = RRScreenChangeNotifyMask;
    if (x11Context.hRandRMinor >= 2)
        eventMask |=  RRCrtcChangeNotifyMask |
            RROutputChangeNotifyMask |
            RROutputPropertyNotifyMask;
    if (x11Context.hRandRMinor >= 4)
        eventMask |= RRProviderChangeNotifyMask |
            RRProviderPropertyNotifyMask |
            RRResourceChangeNotifyMask;
    // if (x11Context.hRandRMinor >= 6)
    //     eventMask |= RRLeaseNotifyMask;


    for (;;)
    {
        uint32_t events;
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
#ifndef USE_XRANDR_BIN
        for (unsigned i = 0, cHeads = 0; i < cDisplaysOut && i < VMW_MAX_HEADS; ++i)
        {
            if (!(aDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
            {
                if ((i == 0) || (aDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_ORIGIN))
                {
                    aRects[cHeads].x =   aDisplays[i].xOrigin < INT16_MAX
                                       ? (int16_t)aDisplays[i].xOrigin : 0;
                    aRects[cHeads].y =   aDisplays[i].yOrigin < INT16_MAX
                                       ? (int16_t)aDisplays[i].yOrigin : 0;
                } else {
                    aRects[cHeads].x = aRects[cHeads - 1].x + aRects[cHeads - 1].w;
                    aRects[cHeads].y = aRects[cHeads - 1].y;
                }
                aRects[cHeads].w = (int16_t)RT_MIN(aDisplays[i].cx, INT16_MAX);
                aRects[cHeads].h = (int16_t)RT_MIN(aDisplays[i].cy, INT16_MAX);
                ++cHeads;
            }
        }
        x11SendHints(&x11Context, aRects, cHeads);
        x11GetScreenInfo(&x11Context);
#else
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
                    if ((idDisplay == 0) || (aDisplays[i].fDisplayFlags & VMMDEV_DISPLAY_ORIGIN))
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
            for (int j = 0; j < iOutputCount; ++j)
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
            /* Disable this for now since we want to fix messed up monitor positions in some other way. */
            if(0)
            {
                /* Additionally update the drm thru ioctl since vmwgfx messes up monitor positions it gets from xserver. */
                /* Create an dense (consisting of enabled monitors only) array to pass to DRM. */
                cEnabledMonitors = 0;
                struct DRMVMWRECT aEnabledMonitors[VMW_MAX_HEADS];
                for (int j = 0; j < VMW_MAX_HEADS; ++j)
                {
                    if (!(aMonitors[j].fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
                    {
                        aEnabledMonitors[cEnabledMonitors].x = aMonitors[j].xOrigin;
                        aEnabledMonitors[cEnabledMonitors].y = aMonitors[j].yOrigin;
                        aEnabledMonitors[cEnabledMonitors].w = aMonitors[j].cx;
                        aEnabledMonitors[cEnabledMonitors].h = aMonitors[j].cy;
                        if (cEnabledMonitors > 0)
                            aEnabledMonitors[cEnabledMonitors].x = aEnabledMonitors[cEnabledMonitors - 1].x + aEnabledMonitors[cEnabledMonitors - 1].w;
                        ++cEnabledMonitors;
                    }
                }
                bool fScreenChangeNotifyEvent = false;
                XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, eventMask);
                XSync(x11Context.pDisplay, false);
                XEvent event;
                while (XCheckTypedEvent(x11Context.pDisplay, RRScreenChangeNotify + x11Context.hRandREventBase, &event) ||
                       XCheckTypedEvent(x11Context.pDisplay, RRNotify + x11Context.hRandREventBase, &event))
                    fScreenChangeNotifyEvent = true;
                XRRSelectInput(x11Context.pDisplay, x11Context.rootWindow, 0);

                if (!fScreenChangeNotifyEvent)
                    VBClLogInfo("Did not receive any RRScreenChangeNotify events.\n");
                else
                    drmSendHints(&drmContext, aEnabledMonitors, cEnabledMonitors);
            }
        }

#endif
        do
        {
            rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, RT_INDEFINITE_WAIT, &events);
        } while (rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failure waiting for event, rc=%Rrc\n", rc);
    }
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
