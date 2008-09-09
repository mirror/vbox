/** @file
 * VirtualBox X11 Guest Additions, mouse driver for X.Org server 1.5
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
 * --------------------------------------------------------------------
 *
 * This code is based on evdev.c from X.Org with the following copyright
 * and permission notice:
 *
 * Copyright © 2004-2008 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Kristian Høgsberg (krh@redhat.com)
 *	Adam Jackson (ajax@redhat.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <VBox/VBoxGuest.h>
#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>

#include <xf86Module.h>

#include <errno.h>
#include <fcntl.h>

#ifdef DEBUG_michael
# define DEBUG_MOUSE 1
#endif

#ifdef DEBUG_MOUSE

#define TRACE \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": entering\n"); \
} while(0)
#define TRACE2 \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, ": leaving\n"); \
} while(0)
#define TRACE3(...) \
do { \
    xf86Msg(X_INFO, __PRETTY_FUNCTION__); \
    xf86Msg(X_INFO, __VA_ARGS__); \
} while(0)

#else  /* DEBUG_MOUSE not defined */

#define TRACE       do { } while(0)
#define TRACE2      do { } while(0)
#define TRACE3(...) do { } while(0)

#endif  /* DEBUG_MOUSE not defined */

#define BOOL_STR(a) ((a) ? "TRUE" : "FALSE")

typedef struct {
    int screen;
} VBoxRec, *VBoxPtr;

static const char *vboxDefaults[] = {
    NULL
};


static void
VBoxReadInput(InputInfoPtr pInfo)
{
    uint32_t cx, cy, fFeatures;
    TRACE;
    VBoxPtr pVBox = pInfo->private;
    int screenWidth = screenInfo.screens[pVBox->screen]->width;
    int screenHeight = screenInfo.screens[pVBox->screen]->height;

    if (RT_SUCCESS(VbglR3GetMouseStatus(&fFeatures, &cx, &cy)))
    {
        /* convert to screen resolution */
        int x, y;
        // x = (cx * screenWidth) / 65535;
        // y = (cy * screenHeight) / 65535;
        /* send absolute movement */
        // xf86PostMotionEvent(pInfo->dev, 1, 0, 2, x, y);
        xf86PostMotionEvent(pInfo->dev, 1, 0, 2, cx, cy);
    }
}

static void
VBoxPtrCtrlProc(DeviceIntPtr device, PtrCtrl *ctrl)
{
    /* Nothing to do, dix handles all settings */
}

static int
VBoxInit(DeviceIntPtr device)
{
    InputInfoPtr pInfo;
    VBoxPtr pVBox;
    int xrc;

    pInfo = device->public.devicePrivate;
    pVBox = pInfo->private;

    if (!InitValuatorClassDeviceStruct(device, 2, GetMotionHistory,
                                       GetMotionHistorySize(), Absolute)) {
        xf86Msg(X_ERROR, "%s: InitValuatorClassDeviceStruct failed\n",
                pInfo->name);
        return BadAlloc;
    }

    /* X valuator */
    xf86InitValuatorAxisStruct(device, 0, 0 /* min X */, 65536 /* max X */,
                               10000, 0, 10000);
    xf86InitValuatorDefaults(device, 0);

    /* Y valuator */
    xf86InitValuatorAxisStruct(device, 1, 0 /* min Y */, 65536 /* max Y */,
                               10000, 0, 10000);
    xf86InitValuatorDefaults(device, 1);
    xf86MotionHistoryAllocate(pInfo);

    if (!InitPtrFeedbackClassDeviceStruct(device, VBoxPtrCtrlProc)) {
        xf86Msg(X_ERROR, "%s: InitPtrFeedbackClassDeviceStruct failed\n",
                pInfo->name);
        return BadAlloc;
    }

    return Success;
}

