/* $Id$ */
/** @file
 * PDM - Pluggable Device and Driver Manager, Driver parts.
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
#define LOG_GROUP LOG_GROUP_PDM_DRIVER
#include "PDMInternal.h"
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/cfgm.h>
#include <VBox/vmm.h>
#include <VBox/sup.h>
#include <VBox/vm.h>
#include <VBox/version.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include <iprt/alloc.h>
#include <iprt/path.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Internal callback structure pointer.
 *
 * The main purpose is to define the extra data we associate
 * with PDMDRVREGCB so we can find the VM instance and so on.
 */
typedef struct PDMDRVREGCBINT
{
    /** The callback structure. */
    PDMDRVREGCB     Core;
    /** A bit of padding. */
    uint32_t        u32[4];
    /** VM Handle. */
    PVM             pVM;
} PDMDRVREGCBINT, *PPDMDRVREGCBINT;
typedef const PDMDRVREGCBINT *PCPDMDRVREGCBINT;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/** @def PDMDRV_ASSERT_DRVINS
 * Asserts the validity of the driver instance.
 */
#ifdef VBOX_STRICT
# define PDMDRV_ASSERT_DRVINS(pDrvIns) \
    do { \
        AssertPtr(pDrvIns); \
        Assert(pDrvIns->u32Version == PDM_DRVINS_VERSION); \
        Assert(pDrvIns->pvInstanceData == (void *)&pDrvIns->achInstanceData[0]); \
    } while (0)
#else
# define PDMDRV_ASSERT_DRVINS(pDrvIns)   do { } while (0)
#endif
/** @} */

static DECLCALLBACK(int) pdmR3DrvRegister(PCPDMDRVREGCB pCallbacks, PCPDMDRVREG pDrvReg);
static int pdmR3DrvLoad(PVM pVM, PPDMDRVREGCBINT pRegCB, const char *pszFilename, const char *pszName);



/**
 * Register external drivers
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pfnCallback Driver registration callback
 */
VMMR3DECL(int) PDMR3RegisterDrivers(PVM pVM, FNPDMVBOXDRIVERSREGISTER pfnCallback)
{
    /*
     * The registration callbacks.
     */
    PDMDRVREGCBINT  RegCB;
    RegCB.Core.u32Version   = PDM_DRVREG_CB_VERSION;
    RegCB.Core.pfnRegister  = pdmR3DrvRegister;
    RegCB.pVM               = pVM;

    int rc = pfnCallback(&RegCB.Core, VBOX_VERSION);
    if (RT_FAILURE(rc))
        AssertMsgFailed(("VBoxDriversRegister failed with rc=%Rrc\n"));

    return rc;
}

/**
 * This function will initialize the drivers for this VM instance.
 *
 * First of all this mean loading the builtin drivers and letting them
 * register themselves. Beyond that any additional driver modules are
 * loaded and called for registration.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 */
