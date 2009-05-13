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
 *      Kristian Høgsberg (krh@redhat.com)
 *      Adam Jackson (ajax@redhat.com)
 */

#include <VBox/VBoxGuest.h>
#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <mipointer.h>

#include <xf86Module.h>

#include <errno.h>
#include <fcntl.h>

static void
VBoxReadInput(InputInfoPtr pInfo)
{
    uint32_t cx, cy, fFeatures;

    /* The first test here is a workaround for an apparant bug in Xorg Server 1.5 */
    if (   miPointerGetScreen(pInfo->dev) != NULL
        && RT_SUCCESS(VbglR3GetMouseStatus(&fFeatures, &cx, &cy)))
        /* send absolute movement */
        xf86PostMotionEvent(pInfo->dev, 1, 0, 2, cx, cy);
}

static int
VBoxInit(DeviceIntPtr device)
{
    CARD8 map[2] = { 0, 1 };
    InputInfoPtr pInfo;

    pInfo = device->public.devicePrivate;

    if (!InitValuatorClassDeviceStruct(device, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
                                       GetMotionHistory,
#endif
                                       GetMotionHistorySize(), Absolute))
        return !Success;

    /* Pretend we have buttons so the server accepts us as a pointing device. */
    if (!InitButtonClassDeviceStruct(device, 2 /* number of buttons */, map))
        return !Success;

    /* Tell the server about the range of axis values we report */
    xf86InitValuatorAxisStruct(device, 0, 0 /* min X */, 65536 /* max X */,
                               10000, 0, 10000);
    xf86InitValuatorDefaults(device, 0);

    xf86InitValuatorAxisStruct(device, 1, 0 /* min Y */, 65536 /* max Y */,
                               10000, 0, 10000);
    xf86InitValuatorDefaults(device, 1);
    xf86MotionHistoryAllocate(pInfo);

    return Success;
}

static int
VBoxProc(DeviceIntPtr device, int what)
{
    InputInfoPtr pInfo;
    int rc, xrc;

    pInfo = device->public.devicePrivate;

    switch (what)
    {
    case DEVICE_INIT:
        xrc = VBoxInit(device);
        if (xrc != Success) {
            VbglR3Term();
            return xrc;
        }
        xf86Msg(X_CONFIG, "%s: Mouse Integration associated with screen %d\n",
                pInfo->name,
                xf86SetIntOption(pInfo->options, "ScreenNumber", 0));
        break;

    case DEVICE_ON:
        xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
        if (device->public.on)
            break;
        /* Tell the host that we want absolute co-ordinates */
        rc = VbglR3SetMouseStatus(  VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE
                                  | VBOXGUEST_MOUSE_GUEST_USES_VMMDEV);
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
            XI86_ALWAYS_CORE;

    xf86CollectInputOptions(pInfo, NULL, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options); 

    device = xf86CheckStrOption(dev->commonOptions, "Device",
                                "/dev/vboxadd");

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

static pointer
VBoxPlug(pointer module,
          pointer options,
          int *errmaj,
          int *errmin)
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
    NULL
};
