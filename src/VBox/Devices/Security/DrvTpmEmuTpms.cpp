/* $Id$ */
/** @file
 * TPM emulation driver based on libtpms.
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
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/uuid.h>
#include <iprt/vfs.h>
#include <iprt/zip.h>

#include <iprt/formats/tpm.h>

#include <libtpms/tpm_library.h>
#include <libtpms/tpm_error.h>
#include <libtpms/tpm_tis.h>
#include <libtpms/tpm_nvfilename.h>

#include <unistd.h>
#include <stdlib.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * TPM emulation driver instance data.
 *
 * @implements PDMITPMCONNECTOR
 */
typedef struct DRVTPMEMU
{
    /** The stream interface. */
    PDMITPMCONNECTOR    ITpmConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS          pDrvIns;

    /** The TPM version we are emulating. */
    TPMVERSION          enmVersion;
    /** The buffer size the TPM advertises. */
    uint32_t            cbBuffer;
    /** Currently set locality. */
    uint8_t             bLoc;

    /** NVRAM file path. */
    char                *pszNvramPath;

    void                *pvNvPermall;
    size_t              cbNvPermall;

    void                *pvNvVolatile;
    size_t              cbNvVolatile;

} DRVTPMEMU;
/** Pointer to the TPM emulator instance data. */
typedef DRVTPMEMU *PDRVTPMEMU;

/** The special no current locality selected value. */
#define TPM_NO_LOCALITY_SELECTED    0xff


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Pointer to the (only) instance data in this driver. */
static PDRVTPMEMU g_pDrvTpmEmuTpms = NULL;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Tries to load the NVRAM.
 *
 * @returns VBox status code.
 * @param   pThis               The emulator driver instance data.
 */
static int drvTpmEmuTpmsNvramLoad(PDRVTPMEMU pThis)
{
    RTVFSIOSTREAM   hVfsIos;
    uint32_t        offError = 0;
    RTERRINFOSTATIC ErrInfo;
    int rc = RTVfsChainOpenIoStream(pThis->pszNvramPath, RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN,
                                    &hVfsIos, &offError, RTErrInfoInitStatic(&ErrInfo));
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_FILE_NOT_FOUND) /* First run. */
            rc = VINF_SUCCESS;

        return rc;
    }

    RTVFSFSSTREAM hVfsFss = NIL_RTVFSFSSTREAM;
    rc = RTZipTarFsStreamFromIoStream(hVfsIos, 0/*fFlags*/, &hVfsFss);
    RTVfsIoStrmRelease(hVfsIos);
    if (RT_SUCCESS(rc))
    {
        /*
         * Process the stream.
         */
        for (;;)
        {
            /*
             * Retrieve the next object.
             */
            char       *pszName;
            RTVFSOBJ    hVfsObj;
            rc = RTVfsFsStrmNext(hVfsFss, &pszName, NULL, &hVfsObj);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_EOF)
                    rc = VINF_SUCCESS;
                break;
            }

            RTFSOBJINFO UnixInfo;
            rc = RTVfsObjQueryInfo(hVfsObj, &UnixInfo, RTFSOBJATTRADD_UNIX);
            if (RT_SUCCESS(rc))
            {
                switch (UnixInfo.Attr.fMode & RTFS_TYPE_MASK)
                {
                    case RTFS_TYPE_FILE:
                    {
                        void **ppvDataPtr = NULL;
                        size_t *pcbData = NULL;
                        if (!RTStrCmp(pszName, TPM_PERMANENT_ALL_NAME))
                        {
                            ppvDataPtr = &pThis->pvNvPermall;
                            pcbData    = &pThis->cbNvPermall;
                        }
                        else if (!RTStrCmp(pszName, TPM_VOLATILESTATE_NAME))
                        {
                            ppvDataPtr = &pThis->pvNvVolatile;
                            pcbData    = &pThis->cbNvVolatile;
                        }
                        else
                            rc = VERR_NOT_FOUND;

                        if (RT_SUCCESS(rc))
                        {
                            *ppvDataPtr = RTMemAllocZ(UnixInfo.cbObject);
                            if (*ppvDataPtr)
                            {
                                RTVFSIOSTREAM hVfsIosData = RTVfsObjToIoStream(hVfsObj);

                                rc = RTVfsIoStrmRead(hVfsIosData, *ppvDataPtr, UnixInfo.cbObject, true /*fBlocking*/, NULL);
                                *pcbData = UnixInfo.cbObject;
                                RTVfsIoStrmRelease(hVfsIosData);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                        break;
                    }
                    default:
                        rc = VERR_NOT_SUPPORTED;
                        break;
                }
            }

            /*
             * Release the current object and string.
             */
            RTVfsObjRelease(hVfsObj);
            RTStrFree(pszName);

            if (RT_FAILURE(rc))
                break;
        }
    }

    return rc;
}


