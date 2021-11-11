/* $Id$ */
/** @file
 * X11 guest client - VMSVGA emulation resize event pass-through to drm guest
 * driver.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

#ifdef RT_OS_LINUX
# include <sys/ioctl.h>
#else  /* Solaris and BSDs, in case they ever adopt the DRM driver. */
# include <sys/ioccom.h>
#endif

/** Ioctl command to query vmwgfx version information. */
#define DRM_IOCTL_VERSION               _IOWR('d', 0x00, struct DRMVERSION)
/** Ioctl command to set new screen layout. */
#define DRM_IOCTL_VMW_UPDATE_LAYOUT     _IOW('d', 0x40 + 20, struct DRMVMWUPDATELAYOUT)
/** A driver name which identifies VMWare driver. */
#define DRM_DRIVER_NAME "vmwgfx"
/** VMWare driver compatible version number. On previous versions resizing does not seem work. */
#define DRM_DRIVER_VERSION_MAJOR_MIN    (2)
#define DRM_DRIVER_VERSION_MINOR_MIN    (10)

/** VMWare char device driver minor numbers range. */
#define VMW_CONTROL_DEVICE_MINOR_START  (64)
#define VMW_RENDER_DEVICE_MINOR_START   (128)
#define VMW_RENDER_DEVICE_MINOR_END     (192)

/** Maximum number of supported screens.  DRM and X11 both limit this to 32. */
/** @todo if this ever changes, dynamically allocate resizeable arrays in the
 *  context structure. */
#define VMW_MAX_HEADS                   (32)

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

/** Rectangle structure for geometry of a single screen. */
struct DRMVMWRECT
{
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
};
AssertCompileSize(struct DRMVMWRECT, 16);

/** Preferred screen layout information for DRM_VMW_UPDATE_LAYOUT IoCtl.  The
 *  rects argument is a cast pointer to an array of drm_vmw_rect. */
struct DRMVMWUPDATELAYOUT
{
    uint32_t cOutputs;
    uint32_t u32Pad;
    uint64_t ptrRects;
};
AssertCompileSize(struct DRMVMWUPDATELAYOUT, 16);

/** These two parameters are mostly unused. Defined here in order to satisfy linking requirements. */
unsigned g_cRespawn = 0;
unsigned g_cVerbosity = 0;

/**
 * Attempts to open DRM device by given path and check if it is
 * compatible for screen resize.
 *
 * @return  DRM device handle on success or NIL_RTFILE otherwise.
 * @param   szPathPattern       Path name pattern to the DRM device.
 * @param   uInstance           Driver / device instance.
 */
static RTFILE drmTryDevice(const char *szPathPattern, uint8_t uInstance)
{
    int rc = VERR_NOT_FOUND;
    char szPath[PATH_MAX];
    struct DRMVERSION vmwgfxVersion;
    RTFILE hDevice = NIL_RTFILE;

    RT_ZERO(szPath);
    RT_ZERO(vmwgfxVersion);

    rc = RTStrPrintf(szPath, sizeof(szPath), szPathPattern, uInstance);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileOpen(&hDevice, szPath, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            char szVmwgfxDriverName[sizeof(DRM_DRIVER_NAME)];
            RT_ZERO(szVmwgfxDriverName);

            vmwgfxVersion.cbName = sizeof(szVmwgfxDriverName);
            vmwgfxVersion.pszName = szVmwgfxDriverName;

            /* Query driver version information and check if it can be used for screen resizing. */
            rc = RTFileIoCtl(hDevice, DRM_IOCTL_VERSION, &vmwgfxVersion, sizeof(vmwgfxVersion), NULL);
            if (   RT_SUCCESS(rc)
                && strncmp(szVmwgfxDriverName, DRM_DRIVER_NAME, sizeof(DRM_DRIVER_NAME) - 1) == 0
                && (   vmwgfxVersion.cMajor >= DRM_DRIVER_VERSION_MAJOR_MIN
                    || (   vmwgfxVersion.cMajor == DRM_DRIVER_VERSION_MAJOR_MIN
                        && vmwgfxVersion.cMinor >= DRM_DRIVER_VERSION_MINOR_MIN)))
            {
                VBClLogInfo("VBoxDRMClient: found compatible device: %s\n", szPath);
            }
            else
            {
                RTFileClose(hDevice);
                hDevice = NIL_RTFILE;
                rc = VERR_NOT_FOUND;
            }
        }
    }
    else
    {
        VBClLogError("VBoxDRMClient: unable to construct path to DRM device: %Rrc\n", rc);
    }

    return RT_SUCCESS(rc) ? hDevice : NIL_RTFILE;
}

/**
 * Attempts to find and open DRM device to be used for screen resize.
 *
 * @return  DRM device handle on success or NIL_RTFILE otherwise.
 */
