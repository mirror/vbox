/* $Id$ */
/** @file
 * VBox NVRAM COM Class implementation.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include "Nvram.h"
#include "ConsoleImpl.h"

#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmnvram.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/base64.h>
#include <VBox/version.h>
#include <iprt/file.h>
#include <iprt/semaphore.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct NVRAM NVRAM;
typedef struct NVRAM *PNVRAM;

struct NVRAM
{
    Nvram *pNvram;
    PDMINVRAM INvram;
    int cLoadedVariables;
    bool fPermanentSave;
};


/**
 * Constructor/destructor
 */
Nvram::Nvram(Console *console)
    : mpDrv(NULL),
      mParent(console)
{
}

Nvram::~Nvram()
{
    if (mpDrv)
    {
        mpDrv->pNvram = NULL;
        mpDrv = NULL;
    }
}


/**
 * @interface_method_impl(PDMINVRAM,pfnStoreNvramValue)
 */
DECLCALLBACK(int) drvNvram_pfnStoreNvramValue(PPDMINVRAM pInterface,
                                              int idxVariable,
                                              RTUUID *pVendorUuid,
                                              const char *pcszVariableName,
                                              size_t cbVariableName,
                                              uint8_t *pu8Value,
                                              size_t cbValue)
{
    int rc = VINF_SUCCESS;
    char szExtraDataKey[256];
    char szExtraDataValue[1024];
    LogFlowFunc(("ENTER: pVendorUuid:%RTuuid, pcszVariableName:%s, cbVariableName:%d, pu8Value:%.*Rhxs, cbValue:%d\n",
                        pVendorUuid,
                        pcszVariableName,
                        cbVariableName,
                        cbValue,
                        pu8Value,
                        cbValue));
    PNVRAM pThis = RT_FROM_MEMBER(pInterface, NVRAM, INvram);
    if (!pThis->fPermanentSave)
    {
        LogFlowFuncLeaveRC(rc);
        return rc;
    }

    bool fFlushVariable = (!pu8Value);

    RT_ZERO(szExtraDataKey);
    RT_ZERO(szExtraDataValue);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableName", idxVariable);
    if (!fFlushVariable)
        RTStrPrintf(szExtraDataValue, 1024, "%s", pcszVariableName);
    pThis->pNvram->getParent()->machine()->SetExtraData(Bstr(szExtraDataKey).raw(), Bstr(szExtraDataValue).raw());

    RT_ZERO(szExtraDataKey);
    RT_ZERO(szExtraDataValue);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VendorGuid", idxVariable);
    if (!fFlushVariable)
        RTUuidToStr(pVendorUuid, szExtraDataValue, 1024);
    pThis->pNvram->getParent()->machine()->SetExtraData(Bstr(szExtraDataKey).raw(), Bstr(szExtraDataValue).raw());

    RT_ZERO(szExtraDataKey);
    RT_ZERO(szExtraDataValue);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableValueLength", idxVariable);
    if (!fFlushVariable)
        RTStrPrintf(szExtraDataValue, 1024, "%d", cbValue);
    pThis->pNvram->getParent()->machine()->SetExtraData(Bstr(szExtraDataKey).raw(), Bstr(szExtraDataValue).raw());

    RT_ZERO(szExtraDataKey);
    RT_ZERO(szExtraDataValue);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableValue", idxVariable);
    size_t cbActualSize;
    if (pu8Value)
        rc = RTBase64Encode(pu8Value, cbValue, szExtraDataValue, 1024, &cbActualSize);
    AssertRCReturn(rc, rc);
    pThis->pNvram->getParent()->machine()->SetExtraData(Bstr(szExtraDataKey).raw(), Bstr(szExtraDataValue).raw());

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl(PDMINVRAM,pfnFlushNvramStorage)
 */
DECLCALLBACK(int) drvNvram_pfnFlushNvramStorage(PPDMINVRAM pInterface)
{
    int rc = VINF_SUCCESS;
    LogFlowFuncEnter();
    PNVRAM pThis = RT_FROM_MEMBER(pInterface, NVRAM, INvram);
    if (!pThis->fPermanentSave)
    {
        LogFlowFuncLeaveRC(rc);
        return rc;
    }

    for (int idxVariable = 0; idxVariable < pThis->cLoadedVariables; ++idxVariable)
    {
        drvNvram_pfnStoreNvramValue(pInterface, idxVariable, NULL, NULL, 0, NULL, 0);
    }
    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl(PDMINVRAM,pfnStoreNvramValue)
 */
DECLCALLBACK(int) drvNvram_pfnLoadNvramValue(PPDMINVRAM pInterface,
                                             int idxVariable,
                                             RTUUID *pVendorUuid,
                                             char *pcszVariableName,
                                             size_t *pcbVariableName,
                                             uint8_t *pu8Value,
                                             size_t *pcbValue)
{
    int rc = VINF_SUCCESS;
    char szExtraDataKey[256];
    Bstr bstrValue;
    HRESULT hrc;
    LogFlowFunc(("ENTER: idxVariable:%d, *pcbVariableName:%d, *pcbValue:%d\n",
                        idxVariable,
                        *pcbVariableName,
                        *pcbValue));
    PNVRAM pThis = RT_FROM_MEMBER(pInterface, NVRAM, INvram);
    if (!pThis->fPermanentSave)
    {
        rc = VERR_NOT_FOUND;
        LogFlowFuncLeaveRC(rc);
        return rc;
    }


    RT_ZERO(szExtraDataKey);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableName", idxVariable);
    hrc = pThis->pNvram->getParent()->machine()->GetExtraData(Bstr(szExtraDataKey).raw(), bstrValue.asOutParam());
    if (!SUCCEEDED(hrc))
        return VERR_NOT_FOUND;
    *pcbVariableName = RTStrCopy(pcszVariableName, 1024, Utf8Str(bstrValue).c_str());

    RT_ZERO(szExtraDataKey);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VendorGuid", idxVariable);
    hrc = pThis->pNvram->getParent()->machine()->GetExtraData(Bstr(szExtraDataKey).raw(), bstrValue.asOutParam());
    RTUuidFromStr(pVendorUuid, Utf8Str(bstrValue).c_str());

#if 0
    RT_ZERO(szExtraDataKey);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableValueLength", idxVariable);
    hrc = pThis->pNvram->getParent()->machine()->GetExtraData(Bstr(szExtraDataKey).raw(), bstrValue.asOutParam());
    *pcbValue = Utf8Str(bstrValue).toUInt32();
#endif

    RT_ZERO(szExtraDataKey);
    RTStrPrintf(szExtraDataKey, 256, "VBoxInternal/Devices/efi/0/LUN#0/Config/NVRAM/%d/VariableValue", idxVariable);
    hrc = pThis->pNvram->getParent()->machine()->GetExtraData(Bstr(szExtraDataKey).raw(), bstrValue.asOutParam());
    rc = RTBase64Decode(Utf8Str(bstrValue).c_str(), pu8Value, 1024, pcbValue, NULL);
    AssertRCReturn(rc, rc);

    pThis->cLoadedVariables++;
    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl(PDMIBASE,pfnQueryInterface)
 */
DECLCALLBACK(void *) Nvram::drvNvram_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    LogFlow(("%s pInterface:%p, pszIID:%s\n", __FUNCTION__, pInterface, pszIID));
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PNVRAM pThis = PDMINS_2_DATA(pDrvIns, PNVRAM);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMINVRAM, &pThis->INvram);
    return NULL;
}


/**
 * @interface_method_impl(PDMDRVREG,pfnDestruct)
 */
DECLCALLBACK(void) Nvram::drvNvram_Destruct(PPDMDRVINS pDrvIns)
{
    LogFlow(("%s: iInstance/#d\n", __FUNCTION__, pDrvIns->iInstance));
    PNVRAM pThis = PDMINS_2_DATA(pDrvIns, PNVRAM);
}


/**
 * @interface_method_impl(PDMDRVREG,pfnConstruct)
 */
DECLCALLBACK(int) Nvram::drvNvram_Construct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    LogFlowFunc(("iInstance/#d, pCfg:%p, fFlags:%x\n", pDrvIns->iInstance, pCfg, fFlags));
    PNVRAM pThis = PDMINS_2_DATA(pDrvIns, PNVRAM);

    if (!CFGMR3AreValuesValid(pCfg, "Object\0"
                                    "PermanentSave\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    AssertMsgRCReturn(rc, ("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc), rc);
    pThis->pNvram = (Nvram *)pv;

    bool fPermanentSave = false;
    rc = CFGMR3QueryBool(pCfg, "PermanentSave", &fPermanentSave);
    if (   RT_SUCCESS(rc)
        || rc == VERR_CFGM_VALUE_NOT_FOUND)
        pThis->fPermanentSave = fPermanentSave;
    else
        AssertRCReturn(rc, rc);

    pDrvIns->IBase.pfnQueryInterface = Nvram::drvNvram_QueryInterface;
    pThis->INvram.pfnFlushNvramStorage = drvNvram_pfnFlushNvramStorage;
    pThis->INvram.pfnStoreNvramValue = drvNvram_pfnStoreNvramValue;
    pThis->INvram.pfnLoadNvramValue = drvNvram_pfnLoadNvramValue;

    return VINF_SUCCESS;
}


const PDMDRVREG Nvram::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName[32] */
    "NvramStorage",
    /* szRCMod[32] */
    "",
    /* szR0Mod[32] */
    "",
    /* pszDescription */
    "NVRAM Main Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_VMMDEV,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(NVRAM),
    /* pfnConstruct */
    Nvram::drvNvram_Construct,
    /* pfnDestruct */
    Nvram::drvNvram_Destruct,
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
    /* u32VersionEnd */
    PDM_DRVREG_VERSION
};
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
