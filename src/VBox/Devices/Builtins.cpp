/** $Id$ */
/** @file
 * Built-in drivers & devices (part 1)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV
#include <VBox/pdm.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/usb.h>

#include <VBox/log.h>
#include <iprt/assert.h>

#include "Builtins.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
const void *g_apvVBoxDDDependencies[] =
{
    NULL,
};


/**
 * Register builtin devices.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxDevicesRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));
    int rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCI);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcArch);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcBios);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePS2KeyboardMouse);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePIIX3IDE);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8254);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8259);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceMC146818);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVga);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVMMDev);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCNet);
    if (VBOX_FAILURE(rc))
        return rc;
#if 0
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceNE2000);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceICHAC97);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAudioSniffer);
    if (VBOX_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_USB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceOHCI);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_EHCI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceEHCI);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_ACPI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceACPI);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceDMA);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceFloppyController);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSerialPort);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceParallelPort);
    if (VBOX_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_AHCI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAHCI);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}


/**
 * Register builtin drivers.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDriversRegister(PCPDMDRVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxDriversRegister: u32Version=%#x\n", u32Version));
    AssertReleaseMsg(u32Version == VBOX_VERSION, ("u32Version=%#x VBOX_VERSION=%#x\n", u32Version, VBOX_VERSION));

    int rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvMouseQueue);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvKeyboardQueue);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvBlock);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVBoxHDD);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVD);
    if (VBOX_FAILURE(rc))
        return rc;
#ifndef RT_OS_L4
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVmdkHDD);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostDVD);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_LINUX) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostFloppy);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvMediaISO);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvRawImage);
    if (VBOX_FAILURE(rc))
        return rc;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_LINUX)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostHDD);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_ISCSI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvISCSI);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvISCSITransportTcp);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#ifndef RT_OS_L4
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNAT);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_L4) || defined(RT_OS_LINUX) || defined(RT_OS_OS2) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostInterface);
    if (VBOX_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvIntNet);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNetSniffer);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvAUDIO);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvACPI);
    if (VBOX_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_USB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVUSBRootHub);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

#if !defined(RT_OS_L4)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNamedPipe);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvChar);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

#if defined(RT_OS_LINUX)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostParallel);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

#if defined(RT_OS_LINUX) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostSerial);
    if (VBOX_FAILURE(rc))
        return rc;
#endif

    return VINF_SUCCESS;
}


#ifdef VBOX_WITH_USB
/**
 * Register builtin USB device.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxUsbRegister(PCPDMUSBREGCB pCallbacks, uint32_t u32Version)
{
    int rc = pCallbacks->pfnRegister(pCallbacks, &g_UsbDevProxy);
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}
#endif

