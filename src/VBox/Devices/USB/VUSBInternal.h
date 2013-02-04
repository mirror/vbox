/* $Id$ */
/** @file
 * Virtual USB - Internal header.
 *
 * This subsystem implements USB devices in a host controller independent
 * way.  All the host controller code has to do is use VUSBHUB for its
 * root hub implementation and any emulated USB device may be plugged into
 * the virtual bus.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VUSBInternal_h
#define ___VUSBInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vusb.h>
#include <VBox/vmm/stam.h>
#include <iprt/assert.h>

RT_C_DECLS_BEGIN


/** @name Internal Device Operations, Structures and Constants.
 * @{
 */

/** Pointer to a Virtual USB device (core). */
typedef struct VUSBDEV *PVUSBDEV;
/** Pointer to a VUSB hub device. */
typedef struct VUSBHUB *PVUSBHUB;
/** Pointer to a VUSB root hub. */
typedef struct VUSBROOTHUB *PVUSBROOTHUB;


/** Number of the default control endpoint */
#define VUSB_PIPE_DEFAULT           0

/** @name Device addresses
 * @{ */
#define VUSB_DEFAULT_ADDRESS        0
#define VUSB_INVALID_ADDRESS        UINT8_C(0xff)
/** @} */

/** @name Feature bits (1<<FEATURE for the u16Status bit)
 * @{ */
#define VUSB_DEV_SELF_POWERED       0
#define VUSB_DEV_REMOTE_WAKEUP      1
#define VUSB_EP_HALT                0
/** @} */

/** Maximum number of endpoint addresses */
#define VUSB_PIPE_MAX           16

/**
 * Control-pipe stages.
 */
typedef enum CTLSTAGE
{
    /** the control pipe is in the setup stage. */
    CTLSTAGE_SETUP = 0,
    /** the control pipe is in the data stage. */
    CTLSTAGE_DATA,
    /** the control pipe is in the status stage. */
    CTLSTAGE_STATUS
} CTLSTAGE;

/**
 * Extra data for a control pipe.
 *
 * This is state information needed for the special multi-stage
 * transfers performed on this kind of pipes.
 */
typedef struct vusb_ctrl_extra
{
    /** Current pipe stage. */
    CTLSTAGE            enmStage;
    /** Success indicator. */
    bool                fOk;
    /** Set if the message URB has been submitted. */
    bool                fSubmitted;
    /** Pointer to the SETUP.
     * This is a pointer to Urb->abData[0]. */
    PVUSBSETUP          pMsg;
    /** Current DATA pointer.
     * This starts at pMsg + 1 and is incremented at we read/write data. */
    uint8_t            *pbCur;
    /** The amount of data left to read on IN operations.
     * On OUT operations this is not used. */
    uint32_t            cbLeft;
    /** The amount of data we can house.
     * This starts at the default 8KB, and this structure will be reallocated to
     * accommodate any larger request (unlikely). */
    uint32_t            cbMax;
    /** The message URB. */
    VUSBURB             Urb;
} VUSBCTRLEXTRA, *PVUSBCTRLEXTRA;

void vusbMsgFreeExtraData(PVUSBCTRLEXTRA pExtra);
void vusbMsgResetExtraData(PVUSBCTRLEXTRA pExtra);


/**
 * A VUSB pipe
 */
typedef struct vusb_pipe
{
    PCVUSBDESCENDPOINTEX in;
    PCVUSBDESCENDPOINTEX out;
    /** Pointer to the extra state data required to run a control pipe. */
    PVUSBCTRLEXTRA      pCtrl;
    /** Count of active async transfers. */
    uint8_t             async;
    /** The periodic read-ahead buffer thread. */
    RTTHREAD            ReadAheadThread;
    /** Pointer to the reset thread arguments. */
    void               *pvReadAheadArgs;
    /** Pointer to the first buffered URB. */
    PVUSBURB            pBuffUrbHead;
    /** Pointer to the last buffered URB. */
    PVUSBURB            pBuffUrbTail;
    /** Count of URBs in read-ahead buffer. */
    uint32_t            cBuffered;
    /** Count of URBs submitted for read-ahead but not yet reaped. */
    uint32_t            cSubmitted;
} VUSBPIPE;
/** Pointer to a VUSB pipe structure. */
typedef VUSBPIPE *PVUSBPIPE;


/**
 * Interface state and possible settings.
 */
