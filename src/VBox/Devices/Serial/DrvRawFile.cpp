/* $Id$ */
/** @file
 * VBox stream drivers - Raw file output.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_DEFAULT
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "Builtins.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Converts a pointer to DRVRAWFILE::IMedia to a PDRVRAWFILE. */
#define PDMISTREAM_2_DRVRAWFILE(pInterface) ( (PDRVRAWFILE)((uintptr_t)pInterface - RT_OFFSETOF(DRVRAWFILE, IStream)) )

/** Converts a pointer to PDMDRVINS::IBase to a PPDMDRVINS. */
#define PDMIBASE_2_DRVINS(pInterface)   ( (PPDMDRVINS)((uintptr_t)pInterface - RT_OFFSETOF(PDMDRVINS, IBase)) )


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Raw file output driver instance data.
 *
 * @implements  PDMISTREAM
 */
typedef struct DRVRAWFILE
{
    /** The stream interface. */
    PDMISTREAM          IStream;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to the file name. (Freed by MM) */
    char               *pszLocation;
    /** Flag whether VirtualBox represents the server or client side. */
    RTFILE              OutputFile;
} DRVRAWFILE, *PDRVRAWFILE;



/* -=-=-=-=- PDMISTREAM -=-=-=-=- */

/** @copydoc PDMISTREAM::pfnWrite */
static DECLCALLBACK(int) drvRawFileWrite(PPDMISTREAM pInterface, const void *pvBuf, size_t *pcbWrite)
{
    int rc = VINF_SUCCESS;
    PDRVRAWFILE pThis = PDMISTREAM_2_DRVRAWFILE(pInterface);
    LogFlow(("%s: pvBuf=%p *pcbWrite=%#x (%s)\n", __FUNCTION__, pvBuf, *pcbWrite, pThis->pszLocation));

    Assert(pvBuf);
    if (pThis->OutputFile != NIL_RTFILE)
    {
        size_t cbWritten;
        rc = RTFileWrite(pThis->OutputFile, pvBuf, *pcbWrite, &cbWritten);
#if 0
        /* don't flush here, takes too long and we will loose characters */
        if (RT_SUCCESS(rc))
            RTFileFlush(pThis->OutputFile);
#endif
        *pcbWrite = cbWritten;
    }

    LogFlow(("%s: returns %Rrc\n", __FUNCTION__, rc));
    return rc;
}

/* -=-=-=-=- PDMIBASE -=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvRawFileQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS  pDrvIns = PDMIBASE_2_DRVINS(pInterface);
    PDRVRAWFILE pThis   = PDMINS_2_DATA(pDrvIns, PDRVRAWFILE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISTREAM, &pThis->IStream);
    return NULL;
}

/* -=-=-=-=- PDMDRVREG -=-=-=-=- */


/**
 * Power off a raw output stream driver instance.
 *
 * This does most of the destruction work, to avoid ordering dependencies.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvRawFilePowerOff(PPDMDRVINS pDrvIns)
{
    PDRVRAWFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWFILE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));

    if (pThis->OutputFile != NIL_RTFILE)
    {
        RTFileClose(pThis->OutputFile);
        pThis->OutputFile = NIL_RTFILE;
    }
}


/**
 * Destruct a raw output stream driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that
 * any non-VM resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvRawFileDestruct(PPDMDRVINS pDrvIns)
{
    PDRVRAWFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWFILE);
    LogFlow(("%s: %s\n", __FUNCTION__, pThis->pszLocation));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pThis->pszLocation)
        MMR3HeapFree(pThis->pszLocation);

    if (pThis->OutputFile != NIL_RTFILE)
    {
        RTFileClose(pThis->OutputFile);
        pThis->OutputFile = NIL_RTFILE;
    }
}


/**
 * Construct a raw output stream driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvRawFileConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    PDRVRAWFILE pThis = PDMINS_2_DATA(pDrvIns, PDRVRAWFILE);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
    pThis->pszLocation                  = NULL;
    pThis->OutputFile                   = NIL_RTFILE;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvRawFileQueryInterface;
    /* IStream */
    pThis->IStream.pfnWrite             = drvRawFileWrite;

    /*
     * Read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Location\0"))
        AssertFailedReturn(VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES);

    int rc = CFGMR3QueryStringAlloc(pCfgHandle, "Location", &pThis->pszLocation);
    if (RT_FAILURE(rc))
        AssertMsgFailedReturn(("Configuration error: query \"Location\" resulted in %Rrc.\n", rc), rc);

    /*
     * Open the raw file.
     */
    rc = RTFileOpen(&pThis->OutputFile, pThis->pszLocation, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
    {
        LogRel(("RawFile%d: CreateFile failed rc=%Rrc\n", pDrvIns->iInstance));
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("RawFile#%d failed to create the raw output file %s"), pDrvIns->iInstance, pThis->pszLocation);
    }

    LogFlow(("drvRawFileConstruct: location %s\n", pThis->pszLocation));
    LogRel(("RawFile#%u: location %s\n", pDrvIns->iInstance, pThis->pszLocation));
    return VINF_SUCCESS;
}


/**
 * Raw file driver registration record.
 */
const PDMDRVREG g_DrvRawFile =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "RawFile",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "RawFile stream driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVRAWFILE),
    /* pfnConstruct */
    drvRawFileConstruct,
    /* pfnDestruct */
    drvRawFileDestruct,
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
    drvRawFilePowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

