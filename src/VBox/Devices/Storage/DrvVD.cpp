/** $Id$ */
/** @file
 *
 * VBox storage devices:
 * Media implementation for VBox disk container
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
*   Header files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_VD
#include <VBox/VBoxHDD-new.h>
#include <VBox/pdmdrv.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include "Builtins.h"


/*******************************************************************************
*   Defined types, constants and macros                                        *
*******************************************************************************/

/** Converts a pointer to VDIDISK::IMedia to a PVBOXDISK. */
#define PDMIMEDIA_2_VBOXDISK(pInterface) \
    ( (PVBOXDISK)((uintptr_t)pInterface - RT_OFFSETOF(VBOXDISK, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface) \
    ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVBOXDISK. */
#define PDMIBASE_2_VBOXDISK(pInterface) \
    ( PDMINS2DATA(PDMIBASE_2_DRVINS(pInterface), PVBOXDISK) )


/**
 * VBox disk container media main structure, private part.
 */
typedef struct VBOXDISK
{
    /** The VBox disk container. */
    PVBOXHDD        pDisk;
    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
    /** Name of the image format backend. */
    char            szFormat[16];
    /** Flag whether suspend has changed image open mode to read only. */
    bool            fTempReadOnly;
} VBOXDISK, *PVBOXDISK;

/*******************************************************************************
*   Error reporting callback                                                   *
*******************************************************************************/

static void vdErrorCallback(void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va)
{
    PPDMDRVINS pDrvIns = (PPDMDRVINS)pvUser;
    pDrvIns->pDrvHlp->pfnVMSetErrorV(pDrvIns, rc, RT_SRC_POS_ARGS, pszFormat, va);
}

/*******************************************************************************
*   Media interface methods                                                    *
*******************************************************************************/

/** @copydoc PDMIMEDIA::pfnRead */
static DECLCALLBACK(int) vdRead(PPDMIMEDIA pInterface,
                                uint64_t off, void *pvBuf, size_t cbRead)
{
    LogFlow(("%s: off=%#llx pvBuf=%p cbRead=%d\n", __FUNCTION__,
             off, pvBuf, cbRead));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDRead(pData->pDisk, off, pvBuf, cbRead);
    if (VBOX_SUCCESS(rc))
        Log2(("%s: off=%#llx pvBuf=%p cbRead=%d %.*Vhxd\n", __FUNCTION__,
              off, pvBuf, cbRead, cbRead, pvBuf));
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) vdWrite(PPDMIMEDIA pInterface,
                                 uint64_t off, const void *pvBuf,
                                 size_t cbWrite)
{
    LogFlow(("%s: off=%#llx pvBuf=%p cbWrite=%d\n", __FUNCTION__,
             off, pvBuf, cbWrite));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    Log2(("%s: off=%#llx pvBuf=%p cbWrite=%d %.*Vhxd\n", __FUNCTION__,
          off, pvBuf, cbWrite, cbWrite, pvBuf));
    int rc = VDWrite(pData->pDisk, off, pvBuf, cbWrite);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) vdFlush(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDFlush(pData->pDisk);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) vdGetSize(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    uint64_t cb = VDGetSize(pData->pDisk);
    LogFlow(("%s: returns %#llx (%llu)\n", __FUNCTION__, cb, cb));
    return cb;
}

/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) vdIsReadOnly(PPDMIMEDIA pInterface)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    bool f = VDIsReadOnly(pData->pDisk);
    LogFlow(("%s: returns %d\n", __FUNCTION__, f));
    return f;
}

/** @copydoc PDMIMEDIA::pfnBiosGetGeometry */
static DECLCALLBACK(int) vdBiosGetGeometry(PPDMIMEDIA pInterface,
                                           uint32_t *pcCylinders,
                                           uint32_t *pcHeads,
                                           uint32_t *pcSectors)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetGeometry(pData->pDisk, pcCylinders, pcHeads, pcSectors);
    if (VBOX_FAILURE(rc))
    {
        Log(("%s: geometry not available.\n", __FUNCTION__));
        rc = VERR_PDM_GEOMETRY_NOT_SET;
    }
    LogFlow(("%s: returns %Vrc (CHS=%d/%d/%d)\n", __FUNCTION__,
             rc, *pcCylinders, *pcHeads, *pcSectors));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosSetGeometry */
static DECLCALLBACK(int) vdBiosSetGeometry(PPDMIMEDIA pInterface,
                                           uint32_t cCylinders,
                                           uint32_t cHeads,
                                           uint32_t cSectors)
{
    LogFlow(("%s: CHS=%d/%d/%d\n", __FUNCTION__,
             cCylinders, cHeads, cSectors));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDSetGeometry(pData->pDisk, cCylinders, cHeads, cSectors);
    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosGetTranslation */
static DECLCALLBACK(int) vdBiosGetTranslation(PPDMIMEDIA pInterface,
                                              PPDMBIOSTRANSLATION penmTranslation)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetTranslation(pData->pDisk, penmTranslation);
    LogFlow(("%s: returns %Vrc (%d)\n", __FUNCTION__, rc, *penmTranslation));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnBiosSetTranslation */
static DECLCALLBACK(int) vdBiosSetTranslation(PPDMIMEDIA pInterface,
                                              PDMBIOSTRANSLATION enmTranslation)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDSetTranslation(pData->pDisk, enmTranslation);
    LogFlow(("%s: returns %Vrc (%d)\n", __FUNCTION__, rc, enmTranslation));
    return rc;
}

/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) vdGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMIMEDIA_2_VBOXDISK(pInterface);
    int rc = VDGetUuid(pData->pDisk, 0, pUuid);
    LogFlow(("%s: returns %Vrc ({%Vuuid})\n", __FUNCTION__, rc, pUuid));
    return rc;
}


/*******************************************************************************
*   Base interface methods                                                     *
*******************************************************************************/

/** @copydoc PDMIBASE::pfnQueryInterface */
static DECLCALLBACK(void *) vdQueryInterface(PPDMIBASE pInterface,
                                             PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_MEDIA:
            return &pData->IMedia;
        default:
            return NULL;
    }
}


/*******************************************************************************
*   Driver methods                                                             *
*******************************************************************************/


/**
 * Construct a VBox disk media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle as it's expected
 *                      to be used frequently in this function.
 */
static DECLCALLBACK(int) vdConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    int rc = VINF_SUCCESS;
    char *pszName;          /**< The path of the disk image file. */
    bool fReadOnly;         /**< True if the media is readonly. */
    bool fHonorZeroWrites;  /**< True if zero blocks should be written. */

    /*
     * Init the static parts.
     */
    pDrvIns->IBase.pfnQueryInterface    = vdQueryInterface;
    pData->pDrvIns = pDrvIns;
    pData->fTempReadOnly = false;

    /* IMedia */
    pData->IMedia.pfnRead               = vdRead;
    pData->IMedia.pfnWrite              = vdWrite;
    pData->IMedia.pfnFlush              = vdFlush;
    pData->IMedia.pfnGetSize            = vdGetSize;
    pData->IMedia.pfnIsReadOnly         = vdIsReadOnly;
    pData->IMedia.pfnBiosGetGeometry    = vdBiosGetGeometry;
    pData->IMedia.pfnBiosSetGeometry    = vdBiosSetGeometry;
    pData->IMedia.pfnBiosGetTranslation = vdBiosGetTranslation;
    pData->IMedia.pfnBiosSetTranslation = vdBiosSetTranslation;
    pData->IMedia.pfnGetUuid            = vdGetUuid;

    /*
     * Validate configuration and find all parent images.
     * It's sort of up side down from the image dependency tree.
     */
    unsigned    iLevel = 0;
    PCFGMNODE   pCurNode = pCfgHandle;
    for (;;)
    {
        bool fValid;

        if (pCurNode == pCfgHandle)
        {
            /* Toplevel configuration contains the format backend name and
             * full image open information. */
            fValid = CFGMR3AreValuesValid(pCurNode,
                                          "Format\0"
                                          "Path\0ReadOnly\0HonorZeroWrites\0");
        }
        else
        {
            /* All other image configurations only contain image name. */
            fValid = CFGMR3AreValuesValid(pCurNode, "Path\0");
        }
        if (!fValid)
        {
            rc = PDMDrvHlpVMSetError(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES,
                                     RT_SRC_POS, N_("DrvVD: Configuration error: keys incorrect at level %d"), iLevel);
            break;
        }

        PCFGMNODE pParent = CFGMR3GetChild(pCurNode, "Parent");
        if (!pParent)
            break;
        pCurNode = pParent;
        iLevel++;
    }

    /*
     * Open the images.
     */
    if (VBOX_SUCCESS(rc))
    {
        rc = CFGMR3QueryString(pCfgHandle, "Format", &pData->szFormat[0],
                               sizeof(pData->szFormat));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            /* Default disk image format is VMDK. */
            rc = VINF_SUCCESS;
            strncpy(&pData->szFormat[0], "VMDK", sizeof(pData->szFormat));
            pData->szFormat[sizeof(pData->szFormat) - 1] = '\0';
        }
        if (VBOX_SUCCESS(rc))
        {
            rc = VDCreate(pData->szFormat, vdErrorCallback, pDrvIns, &pData->pDisk);
            /* Error message is already set correctly. */
        }
        else
            rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                  N_("DrvVD: Configuration error: Querying \"Format\" as string failed"));
    }

    while (pCurNode && VBOX_SUCCESS(rc))
    {
        /*
         * Read the image configuration.
         */
        rc = CFGMR3QueryStringAlloc(pCurNode, "Path", &pszName);
        if (VBOX_FAILURE(rc))
        {
            VDDestroy(pData->pDisk);
            rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                  N_("DrvVD: Configuration error: Querying \"Path\" as string failed"));
            break;
        }

        if (iLevel == 0)
        {
            rc = CFGMR3QueryBool(pCurNode, "ReadOnly", &fReadOnly);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fReadOnly = false;
            else if (VBOX_FAILURE(rc))
            {
                MMR3HeapFree(pszName);
                VDDestroy(pData->pDisk);
                rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                      N_("DrvVD: Configuration error: Querying \"ReadOnly\" as boolean failed"));
                break;
            }

            rc = CFGMR3QueryBool(pCfgHandle, "HonorZeroWrites", &fHonorZeroWrites);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fHonorZeroWrites = false;
            else if (VBOX_FAILURE(rc))
            {
                MMR3HeapFree(pszName);
                VDDestroy(pData->pDisk);
                rc = PDMDRV_SET_ERROR(pDrvIns, rc,
                                      N_("DrvVD: Configuration error: Querying \"HonorZeroWrites\" as boolean failed"));
                break;
            }
        }
        else
        {
            fReadOnly = true;
            fHonorZeroWrites = false;
        }

        /*
         * Open the image.
         */
        unsigned uOpenFlags;
        if (fReadOnly)
            uOpenFlags = VD_OPEN_FLAGS_READONLY;
        else
            uOpenFlags = VD_OPEN_FLAGS_NORMAL;
        if (fHonorZeroWrites)
            uOpenFlags |= VD_OPEN_FLAGS_HONOR_ZEROES;
        rc = VDOpen(pData->pDisk, pszName, uOpenFlags);
        if (VBOX_SUCCESS(rc))
            Log(("%s: %d - Opened '%s' in %s mode\n", __FUNCTION__,
                 iLevel, pszName,
                 VDIsReadOnly(pData->pDisk) ? "read-only" : "read-write"));
        else
        {
            AssertMsgFailed(("Failed to open image '%s' rc=%Vrc\n", pszName, rc));
            VDDestroy(pData->pDisk);
            break;
        }
        MMR3HeapFree(pszName);

        /* next */
        iLevel--;
        pCurNode = CFGMR3GetParent(pCurNode);
    }

    LogFlow(("%s: returns %Vrc\n", __FUNCTION__, rc));
    return rc;
}

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdDestruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    int rc = VDCloseAll(pData->pDisk);
    AssertRC(rc);
}