typedef struct vusb_interface_state
{
    /** Pointer to the interface descriptor of the currently selected (active)
     * interface. */
    PCVUSBDESCINTERFACEEX   pCurIfDesc;
    /** Pointer to the interface settings. */
    PCVUSBINTERFACE         pIf;
} VUSBINTERFACESTATE;
/** Pointer to interface state. */
typedef VUSBINTERFACESTATE *PVUSBINTERFACESTATE;
/** Pointer to const interface state. */
typedef const VUSBINTERFACESTATE *PCVUSBINTERFACESTATE;


/**
 * A Virtual USB device (core).
 *
 * @implements  VUSBIDEVICE
 */
typedef struct VUSBDEV
{
    /** The device interface exposed to the HCI. */
    VUSBIDEVICE         IDevice;
    /** Pointer to the PDM USB device instance. */
    PPDMUSBINS          pUsbIns;
    /** Next device in the chain maintained by the roothub. */
    PVUSBDEV            pNext;
    /** Pointer to the next device with the same address hash. */
    PVUSBDEV            pNextHash;
    /** Pointer to the hub this device is attached to. */
    PVUSBHUB            pHub;
    /** The device state.
     * Only EMT changes this value. */
    VUSBDEVICESTATE volatile enmState;

    /** The device address. */
    uint8_t             u8Address;
    /** The new device address. */
    uint8_t             u8NewAddress;
    /** The port. */
    int16_t             i16Port;
    /** Device status.  (VUSB_DEV_SELF_POWERED or not.)  */
    uint16_t            u16Status;

    /** Pointer to the descriptor cache.
     * (Provided by the device thru the pfnGetDescriptorCache method.) */
    PCPDMUSBDESCCACHE   pDescCache;
    /** Current configuration. */
    PCVUSBDESCCONFIGEX  pCurCfgDesc;

    /** Current interface state (including alternate interface setting) - maximum
     * valid index is config->bNumInterfaces
     */
    PVUSBINTERFACESTATE paIfStates;

    /** Pipe/direction -> endpoint descriptor mapping */
    VUSBPIPE            aPipes[VUSB_PIPE_MAX];

    /** Dumper state. */
    union VUSBDEVURBDUMPERSTATE
    {
        /** The current scsi command. */
        uint8_t             u8ScsiCmd;
    } Urb;

    /** The reset thread. */
    RTTHREAD            hResetThread;
    /** Pointer to the reset thread arguments. */
    void               *pvResetArgs;
    /** The reset timer handle. */
    PTMTIMER            pResetTimer;
} VUSBDEV;



/** Pointer to the virtual method table for a kind of USB devices. */
typedef struct vusb_dev_ops *PVUSBDEVOPS;

/** Pointer to the const virtual method table for a kind of USB devices. */
typedef const struct vusb_dev_ops *PCVUSBDEVOPS;

/**
 * Virtual method table for USB devices - these are the functions you need to
 * implement when writing a new device (or hub)
 *
 * Note that when creating your structure, you are required to zero the
 * vusb_dev fields (ie. use calloc).
 */
typedef struct vusb_dev_ops
{
    /* mandatory */
    const char *name;
} VUSBDEVOPS;


int vusbDevInit(PVUSBDEV pDev, PPDMUSBINS pUsbIns);
int vusbDevCreateOld(const char *pszDeviceName, void *pvDriverInit, PCRTUUID pUuid, PVUSBDEV *ppDev);
void vusbDevDestroy(PVUSBDEV pDev);

DECLINLINE(bool) vusbDevIsRh(PVUSBDEV pDev)
{
    return (pDev->pHub == (PVUSBHUB)pDev);
}

bool vusbDevDoSelectConfig(PVUSBDEV dev, PCVUSBDESCCONFIGEX pCfg);
void vusbDevMapEndpoint(PVUSBDEV dev, PCVUSBDESCENDPOINTEX ep);
int vusbDevDetach(PVUSBDEV pDev);
DECLINLINE(PVUSBROOTHUB) vusbDevGetRh(PVUSBDEV pDev);
size_t vusbDevMaxInterfaces(PVUSBDEV dev);

DECLCALLBACK(int) vusbDevReset(PVUSBIDEVICE pDevice, bool fResetOnLinux, PFNVUSBRESETDONE pfnDone, void *pvUser, PVM pVM);
DECLCALLBACK(int) vusbDevPowerOn(PVUSBIDEVICE pInterface);
DECLCALLBACK(int) vusbDevPowerOff(PVUSBIDEVICE pInterface);
DECLCALLBACK(VUSBDEVICESTATE) vusbDevGetState(PVUSBIDEVICE pInterface);
void vusbDevSetAddress(PVUSBDEV pDev, uint8_t u8Address);
bool vusbDevStandardRequest(PVUSBDEV pDev, int EndPt, PVUSBSETUP pSetup, void *pvBuf, uint32_t *pcbBuf);


