/** @file
 * VUSB - VirtualBox USB.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_vusb_h
#define ___VBox_vusb_h

#include <VBox/cdefs.h>
#include <VBox/types.h>

__BEGIN_DECLS

/** @defgroup grp_vusb  VBox USB API
 * @{
 */

/** Frequency of USB bus (from spec). */
#define VUSB_BUS_HZ                 12000000

/** @name USB Standard version
 * @{ */
 typedef enum VUSBREVISION
{
    /** Indicates USB 1.1 support. */
    VUSBREVISION_11 = 0x10,
    /** Indicates USB 2.0 support. */
    VUSBREVISION_20 = 0x20
} VUSBREVISION;
/** @} */



/** Pointer to a VBox USB device interface. */
typedef struct VUSBIDEVICE      *PVUSBIDEVICE;

/** Pointer to a VUSB RootHub port interface. */
typedef struct VUSBIROOTHUBPORT *PVUSBIROOTHUBPORT;

/** Pointer to an USB request descriptor. */
typedef struct vusb_urb *PVUSBURB;



/**
 * VBox USB port bitmap.
 *
 * Bit 0 == Port 0, ... , Bit 127 == Port 127.
 */
typedef struct VUSBPORTBITMAP
{
    /** 128 bits */
    char ach[16];
} VUSBPORTBITMAP;
/** Pointer to a VBox USB port bitmap. */
typedef VUSBPORTBITMAP *PVUSBPORTBITMAP;





/**
 * The VUSB RootHub port interface provided by the HCI.
 */
typedef struct VUSBIROOTHUBPORT
{
    /**
     * Get the number of avilable ports in the hub.
     *
     * @returns The number of ports available.
     * @param   pInterface      Pointer to this structure.
     * @param   pAvailable      Bitmap indicating the available ports. Set bit == available port.
     */
    DECLR3CALLBACKMEMBER(unsigned, pfnGetAvailablePorts,(PVUSBIROOTHUBPORT pInterface, PVUSBPORTBITMAP pAvailable));

    /**
     * Get the supported USB revision
     *
     * @returns The supported revision
     * @param   pInterface      Pointer to this structure.
     */
    DECLR3CALLBACKMEMBER(VUSBREVISION, pfnGetRevision, (PVUSBIROOTHUBPORT pInterface));

    /**
     * A device is being attached to a port in the roothub.
     *
     * @param   pInterface      Pointer to this structure.
     * @param   pDev            Pointer to the device being attached.
     * @param   uPort           The port number assigned to the device.
     */
    DECLR3CALLBACKMEMBER(int, pfnAttach,(PVUSBIROOTHUBPORT pInterface, PVUSBIDEVICE pDev, unsigned uPort));

    /**
     * A device is being detached from a port in the roothub.
     *
     * @param   pInterface      Pointer to this structure.
     * @param   pDev            Pointer to the device being detached.
     * @param   uPort           The port number assigned to the device.
     */
    DECLR3CALLBACKMEMBER(void, pfnDetach,(PVUSBIROOTHUBPORT pInterface, PVUSBIDEVICE pDev, unsigned uPort));

    /**
     * Reset the root hub.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this structure.
     * @param   pResetOnLinux   Whether or not to do real reset on linux.
     */
    DECLR3CALLBACKMEMBER(int, pfnReset,(PVUSBIROOTHUBPORT pInterface, bool fResetOnLinux));

    /**
     * Transfer completion callback routine.
     *
     * VUSB will call this when a transfer have been completed
     * in a one or another way.
     *
     * @param   pInterface      Pointer to this structure.
     * @param   pUrb            Pointer to the URB in question.
     */
    DECLR3CALLBACKMEMBER(void, pfnXferCompletion,(PVUSBIROOTHUBPORT pInterface, PVUSBURB urb));

    /**
     * Handle transfer errors.
     *
     * VUSB calls this when a transfer attempt failed. This function will respond
     * indicating wheter to retry or complete the URB with failure.
     *
     * @returns Retry indicator.
     * @param   pInterface      Pointer to this structure.
     * @param   pUrb            Pointer to the URB in question.
     */
    DECLR3CALLBACKMEMBER(bool, pfnXferError,(PVUSBIROOTHUBPORT pInterface, PVUSBURB pUrb));

} VUSBIROOTHUBPORT;