static int drvTpmEmuTpmsNvramStoreEntity(RTVFSFSSTREAM hVfsFss, const char *pszName, const void *pvData, size_t cbData)
{
    RTVFSIOSTREAM hVfsIosData;

    int rc = RTVfsIoStrmFromBuffer(RTFILE_O_READ, pvData, cbData, &hVfsIosData);
    if (RT_SUCCESS(rc))
    {
        RTVFSOBJ hVfsObj = RTVfsObjFromIoStream(hVfsIosData);
        rc = RTVfsFsStrmAdd(hVfsFss, pszName, hVfsObj, 0 /*fFlags*/);
        RTVfsObjRelease(hVfsObj);

        RTVfsIoStrmRelease(hVfsIosData);
    }

    return rc;
}


/**
 * Stores the NVRAM content.
 *
 * @returns VBox status code.
 * @param   pThis               The emulator driver instance data.
 */
static int drvTpmEmuTpmsNvramStore(PDRVTPMEMU pThis)
{
    uint32_t        offError = 0;
    RTERRINFOSTATIC ErrInfo;
    RTVFSIOSTREAM   hVfsIos;

    int rc = RTVfsChainOpenIoStream(pThis->pszNvramPath, RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE,
                                    &hVfsIos, &offError, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
    {
        RTVFSFSSTREAM hVfsFss;
        rc = RTZipTarFsStreamToIoStream(hVfsIos, RTZIPTARFORMAT_GNU, 0 /*fFlags*/, &hVfsFss);
        if (RT_SUCCESS(rc))
        {
            rc = drvTpmEmuTpmsNvramStoreEntity(hVfsFss, TPM_PERMANENT_ALL_NAME, pThis->pvNvPermall, pThis->cbNvPermall);
            if (RT_SUCCESS(rc) && pThis->pvNvVolatile)
                rc = drvTpmEmuTpmsNvramStoreEntity(hVfsFss, TPM_VOLATILESTATE_NAME, pThis->pvNvVolatile, pThis->cbNvVolatile);

            RTVfsFsStrmRelease(hVfsFss);
        }

        RTVfsIoStrmRelease(hVfsIos);
    }

    return rc;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnStartup} */
static DECLCALLBACK(int) drvTpmEmuTpmsStartup(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);

    TPM_RESULT rcTpm = TPMLIB_MainInit();
    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
        return VINF_SUCCESS;

    LogRel(("DrvTpmEmuTpms#%u: Failed to initialize TPM emulation with %#x\n",
            pThis->pDrvIns->iInstance, rcTpm));
    return VERR_DEV_IO_ERROR;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnShutdown} */
static DECLCALLBACK(int) drvTpmEmuTpmsShutdown(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);

    TPMLIB_Terminate();
    return VINF_SUCCESS;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnReset} */
static DECLCALLBACK(int) drvTpmEmuTpmsReset(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);

    TPMLIB_Terminate();
    TPM_RESULT rcTpm = TPMLIB_MainInit();
    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
        return VINF_SUCCESS;


    LogRel(("DrvTpmEmuTpms#%u: Failed to reset TPM emulation with %#x\n",
            pThis->pDrvIns->iInstance, rcTpm));
    return VERR_DEV_IO_ERROR;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetVersion} */