/** @} */





/** @name Internal Hub Operations, Structures and Constants.
 * @{
 */


/** Virtual method table for USB hub devices.
 * Hub and roothub drivers need to implement these functions in addition to the
 * vusb_dev_ops.
 */
typedef struct VUSBHUBOPS
{
    int     (*pfnAttach)(PVUSBHUB pHub, PVUSBDEV pDev);
    void    (*pfnDetach)(PVUSBHUB pHub, PVUSBDEV pDev);
} VUSBHUBOPS;
/** Pointer to a const HUB method table. */
typedef const VUSBHUBOPS *PCVUSBHUBOPS;

/** A VUSB Hub Device - Hub and roothub drivers need to use this struct
 * @todo eliminate this (PDM  / roothubs only).
 */
typedef struct VUSBHUB
{
    VUSBDEV             Dev;
    PCVUSBHUBOPS        pOps;
    PVUSBROOTHUB        pRootHub;
    uint16_t            cPorts;
    uint16_t            cDevices;
    /** Name of the hub. Used for logging. */
    char               *pszName;
} VUSBHUB;

/** @} */


/** @name Internal Root Hub Operations, Structures and Constants.
 * @{
 */

/**
 * Per transfer type statistics.
 */
typedef struct VUSBROOTHUBTYPESTATS
{
    STAMCOUNTER         StatUrbsSubmitted;
    STAMCOUNTER         StatUrbsFailed;
    STAMCOUNTER         StatUrbsCancelled;

    STAMCOUNTER         StatReqBytes;
    STAMCOUNTER         StatReqReadBytes;
    STAMCOUNTER         StatReqWriteBytes;

    STAMCOUNTER         StatActBytes;
    STAMCOUNTER         StatActReadBytes;
    STAMCOUNTER         StatActWriteBytes;
} VUSBROOTHUBTYPESTATS, *PVUSBROOTHUBTYPESTATS;



/** The address hash table size. */
#define VUSB_ADDR_HASHSZ    5

/**
 * The instance data of a root hub driver.
 *
 * This extends the generic VUSB hub.
 *
 * @implements  VUSBIROOTHUBCONNECTOR
 */
typedef struct VUSBROOTHUB
{
    /** The HUB.
     * @todo remove this? */
    VUSBHUB                 Hub;
#if HC_ARCH_BITS == 32
    uint32_t                Alignment0;
#endif
    /** Address hash table. */
    PVUSBDEV                apAddrHash[VUSB_ADDR_HASHSZ];
    /** List of async URBs. */
    PVUSBURB                pAsyncUrbHead;
    /** The default address. */
    PVUSBDEV                pDefaultAddress;

    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to the root hub port interface we're attached to. */
    PVUSBIROOTHUBPORT       pIRhPort;
    /** Connector interface exposed upwards. */
    VUSBIROOTHUBCONNECTOR   IRhConnector;

    /** Chain of devices attached to this hub. */
    PVUSBDEV                pDevices;
    /** Availability Bitmap. */
    VUSBPORTBITMAP          Bitmap;

    /** Critical section protecting the free list. */
    RTCRITSECT              CritSect;
    /** Chain of free URBs. (Singly linked) */
    PVUSBURB                pFreeUrbs;
    /** The number of URBs in the pool. */
    uint32_t                cUrbsInPool;
    /** Version of the attached Host Controller. */
    uint32_t                fHcVersions;
#ifdef VBOX_WITH_STATISTICS
#if HC_ARCH_BITS == 32
    uint32_t                Alignment1; /**< Counters must be 64-bit aligned. */
#endif
    VUSBROOTHUBTYPESTATS    Total;
    VUSBROOTHUBTYPESTATS    aTypes[VUSBXFERTYPE_MSG];
    STAMCOUNTER             StatIsocReqPkts;
    STAMCOUNTER             StatIsocReqReadPkts;
    STAMCOUNTER             StatIsocReqWritePkts;
    STAMCOUNTER             StatIsocActPkts;
    STAMCOUNTER             StatIsocActReadPkts;
    STAMCOUNTER             StatIsocActWritePkts;
    struct
    {
        STAMCOUNTER         Pkts;
        STAMCOUNTER         Ok;
        STAMCOUNTER         Ok0;
        STAMCOUNTER         DataUnderrun;
        STAMCOUNTER         DataUnderrun0;
        STAMCOUNTER         DataOverrun;
        STAMCOUNTER         NotAccessed;
        STAMCOUNTER         Misc;
        STAMCOUNTER         Bytes;
    }                       aStatIsocDetails[8];

    STAMPROFILE             StatReapAsyncUrbs;
    STAMPROFILE             StatSubmitUrb;
#endif
} VUSBROOTHUB;
AssertCompileMemberAlignment(VUSBROOTHUB, IRhConnector, 8);
AssertCompileMemberAlignment(VUSBROOTHUB, Bitmap, 8);
AssertCompileMemberAlignment(VUSBROOTHUB, CritSect, 8);
#ifdef VBOX_WITH_STATISTICS
AssertCompileMemberAlignment(VUSBROOTHUB, Total, 8);
#endif