/** Pointer to a VUSB RootHub connector interface. */
typedef struct VUSBIROOTHUBCONNECTOR *PVUSBIROOTHUBCONNECTOR;

/**
 * The VUSB RootHub connector interface provided by the VBox USB RootHub driver.
 */
typedef struct VUSBIROOTHUBCONNECTOR
{
    /**
     * Allocates a new URB for a transfer.
     *
     * Either submit using pfnSubmitUrb or free using VUSBUrbFree().
     *
     * @returns Pointer to a new URB.
     * @returns NULL on failure - try again later.
     *          This will not fail if the device wasn't found. We'll fail it
     *          at submit time, since that makes the usage of this api simpler.
     * @param   pInterface  Pointer to this struct.
     * @param   DstAddress  The destination address of the URB.
     * @param   cbData      The amount of data space required.
     * @param   cTds        The amount of TD space.
     */
    DECLR3CALLBACKMEMBER(PVUSBURB, pfnNewUrb,(PVUSBIROOTHUBCONNECTOR pInterface, uint8_t DstAddress, uint32_t cbData, uint32_t cTds));

    /**
     * Submits a URB for transfer.
     * The transfer will do asynchronously if possible.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to this struct.
     * @param   pUrb        Pointer to the URB returned by pfnNewUrb.
     *                      The URB will be freed in case of failure.
     * @param   pLed        Pointer to USB Status LED
     */
    DECLR3CALLBACKMEMBER(int, pfnSubmitUrb,(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBURB pUrb, PPDMLED pLed));

    /**
     * Call to service asynchronous URB completions in a polling fashion.
     *
     * Reaped URBs will be finished by calling the completion callback,
     * thus there is no return code or input or anything from this function
     * except for potential state changes elsewhere.
     *
     * @returns VINF_SUCCESS if no URBs are pending upon return.
     * @returns VERR_TIMEOUT if one or more URBs are still in flight upon returning.
     * @returns Other VBox status code.
     *
     * @param   pInterface  Pointer to this struct.
     * @param   cMillies    Number of milliseconds to poll for completion.
     */
    DECLR3CALLBACKMEMBER(void, pfnReapAsyncUrbs,(PVUSBIROOTHUBCONNECTOR pInterface, unsigned cMillies));

    /**
     * Cancels and completes - with CRC failure - all in-flight async URBs.
     * This is typically done before saving a state.
     *
     * @param   pInterface  Pointer to this struct.
     */
    DECLR3CALLBACKMEMBER(void, pfnCancelAllUrbs,(PVUSBIROOTHUBCONNECTOR pInterface));

    /**
     * Attach the device to the root hub.
     * The device must not be attached to any hub for this call to succeed.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to this struct.
     * @param   pDevice     Pointer to the device (interface) attach.
     */
    DECLR3CALLBACKMEMBER(int, pfnAttachDevice,(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBIDEVICE pDevice));

    /**
     * Detach the device from the root hub.
     * The device must already be attached for this call to succeed.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to this struct.
     * @param   pDevice     Pointer to the device (interface) to detach.
     */
    DECLR3CALLBACKMEMBER(int, pfnDetachDevice,(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBIDEVICE pDevice));

} VUSBIROOTHUBCONNECTOR;


#ifdef IN_RING3
/** @copydoc VUSBIROOTHUBCONNECTOR::pfnNewUrb */
DECLINLINE(PVUSBURB) VUSBIRhNewUrb(PVUSBIROOTHUBCONNECTOR pInterface, uint32_t DstAddress, uint32_t cbData, uint32_t cTds)
{
    return pInterface->pfnNewUrb(pInterface, DstAddress, cbData, cTds);
}

/** @copydoc VUSBIROOTHUBCONNECTOR::pfnSubmitUrb */
DECLINLINE(int) VUSBIRhSubmitUrb(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBURB pUrb, PPDMLED pLed)
{
    return pInterface->pfnSubmitUrb(pInterface, pUrb, pLed);
}

/** @copydoc VUSBIROOTHUBCONNECTOR::pfnReapAsyncUrbs */
DECLINLINE(void) VUSBIRhReapAsyncUrbs(PVUSBIROOTHUBCONNECTOR pInterface, unsigned cMillies)
{
    pInterface->pfnReapAsyncUrbs(pInterface, cMillies);
}