int pdmR3DrvInit(PVM pVM)
{
    LogFlow(("pdmR3DrvInit:\n"));

    AssertRelease(!(RT_OFFSETOF(PDMDRVINS, achInstanceData) & 15));
    PPDMDRVINS pDrvInsAssert;
    AssertRelease(sizeof(pDrvInsAssert->Internal.s) <= sizeof(pDrvInsAssert->Internal.padding));

    /*
     * The registration callbacks.
     */
    PDMDRVREGCBINT  RegCB;
    RegCB.Core.u32Version   = PDM_DRVREG_CB_VERSION;
    RegCB.Core.pfnRegister  = pdmR3DrvRegister;
    RegCB.pVM               = pVM;

    /*
     * Load the builtin module
     */
    PCFGMNODE pDriversNode = CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM/Drivers");
    bool fLoadBuiltin;
    int rc = CFGMR3QueryBool(pDriversNode, "LoadBuiltin", &fLoadBuiltin);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        fLoadBuiltin = true;
    else if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: Querying boolean \"LoadBuiltin\" failed with %Rrc\n", rc));
        return rc;
    }
    if (fLoadBuiltin)
    {
        /* make filename */
        char *pszFilename = pdmR3FileR3("VBoxDD", /*fShared=*/true);
        if (!pszFilename)
            return VERR_NO_TMP_MEMORY;
        rc = pdmR3DrvLoad(pVM, &RegCB, pszFilename, "VBoxDD");
        RTMemTmpFree(pszFilename);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Load additional driver modules.
     */
    for (PCFGMNODE pCur = CFGMR3GetFirstChild(pDriversNode); pCur; pCur = CFGMR3GetNextChild(pCur))
    {
        /*
         * Get the name and path.
         */
        char szName[PDMMOD_NAME_LEN];
        rc = CFGMR3GetName(pCur, &szName[0], sizeof(szName));
        if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
        {
            AssertMsgFailed(("configuration error: The module name is too long, cchName=%zu.\n", CFGMR3GetNameLen(pCur)));
            return VERR_PDM_MODULE_NAME_TOO_LONG;
        }
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("CFGMR3GetName -> %Rrc.\n", rc));
            return rc;
        }

        /* the path is optional, if no path the module name + path is used. */
        char szFilename[RTPATH_MAX];
        rc = CFGMR3QueryString(pCur, "Path", &szFilename[0], sizeof(szFilename));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
            strcpy(szFilename, szName);
        else if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("configuration error: Failure to query the module path, rc=%Rrc.\n", rc));
            return rc;
        }

        /* prepend path? */
        if (!RTPathHavePath(szFilename))
        {
            char *psz = pdmR3FileR3(szFilename);
            if (!psz)
                return VERR_NO_TMP_MEMORY;
            size_t cch = strlen(psz) + 1;
            if (cch > sizeof(szFilename))
            {
                RTMemTmpFree(psz);
                AssertMsgFailed(("Filename too long! cch=%d '%s'\n", cch, psz));
                return VERR_FILENAME_TOO_LONG;
            }
            memcpy(szFilename, psz, cch);
            RTMemTmpFree(psz);
        }

        /*
         * Load the module and register it's drivers.
         */
        rc = pdmR3DrvLoad(pVM, &RegCB, szFilename, szName);
        if (RT_FAILURE(rc))
            return rc;
    }

    LogFlow(("pdmR3DrvInit: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Loads one driver module and call the registration entry point.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pRegCB          The registration callback stuff.
 * @param   pszFilename     Module filename.
 * @param   pszName         Module name.
 */
static int pdmR3DrvLoad(PVM pVM, PPDMDRVREGCBINT pRegCB, const char *pszFilename, const char *pszName)
{
    /*
     * Load it.
     */
    int rc = pdmR3LoadR3U(pVM->pUVM, pszFilename, pszName);
    if (RT_SUCCESS(rc))
    {
        /*
         * Get the registration export and call it.
         */
        FNPDMVBOXDRIVERSREGISTER *pfnVBoxDriversRegister;
        rc = PDMR3LdrGetSymbolR3(pVM, pszName, "VBoxDriversRegister", (void **)&pfnVBoxDriversRegister);
        if (RT_SUCCESS(rc))
        {
            Log(("PDM: Calling VBoxDriversRegister (%p) of %s (%s)\n", pfnVBoxDriversRegister, pszName, pszFilename));
            rc = pfnVBoxDriversRegister(&pRegCB->Core, VBOX_VERSION);
            if (RT_SUCCESS(rc))
                Log(("PDM: Successfully loaded driver module %s (%s).\n", pszName, pszFilename));
            else
                AssertMsgFailed(("VBoxDriversRegister failed with rc=%Rrc\n"));
        }
        else
        {
            AssertMsgFailed(("Failed to locate 'VBoxDriversRegister' in %s (%s) rc=%Rrc\n", pszName, pszFilename, rc));
            if (rc == VERR_SYMBOL_NOT_FOUND)
                rc = VERR_PDM_NO_REGISTRATION_EXPORT;
        }
    }
    else
        AssertMsgFailed(("Failed to load %s (%s) rc=%Rrc!\n", pszName, pszFilename, rc));
    return rc;
}


/** @copydoc PDMDRVREGCB::pfnRegister */
static DECLCALLBACK(int) pdmR3DrvRegister(PCPDMDRVREGCB pCallbacks, PCPDMDRVREG pDrvReg)
{
    /*
     * Validate the registration structure.
     */
    Assert(pDrvReg);
    if (pDrvReg->u32Version != PDM_DRVREG_VERSION)
    {
        AssertMsgFailed(("Unknown struct version %#x!\n", pDrvReg->u32Version));
        return VERR_PDM_UNKNOWN_DRVREG_VERSION;
    }
    if (    !pDrvReg->szDriverName[0]
        ||  strlen(pDrvReg->szDriverName) >= sizeof(pDrvReg->szDriverName))
    {
        AssertMsgFailed(("Invalid name '%s'\n", pDrvReg->szDriverName));
        return VERR_PDM_INVALID_DRIVER_REGISTRATION;
    }
    if ((pDrvReg->fFlags & PDM_DRVREG_FLAGS_HOST_BITS_MASK) != PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT)
    {
        AssertMsgFailed(("Invalid host bits flags! fFlags=%#x (Driver %s)\n", pDrvReg->fFlags, pDrvReg->szDriverName));
        return VERR_PDM_INVALID_DRIVER_HOST_BITS;
    }
    if (pDrvReg->cMaxInstances <= 0)
    {
        AssertMsgFailed(("Max instances %u! (Driver %s)\n", pDrvReg->cMaxInstances, pDrvReg->szDriverName));
        return VERR_PDM_INVALID_DRIVER_REGISTRATION;
    }
    if (pDrvReg->cbInstance > _1M)
    {
        AssertMsgFailed(("Instance size above 1MB, %d bytes! (Driver %s)\n", pDrvReg->cbInstance, pDrvReg->szDriverName));
        return VERR_PDM_INVALID_DRIVER_REGISTRATION;
    }
    if (!pDrvReg->pfnConstruct)
    {
        AssertMsgFailed(("No constructore! (Driver %s)\n", pDrvReg->szDriverName));
        return VERR_PDM_INVALID_DRIVER_REGISTRATION;
    }

    /*
     * Check for duplicate and find FIFO entry at the same time.
     */
    PCPDMDRVREGCBINT pRegCB = (PCPDMDRVREGCBINT)pCallbacks;
    PPDMDRV pDrvPrev = NULL;
    PPDMDRV pDrv = pRegCB->pVM->pdm.s.pDrvs;
    for (; pDrv; pDrvPrev = pDrv, pDrv = pDrv->pNext)
    {
        if (!strcmp(pDrv->pDrvReg->szDriverName, pDrvReg->szDriverName))
        {
            AssertMsgFailed(("Driver '%s' already exists\n", pDrvReg->szDriverName));
            return VERR_PDM_DRIVER_NAME_CLASH;
        }
    }

    /*
     * Allocate new driver structure and insert it into the list.
     */
    pDrv = (PPDMDRV)MMR3HeapAlloc(pRegCB->pVM, MM_TAG_PDM_DRIVER, sizeof(*pDrv));
    if (pDrv)
    {
        pDrv->pNext = NULL;
        pDrv->cInstances = 0;
        pDrv->pDrvReg = pDrvReg;

        if (pDrvPrev)
            pDrvPrev->pNext = pDrv;
        else
            pRegCB->pVM->pdm.s.pDrvs = pDrv;
        Log(("PDM: Registered driver '%s'\n", pDrvReg->szDriverName));
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Lookups a driver structure by name.
 * @internal
 */
PPDMDRV pdmR3DrvLookup(PVM pVM, const char *pszName)
{
    for (PPDMDRV pDrv = pVM->pdm.s.pDrvs; pDrv; pDrv = pDrv->pNext)
        if (!strcmp(pDrv->pDrvReg->szDriverName, pszName))
            return pDrv;
    return NULL;
}


/**
 * Detaches a driver from whatever it's attached to.
 * This will of course lead to the destruction of the driver and all drivers below it in the chain.
 *
 * @returns VINF_SUCCESS
 * @param   pDrvIns     The driver instance to detach.
 */
int pdmR3DrvDetach(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvDetach: pDrvIns=%p '%s'/%d\n", pDrvIns, pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance));
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    /*
     * Check that we're not doing this recursively, that could have unwanted sideeffects!
     */
    if (pDrvIns->Internal.s.fDetaching)
    {
        AssertMsgFailed(("Recursive detach! '%s'/%d\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance));
        return VINF_SUCCESS;
    }

    /*
     * Check that we actually can detach this instance.
     * The requirement is that the driver/device above has a detach method.
     */
    if (pDrvIns->Internal.s.pUp
            ? !pDrvIns->Internal.s.pUp->pDrvReg->pfnDetach
            : !pDrvIns->Internal.s.pLun->pDevIns->pDevReg->pfnDetach)
    {
        AssertMsgFailed(("Cannot detach driver instance because the driver/device above doesn't support it!\n"));
        return VERR_PDM_DRIVER_DETACH_NOT_POSSIBLE;
    }

    /*
     * Join paths with pdmR3DrvDestroyChain.
     */
    pdmR3DrvDestroyChain(pDrvIns);
    return VINF_SUCCESS;
}


/**
 * Destroys a driver chain starting with the specified driver.
 *
 * This is used when unplugging a device at run time.
 *
 * @param   pDrvIns     Pointer to the driver instance to start with.
 */
void pdmR3DrvDestroyChain(PPDMDRVINS pDrvIns)
{
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    /*
     * Detach the bottommost driver until we've detached pDrvIns.
     */
    pDrvIns->Internal.s.fDetaching = true;
    PPDMDRVINS pCur;
    do
    {
        /* find the driver to detach. */
        pCur = pDrvIns;
        while (pCur->Internal.s.pDown)
            pCur = pCur->Internal.s.pDown;
        LogFlow(("pdmR3DrvDetach: pCur=%p '%s'/%d\n", pCur, pCur->pDrvReg->szDriverName, pCur->iInstance));

        /*
         * Unlink it and notify parent.
         */
        pCur->Internal.s.fDetaching = true;

        PPDMLUN pLun = pCur->Internal.s.pLun;
        Assert(pLun->pBottom == pCur);
        pLun->pBottom = pCur->Internal.s.pUp;

        if (pCur->Internal.s.pUp)
        {
            /* driver parent */
            PPDMDRVINS pParent = pCur->Internal.s.pUp;
            pCur->Internal.s.pUp = NULL;
            pParent->Internal.s.pDown = NULL;

            if (pParent->pDrvReg->pfnDetach)
                pParent->pDrvReg->pfnDetach(pParent);

            pParent->pDownBase = NULL;
        }
        else
        {
            /* device parent */
            Assert(pLun->pTop == pCur);
            pLun->pTop = NULL;
            if (pLun->pDevIns->pDevReg->pfnDetach)
                pLun->pDevIns->pDevReg->pfnDetach(pLun->pDevIns, pLun->iLun);
        }

        /*
         * Call destructor.
         */
        pCur->pUpBase = NULL;
        if (pCur->pDrvReg->pfnDestruct)
            pCur->pDrvReg->pfnDestruct(pCur);

        /*
         * Free all resources allocated by the driver.
         */
        /* Queues. */
        int rc = PDMR3QueueDestroyDriver(pCur->Internal.s.pVM, pCur);
        AssertRC(rc);
        /* Timers. */
        rc = TMR3TimerDestroyDriver(pCur->Internal.s.pVM, pCur);
        AssertRC(rc);
        /* SSM data units. */
        rc = SSMR3DeregisterDriver(pCur->Internal.s.pVM, pCur, NULL, 0);
        AssertRC(rc);
        /* PDM threads. */
        rc = pdmR3ThreadDestroyDriver(pCur->Internal.s.pVM, pCur);
        AssertRC(rc);
        /* Finally, the driver it self. */
        ASMMemFill32(pCur, RT_OFFSETOF(PDMDRVINS, achInstanceData[pCur->pDrvReg->cbInstance]), 0xdeadd0d0);
        MMR3HeapFree(pCur);

    } while (pCur != pDrvIns);
}




/** @name Driver Helpers
 * @{
 */

/** @copydoc PDMDRVHLP::pfnAttach */
static DECLCALLBACK(int) pdmR3DrvHlp_Attach(PPDMDRVINS pDrvIns, PPDMIBASE *ppBaseInterface)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_Attach: caller='%s'/%d:\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance));

    /*
     * Check that there isn't anything attached already.
     */
    int rc;
    if (!pDrvIns->Internal.s.pDown)
    {
        Assert(pDrvIns->Internal.s.pLun->pBottom == pDrvIns);

        /*
         * Get the attached driver configuration.
         */
        PCFGMNODE pNode = CFGMR3GetChild(pDrvIns->Internal.s.pCfgHandle, "AttachedDriver");
        if (pNode)
        {
            char *pszName;
            rc = CFGMR3QueryStringAlloc(pNode, "Driver", &pszName);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Find the driver and allocate instance data.
                 */
                PVM pVM = pDrvIns->Internal.s.pVM;
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

                        size_t cb = RT_OFFSETOF(PDMDRVINS, achInstanceData[pDrv->pDrvReg->cbInstance]);
                        cb = RT_ALIGN_Z(cb, 16);
                        PPDMDRVINS pNew = (PPDMDRVINS)MMR3HeapAllocZ(pVM, MM_TAG_PDM_DRIVER, cb);
                        if (pNew)
                        {
                            /*
                             * Initialize the instance structure (declaration order).
                             */
                            pNew->u32Version                = PDM_DRVINS_VERSION;
                            pNew->Internal.s.pUp            = pDrvIns;
                            pNew->Internal.s.pDown          = NULL;
                            pNew->Internal.s.pLun           = pDrvIns->Internal.s.pLun;
                            pNew->Internal.s.pDrv           = pDrv;
                            pNew->Internal.s.pVM            = pVM;
                            pNew->Internal.s.fDetaching     = false;
                            pNew->Internal.s.pCfgHandle     = pNode;
                            pNew->pDrvHlp                   = &g_pdmR3DrvHlp;
                            pNew->pDrvReg                   = pDrv->pDrvReg;
                            pNew->pCfgHandle                = pConfigNode;
                            pNew->iInstance                 = pDrv->cInstances++;
                            pNew->pUpBase                   = &pDrvIns->IBase; /* This ain't safe, you can calc the pDrvIns of the up/down driver! */
                            pNew->pDownBase                 = NULL;
                            pNew->IBase.pfnQueryInterface   = NULL;
                            pNew->pvInstanceData            = &pNew->achInstanceData[0];

                            /*
                             * Hook it onto the chain and call the constructor.
                             */
                            pDrvIns->Internal.s.pDown = pNew;
                            pDrvIns->Internal.s.pLun->pBottom = pNew;

                            Log(("PDM: Constructing driver '%s' instance %d...\n", pNew->pDrvReg->szDriverName, pNew->iInstance));
                            rc = pDrv->pDrvReg->pfnConstruct(pNew, pNew->pCfgHandle);
                            if (RT_SUCCESS(rc))
                            {
                                *ppBaseInterface = &pNew->IBase;
                                rc = VINF_SUCCESS;
                            }
                            else
                            {
                                /*
                                 * Unlink and free the data.
                                 */
                                Assert(pDrvIns->Internal.s.pLun->pBottom == pNew);
                                pDrvIns->Internal.s.pLun->pBottom = pDrvIns;
                                pDrvIns->Internal.s.pDown = NULL;
                                ASMMemFill32(pNew, cb, 0xdeadd0d0);
                                MMR3HeapFree(pNew);
                                pDrv->cInstances--;
                            }
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
    }
    else
    {
        AssertMsgFailed(("Already got a driver attached. The driver should keep track of such things!\n"));
        rc = VERR_PDM_DRIVER_ALREADY_ATTACHED;
    }

    LogFlow(("pdmR3DrvHlp_Attach: caller='%s'/%d: return %Rrc\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnDetach */
static DECLCALLBACK(int) pdmR3DrvHlp_Detach(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_Detach: caller='%s'/%d:\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance));
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    /*
     * Anything attached?
     */
    int rc;
    if (pDrvIns->Internal.s.pDown)
    {
        rc = pdmR3DrvDetach(pDrvIns->Internal.s.pDown);
    }
    else
    {
        AssertMsgFailed(("Nothing attached!\n"));
        rc = VERR_PDM_NO_DRIVER_ATTACHED;
    }

    LogFlow(("pdmR3DrvHlp_Detach: caller='%s'/%d: returns %Rrc\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnDetachSelf */
static DECLCALLBACK(int) pdmR3DrvHlp_DetachSelf(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_DetachSelf: caller='%s'/%d:\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance));
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    int rc = pdmR3DrvDetach(pDrvIns);

    LogFlow(("pdmR3DrvHlp_Detach: returns %Rrc\n", rc)); /* pDrvIns is freed by now. */
    return rc;
}


/** @copydoc PDMDRVHLP::pfnMountPrepare */
static DECLCALLBACK(int) pdmR3DrvHlp_MountPrepare(PPDMDRVINS pDrvIns, const char *pszFilename, const char *pszCoreDriver)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_MountPrepare: caller='%s'/%d: pszFilename=%p:{%s} pszCoreDriver=%p:{%s}\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pszFilename, pszFilename, pszCoreDriver, pszCoreDriver));
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    /*
     * Do the caller have anything attached below itself?
     */
    if (pDrvIns->Internal.s.pDown)
    {
        AssertMsgFailed(("Cannot prepare a mount when something's attached to you!\n"));
        return VERR_PDM_DRIVER_ALREADY_ATTACHED;
    }

    /*
     * We're asked to prepare, so we'll start off by nuking the
     * attached configuration tree.
     */
    PCFGMNODE   pNode = CFGMR3GetChild(pDrvIns->Internal.s.pCfgHandle, "AttachedDriver");
    if (pNode)
        CFGMR3RemoveNode(pNode);

    /*
     * If there is no core driver, we'll have to probe for it.
     */
    if (!pszCoreDriver)
    {
        /** @todo implement image probing. */
        AssertReleaseMsgFailed(("Not implemented!\n"));
        return VERR_NOT_IMPLEMENTED;
    }

    /*
     * Construct the basic attached driver configuration.
     */
    int rc = CFGMR3InsertNode(pDrvIns->Internal.s.pCfgHandle, "AttachedDriver", &pNode);
    if (RT_SUCCESS(rc))
    {
        rc = CFGMR3InsertString(pNode, "Driver", pszCoreDriver);
        if (RT_SUCCESS(rc))
        {
            PCFGMNODE pCfg;
            rc = CFGMR3InsertNode(pNode, "Config", &pCfg);
            if (RT_SUCCESS(rc))
            {
                rc = CFGMR3InsertString(pCfg, "Path", pszFilename);
                if (RT_SUCCESS(rc))
                {
                    LogFlow(("pdmR3DrvHlp_MountPrepare: caller='%s'/%d: returns %Rrc (Driver=%s)\n",
                             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc, pszCoreDriver));
                    return rc;
                }
                else
                    AssertMsgFailed(("Path string insert failed, rc=%Rrc\n", rc));
            }
            else
                AssertMsgFailed(("Config node failed, rc=%Rrc\n", rc));
        }
        else
            AssertMsgFailed(("Driver string insert failed, rc=%Rrc\n", rc));
        CFGMR3RemoveNode(pNode);
    }
    else
        AssertMsgFailed(("AttachedDriver node insert failed, rc=%Rrc\n", rc));

    LogFlow(("pdmR3DrvHlp_MountPrepare: caller='%s'/%d: returns %Rrc\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnAssertEMT */
static DECLCALLBACK(bool) pdmR3DrvHlp_AssertEMT(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (VM_IS_EMT(pDrvIns->Internal.s.pVM))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertEMT '%s'/%d\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    return false;
}


/** @copydoc PDMDRVHLP::pfnAssertOther */
static DECLCALLBACK(bool) pdmR3DrvHlp_AssertOther(PPDMDRVINS pDrvIns, const char *pszFile, unsigned iLine, const char *pszFunction)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    if (!VM_IS_EMT(pDrvIns->Internal.s.pVM))
        return true;

    char szMsg[100];
    RTStrPrintf(szMsg, sizeof(szMsg), "AssertOther '%s'/%d\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance);
    AssertMsg1(szMsg, iLine, pszFile, pszFunction);
    AssertBreakpoint();
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    return false;
}


/** @copydoc PDMDRVHLP::pfnVMSetError */
static DECLCALLBACK(int) pdmR3DrvHlp_VMSetError(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list args;
    va_start(args, pszFormat);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVM, rc, RT_SRC_POS_ARGS, pszFormat, args); Assert(rc2 == rc); NOREF(rc2);
    va_end(args);
    return rc;
}


/** @copydoc PDMDRVHLP::pfnVMSetErrorV */
static DECLCALLBACK(int) pdmR3DrvHlp_VMSetErrorV(PPDMDRVINS pDrvIns, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc2 = VMSetErrorV(pDrvIns->Internal.s.pVM, rc, RT_SRC_POS_ARGS, pszFormat, va); Assert(rc2 == rc); NOREF(rc2);
    return rc;
}


/** @copydoc PDMDRVHLP::pfnVMSetRuntimeError */
static DECLCALLBACK(int) pdmR3DrvHlp_VMSetRuntimeError(PPDMDRVINS pDrvIns, bool fFatal, const char *pszErrorID, const char *pszFormat, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    va_list args;
    va_start(args, pszFormat);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVM, fFatal, pszErrorID, pszFormat, args);
    va_end(args);
    return rc;
}


/** @copydoc PDMDRVHLP::pfnVMSetRuntimeErrorV */
static DECLCALLBACK(int) pdmR3DrvHlp_VMSetRuntimeErrorV(PPDMDRVINS pDrvIns, bool fFatal, const char *pszErrorID, const char *pszFormat, va_list va)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    int rc = VMSetRuntimeErrorV(pDrvIns->Internal.s.pVM, fFatal, pszErrorID, pszFormat, va);
    return rc;
}


/** @copydoc PDMDRVHLP::pfnPDMQueueCreate */
static DECLCALLBACK(int) pdmR3DrvHlp_PDMQueueCreate(PPDMDRVINS pDrvIns, RTUINT cbItem, RTUINT cItems, uint32_t cMilliesInterval, PFNPDMQUEUEDRV pfnCallback, PPDMQUEUE *ppQueue)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_PDMQueueCreate: caller='%s'/%d: cbItem=%d cItems=%d cMilliesInterval=%d pfnCallback=%p ppQueue=%p\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, cbItem, cItems, cMilliesInterval, pfnCallback, ppQueue, ppQueue));
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    int rc = PDMR3QueueCreateDriver(pDrvIns->Internal.s.pVM, pDrvIns, cbItem, cItems, cMilliesInterval, pfnCallback, ppQueue);

    LogFlow(("pdmR3DrvHlp_PDMQueueCreate: caller='%s'/%d: returns %Rrc *ppQueue=%p\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc, *ppQueue));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnPDMPollerRegister */
static DECLCALLBACK(int) pdmR3DrvHlp_PDMPollerRegister(PPDMDRVINS pDrvIns, PFNPDMDRVPOLLER pfnPoller)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    AssertLogRelMsgFailedReturn(("pdmR3DrvHlp_PDMPollerRegister: caller='%s'/%d: pfnPoller=%p -> VERR_NOT_SUPPORTED\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pfnPoller),
                                VERR_NOT_SUPPORTED);
}


/** @copydoc PDMDRVHLP::pfnTMGetVirtualFreq */
static DECLCALLBACK(uint64_t) pdmR3DrvHlp_TMGetVirtualFreq(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);

    return TMVirtualGetFreq(pDrvIns->Internal.s.pVM);
}


/** @copydoc PDMDRVHLP::pfnTMGetVirtualTime */
static DECLCALLBACK(uint64_t) pdmR3DrvHlp_TMGetVirtualTime(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);

    return TMVirtualGet(pDrvIns->Internal.s.pVM);
}


/** @copydoc PDMDRVHLP::pfnTMTimerCreate */
static DECLCALLBACK(int) pdmR3DrvHlp_TMTimerCreate(PPDMDRVINS pDrvIns, TMCLOCK enmClock, PFNTMTIMERDRV pfnCallback, const char *pszDesc, PPTMTIMERR3 ppTimer)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_TMTimerCreate: caller='%s'/%d: enmClock=%d pfnCallback=%p pszDesc=%p:{%s} ppTimer=%p\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, enmClock, pfnCallback, pszDesc, pszDesc, ppTimer));

    int rc = TMR3TimerCreateDriver(pDrvIns->Internal.s.pVM, pDrvIns, enmClock, pfnCallback, pszDesc, ppTimer);

    LogFlow(("pdmR3DrvHlp_TMTimerCreate: caller='%s'/%d: returns %Rrc *ppTimer=%p\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc, *ppTimer));
    return rc;
}



/** @copydoc PDMDRVHLP::pfnSSMRegister */
static DECLCALLBACK(int) pdmR3DrvHlp_SSMRegister(PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance, uint32_t u32Version, size_t cbGuess,
                                                 PFNSSMDRVSAVEPREP pfnSavePrep, PFNSSMDRVSAVEEXEC pfnSaveExec, PFNSSMDRVSAVEDONE pfnSaveDone,
                                                 PFNSSMDRVLOADPREP pfnLoadPrep, PFNSSMDRVLOADEXEC pfnLoadExec, PFNSSMDRVLOADDONE pfnLoadDone)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_SSMRegister: caller='%s'/%d: pszName=%p:{%s} u32Instance=%#x u32Version=#x cbGuess=%#x pfnSavePrep=%p pfnSaveExec=%p pfnSaveDone=%p pszLoadPrep=%p pfnLoadExec=%p pfnLoaddone=%p\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pszName, pszName, u32Instance, u32Version, cbGuess, pfnSavePrep, pfnSaveExec, pfnSaveDone, pfnLoadPrep, pfnLoadExec, pfnLoadDone));

    int rc = SSMR3RegisterDriver(pDrvIns->Internal.s.pVM, pDrvIns, pszName, u32Instance, u32Version, cbGuess,
                                 pfnSavePrep, pfnSaveExec, pfnSaveDone,
                                 pfnLoadPrep, pfnLoadExec, pfnLoadDone);

    LogFlow(("pdmR3DrvHlp_SSMRegister: caller='%s'/%d: returns %Rrc\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnSSMDeregister */
static DECLCALLBACK(int) pdmR3DrvHlp_SSMDeregister(PPDMDRVINS pDrvIns, const char *pszName, uint32_t u32Instance)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_SSMDeregister: caller='%s'/%d: pszName=%p:{%s} u32Instance=%#x\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, pszName, pszName, u32Instance));

    int rc = SSMR3DeregisterDriver(pDrvIns->Internal.s.pVM, pDrvIns, pszName, u32Instance);

    LogFlow(("pdmR3DrvHlp_SSMDeregister: caller='%s'/%d: returns %Rrc\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnSTAMRegister */
static DECLCALLBACK(void) pdmR3DrvHlp_STAMRegister(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, const char *pszName, STAMUNIT enmUnit, const char *pszDesc)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    STAM_REG(pDrvIns->Internal.s.pVM, pvSample, enmType, pszName, enmUnit, pszDesc);
    /** @todo track the samples so they can be dumped & deregistered when the driver instance is destroyed.
     * For now we just have to be careful not to use this call for drivers which can be unloaded. */
}


/** @copydoc PDMDRVHLP::pfnSTAMRegisterF */
static DECLCALLBACK(void) pdmR3DrvHlp_STAMRegisterF(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, ...)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    va_list args;
    va_start(args, pszName);
    int rc = STAMR3RegisterV(pDrvIns->Internal.s.pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    va_end(args);
    AssertRC(rc);
}


/** @copydoc PDMDRVHLP::pfnSTAMRegisterV */
static DECLCALLBACK(void) pdmR3DrvHlp_STAMRegisterV(PPDMDRVINS pDrvIns, void *pvSample, STAMTYPE enmType, STAMVISIBILITY enmVisibility,
                                                    STAMUNIT enmUnit, const char *pszDesc, const char *pszName, va_list args)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);

    int rc = STAMR3RegisterV(pDrvIns->Internal.s.pVM, pvSample, enmType, enmVisibility, enmUnit, pszDesc, pszName, args);
    AssertRC(rc);
}


/** @copydoc PDMDRVHLP::pfnSUPCallVMMR0Ex */
static DECLCALLBACK(int) pdmR3DrvHlp_SUPCallVMMR0Ex(PPDMDRVINS pDrvIns, unsigned uOperation, void *pvArg, unsigned cbArg)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    LogFlow(("pdmR3DrvHlp_SSMCallVMMR0Ex: caller='%s'/%d: uOperation=%u pvArg=%p cbArg=%d\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, uOperation, pvArg, cbArg));
    int rc;
    if (    uOperation >= VMMR0_DO_SRV_START
        &&  uOperation <  VMMR0_DO_SRV_END)
        rc = SUPCallVMMR0Ex(pDrvIns->Internal.s.pVM->pVMR0, uOperation, 0, (PSUPVMMR0REQHDR)pvArg);
    else
    {
        AssertMsgFailed(("Invalid uOperation=%u\n", uOperation));
        rc = VERR_INVALID_PARAMETER;
    }

    LogFlow(("pdmR3DrvHlp_SUPCallVMMR0Ex: caller='%s'/%d: returns %Rrc\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnUSBRegisterHub */
static DECLCALLBACK(int) pdmR3DrvHlp_USBRegisterHub(PPDMDRVINS pDrvIns, uint32_t fVersions, uint32_t cPorts, PCPDMUSBHUBREG pUsbHubReg, PPCPDMUSBHUBHLP ppUsbHubHlp)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_USBRegisterHub: caller='%s'/%d: fVersions=%#x cPorts=%#x pUsbHubReg=%p ppUsbHubHlp=%p\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, fVersions, cPorts, pUsbHubReg, ppUsbHubHlp));

#ifdef VBOX_WITH_USB
    int rc = pdmR3UsbRegisterHub(pDrvIns->Internal.s.pVM, pDrvIns, fVersions, cPorts, pUsbHubReg, ppUsbHubHlp);
#else
    int rc = VERR_NOT_SUPPORTED;
#endif

    LogFlow(("pdmR3DrvHlp_USBRegisterHub: caller='%s'/%d: returns %Rrc\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, rc));
    return rc;
}


/** @copydoc PDMDRVHLP::pfnPDMThreadCreate */
static DECLCALLBACK(int) pdmR3DrvHlp_PDMThreadCreate(PPDMDRVINS pDrvIns, PPPDMTHREAD ppThread, void *pvUser, PFNPDMTHREADDRV pfnThread,
                                                     PFNPDMTHREADWAKEUPDRV pfnWakeup, size_t cbStack, RTTHREADTYPE enmType, const char *pszName)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_PDMThreadCreate: caller='%s'/%d: ppThread=%p pvUser=%p pfnThread=%p pfnWakeup=%p cbStack=%#zx enmType=%d pszName=%p:{%s}\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName, pszName));

    int rc = pdmR3ThreadCreateDriver(pDrvIns->Internal.s.pVM, pDrvIns, ppThread, pvUser, pfnThread, pfnWakeup, cbStack, enmType, pszName);

    LogFlow(("pdmR3DrvHlp_PDMThreadCreate: caller='%s'/%d: returns %Rrc *ppThread=%RTthrd\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance,
            rc, *ppThread));
    return rc;
}


/** @copydoc PDMDEVHLPR3::pfnVMState */
static DECLCALLBACK(VMSTATE) pdmR3DrvHlp_VMState(PPDMDRVINS pDrvIns)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);

    VMSTATE enmVMState = VMR3GetState(pDrvIns->Internal.s.pVM);

    LogFlow(("pdmR3DrvHlp_VMState: caller='%s'/%d: returns %d (%s)\n", pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance,
             enmVMState, VMR3GetStateName(enmVMState)));
    return enmVMState;
}


#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
/** @copydoc PDMDRVHLP::pfnPDMAsyncCompletionTemplateCreate */
static DECLCALLBACK(int) pdmR3DrvHlp_PDMAsyncCompletionTemplateCreate(PPDMDRVINS pDrvIns, PPPDMASYNCCOMPLETIONTEMPLATE ppTemplate,
                                                                      PFNPDMASYNCCOMPLETEDRV pfnCompleted, const char *pszDesc)
{
    PDMDRV_ASSERT_DRVINS(pDrvIns);
    VM_ASSERT_EMT(pDrvIns->Internal.s.pVM);
    LogFlow(("pdmR3DrvHlp_PDMAsyncCompletionTemplateCreate: caller='%s'/%d: ppTemplate=%p pfnCompleted=%p pszDesc=%p:{%s}\n",
             pDrvIns->pDrvReg->szDriverName, pDrvIns->iInstance, ppTemplate, pfnCompleted, pszDesc, pszDesc));

    int rc = PDMR3AsyncCompletionTemplateCreateDriver(pDrvIns->Internal.s.pVM, pDrvIns, ppTemplate, pfnCompleted, pszDesc);

    LogFlow(("pdmR3DrvHlp_PDMAsyncCompletionTemplateCreate: caller='%s'/%d: returns %Rrc *ppThread=%p\n", pDrvIns->pDrvReg->szDriverName,
             pDrvIns->iInstance, rc, *ppTemplate));
    return rc;
}
#endif


/**
 * The driver helper structure.
 */
const PDMDRVHLP g_pdmR3DrvHlp =
{
    PDM_DRVHLP_VERSION,
    pdmR3DrvHlp_Attach,
    pdmR3DrvHlp_Detach,
    pdmR3DrvHlp_DetachSelf,
    pdmR3DrvHlp_MountPrepare,
    pdmR3DrvHlp_AssertEMT,
    pdmR3DrvHlp_AssertOther,
    pdmR3DrvHlp_VMSetError,
    pdmR3DrvHlp_VMSetErrorV,
    pdmR3DrvHlp_VMSetRuntimeError,
    pdmR3DrvHlp_VMSetRuntimeErrorV,
    pdmR3DrvHlp_PDMQueueCreate,
    pdmR3DrvHlp_PDMPollerRegister,
    pdmR3DrvHlp_TMGetVirtualFreq,
    pdmR3DrvHlp_TMGetVirtualTime,
    pdmR3DrvHlp_TMTimerCreate,
    pdmR3DrvHlp_SSMRegister,
    pdmR3DrvHlp_SSMDeregister,
    pdmR3DrvHlp_STAMRegister,
    pdmR3DrvHlp_STAMRegisterF,
    pdmR3DrvHlp_STAMRegisterV,
    pdmR3DrvHlp_SUPCallVMMR0Ex,
    pdmR3DrvHlp_USBRegisterHub,
    pdmR3DrvHlp_PDMThreadCreate,
    pdmR3DrvHlp_VMState,
#ifdef VBOX_WITH_PDM_ASYNC_COMPLETION
    pdmR3DrvHlp_PDMAsyncCompletionTemplateCreate,
#endif
    PDM_DRVHLP_VERSION /* u32TheEnd */
};

/** @} */
