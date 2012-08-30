/* $Id$ */
/** @file
 * VNC main module.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/ExtPack/ExtPack.h>

#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/vmm/cfgm.h>
#include <iprt/string.h>
#include <iprt/param.h>
#include <iprt/path.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the extension pack helpers. */
static PCVBOXEXTPACKHLP g_pHlp;


// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnInstalled}
//  */
// static DECLCALLBACK(void) vboxVNCExtPack_Installed(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnUninstall}
//  */
// static DECLCALLBACK(int)  vboxVNCExtPack_Uninstall(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVirtualBoxReady}
//  */
// static DECLCALLBACK(void)  vboxVNCExtPack_VirtualBoxReady(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnUnload}
//  */
// static DECLCALLBACK(void) vboxVNCExtPack_Unload(PCVBOXEXTPACKREG pThis);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMCreated}
//  */
// static DECLCALLBACK(int)  vboxVNCExtPack_VMCreated(PCVBOXEXTPACKREG pThis, VBOXEXTPACK_IF_CS(IVirtualBox) *pVirtualBox, IMachine *pMachine);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMConfigureVMM}
//  */
// static DECLCALLBACK(int)  vboxVNCExtPack_VMConfigureVMM(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
//
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOn}
//  */
// static DECLCALLBACK(int)  vboxVNCExtPack_VMPowerOn(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vboxVNCExtPack_VMPowerOff(PCVBOXEXTPACKREG pThis, IConsole *pConsole, PVM pVM);
// /**
//  * @interface_method_impl{VBOXEXTPACKREG,pfnVMPowerOff}
//  */
// static DECLCALLBACK(void) vboxVNCExtPack_QueryObject(PCVBOXEXTPACKREG pThis, PCRTUUID pObjectId);


static const VBOXEXTPACKREG g_vboxVNCExtPackReg =
{
    VBOXEXTPACKREG_VERSION,
    /* .pfnInstalled =      */  NULL,
    /* .pfnUninstall =      */  NULL,
    /* .pfnVirtualBoxReady =*/  NULL,
    /* .pfnConsoleReady =   */  NULL,
    /* .pfnUnload =         */  NULL,
    /* .pfnVMCreated =      */  NULL,
    /* .pfnVMConfigureVMM = */  NULL,
    /* .pfnVMPowerOn =      */  NULL,
    /* .pfnVMPowerOff =     */  NULL,
    /* .pfnQueryObject =    */  NULL,
    VBOXEXTPACKREG_VERSION
};


/** @callback_method_impl{FNVBOXEXTPACKREGISTER}  */
extern "C" DECLEXPORT(int) VBoxExtPackRegister(PCVBOXEXTPACKHLP pHlp, PCVBOXEXTPACKREG *ppReg, PRTERRINFO pErrInfo)
{
    /*
     * Check the VirtualBox version.
     */
    if (!VBOXEXTPACK_IS_VER_COMPAT(pHlp->u32Version, VBOXEXTPACKHLP_VERSION))
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "Helper version mismatch - expected %#x got %#x",
                             VBOXEXTPACKHLP_VERSION, pHlp->u32Version);
    if (   VBOX_FULL_VERSION_GET_MAJOR(pHlp->uVBoxFullVersion) != VBOX_VERSION_MAJOR
        || VBOX_FULL_VERSION_GET_MINOR(pHlp->uVBoxFullVersion) != VBOX_VERSION_MINOR)
        return RTErrInfoSetF(pErrInfo, VERR_VERSION_MISMATCH,
                             "VirtualBox version mismatch - expected %u.%u got %u.%u",
                             VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR,
                             VBOX_FULL_VERSION_GET_MAJOR(pHlp->uVBoxFullVersion),
                             VBOX_FULL_VERSION_GET_MINOR(pHlp->uVBoxFullVersion));

    /*
     * We're good, save input and return the registration structure.
     */
    g_pHlp = pHlp;
    *ppReg = &g_vboxVNCExtPackReg;

    return VINF_SUCCESS;
}

