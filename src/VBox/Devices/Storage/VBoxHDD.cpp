/* $Id$ */
/** @file
 * VBoxHDD - VBox HDD container implementation.
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
#define LOG_GROUP LOG_GROUP_DRV_VBOXHDD
#include <VBox/VBoxHDD.h>
#include <VBox/pdmdrv.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include "VDICore.h"
#include "Builtins.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Converts a pointer to VDIDISK::IMedia to a PVDIDISK. */
#define PDMIMEDIA_2_VDIDISK(pInterface) ( (PVDIDISK)((uintptr_t)pInterface - RT_OFFSETOF(VDIDISK, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVDIDISK. */
#define PDMIBASE_2_VDIDISK(pInterface)  ( PDMINS_2_DATA(PDMIBASE_2_DRVINS(pInterface), PVDIDISK) )




/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) vdiGetSize(PPDMIMEDIA pInterface)
{
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    uint64_t cb = VDIDiskGetSize(pThis);
    LogFlow(("vdiGetSize: returns %#llx (%llu)\n", cb, cb));
    return cb;
}


/**
 * Get stored media PCHS geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosGetPCHSGeometry for details.
 */
static DECLCALLBACK(int) vdiBiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlow(("%s: returns VERR_NOT_IMPLEMENTED\n", __FUNCTION__));
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Set stored media PCHS geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosSetPCHSGeometry for details.
 */
static DECLCALLBACK(int) vdiBiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlow(("%s: returns VERR_NOT_IMPLEMENTED\n", __FUNCTION__));
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Get stored media LCHS geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosGetLCHSGeometry for details.
 */
static DECLCALLBACK(int) vdiBiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    int rc = VDIDiskGetLCHSGeometry(pThis, pLCHSGeometry);
    if (RT_SUCCESS(rc))
    {
        LogFlow(("%s: returns VINF_SUCCESS\n", __FUNCTION__));
        return VINF_SUCCESS;
    }
    Log(("%s: The Bios geometry data was not available.\n", __FUNCTION__));
    return VERR_PDM_GEOMETRY_NOT_SET;
}


/**
 * Set stored media LCHS geometry - BIOS property.
 *
 * @see PDMIMEDIA::pfnBiosSetLCHSGeometry for details.
 */
static DECLCALLBACK(int) vdiBiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    int rc = VDIDiskSetLCHSGeometry(pThis, pLCHSGeometry);
    LogFlow(("%s: returns %Rrc (%d,%d,%d)\n", __FUNCTION__, rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}


/**
 * Read bits.
 *
 * @see PDMIMEDIA::pfnRead for details.
 */
static DECLCALLBACK(int) vdiRead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    LogFlow(("vdiRead: off=%#llx pvBuf=%p cbRead=%d\n", off, pvBuf, cbRead));
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    int rc = VDIDiskRead(pThis, off, pvBuf, cbRead);
    if (RT_SUCCESS(rc))
        Log2(("vdiRead: off=%#llx pvBuf=%p cbRead=%d\n"
              "%.*Vhxd\n",
              off, pvBuf, cbRead, cbRead, pvBuf));
    LogFlow(("vdiRead: returns %Rrc\n", rc));
    return rc;
}


/**
 * Write bits.
 *
 * @see PDMIMEDIA::pfnWrite for details.
 */
static DECLCALLBACK(int) vdiWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    LogFlow(("vdiWrite: off=%#llx pvBuf=%p cbWrite=%d\n", off, pvBuf, cbWrite));
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    Log2(("vdiWrite: off=%#llx pvBuf=%p cbWrite=%d\n"
          "%.*Vhxd\n",
          off, pvBuf, cbWrite, cbWrite, pvBuf));
    int rc = VDIDiskWrite(pThis, off, pvBuf, cbWrite);
    LogFlow(("vdiWrite: returns %Rrc\n", rc));
    return rc;
}


/**
 * Flush bits to media.
 *
 * @see PDMIMEDIA::pfnFlush for details.
 */
static DECLCALLBACK(int) vdiFlush(PPDMIMEDIA pInterface)
{
    LogFlow(("vdiFlush:\n"));
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    VDIFlushImage(pThis->pLast);
    int rc = VINF_SUCCESS;
    LogFlow(("vdiFlush: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) vdiGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    int rc = VDIDiskGetImageUuid(pThis, 0, pUuid);
    LogFlow(("vdiGetUuid: returns %Rrc ({%RTuuid})\n", rc, pUuid));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) vdiIsReadOnly(PPDMIMEDIA pInterface)
{
    PVDIDISK pThis = PDMIMEDIA_2_VDIDISK(pInterface);
    LogFlow(("vdiIsReadOnly: returns %d\n", VDIDiskIsReadOnly(pThis)));
    return VDIDiskIsReadOnly(pThis);
}


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) vdiQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PVDIDISK pThis = PDMINS_2_DATA(pDrvIns, PVDIDISK);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MEDIA:
            return &pThis->IMedia;
        default:
            return NULL;
    }
}