static RTFILE drmOpenVmwgfx(void)
{
    /* Control devices for drm graphics driver control devices go from
     * controlD64 to controlD127.  Render node devices go from renderD128
     * to renderD192. The driver takes resize hints via the control device
     * on pre-4.10 (???) kernels and on the render device on newer ones.
     * At first, try to find control device and render one if not found.
     */
    uint8_t i;
    RTFILE hDevice = NIL_RTFILE;

    /* Lookup control device. */
    for (i = VMW_CONTROL_DEVICE_MINOR_START; i < VMW_RENDER_DEVICE_MINOR_START; i++)
    {
        hDevice = drmTryDevice("/dev/dri/controlD%u", i);
        if (hDevice != NIL_RTFILE)
            return hDevice;
    }

    /* Lookup render device. */
    for (i = VMW_RENDER_DEVICE_MINOR_START; i <= VMW_RENDER_DEVICE_MINOR_END; i++)
    {
        hDevice = drmTryDevice("/dev/dri/renderD%u", i);
        if (hDevice != NIL_RTFILE)
            return hDevice;
    }

    VBClLogError("VBoxDRMClient: unable to find DRM device\n");

    return hDevice;
}

static void drmSendHints(RTFILE hDevice, struct DRMVMWRECT *paRects, unsigned cHeads)
{
    uid_t curuid = getuid();
    if (setreuid(0, 0) != 0)
        perror("setreuid failed during drm ioctl.");
    int rc;
    struct DRMVMWUPDATELAYOUT ioctlLayout;

    ioctlLayout.cOutputs = cHeads;
    ioctlLayout.ptrRects = (uint64_t)paRects;
    rc = RTFileIoCtl(hDevice, DRM_IOCTL_VMW_UPDATE_LAYOUT,
                     &ioctlLayout, sizeof(ioctlLayout), NULL);
    if (RT_FAILURE(rc) && rc != VERR_INVALID_PARAMETER)
        VBClLogFatalError("Failure updating layout, rc=%Rrc\n", rc);
    if (setreuid(curuid, 0) != 0)
        perror("reset of setreuid failed after drm ioctl.");
}

int main(int argc, char *argv[])
{
    RTFILE hDevice = NIL_RTFILE;
    static struct VMMDevDisplayDef aMonitors[VMW_MAX_HEADS];
    unsigned cEnabledMonitors;
    /* Do not acknowledge the first event we query for to pick up old events,
     * e.g. from before a guest reboot. */
    bool fAck = false;

    /** The name and handle of the PID file. */
    static const char szPidFile[RTPATH_MAX] = "/var/run/VBoxDRMClient";
    RTFILE hPidFile;

    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClLogFatalError("VbglR3InitUser failed: %Rrc", rc);

    /* Check PID file before attempting to initialize anything. */
    rc = VbglR3PidFile(szPidFile, &hPidFile);
    if (rc == VERR_FILE_LOCK_VIOLATION)
    {
        VBClLogInfo("VBoxDRMClient: already running, exiting\n");
        return RTEXITCODE_SUCCESS;
    }
    else if (RT_FAILURE(rc))
    {
        VBClLogError("VBoxDRMClient: unable to lock PID file (%Rrc), exiting\n", rc);
        return RTEXITCODE_FAILURE;
    }

    hDevice = drmOpenVmwgfx();
    if (hDevice == NIL_RTFILE)
        return VERR_OPEN_FAILED;

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        VBClLogFatalError("Failed to request display change events, rc=%Rrc\n", rc);
        return VERR_INVALID_HANDLE;
    }
    rc = VbglR3AcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, false);
    if (rc == VERR_RESOURCE_BUSY)  /* Someone else has already acquired it. */
    {
        return VERR_RESOURCE_BUSY;
    }
    if (RT_FAILURE(rc))
    {
        VBClLogFatalError("Failed to register resizing support, rc=%Rrc\n", rc);
        return VERR_INVALID_HANDLE;
    }

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
            VBClLogError("Failed to get display change request, rc=%Rrc\n", rc);
        if (cDisplaysOut > VMW_MAX_HEADS)
            VBClLogError("Display change request contained, rc=%Rrc\n", rc);
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
            for (unsigned i = 0; i < cEnabledMonitors; ++i)
                printf("Monitor %u: %dx%d, (%d, %d)\n", i, (int)aEnabledMonitors[i].w, (int)aEnabledMonitors[i].h,
                       (int)aEnabledMonitors[i].x, (int)aEnabledMonitors[i].y);
            drmSendHints(hDevice, aEnabledMonitors, cEnabledMonitors);
        }
        do
        {
            rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, RT_INDEFINITE_WAIT, &events);
        } while (rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failure waiting for event, rc=%Rrc\n", rc);
    }

    /** @todo this code never executed since we do not have yet a clean way to exit
     * main event loop above. */

    RTFileClose(hDevice);

    VBClLogInfo("VBoxDRMClient: releasing PID file lock\n");
    VbglR3ClosePidFile(szPidFile, hPidFile);

    return 0;
}
