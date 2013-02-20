/* $Id$ */
/** @file
 * UsbWebcamInterface - Driver Interface for USB Webcam emulation.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#define LOG_GROUP LOG_GROUP_USB_WEBCAM
#include "UsbWebcamInterface.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include <VBox/vmm/pdmwebcaminfs.h>


typedef struct EMWEBCAMDRV
{
    EmWebcam *pEmWebcam;
    PPDMIWEBCAMUP  pIWebcamUp;
    PDMIWEBCAMDOWN IWebcamDown;
} EMWEBCAMDRV, *PEMWEBCAMDRV;

typedef struct EMWEBCAMREMOTE
{
    EmWebcam *pEmWebcam;

    VRDEVIDEOINDEVICEHANDLE deviceHandle; /* The remote identifier. */
    uint32_t u32Version;                  /* VRDE_VIDEOIN_NEGOTIATE_VERSION */
    uint32_t fu32Capabilities;            /* VRDE_VIDEOIN_NEGOTIATE_CAP_* */

    /* The device identifier for the PDM device.*/
    uint64_t u64DeviceId;
} EMWEBCAMREMOTE;

typedef struct EMWEBCAMREQCTX
{
    EMWEBCAMREMOTE *pRemote;
    void *pvUser;
} EMWEBCAMREQCTX;


static DECLCALLBACK(int) drvEmWebcamControl(PPDMIWEBCAMDOWN pInterface,
                                            void *pvUser,
                                            uint64_t u64DeviceId,
                                            const PDMIWEBCAM_CTRLHDR *pCtrl,
                                            uint32_t cbCtrl)
{
    PEMWEBCAMDRV pThis = RT_FROM_MEMBER(pInterface, EMWEBCAMDRV, IWebcamDown);

    LogFlowFunc(("pEmWebcam:%p, u64DeviceId %lld\n", pThis->pEmWebcam, u64DeviceId));

    return pThis->pEmWebcam->SendControl(pThis, pvUser, u64DeviceId, (const VRDEVIDEOINCTRLHDR *)pCtrl, cbCtrl);
}


EmWebcam::EmWebcam(Console *console)
    : mpDrv(NULL),
      mParent(console),
      mpRemote(NULL),
      mu64DeviceIdSrc(0)
{
}

EmWebcam::~EmWebcam()
{
    if (mpDrv)
    {
        mpDrv->pEmWebcam = NULL;
        mpDrv = NULL;
    }
}

void EmWebcam::EmWebcamConstruct(EMWEBCAMDRV *pDrv)
{
    AssertReturnVoid(mpDrv == NULL);

    mpDrv = pDrv;
}

void EmWebcam::EmWebcamDestruct(EMWEBCAMDRV *pDrv)
{
    AssertReturnVoid(pDrv == mpDrv);

    if (mpRemote)
    {
        mParent->consoleVRDPServer()->VideoInDeviceDetach(&mpRemote->deviceHandle);
        RTMemFree(mpRemote);
        mpRemote = NULL;
    }

    mpDrv = NULL;
}

