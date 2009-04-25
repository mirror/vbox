/* $Id$ */
/** @file
 * Built-in drivers & devices (part 1)
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#ifdef VBOX_WITH_EFI
    &g_abEfiThunkBinary[0],
#endif
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
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcArch);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePcBios);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePS2KeyboardMouse);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePIIX3IDE);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8254);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceI8259);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_HPET
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceHPET);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_SMC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSMC);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_LPC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLPC);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_EFI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceEFI);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceMC146818);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVga);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceVMMDev);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCNet);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_E1000
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceE1000);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_INIP
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceINIP);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceICHAC97);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSB16);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAudioSniffer);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_USB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceOHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_EHCI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceEHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_ACPI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceACPI);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceDMA);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceFloppyController);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceSerialPort);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceParallelPort);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_AHCI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAHCI);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_BUSLOGIC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceBusLogic);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DevicePCIBridge);
    if (RT_FAILURE(rc))
        return rc;
#ifdef VBOX_WITH_LSILOGIC
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceLsiLogicSCSI);
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
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvKeyboardQueue);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvBlock);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVD);
    if (RT_FAILURE(rc))
        return rc;
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostDVD);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_LINUX) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostFloppy);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvMediaISO);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvRawImage);
    if (RT_FAILURE(rc))
        return rc;
#ifndef RT_OS_L4
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNAT);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if defined(RT_OS_LINUX)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostInterface);
    if (RT_FAILURE(rc))
        return rc;
#endif
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvIntNet);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNetSniffer);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvAUDIO);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvACPI);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_USB
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvVUSBRootHub);
    if (RT_FAILURE(rc))
        return rc;
#endif

#if !defined(RT_OS_L4)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvNamedPipe);
    if (RT_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvChar);
    if (RT_FAILURE(rc))
        return rc;
#endif

#if defined(RT_OS_LINUX)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostParallel);
    if (RT_FAILURE(rc))
        return rc;
#endif

#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvHostSerial);
    if (RT_FAILURE(rc))
        return rc;
#endif

#if defined(VBOX_WITH_PDM_ASYNC_COMPLETION)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvRawImageAsync);
    if (RT_FAILURE(rc))
        return rc;

    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvTransportAsync);
    if (RT_FAILURE(rc))
        return rc;
#endif
#ifdef VBOX_WITH_SCSI
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvSCSI);
    if (RT_FAILURE(rc))
        return rc;

#if defined(RT_OS_LINUX)
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvSCSIHost);
    if (RT_FAILURE(rc))
        return rc;
#endif
#endif

#ifdef VBOX_WITH_FAULT_INJECTION
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DrvFaultInject);
    if (RT_FAILURE(rc))
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
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}
#endif
