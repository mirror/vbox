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

static bool checkRecentLinuxKernel(void)
{
    struct utsname name;

    if (uname(&name) == -1)
        VBClLogFatalError("Failed to get kernel name\n");
    if (strcmp(name.sysname, "Linux"))
        return false;
    return (RTStrVersionCompare(name.release, "4.6") >= 0);
}

struct X11CONTEXT
{
    Display *pDisplay;
    int hRandRMajor;
    int hVMWMajor;
};

static void x11Connect(struct X11CONTEXT *pContext)
{
    int dummy;

    if (pContext->pDisplay != NULL)
        VBClLogFatalError("%s called with bad argument\n", __func__);
    pContext->pDisplay = XOpenDisplay(NULL);
    if (pContext->pDisplay == NULL)
        return;
    if (   !XQueryExtension(pContext->pDisplay, "RANDR",
                            &pContext->hRandRMajor, &dummy, &dummy)
        || !XQueryExtension(pContext->pDisplay, "VMWARE_CTRL",
                            &pContext->hVMWMajor, &dummy, &dummy))
    {
        XCloseDisplay(pContext->pDisplay);
        pContext->pDisplay = NULL;
    }
}

#define X11_VMW_TOPOLOGY_REQ 2
struct X11VMWRECT /* xXineramaScreenInfo in Xlib headers. */
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
};
AssertCompileSize(struct X11VMWRECT, 8);

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
    (void)ppInterface;
    (void)fDaemonised;
    struct X11CONTEXT x11Context = { NULL };
    unsigned i;
    int rc;
    struct X11VMWRECT aRects[VMW_MAX_HEADS];
    unsigned cHeads;

    if (checkRecentLinuxKernel())
        return VINF_SUCCESS;
    x11Connect(&x11Context);
    if (x11Context.pDisplay == NULL)
        return VINF_SUCCESS;
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
        uint32_t events;
        struct VMMDevDisplayDef aDisplays[VMW_MAX_HEADS];
        uint32_t cDisplaysOut;

        /* Query the first size without waiting.  This lets us e.g. pick up
         * the last event before a guest reboot when we start again after. */
        rc = VbglR3GetDisplayChangeRequestMulti(VMW_MAX_HEADS, &cDisplaysOut, aDisplays, true);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failed to get display change request, rc=%Rrc\n", rc);
        if (cDisplaysOut > VMW_MAX_HEADS)
            VBClLogFatalError("Display change request contained, rc=%Rrc\n", rc);
        for (i = 0, cHeads = 0; i < cDisplaysOut && i < VMW_MAX_HEADS; ++i)
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
        rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, RT_INDEFINITE_WAIT, &events);
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
