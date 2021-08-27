/* $Id$ */
/** @file
 * TPM host access driver.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_TCP /** @todo */
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmtpmifs.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/tpm.h>

#include <iprt/formats/tpm.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * TPM Host driver instance data.
 *
 * @implements PDMITPMCONNECTOR
 */
typedef struct DRVTPMHOST
{
    /** The stream interface. */
    PDMITPMCONNECTOR    ITpmConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;

    /** Handle to the host TPM. */
    RTTPM               hTpm;

} DRVTPMHOST;
/** Pointer to the TPM emulator instance data. */
typedef DRVTPMHOST *PDRVTPMHOST;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/** @interface_method_impl{PDMITPMCONNECTOR,pfnStartup} */
static DECLCALLBACK(int) drvTpmHostStartup(PPDMITPMCONNECTOR pInterface, size_t cbCmdResp)
{
    RT_NOREF(pInterface, cbCmdResp);
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnShutdown} */
static DECLCALLBACK(int) drvTpmHostShutdown(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnReset} */
static DECLCALLBACK(int) drvTpmHostReset(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetVersion} */
static DECLCALLBACK(TPMVERSION) drvTpmHostGetVersion(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMHOST pThis = RT_FROM_MEMBER(pInterface, DRVTPMHOST, ITpmConnector);
    RTTPMVERSION enmVersion = RTTpmGetVersion(pThis->hTpm);

    switch (enmVersion)
    {
        case RTTPMVERSION_1_2:
            return TPMVERSION_1_2;
        case RTTPMVERSION_2_0:
            return TPMVERSION_2_0;
        case RTTPMVERSION_UNKNOWN:
        default:
            return TPMVERSION_UNKNOWN;
    }

    AssertFailed(); /* Shouldnb't get here. */
    return TPMVERSION_UNKNOWN;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetEstablishedFlag} */
static DECLCALLBACK(bool) drvTpmHostGetEstablishedFlag(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return false;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnResetEstablishedFlag} */
static DECLCALLBACK(int) drvTpmHostResetEstablishedFlag(PPDMITPMCONNECTOR pInterface, uint8_t bLoc)
{
    RT_NOREF(pInterface, bLoc);
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdExec} */
static DECLCALLBACK(int) drvTpmHostCmdExec(PPDMITPMCONNECTOR pInterface, uint8_t bLoc, const void *pvCmd, size_t cbCmd, void *pvResp, size_t cbResp)
{
    RT_NOREF(bLoc);
    PDRVTPMHOST pThis = RT_FROM_MEMBER(pInterface, DRVTPMHOST, ITpmConnector);

    return RTTpmReqExec(pThis->hTpm, 0 /*bLoc*/, pvCmd, cbCmd, pvResp, cbResp, NULL /*pcbResp*/);
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdCancel} */
static DECLCALLBACK(int) drvTpmHostCmdCancel(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMHOST pThis = RT_FROM_MEMBER(pInterface, DRVTPMHOST, ITpmConnector);

    return RTTpmReqCancel(pThis->hTpm);
}


/** @interface_method_impl{PDMIBASE,pfnQueryInterface} */
static DECLCALLBACK(void *) drvTpmHostQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTPMHOST     pThis   = PDMINS_2_DATA(pDrvIns, PDRVTPMHOST);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMITPMCONNECTOR, &pThis->ITpmConnector);
    return NULL;
}


/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/** @copydoc FNPDMDRVDESTRUCT */
static DECLCALLBACK(void) drvTpmHostDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    PDRVTPMHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMHOST);
    LogFlow(("%s\n", __FUNCTION__));

    if (pThis->hTpm != NIL_RTTPM)
    {
        int rc = RTTpmClose(pThis->hTpm);
        AssertRC(rc);

        pThis->hTpm = NIL_RTTPM;
    }
}


/** @copydoc FNPDMDRVCONSTRUCT */
static DECLCALLBACK(int) drvTpmHostConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVTPMHOST pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMHOST);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                                  = pDrvIns;
    pThis->hTpm                                     = NIL_RTTPM;

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface                = drvTpmHostQueryInterface;
    /* ITpmConnector */
    pThis->ITpmConnector.pfnStartup                 = drvTpmHostStartup;
    pThis->ITpmConnector.pfnShutdown                = drvTpmHostShutdown;
    pThis->ITpmConnector.pfnReset                   = drvTpmHostReset;
    pThis->ITpmConnector.pfnGetVersion              = drvTpmHostGetVersion;
    pThis->ITpmConnector.pfnGetEstablishedFlag      = drvTpmHostGetEstablishedFlag;
    pThis->ITpmConnector.pfnResetEstablishedFlag    = drvTpmHostResetEstablishedFlag;
    pThis->ITpmConnector.pfnCmdExec                 = drvTpmHostCmdExec;
    pThis->ITpmConnector.pfnCmdCancel               = drvTpmHostCmdCancel;

    /*
     * Validate and read the configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "TpmId", "");

    uint32_t idTpm = RTTPM_ID_DEFAULT;
    int rc = CFGMR3QueryU32Def(pCfg, "TpmId", &idTpm, RTTPM_ID_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"TpmId\" resulted in %Rrc"), rc);

    rc = RTTpmOpen(&pThis->hTpm, idTpm);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("DrvTpmHost%d: Opening TPM with id %u failed with %Rrc"), idTpm, rc);

    LogRel(("DrvTpmHost#%d: Connected to TPM %u.\n", pDrvIns->iInstance, idTpm));
    return VINF_SUCCESS;
}


/**
 * TPM host driver registration record.
 */
const PDMDRVREG g_DrvTpmHost =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "TpmHost",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "TPM host driver.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVTPMHOST),
    /* pfnConstruct */
    drvTpmHostConstruct,
    /* pfnDestruct */
    drvTpmHostDestruct,
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

