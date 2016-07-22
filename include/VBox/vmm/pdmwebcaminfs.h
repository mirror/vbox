/* $Id$ */

/** @file
 * webcaminfs - interfaces between dev and driver.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_pdmwebcaminfs_h
#define ___VBox_vmm_pdmwebcaminfs_h


typedef struct PDMIWEBCAM_DEVICEDESC PDMIWEBCAM_DEVICEDESC;
typedef struct PDMIWEBCAM_CTRLHDR PDMIWEBCAM_CTRLHDR;
typedef struct PDMIWEBCAM_FRAMEHDR PDMIWEBCAM_FRAMEHDR;


#define PDMIWEBCAMDOWN_IID "0d29b9a1-f4cd-4719-a564-38d5634ba9f8"
typedef struct PDMIWEBCAMDOWN *PPDMIWEBCAMDOWN;
typedef struct PDMIWEBCAMDOWN
{
    /*
     * The PDM device is ready to get webcam notifications.
     *
     * @param pInterface  Pointer to the interface.
     * @param fReady      Whether the device is ready.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamDownReady,(PPDMIWEBCAMDOWN pInterface, bool fReady));

    /*
     * Send a control request to the webcam.
     * Async response will be returned by pfnWebcamUpControl callback.
     *
     * @param pInterface  Pointer to the interface.
     * @param pvUser      The callers context.
     * @param u64DeviceId Unique id for the reported webcam assigned by the driver.
     * @param pCtrl       The control data.
     * @param cbCtrl      The size of the control data.
     */
    DECLR3CALLBACKMEMBER(int, pfnWebcamDownControl,(PPDMIWEBCAMDOWN pInterface, void *pvUser, uint64_t u64DeviceId,
                                                    const PDMIWEBCAM_CTRLHDR *pCtrl, uint32_t cbCtrl));
} PDMIWEBCAMDOWN;


#define PDMIWEBCAMUP_IID "6ac03e3c-f56c-4a35-80af-c13ce47a9dd7"
typedef struct PDMIWEBCAMUP *PPDMIWEBCAMUP;
typedef struct PDMIWEBCAMUP
{
    /*
     * A webcam is available.
     *
     * @param pInterface   Pointer to the interface.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     * @param pDeviceDesc  The device description.
     * @param cbDeviceDesc The size of the device description.
     * @param u32Version   The remote video input protocol version.
     * @param fu32Capabilities The remote video input protocol capabilities.
     */
    DECLR3CALLBACKMEMBER(int, pfnWebcamUpAttached,(PPDMIWEBCAMUP pInterface, uint64_t u64DeviceId,
                                                   const PDMIWEBCAM_DEVICEDESC *pDeviceDesc,
                                                   uint32_t cbDeviceDesc, uint32_t u32Version, uint32_t fu32Capabilities));

    /*
     * The webcam is not available anymore.
     *
     * @param pInterface   Pointer to the interface.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpDetached,(PPDMIWEBCAMUP pInterface, uint64_t u64DeviceId));

    /*
     * There is a control response or a control change for the webcam.
     *
     * @param pInterface   Pointer to the interface.
     * @param fResponse    True if this is a response for a previous pfnWebcamDownControl call.
     * @param pvUser       The pvUser parameter of the pfnWebcamDownControl call. Undefined if fResponse == false.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     * @param pCtrl        The control data.
     * @param cbCtrl       The size of the control data.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpControl,(PPDMIWEBCAMUP pInterface, bool fResponse, void *pvUser,
                                                   uint64_t u64DeviceId, const PDMIWEBCAM_CTRLHDR *pCtrl, uint32_t cbCtrl));

    /*
     * A new frame.
     *
     * @param pInterface   Pointer to the interface.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     * @param pHeader      Payload header.
     * @param cbHeader     Size of the payload header.
     * @param pvFrame      Frame (image) data.
     * @param cbFrame      Size of the image data.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpFrame,(PPDMIWEBCAMUP pInterface, uint64_t u64DeviceId, PDMIWEBCAM_FRAMEHDR *pHeader,
                                                 uint32_t cbHeader, const void *pvFrame, uint32_t cbFrame));
} PDMIWEBCAMUP;

#endif
