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

/** Path to the PID file. */
static const char g_szPidFile[RTPATH_MAX] = "/var/run/VBoxDRMClient";

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

/**
 * This function converts input monitors layout array passed from DevVMM
 * into monitors layout array to be passed to DRM stack.
 *
 * @return  VINF_SUCCESS on success, IPRT error code otherwise.
 * @param   aDisplaysIn         Input displays array.
 * @param   cDisplaysIn         Number of elements in input displays array.
 * @param   aDisplaysOut        Output displays array.
 * @param   cDisplaysOutMax     Number of elements in output displays array.
 * @param   pcActualDisplays    Number of displays to report to DRM stack (number of enabled displays).
 */
static int drmValidateLayout(VMMDevDisplayDef *aDisplaysIn, uint32_t cDisplaysIn,
                             struct DRMVMWRECT *aDisplaysOut, uint32_t cDisplaysOutMax, uint32_t *pcActualDisplays)
{
    /* This array is a cache of what was received from DevVMM so far.
     * DevVMM may send to us partial information bout scree layout. This
     * cache remembers entire picture. */
    static struct VMMDevDisplayDef aVmMonitorsCache[VMW_MAX_HEADS];
    /* Number of valid (enabled) displays in output array. */
    uint32_t cDisplaysOut = 0;
    /* Flag indicates that current layout cache is consistent and can be passed to DRM stack. */
    bool fValid = true;

    /* Make sure input array fits cache size. */
    if (cDisplaysIn > VMW_MAX_HEADS)
    {
        VBClLogError("VBoxDRMClient: unable to validate screen layout: input (%u) array does not fit to cache size (%u)\n",
                     cDisplaysIn, VMW_MAX_HEADS);
        return VERR_INVALID_PARAMETER;
    }

    /* Make sure there is enough space in output array. */
    if (cDisplaysIn > cDisplaysOutMax)
    {
        VBClLogError("VBoxDRMClient: unable to validate screen layout: input array (%u) is bigger than output one (%u)\n",
                     cDisplaysIn, cDisplaysOut);
        return VERR_INVALID_PARAMETER;
    }

    /* Make sure input and output arrays are of non-zero size. */
    if (!(cDisplaysIn > 0 && cDisplaysOutMax > 0))
    {
        VBClLogError("VBoxDRMClient: unable to validate screen layout: invalid size of either input (%u) or output display array\n",
                     cDisplaysIn, cDisplaysOutMax);
        return VERR_INVALID_PARAMETER;
    }

    /* Update cache. */
    for (uint32_t i = 0; i < cDisplaysIn; i++)
    {
        uint32_t idDisplay = aDisplaysIn[i].idDisplay;
        if (idDisplay < VMW_MAX_HEADS)
        {
            aVmMonitorsCache[idDisplay].idDisplay = idDisplay;
            aVmMonitorsCache[idDisplay].fDisplayFlags = aDisplaysIn[i].fDisplayFlags;
            aVmMonitorsCache[idDisplay].cBitsPerPixel = aDisplaysIn[i].cBitsPerPixel;
            aVmMonitorsCache[idDisplay].cx = aDisplaysIn[i].cx;
            aVmMonitorsCache[idDisplay].cy = aDisplaysIn[i].cy;
            aVmMonitorsCache[idDisplay].xOrigin = aDisplaysIn[i].xOrigin;
            aVmMonitorsCache[idDisplay].yOrigin = aDisplaysIn[i].yOrigin;
        }
        else
        {
            VBClLogError("VBoxDRMClient: received display ID (0x%x, position %u) is invalid\n", idDisplay, i);
            /* If monitor configuration cannot be placed into cache, consider entire cache is invalid. */
            fValid = false;
        }
    }

