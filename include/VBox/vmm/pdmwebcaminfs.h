/* $Id$ */

/** @file
 * webcaminfs - interfaces between dev and driver.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */

#ifndef ___VBox_vmm_pdmwebcaminfs_h
#define ___VBox_vmm_pdmwebcaminfs_h


typedef struct PDMIWEBCAM_DEVICEDESC PDMIWEBCAM_DEVICEDESC;
typedef struct PDMIWEBCAM_CTRLHDR PDMIWEBCAM_CTRLHDR;


#define PDMIWEBCAMDOWN_IID "0846959f-8b4a-4b2c-be78-9903b2ae9de5"
typedef struct PDMIWEBCAMDOWN *PPDMIWEBCAMDOWN;
typedef struct PDMIWEBCAMDOWN
{
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
    DECLR3CALLBACKMEMBER(int, pfnWebcamDownControl, (PPDMIWEBCAMDOWN pInterface,
                                                     void *pvUser,
                                                     uint64_t u64DeviceId,
                                                     const PDMIWEBCAM_CTRLHDR *pCtrl,
                                                     uint32_t cbCtrl));
} PDMIWEBCAMDOWN;


#define PDMIWEBCAMUP_IID "7921e96b-b8e2-4173-a73d-787620fc3cab"
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
     */
    DECLR3CALLBACKMEMBER(int, pfnWebcamUpAttached,(PPDMIWEBCAMUP pInterface,
                                                   uint64_t u64DeviceId,
                                                   const PDMIWEBCAM_DEVICEDESC *pDeviceDesc,
                                                   uint32_t cbDeviceDesc));

    /*
     * The webcam is not available anymore.
     *
     * @param pInterface   Pointer to the interface.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpDetached,(PPDMIWEBCAMUP pInterface,
                                                    uint64_t u64DeviceId));

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
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpControl,(PPDMIWEBCAMUP pInterface,
                                                   bool fResponse,
                                                   void *pvUser,
                                                   uint64_t u64DeviceId,
                                                   const PDMIWEBCAM_CTRLHDR *pCtrl,
                                                   uint32_t cbCtrl));

    /*
     * A new frame.
     *
     * @param pInterface   Pointer to the interface.
     * @param u64DeviceId  Unique id for the reported webcam assigned by the driver.
     * @param pu8Frame     The frame data including the payload header and the image data.
     * @param cbFrame      The size of the frame data.
     */
    DECLR3CALLBACKMEMBER(void, pfnWebcamUpFrame,(PPDMIWEBCAMUP pInterface,
                                                 uint64_t u64DeviceId,
                                                 const uint8_t *pu8Frame,
                                                 uint32_t cbFrame));
} PDMIWEBCAMUP;

#endif