void EmWebcam::EmWebcamCbNotify(uint32_t u32Id, const void *pvData, uint32_t cbData)
{
    int rc = VINF_SUCCESS;

    switch (u32Id)
    {
        case VRDE_VIDEOIN_NOTIFY_ID_ATTACH:
        {
            VRDEVIDEOINNOTIFYATTACH *p = (VRDEVIDEOINNOTIFYATTACH *)pvData;

            /* Older versions did not report u32Version and fu32Capabilities. */
            uint32_t u32Version = 1;
            uint32_t fu32Capabilities = VRDE_VIDEOIN_NEGOTIATE_CAP_VOID;

            if (cbData >= RT_OFFSETOF(VRDEVIDEOINNOTIFYATTACH, u32Version) + sizeof(p->u32Version))
            {
                u32Version = p->u32Version;
            }

            if (cbData >= RT_OFFSETOF(VRDEVIDEOINNOTIFYATTACH, fu32Capabilities) + sizeof(p->fu32Capabilities))
            {
                fu32Capabilities = p->fu32Capabilities;
            }

            LogFlowFunc(("ATTACH[%d,%d] version %d, caps 0x%08X\n",
                         p->deviceHandle.u32ClientId, p->deviceHandle.u32DeviceId,
                         u32Version, fu32Capabilities));

            /* Currently only one device is allowed. */
            if (mpRemote)
            {
                AssertFailed();
                rc = VERR_NOT_SUPPORTED;
                break;
            }

            EMWEBCAMREMOTE *pRemote = (EMWEBCAMREMOTE *)RTMemAllocZ(sizeof(EMWEBCAMREMOTE));
            if (pRemote == NULL)
            {
                rc = VERR_NO_MEMORY;
                break;
            }

            pRemote->pEmWebcam        = this;
            pRemote->deviceHandle     = p->deviceHandle;
            pRemote->u32Version       = u32Version;
            pRemote->fu32Capabilities = fu32Capabilities;
            pRemote->u64DeviceId      = ASMAtomicIncU64(&mu64DeviceIdSrc);

            mpRemote = pRemote;

            /* Tell the server that this webcam will be used. */
            rc = mParent->consoleVRDPServer()->VideoInDeviceAttach(&mpRemote->deviceHandle, mpRemote);
            if (RT_FAILURE(rc))
            {
                RTMemFree(mpRemote);
                mpRemote = NULL;
                break;
            }

            /* Get the device description. */
            rc = mParent->consoleVRDPServer()->VideoInGetDeviceDesc(NULL, &mpRemote->deviceHandle);

            if (RT_FAILURE(rc))
            {
                mParent->consoleVRDPServer()->VideoInDeviceDetach(&mpRemote->deviceHandle);
                RTMemFree(mpRemote);
                mpRemote = NULL;
                break;
            }

            LogFlowFunc(("sent DeviceDesc\n"));
        } break;

        case VRDE_VIDEOIN_NOTIFY_ID_DETACH:
        {
            VRDEVIDEOINNOTIFYDETACH *p = (VRDEVIDEOINNOTIFYDETACH *)pvData;
            Assert(cbData == sizeof(VRDEVIDEOINNOTIFYDETACH));

            LogFlowFunc(("DETACH[%d,%d]\n", p->deviceHandle.u32ClientId, p->deviceHandle.u32DeviceId));

            /* @todo */
            if (mpRemote)
            {
                if (mpDrv && mpDrv->pIWebcamUp)
                {
                    mpDrv->pIWebcamUp->pfnWebcamUpDetached(mpDrv->pIWebcamUp,
                                                           mpRemote->u64DeviceId);
                }

                /* No need to tell the server by calling VideoInDeviceDetach because the server is telling. */
                RTMemFree(mpRemote);
                mpRemote = NULL;
            }
        } break;

        default:
            rc = VERR_INVALID_PARAMETER;
            AssertFailed();
            break;
    }

    return;
}

void EmWebcam::EmWebcamCbDeviceDesc(int rcRequest, void *pDeviceCtx, void *pvUser,
                                    const VRDEVIDEOINDEVICEDESC *pDeviceDesc, uint32_t cbDeviceDesc)
{
    EMWEBCAMREMOTE *pRemote = (EMWEBCAMREMOTE *)pDeviceCtx;
    Assert(pRemote == mpRemote);

    LogFlowFunc(("mpDrv %p, rcRequest %Rrc %p %p %p %d\n",
                 mpDrv, rcRequest, pDeviceCtx, pvUser, pDeviceDesc, cbDeviceDesc));

    if (RT_SUCCESS(rcRequest))
    {
        if (mpDrv && mpDrv->pIWebcamUp)
        {
            mpDrv->pIWebcamUp->pfnWebcamUpAttached(mpDrv->pIWebcamUp,
                                                   pRemote->u64DeviceId,
                                                   (const PDMIWEBCAM_DEVICEDESC *)pDeviceDesc,
                                                   cbDeviceDesc,
                                                   pRemote->u32Version,
                                                   pRemote->fu32Capabilities);
        }
    }
    else
    {
        mParent->consoleVRDPServer()->VideoInDeviceDetach(&mpRemote->deviceHandle);
        RTMemFree(mpRemote);
        mpRemote = NULL;
    }
}

void EmWebcam::EmWebcamCbControl(int rcRequest, void *pDeviceCtx, void *pvUser,
                                 const VRDEVIDEOINCTRLHDR *pControl, uint32_t cbControl)
{
    EMWEBCAMREMOTE *pRemote = (EMWEBCAMREMOTE *)pDeviceCtx;
    Assert(pRemote == mpRemote);

    LogFlowFunc(("rcRequest %Rrc %p %p %p %d\n",
                 rcRequest, pDeviceCtx, pvUser, pControl, cbControl));

    bool fResponse = (pvUser != NULL);

    if (mpDrv && mpDrv->pIWebcamUp)
    {
        mpDrv->pIWebcamUp->pfnWebcamUpControl(mpDrv->pIWebcamUp,
                                              fResponse,
                                              pvUser,
                                              mpRemote->u64DeviceId,
                                              (const PDMIWEBCAM_CTRLHDR *)pControl,
                                              cbControl);
    }

    RTMemFree(pvUser);
}