/** @copydoc VUSBIROOTHUBCONNECTOR::pfnCancelAllUrbs */
DECLINLINE(void) VUSBIRhCancelAllUrbs(PVUSBIROOTHUBCONNECTOR pInterface)
{
    pInterface->pfnCancelAllUrbs(pInterface);
}

/** @copydoc VUSBIROOTHUBCONNECTOR::pfnAttachDevice */
DECLINLINE(int) VUSBIRhAttachDevice(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBIDEVICE pDevice)
{
    return pInterface->pfnAttachDevice(pInterface, pDevice);
}

/** @copydoc VUSBIROOTHUBCONNECTOR::pfnDetachDevice */
DECLINLINE(int) VUSBIRhDetachDevice(PVUSBIROOTHUBCONNECTOR pInterface, PVUSBIDEVICE pDevice)
{
    return pInterface->pfnDetachDevice(pInterface, pDevice);
}
#endif /* IN_RING3 */



/** Pointer to a Root Hub Configuration Interface. */
typedef struct VUSBIRHCONFIG *PVUSBIRHCONFIG;

/**
 * Root Hub Configuration Interface (intended for MAIN).
 */
typedef struct VUSBIRHCONFIG
{
    /**
     * Creates a USB proxy device and attaches it to the root hub.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the root hub configuration interface structure.
     * @param   pUuid           Pointer to the UUID for the new device.
     * @param   fRemote         Whether the device must use the VRDP backend.
     * @param   pszAddress      OS specific device address.
     * @param   pvBackend       An opaque pointer for the backend. Only used by
     *                          the VRDP backend so far.
     */
    DECLR3CALLBACKMEMBER(int, pfnCreateProxyDevice,(PVUSBIRHCONFIG pInterface, PCRTUUID pUuid, bool fRemote, const char *pszAddress, void *pvBackend));

    /**
     * Removes a USB proxy device from the root hub and destroys it.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the root hub configuration interface structure.
     * @param   pUuid           Pointer to the UUID for the device.
     */
    DECLR3CALLBACKMEMBER(int, pfnDestroyProxyDevice,(PVUSBIRHCONFIG pInterface, PCRTUUID pUuid));

} VUSBIRHCONFIG;

#ifdef IN_RING3
/** @copydoc  VUSBIRHCONFIG::pfnCreateProxyDevice */
DECLINLINE(int) VUSBIRhCreateProxyDevice(PVUSBIRHCONFIG pInterface, PCRTUUID pUuid, bool fRemote, const char *pszAddress, void *pvBackend)
{
    return pInterface->pfnCreateProxyDevice(pInterface, pUuid, fRemote, pszAddress, pvBackend);
}

/** @copydoc VUSBIRHCONFIG::pfnDestroyProxyDevice */
DECLINLINE(int) VUSBIRhDestroyProxyDevice(PVUSBIRHCONFIG pInterface, PCRTUUID pUuid)
{
    return pInterface->pfnDestroyProxyDevice(pInterface, pUuid);
}
#endif /* IN_RING3 */



/**
 * VUSB device reset completion callback function.
 * This is called by the reset thread when the reset has been completed.
 *
 * @param   pDev        Pointer to the virtual USB device core.
 * @param   rc      The VBox status code of the reset operation.
 * @param   pvUser      User specific argument.
 *
 * @thread  The reset thread or EMT.
 */
typedef DECLCALLBACK(void) FNVUSBRESETDONE(PVUSBIDEVICE pDevice, int rc, void *pvUser);
/** Pointer to a device reset completion callback function (FNUSBRESETDONE). */
typedef FNVUSBRESETDONE *PFNVUSBRESETDONE;

/**
 * The state of a VUSB Device.
 *
 * @remark  The order of these states is vital.
 */
typedef enum VUSBDEVICESTATE
{
    VUSB_DEVICE_STATE_INVALID = 0,
    VUSB_DEVICE_STATE_DETACHED,
    VUSB_DEVICE_STATE_ATTACHED,
    VUSB_DEVICE_STATE_POWERED,
    VUSB_DEVICE_STATE_DEFAULT,
    VUSB_DEVICE_STATE_ADDRESS,
    VUSB_DEVICE_STATE_CONFIGURED,
    VUSB_DEVICE_STATE_SUSPENDED,
    /** The device is being reset. Don't mess with it.
     * Next states: VUSB_DEVICE_STATE_DEFAULT, VUSB_DEVICE_STATE_RESET_DESTROY
     */
    VUSB_DEVICE_STATE_RESET,
    /** The device is being reset and should be destroyed. Don't mess with it.
     * Prev state: VUSB_DEVICE_STATE_RESET
     * Next state: VUSB_DEVICE_STATE_DESTROY
     */
    VUSB_DEVICE_STATE_RESET_DESTROY,
    /** The device is being destroyed.
     * Prev state: Any but VUSB_DEVICE_STATE_RESET
     * Next state: VUSB_DEVICE_STATE_DESTROYED
     */
    VUSB_DEVICE_STATE_DESTROY,
    /** The device has been destroy. */
    VUSB_DEVICE_STATE_DESTROYED,
    /** The usual 32-bit hack. */
    VUSB_DEVICE_STATE_32BIT_HACK = 0x7fffffff
} VUSBDEVICESTATE;


