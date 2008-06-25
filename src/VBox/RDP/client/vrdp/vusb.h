/** @file
 *
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

/** Pointer to a VBox USB device interface. */
typedef struct VUSBIDEVICE      *PVUSBIDEVICE;

/** Pointer to a VUSB RootHub port interface. */
typedef struct VUSBIROOTHUBPORT *PVUSBIROOTHUBPORT;

/** Pointer to an USB request descriptor. */
typedef struct VUSBURB          *PVUSBURB;


/** @name URB
 * @{ */

/**
 * VUSB Transfer status codes.
 */
typedef enum VUSBSTATUS
{
    /** Transer was ok. */
    VUSBSTATUS_OK = 0,
#define VUSB_XFER_OK    VUSBSTATUS_OK
    /** Transfer stalled, endpoint halted. */
    VUSBSTATUS_STALL,
#define VUSB_XFER_STALL VUSBSTATUS_STALL
    /** Device not responding. */
    VUSBSTATUS_DNR,
#define VUSB_XFER_DNR   VUSBSTATUS_DNR
    /** CRC error. */
    VUSBSTATUS_CRC,
#define VUSB_XFER_CRC	VUSBSTATUS_CRC
    /** Underflow error. */
    VUSBSTATUS_UNDERFLOW,
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
#define VUSB_TRANSFER_TYPE_CTRL	VUSBXFERTYPE_CTRL
    /* Isochronous transfer. */
    VUSBXFERTYPE_ISOC,
#define VUSB_TRANSFER_TYPE_ISOC	VUSBXFERTYPE_ISOC
    /** Bulk transfer. */
    VUSBXFERTYPE_BULK,
#define VUSB_TRANSFER_TYPE_BULK	VUSBXFERTYPE_BULK
    /** Interrupt transfer. */
    VUSBXFERTYPE_INTR,
#define VUSB_TRANSFER_TYPE_INTR	VUSBXFERTYPE_INTR
    /** Complete control message. Used to represent an entire control message. */
    VUSBXFERTYPE_MSG,
#define VUSB_TRANSFER_TYPE_MSG	VUSBXFERTYPE_MSG
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
#define VUSB_DIRECTION_OUT	VUSBDIRECTION_OUT
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
 * Asynchronous USB request descriptor
 */
typedef struct VUSBURB
{
    /** URB magic value. */
    uint32_t        u32Magic;
    /** The USR state. */
    VUSBURBSTATE    enmState;

    /* Private fields not accessed by the backend. */
    PVUSBURB        next;
    PVUSBURB        prev;
    uint32_t        handle;

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
            /** The address of the */
            uint32_t        TdAddr;
            /** A copy of the TD. */
            uint32_t        TdCopy[4];
        }              *paTds;
        /** URB chain pointer. */
        PVUSBURB        pNext;
        /** When this URB was created. (logging only) */
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

    /** The device - can be NULL untill submit is attempted.
     * This is set when allocating the URB. */
    struct VUSBDEV *pDev;
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
