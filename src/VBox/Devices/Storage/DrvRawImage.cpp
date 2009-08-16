/** @file
 *
 * VBox storage devices:
 * Raw image driver
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
#define LOG_GROUP LOG_GROUP_DRV_RAW_IMAGE
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>

#include "Builtins.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Converts a pointer to RAWIMAGE::IMedia to a PRDVRAWIMAGE. */
#define PDMIMEDIA_2_DRVRAWIMAGE(pInterface) ( (PDRVRAWIMAGE)((uintptr_t)pInterface - RT_OFFSETOF(DRVRAWIMAGE, IMedia)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/** Converts a pointer to PDMDRVINS::IBase to a PVBOXHDD. */
#define PDMIBASE_2_DRVRAWIMAGE(pInterface)  ( PDMINS_2_DATA(PDMIBASE_2_DRVINS(pInterface), PDRVRAWIMAGE) )



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Block driver instance data.
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
    RTFILE          File;
    /** True if the image is operating in readonly mode. */
    bool            fReadOnly;
} DRVRAWIMAGE, *PDRVRAWIMAGE;



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) drvRawImageRead(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead);
static DECLCALLBACK(int) drvRawImageWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite);
static DECLCALLBACK(int) drvRawImageFlush(PPDMIMEDIA pInterface);
static DECLCALLBACK(bool) drvRawImageIsReadOnly(PPDMIMEDIA pInterface);
static DECLCALLBACK(uint64_t) drvRawImageGetSize(PPDMIMEDIA pInterface);
static DECLCALLBACK(int) drvRawImageGetUuid(PPDMIMEDIA pInterface, PRTUUID pUuid);
static DECLCALLBACK(int) drvRawImageBiosGetPCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry);
static DECLCALLBACK(int) drvRawImageBiosSetPCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry);
static DECLCALLBACK(int) drvRawImageBiosGetLCHSGeometry(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry);
static DECLCALLBACK(int) drvRawImageBiosSetLCHSGeometry(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry);

static DECLCALLBACK(void *) drvRawImageQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface);




/**
 * Construct a raw image driver instance.
 *  
 * @copydoc FNPDMDRVCONSTRUCT
 */ 
static DECLCALLBACK(int) drvRawImageConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVRAWIMAGE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWIMAGE);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->File                         = NIL_RTFILE;
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
    if (!CFGMR3AreValuesValid(pCfgHandle, "Path\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    char *pszName;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "Path", &pszName);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query for \"Path\" string return %Rrc.\n", rc));
        return rc;
    }

    /*
     * Open the image.
     */
    rc = RTFileOpen(&pThis->File, pszName,
                    RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    if (RT_SUCCESS(rc))
    {
        LogFlow(("drvRawImageConstruct: Raw image '%s' opened successfully.\n", pszName));
        pThis->pszFilename = pszName;
        pThis->fReadOnly = false;
    }
    else
    {
        rc = RTFileOpen(&pThis->File, pszName,
                        RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
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

    if (pThis->File != NIL_RTFILE)
    {
        RTFileClose(pThis->File);
        pThis->File = NIL_RTFILE;
    }
    if (pThis->pszFilename)
        MMR3HeapFree(pThis->pszFilename);
}


/** @copydoc PDMIMEDIA::pfnGetSize */
static DECLCALLBACK(uint64_t) drvRawImageGetSize(PPDMIMEDIA pInterface)
{
    PDRVRAWIMAGE pThis = PDMIMEDIA_2_DRVRAWIMAGE(pInterface);
    LogFlow(("drvRawImageGetSize: '%s'\n", pThis->pszFilename));

    uint64_t cbFile;
    int rc = RTFileGetSize(pThis->File, &cbFile);
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
    PDRVRAWIMAGE pThis = PDMIMEDIA_2_DRVRAWIMAGE(pInterface);
    LogFlow(("drvRawImageRead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n", off, pvBuf, cbRead, pThis->pszFilename));

    Assert(pThis->File);
    Assert(pvBuf);

    /*
     * Seek to the position and read.
     */
    int rc = RTFileSeek(pThis->File, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileRead(pThis->File, pvBuf, cbRead, NULL);
        if (RT_SUCCESS(rc))
        {
            Log2(("drvRawImageRead: off=%#llx pvBuf=%p cbRead=%#x (%s)\n"
                  "%16.*Rhxd\n",
                  off, pvBuf, cbRead, pThis->pszFilename,
                  cbRead, pvBuf));
        }
        else
            AssertMsgFailed(("RTFileRead(%d, %p, %#x) -> %Rrc (off=%#llx '%s')\n",
                             pThis->File, pvBuf, cbRead, rc, off, pThis->pszFilename));
    }
    else
        AssertMsgFailed(("RTFileSeek(%d,%#llx,) -> %Rrc\n", pThis->File, off, rc));
    LogFlow(("drvRawImageRead: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnWrite */
static DECLCALLBACK(int) drvRawImageWrite(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PDRVRAWIMAGE pThis = PDMIMEDIA_2_DRVRAWIMAGE(pInterface);
    LogFlow(("drvRawImageWrite: off=%#llx pvBuf=%p cbWrite=%#x (%s)\n", off, pvBuf, cbWrite, pThis->pszFilename));

    Assert(pThis->File);
    Assert(pvBuf);

    /*
     * Seek to the position and write.
     */
    int rc = RTFileSeek(pThis->File, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileWrite(pThis->File, pvBuf, cbWrite, NULL);
        if (RT_SUCCESS(rc))
        {
            Log2(("drvRawImageWrite: off=%#llx pvBuf=%p cbWrite=%#x (%s)\n"
                  "%16.*Rhxd\n",
                  off, pvBuf, cbWrite, pThis->pszFilename,
                  cbWrite, pvBuf));
        }
        else
            AssertMsgFailed(("RTFileWrite(%d, %p, %#x) -> %Rrc (off=%#llx '%s')\n",
                             pThis->File, pvBuf, cbWrite, rc, off, pThis->pszFilename));
    }
    else
        AssertMsgFailed(("RTFileSeek(%d,%#llx,) -> %Rrc\n", pThis->File, off, rc));
    LogFlow(("drvRawImageWrite: returns %Rrc\n", rc));
    return rc;
}


/** @copydoc PDMIMEDIA::pfnFlush */
static DECLCALLBACK(int) drvRawImageFlush(PPDMIMEDIA pInterface)
{
    PDRVRAWIMAGE pThis = PDMIMEDIA_2_DRVRAWIMAGE(pInterface);
    LogFlow(("drvRawImageFlush: (%s)\n", pThis->pszFilename));

    Assert(pThis->File != NIL_RTFILE);
    int rc = RTFileFlush(pThis->File);
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
    PDRVRAWIMAGE pThis = PDMIMEDIA_2_DRVRAWIMAGE(pInterface);
    return pThis->fReadOnly;
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
static DECLCALLBACK(void *) drvRawImageQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PDRVRAWIMAGE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWIMAGE);
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
 * Raw image driver registration record.
 */
const PDMDRVREG g_DrvRawImage =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "RawImage",
    /* pszDescription */
    "Raw image access driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_MEDIA,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVRAWIMAGE),
    /* pfnConstruct */
    drvRawImageConstruct,
    /* pfnDestruct */
    drvRawImageDestruct,
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