/**
 * USB Device Interface.
 */
typedef struct VUSBIDEVICE
{
    /**
     * Resets the device.
     *
     * Since a device reset shall take at least 10ms from the guest point of view,
     * it must be performed asynchronously. We create a thread which performs this
     * operation and ensures it will take at least 10ms.
     *
     * At times - like init - a synchronous reset is required, this can be done
     * by passing NULL for pfnDone.
     *
     * -- internal stuff, move it --
     * While the device is being reset it is in the VUSB_DEVICE_STATE_RESET state.
     * On completion it will be in the VUSB_DEVICE_STATE_DEFAULT state if successful,
     * or in the VUSB_DEVICE_STATE_DETACHED state if the rest failed.
     * -- internal stuff, move it --
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this structure.
     * @param   fResetOnLinux   Set if we can permit a real reset and a potential logical
     *                          device reconnect on linux hosts.
     * @param   pfnDone         Pointer to the completion routine. If NULL a synchronous
     *                          reset  is preformed not respecting the 10ms.
     * @param   pvUser          User argument to the completion routine.
     * @param   pVM             Pointer to the VM handle if callback in EMT is required. (optional)
     */
    DECLR3CALLBACKMEMBER(int, pfnReset,(PVUSBIDEVICE pInterface, bool fResetOnLinux,
                                        PFNVUSBRESETDONE pfnDone, void *pvUser, PVM pVM));

    /**
     * Powers on the device.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the device interface structure.
     */
    DECLR3CALLBACKMEMBER(int, pfnPowerOn,(PVUSBIDEVICE pInterface));

    /**
     * Powers off the device.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the device interface structure.
     */
    DECLR3CALLBACKMEMBER(int, pfnPowerOff,(PVUSBIDEVICE pInterface));

    /**
     * Get the state of the device.
     *
     * @returns Device state.
     * @param   pInterface      Pointer to the device interface structure.
     */
    DECLR3CALLBACKMEMBER(VUSBDEVICESTATE, pfnGetState,(PVUSBIDEVICE pInterface));

} VUSBIDEVICE;


#ifdef IN_RING3
/**
 * Resets the device.
 *
 * Since a device reset shall take at least 10ms from the guest point of view,
 * it must be performed asynchronously. We create a thread which performs this
 * operation and ensures it will take at least 10ms.
 *
 * At times - like init - a synchronous reset is required, this can be done
 * by passing NULL for pfnDone.
 *
 * -- internal stuff, move it --
 * While the device is being reset it is in the VUSB_DEVICE_STATE_RESET state.
 * On completion it will be in the VUSB_DEVICE_STATE_DEFAULT state if successful,
 * or in the VUSB_DEVICE_STATE_DETACHED state if the rest failed.
 * -- internal stuff, move it --
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the device interface structure.
 * @param   fResetOnLinux   Set if we can permit a real reset and a potential logical
 *                          device reconnect on linux hosts.
 * @param   pfnDone         Pointer to the completion routine. If NULL a synchronous
 *                          reset  is preformed not respecting the 10ms.
 * @param   pvUser          User argument to the completion routine.
 * @param   pVM             Pointer to the VM handle if callback in EMT is required. (optional)
 */
DECLINLINE(int) VUSBIDevReset(PVUSBIDEVICE pInterface, bool fResetOnLinux, PFNVUSBRESETDONE pfnDone, void *pvUser, PVM pVM)
{
    return pInterface->pfnReset(pInterface, fResetOnLinux, pfnDone, pvUser, pVM);
}