/**
 * When the VM has been suspended we'll change the image mode to read-only
 * so that main and others can read the VDIs. This is important when
 * saving state and so forth.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdSuspend(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    if (!VDIsReadOnly(pData->pDisk))
    {
        unsigned uOpenFlags;
        int rc = VDGetOpenFlags(pData->pDisk, &uOpenFlags);
        AssertRC(rc);
        uOpenFlags |= VD_OPEN_FLAGS_READONLY;
        rc = VDSetOpenFlags(pData->pDisk, uOpenFlags);
        AssertRC(rc);
        pData->fTempReadOnly = true;
    }
}

/**
 * Before the VM resumes we'll have to undo the read-only mode change
 * done in vdSuspend.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vdResume(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s:\n", __FUNCTION__));
    PVBOXDISK pData = PDMINS2DATA(pDrvIns, PVBOXDISK);
    if (pData->fTempReadOnly)
    {
        unsigned uOpenFlags;
        int rc = VDGetOpenFlags(pData->pDisk, &uOpenFlags);
        AssertRC(rc);
        uOpenFlags &= ~VD_OPEN_FLAGS_READONLY;
        rc = VDSetOpenFlags(pData->pDisk, uOpenFlags);
        AssertRC(rc);
        pData->fTempReadOnly = false;
    }
}



/**
 * VBox disk container media driver registration record.
 */
const PDMDRVREG g_DrvVD =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "VD",
    /* pszDescription */
    "Generic VBox disk media driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(VBOXDISK),
    /* pfnConstruct */
    vdConstruct,
    /* pfnDestruct */
    vdDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    vdSuspend,
    /* pfnResume */
    vdResume,
    /* pfnDetach */
    NULL
};