void EmWebcam::EmWebcamCbFrame(int rcRequest, void *pDeviceCtx,
                               const VRDEVIDEOINPAYLOADHDR *pFrame, uint32_t cbFrame)
{
    LogFlowFunc(("rcRequest %Rrc %p %p %d\n",
                 rcRequest, pDeviceCtx, pFrame, cbFrame));

    if (mpDrv && mpDrv->pIWebcamUp)
    {
        mpDrv->pIWebcamUp->pfnWebcamUpFrame(mpDrv->pIWebcamUp,
                                            mpRemote->u64DeviceId,
                                            (const uint8_t *)pFrame,
                                            cbFrame);
    }
}

int EmWebcam::SendControl(EMWEBCAMDRV *pDrv, void *pvUser, uint64_t u64DeviceId,
                          const VRDEVIDEOINCTRLHDR *pControl, uint32_t cbControl)
{
    AssertReturn(pDrv == mpDrv, VERR_NOT_SUPPORTED);

    int rc = VINF_SUCCESS;

    EMWEBCAMREQCTX *pCtx = NULL;

    /* Verify that there is a remote device. */
    if (   !mpRemote
        || mpRemote->u64DeviceId != u64DeviceId)
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        pCtx = (EMWEBCAMREQCTX *)RTMemAlloc(sizeof(EMWEBCAMREQCTX));
        if (!pCtx)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
    {
        pCtx->pRemote = mpRemote;
        pCtx->pvUser = pvUser;

        rc = mParent->consoleVRDPServer()->VideoInControl(pCtx, &mpRemote->deviceHandle, pControl, cbControl);

        if (RT_FAILURE(rc))
        {
            RTMemFree(pCtx);
        }
    }

    return rc;
}

/* static */ DECLCALLBACK(void *) EmWebcam::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PEMWEBCAMDRV pThis = PDMINS_2_DATA(pDrvIns, PEMWEBCAMDRV);

    LogFlowFunc(("pszIID:%s\n", pszIID));

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIWEBCAMDOWN, &pThis->IWebcamDown);
    return NULL;
}

/* static */ DECLCALLBACK(int) EmWebcam::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    LogFlowFunc(("iInstance:%d, pCfg:%p, fFlags:%x\n", pDrvIns->iInstance, pCfg, fFlags));

    PEMWEBCAMDRV pThis = PDMINS_2_DATA(pDrvIns, PEMWEBCAMDRV);

    if (!CFGMR3AreValuesValid(pCfg, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /* Check early that there is a device. No need to init anything if there is no device. */
    pThis->pIWebcamUp = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIWEBCAMUP);
    if (pThis->pIWebcamUp == NULL)
    {
        LogRel(("USBWEBCAM: Emulated webcam device does not exist.\n"));
        return VERR_PDM_MISSING_INTERFACE;
    }

    void *pv = NULL;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (!RT_VALID_PTR(pv))
         rc = VERR_INVALID_PARAMETER;
    AssertMsgReturn(RT_SUCCESS(rc),
                    ("Configuration error: No/bad \"Object\" %p value! rc=%Rrc\n", pv, rc), rc);

    /* Everything ok. Initialize. */
    pThis->pEmWebcam = (EmWebcam *)pv;
    pThis->pEmWebcam->EmWebcamConstruct(pThis);

    pDrvIns->IBase.pfnQueryInterface = drvQueryInterface;

    pThis->IWebcamDown.pfnWebcamDownControl = drvEmWebcamControl;

    return VINF_SUCCESS;
}

/* static */ DECLCALLBACK(void) EmWebcam::drvDestruct(PPDMDRVINS pDrvIns)
{
    PEMWEBCAMDRV pThis = PDMINS_2_DATA(pDrvIns, PEMWEBCAMDRV);

    LogFlowFunc(("iInstance %d, pEmWebcam %p, pIWebcamUp %p\n",
                 pDrvIns->iInstance, pThis->pEmWebcam, pThis->pIWebcamUp));

    if (pThis->pEmWebcam)
    {
        pThis->pEmWebcam->EmWebcamDestruct(pThis);
        pThis->pEmWebcam = NULL;
    }
}

/* static */ const PDMDRVREG EmWebcam::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName[32] */
    "EmWebcam",
    /* szRCMod[32] */
    "",
    /* szR0Mod[32] */
    "",
    /* pszDescription */
    "Main Driver communicating with VRDE",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_USB,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(EMWEBCAMDRV),
    /* pfnConstruct */
    EmWebcam::drvConstruct,
    /* pfnDestruct */
    EmWebcam::drvDestruct,
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