/**
 * Powers on the device.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLINLINE(int) VUSBIDevPowerOn(PVUSBIDEVICE pInterface)
{
    return pInterface->pfnPowerOn(pInterface);
}

/**
 * Powers off the device.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLINLINE(int) VUSBIDevPowerOff(PVUSBIDEVICE pInterface)
{
    return pInterface->pfnPowerOff(pInterface);
}

/**
 * Get the state of the device.
 *
 * @returns Device state.
 * @param   pInterface      Pointer to the device interface structure.
 */
DECLINLINE(VUSBDEVICESTATE) VUSBIDevGetState(PVUSBIDEVICE pInterface)
{
    return pInterface->pfnGetState(pInterface);
}
#endif /* IN_RING3 */


/** @name URB
 * @{ */

/**
 * VUSB Transfer status codes.
 */
typedef enum VUSBSTATUS
{
    /** Transer was ok. */
    VUSBSTATUS_OK = 0,
    /** Transfer stalled, endpoint halted. */
    VUSBSTATUS_STALL,
    /** Device not responding. */
    VUSBSTATUS_DNR,
    /** CRC error. */
    VUSBSTATUS_CRC,
    /** Data overrun error. */
    VUSBSTATUS_DATA_UNDERRUN,
    /** Data overrun error. */
    VUSBSTATUS_DATA_OVERRUN,
    /** The isochronous buffer hasn't been touched. */
    VUSBSTATUS_NOT_ACCESSED,
    /** Invalid status. */
    VUSBSTATUS_INVALID = 0x7f
} VUSBSTATUS;


/**
 * VUSB Transfer types.
 */
typedef enum VUSBXFERTYPE
{
    /** Control message. Used to represent a single control transfer. */
    VUSBXFERTYPE_CTRL = 0,
    /* Isochronous transfer. */
    VUSBXFERTYPE_ISOC,
    /** Bulk transfer. */
    VUSBXFERTYPE_BULK,
    /** Interrupt transfer. */
    VUSBXFERTYPE_INTR,
    /** Complete control message. Used to represent an entire control message. */
    VUSBXFERTYPE_MSG,
    /** Invalid transfer type. */
    VUSBXFERTYPE_INVALID = 0x7f
} VUSBXFERTYPE;


/**
 * VUSB transfer direction.
 */
typedef enum VUSBDIRECTION
{
    /** Setup */
    VUSBDIRECTION_SETUP = 0,
#define VUSB_DIRECTION_SETUP    VUSBDIRECTION_SETUP
    /** In - Device to host. */
    VUSBDIRECTION_IN = 1,
#define VUSB_DIRECTION_IN       VUSBDIRECTION_IN
    /** Out - Host to device. */
    VUSBDIRECTION_OUT = 2,
#define VUSB_DIRECTION_OUT  VUSBDIRECTION_OUT
    /** Invalid direction */
    VUSBDIRECTION_INVALID = 0x7f
} VUSBDIRECTION;

/**
 * The URB states
 */
typedef enum VUSBURBSTATE
{
    /** The usual invalid state. */
    VUSBURBSTATE_INVALID = 0,
    /** The URB is free, i.e. not in use.
     * Next state: ALLOCATED */
    VUSBURBSTATE_FREE,
    /** The URB is allocated, i.e. being prepared for submission.
     * Next state: FREE, IN_FLIGHT */
    VUSBURBSTATE_ALLOCATED,
    /** The URB is in flight.
     * Next state: REAPED, CANCELLED */
    VUSBURBSTATE_IN_FLIGHT,
    /** The URB has been reaped and is being completed.
     * Next state: FREE */
    VUSBURBSTATE_REAPED,
    /** The URB has been cancelled and is awaiting reaping and immediate freeing.
     * Next state: FREE */
    VUSBURBSTATE_CANCELLED,
    /** The end of the valid states (exclusive). */
    VUSBURBSTATE_END,
    /** The usual 32-bit blow up. */
    VUSBURBSTATE_32BIT_HACK = 0x7fffffff
} VUSBURBSTATE;


/**
 * Information about a isochronous packet.
 */
