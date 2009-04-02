/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, Device Helpers.
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
#define LOG_GROUP LOG_GROUP_PDM_DEVICE
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/iom.h>
#include <VBox/rem.h>
#include <VBox/dbgf.h>
#include <VBox/vm.h>
#include <VBox/vmm.h>

#include <VBox/version.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PDM_DEVHLP_DEADLOCK_DETECTION
 * Define this to enable the deadlock detection when accessing physical memory.
 */
#if defined(DEBUG_bird) || defined(DOXYGEN_RUNNING)
# define PDM_DEVHLP_DEADLOCK_DETECTION
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name R3 DevHlp
 * @{
 */


/** @copydoc PDMDEVHLPR3::pfnIOPortRegister */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTHCPTR pvUser, PFNIOMIOPORTOUT pfnOut, PFNIOMIOPORTIN pfnIn,
                                                    PFNIOMIOPORTOUTSTRING pfnOutStr, PFNIOMIOPORTINSTRING pfnInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pfnOut=%p pfnIn=%p pfnOutStr=%p pfnInStr=%p p32_tszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc, pszDesc));
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    int rc = IOMR3IOPortRegisterR3(pDevIns->Internal.s.pVMR3, pDevIns, Port, cPorts, pvUser, pfnOut, pfnIn, pfnOutStr, pfnInStr, pszDesc);

    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnIOPortRegisterGC */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterGC(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTRCPTR pvUser,
                                                      const char *pszOut, const char *pszIn,
                                                      const char *pszOutStr, const char *pszInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_IOPortRegister: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pszOut=%p:{%s} pszIn=%p:{%s} pszOutStr=%p:{%s} pszInStr=%p:{%s} pszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pszOut, pszOut, pszIn, pszIn, pszOutStr, pszOutStr, pszInStr, pszInStr, pszDesc, pszDesc));

    /*
     * Resolve the functions (one of the can be NULL).
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szRCMod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC))
    {
        RTRCPTR RCPtrIn = NIL_RTRCPTR;
        if (pszIn)
        {
            rc = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszIn, &RCPtrIn);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszIn)\n", pDevIns->pDevReg->szRCMod, pszIn));
        }
        RTRCPTR RCPtrOut = NIL_RTRCPTR;
        if (pszOut && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszOut, &RCPtrOut);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOut)\n", pDevIns->pDevReg->szRCMod, pszOut));
        }
        RTRCPTR RCPtrInStr = NIL_RTRCPTR;
        if (pszInStr && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszInStr, &RCPtrInStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszInStr)\n", pDevIns->pDevReg->szRCMod, pszInStr));
        }
        RTRCPTR RCPtrOutStr = NIL_RTRCPTR;
        if (pszOutStr && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszOutStr, &RCPtrOutStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOutStr)\n", pDevIns->pDevReg->szRCMod, pszOutStr));
        }

        if (RT_SUCCESS(rc))
            rc = IOMR3IOPortRegisterRC(pDevIns->Internal.s.pVMR3, pDevIns, Port, cPorts, pvUser, RCPtrOut, RCPtrIn, RCPtrOutStr, RCPtrInStr, pszDesc);
    }
    else
    {
        AssertMsgFailed(("No GC module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_IOPortRegisterGC: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnIOPortRegisterR0 */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortRegisterR0(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts, RTR0PTR pvUser,
                                                      const char *pszOut, const char *pszIn,
                                                      const char *pszOutStr, const char *pszInStr, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_IOPortRegisterR0: caller='%s'/%d: Port=%#x cPorts=%#x pvUser=%p pszOut=%p:{%s} pszIn=%p:{%s} pszOutStr=%p:{%s} pszInStr=%p:{%s} pszDesc=%p:{%s}\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts, pvUser, pszOut, pszOut, pszIn, pszIn, pszOutStr, pszOutStr, pszInStr, pszInStr, pszDesc, pszDesc));

    /*
     * Resolve the functions (one of the can be NULL).
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szR0Mod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        R0PTRTYPE(PFNIOMIOPORTIN) pfnR0PtrIn = 0;
        if (pszIn)
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszIn, &pfnR0PtrIn);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszIn)\n", pDevIns->pDevReg->szR0Mod, pszIn));
        }
        R0PTRTYPE(PFNIOMIOPORTOUT) pfnR0PtrOut = 0;
        if (pszOut && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszOut, &pfnR0PtrOut);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOut)\n", pDevIns->pDevReg->szR0Mod, pszOut));
        }
        R0PTRTYPE(PFNIOMIOPORTINSTRING) pfnR0PtrInStr = 0;
        if (pszInStr && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszInStr, &pfnR0PtrInStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszInStr)\n", pDevIns->pDevReg->szR0Mod, pszInStr));
        }
        R0PTRTYPE(PFNIOMIOPORTOUTSTRING) pfnR0PtrOutStr = 0;
        if (pszOutStr && RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszOutStr, &pfnR0PtrOutStr);
            AssertMsgRC(rc, ("Failed to resolve %s.%s (pszOutStr)\n", pDevIns->pDevReg->szR0Mod, pszOutStr));
        }

        if (RT_SUCCESS(rc))
            rc = IOMR3IOPortRegisterR0(pDevIns->Internal.s.pVMR3, pDevIns, Port, cPorts, pvUser, pfnR0PtrOut, pfnR0PtrIn, pfnR0PtrOutStr, pfnR0PtrInStr, pszDesc);
    }
    else
    {
        AssertMsgFailed(("No R0 module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_IOPortRegisterR0: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnIOPortDeregister */
static DECLCALLBACK(int) pdmR3DevHlp_IOPortDeregister(PPDMDEVINS pDevIns, RTIOPORT Port, RTUINT cPorts)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_IOPortDeregister: caller='%s'/%d: Port=%#x cPorts=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             Port, cPorts));

    int rc = IOMR3IOPortDeregister(pDevIns->Internal.s.pVMR3, pDevIns, Port, cPorts);

    LogFlow(("pdmR3DevHlp_IOPortDeregister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnMMIORegister */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTHCPTR pvUser,
                                                  PFNIOMMMIOWRITE pfnWrite, PFNIOMMMIOREAD pfnRead, PFNIOMMMIOFILL pfnFill,
                                                  const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIORegister: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x pvUser=%p pfnWrite=%p pfnRead=%p pfnFill=%p pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc, pszDesc));

    int rc = IOMR3MMIORegisterR3(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange, pvUser, pfnWrite, pfnRead, pfnFill, pszDesc);

    LogFlow(("pdmR3DevHlp_MMIORegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnMMIORegisterGC */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterGC(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTGCPTR pvUser,
                                                    const char *pszWrite, const char *pszRead, const char *pszFill,
                                                    const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIORegisterGC: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x pvUser=%p pszWrite=%p:{%s} pszRead=%p:{%s} pszFill=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pszWrite, pszWrite, pszRead, pszRead, pszFill, pszFill));

    /*
     * Resolve the functions.
     * Not all function have to present, leave it to IOM to enforce this.
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szRCMod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC))
    {
        RTRCPTR RCPtrWrite = NIL_RTRCPTR;
        if (pszWrite)
            rc = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszWrite, &RCPtrWrite);

        RTRCPTR RCPtrRead = NIL_RTRCPTR;
        int rc2 = VINF_SUCCESS;
        if (pszRead)
            rc2 = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszRead, &RCPtrRead);

        RTRCPTR RCPtrFill = NIL_RTRCPTR;
        int rc3 = VINF_SUCCESS;
        if (pszFill)
            rc3 = PDMR3LdrGetSymbolRCLazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szRCMod, pszFill, &RCPtrFill);

        if (RT_SUCCESS(rc) && RT_SUCCESS(rc2) && RT_SUCCESS(rc3))
            rc = IOMR3MMIORegisterRC(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange, pvUser, RCPtrWrite, RCPtrRead, RCPtrFill);
        else
        {
            AssertMsgRC(rc,  ("Failed to resolve %s.%s (pszWrite)\n", pDevIns->pDevReg->szRCMod, pszWrite));
            AssertMsgRC(rc2, ("Failed to resolve %s.%s (pszRead)\n",  pDevIns->pDevReg->szRCMod, pszRead));
            AssertMsgRC(rc3, ("Failed to resolve %s.%s (pszFill)\n",  pDevIns->pDevReg->szRCMod, pszFill));
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
            if (RT_FAILURE(rc3) && RT_SUCCESS(rc))
                rc = rc3;
        }
    }
    else
    {
        AssertMsgFailed(("No GC module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_MMIORegisterGC: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLPR3::pfnMMIORegisterR0 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIORegisterR0(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, RTR0PTR pvUser,
                                                    const char *pszWrite, const char *pszRead, const char *pszFill,
                                                    const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIORegisterHC: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x pvUser=%p pszWrite=%p:{%s} pszRead=%p:{%s} pszFill=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvUser, pszWrite, pszWrite, pszRead, pszRead, pszFill, pszFill));

    /*
     * Resolve the functions.
     * Not all function have to present, leave it to IOM to enforce this.
     */
    int rc = VINF_SUCCESS;
    if (    pDevIns->pDevReg->szR0Mod[0]
        &&  (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        R0PTRTYPE(PFNIOMMMIOWRITE) pfnR0PtrWrite = 0;
        if (pszWrite)
            rc = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszWrite, &pfnR0PtrWrite);
        R0PTRTYPE(PFNIOMMMIOREAD) pfnR0PtrRead = 0;
        int rc2 = VINF_SUCCESS;
        if (pszRead)
            rc2 = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszRead, &pfnR0PtrRead);
        R0PTRTYPE(PFNIOMMMIOFILL) pfnR0PtrFill = 0;
        int rc3 = VINF_SUCCESS;
        if (pszFill)
            rc3 = PDMR3LdrGetSymbolR0Lazy(pDevIns->Internal.s.pVMR3, pDevIns->pDevReg->szR0Mod, pszFill, &pfnR0PtrFill);
        if (RT_SUCCESS(rc) && RT_SUCCESS(rc2) && RT_SUCCESS(rc3))
            rc = IOMR3MMIORegisterR0(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange, pvUser, pfnR0PtrWrite, pfnR0PtrRead, pfnR0PtrFill);
        else
        {
            AssertMsgRC(rc,  ("Failed to resolve %s.%s (pszWrite)\n", pDevIns->pDevReg->szR0Mod, pszWrite));
            AssertMsgRC(rc2, ("Failed to resolve %s.%s (pszRead)\n",  pDevIns->pDevReg->szR0Mod, pszRead));
            AssertMsgRC(rc3, ("Failed to resolve %s.%s (pszFill)\n",  pDevIns->pDevReg->szR0Mod, pszFill));
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
            if (RT_FAILURE(rc3) && RT_SUCCESS(rc))
                rc = rc3;
        }
    }
    else
    {
        AssertMsgFailed(("No R0 module for this driver!\n"));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DevHlp_MMIORegisterR0: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnMMIODeregister */
static DECLCALLBACK(int) pdmR3DevHlp_MMIODeregister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIODeregister: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange));

    int rc = IOMR3MMIODeregister(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange);

    LogFlow(("pdmR3DevHlp_MMIODeregister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnROMRegister */
static DECLCALLBACK(int) pdmR3DevHlp_ROMRegister(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, const void *pvBinary, uint32_t fFlags, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_ROMRegister: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x pvBinary=%p fFlags=%#RX32 pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, pvBinary, fFlags, pszDesc, pszDesc));

#ifdef VBOX_WITH_NEW_PHYS_CODE
    int rc = PGMR3PhysRomRegister(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange, pvBinary, fFlags, pszDesc);
#else
    int rc = MMR3PhysRomRegister(pDevIns->Internal.s.pVMR3, pDevIns, GCPhysStart, cbRange, pvBinary,
                                 !!(fFlags & PGMPHYS_ROM_FLAGS_SHADOWED), pszDesc);
#endif

    LogFlow(("pdmR3DevHlp_ROMRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnSSMRegister */
static DECLCALLBACK(int) pdmR3DevHlp_SSMRegister(PPDMDEVINS pDevIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                                 PFNSSMDEVSAVEPREP pfnSavePrep, PFNSSMDEVSAVEEXEC pfnSaveExec, PFNSSMDEVSAVEDONE pfnSaveDone,
                                                 PFNSSMDEVLOADPREP pfnLoadPrep, PFNSSMDEVLOADEXEC pfnLoadExec, PFNSSMDEVLOADDONE pfnLoadDone)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_SSMRegister: caller='%s'/%d: pszName=%p:{%s} u32Instance=%#x u32Version=#x cbGuess=%#x pfnSavePrep=%p pfnSaveExec=%p pfnSaveDone=%p pszLoadPrep=%p pfnLoadExec=%p pfnLoaddone=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszName, pszName, u32Instance, u32Version, cbGuess, pfnSavePrep, pfnSaveExec, pfnSaveDone, pfnLoadPrep, pfnLoadExec, pfnLoadDone));

    int rc = SSMR3RegisterDevice(pDevIns->Internal.s.pVMR3, pDevIns, pszName, u32Instance, u32Version, cbGuess,
                                 pfnSavePrep, pfnSaveExec, pfnSaveDone,
                                 pfnLoadPrep, pfnLoadExec, pfnLoadDone);

    LogFlow(("pdmR3DevHlp_SSMRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnTMTimerCreate */
static DECLCALLBACK(int) pdmR3DevHlp_TMTimerCreate(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMERDEV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_TMTimerCreate: caller='%s'/%d: enmClock=%d pfnCallback=%p pszDesc=%p:{%s} ppTimer=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, enmClock, pfnCallback, pszDesc, pszDesc, ppTimer));

    int rc = TMR3TimerCreateDevice(pDevIns->Internal.s.pVMR3, pDevIns, enmClock, pfnCallback, pszDesc, ppTimer);

    LogFlow(("pdmR3DevHlp_TMTimerCreate: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnTMTimerCreateExternal */
static DECLCALLBACK(PTMTIMERR3) pdmR3DevHlp_TMTimerCreateExternal(PPDMDEVINS pDevIns, TMCLOCK enmClock, PFNTMTIMEREXT pfnCallback, void *pvUser, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    return TMR3TimerCreateExternal(pDevIns->Internal.s.pVMR3, enmClock, pfnCallback, pvUser, pszDesc);
}


/** @copydoc PDMDEVHLPR3::pfnPCIRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: pPciDev=%p:{.config={%#.256Rhxs}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev, pPciDev->config));

    /*
     * Validate input.
     */
    if (!pPciDev)
    {
        Assert(pPciDev);
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc (pPciDev)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!pPciDev->config[0] && !pPciDev->config[1])
    {
        Assert(pPciDev->config[0] || pPciDev->config[1]);
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc (vendor)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (pDevIns->Internal.s.pPciDeviceR3)
    {
        /** @todo the PCI device vs. PDM device designed is a bit flawed if we have to
         * support a PDM device with multiple PCI devices. This might become a problem
         * when upgrading the chipset for instance because of multiple functions in some
         * devices...
         */
        AssertMsgFailed(("Only one PCI device per device is currently implemented!\n"));
        return VERR_INTERNAL_ERROR;
    }

    /*
     * Choose the PCI bus for the device.
     *
     * This is simple. If the device was configured for a particular bus, the PCIBusNo
     * configuration value will be set. If not the default bus is 0.
     */
    int rc;
    PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusR3;
    if (!pBus)
    {
        uint8_t u8Bus;
        rc = CFGMR3QueryU8Def(pDevIns->Internal.s.pCfgHandle, "PCIBusNo", &u8Bus, 0);
        AssertLogRelMsgRCReturn(rc, ("Configuration error: PCIBusNo query failed with rc=%Rrc (%s/%d)\n",
                                     rc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance), rc);
        AssertLogRelMsgReturn(u8Bus < RT_ELEMENTS(pVM->pdm.s.aPciBuses),
                              ("Configuration error: PCIBusNo=%d, max is %d. (%s/%d)\n", u8Bus,
                               RT_ELEMENTS(pVM->pdm.s.aPciBuses), pDevIns->pDevReg->szDeviceName, pDevIns->iInstance),
                              VERR_PDM_NO_PCI_BUS);
        pBus = pDevIns->Internal.s.pPciBusR3 = &pVM->pdm.s.aPciBuses[u8Bus];
    }
    if (pBus->pDevInsR3)
    {
        if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC)
            pDevIns->Internal.s.pPciBusR0 = MMHyperR3ToR0(pVM, pDevIns->Internal.s.pPciBusR3);
        else
            pDevIns->Internal.s.pPciBusR0 = NIL_RTR0PTR;

        if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC)
            pDevIns->Internal.s.pPciBusRC = MMHyperR3ToRC(pVM, pDevIns->Internal.s.pPciBusR3);
        else
            pDevIns->Internal.s.pPciBusRC = NIL_RTRCPTR;

        /*
         * Check the configuration for PCI device and function assignment.
         */
        int iDev = -1;
        uint8_t     u8Device;
        rc = CFGMR3QueryU8(pDevIns->Internal.s.pCfgHandle, "PCIDeviceNo", &u8Device);
        if (RT_SUCCESS(rc))
        {
            if (u8Device > 31)
            {
                AssertMsgFailed(("Configuration error: PCIDeviceNo=%d, max is 31. (%s/%d)\n",
                                 u8Device, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return VERR_INTERNAL_ERROR;
            }

            uint8_t     u8Function;
            rc = CFGMR3QueryU8(pDevIns->Internal.s.pCfgHandle, "PCIFunctionNo", &u8Function);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("Configuration error: PCIDeviceNo, but PCIFunctionNo query failed with rc=%Rrc (%s/%d)\n",
                                 rc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return rc;
            }
            if (u8Function > 7)
            {
                AssertMsgFailed(("Configuration error: PCIFunctionNo=%d, max is 7. (%s/%d)\n",
                                 u8Function, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                return VERR_INTERNAL_ERROR;
            }
            iDev = (u8Device << 3) | u8Function;
        }
        else if (rc != VERR_CFGM_VALUE_NOT_FOUND)
        {
            AssertMsgFailed(("Configuration error: PCIDeviceNo query failed with rc=%Rrc (%s/%d)\n",
                             rc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
            return rc;
        }

        /*
         * Call the pci bus device to do the actual registration.
         */
        pdmLock(pVM);
        rc = pBus->pfnRegisterR3(pBus->pDevInsR3, pPciDev, pDevIns->pDevReg->szDeviceName, iDev);
        pdmUnlock(pVM);
        if (RT_SUCCESS(rc))
        {
            pPciDev->pDevIns = pDevIns;

            pDevIns->Internal.s.pPciDeviceR3 = pPciDev;
            if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0)
                pDevIns->Internal.s.pPciDeviceR0 = MMHyperR3ToR0(pVM, pPciDev);
            else
                pDevIns->Internal.s.pPciDeviceR0 = NIL_RTR0PTR;

            if (pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC)
                pDevIns->Internal.s.pPciDeviceRC = MMHyperR3ToRC(pVM, pPciDev);
            else
                pDevIns->Internal.s.pPciDeviceRC = NIL_RTRCPTR;

            Log(("PDM: Registered device '%s'/%d as PCI device %d on bus %d\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev->devfn, pDevIns->Internal.s.pPciBusR3->iBus));
        }
    }
    else
    {
        AssertLogRelMsgFailed(("Configuration error: No PCI bus available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_PCI_BUS;
    }

    LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPCIIORegionRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIIORegionRegister(PPDMDEVINS pDevIns, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: iRegion=%d cbRegion=%#x enmType=%d pfnCallback=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, cbRegion, enmType, pfnCallback));

    /*
     * Validate input.
     */
    if (iRegion < 0 || iRegion >= PCI_NUM_REGIONS)
    {
        Assert(iRegion >= 0 && iRegion < PCI_NUM_REGIONS);
        LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Rrc (iRegion)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    switch (enmType)
    {
        case PCI_ADDRESS_SPACE_IO:
            /*
             * Sanity check: don't allow to register more than 32K of the PCI I/O space.
             */
            AssertMsgReturn(cbRegion <= _32K,
                            ("caller='%s'/%d: %#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbRegion),
                            VERR_INVALID_PARAMETER);
            break;

        case PCI_ADDRESS_SPACE_MEM:
        case PCI_ADDRESS_SPACE_MEM_PREFETCH:
            /*
             * Sanity check: don't allow to register more than 512MB of the PCI MMIO space for
             * now. If this limit is increased beyond 2GB, adapt the aligned check below as well!
             */
            AssertMsgReturn(cbRegion <= 512 * _1M,
                            ("caller='%s'/%d: %#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbRegion),
                            VERR_INVALID_PARAMETER);
            break;
        default:
            AssertMsgFailed(("enmType=%#x is unknown\n", enmType));
            LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Rrc (enmType)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
            return VERR_INVALID_PARAMETER;
    }
    if (!pfnCallback)
    {
        Assert(pfnCallback);
        LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Rrc (callback)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    AssertRelease(VMR3GetState(pVM) != VMSTATE_RUNNING);

    /*
     * Must have a PCI device registered!
     */
    int rc;
    PPCIDEVICE pPciDev = pDevIns->Internal.s.pPciDeviceR3;
    if (pPciDev)
    {
        /*
         * We're currently restricted to page aligned MMIO regions.
         */
        if (    (enmType == PCI_ADDRESS_SPACE_MEM || enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH)
            &&  cbRegion != RT_ALIGN_32(cbRegion, PAGE_SIZE))
        {
            Log(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: aligning cbRegion %#x -> %#x\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbRegion, RT_ALIGN_32(cbRegion, PAGE_SIZE)));
            cbRegion = RT_ALIGN_32(cbRegion, PAGE_SIZE);
        }

        /*
         * For registering PCI MMIO memory or PCI I/O memory, the size of the region must be a power of 2!
         */
        int iLastSet = ASMBitLastSetU32(cbRegion);
        Assert(iLastSet > 0);
        uint32_t cbRegionAligned = RT_BIT_32(iLastSet - 1);
        if (cbRegion > cbRegionAligned)
            cbRegion = cbRegionAligned * 2; /* round up */

        PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusR3;
        Assert(pBus);
        pdmLock(pVM);
        rc = pBus->pfnIORegionRegisterR3(pBus->pDevInsR3, pPciDev, iRegion, cbRegion, enmType, pfnCallback);
        pdmUnlock(pVM);
    }
    else
    {
        AssertMsgFailed(("No PCI device registered!\n"));
        rc = VERR_PDM_NOT_PCI_DEVICE;
    }

    LogFlow(("pdmR3DevHlp_PCIIORegionRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPCISetConfigCallbacks */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                            PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCISetConfigCallbacks: caller='%s'/%d: pPciDev=%p pfnRead=%p ppfnReadOld=%p pfnWrite=%p ppfnWriteOld=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciDev, pfnRead, ppfnReadOld, pfnWrite, ppfnWriteOld));

    /*
     * Validate input and resolve defaults.
     */
    AssertPtr(pfnRead);
    AssertPtr(pfnWrite);
    AssertPtrNull(ppfnReadOld);
    AssertPtrNull(ppfnWriteOld);
    AssertPtrNull(pPciDev);

    if (!pPciDev)
        pPciDev = pDevIns->Internal.s.pPciDeviceR3;
    AssertReleaseMsg(pPciDev, ("You must register your device first!\n"));
    PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusR3;
    AssertRelease(pBus);
    AssertRelease(VMR3GetState(pVM) != VMSTATE_RUNNING);

    /*
     * Do the job.
     */
    pdmLock(pVM);
    pBus->pfnSetConfigCallbacksR3(pBus->pDevInsR3, pPciDev, pfnRead, ppfnReadOld, pfnWrite, ppfnWriteOld);
    pdmUnlock(pVM);

    LogFlow(("pdmR3DevHlp_PCISetConfigCallbacks: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnPCISetIrq */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PCISetIrq: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    /*
     * Must have a PCI device registered!
     */
    PPCIDEVICE pPciDev = pDevIns->Internal.s.pPciDeviceR3;
    if (pPciDev)
    {
        PPDMPCIBUS pBus = pDevIns->Internal.s.pPciBusR3; /** @todo the bus should be associated with the PCI device not the PDM device. */
        Assert(pBus);
        PVM pVM = pDevIns->Internal.s.pVMR3;
        pdmLock(pVM);
        pBus->pfnSetIrqR3(pBus->pDevInsR3, pPciDev, iIrq, iLevel);
        pdmUnlock(pVM);
    }
    else
        AssertReleaseMsgFailed(("No PCI device registered!\n"));

    LogFlow(("pdmR3DevHlp_PCISetIrq: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnPCISetIrqNoWait */
static DECLCALLBACK(void) pdmR3DevHlp_PCISetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pdmR3DevHlp_PCISetIrq(pDevIns, iIrq, iLevel);
}


/** @copydoc PDMDEVHLPR3::pfnISASetIrq */
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrq(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_ISASetIrq: caller='%s'/%d: iIrq=%d iLevel=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iIrq, iLevel));

    /*
     * Validate input.
     */
    /** @todo iIrq and iLevel checks. */

    PVM pVM = pDevIns->Internal.s.pVMR3;
    PDMIsaSetIrq(pVM, iIrq, iLevel);    /* (The API takes the lock.) */

    LogFlow(("pdmR3DevHlp_ISASetIrq: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnISASetIrqNoWait */
static DECLCALLBACK(void) pdmR3DevHlp_ISASetIrqNoWait(PPDMDEVINS pDevIns, int iIrq, int iLevel)
{
    pdmR3DevHlp_ISASetIrq(pDevIns, iIrq, iLevel);
}


/** @copydoc PDMDEVHLPR3::pfnDriverAttach */
static DECLCALLBACK(int) pdmR3DevHlp_DriverAttach(PPDMDEVINS pDevIns, RTUINT iLun, PPDMIBASE pBaseInterface, PPDMIBASE *ppBaseInterface, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: iLun=%d pBaseInterface=%p ppBaseInterface=%p pszDesc=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iLun, pBaseInterface, ppBaseInterface, pszDesc, pszDesc));

    /*
     * Lookup the LUN, it might already be registered.
     */
    PPDMLUN pLunPrev = NULL;
    PPDMLUN pLun = pDevIns->Internal.s.pLunsR3;
    for (; pLun; pLunPrev = pLun, pLun = pLun->pNext)
        if (pLun->iLun == iLun)
            break;

    /*
     * Create the LUN if if wasn't found, else check if driver is already attached to it.
     */
    if (!pLun)
    {
        if (    !pBaseInterface
            ||  !pszDesc
            ||  !*pszDesc)
        {
            Assert(pBaseInterface);
            Assert(pszDesc || *pszDesc);
            return VERR_INVALID_PARAMETER;
        }

        pLun = (PPDMLUN)MMR3HeapAlloc(pVM, MM_TAG_PDM_LUN, sizeof(*pLun));
        if (!pLun)
            return VERR_NO_MEMORY;

        pLun->iLun      = iLun;
        pLun->pNext     = pLunPrev ? pLunPrev->pNext : NULL;
        pLun->pTop      = NULL;
        pLun->pBottom   = NULL;
        pLun->pDevIns   = pDevIns;
        pLun->pszDesc   = pszDesc;
        pLun->pBase     = pBaseInterface;
        if (!pLunPrev)
            pDevIns->Internal.s.pLunsR3 = pLun;
        else
            pLunPrev->pNext = pLun;
        Log(("pdmR3DevHlp_DriverAttach: Registered LUN#%d '%s' with device '%s'/%d.\n",
             iLun, pszDesc, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    }
    else if (pLun->pTop)
    {
        AssertMsgFailed(("Already attached! The device should keep track of such things!\n"));
        LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_PDM_DRIVER_ALREADY_ATTACHED));
        return VERR_PDM_DRIVER_ALREADY_ATTACHED;
    }
    Assert(pLun->pBase == pBaseInterface);


    /*
     * Get the attached driver configuration.
     */
    int rc;
    char szNode[48];
    RTStrPrintf(szNode, sizeof(szNode), "LUN#%d", iLun);
    PCFGMNODE   pNode = CFGMR3GetChild(pDevIns->Internal.s.pCfgHandle, szNode);
    if (pNode)
    {
        char *pszName;
        rc = CFGMR3QueryStringAlloc(pNode, "Driver", &pszName);
        if (RT_SUCCESS(rc))
        {
            /*
             * Find the driver.
             */
            PPDMDRV pDrv = pdmR3DrvLookup(pVM, pszName);
            if (pDrv)
            {
                /* config node */
                PCFGMNODE pConfigNode = CFGMR3GetChild(pNode, "Config");
                if (!pConfigNode)
                    rc = CFGMR3InsertNode(pNode, "Config", &pConfigNode);
                if (RT_SUCCESS(rc))
                {
                    CFGMR3SetRestrictedRoot(pConfigNode);

                    /*
                     * Allocate the driver instance.
                     */
                    size_t cb = RT_OFFSETOF(PDMDRVINS, achInstanceData[pDrv->pDrvReg->cbInstance]);
                    cb = RT_ALIGN_Z(cb, 16);
                    PPDMDRVINS pNew = (PPDMDRVINS)MMR3HeapAllocZ(pVM, MM_TAG_PDM_DRIVER, cb);
                    if (pNew)
                    {
                        /*
                         * Initialize the instance structure (declaration order).
                         */
                        pNew->u32Version                = PDM_DRVINS_VERSION;
                        //pNew->Internal.s.pUp            = NULL;
                        //pNew->Internal.s.pDown          = NULL;
                        pNew->Internal.s.pLun           = pLun;
                        pNew->Internal.s.pDrv           = pDrv;
                        pNew->Internal.s.pVM            = pVM;
                        //pNew->Internal.s.fDetaching     = false;
                        pNew->Internal.s.pCfgHandle     = pNode;
                        pNew->pDrvHlp                   = &g_pdmR3DrvHlp;
                        pNew->pDrvReg                   = pDrv->pDrvReg;
                        pNew->pCfgHandle                = pConfigNode;
                        pNew->iInstance                 = pDrv->cInstances++;
                        pNew->pUpBase                   = pBaseInterface;
                        //pNew->pDownBase                 = NULL;
                        //pNew->IBase.pfnQueryInterface   = NULL;
                        pNew->pvInstanceData            = &pNew->achInstanceData[0];

                        /*
                         * Link with LUN and call the constructor.
                         */
                        pLun->pTop = pLun->pBottom = pNew;
                        rc = pDrv->pDrvReg->pfnConstruct(pNew, pNew->pCfgHandle);
                        if (RT_SUCCESS(rc))
                        {
                            MMR3HeapFree(pszName);
                            *ppBaseInterface = &pNew->IBase;
                            Log(("PDM: Attached driver '%s'/%d to LUN#%d on device '%s'/%d.\n",
                                 pDrv->pDrvReg->szDriverName, pNew->iInstance, iLun, pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
                            LogFlow(("pdmR3DevHlp_DriverAttach: caller '%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));

                            return rc;  /* Might return != VINF_SUCCESS (e.g. VINF_NAT_DNS). */
                        }

                        /*
                         * Free the driver.
                         */
                        pLun->pTop = pLun->pBottom = NULL;
                        ASMMemFill32(pNew, cb, 0xdeadd0d0);
                        MMR3HeapFree(pNew);
                        pDrv->cInstances--;
                    }
                    else
                    {
                        AssertMsgFailed(("Failed to allocate %d bytes for instantiating driver '%s'\n", cb, pszName));
                        rc = VERR_NO_MEMORY;
                    }
                }
                else
                    AssertMsgFailed(("Failed to create Config node! rc=%Rrc\n", rc));
            }
            else
            {
                AssertMsgFailed(("Driver '%s' wasn't found!\n", pszName));
                rc = VERR_PDM_DRIVER_NOT_FOUND;
            }
            MMR3HeapFree(pszName);
        }
        else
        {
            AssertMsgFailed(("Query for string value of \"Driver\" -> %Rrc\n", rc));
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                rc = VERR_PDM_CFG_MISSING_DRIVER_NAME;
        }
    }
    else
        rc = VERR_PDM_NO_ATTACHED_DRIVER;


    LogFlow(("pdmR3DevHlp_DriverAttach: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnMMHeapAlloc */
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAlloc(PPDMDEVINS pDevIns, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: cb=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cb));

    void *pv = MMR3HeapAlloc(pDevIns->Internal.s.pVMR3, MM_TAG_PDM_DEVICE_USER, cb);

    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));
    return pv;
}


/** @copydoc PDMDEVHLPR3::pfnMMHeapAllocZ */
static DECLCALLBACK(void *) pdmR3DevHlp_MMHeapAllocZ(PPDMDEVINS pDevIns, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapAllocZ: caller='%s'/%d: cb=%#x\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cb));

    void *pv = MMR3HeapAllocZ(pDevIns->Internal.s.pVMR3, MM_TAG_PDM_DEVICE_USER, cb);

    LogFlow(("pdmR3DevHlp_MMHeapAllocZ: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));
    return pv;
}


/** @copydoc PDMDEVHLPR3::pfnMMHeapFree */
static DECLCALLBACK(void) pdmR3DevHlp_MMHeapFree(PPDMDEVINS pDevIns, void *pv)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_MMHeapFree: caller='%s'/%d: pv=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pv));

    MMR3HeapFree(pv);

    LogFlow(("pdmR3DevHlp_MMHeapAlloc: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnVMSetError */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetError(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMR3, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetErrorV(PPDMDEVINS pDevIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc2 = VMSetErrorV(pDevIns->Internal.s.pVMR3, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeError(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMR3, fFlags, pszErrorId, pszFormat, args);
    va_end(args);
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMSetRuntimeErrorV */
static DECLCALLBACK(int) pdmR3DevHlp_VMSetRuntimeErrorV(PPDMDEVINS pDevIns, uint32_t fFlags, const char *pszErrorId, const char *pszFormat, va_list va)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    int rc = VMSetRuntimeErrorV(pDevIns->Internal.s.pVMR3, fFlags, pszErrorId, pszFormat, va);
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnAssertEMT */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertEMT(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (VM_IS_EMT(pDevIns->Internal.s.pVMR3))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertEMT '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}


/** @copydoc PDMDEVHLPR3::pfnAssertOther */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertOther(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    if (!VM_IS_EMT(pDevIns->Internal.s.pVMR3))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertOther '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}


/** @copydoc PDMDEVHLPR3::pfnDBGFStopV */
static DECLCALLBACK(int) pdmR3DevHlp_DBGFStopV(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction, const char *pszFormat, va_list args)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
#ifdef LOG_ENABLED
    va_list va2;
    va_copy(va2, args);
    LogFlow(("pdmR3DevHlp_DBGFStopV: caller='%s'/%d: pszFile=%p:{%s} iLine=%d pszFunction=%p:{%s} pszFormat=%p:{%s} (%N)\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszFile, pszFile, iLine, pszFunction, pszFunction, pszFormat, pszFormat, pszFormat, &va2));
    va_end(va2);
#endif

    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    int rc = DBGFR3EventSrcV(pVM, DBGFEVENT_DEV_STOP, pszFile, iLine, pszFunction, pszFormat, args);

    LogFlow(("pdmR3DevHlp_DBGFStopV: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnDBGFInfoRegister */
static DECLCALLBACK(int) pdmR3DevHlp_DBGFInfoRegister(PPDMDEVINS pDevIns, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_DBGFInfoRegister: caller='%s'/%d: pszName=%p:{%s} pszDesc=%p:{%s} pfnHandler=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pszName, pszName, pszDesc, pszDesc, pfnHandler));

    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    int rc = DBGFR3InfoRegisterDevice(pVM, pszName, pszDesc, pfnHandler, pDevIns);

    LogFlow(("pdmR3DevHlp_DBGFInfoRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnSTAMRegister */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegister(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    STAM_REG(pVM, pvSample, enmType, pszName, enmUnit, pszDesc);
    NOREF(pVM);
}



/** @copydoc PDMDEVHLPR3::pfnSTAMRegisterF */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterF(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    va_list args;
    va_start(args, pszName);
    int rc = STAMR3RegisterV(pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    va_end(args);
    AssertRC(rc);

    NOREF(pVM);
}


/** @copydoc PDMDEVHLPR3::pfnSTAMRegisterV */
static DECLCALLBACK(void) pdmR3DevHlp_STAMRegisterV(PPDMDEVINS pDevIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    int rc = STAMR3RegisterV(pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    AssertRC(rc);

    NOREF(pVM);
}


/** @copydoc PDMDEVHLPR3::pfnRTCRegister */
static DECLCALLBACK(int) pdmR3DevHlp_RTCRegister(PPDMDEVINS pDevIns, PCPDMRTCREG pRtcReg, PCPDMRTCHLP *ppRtcHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: pRtcReg=%p:{.u32Version=%#x, .pfnWrite=%p, .pfnRead=%p} ppRtcHlp=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pRtcReg, pRtcReg->u32Version, pRtcReg->pfnWrite,
             pRtcReg->pfnWrite, ppRtcHlp));

    /*
     * Validate input.
     */
    if (pRtcReg->u32Version != PDM_RTCREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pRtcReg->u32Version,
                         PDM_RTCREG_VERSION));
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Rrc (version)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pRtcReg->pfnWrite
        ||  !pRtcReg->pfnRead)
    {
        Assert(pRtcReg->pfnWrite);
        Assert(pRtcReg->pfnRead);
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Rrc (callbacks)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppRtcHlp)
    {
        Assert(ppRtcHlp);
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Rrc (ppRtcHlp)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one DMA device.
     */
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (pVM->pdm.s.pRtc)
    {
        AssertMsgFailed(("Only one RTC device is supported!\n"));
        LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Rrc\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate and initialize pci bus structure.
     */
    int rc = VINF_SUCCESS;
    PPDMRTC pRtc = (PPDMRTC)MMR3HeapAlloc(pDevIns->Internal.s.pVMR3, MM_TAG_PDM_DEVICE, sizeof(*pRtc));
    if (pRtc)
    {
        pRtc->pDevIns   = pDevIns;
        pRtc->Reg       = *pRtcReg;
        pVM->pdm.s.pRtc = pRtc;

        /* set the helper pointer. */
        *ppRtcHlp = &g_pdmR3DevRtcHlp;
        Log(("PDM: Registered RTC device '%s'/%d pDevIns=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlow(("pdmR3DevHlp_RTCRegister: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPDMQueueCreate */
static DECLCALLBACK(int) pdmR3DevHlp_PDMQueueCreate(PPDMDEVINS pDevIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval,
                                                    PFNPDMQUEUEDEV pfnCallback, bool fGCEnabled, PPDMQUEUE *ppQueue)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_PDMQueueCreate: caller='%s'/%d: cbItem=%#x cItems=%#x cMilliesInterval=%u pfnCallback=%p fGCEnabled=%RTbool ppQueue=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue));

    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    int rc = PDMR3QueueCreateDevice(pVM, pDevIns, cbItem, cItems, cMilliesInterval, pfnCallback, fGCEnabled, ppQueue);

    LogFlow(("pdmR3DevHlp_PDMQueueCreate: caller='%s'/%d: returns %Rrc *ppQueue=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *ppQueue));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnCritSectInit */
static DECLCALLBACK(int) pdmR3DevHlp_CritSectInit(PPDMDEVINS pDevIns, PPDMCRITSECT pCritSect, const char *pszName)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_CritSectInit: caller='%s'/%d: pCritSect=%p pszName=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pCritSect, pszName, pszName));

    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    int rc = pdmR3CritSectInitDevice(pVM, pDevIns, pCritSect, pszName);

    LogFlow(("pdmR3DevHlp_CritSectInit: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnUTCNow */
static DECLCALLBACK(PRTTIMESPEC) pdmR3DevHlp_UTCNow(PPDMDEVINS pDevIns, PRTTIMESPEC pTime)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_UTCNow: caller='%s'/%d: pTime=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pTime));

    pTime = TMR3UTCNow(pDevIns->Internal.s.pVMR3, pTime);

    LogFlow(("pdmR3DevHlp_UTCNow: caller='%s'/%d: returns %RU64\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, RTTimeSpecGetNano(pTime)));
    return pTime;
}


/** @copydoc PDMDEVHLPR3::pfnPDMThreadCreate */
static DECLCALLBACK(int) pdmR3DevHlp_PDMThreadCreate(PPDMDEVINS pDevIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDEV pfnThread,
                                                     PFNPDMTHREADWAKEUPDEV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_PDMThreadCreate: caller='%s'/%d: ppThread=%p pvUser=%p pfnThread=%p pfnWakeup=%p cbStack=%#zx enmType=%d pszName=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName, pszName));

    int rc = pdmR3ThreadCreateDevice(pDevIns->Internal.s.pVMR3, pDevIns, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName);

    LogFlow(("pdmR3DevHlp_PDMThreadCreate: caller='%s'/%d: returns %Rrc *ppThread=%RTthrd\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
            rc, *ppThread));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnGetVM */
static DECLCALLBACK(PVM) pdmR3DevHlp_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_GetVM: caller='%s'/%d: returns %p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns->Internal.s.pVMR3));
    return pDevIns->Internal.s.pVMR3;
}


/** @copydoc PDMDEVHLPR3::pfnPCIBusRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: pPciBusReg=%p:{.u32Version=%#x, .pfnRegisterR3=%p, .pfnIORegionRegisterR3=%p, .pfnSetIrqR3=%p, "
             ".pfnSaveExecR3=%p, .pfnLoadExecR3=%p, .pfnFakePCIBIOSR3=%p, .pszSetIrqRC=%p:{%s}, .pszSetIrqR0=%p:{%s}} ppPciHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPciBusReg, pPciBusReg->u32Version, pPciBusReg->pfnRegisterR3,
             pPciBusReg->pfnIORegionRegisterR3, pPciBusReg->pfnSetIrqR3, pPciBusReg->pfnSaveExecR3, pPciBusReg->pfnLoadExecR3,
             pPciBusReg->pfnFakePCIBIOSR3, pPciBusReg->pszSetIrqRC, pPciBusReg->pszSetIrqRC, pPciBusReg->pszSetIrqR0, pPciBusReg->pszSetIrqR0, ppPciHlpR3));

    /*
     * Validate the structure.
     */
    if (pPciBusReg->u32Version != PDM_PCIBUSREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pPciBusReg->u32Version, PDM_PCIBUSREG_VERSION));
        LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pPciBusReg->pfnRegisterR3
        ||  !pPciBusReg->pfnIORegionRegisterR3
        ||  !pPciBusReg->pfnSetIrqR3
        ||  !pPciBusReg->pfnSaveExecR3
        ||  !pPciBusReg->pfnLoadExecR3
        ||  (!pPciBusReg->pfnFakePCIBIOSR3 && !pVM->pdm.s.aPciBuses[0].pDevInsR3)) /* Only the first bus needs to do the BIOS work. */
    {
        Assert(pPciBusReg->pfnRegisterR3);
        Assert(pPciBusReg->pfnIORegionRegisterR3);
        Assert(pPciBusReg->pfnSetIrqR3);
        Assert(pPciBusReg->pfnSaveExecR3);
        Assert(pPciBusReg->pfnLoadExecR3);
        Assert(pPciBusReg->pfnFakePCIBIOSR3);
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc (R3 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPciBusReg->pszSetIrqRC
        &&  !VALID_PTR(pPciBusReg->pszSetIrqRC))
    {
        Assert(VALID_PTR(pPciBusReg->pszSetIrqRC));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPciBusReg->pszSetIrqR0
        &&  !VALID_PTR(pPciBusReg->pszSetIrqR0))
    {
        Assert(VALID_PTR(pPciBusReg->pszSetIrqR0));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
     if (!ppPciHlpR3)
    {
        Assert(ppPciHlpR3);
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc (ppPciHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find free PCI bus entry.
     */
    unsigned iBus = 0;
    for (iBus = 0; iBus < RT_ELEMENTS(pVM->pdm.s.aPciBuses); iBus++)
        if (!pVM->pdm.s.aPciBuses[iBus].pDevInsR3)
            break;
    if (iBus >= RT_ELEMENTS(pVM->pdm.s.aPciBuses))
    {
        AssertMsgFailed(("Too many PCI buses. Max=%u\n", RT_ELEMENTS(pVM->pdm.s.aPciBuses)));
        LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc (pci bus)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    PPDMPCIBUS pPciBus = &pVM->pdm.s.aPciBuses[iBus];

    /*
     * Resolve and init the RC bits.
     */
    if (pPciBusReg->pszSetIrqRC)
    {
        int rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pPciBusReg->pszSetIrqRC, &pPciBus->pfnSetIrqRC);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pPciBusReg->pszSetIrqRC, rc));
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pPciBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    }
    else
    {
        pPciBus->pfnSetIrqRC = 0;
        pPciBus->pDevInsRC   = 0;
    }

    /*
     * Resolve and init the R0 bits.
     */
    if (pPciBusReg->pszSetIrqR0)
    {
        int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPciBusReg->pszSetIrqR0, &pPciBus->pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pPciBusReg->pszSetIrqR0, rc));
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PCIRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pPciBus->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    }
    else
    {
        pPciBus->pfnSetIrqR0 = 0;
        pPciBus->pDevInsR0   = 0;
    }

    /*
     * Init the R3 bits.
     */
    pPciBus->iBus                    = iBus;
    pPciBus->pDevInsR3               = pDevIns;
    pPciBus->pfnRegisterR3           = pPciBusReg->pfnRegisterR3;
    pPciBus->pfnIORegionRegisterR3   = pPciBusReg->pfnIORegionRegisterR3;
    pPciBus->pfnSetConfigCallbacksR3 = pPciBusReg->pfnSetConfigCallbacksR3;
    pPciBus->pfnSetIrqR3             = pPciBusReg->pfnSetIrqR3;
    pPciBus->pfnSaveExecR3           = pPciBusReg->pfnSaveExecR3;
    pPciBus->pfnLoadExecR3           = pPciBusReg->pfnLoadExecR3;
    pPciBus->pfnFakePCIBIOSR3        = pPciBusReg->pfnFakePCIBIOSR3;

    Log(("PDM: Registered PCI bus device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppPciHlpR3 = &g_pdmR3DevPciHlp;
    LogFlow(("pdmR3DevHlp_PCIBusRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLPR3::pfnPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: pPicReg=%p:{.u32Version=%#x, .pfnSetIrqR3=%p, .pfnGetInterruptR3=%p, .pszGetIrqRC=%p:{%s}, .pszGetInterruptRC=%p:{%s}, .pszGetIrqR0=%p:{%s}, .pszGetInterruptR0=%p:{%s} } ppPicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pPicReg, pPicReg->u32Version, pPicReg->pfnSetIrqR3, pPicReg->pfnGetInterruptR3,
             pPicReg->pszSetIrqRC, pPicReg->pszSetIrqRC, pPicReg->pszGetInterruptRC, pPicReg->pszGetInterruptRC,
             pPicReg->pszSetIrqR0, pPicReg->pszSetIrqR0, pPicReg->pszGetInterruptR0, pPicReg->pszGetInterruptR0,
             ppPicHlpR3));

    /*
     * Validate input.
     */
    if (pPicReg->u32Version != PDM_PICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pPicReg->u32Version, PDM_PICREG_VERSION));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pPicReg->pfnSetIrqR3
        ||  !pPicReg->pfnGetInterruptR3)
    {
        Assert(pPicReg->pfnSetIrqR3);
        Assert(pPicReg->pfnGetInterruptR3);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (R3 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    (   pPicReg->pszSetIrqRC
             || pPicReg->pszGetInterruptRC)
        &&  (   !VALID_PTR(pPicReg->pszSetIrqRC)
             || !VALID_PTR(pPicReg->pszGetInterruptRC))
       )
    {
        Assert(VALID_PTR(pPicReg->pszSetIrqRC));
        Assert(VALID_PTR(pPicReg->pszGetInterruptRC));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (RC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPicReg->pszSetIrqRC
        &&  !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC))
    {
        Assert(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_RC);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (RC flag)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pPicReg->pszSetIrqR0
        &&  !(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0))
    {
        Assert(pDevIns->pDevReg->fFlags & PDM_DEVREG_FLAGS_R0);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (R0 flag)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppPicHlpR3)
    {
        Assert(ppPicHlpR3);
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc (ppPicHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one PIC device.
     */
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (pVM->pdm.s.Pic.pDevInsR3)
    {
        AssertMsgFailed(("Only one pic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * RC stuff.
     */
    if (pPicReg->pszSetIrqRC)
    {
        int rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pPicReg->pszSetIrqRC, &pVM->pdm.s.Pic.pfnSetIrqRC);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pPicReg->pszSetIrqRC, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pPicReg->pszGetInterruptRC, &pVM->pdm.s.Pic.pfnGetInterruptRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pPicReg->pszGetInterruptRC, rc));
        }
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Pic.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.Pic.pDevInsRC = 0;
        pVM->pdm.s.Pic.pfnSetIrqRC = 0;
        pVM->pdm.s.Pic.pfnGetInterruptRC = 0;
    }

    /*
     * R0 stuff.
     */
    if (pPicReg->pszSetIrqR0)
    {
        int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPicReg->pszSetIrqR0, &pVM->pdm.s.Pic.pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pPicReg->pszSetIrqR0, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pPicReg->pszGetInterruptR0, &pVM->pdm.s.Pic.pfnGetInterruptR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pPicReg->pszGetInterruptR0, rc));
        }
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Pic.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
        Assert(pVM->pdm.s.Pic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.Pic.pfnSetIrqR0 = 0;
        pVM->pdm.s.Pic.pfnGetInterruptR0 = 0;
        pVM->pdm.s.Pic.pDevInsR0 = 0;
    }

    /*
     * R3 stuff.
     */
    pVM->pdm.s.Pic.pDevInsR3 = pDevIns;
    pVM->pdm.s.Pic.pfnSetIrqR3 = pPicReg->pfnSetIrqR3;
    pVM->pdm.s.Pic.pfnGetInterruptR3 = pPicReg->pfnGetInterruptR3;
    Log(("PDM: Registered PIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppPicHlpR3 = &g_pdmR3DevPicHlp;
    LogFlow(("pdmR3DevHlp_PICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLPR3::pfnAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: pApicReg=%p:{.u32Version=%#x, .pfnGetInterruptR3=%p, .pfnSetBaseR3=%p, .pfnGetBaseR3=%p, "
             ".pfnSetTPRR3=%p, .pfnGetTPRR3=%p, .pfnWriteMSR3=%p, .pfnReadMSR3=%p, .pfnBusDeliverR3=%p, pszGetInterruptRC=%p:{%s}, pszSetBaseRC=%p:{%s}, pszGetBaseRC=%p:{%s}, "
             ".pszSetTPRRC=%p:{%s}, .pszGetTPRRC=%p:{%s}, .pszWriteMSRRC=%p:{%s}, .pszReadMSRRC=%p:{%s}, .pszBusDeliverRC=%p:{%s}} ppApicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pApicReg, pApicReg->u32Version, pApicReg->pfnGetInterruptR3, pApicReg->pfnSetBaseR3,
             pApicReg->pfnGetBaseR3, pApicReg->pfnSetTPRR3, pApicReg->pfnGetTPRR3, pApicReg->pfnWriteMSRR3, pApicReg->pfnReadMSRR3, pApicReg->pfnBusDeliverR3, pApicReg->pszGetInterruptRC,
             pApicReg->pszGetInterruptRC, pApicReg->pszSetBaseRC, pApicReg->pszSetBaseRC, pApicReg->pszGetBaseRC, pApicReg->pszGetBaseRC,
             pApicReg->pszSetTPRRC, pApicReg->pszSetTPRRC, pApicReg->pszGetTPRRC, pApicReg->pszGetTPRRC, pApicReg->pszWriteMSRRC, pApicReg->pszWriteMSRRC, pApicReg->pszReadMSRRC, pApicReg->pszReadMSRRC, pApicReg->pszBusDeliverRC,
             pApicReg->pszBusDeliverRC, ppApicHlpR3));

    /*
     * Validate input.
     */
    if (pApicReg->u32Version != PDM_APICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pApicReg->u32Version, PDM_APICREG_VERSION));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pApicReg->pfnGetInterruptR3
        ||  !pApicReg->pfnHasPendingIrqR3
        ||  !pApicReg->pfnSetBaseR3
        ||  !pApicReg->pfnGetBaseR3
        ||  !pApicReg->pfnSetTPRR3
        ||  !pApicReg->pfnGetTPRR3
        ||  !pApicReg->pfnWriteMSRR3
        ||  !pApicReg->pfnReadMSRR3
        ||  !pApicReg->pfnBusDeliverR3)
    {
        Assert(pApicReg->pfnGetInterruptR3);
        Assert(pApicReg->pfnHasPendingIrqR3);
        Assert(pApicReg->pfnSetBaseR3);
        Assert(pApicReg->pfnGetBaseR3);
        Assert(pApicReg->pfnSetTPRR3);
        Assert(pApicReg->pfnGetTPRR3);
        Assert(pApicReg->pfnWriteMSRR3);
        Assert(pApicReg->pfnReadMSRR3);
        Assert(pApicReg->pfnBusDeliverR3);
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc (R3 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (   (    pApicReg->pszGetInterruptRC
            ||  pApicReg->pszHasPendingIrqRC
            ||  pApicReg->pszSetBaseRC
            ||  pApicReg->pszGetBaseRC
            ||  pApicReg->pszSetTPRRC
            ||  pApicReg->pszGetTPRRC
            ||  pApicReg->pszWriteMSRRC
            ||  pApicReg->pszReadMSRRC
            ||  pApicReg->pszBusDeliverRC)
        &&  (   !VALID_PTR(pApicReg->pszGetInterruptRC)
            ||  !VALID_PTR(pApicReg->pszHasPendingIrqRC)
            ||  !VALID_PTR(pApicReg->pszSetBaseRC)
            ||  !VALID_PTR(pApicReg->pszGetBaseRC)
            ||  !VALID_PTR(pApicReg->pszSetTPRRC)
            ||  !VALID_PTR(pApicReg->pszGetTPRRC)
            ||  !VALID_PTR(pApicReg->pszWriteMSRRC)
            ||  !VALID_PTR(pApicReg->pszReadMSRRC)
            ||  !VALID_PTR(pApicReg->pszBusDeliverRC))
       )
    {
        Assert(VALID_PTR(pApicReg->pszGetInterruptRC));
        Assert(VALID_PTR(pApicReg->pszHasPendingIrqRC));
        Assert(VALID_PTR(pApicReg->pszSetBaseRC));
        Assert(VALID_PTR(pApicReg->pszGetBaseRC));
        Assert(VALID_PTR(pApicReg->pszSetTPRRC));
        Assert(VALID_PTR(pApicReg->pszGetTPRRC));
        Assert(VALID_PTR(pApicReg->pszReadMSRRC));
        Assert(VALID_PTR(pApicReg->pszWriteMSRRC));
        Assert(VALID_PTR(pApicReg->pszBusDeliverRC));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc (RC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (   (    pApicReg->pszGetInterruptR0
            ||  pApicReg->pszHasPendingIrqR0
            ||  pApicReg->pszSetBaseR0
            ||  pApicReg->pszGetBaseR0
            ||  pApicReg->pszSetTPRR0
            ||  pApicReg->pszGetTPRR0
            ||  pApicReg->pszWriteMSRR0
            ||  pApicReg->pszReadMSRR0
            ||  pApicReg->pszBusDeliverR0)
        &&  (   !VALID_PTR(pApicReg->pszGetInterruptR0)
            ||  !VALID_PTR(pApicReg->pszHasPendingIrqR0)
            ||  !VALID_PTR(pApicReg->pszSetBaseR0)
            ||  !VALID_PTR(pApicReg->pszGetBaseR0)
            ||  !VALID_PTR(pApicReg->pszSetTPRR0)
            ||  !VALID_PTR(pApicReg->pszGetTPRR0)
            ||  !VALID_PTR(pApicReg->pszReadMSRR0)
            ||  !VALID_PTR(pApicReg->pszWriteMSRR0)
            ||  !VALID_PTR(pApicReg->pszBusDeliverR0))
       )
    {
        Assert(VALID_PTR(pApicReg->pszGetInterruptR0));
        Assert(VALID_PTR(pApicReg->pszHasPendingIrqR0));
        Assert(VALID_PTR(pApicReg->pszSetBaseR0));
        Assert(VALID_PTR(pApicReg->pszGetBaseR0));
        Assert(VALID_PTR(pApicReg->pszSetTPRR0));
        Assert(VALID_PTR(pApicReg->pszGetTPRR0));
        Assert(VALID_PTR(pApicReg->pszReadMSRR0));
        Assert(VALID_PTR(pApicReg->pszWriteMSRR0));
        Assert(VALID_PTR(pApicReg->pszBusDeliverR0));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc (R0 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppApicHlpR3)
    {
        Assert(ppApicHlpR3);
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc (ppApicHlpR3)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one APIC device. On SMP we have single logical device covering all LAPICs,
     * as they need to communicate and share state easily.
     */
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (pVM->pdm.s.Apic.pDevInsR3)
    {
        AssertMsgFailed(("Only one apic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve & initialize the RC bits.
     */
    if (pApicReg->pszGetInterruptRC)
    {
        int rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszGetInterruptRC, &pVM->pdm.s.Apic.pfnGetInterruptRC);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszGetInterruptRC, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszHasPendingIrqRC, &pVM->pdm.s.Apic.pfnHasPendingIrqRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszHasPendingIrqRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszSetBaseRC, &pVM->pdm.s.Apic.pfnSetBaseRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszSetBaseRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszGetBaseRC, &pVM->pdm.s.Apic.pfnGetBaseRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszGetBaseRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszSetTPRRC, &pVM->pdm.s.Apic.pfnSetTPRRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszSetTPRRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszGetTPRRC, &pVM->pdm.s.Apic.pfnGetTPRRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszGetTPRRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszWriteMSRRC, &pVM->pdm.s.Apic.pfnWriteMSRRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszWriteMSRRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszReadMSRRC, &pVM->pdm.s.Apic.pfnReadMSRRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszReadMSRRC, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pApicReg->pszBusDeliverRC, &pVM->pdm.s.Apic.pfnBusDeliverRC);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pApicReg->pszBusDeliverRC, rc));
        }
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Apic.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.Apic.pDevInsRC           = 0;
        pVM->pdm.s.Apic.pfnGetInterruptRC   = 0;
        pVM->pdm.s.Apic.pfnHasPendingIrqRC  = 0;
        pVM->pdm.s.Apic.pfnSetBaseRC        = 0;
        pVM->pdm.s.Apic.pfnGetBaseRC        = 0;
        pVM->pdm.s.Apic.pfnSetTPRRC         = 0;
        pVM->pdm.s.Apic.pfnGetTPRRC         = 0;
        pVM->pdm.s.Apic.pfnWriteMSRRC       = 0;
        pVM->pdm.s.Apic.pfnReadMSRRC        = 0;
        pVM->pdm.s.Apic.pfnBusDeliverRC     = 0;
    }

    /*
     * Resolve & initialize the R0 bits.
     */
    if (pApicReg->pszGetInterruptR0)
    {
        int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetInterruptR0, &pVM->pdm.s.Apic.pfnGetInterruptR0);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetInterruptR0, rc));
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszHasPendingIrqR0, &pVM->pdm.s.Apic.pfnHasPendingIrqR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszHasPendingIrqR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszSetBaseR0, &pVM->pdm.s.Apic.pfnSetBaseR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszSetBaseR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetBaseR0, &pVM->pdm.s.Apic.pfnGetBaseR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetBaseR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszSetTPRR0, &pVM->pdm.s.Apic.pfnSetTPRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszSetTPRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszGetTPRR0, &pVM->pdm.s.Apic.pfnGetTPRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszGetTPRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszWriteMSRR0, &pVM->pdm.s.Apic.pfnWriteMSRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszWriteMSRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszReadMSRR0, &pVM->pdm.s.Apic.pfnReadMSRR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszReadMSRR0, rc));
        }
        if (RT_SUCCESS(rc))
        {
            rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pApicReg->pszBusDeliverR0, &pVM->pdm.s.Apic.pfnBusDeliverR0);
            AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pApicReg->pszBusDeliverR0, rc));
        }
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.Apic.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
        Assert(pVM->pdm.s.Apic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.Apic.pfnGetInterruptR0   = 0;
        pVM->pdm.s.Apic.pfnHasPendingIrqR0  = 0;
        pVM->pdm.s.Apic.pfnSetBaseR0        = 0;
        pVM->pdm.s.Apic.pfnGetBaseR0        = 0;
        pVM->pdm.s.Apic.pfnSetTPRR0         = 0;
        pVM->pdm.s.Apic.pfnGetTPRR0         = 0;
        pVM->pdm.s.Apic.pfnWriteMSRR0       = 0;
        pVM->pdm.s.Apic.pfnReadMSRR0        = 0;
        pVM->pdm.s.Apic.pfnBusDeliverR0     = 0;
        pVM->pdm.s.Apic.pDevInsR0           = 0;
    }

    /*
     * Initialize the HC bits.
     */
    pVM->pdm.s.Apic.pDevInsR3           = pDevIns;
    pVM->pdm.s.Apic.pfnGetInterruptR3   = pApicReg->pfnGetInterruptR3;
    pVM->pdm.s.Apic.pfnHasPendingIrqR3  = pApicReg->pfnHasPendingIrqR3;
    pVM->pdm.s.Apic.pfnSetBaseR3        = pApicReg->pfnSetBaseR3;
    pVM->pdm.s.Apic.pfnGetBaseR3        = pApicReg->pfnGetBaseR3;
    pVM->pdm.s.Apic.pfnSetTPRR3         = pApicReg->pfnSetTPRR3;
    pVM->pdm.s.Apic.pfnGetTPRR3         = pApicReg->pfnGetTPRR3;
    pVM->pdm.s.Apic.pfnWriteMSRR3       = pApicReg->pfnWriteMSRR3;
    pVM->pdm.s.Apic.pfnReadMSRR3        = pApicReg->pfnReadMSRR3;
    pVM->pdm.s.Apic.pfnBusDeliverR3     = pApicReg->pfnBusDeliverR3;
    Log(("PDM: Registered APIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppApicHlpR3 = &g_pdmR3DevApicHlp;
    LogFlow(("pdmR3DevHlp_APICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLPR3::pfnIOAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: pIoApicReg=%p:{.u32Version=%#x, .pfnSetIrqR3=%p, .pszSetIrqRC=%p:{%s}, .pszSetIrqR0=%p:{%s}} ppIoApicHlpR3=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pIoApicReg, pIoApicReg->u32Version, pIoApicReg->pfnSetIrqR3,
             pIoApicReg->pszSetIrqRC, pIoApicReg->pszSetIrqRC, pIoApicReg->pszSetIrqR0, pIoApicReg->pszSetIrqR0, ppIoApicHlpR3));

    /*
     * Validate input.
     */
    if (pIoApicReg->u32Version != PDM_IOAPICREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pIoApicReg->u32Version, PDM_IOAPICREG_VERSION));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (version)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!pIoApicReg->pfnSetIrqR3)
    {
        Assert(pIoApicReg->pfnSetIrqR3);
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (R3 callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqRC
        &&  !VALID_PTR(pIoApicReg->pszSetIrqRC))
    {
        Assert(VALID_PTR(pIoApicReg->pszSetIrqRC));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqR0
        &&  !VALID_PTR(pIoApicReg->pszSetIrqR0))
    {
        Assert(VALID_PTR(pIoApicReg->pszSetIrqR0));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (GC callbacks)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (!ppIoApicHlpR3)
    {
        Assert(ppIoApicHlpR3);
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (ppApicHlp)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * The I/O APIC requires the APIC to be present (hacks++).
     * If the I/O APIC does GC stuff so must the APIC.
     */
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (!pVM->pdm.s.Apic.pDevInsR3)
    {
        AssertMsgFailed(("Configuration error / Init order error! No APIC!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (no APIC)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    pIoApicReg->pszSetIrqRC
        &&  !pVM->pdm.s.Apic.pDevInsRC)
    {
        AssertMsgFailed(("Configuration error! APIC doesn't do GC, I/O APIC does!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (no GC APIC)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one I/O APIC device.
     */
    if (pVM->pdm.s.IoApic.pDevInsR3)
    {
        AssertMsgFailed(("Only one ioapic device is supported!\n"));
        LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc (only one)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Resolve & initialize the GC bits.
     */
    if (pIoApicReg->pszSetIrqRC)
    {
        int rc = PDMR3LdrGetSymbolRCLazy(pVM, pDevIns->pDevReg->szRCMod, pIoApicReg->pszSetIrqRC, &pVM->pdm.s.IoApic.pfnSetIrqRC);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szRCMod, pIoApicReg->pszSetIrqRC, rc));
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.IoApic.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    }
    else
    {
        pVM->pdm.s.IoApic.pDevInsRC   = 0;
        pVM->pdm.s.IoApic.pfnSetIrqRC = 0;
    }

    /*
     * Resolve & initialize the R0 bits.
     */
    if (pIoApicReg->pszSetIrqR0)
    {
        int rc = PDMR3LdrGetSymbolR0Lazy(pVM, pDevIns->pDevReg->szR0Mod, pIoApicReg->pszSetIrqR0, &pVM->pdm.s.IoApic.pfnSetIrqR0);
        AssertMsgRC(rc, ("%s::%s rc=%Rrc\n", pDevIns->pDevReg->szR0Mod, pIoApicReg->pszSetIrqR0, rc));
        if (RT_FAILURE(rc))
        {
            LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
            return rc;
        }
        pVM->pdm.s.IoApic.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
        Assert(pVM->pdm.s.IoApic.pDevInsR0);
    }
    else
    {
        pVM->pdm.s.IoApic.pfnSetIrqR0 = 0;
        pVM->pdm.s.IoApic.pDevInsR0   = 0;
    }

    /*
     * Initialize the R3 bits.
     */
    pVM->pdm.s.IoApic.pDevInsR3   = pDevIns;
    pVM->pdm.s.IoApic.pfnSetIrqR3 = pIoApicReg->pfnSetIrqR3;
    Log(("PDM: Registered I/O APIC device '%s'/%d pDevIns=%p\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));

    /* set the helper pointer and return. */
    *ppIoApicHlpR3 = &g_pdmR3DevIoApicHlp;
    LogFlow(("pdmR3DevHlp_IOAPICRegister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VINF_SUCCESS));
    return VINF_SUCCESS;
}


/** @copydoc PDMDEVHLPR3::pfnDMACRegister */
static DECLCALLBACK(int) pdmR3DevHlp_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: pDmacReg=%p:{.u32Version=%#x, .pfnRun=%p, .pfnRegister=%p, .pfnReadMemory=%p, .pfnWriteMemory=%p, .pfnSetDREQ=%p, .pfnGetChannelMode=%p} ppDmacHlp=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDmacReg, pDmacReg->u32Version, pDmacReg->pfnRun, pDmacReg->pfnRegister,
             pDmacReg->pfnReadMemory, pDmacReg->pfnWriteMemory, pDmacReg->pfnSetDREQ, pDmacReg->pfnGetChannelMode, ppDmacHlp));

    /*
     * Validate input.
     */
    if (pDmacReg->u32Version != PDM_DMACREG_VERSION)
    {
        AssertMsgFailed(("u32Version=%#x expected %#x\n", pDmacReg->u32Version,
                         PDM_DMACREG_VERSION));
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Rrc (version)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }
    if (    !pDmacReg->pfnRun
        ||  !pDmacReg->pfnRegister
        ||  !pDmacReg->pfnReadMemory
        ||  !pDmacReg->pfnWriteMemory
        ||  !pDmacReg->pfnSetDREQ
        ||  !pDmacReg->pfnGetChannelMode)
    {
        Assert(pDmacReg->pfnRun);
        Assert(pDmacReg->pfnRegister);
        Assert(pDmacReg->pfnReadMemory);
        Assert(pDmacReg->pfnWriteMemory);
        Assert(pDmacReg->pfnSetDREQ);
        Assert(pDmacReg->pfnGetChannelMode);
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Rrc (callbacks)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    if (!ppDmacHlp)
    {
        Assert(ppDmacHlp);
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Rrc (ppDmacHlp)\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Only one DMA device.
     */
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (pVM->pdm.s.pDmac)
    {
        AssertMsgFailed(("Only one DMA device is supported!\n"));
        LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Rrc\n",
                 pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VERR_INVALID_PARAMETER));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Allocate and initialize pci bus structure.
     */
    int rc = VINF_SUCCESS;
    PPDMDMAC  pDmac = (PPDMDMAC)MMR3HeapAlloc(pDevIns->Internal.s.pVMR3, MM_TAG_PDM_DEVICE, sizeof(*pDmac));
    if (pDmac)
    {
        pDmac->pDevIns   = pDevIns;
        pDmac->Reg       = *pDmacReg;
        pVM->pdm.s.pDmac = pDmac;

        /* set the helper pointer. */
        *ppDmacHlp = &g_pdmR3DevDmacHlp;
        Log(("PDM: Registered DMAC device '%s'/%d pDevIns=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pDevIns));
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlow(("pdmR3DevHlp_DMACRegister: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysRead */
static DECLCALLBACK(int) pdmR3DevHlp_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3DevHlp_PhysRead: caller='%s'/%d: GCPhys=%RGp pvBuf=%p cbRead=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, pvBuf, cbRead));

#ifdef VBOX_WITH_NEW_PHYS_CODE
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    if (!VM_IS_EMT(pVM)) /** @todo not true for SMP. oh joy! */
    {
        char szNames[128];
        uint32_t cLocks = PDMR3CritSectCountOwned(pVM, szNames, sizeof(szNames));
        AssertMsg(cLocks == 0, ("cLocks=%u %s\n", cLocks, szNames));
    }
#endif

    int rc;
    if (VM_IS_EMT(pVM))
        rc = PGMPhysRead(pVM, GCPhys, pvBuf, cbRead);
    else
        rc = PGMR3PhysReadExternal(pVM, GCPhys, pvBuf, cbRead);
#else
    PGMPhysRead(pVM, GCPhys, pvBuf, cbRead);
    int rc = VINF_SUCCESS;
#endif
    Log(("pdmR3DevHlp_PhysRead: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysWrite */
static DECLCALLBACK(int) pdmR3DevHlp_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3DevHlp_PhysWrite: caller='%s'/%d: GCPhys=%RGp pvBuf=%p cbWrite=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, pvBuf, cbWrite));

#ifdef VBOX_WITH_NEW_PHYS_CODE
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    if (!VM_IS_EMT(pVM)) /** @todo not true for SMP. oh joy! */
    {
        char szNames[128];
        uint32_t cLocks = PDMR3CritSectCountOwned(pVM, szNames, sizeof(szNames));
        AssertMsg(cLocks == 0, ("cLocks=%u %s\n", cLocks, szNames));
    }
#endif

    int rc;
    if (VM_IS_EMT(pVM))
        rc = PGMPhysWrite(pVM, GCPhys, pvBuf, cbWrite);
    else
        rc = PGMR3PhysWriteExternal(pVM, GCPhys, pvBuf, cbWrite);
#else
    PGMPhysWrite(pVM, GCPhys, pvBuf, cbWrite);
    int rc = VINF_SUCCESS;
#endif
    Log(("pdmR3DevHlp_PhysWrite: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtr */
static DECLCALLBACK(int) pdmR3DevHlp_PhysGCPhys2CCPtr(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3DevHlp_PhysGCPhys2CCPtr: caller='%s'/%d: GCPhys=%RGp fFlags=%#x ppv=%p pLock=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, fFlags, ppv, pLock));
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

#ifdef VBOX_WITH_NEW_PHYS_CODE
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    if (!VM_IS_EMT(pVM)) /** @todo not true for SMP. oh joy! */
    {
        char szNames[128];
        uint32_t cLocks = PDMR3CritSectCountOwned(pVM, szNames, sizeof(szNames));
        AssertMsg(cLocks == 0, ("cLocks=%u %s\n", cLocks, szNames));
    }
#endif
#endif

    int rc = PGMR3PhysGCPhys2CCPtrExternal(pVM, GCPhys, ppv, pLock);

    Log(("pdmR3DevHlp_PhysGCPhys2CCPtr: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtrReadOnly */
static DECLCALLBACK(int) pdmR3DevHlp_PhysGCPhys2CCPtrReadOnly(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, const void **ppv, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3DevHlp_PhysGCPhys2CCPtrReadOnly: caller='%s'/%d: GCPhys=%RGp fFlags=%#x ppv=%p pLock=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhys, fFlags, ppv, pLock));
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

#ifdef VBOX_WITH_NEW_PHYS_CODE
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    if (!VM_IS_EMT(pVM)) /** @todo not true for SMP. oh joy! */
    {
        char szNames[128];
        uint32_t cLocks = PDMR3CritSectCountOwned(pVM, szNames, sizeof(szNames));
        AssertMsg(cLocks == 0, ("cLocks=%u %s\n", cLocks, szNames));
    }
#endif
#endif

    int rc = PGMR3PhysGCPhys2CCPtrReadOnlyExternal(pVM, GCPhys, ppv, pLock);

    Log(("pdmR3DevHlp_PhysGCPhys2CCPtrReadOnly: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysReleasePageMappingLock */
static DECLCALLBACK(void) pdmR3DevHlp_PhysReleasePageMappingLock(PPDMDEVINS pDevIns, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    LogFlow(("pdmR3DevHlp_PhysReleasePageMappingLock: caller='%s'/%d: pLock=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pLock));

    PGMPhysReleasePageMappingLock(pVM, pLock);

    Log(("pdmR3DevHlp_PhysReleasePageMappingLock: caller='%s'/%d: returns void\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnPhysReadGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysReadGCVirt: caller='%s'/%d: pvDst=%p GCVirt=%RGv cb=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, pvDst, GCVirtSrc, cb));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    /** @todo SMP. */
#endif

    int rc = PGMPhysSimpleReadGCPtr(pVM, pvDst, GCVirtSrc, cb);

    LogFlow(("pdmR3DevHlp_PhysReadGCVirt: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));

    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysWriteGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysWriteGCVirt: caller='%s'/%d: GCVirtDst=%RGv pvSrc=%p cb=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCVirtDst, pvSrc, cb));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    /** @todo SMP. */
#endif

    int rc = PGMPhysSimpleWriteGCPtr(pVM, GCVirtDst, pvSrc, cb);

    LogFlow(("pdmR3DevHlp_PhysWriteGCVirt: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));

    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnPhysGCPtr2GCPhys */
static DECLCALLBACK(int) pdmR3DevHlp_PhysGCPtr2GCPhys(PPDMDEVINS pDevIns, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_PhysGCPtr2GCPhys: caller='%s'/%d: GCPtr=%RGv pGCPhys=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPtr, pGCPhys));

    if (!VM_IS_EMT(pVM))
        return VERR_ACCESS_DENIED;
#if defined(VBOX_STRICT) && defined(PDM_DEVHLP_DEADLOCK_DETECTION)
    /** @todo SMP. */
#endif

    int rc = PGMPhysGCPtr2GCPhys(pVM, GCPtr, pGCPhys);

    LogFlow(("pdmR3DevHlp_PhysGCPtr2GCPhys: caller='%s'/%d: returns %Rrc *pGCPhys=%RGp\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *pGCPhys));

    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMState */
static DECLCALLBACK(VMSTATE) pdmR3DevHlp_VMState(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);

    VMSTATE enmVMState = VMR3GetState(pDevIns->Internal.s.pVMR3);

    LogFlow(("pdmR3DevHlp_VMState: caller='%s'/%d: returns %d (%s)\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
             enmVMState, VMR3GetStateName(enmVMState)));
    return enmVMState;
}


/** @copydoc PDMDEVHLPR3::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR3DevHlp_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    bool fRc = PGMPhysIsA20Enabled(pDevIns->Internal.s.pVMR3);

    LogFlow(("pdmR3DevHlp_A20IsEnabled: caller='%s'/%d: returns %d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, fRc));
    return fRc;
}


/** @copydoc PDMDEVHLPR3::pfnA20Set */
static DECLCALLBACK(void) pdmR3DevHlp_A20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_A20Set: caller='%s'/%d: fEnable=%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, fEnable));
    //Assert(*(unsigned *)&fEnable <= 1);
    PGMR3PhysSetA20(pDevIns->Internal.s.pVMR3, fEnable);
}


/** @copydoc PDMDEVHLPR3::pfnVMReset */
static DECLCALLBACK(int) pdmR3DevHlp_VMReset(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_VMReset: caller='%s'/%d: VM_FF_RESET %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_RESET)));

    /*
     * We postpone this operation because we're likely to be inside a I/O instruction
     * and the EIP will be updated when we return.
     * We still return VINF_EM_RESET to break out of any execution loops and force FF evaluation.
     */
    bool fHaltOnReset;
    int rc = CFGMR3QueryBool(CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM"), "HaltOnReset", &fHaltOnReset);
    if (RT_SUCCESS(rc) && fHaltOnReset)
    {
        Log(("pdmR3DevHlp_VMReset: Halt On Reset!\n"));
        rc = VINF_EM_HALT;
    }
    else
    {
        VM_FF_SET(pVM, VM_FF_RESET);
        rc = VINF_EM_RESET;
    }

    LogFlow(("pdmR3DevHlp_VMReset: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMSuspend */
static DECLCALLBACK(int) pdmR3DevHlp_VMSuspend(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_VMSuspend: caller='%s'/%d:\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));

    int rc = VMR3Suspend(pDevIns->Internal.s.pVMR3);

    LogFlow(("pdmR3DevHlp_VMSuspend: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMPowerOff */
static DECLCALLBACK(int) pdmR3DevHlp_VMPowerOff(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_VMPowerOff: caller='%s'/%d:\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));

    int rc = VMR3PowerOff(pDevIns->Internal.s.pVMR3);

    LogFlow(("pdmR3DevHlp_VMPowerOff: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnLockVM */
static DECLCALLBACK(int) pdmR3DevHlp_LockVM(PPDMDEVINS pDevIns)
{
    return VMMR3Lock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMDEVHLPR3::pfnUnlockVM */
static DECLCALLBACK(int) pdmR3DevHlp_UnlockVM(PPDMDEVINS pDevIns)
{
    return VMMR3Unlock(pDevIns->Internal.s.pVMR3);
}


/** @copydoc PDMDEVHLPR3::pfnAssertVMLock */
static DECLCALLBACK(bool) pdmR3DevHlp_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PVM pVM = pDevIns->Internal.s.pVMR3;
    if (VMMR3LockIsOwner(pVM))
        return true;

    RTNATIVETHREAD NativeThreadOwner = VMMR3LockGetOwner(pVM);
    RTTHREAD ThreadOwner = RTThreadFromNative(NativeThreadOwner);
    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertVMLocked '%s'/%d ThreadOwner=%RTnthrd/%RTthrd/'%s' Self='%s'\n",
                pDevIns->pDevReg->szDeviceName, pDevIns->iInstance,
                NativeThreadOwner, ThreadOwner, RTThreadGetName(ThreadOwner), RTThreadSelfName());
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    return false;
}

/** @copydoc PDMDEVHLPR3::pfnDMARegister */
static DECLCALLBACK(int) pdmR3DevHlp_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMARegister: caller='%s'/%d: uChannel=%d pfnTransferHandler=%p pvUser=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pfnTransferHandler, pvUser));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
        pVM->pdm.s.pDmac->Reg.pfnRegister(pVM->pdm.s.pDmac->pDevIns, uChannel, pfnTransferHandler, pvUser);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMARegister: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLPR3::pfnDMAReadMemory */
static DECLCALLBACK(int) pdmR3DevHlp_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAReadMemory: caller='%s'/%d: uChannel=%d pvBuffer=%p off=%#x cbBlock=%#x pcbRead=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pvBuffer, off, cbBlock, pcbRead));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
    {
        uint32_t cb = pVM->pdm.s.pDmac->Reg.pfnReadMemory(pVM->pdm.s.pDmac->pDevIns, uChannel, pvBuffer, off, cbBlock);
        if (pcbRead)
            *pcbRead = cb;
    }
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMAReadMemory: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLPR3::pfnDMAWriteMemory */
static DECLCALLBACK(int) pdmR3DevHlp_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAWriteMemory: caller='%s'/%d: uChannel=%d pvBuffer=%p off=%#x cbBlock=%#x pcbWritten=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, pvBuffer, off, cbBlock, pcbWritten));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
    {
        uint32_t cb = pVM->pdm.s.pDmac->Reg.pfnWriteMemory(pVM->pdm.s.pDmac->pDevIns, uChannel, pvBuffer, off, cbBlock);
        if (pcbWritten)
            *pcbWritten = cb;
    }
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMAWriteMemory: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLPR3::pfnDMASetDREQ */
static DECLCALLBACK(int) pdmR3DevHlp_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMASetDREQ: caller='%s'/%d: uChannel=%d uLevel=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel, uLevel));
    int rc = VINF_SUCCESS;
    if (pVM->pdm.s.pDmac)
        pVM->pdm.s.pDmac->Reg.pfnSetDREQ(pVM->pdm.s.pDmac->pDevIns, uChannel, uLevel);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        rc = VERR_PDM_NO_DMAC_INSTANCE;
    }
    LogFlow(("pdmR3DevHlp_DMASetDREQ: caller='%s'/%d: returns %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}

/** @copydoc PDMDEVHLPR3::pfnDMAGetChannelMode */
static DECLCALLBACK(uint8_t) pdmR3DevHlp_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMAGetChannelMode: caller='%s'/%d: uChannel=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, uChannel));
    uint8_t u8Mode;
    if (pVM->pdm.s.pDmac)
        u8Mode = pVM->pdm.s.pDmac->Reg.pfnGetChannelMode(pVM->pdm.s.pDmac->pDevIns, uChannel);
    else
    {
        AssertMsgFailed(("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
        u8Mode = 3 << 2 /* illegal mode type */;
    }
    LogFlow(("pdmR3DevHlp_DMAGetChannelMode: caller='%s'/%d: returns %#04x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, u8Mode));
    return u8Mode;
}

/** @copydoc PDMDEVHLPR3::pfnDMASchedule */
static DECLCALLBACK(void) pdmR3DevHlp_DMASchedule(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);
    LogFlow(("pdmR3DevHlp_DMASchedule: caller='%s'/%d: VM_FF_PDM_DMA %d -> 1\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, VM_FF_ISSET(pVM, VM_FF_PDM_DMA)));

    AssertMsg(pVM->pdm.s.pDmac, ("Configuration error: No DMAC controller available. This could be related to init order too!\n"));
    VM_FF_SET(pVM, VM_FF_PDM_DMA);
    REMR3NotifyDmaPending(pVM);
    VMR3NotifyFF(pVM, true);
}


/** @copydoc PDMDEVHLPR3::pfnCMOSWrite */
static DECLCALLBACK(int) pdmR3DevHlp_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: iReg=%#04x u8Value=%#04x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iReg, u8Value));
    int rc;
    if (pVM->pdm.s.pRtc)
        rc = pVM->pdm.s.pRtc->Reg.pfnWrite(pVM->pdm.s.pRtc->pDevIns, iReg, u8Value);
    else
        rc = VERR_PDM_NO_RTC_INSTANCE;

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: return %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnCMOSRead */
static DECLCALLBACK(int) pdmR3DevHlp_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    PVM pVM = pDevIns->Internal.s.pVMR3;
    VM_ASSERT_EMT(pVM);

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: iReg=%#04x pu8Value=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iReg, pu8Value));
    int rc;
    if (pVM->pdm.s.pRtc)
        rc = pVM->pdm.s.pRtc->Reg.pfnRead(pVM->pdm.s.pRtc->pDevIns, iReg, pu8Value);
    else
        rc = VERR_PDM_NO_RTC_INSTANCE;

    LogFlow(("pdmR3DevHlp_CMOSWrite: caller='%s'/%d: return %Rrc\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnGetCpuId */
static DECLCALLBACK(void) pdmR3DevHlp_GetCpuId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                               uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_GetCpuId: caller='%s'/%d: iLeaf=%d pEax=%p pEbx=%p pEcx=%p pEdx=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iLeaf, pEax, pEbx, pEcx, pEdx));
    AssertPtr(pEax); AssertPtr(pEbx); AssertPtr(pEcx); AssertPtr(pEdx);

    CPUMGetGuestCpuId(pDevIns->Internal.s.pVMR3, iLeaf, pEax, pEbx, pEcx, pEdx);

    LogFlow(("pdmR3DevHlp_GetCpuId: caller='%s'/%d: returns void - *pEax=%#x *pEbx=%#x *pEcx=%#x *pEdx=%#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, *pEax, *pEbx, *pEcx, *pEdx));
}


/** @copydoc PDMDEVHLPR3::pfnROMProtectShadow */
static DECLCALLBACK(int) pdmR3DevHlp_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, PGMROMPROT enmProt)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    LogFlow(("pdmR3DevHlp_ROMProtectShadow: caller='%s'/%d: GCPhysStart=%RGp cbRange=%#x enmProt=%d\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, GCPhysStart, cbRange, enmProt));

#ifdef VBOX_WITH_NEW_PHYS_CODE
    int rc = PGMR3PhysRomProtect(pDevIns->Internal.s.pVMR3, GCPhysStart, cbRange, enmProt);
#else
    int rc = MMR3PhysRomProtect(pDevIns->Internal.s.pVMR3, GCPhysStart, cbRange);
#endif

    LogFlow(("pdmR3DevHlp_ROMProtectShadow: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Register
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIO2Register(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIO2Register: caller='%s'/%d: iRegion=#x cb=%#RGp fFlags=%RX32 ppv=%p pszDescp=%p:{%s}\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, cb, fFlags, ppv, pszDesc, pszDesc));

    int rc = PGMR3PhysMMIO2Register(pDevIns->Internal.s.pVMR3, pDevIns, iRegion, cb, fFlags, ppv, pszDesc);

    LogFlow(("pdmR3DevHlp_MMIO2Register: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Deregister
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIO2Deregister(PPDMDEVINS pDevIns, uint32_t iRegion)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIO2Deregister: caller='%s'/%d: iRegion=#x\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion));

    AssertReturn(iRegion == UINT32_MAX, VERR_INVALID_PARAMETER);

    int rc = PGMR3PhysMMIO2Deregister(pDevIns->Internal.s.pVMR3, pDevIns, iRegion);

    LogFlow(("pdmR3DevHlp_MMIO2Deregister: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Map
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIO2Map(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIO2Map: caller='%s'/%d: iRegion=#x GCPhys=%#RGp\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, GCPhys));

    int rc = PGMR3PhysMMIO2Map(pDevIns->Internal.s.pVMR3, pDevIns, iRegion, GCPhys);

    LogFlow(("pdmR3DevHlp_MMIO2Map: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2Unmap
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIO2Unmap(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIO2Unmap: caller='%s'/%d: iRegion=#x GCPhys=%#RGp\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, GCPhys));

    int rc = PGMR3PhysMMIO2Unmap(pDevIns->Internal.s.pVMR3, pDevIns, iRegion, GCPhys);

    LogFlow(("pdmR3DevHlp_MMIO2Unmap: caller='%s'/%d: returns %Rrc\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMHyperMapMMIO2
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMHyperMapMMIO2(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                                     const char *pszDesc, PRTRCPTR pRCPtr)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMHyperMapMMIO2: caller='%s'/%d: iRegion=#x off=%RGp cb=%RGp pszDesc=%p:{%s} pRCPtr=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, off, cb, pszDesc, pszDesc, pRCPtr));

    int rc = MMR3HyperMapMMIO2(pDevIns->Internal.s.pVMR3, pDevIns, iRegion, off, cb, pszDesc, pRCPtr);

    LogFlow(("pdmR3DevHlp_MMHyperMapMMIO2: caller='%s'/%d: returns %Rrc *pRCPtr=%RRv\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *pRCPtr));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnMMIO2MapKernel
 */
static DECLCALLBACK(int) pdmR3DevHlp_MMIO2MapKernel(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb,
                                                    const char *pszDesc, PRTR0PTR pR0Ptr)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);
    LogFlow(("pdmR3DevHlp_MMIO2MapKernel: caller='%s'/%d: iRegion=#x off=%RGp cb=%RGp pszDesc=%p:{%s} pR0Ptr=%p\n",
             pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, iRegion, off, cb, pszDesc, pszDesc, pR0Ptr));

    int rc = PGMR3PhysMMIO2MapKernel(pDevIns->Internal.s.pVMR3, pDevIns, iRegion, off, cb, pszDesc, pR0Ptr);

    LogFlow(("pdmR3DevHlp_MMIO2MapKernel: caller='%s'/%d: returns %Rrc *pR0Ptr=%RHv\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance, rc, *pR0Ptr));
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnRegisterVMMDevHeap
 */
static DECLCALLBACK(int) pdmR3DevHlp_RegisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    int rc = PDMR3RegisterVMMDevHeap(pDevIns->Internal.s.pVMR3, GCPhys, pvHeap, cbSize);
    return rc;
}


/**
 * @copydoc PDMDEVHLPR3::pfnUnregisterVMMDevHeap
 */
static DECLCALLBACK(int) pdmR3DevHlp_UnregisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    VM_ASSERT_EMT(pDevIns->Internal.s.pVMR3);

    int rc = PDMR3UnregisterVMMDevHeap(pDevIns->Internal.s.pVMR3, GCPhys);
    return rc;
}


/**
 * The device helper structure for trusted devices.
 */
const PDMDEVHLPR3 g_pdmR3DevHlpTrusted =
{
    PDM_DEVHLP_VERSION,
    pdmR3DevHlp_IOPortRegister,
    pdmR3DevHlp_IOPortRegisterGC,
    pdmR3DevHlp_IOPortRegisterR0,
    pdmR3DevHlp_IOPortDeregister,
    pdmR3DevHlp_MMIORegister,
    pdmR3DevHlp_MMIORegisterGC,
    pdmR3DevHlp_MMIORegisterR0,
    pdmR3DevHlp_MMIODeregister,
    pdmR3DevHlp_ROMRegister,
    pdmR3DevHlp_SSMRegister,
    pdmR3DevHlp_TMTimerCreate,
    pdmR3DevHlp_TMTimerCreateExternal,
    pdmR3DevHlp_PCIRegister,
    pdmR3DevHlp_PCIIORegionRegister,
    pdmR3DevHlp_PCISetConfigCallbacks,
    pdmR3DevHlp_PCISetIrq,
    pdmR3DevHlp_PCISetIrqNoWait,
    pdmR3DevHlp_ISASetIrq,
    pdmR3DevHlp_ISASetIrqNoWait,
    pdmR3DevHlp_DriverAttach,
    pdmR3DevHlp_MMHeapAlloc,
    pdmR3DevHlp_MMHeapAllocZ,
    pdmR3DevHlp_MMHeapFree,
    pdmR3DevHlp_VMSetError,
    pdmR3DevHlp_VMSetErrorV,
    pdmR3DevHlp_VMSetRuntimeError,
    pdmR3DevHlp_VMSetRuntimeErrorV,
    pdmR3DevHlp_AssertEMT,
    pdmR3DevHlp_AssertOther,
    pdmR3DevHlp_DBGFStopV,
    pdmR3DevHlp_DBGFInfoRegister,
    pdmR3DevHlp_STAMRegister,
    pdmR3DevHlp_STAMRegisterF,
    pdmR3DevHlp_STAMRegisterV,
    pdmR3DevHlp_RTCRegister,
    pdmR3DevHlp_PDMQueueCreate,
    pdmR3DevHlp_CritSectInit,
    pdmR3DevHlp_UTCNow,
    pdmR3DevHlp_PDMThreadCreate,
    pdmR3DevHlp_PhysGCPtr2GCPhys,
    pdmR3DevHlp_VMState,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    pdmR3DevHlp_GetVM,
    pdmR3DevHlp_PCIBusRegister,
    pdmR3DevHlp_PICRegister,
    pdmR3DevHlp_APICRegister,
    pdmR3DevHlp_IOAPICRegister,
    pdmR3DevHlp_DMACRegister,
    pdmR3DevHlp_PhysRead,
    pdmR3DevHlp_PhysWrite,
    pdmR3DevHlp_PhysGCPhys2CCPtr,
    pdmR3DevHlp_PhysGCPhys2CCPtrReadOnly,
    pdmR3DevHlp_PhysReleasePageMappingLock,
    pdmR3DevHlp_PhysReadGCVirt,
    pdmR3DevHlp_PhysWriteGCVirt,
    pdmR3DevHlp_A20IsEnabled,
    pdmR3DevHlp_A20Set,
    pdmR3DevHlp_VMReset,
    pdmR3DevHlp_VMSuspend,
    pdmR3DevHlp_VMPowerOff,
    pdmR3DevHlp_LockVM,
    pdmR3DevHlp_UnlockVM,
    pdmR3DevHlp_AssertVMLock,
    pdmR3DevHlp_DMARegister,
    pdmR3DevHlp_DMAReadMemory,
    pdmR3DevHlp_DMAWriteMemory,
    pdmR3DevHlp_DMASetDREQ,
    pdmR3DevHlp_DMAGetChannelMode,
    pdmR3DevHlp_DMASchedule,
    pdmR3DevHlp_CMOSWrite,
    pdmR3DevHlp_CMOSRead,
    pdmR3DevHlp_GetCpuId,
    pdmR3DevHlp_ROMProtectShadow,
    pdmR3DevHlp_MMIO2Register,
    pdmR3DevHlp_MMIO2Deregister,
    pdmR3DevHlp_MMIO2Map,
    pdmR3DevHlp_MMIO2Unmap,
    pdmR3DevHlp_MMHyperMapMMIO2,
    pdmR3DevHlp_MMIO2MapKernel,
    pdmR3DevHlp_RegisterVMMDevHeap,
    pdmR3DevHlp_UnregisterVMMDevHeap,
    PDM_DEVHLP_VERSION /* the end */
};




/** @copydoc PDMDEVHLPR3::pfnGetVM */
static DECLCALLBACK(PVM) pdmR3DevHlp_Untrusted_GetVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return NULL;
}


/** @copydoc PDMDEVHLPR3::pfnPCIBusRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PCIBusRegister(PPDMDEVINS pDevIns, PPDMPCIBUSREG pPciBusReg, PCPDMPCIHLPR3 *ppPciHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pPciBusReg);
    NOREF(ppPciHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PICRegister(PPDMDEVINS pDevIns, PPDMPICREG pPicReg, PCPDMPICHLPR3 *ppPicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pPicReg);
    NOREF(ppPicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_APICRegister(PPDMDEVINS pDevIns, PPDMAPICREG pApicReg, PCPDMAPICHLPR3 *ppApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pApicReg);
    NOREF(ppApicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnIOAPICRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_IOAPICRegister(PPDMDEVINS pDevIns, PPDMIOAPICREG pIoApicReg, PCPDMIOAPICHLPR3 *ppIoApicHlpR3)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pIoApicReg);
    NOREF(ppIoApicHlpR3);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnDMACRegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMACRegister(PPDMDEVINS pDevIns, PPDMDMACREG pDmacReg, PCPDMDMACHLP *ppDmacHlp)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pDmacReg);
    NOREF(ppDmacHlp);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysRead */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(pvBuf);
    NOREF(cbRead);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysWrite */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(pvBuf);
    NOREF(cbWrite);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtr */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysGCPhys2CCPtr(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(fFlags);
    NOREF(ppv);
    NOREF(pLock);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysGCPhys2CCPtrReadOnly */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysGCPhys2CCPtrReadOnly(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, uint32_t fFlags, const void **ppv, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCPhys);
    NOREF(fFlags);
    NOREF(ppv);
    NOREF(pLock);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysReleasePageMappingLock */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_PhysReleasePageMappingLock(PPDMDEVINS pDevIns, PPGMPAGEMAPLOCK pLock)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pLock);
}


/** @copydoc PDMDEVHLPR3::pfnPhysReadGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysReadGCVirt(PPDMDEVINS pDevIns, void *pvDst, RTGCPTR GCVirtSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(pvDst);
    NOREF(GCVirtSrc);
    NOREF(cb);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnPhysWriteGCVirt */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_PhysWriteGCVirt(PPDMDEVINS pDevIns, RTGCPTR GCVirtDst, const void *pvSrc, size_t cb)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(GCVirtDst);
    NOREF(pvSrc);
    NOREF(cb);
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnA20IsEnabled */
static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_A20IsEnabled(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return false;
}


/** @copydoc PDMDEVHLPR3::pfnA20Set */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_A20Set(PPDMDEVINS pDevIns, bool fEnable)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    NOREF(fEnable);
}


/** @copydoc PDMDEVHLPR3::pfnVMReset */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMReset(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnVMSuspend */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMSuspend(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnVMPowerOff */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_VMPowerOff(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnLockVM */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_LockVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnUnlockVM */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_UnlockVM(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnAssertVMLock */
static DECLCALLBACK(bool) pdmR3DevHlp_Untrusted_AssertVMLock(PPDMDEVINS pDevIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return false;
}


/** @copydoc PDMDEVHLPR3::pfnDMARegister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMARegister(PPDMDEVINS pDevIns, unsigned uChannel, PFNDMATRANSFERHANDLER pfnTransferHandler, void *pvUser)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnDMAReadMemory */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAReadMemory(PPDMDEVINS pDevIns, unsigned uChannel, void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbRead)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    if (pcbRead)
        *pcbRead = 0;
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnDMAWriteMemory */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMAWriteMemory(PPDMDEVINS pDevIns, unsigned uChannel, const void *pvBuffer, uint32_t off, uint32_t cbBlock, uint32_t *pcbWritten)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    if (pcbWritten)
        *pcbWritten = 0;
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnDMASetDREQ */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_DMASetDREQ(PPDMDEVINS pDevIns, unsigned uChannel, unsigned uLevel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnDMAGetChannelMode */
static DECLCALLBACK(uint8_t) pdmR3DevHlp_Untrusted_DMAGetChannelMode(PPDMDEVINS pDevIns, unsigned uChannel)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return 3 << 2 /* illegal mode type */;
}


/** @copydoc PDMDEVHLPR3::pfnDMASchedule */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_DMASchedule(PPDMDEVINS pDevIns)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnCMOSWrite */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSWrite(PPDMDEVINS pDevIns, unsigned iReg, uint8_t u8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnCMOSRead */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_CMOSRead(PPDMDEVINS pDevIns, unsigned iReg, uint8_t *pu8Value)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnGetCpuId */
static DECLCALLBACK(void) pdmR3DevHlp_Untrusted_GetCpuId(PPDMDEVINS pDevIns, uint32_t iLeaf,
                                                         uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
}


/** @copydoc PDMDEVHLPR3::pfnROMProtectShadow */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_ROMProtectShadow(PPDMDEVINS pDevIns, RTGCPHYS GCPhysStart, RTUINT cbRange, PGMROMPROT enmProt)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMIO2Register */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMIO2Register(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS cb, uint32_t fFlags, void **ppv, const char *pszDesc)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMIO2Deregister */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMIO2Deregister(PPDMDEVINS pDevIns, uint32_t iRegion)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMIO2Map */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMIO2Map(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMIO2Unmap */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMIO2Unmap(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMHyperMapMMIO2 */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMHyperMapMMIO2(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb, const char *pszDesc, PRTRCPTR pRCPtr)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnMMIO2MapKernel */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_MMIO2MapKernel(PPDMDEVINS pDevIns, uint32_t iRegion, RTGCPHYS off, RTGCPHYS cb, const char *pszDesc, PRTR0PTR pR0Ptr)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnRegisterVMMDevHeap */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_RegisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys, RTR3PTR pvHeap, unsigned cbSize)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/** @copydoc PDMDEVHLPR3::pfnUnregisterVMMDevHeap */
static DECLCALLBACK(int) pdmR3DevHlp_Untrusted_UnregisterVMMDevHeap(PPDMDEVINS pDevIns, RTGCPHYS GCPhys)
{
    PDMDEV_ASSERT_DEVINS(pDevIns);
    AssertReleaseMsgFailed(("Untrusted device called trusted helper! '%s'/%d\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
    return VERR_ACCESS_DENIED;
}


/**
 * The device helper structure for non-trusted devices.
 */
const PDMDEVHLPR3 g_pdmR3DevHlpUnTrusted =
{
    PDM_DEVHLP_VERSION,
    pdmR3DevHlp_IOPortRegister,
    pdmR3DevHlp_IOPortRegisterGC,
    pdmR3DevHlp_IOPortRegisterR0,
    pdmR3DevHlp_IOPortDeregister,
    pdmR3DevHlp_MMIORegister,
    pdmR3DevHlp_MMIORegisterGC,
    pdmR3DevHlp_MMIORegisterR0,
    pdmR3DevHlp_MMIODeregister,
    pdmR3DevHlp_ROMRegister,
    pdmR3DevHlp_SSMRegister,
    pdmR3DevHlp_TMTimerCreate,
    pdmR3DevHlp_TMTimerCreateExternal,
    pdmR3DevHlp_PCIRegister,
    pdmR3DevHlp_PCIIORegionRegister,
    pdmR3DevHlp_PCISetConfigCallbacks,
    pdmR3DevHlp_PCISetIrq,
    pdmR3DevHlp_PCISetIrqNoWait,
    pdmR3DevHlp_ISASetIrq,
    pdmR3DevHlp_ISASetIrqNoWait,
    pdmR3DevHlp_DriverAttach,
    pdmR3DevHlp_MMHeapAlloc,
    pdmR3DevHlp_MMHeapAllocZ,
    pdmR3DevHlp_MMHeapFree,
    pdmR3DevHlp_VMSetError,
    pdmR3DevHlp_VMSetErrorV,
    pdmR3DevHlp_VMSetRuntimeError,
    pdmR3DevHlp_VMSetRuntimeErrorV,
    pdmR3DevHlp_AssertEMT,
    pdmR3DevHlp_AssertOther,
    pdmR3DevHlp_DBGFStopV,
    pdmR3DevHlp_DBGFInfoRegister,
    pdmR3DevHlp_STAMRegister,
    pdmR3DevHlp_STAMRegisterF,
    pdmR3DevHlp_STAMRegisterV,
    pdmR3DevHlp_RTCRegister,
    pdmR3DevHlp_PDMQueueCreate,
    pdmR3DevHlp_CritSectInit,
    pdmR3DevHlp_UTCNow,
    pdmR3DevHlp_PDMThreadCreate,
    pdmR3DevHlp_PhysGCPtr2GCPhys,
    pdmR3DevHlp_VMState,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    pdmR3DevHlp_Untrusted_GetVM,
    pdmR3DevHlp_Untrusted_PCIBusRegister,
    pdmR3DevHlp_Untrusted_PICRegister,
    pdmR3DevHlp_Untrusted_APICRegister,
    pdmR3DevHlp_Untrusted_IOAPICRegister,
    pdmR3DevHlp_Untrusted_DMACRegister,
    pdmR3DevHlp_Untrusted_PhysRead,
    pdmR3DevHlp_Untrusted_PhysWrite,
    pdmR3DevHlp_Untrusted_PhysGCPhys2CCPtr,
    pdmR3DevHlp_Untrusted_PhysGCPhys2CCPtrReadOnly,
    pdmR3DevHlp_Untrusted_PhysReleasePageMappingLock,
    pdmR3DevHlp_Untrusted_PhysReadGCVirt,
    pdmR3DevHlp_Untrusted_PhysWriteGCVirt,
    pdmR3DevHlp_Untrusted_A20IsEnabled,
    pdmR3DevHlp_Untrusted_A20Set,
    pdmR3DevHlp_Untrusted_VMReset,
    pdmR3DevHlp_Untrusted_VMSuspend,
    pdmR3DevHlp_Untrusted_VMPowerOff,
    pdmR3DevHlp_Untrusted_LockVM,
    pdmR3DevHlp_Untrusted_UnlockVM,
    pdmR3DevHlp_Untrusted_AssertVMLock,
    pdmR3DevHlp_Untrusted_DMARegister,
    pdmR3DevHlp_Untrusted_DMAReadMemory,
    pdmR3DevHlp_Untrusted_DMAWriteMemory,
    pdmR3DevHlp_Untrusted_DMASetDREQ,
    pdmR3DevHlp_Untrusted_DMAGetChannelMode,
    pdmR3DevHlp_Untrusted_DMASchedule,
    pdmR3DevHlp_Untrusted_CMOSWrite,
    pdmR3DevHlp_Untrusted_CMOSRead,
    pdmR3DevHlp_Untrusted_GetCpuId,
    pdmR3DevHlp_Untrusted_ROMProtectShadow,
    pdmR3DevHlp_Untrusted_MMIO2Register,
    pdmR3DevHlp_Untrusted_MMIO2Deregister,
    pdmR3DevHlp_Untrusted_MMIO2Map,
    pdmR3DevHlp_Untrusted_MMIO2Unmap,
    pdmR3DevHlp_Untrusted_MMHyperMapMMIO2,
    pdmR3DevHlp_Untrusted_MMIO2MapKernel,
    pdmR3DevHlp_Untrusted_RegisterVMMDevHeap,
    pdmR3DevHlp_Untrusted_UnregisterVMMDevHeap,
    PDM_DEVHLP_VERSION /* the end */
};



/**
 * Queue consumer callback for internal component.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pVM         The VM handle.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
DECLCALLBACK(bool) pdmR3DevHlpQueueConsumer(PVM pVM, PPDMQUEUEITEMCORE pItem)
{
    PPDMDEVHLPTASK pTask = (PPDMDEVHLPTASK)pItem;
    LogFlow(("pdmR3DevHlpQueueConsumer: enmOp=%d pDevIns=%p\n", pTask->enmOp, pTask->pDevInsR3));
    switch (pTask->enmOp)
    {
        case PDMDEVHLPTASKOP_ISA_SET_IRQ:
            PDMIsaSetIrq(pVM, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        case PDMDEVHLPTASKOP_PCI_SET_IRQ:
            pdmR3DevHlp_PCISetIrq(pTask->pDevInsR3, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        case PDMDEVHLPTASKOP_IOAPIC_SET_IRQ:
            PDMIoApicSetIrq(pVM, pTask->u.SetIRQ.iIrq, pTask->u.SetIRQ.iLevel);
            break;

        default:
            AssertReleaseMsgFailed(("Invalid operation %d\n", pTask->enmOp));
            break;
    }
    return true;
}

/** @} */