/**
 * Before the VM resumes we'll have to undo the read-only mode change
 * done in vdiSuspend.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdiResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("vdiSuspend:\n"));
#if 1 //#ifdef DEBUG_dmik
    PVDIDISK pThis = PDMINS_2_DATA(pDrvIns, PVDIDISK);
    if (!(pThis->pLast->fOpen & VDI_OPEN_FLAGS_READONLY))
    {
        int rc = vdiChangeImageMode(pThis->pLast, false);
        AssertRC(rc);
    }
#endif
}


/**
 * When the VM has been suspended we'll change the image mode to read-only
 * so that main and others can read the VDIs. This is important when
 * saving state and so forth.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdiSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("vdiSuspend:\n"));
#if 1 // #ifdef DEBUG_dmik
    PVDIDISK pThis = PDMINS_2_DATA(pDrvIns, PVDIDISK);
    if (!(pThis->pLast->fOpen & VDI_OPEN_FLAGS_READONLY))
    {
        int rc = vdiChangeImageMode(pThis->pLast, true);
        AssertRC(rc);
    }
#endif
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdiDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("vdiDestruct:\n"));
    PVDIDISK pThis = PDMINS_2_DATA(pDrvIns, PVDIDISK);
    VDIDiskCloseAllImages(pThis);
}


/**
 * Construct a VBox HDD media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) vdiConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    LogFlow(("vdiConstruct:\n"));
    PVDIDISK pThis = PDMINS_2_DATA(pDrvIns, PVDIDISK);
    char *pszName;      /**< The path of the disk image file. */
    bool fReadOnly;     /**< True if the media is readonly. */
    bool fHonorZeroWrites = false;

    /*
     * Init the static parts.
     */
    pDrvIns->IBase.pfnQueryInterface    = vdiQueryInterface;
    pThis->pDrvIns = pDrvIns;

    vdiInitVDIDisk(pThis);

    /* IMedia */
    pThis->IMedia.pfnRead               = vdiRead;
    pThis->IMedia.pfnWrite              = vdiWrite;
    pThis->IMedia.pfnFlush              = vdiFlush;
    pThis->IMedia.pfnGetSize            = vdiGetSize;
    pThis->IMedia.pfnGetUuid            = vdiGetUuid;
    pThis->IMedia.pfnIsReadOnly         = vdiIsReadOnly;
    pThis->IMedia.pfnBiosGetPCHSGeometry = vdiBiosGetPCHSGeometry;
    pThis->IMedia.pfnBiosSetPCHSGeometry = vdiBiosSetPCHSGeometry;
    pThis->IMedia.pfnBiosGetLCHSGeometry = vdiBiosGetLCHSGeometry;
    pThis->IMedia.pfnBiosSetLCHSGeometry = vdiBiosSetLCHSGeometry;

    /*
     * Validate configuration and find the great to the level of umpteen grandparent.
     * The parents are found in the 'Parent' subtree, so it's sorta up side down
     * from the image dependency tree.
     */
    unsigned    iLevel = 0;
    PCFGMNODE   pCurNode = pCfgHandle;
    for (;;)
    {
        if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0ReadOnly\0HonorZeroWrites\0"))
            return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

        PCFGMNODE pParent = CFGMR3GetChild(pCurNode, "Parent");
        if (!pParent)
            break;
        pCurNode = pParent;
        iLevel++;
    }

    /*
     * Open the images.
     */
    int rc = VINF_SUCCESS;
    while (pCurNode && RT_SUCCESS(rc))
    {
        /*
         * Read the image configuration.
         */
        int rc = CFGMR3QueryStringAlloc(pCurNode, "Path", &pszName);
        if (RT_FAILURE(rc))
            return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("VHDD: Configuration error: Querying \"Path\" as string failed"));

        rc = CFGMR3QueryBool(pCurNode, "ReadOnly", &fReadOnly);
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            fReadOnly = false;
        else if (RT_FAILURE(rc))
        {
            MMR3HeapFree(pszName);
            return PDMDRV_SET_ERROR(pDrvIns, rc,
                                    N_("VHDD: Configuration error: Querying \"ReadOnly\" as boolean failed"));
        }

        if (!fHonorZeroWrites)
        {
            rc = CFGMR3QueryBool(pCfgHandle, "HonorZeroWrites", &fHonorZeroWrites);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fHonorZeroWrites = false;
            else if (RT_FAILURE(rc))
            {
                MMR3HeapFree(pszName);
                return PDMDRV_SET_ERROR(pDrvIns, rc,
                                        N_("VHDD: Configuration error: Querying \"HonorZeroWrites\" as boolean failed"));
            }
        }

        /*
         * Open the image.
         */
        rc = VDIDiskOpenImage(pThis, pszName, fReadOnly ? VDI_OPEN_FLAGS_READONLY
                                                        : VDI_OPEN_FLAGS_NORMAL);
        if (RT_SUCCESS(rc))
            Log(("vdiConstruct: %d - Opened '%s' in %s mode\n",
                 iLevel, pszName, VDIDiskIsReadOnly(pThis) ? "read-only" : "read-write"));
        else
            AssertMsgFailed(("Failed to open image '%s' rc=%Rrc\n", pszName, rc));
        MMR3HeapFree(pszName);

        /* next */
        iLevel--;
        pCurNode = CFGMR3GetParent(pCurNode);
    }

    /* If any of the images has the flag set, handle zero writes like normal. */
    if (RT_SUCCESS(rc))
        pThis->fHonorZeroWrites = fHonorZeroWrites;

    /* On failure, vdiDestruct will be called, so no need to clean up here. */

    if (rc == VERR_ACCESS_DENIED)
        /* This should never happen here since this case is covered by Console::PowerUp */
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Cannot open virtual disk image '%s' for %s access"),
                                   pszName, fReadOnly ? "readonly" : "read/write");

    return rc;
}


/**
 * VBox HDD driver registration record.
 */
const PDMDRVREG g_DrvVBoxHDD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "VBoxHDD",
    /* pszDescription */
    "VBoxHDD media driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(VDIDISK),
    /* pfnConstruct */
    vdiConstruct,
    /* pfnDestruct */
    vdiDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    vdiSuspend,
    /* pfnResume */
    vdiResume,
    /* pfnDetach */
    NULL
};

