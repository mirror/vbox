/* $Id$ */
/** @file
 * VirtualBox PDM Driver for Emulated USB Webcam
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ____H_USBWEBCAMINTERFACE
#define ____H_USBWEBCAMINTERFACE

#include <VBox/vmm/pdmdrv.h>
#define VRDE_VIDEOIN_WITH_VRDEINTERFACE /* Get the VRDE interface definitions. */
#include <VBox/RemoteDesktop/VRDEVideoIn.h>

class Console;
typedef struct EMWEBCAMDRV EMWEBCAMDRV;
typedef struct EMWEBCAMREMOTE EMWEBCAMREMOTE;

class EmWebcam
{
    public:
        EmWebcam(Console *console);
        virtual ~EmWebcam();

        static const PDMDRVREG DrvReg;
        EMWEBCAMDRV *mpDrv;

        void EmWebcamDestruct(EMWEBCAMDRV *pDrv);

        /* Callbacks. */
        void EmWebcamCbNotify(uint32_t u32Id, const void *pvData, uint32_t cbData);
        void EmWebcamCbDeviceDesc(int rcRequest, void *pDeviceCtx, void *pvUser,
                                  const VRDEVIDEOINDEVICEDESC *pDeviceDesc, uint32_t cbDeviceDesc);
        void EmWebcamCbControl(int rcRequest, void *pDeviceCtx, void *pvUser,
                               const VRDEVIDEOINCTRLHDR *pControl, uint32_t cbControl);
        void EmWebcamCbFrame(int rcRequest, void *pDeviceCtx,
                             const VRDEVIDEOINPAYLOADHDR *pFrame, uint32_t cbFrame);

        /* Methods for the PDM driver. */
        int SendControl(EMWEBCAMDRV *pDrv, void *pvUser, uint64_t u64DeviceId,
                        const VRDEVIDEOINCTRLHDR *pControl, uint32_t cbControl);

    private:
        static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
        static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
        static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

        Console * const mParent;

        EMWEBCAMREMOTE *mpRemote;
        uint64_t volatile mu64DeviceIdSrc;
};

#endif /* !____H_USBWEBCAMINTERFACE */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
