/* $Id $ */
/** @file
 * Built-in drivers & devices (part 2).
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV
#include <VBox/pdm.h>
#include <VBox/version.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>

#include "Builtins2.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
const void *g_apvVBoxDDDependencies2[] =
{
    (void *)&g_abPcBiosBinary,
    (void *)&g_abVgaBiosBinary,
    (void *)&g_abNetBiosBinary
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

    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceAPIC);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = pCallbacks->pfnRegister(pCallbacks, &g_DeviceIOAPIC);
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

