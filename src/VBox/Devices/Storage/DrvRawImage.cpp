/* $Id$ */
/** @file
 * VBox storage devices: Raw image driver
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_RAW_IMAGE
#include <VBox/vmm/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
 *
 * @implements  PDMIMEDIA
 */
typedef struct DRVRAWIMAGE
{
    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
    /** Pointer to the filename. (Freed by MM) */
    char           *pszFilename;
    /** File handle of the raw image file. */
    RTFILE          hFile;
    /** True if the image is operating in readonly mode. */
    bool            fReadOnly;
} DRVRAWIMAGE, *PDRVRAWIMAGE;



/* -=-=-=-=- PDMIMEDIA -=-=-=-=- */

/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvRawImageGetSize(PPDMIMEDIA pInterface)
{
    PDRVRAWIMAGE pThis = RT_FROM_MEMBER(pInterface, DRVRAWIMAGE, IMedia);
    LogFlow(("drvRawImageGetSize: '%s'\n", pThis->pszFilename));

    uint64_t cbFile;
    int rc = RTFileGetSize(pThis->hFile, &cbFile);
    if (RT_SUCCESS(rc))
    {
        LogFlow(("drvRawImageGetSize: returns %lld (%s)\n", cbFile, pThis->pszFilename));
        return cbFile;
    }

    AssertMsgFailed(("Error querying Raw image file size, rc=%Rrc. (%s)\n", rc, pThis->pszFilename));
    return 0;
}


/** @copydoc PDMIMEDIA::pfnBiosGetPCHSGeometry */
static DECLCALLBACK(int) drvRawImageBiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosSetPCHSGeometry */
static DECLCALLBACK(int) drvRawImageBiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosGetLCHSGeometry */
static DECLCALLBACK(int) drvRawImageBiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnBiosSetLCHSGeometry */
static DECLCALLBACK(int) drvRawImageBiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Read bits.
 *
 * @see PDMIMEDIA::pfnRead for details.
 */
