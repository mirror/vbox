/** @file
 *
 * VBox stream devices:
 * Raw file output
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
#define LOG_GROUP LOG_GROUP_DRV_NAMEDPIPE
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>

#include "Builtins.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Converts a pointer to DRVOUTPUTFILE::IMedia to a PDRVOUTPUTFILE. */
#define PDMISTREAM_2_DRVOUTPUTFILE(pInterface) ( (PDRVOUTPUTFILE)((uintptr_t)pInterface - RT_OFFSETOF(DRVOUTPUTFILE, IStream)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Raw file output driver instance data.
 */
typedef struct DRVOUTPUTFILE
{
    /** The stream interface. */
    PDMISTREAM          IStream;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to the file name. (Freed by MM) */
    char                *pszLocation;
    /** Flag whether VirtualBox represents the server or client side. */
    RTFILE              OutputFile;
    /** Flag to signal listening thread to shut down. */
    bool                fShutdown;
} DRVOUTPUTFILE, *PDRVOUTPUTFILE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/** @copydoc PDMISTREAM::pfnWrite */
static DECLCALLBACK(int) drvOutputFileWrite(PPDMISTREAM pInterface, const void *pvBuf, size_t *pcbWrite)
{
    int rc = VINF_SUCCESS;
    PDRVOUTPUTFILE pThis = PDMISTREAM_2_DRVOUTPUTFILE(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbWrite=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbWrite, pThis->pszLocation));

    Assert(pvBuf);
    if (pThis->OutputFile != NIL_RTFILE)
    {
        size_t cbWritten;
        rc = RTFileWrite(pThis->OutputFile, pvBuf, *pcbWrite, &cbWritten);
        if (RT_SUCCESS(rc))
            RTFileFlush(pThis->OutputFile);
        *pcbWrite = cbWritten;
    }

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
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
static DECLCALLBACK(void *) drvOutputFileQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PDRVOUTPUTFILE pDrv = PDMINS_2_DATA(pDrvIns, PDRVOUTPUTFILE);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_STREAM:
            return &pDrv->IStream;
        default:
            return NULL;
    }
}


/**
 * Construct a raw output stream driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvOutputFileConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    int rc;
    char *pszLocation = NULL;
    PDRVOUTPUTFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVOUTPUTFILE);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pszLocation                  = NULL;
    pThis->OutputFile                   = NIL_RTFILE;
    pThis->fShutdown                    = false;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvOutputFileQueryInterface;
    /* IStream */
    pThis->IStream.pfnWrite             = drvOutputFileWrite;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Location\0"))
    {
        rc = VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
        goto out;
    }

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "Location", &pszLocation);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: query \"Location\" resulted in %Rrc.\n", rc));
        goto out;
    }
    pThis->pszLocation = pszLocation;

    rc = RTFileOpen(&pThis->OutputFile, pThis->pszLocation, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE);
    if (RT_FAILURE(rc))
    {
        LogRel(("RawFile%d: CreateFile failed rc=%Rrc\n", pThis->pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawFile#%d failed to create the raw output file %s"), pDrvIns->iInstance, pszLocation);
    }

out:
    if (RT_FAILURE(rc))
    {
        if (pszLocation)
            MMR3HeapFree(pszLocation);
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawFile#%d failed to initialize"), pDrvIns->iInstance);
    }

    LogFlow(("drvOutputFileConstruct: location %s\n", pszLocation));
    LogRel(("RawFile: location %s\n", pszLocation));
    return VINF_SUCCESS;
}


/**
 * Destruct a raw output stream driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvOutputFileDestruct(PPDMDRVINS pDrvIns)
{
    PDRVOUTPUTFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVOUTPUTFILE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));

    if (pThis->pszLocation)
        MMR3HeapFree(pThis->pszLocation);
}


/**
 * Power off a raw output stream driver instance.
 *
 * This does most of the destruction work, to avoid ordering dependencies.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvOutputFilePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVOUTPUTFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVOUTPUTFILE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));

    pThis->fShutdown = true;

    if (pThis->OutputFile != NIL_RTFILE)
        RTFileClose(pThis->OutputFile);
}


/**
 * Raw file driver registration record.
 */
const PDMDRVREG g_DrvRawFile =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "RawFile",
    /* pszDescription */
    "RawFile stream driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVOUTPUTFILE),
    /* pfnConstruct */
    drvOutputFileConstruct,
    /* pfnDestruct */
    drvOutputFileDestruct,
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
    drvOutputFilePowerOff,
};