static DECLCALLBACK(TPMVERSION) drvTpmEmuTpmsGetVersion(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);
    return pThis->enmVersion;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetLocalityMax} */
static DECLCALLBACK(uint32_t) drvTpmEmuGetLocalityMax(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);
    return 4;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetBufferSize} */
static DECLCALLBACK(uint32_t) drvTpmEmuGetBufferSize(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);
    return pThis->cbBuffer;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnGetEstablishedFlag} */
static DECLCALLBACK(bool) drvTpmEmuTpmsGetEstablishedFlag(PPDMITPMCONNECTOR pInterface)
{
    RT_NOREF(pInterface);

    TPM_BOOL fTpmEst = FALSE;
    TPM_RESULT rcTpm = TPM_IO_TpmEstablished_Get(&fTpmEst);
    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
        return RT_BOOL(fTpmEst);

    return false;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnResetEstablishedFlag} */
static DECLCALLBACK(int) drvTpmEmuTpmsResetEstablishedFlag(PPDMITPMCONNECTOR pInterface, uint8_t bLoc)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);
    uint8_t bLocOld = pThis->bLoc;

    pThis->bLoc = bLoc;
    TPM_RESULT rcTpm = TPM_IO_TpmEstablished_Reset();
    pThis->bLoc = bLocOld;

    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
        return VINF_SUCCESS;

    LogRelMax(10, ("DrvTpmEmuTpms#%u: Failed to reset the established flag with %#x\n",
                   pThis->pDrvIns->iInstance, rcTpm));
    return VERR_DEV_IO_ERROR;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdExec} */
static DECLCALLBACK(int) drvTpmEmuTpmsCmdExec(PPDMITPMCONNECTOR pInterface, uint8_t bLoc, const void *pvCmd, size_t cbCmd, void *pvResp, size_t cbResp)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);

    pThis->bLoc = bLoc;

    uint8_t *pbRespBuf = NULL;
    uint32_t cbRespBuf = 0;
    uint32_t cbRespActual = 0;
    TPM_RESULT rcTpm = TPMLIB_Process(&pbRespBuf, &cbRespActual, &cbRespBuf, (uint8_t *)pvCmd, cbCmd);
    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
    {
        memcpy(pvResp, pbRespBuf, RT_MIN(cbResp, cbRespActual));
        free(pbRespBuf);
        return VINF_SUCCESS;
    }

    LogRelMax(10, ("DrvTpmEmuTpms#%u: Failed to execute command with %#x\n",
                   pThis->pDrvIns->iInstance, rcTpm));
    return VERR_DEV_IO_ERROR;
}


/** @interface_method_impl{PDMITPMCONNECTOR,pfnCmdCancel} */
static DECLCALLBACK(int) drvTpmEmuTpmsCmdCancel(PPDMITPMCONNECTOR pInterface)
{
    PDRVTPMEMU pThis = RT_FROM_MEMBER(pInterface, DRVTPMEMU, ITpmConnector);

    TPM_RESULT rcTpm = TPMLIB_CancelCommand();
    if (RT_LIKELY(rcTpm == TPM_SUCCESS))
        return VINF_SUCCESS;

    LogRelMax(10, ("DrvTpmEmuTpms#%u: Failed to cancel outstanding command with %#x\n",
                   pThis->pDrvIns->iInstance, rcTpm));
    return VERR_DEV_IO_ERROR;
}


/** @interface_method_impl{PDMIBASE,pfnQueryInterface} */
static DECLCALLBACK(void *) drvTpmEmuTpmsQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS      pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVTPMEMU     pThis   = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMITPMCONNECTOR, &pThis->ITpmConnector);
    return NULL;
}