static int
VBoxProc(DeviceIntPtr device, int what)
{
    InputInfoPtr pInfo;
    VBoxPtr pVBox;
    int rc, xrc;

    pInfo = device->public.devicePrivate;
    pVBox = pInfo->private;

    switch (what)
    {
    case DEVICE_INIT:
        xrc = VBoxInit(device);
        if (xrc != Success) {
            VbglR3Term();
            return xrc;
        }
        xf86Msg(X_CONFIG, "%s: Mouse Integration associated with screen %d\n",
                pInfo->name, pVBox->screen);
        break;

    case DEVICE_ON:
        xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
        if (device->public.on)
            break;
        /* Tell the host that we want absolute co-ordinates */
        rc = VbglR3SetMouseStatus(VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE);
        if (!RT_SUCCESS(rc)) {
            xf86Msg(X_ERROR, "%s: Failed to switch guest mouse into absolute mode\n",
                    pInfo->name);
            return !Success;
        }

        xf86AddEnabledDevice(pInfo);
	device->public.on = TRUE;
	break;
	    
    case DEVICE_OFF:
        xf86Msg(X_INFO, "%s: Off.\n", pInfo->name);
        VbglR3SetMouseStatus(0);
        xf86RemoveEnabledDevice(pInfo);
	device->public.on = FALSE;
	break;

    case DEVICE_CLOSE:
        VbglR3Term();
	xf86Msg(X_INFO, "%s: Close\n", pInfo->name);
	break;
    }

    return Success;
}

static int
VBoxProbe(InputInfoPtr pInfo)
{
    int rc = VbglR3Init();
    if (!RT_SUCCESS(rc)) {
        xf86Msg(X_ERROR, "%s: Failed to open the VirtualBox device (error %d)\n",
                pInfo->name, rc);
        return !Success;
    }

    return Success;
}

static InputInfoPtr
VBoxPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    InputInfoPtr pInfo;
    const char *device;
    VBoxPtr pVBox;

    if (!(pInfo = xf86AllocateInput(drv, 0)))
	return NULL;

    /* Initialise the InputInfoRec. */
    pInfo->name = dev->identifier;
    pInfo->device_control = VBoxProc;
    pInfo->read_input = VBoxReadInput;
    pInfo->conf_idev = dev;
    /* Unlike evdev, we set this unconditionally, as we don't handle keyboards. */
    pInfo->type_name = XI_MOUSE;
    pInfo->flags = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS |
            XI86_ALWAYS_CORE | XI86_OPEN_ON_INIT;

    if (!(pVBox = xcalloc(sizeof(*pVBox), 1)))
        return pInfo;
    pInfo->private = pVBox;

    xf86CollectInputOptions(pInfo, vboxDefaults, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options); 

    pVBox->screen = xf86SetIntOption(pInfo->options, "ScreenNumber", 0);

    device = xf86CheckStrOption(dev->commonOptions, "Path", "/dev/vboxadd");
    if (!device)
	device = xf86CheckStrOption(dev->commonOptions, "Device",
	                            "/dev/vboxadd");
    if (!device) {
        xf86Msg(X_ERROR, "%s: No device specified.\n", pInfo->name);
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }
	
    xf86Msg(X_CONFIG, "%s: Device: \"%s\"\n", pInfo->name, device);
    do {
        pInfo->fd = open(device, O_RDWR, 0);
    }
    while (pInfo->fd < 0 && errno == EINTR);

    if (pInfo->fd < 0) {
        xf86Msg(X_ERROR, "Unable to open VirtualBox device \"%s\".\n", device);
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    if (VBoxProbe(pInfo) != Success) {
	xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    pInfo->flags |= XI86_CONFIGURED;
    pInfo->flags |= XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS |
            XI86_ALWAYS_CORE | XI86_OPEN_ON_INIT | XI86_CONFIGURED;
    return pInfo;
}

_X_EXPORT InputDriverRec VBOXMOUSE = {
    1,
    "vboxmouse",
    NULL,
    VBoxPreInit,
    NULL,
    NULL,
    0
};

static void
VBoxUnplug(pointer	p)
{
}

static pointer
VBoxPlug(pointer	module,
          pointer	options,
          int		*errmaj,
          int		*errmin)
{
    xf86AddInputDriver(&VBOXMOUSE, module, 0);
    return module;
}

static XF86ModuleVersionInfo VBoxVersionRec =
{
    "vboxmouse",
    "Sun Microsystems Inc.",
    MODINFOSTRING1,
    MODINFOSTRING2,
    0, /* Missing from SDK: XORG_VERSION_CURRENT, */
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData vboxmouseModuleData =
{
    &VBoxVersionRec,
    VBoxPlug,
    VBoxUnplug
};