typedef struct VUSBURBISOCPKT
{
    /** The size of the packet.
     * IN: The packet size. I.e. the number of bytes to the next packet or end of buffer.
     * OUT: The actual size transfered. */
    uint16_t        cb;
    /** The offset of the packet. (Relative to VUSBURB::abData[0].)
     * OUT: This can be changed by the USB device if it does some kind of buffer squeezing. */
    uint16_t        off;
    /** The status of the transfer.
     * IN: VUSBSTATUS_INVALID
     * OUT: VUSBSTATUS_INVALID if nothing was done, otherwise the correct status. */
    VUSBSTATUS      enmStatus;
} VUSBURBISOCPKT;
/** Pointer to a isochronous packet. */
typedef VUSBURBISOCPKT *PVUSBURBISOCPTK;
/** Pointer to a const isochronous packet. */
typedef const VUSBURBISOCPKT *PCVUSBURBISOCPKT;

/**
 * Asynchronous USB request descriptor
 */
typedef struct vusb_urb
{
    /** URB magic value. */
    uint32_t        u32Magic;
    /** The USR state. */
    VUSBURBSTATE    enmState;
    /** URB description, can be null. intended for logging. */
    char           *pszDesc;

    /** The VUSB data. */
    struct VUSBURBVUSB
    {
        /** URB chain pointer. */
        PVUSBURB        pNext;
        /** URB chain pointer. */
        PVUSBURB       *ppPrev;
        /** Pointer to the original for control messages. */
        PVUSBURB        pCtrlUrb;
        /** Sepcific to the pfnFree function. */
        void           *pvFreeCtx;
        /**
         * Callback which will free the URB once it's reaped and completed.
         * @param   pUrb    The URB.
         */
        DECLCALLBACKMEMBER(void, pfnFree)(PVUSBURB pUrb);
        /** Submit timestamp. (logging only) */
        uint64_t        u64SubmitTS;
        /** The allocated data length. */
        uint32_t        cbDataAllocated;
        /** The allocated TD length. */
        uint32_t        cTdsAllocated;
    } VUsb;

    /** The host controller data. */
    struct VUSBURBHCI
    {
        /** The endpoint descriptor address. */
        uint32_t        EdAddr;
        /** Number of Tds in the array. */
        uint32_t        cTds;
        /** Pointer to an array of TD info items.*/
        struct VUSBURBHCITD
        {
            /** Type of TD (private) */
            uint32_t        TdType;
            /** The address of the */
            uint32_t        TdAddr;
            /** A copy of the TD. */
            uint32_t        TdCopy[16];
        }              *paTds;
        /** URB chain pointer. */
        PVUSBURB        pNext;
        /** When this URB was created.
         * (Used for isochronous frames and for logging.) */
        uint32_t        u32FrameNo;
        /** Flag indicating that the TDs have been unlinked. */
        bool            fUnlinked;
    } Hci;

    /** The device data. */
    struct VUSBURBDEV
    {
        /** Pointer to the proxy URB.  */
        void           *pvProxyUrb;
    } Dev;

    /** The device - NULL until a submit has been is attempted.
     * This is set when allocating the URB. */
    struct vusb_dev *pDev;
    /** The device address.
     * This is set at allocation time. */
    uint8_t         DstAddress;

    /** The endpoint.
     * IN: Must be set before submitting the URB. */
    uint8_t         EndPt;
    /** The transfer type.
     * IN: Must be set before submitting the URB. */
    VUSBXFERTYPE    enmType;
    /** The transfer direction.
     * IN: Must be set before submitting the URB. */
    VUSBDIRECTION   enmDir;
    /** Indicates whether it is OK to receive/send less data than requested.
     * IN: Must be initialized before submitting the URB. */
    bool            fShortNotOk;
    /** The transfer status.
     * OUT: This is set when reaping the URB. */
    VUSBSTATUS      enmStatus;

    /** The number of isochronous packets describe in aIsocPkts.
     * This is ignored when enmType isn't VUSBXFERTYPE_ISOC. */
    uint32_t        cIsocPkts;
    /** The iso packets within abData.
     * This is ignored when enmType isn't VUSBXFERTYPE_ISOC. */
    VUSBURBISOCPKT  aIsocPkts[8];

    /** The message length.
     * IN: The amount of data to send / receive - set at allocation time.
     * OUT: The amount of data sent / received. */
    uint32_t        cbData;
    /** The message data.
     * IN: On host to device transfers, the data to send.
     * OUT: On device to host transfers, the data to received. */
    uint8_t         abData[8*_1K];
} VUSBURB;

/** The magic value of a valid VUSBURB. (Murakami Haruki) */
#define VUSBURB_MAGIC   0x19490112

/** @} */


/** @} */

__END_DECLS

#endif