static DECLCALLBACK(int) drvRawImageRead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead)
{
    PDRVRAWIMAGE pThis = RT_FROM_MEMBER(pInterface, DRVRAWIMAGE, IMedia);
    LogFlow(("drvRawImageRead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n", off, pvBuf, cbRead, pThis->pszFilename));

    Assert(pThis->hFile != NIL_RTFILE);
    Assert(pvBuf);

    /*
     * Seek to the position and read.
     */
    int rc = RTFileSeek(pThis->hFile, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileRead(pThis->hFile, pvBuf, cbRead, NULL);
        if (RT_SUCCESS(rc))
        {
            Log2(("drvRawImageRead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n"
                  "%16.*Rhxd\n",
                  off, pvBuf, cbRead, pThis->pszFilename,
                  cbRead, pvBuf));
        }
        else
            AssertMsgFailed(("RTFileRead(%RTfile, %p, %#x) -> %Rrc (off=%#llx '%s')\n",
                             pThis->hFile, pvBuf, cbRead, rc, off, pThis->pszFilename));
    }
    else
        AssertMsgFailed(("RTFileSeek(%RTfile,%#llx,) -> %Rrc\n", pThis->hFile, off, rc));
    LogFlow(("drvRawImageRead: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvRawImageWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PDRVRAWIMAGE pThis = RT_FROM_MEMBER(pInterface, DRVRAWIMAGE, IMedia);
    LogFlow(("drvRawImageWrite: off=%#llx pvBuf=%p cbWrite=%#x (%s)\n", off, pvBuf, cbWrite, pThis->pszFilename));

    Assert(pThis->hFile != NIL_RTFILE);
    Assert(pvBuf);

    /*
     * Seek to the position and write.
     */
    int rc = RTFileSeek(pThis->hFile, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(pThis->hFile, pvBuf, cbWrite, NULL);
        if (RT_SUCCESS(rc))
        {
            Log2(("drvRawImageWrite: off=%#llx pvBuf=%p cbWrite=%#x (%s)\n"
                  "%16.*Rhxd\n",
                  off, pvBuf, cbWrite, pThis->pszFilename,
                  cbWrite, pvBuf));
        }
        else
            AssertMsgFailed(("RTFileWrite(%RTfile, %p, %#x) -> %Rrc (off=%#llx '%s')\n",
                             pThis->hFile, pvBuf, cbWrite, rc, off, pThis->pszFilename));
    }
    else
        AssertMsgFailed(("RTFileSeek(%RTfile,%#llx,) -> %Rrc\n", pThis->hFile, off, rc));
    LogFlow(("drvRawImageWrite: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvRawImageFlush(PPDMIMEDIA pInterface)
{
    PDRVRAWIMAGE pThis = RT_FROM_MEMBER(pInterface, DRVRAWIMAGE, IMedia);
    LogFlow(("drvRawImageFlush: (%s)\n", pThis->pszFilename));

    Assert(pThis->hFile != NIL_RTFILE);
    int rc = RTFileFlush(pThis->hFile);
    LogFlow(("drvRawImageFlush: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnGetUuid */
static DECLCALLBACK(int) drvRawImageGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid)
{
    LogFlow(("drvRawImageGetUuid: returns VERR_NOT_IMPLEMENTED\n"));
    return VERR_NOT_IMPLEMENTED;
}


/** @copydoc PDMIMEDIA::pfnIsReadOnly */
static DECLCALLBACK(bool) drvRawImageIsReadOnly(PPDMIMEDIA pInterface)
{
    PDRVRAWIMAGE pThis = RT_FROM_MEMBER(pInterface, DRVRAWIMAGE, IMedia);
    return pThis->fReadOnly;
}


/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvRawImageQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVRAWIMAGE    pThis   = PDMINS_2_DATA(pDrvIns, PDRVRAWIMAGE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIA, &pThis->IMedia);
    return NULL;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvRawImageDestruct(PPDMDRVINS pDrvIns)
{
    PDRVRAWIMAGE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWIMAGE);
    LogFlow(("drvRawImageDestruct: '%s'\n", pThis->pszFilename));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    RTFileClose(pThis->hFile);
    pThis->hFile = NIL_RTFILE;

    if (pThis->pszFilename)
    {
        MMR3HeapFree(pThis->pszFilename);
        pThis->pszFilename = NULL;
    }
}


/**
 * Construct a raw image driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvRawImageConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVRAWIMAGE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWIMAGE);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->hFile                        = NIL_RTFILE;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvRawImageQueryInterface;
    /* IMedia */
    pThis->IMedia.pfnRead               = drvRawImageRead;
    pThis->IMedia.pfnWrite              = drvRawImageWrite;
    pThis->IMedia.pfnFlush              = drvRawImageFlush;
    pThis->IMedia.pfnGetSize            = drvRawImageGetSize;
    pThis->IMedia.pfnGetUuid            = drvRawImageGetUuid;
    pThis->IMedia.pfnIsReadOnly         = drvRawImageIsReadOnly;
    pThis->IMedia.pfnBiosGetPCHSGeometry = drvRawImageBiosGetPCHSGeometry;
    pThis->IMedia.pfnBiosSetPCHSGeometry = drvRawImageBiosSetPCHSGeometry;
    pThis->IMedia.pfnBiosGetLCHSGeometry = drvRawImageBiosGetLCHSGeometry;
    pThis->IMedia.pfnBiosSetLCHSGeometry = drvRawImageBiosSetLCHSGeometry;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Path\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    char *pszName;
    int rc = CFGMR3QueryStringAlloc(pCfg, "Path", &pszName);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"Path\" string return %Rrc.\n", rc));
        return rc;
    }

    /*
     * Open the image.
     */
    rc = RTFileOpen(&pThis->hFile, pszName, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        LogFlow(("drvRawImageConstruct: Raw image '%s' opened successfully.\n", pszName));
        pThis->pszFilename = pszName;
        pThis->fReadOnly = false;
    }
    else
    {
        rc = RTFileOpen(&pThis->hFile, pszName, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
        if (RT_SUCCESS(rc))
        {
            LogFlow(("drvRawImageConstruct: Raw image '%s' opened successfully.\n", pszName));
            pThis->pszFilename = pszName;
            pThis->fReadOnly = true;
        }
        else
        {
            AssertMsgFailed(("Could not open Raw image file %s, rc=%Rrc\n", pszName, rc));
            MMR3HeapFree(pszName);
        }
    }

    return rc;
}


/**
 * Raw image driver registration record.
 */
const PDMDRVREG g_DrvRawImage =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "RawImage",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Raw image access driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVRAWIMAGE),
    /* pfnConstruct */
    drvRawImageConstruct,
    /* pfnDestruct */
    drvRawImageDestruct,
    /* pfnRelocate */
    NULL,
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
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
