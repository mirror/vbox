/** @file
 *
 * VBox storage devices:
 * ISO image media driver
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
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_ISO
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>

#include <string.h>

#include "Builtins.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Converts a pointer to MEDIAISO::IMedia to a PRDVMEDIAISO. */
#define PDMIMEDIA_2_DRVMEDIAISO(pInterface) ( (PDRVMEDIAISO)((uintptr_t)pInterface - RT_OFFSETOF(DRVMEDIAISO, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVBOXHDD. */
#define PDMIBASE_2_DRVMEDIAISO(pInterface)  ( PDMINS2DATA(PDMIBASE_2_DRVINS(pInterface), PDRVMEDIAISO) )



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 */
typedef struct DRVMEDIAISO
{
    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
    /** Pointer to the filename. (Freed by MM) */
    char           *pszFilename;
    /** File handle of the ISO file. */
    RTFILE          File;
} DRVMEDIAISO, *PDRVMEDIAISO;



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) drvMediaISORead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead);
static DECLCALLBACK(int) drvMediaISOWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(int) drvMediaISOFlush(PPDMIMEDIA pInterface);
static DECLCALLBACK(bool) drvMediaISOIsReadOnly(PPDMIMEDIA pInterface);
static DECLCALLBACK(uint64_t) drvMediaISOGetSize(PPDMIMEDIA pInterface);
static DECLCALLBACK(int) drvMediaISOGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid);
static DECLCALLBACK(int) drvMediaISOBiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry);
static DECLCALLBACK(int) drvMediaISOBiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry);
static DECLCALLBACK(int) drvMediaISOBiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry);
static DECLCALLBACK(int) drvMediaISOBiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry);

static DECLCALLBACK(void *) drvMediaISOQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);




/**
 * Construct a ISO media driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvMediaISOConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVMEDIAISO pData = PDMINS2DATA(pDrvIns, PDRVMEDIAISO);

    /*
     * Init the static parts.
     */
    pData->pDrvIns                      = pDrvIns;
    pData->File                         = NIL_RTFILE;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvMediaISOQueryInterface;
    /* IMedia */
    pData->IMedia.pfnRead               = drvMediaISORead;
    pData->IMedia.pfnWrite              = drvMediaISOWrite;
    pData->IMedia.pfnFlush              = drvMediaISOFlush;
    pData->IMedia.pfnGetSize            = drvMediaISOGetSize;
    pData->IMedia.pfnGetUuid            = drvMediaISOGetUuid;
    pData->IMedia.pfnIsReadOnly         = drvMediaISOIsReadOnly;
    pData->IMedia.pfnBiosGetPCHSGeometry = drvMediaISOBiosGetPCHSGeometry;
    pData->IMedia.pfnBiosSetPCHSGeometry = drvMediaISOBiosSetPCHSGeometry;
    pData->IMedia.pfnBiosGetLCHSGeometry = drvMediaISOBiosGetLCHSGeometry;
    pData->IMedia.pfnBiosSetLCHSGeometry = drvMediaISOBiosSetLCHSGeometry;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    char *pszName;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "Path", &pszName);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"Path\" string return %Vra.\n", rc));
        return rc;
    }

    /*
     * Open the image.
     */
    rc = RTFileOpen(&pData->File, pszName,
                    RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("drvMediaISOConstruct: ISO image '%s' opened successfully.\n", pszName));
        pData->pszFilename = pszName;
    }
    else
    {
        AssertMsgFailed(("Could not open ISO file %s, rc=%Vrc\n", pszName, rc));
        MMR3HeapFree(pszName);
    }

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
static DECLCALLBACK(void) drvMediaISODestruct(PPDMDRVINS pDrvIns)
{
    PDRVMEDIAISO pData = PDMINS2DATA(pDrvIns, PDRVMEDIAISO);
    LogFlow(("drvMediaISODestruct: '%s'\n", pData->pszFilename));

    if (pData->File != NIL_RTFILE)
    {
        RTFileClose(pData->File);
        pData->File = NIL_RTFILE;
    }
    if (pData->pszFilename)
        MMR3HeapFree(pData->pszFilename);
}


/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvMediaISOGetSize(PPDMIMEDIA pInterface)
{
    PDRVMEDIAISO pData = PDMIMEDIA_2_DRVMEDIAISO(pInterface);
    LogFlow(("drvMediaISOGetSize: '%s'\n", pData->pszFilename));

    uint64_t cbFile;
    int rc = RTFileGetSize(pData->File, &cbFile);
    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("drvMediaISOGetSize: returns %lld (%s)\n", cbFile, pData->pszFilename));
        return cbFile;
    }

    AssertMsgFailed(("Error querying ISO file size, rc=%Vrc. (%s)\n", rc, pData->pszFilename));
    return 0;
}


/** @copydoc PDMIMEDIA::pfnBiosGetPCHSGeometry */
static DECLCALLBACK(int) drvMediaISOBiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosSetPCHSGeometry */
static DECLCALLBACK(int) drvMediaISOBiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosGetLCHSGeometry */
static DECLCALLBACK(int) drvMediaISOBiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosSetLCHSGeometry */
static DECLCALLBACK(int) drvMediaISOBiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Read bits.
 *
 * @see PDMIMEDIA::pfnRead for details.
 */
static DECLCALLBACK(int) drvMediaISORead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    PDRVMEDIAISO pData = PDMIMEDIA_2_DRVMEDIAISO(pInterface);
    LogFlow(("drvMediaISORead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n", off, pvBuf, cbRead, pData->pszFilename));

    Assert(pData->File);
    Assert(pvBuf);

    /*
     * Seek to the position and read.
     */
    int rc = RTFileSeek(pData->File, off, RTFILE_SEEK_BEGIN, NULL);
    if (VBOX_SUCCESS(rc))
    {
        rc = RTFileRead(pData->File, pvBuf, cbRead, NULL);
        if (VBOX_SUCCESS(rc))
        {
            Log2(("drvMediaISORead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n"
                  "%16.*Vhxd\n",
                  off, pvBuf, cbRead, pData->pszFilename,
                  cbRead, pvBuf));
        }
        else
            AssertMsgFailed(("RTFileRead(%d, %p, %#x) -> %Vrc (off=%#llx '%s')\n",
                             pData->File, pvBuf, cbRead, rc, off, pData->pszFilename));
    }
    else
        AssertMsgFailed(("RTFileSeek(%d,%#llx,) -> %Vrc\n", pData->File, off, rc));
    LogFlow(("drvMediaISORead: returns %Vrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvMediaISOWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    AssertMsgFailed(("Attempt to write to an ISO file!\n"));
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvMediaISOFlush(PPDMIMEDIA pInterface)
{
    /* No buffered data that still needs to be written. */
    return VINF_SUCCESS;
}


/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvMediaISOGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    LogFlow(("drvMediaISOGetUuid: returns VERR_NOT_IMPLEMENTED\n"));
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvMediaISOIsReadOnly(PPDMIMEDIA pInterface)
{
    return true;
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
static DECLCALLBACK(void *) drvMediaISOQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PDRVMEDIAISO pData = PDMINS2DATA(pDrvIns, PDRVMEDIAISO);
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


/**
 * ISO media driver registration record.
 */
const PDMDRVREG g_DrvMediaISO =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "MediaISO",
    /* pszDescription */
    "ISO media access driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVMEDIAISO),
    /* pfnConstruct */
    drvMediaISOConstruct,
    /* pfnDestruct */
    drvMediaISODestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};