/** Converts a pointer to VUSBROOTHUB::IRhConnector to a PVUSBROOTHUB. */
#define VUSBIROOTHUBCONNECTOR_2_VUSBROOTHUB(pInterface) (PVUSBROOTHUB)( (uintptr_t)(pInterface) - RT_OFFSETOF(VUSBROOTHUB, IRhConnector) )

/**
 * URB cancellation modes
 */
typedef enum CANCELMODE
{
    /** complete the URB with an error (CRC). */
    CANCELMODE_FAIL = 0,
    /** do not change the URB contents. */
    CANCELMODE_UNDO
} CANCELMODE;

/* @} */



/** @name Internal URB Operations, Structures and Constants.
 * @{ */
int  vusbUrbSubmit(PVUSBURB pUrb);
void vusbUrbTrace(PVUSBURB pUrb, const char *pszMsg, bool fComplete);
void vusbUrbDoReapAsync(PVUSBURB pHead, RTMSINTERVAL cMillies);
void vusbUrbCancel(PVUSBURB pUrb, CANCELMODE mode);
void vusbUrbRipe(PVUSBURB pUrb);
void vusbUrbCompletionRh(PVUSBURB pUrb);

void vusbUrbCompletionReadAhead(PVUSBURB pUrb);
void vusbReadAheadStart(PVUSBDEV pDev, PVUSBPIPE pPipe);
void vusbReadAheadStop(void *pvReadAheadArgs);
int  vusbUrbQueueAsyncRh(PVUSBURB pUrb);
int  vusbUrbSubmitBufferedRead(PVUSBURB pUrb, PVUSBPIPE pPipe);
PVUSBURB vusbRhNewUrb(PVUSBROOTHUB pRh, uint8_t DstAddress, uint32_t cbData, uint32_t cTds);


DECLINLINE(void) vusbUrbUnlink(PVUSBURB pUrb)
{
    *pUrb->VUsb.ppPrev = pUrb->VUsb.pNext;
    if (pUrb->VUsb.pNext)
        pUrb->VUsb.pNext->VUsb.ppPrev = pUrb->VUsb.ppPrev;
    pUrb->VUsb.pNext = NULL;
    pUrb->VUsb.ppPrev = NULL;
}

/** @def vusbUrbAssert
 * Asserts that a URB is valid.
 */
#ifdef VBOX_STRICT
# define vusbUrbAssert(pUrb) do { \
    AssertMsg(VALID_PTR((pUrb)),  ("%p\n", (pUrb))); \
    AssertMsg((pUrb)->u32Magic == VUSBURB_MAGIC, ("%#x", (pUrb)->u32Magic)); \
    AssertMsg((pUrb)->enmState > VUSBURBSTATE_INVALID && (pUrb)->enmState < VUSBURBSTATE_END, \
              ("%d\n", (pUrb)->enmState)); \
    } while (0)
#else
# define vusbUrbAssert(pUrb) do {} while (0)
#endif

/** @} */




/**
 * Addresses are between 0 and 127 inclusive
 */
DECLINLINE(uint8_t) vusbHashAddress(uint8_t Address)
{
    uint8_t u8Hash = Address;
    u8Hash ^= (Address >> 2);
    u8Hash ^= (Address >> 3);
    u8Hash %= VUSB_ADDR_HASHSZ;
    return u8Hash;
}


/**
 * Gets the roothub of a device.
 *
 * @returns Pointer to the roothub instance the device is attached to.
 * @returns NULL if not attached to any hub.
 * @param   pDev    Pointer to the device in question.
 */
DECLINLINE(PVUSBROOTHUB) vusbDevGetRh(PVUSBDEV pDev)
{
    if (!pDev->pHub)
        return NULL;
    return pDev->pHub->pRootHub;
}



/** Strings for the CTLSTAGE enum values. */
extern const char * const g_apszCtlStates[4];
/** Default message pipe. */
extern const VUSBDESCENDPOINTEX g_Endpoint0;
/** Default configuration. */
extern const VUSBDESCCONFIGEX g_Config0;

RT_C_DECLS_END
#endif