    /* Now, go though complete cache and check if it is valid. */
    for (uint32_t i = 0; i < VMW_MAX_HEADS; i++)
    {
        if (i == 0)
        {
            if (aVmMonitorsCache[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED)
            {
                VBClLogError("VBoxDRMClient: unable to validate screen layout: first monitor is not allowed to be disabled");
                fValid = false;
            }
            else
                cDisplaysOut++;
        }
        else
        {
            /* Check if there is no hole in between monitors (i.e., if current monitor is enabled, but privious one does not). */
            if (   !(aVmMonitorsCache[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED)
                && aVmMonitorsCache[i - 1].fDisplayFlags & VMMDEV_DISPLAY_DISABLED)
            {
                VBClLogError("VBoxDRMClient: unable to validate screen layout: there is a hole in displays layout config, "
                             "monitor (%u) is ENABLED while (%u) does not\n", i, i - 1);
                fValid = false;
            }
            else
            {
                /* Align displays next to each other (if needed) and check if there is no holes in between monitors. */
                if (!(aVmMonitorsCache[i].fDisplayFlags & VMMDEV_DISPLAY_ORIGIN))
                {
                    aVmMonitorsCache[i].xOrigin = aVmMonitorsCache[i - 1].xOrigin + aVmMonitorsCache[i - 1].cx;
                    aVmMonitorsCache[i].yOrigin = aVmMonitorsCache[i - 1].yOrigin;
                }

                /* Only count enabled monitors. */
                if (!(aVmMonitorsCache[i].fDisplayFlags & VMMDEV_DISPLAY_DISABLED))
                    cDisplaysOut++;
            }
        }
    }

    /* Copy out layout data. */
    if (fValid)
    {
        for (uint32_t i = 0; i < cDisplaysOut; i++)
        {
            aDisplaysOut[i].x = aVmMonitorsCache[i].xOrigin;
            aDisplaysOut[i].y = aVmMonitorsCache[i].yOrigin;
            aDisplaysOut[i].w = aVmMonitorsCache[i].cx;
            aDisplaysOut[i].h = aVmMonitorsCache[i].cy;

            VBClLogInfo("VBoxDRMClient: update monitor %u parameters: %dx%d, (%d, %d)\n",
                        i,
                        aDisplaysOut[i].w, aDisplaysOut[i].h,
                        aDisplaysOut[i].x, aDisplaysOut[i].y);

        }

        *pcActualDisplays = cDisplaysOut;
    }

    return (fValid && cDisplaysOut > 0) ? VINF_SUCCESS : VERR_INVALID_PARAMETER;
}

/**
 * This function sends screen layout data to DRM stack.
 *
 * @return  VINF_SUCCESS on success, IPRT error code otherwise.
 * @param   hDevice             Handle to opened DRM device.
 * @param   paRects             Array of screen configuration data.
 * @param   cRects              Number of elements in screen configuration array.
 */
static int drmSendHints(RTFILE hDevice, struct DRMVMWRECT *paRects, uint32_t cRects)
{
    int rc = 0;
    uid_t curuid;

    /* Store real user id. */
    curuid = getuid();

    /* Chenge effective user id. */
    if (setreuid(0, 0) == 0)
    {
        struct DRMVMWUPDATELAYOUT ioctlLayout;

        RT_ZERO(ioctlLayout);
        ioctlLayout.cOutputs = cRects;
        ioctlLayout.ptrRects = (uint64_t)paRects;

        rc = RTFileIoCtl(hDevice, DRM_IOCTL_VMW_UPDATE_LAYOUT,
                         &ioctlLayout, sizeof(ioctlLayout), NULL);

        if (setreuid(curuid, 0) != 0)
        {
            VBClLogError("VBoxDRMClient: reset of setreuid failed after drm ioctl");
            rc = VERR_ACCESS_DENIED;
        }
    }
    else
    {
        VBClLogError("VBoxDRMClient: setreuid failed during drm ioctl\n");
        rc = VERR_ACCESS_DENIED;
    }

    return rc;
}

/**
 * This function converts vmwgfx monitors layout data into an array of monitors offsets
 * and sends it back to the host in order to ensure that host and guest have the same
 * monitors layout representation.
 *
 * @return IPRT status code.
 * @param cDisplays     Number of displays (elements in @pDisplays).
 * @param pDisplays     Displays parameters as it was sent to vmwgfx driver.
 */
static int drmSendMonitorPositions(uint32_t cDisplays, struct DRMVMWRECT *pDisplays)
{
    static RTPOINT aPositions[VMW_MAX_HEADS];

    if (!pDisplays || !cDisplays || cDisplays > VMW_MAX_HEADS)
    {
        return VERR_INVALID_PARAMETER;
    }

    /* Prepare monitor offsets list to be sent to the host. */
    for (uint32_t i = 0; i < cDisplays; i++)
    {
        aPositions[i].x = pDisplays[i].x;
        aPositions[i].y = pDisplays[i].y;
    }

    return VbglR3SeamlessSendMonitorPositions(cDisplays, aPositions);
}


static void drmMainLoop(RTFILE hDevice)
{
    int rc;

    for (;;)
    {
        /* Do not acknowledge the first event we query for to pick up old events,
         * e.g. from before a guest reboot. */
        bool fAck = false;

        uint32_t events;

        VMMDevDisplayDef aDisplaysIn[VMW_MAX_HEADS];
        uint32_t cDisplaysIn = 0;

        struct DRMVMWRECT aDisplaysOut[VMW_MAX_HEADS];
        uint32_t cDisplaysOut = 0;

        RT_ZERO(aDisplaysIn);
        RT_ZERO(aDisplaysOut);

        /* Query the first size without waiting.  This lets us e.g. pick up
         * the last event before a guest reboot when we start again after. */
        rc = VbglR3GetDisplayChangeRequestMulti(VMW_MAX_HEADS, &cDisplaysIn, aDisplaysIn, fAck);
        fAck = true;
        if (RT_FAILURE(rc))
        {
            VBClLogError("Failed to get display change request, rc=%Rrc\n", rc);
        }
        else
        {
            /* Validate displays layout and push it to DRM stack if valid. */
            rc = drmValidateLayout(aDisplaysIn, cDisplaysIn, aDisplaysOut, sizeof(aDisplaysOut), &cDisplaysOut);
            if (RT_SUCCESS(rc))
            {
                rc = drmSendHints(hDevice, aDisplaysOut, cDisplaysOut);
                VBClLogInfo("VBoxDRMClient: push screen layout data of %u display(s) to DRM stack has %s (%Rrc)\n",
                            cDisplaysOut, RT_SUCCESS(rc) ? "succeeded" : "failed", rc);
                /* In addition, notify host that configuration was successfully applied to the guest vmwgfx driver. */
                if (RT_SUCCESS(rc))
                {
                    rc = drmSendMonitorPositions(cDisplaysOut, aDisplaysOut);
                    if (RT_FAILURE(rc))
                    {
                        VBClLogError("VBoxDRMClient: cannot send host notification: %Rrc\n", rc);
                    }
                }
            }
            else
            {
                VBClLogError("VBoxDRMClient: displays layout is invalid, will not notify guest driver, rc=%Rrc\n", rc);
            }
        }

        do
        {
            rc = VbglR3WaitEvent(VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST, RT_INDEFINITE_WAIT, &events);
        } while (rc == VERR_INTERRUPTED);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Failure waiting for event, rc=%Rrc\n", rc);
    }
}

int main(int argc, char *argv[])
{
    RTFILE hDevice = NIL_RTFILE;
    RTFILE hPidFile;

    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClLogFatalError("VBoxDRMClient: VbglR3InitUser failed: %Rrc", rc);

    rc = VBClLogCreate("");
    if (RT_FAILURE(rc))
        VBClLogFatalError("VBoxDRMClient: failed to setup logging, rc=%Rrc\n", rc);

    PRTLOGGER pReleaseLog = RTLogRelGetDefaultInstance();
    if (pReleaseLog)
    {
        rc = RTLogDestinations(pReleaseLog, "stdout");
        if (RT_FAILURE(rc))
            VBClLogFatalError("VBoxDRMClient: failed to redirert error output, rc=%Rrc", rc);
    }
    else
    {
        VBClLogFatalError("VBoxDRMClient: failed to get logger instance");
    }

    /* Check PID file before attempting to initialize anything. */
    rc = VbglR3PidFile(g_szPidFile, &hPidFile);
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

    drmMainLoop(hDevice);

    /** @todo this code never executed since we do not have yet a clean way to exit
     * main event loop above. */

    RTFileClose(hDevice);

    VBClLogInfo("VBoxDRMClient: releasing PID file lock\n");
    VbglR3ClosePidFile(g_szPidFile, hPidFile);

    return 0;
}