/* -=-=-=-=- libtpms_callbacks -=-=-=-=- */


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkNvRamInit(void)
{
    PDRVTPMEMU pThis = g_pDrvTpmEmuTpms;
    RT_NOREF(pThis);

    return TPM_SUCCESS;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkNvRamLoadData(uint8_t **ppvData, uint32_t *pcbLength,
                                                              uint32_t idTpm, const char *pszName)
{
    PDRVTPMEMU pThis = g_pDrvTpmEmuTpms;

    AssertReturn(idTpm == 0, TPM_FAIL);

    void *pvDataPtr = NULL;
    size_t cbData = 0;
    if (!RTStrCmp(pszName, TPM_PERMANENT_ALL_NAME))
    {
        pvDataPtr = pThis->pvNvPermall;
        cbData    = pThis->cbNvPermall;
    }
    else if (!RTStrCmp(pszName, TPM_VOLATILESTATE_NAME))
    {
        pvDataPtr = pThis->pvNvVolatile;
        cbData    = pThis->cbNvVolatile;
    }
    else
        return TPM_FAIL;

    if (pvDataPtr)
    {
        *ppvData = (uint8_t *)malloc(cbData);
        if (*ppvData)
        {
            memcpy(*ppvData, pvDataPtr, cbData);
            *pcbLength = (uint32_t)cbData;
            return TPM_SUCCESS;
        }

        return TPM_FAIL;
    }

    return TPM_RETRY;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkNvRamStoreData(const uint8_t *pvData, uint32_t cbLength,
                                                               uint32_t idTpm, const char *pszName)
{
    PDRVTPMEMU pThis = g_pDrvTpmEmuTpms;

    AssertReturn(idTpm == 0, TPM_FAIL);

    void **ppvDataPtr = NULL;
    size_t *pcbData = NULL;
    if (!RTStrCmp(pszName, TPM_PERMANENT_ALL_NAME))
    {
        ppvDataPtr = &pThis->pvNvPermall;
        pcbData    = &pThis->cbNvPermall;
    }
    else if (!RTStrCmp(pszName, TPM_VOLATILESTATE_NAME))
    {
        ppvDataPtr = &pThis->pvNvVolatile;
        pcbData    = &pThis->cbNvVolatile;
    }
    else
        return TPM_FAIL;

    if (   *ppvDataPtr
        && *pcbData == cbLength)
    {
        memcpy(*ppvDataPtr, pvData, cbLength);
        return TPM_SUCCESS;
    }
    else
    {
        if (*ppvDataPtr)
            RTMemFree(*ppvDataPtr);

        *ppvDataPtr = RTMemDup(pvData, cbLength);
        if (*ppvDataPtr)
        {
            *pcbData = cbLength;
            return TPM_SUCCESS;
        }
    }

    return TPM_FAIL;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkNvRamDeleteName(uint32_t idTpm, const char *pszName, TPM_BOOL fMustExist)
{
    PDRVTPMEMU pThis = g_pDrvTpmEmuTpms;

    AssertReturn(idTpm == 0, TPM_FAIL);

    void **ppvDataPtr = NULL;
    size_t *pcbData = NULL;
    if (!RTStrCmp(pszName, TPM_PERMANENT_ALL_NAME))
    {
        ppvDataPtr = &pThis->pvNvPermall;
        pcbData    = &pThis->cbNvPermall;
    }
    else if (!RTStrCmp(pszName, TPM_VOLATILESTATE_NAME))
    {
        ppvDataPtr = &pThis->pvNvVolatile;
        pcbData    = &pThis->cbNvVolatile;
    }
    else
        return TPM_SUCCESS;

    if (*ppvDataPtr)
    {
        RTMemFree(*ppvDataPtr);
        *ppvDataPtr = NULL;
        *pcbData    = 0;
    }
    else if (fMustExist)
        return TPM_FAIL;

    return TPM_SUCCESS;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkIoInit(void)
{
    return TPM_SUCCESS;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkIoGetLocality(TPM_MODIFIER_INDICATOR *pLocalityModifier, uint32_t idTpm)
{
    PDRVTPMEMU pThis = g_pDrvTpmEmuTpms;

    AssertReturn(idTpm == 0, TPM_FAIL);

    *pLocalityModifier = pThis->bLoc;
    return TPM_SUCCESS;
}


static DECLCALLBACK(TPM_RESULT) drvTpmEmuTpmsCbkIoGetPhysicalPresence(TPM_BOOL *pfPhysicalPresence, uint32_t idTpm)
{
    AssertReturn(idTpm == 0, TPM_FAIL);

    *pfPhysicalPresence = TRUE;
    return TPM_SUCCESS;
}


/* -=-=-=-=- PDMDRVREG -=-=-=-=- */

/** @copydoc FNPDMDRVDESTRUCT */
static DECLCALLBACK(void) drvTpmEmuTpmsDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    PDRVTPMEMU pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);
    LogFlow(("%s\n", __FUNCTION__));

    int rc = drvTpmEmuTpmsNvramStore(pThis);
    AssertRC(rc);

    if (pThis->pvNvPermall)
    {
        RTMemFree(pThis->pvNvPermall);
        pThis->pvNvPermall = NULL;
    }

    if (pThis->pvNvVolatile)
    {
        RTMemFree(pThis->pvNvVolatile);
        pThis->pvNvVolatile = NULL;
    }

#if 0
    if (pThis->pszNvramPath)
    {
        PDMDrvHlpMMHeapFree(pDrvIns, pThis->pszNvramPath);
        pThisCC->pszNvramPath = NULL;
    }
#endif
}


/** @copydoc FNPDMDRVCONSTRUCT */
static DECLCALLBACK(int) drvTpmEmuTpmsConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVTPMEMU pThis = PDMINS_2_DATA(pDrvIns, PDRVTPMEMU);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                                  = pDrvIns;
    pThis->enmVersion                               = TPMVERSION_UNKNOWN;
    pThis->bLoc                                     = TPM_NO_LOCALITY_SELECTED;
    pThis->pvNvPermall                              = NULL;
    pThis->cbNvPermall                              = 0;
    pThis->pvNvVolatile                             = NULL;
    pThis->cbNvVolatile                             = 0;

    /* IBase */
    pDrvIns->IBase.pfnQueryInterface                = drvTpmEmuTpmsQueryInterface;
    /* ITpmConnector */
    pThis->ITpmConnector.pfnStartup                 = drvTpmEmuTpmsStartup;
    pThis->ITpmConnector.pfnShutdown                = drvTpmEmuTpmsShutdown;
    pThis->ITpmConnector.pfnReset                   = drvTpmEmuTpmsReset;
    pThis->ITpmConnector.pfnGetVersion              = drvTpmEmuTpmsGetVersion;
    pThis->ITpmConnector.pfnGetLocalityMax          = drvTpmEmuGetLocalityMax;
    pThis->ITpmConnector.pfnGetBufferSize           = drvTpmEmuGetBufferSize;
    pThis->ITpmConnector.pfnGetEstablishedFlag      = drvTpmEmuTpmsGetEstablishedFlag;
    pThis->ITpmConnector.pfnResetEstablishedFlag    = drvTpmEmuTpmsResetEstablishedFlag;
    pThis->ITpmConnector.pfnCmdExec                 = drvTpmEmuTpmsCmdExec;
    pThis->ITpmConnector.pfnCmdCancel               = drvTpmEmuTpmsCmdCancel;

    /*
     * Validate and read the configuration.
     */
    PDMDRV_VALIDATE_CONFIG_RETURN(pDrvIns, "TpmVersion|BufferSize|NvramPath", "");

    TPMLIB_SetDebugFD(STDERR_FILENO);
    TPMLIB_SetDebugLevel(~0);

    int rc = CFGMR3QueryStringAlloc(pCfg, "NvramPath", &pThis->pszNvramPath);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"NvramPath\" resulted in %Rrc"), rc);

    rc = drvTpmEmuTpmsNvramLoad(pThis);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Failed to load TPM NVRAM data with %Rrc"), rc);

    TPMLIB_TPMVersion enmVersion = TPMLIB_TPM_VERSION_2;
    uint32_t uTpmVersion = 0;
    rc = CFGMR3QueryU32Def(pCfg, "TpmVersion", &uTpmVersion, 2);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"TpmVersion\" resulted in %Rrc"), rc);

    switch (uTpmVersion)
    {
        case 1:
            enmVersion = TPMLIB_TPM_VERSION_1_2;
            pThis->enmVersion = TPMVERSION_1_2;
            break;
        case 2:
            enmVersion = TPMLIB_TPM_VERSION_2;
            pThis->enmVersion = TPMVERSION_2_0;
            break;
        default:
            return PDMDrvHlpVMSetError(pDrvIns, VERR_NOT_SUPPORTED, RT_SRC_POS,
                                       N_("Configuration error: \"TpmVersion\" %u is not supported"), uTpmVersion);
    }

    TPM_RESULT rcTpm = TPMLIB_ChooseTPMVersion(enmVersion);
    if (rcTpm != TPM_SUCCESS)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Failed to set the TPM version for the emulated TPM with %d"), rcTpm);

    int cbBufferMax = 0;
    rcTpm = TPMLIB_GetTPMProperty(TPMPROP_TPM_BUFFER_MAX, &cbBufferMax);
    if (rcTpm != TPM_SUCCESS)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Querying the maximum supported buffer size failed with %u"), rcTpm);

    rc = CFGMR3QueryU32Def(pCfg, "BufferSize", &pThis->cbBuffer, (uint32_t)cbBufferMax);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS,
                                   N_("Configuration error: querying \"BufferSize\" resulted in %Rrc"), rc);

    uint32_t cbBufferMin = 0;
    uint32_t cbBuffer = TPMLIB_SetBufferSize(pThis->cbBuffer, &cbBufferMin, NULL /*max_size*/);
    if (pThis->cbBuffer != cbBuffer)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Failed to set buffer size (%u) of the emulated TPM with %u (min %u, max %d)"),
                                   pThis->cbBuffer, cbBuffer, cbBufferMin, cbBufferMax);

    struct libtpms_callbacks Callbacks;
    Callbacks.sizeOfStruct               = sizeof(Callbacks);
    Callbacks.tpm_nvram_init             = drvTpmEmuTpmsCbkNvRamInit;
    Callbacks.tpm_nvram_loaddata         = drvTpmEmuTpmsCbkNvRamLoadData;
    Callbacks.tpm_nvram_storedata        = drvTpmEmuTpmsCbkNvRamStoreData;
    Callbacks.tpm_nvram_deletename       = drvTpmEmuTpmsCbkNvRamDeleteName;
    Callbacks.tpm_io_init                = drvTpmEmuTpmsCbkIoInit;
    Callbacks.tpm_io_getlocality         = drvTpmEmuTpmsCbkIoGetLocality;
    Callbacks.tpm_io_getphysicalpresence = drvTpmEmuTpmsCbkIoGetPhysicalPresence;
    rcTpm = TPMLIB_RegisterCallbacks(&Callbacks);
    if (rcTpm != TPM_SUCCESS)
        return PDMDrvHlpVMSetError(pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Failed to register callbacks with the TPM emulation: %u"),
                                   rcTpm);

    /* We can only have one instance of the TPM emulation and require the global variable for the callbacks unfortunately. */
    g_pDrvTpmEmuTpms = pThis;
    return VINF_SUCCESS;
}


/**
 * TPM host driver registration record.
 */
const PDMDRVREG g_DrvTpmEmuTpms =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "TpmEmuTpms",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "TPM emulation driver based on libtpms.",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_STREAM,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DRVTPMEMU),
    /* pfnConstruct */
    drvTpmEmuTpmsConstruct,
    /* pfnDestruct */
    drvTpmEmuTpmsDestruct,
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

