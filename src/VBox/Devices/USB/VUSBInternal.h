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
#include <iprt/queueatomic.h>
#include <iprt/req.h>

#include "VUSBSniffer.h"

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

/** Opaque VUSB read ahead buffer management handle. */
typedef struct VUSBREADAHEADINT *VUSBREADAHEAD;

/**
 * A VUSB pipe
 */
typedef struct vusb_pipe
{
    PCVUSBDESCENDPOINTEX in;
    PCVUSBDESCENDPOINTEX out;
    /** Pointer to the extra state data required to run a control pipe. */
    PVUSBCTRLEXTRA      pCtrl;
    /** Critical section serializing access to the extra state data for a control pipe. */
    RTCRITSECT          CritSectCtrl;
    /** Count of active async transfers. */
    volatile uint32_t   async;
    /** Read ahead handle. */
    VUSBREADAHEAD       hReadAhead;
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
    /** The device state. */
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
    /** Critical section protecting the active URB list. */
    RTCRITSECT          CritSectAsyncUrbs;
    /** List of active async URBs. */
    PVUSBURB            pAsyncUrbHead;

    /** Dumper state. */
    union VUSBDEVURBDUMPERSTATE
    {
        /** The current scsi command. */
        uint8_t             u8ScsiCmd;
    } Urb;

    /** The reset timer handle. */
    PTMTIMER            pResetTimer;
    /** Reset handler arguments. */
    void               *pvArgs;
    /** URB submit and reap thread. */
    RTTHREAD            hUrbIoThread;
    /** Request queue for executing tasks on the I/O thread which should be done
     * synchronous and without any other thread accessing the USB device. */
    RTREQQUEUE          hReqQueueSync;
    /** Sniffer instance for this device if configured. */
    VUSBSNIFFER         hSniffer;
    /** Flag whether the URB I/O thread should terminate. */
    bool volatile       fTerminate;
    /** Flag whether the I/O thread was woken up. */
    bool volatile       fWokenUp;
#if HC_ARCH_BITS == 32
    /** Align the size to a 8 byte boundary. */
    bool                afAlignment0[6];
#endif
} VUSBDEV;
AssertCompileSizeAlignment(VUSBDEV, 8);


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


int vusbDevInit(PVUSBDEV pDev, PPDMUSBINS pUsbIns, const char *pszCaptureFilename);
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
AssertCompileMemberAlignment(VUSBHUB, pOps, 8);
AssertCompileSizeAlignment(VUSBHUB, 8);

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
    /** Address hash table. */
    PVUSBDEV                apAddrHash[VUSB_ADDR_HASHSZ];
    /** The default address. */
    PVUSBDEV                pDefaultAddress;

    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to the root hub port interface we're attached to. */
    PVUSBIROOTHUBPORT       pIRhPort;
    /** Connector interface exposed upwards. */
    VUSBIROOTHUBCONNECTOR   IRhConnector;

#if HC_ARCH_BITS == 32
    uint32_t                Alignment0;
#endif
    /** Critical section protecting the device list. */
    RTCRITSECT              CritSectDevices;
    /** Chain of devices attached to this hub. */
    PVUSBDEV                pDevices;

#if HC_ARCH_BITS == 32
    uint32_t                Alignment1;
#endif

    /** Availability Bitmap. */
    VUSBPORTBITMAP          Bitmap;

    /** Critical section protecting the free list. */
    RTCRITSECT              CritSectFreeUrbs;
    /** Chain of free URBs. (Singly linked) */
    PVUSBURB                pFreeUrbs;
    /** The number of URBs in the pool. */
    uint32_t                cUrbsInPool;
    /** Version of the attached Host Controller. */
    uint32_t                fHcVersions;
#ifdef VBOX_WITH_STATISTICS
#if HC_ARCH_BITS == 32
    uint32_t                Alignment2; /**< Counters must be 64-bit aligned. */
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
AssertCompileMemberAlignment(VUSBROOTHUB, CritSectDevices, 8);
AssertCompileMemberAlignment(VUSBROOTHUB, CritSectFreeUrbs, 8);
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
void vusbUrbDoReapAsyncDev(PVUSBDEV pDev, RTMSINTERVAL cMillies);
void vusbUrbCancel(PVUSBURB pUrb, CANCELMODE mode);
void vusbUrbCancelAsync(PVUSBURB pUrb, CANCELMODE mode);
void vusbUrbRipe(PVUSBURB pUrb);
void vusbUrbCompletionRh(PVUSBURB pUrb);
int vusbUrbSubmitHardError(PVUSBURB pUrb);
int vusbUrbErrorRh(PVUSBURB pUrb);
int vusbDevUrbIoThreadWakeup(PVUSBDEV pDev);
int vusbDevUrbIoThreadCreate(PVUSBDEV pDev);
int vusbDevUrbIoThreadDestroy(PVUSBDEV pDev);
DECLHIDDEN(int) vusbDevIoThreadExecV(PVUSBDEV pDev, uint32_t fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);
DECLHIDDEN(int) vusbDevIoThreadExec(PVUSBDEV pDev, uint32_t fFlags, PFNRT pfnFunction, unsigned cArgs, ...);
DECLHIDDEN(int) vusbDevIoThreadExecSync(PVUSBDEV pDev, PFNRT pfnFunction, unsigned cArgs, ...);
DECLHIDDEN(void) vusbUrbCancelWorker(PVUSBURB pUrb, CANCELMODE enmMode);

void vusbUrbCompletionReadAhead(PVUSBURB pUrb);
VUSBREADAHEAD vusbReadAheadStart(PVUSBDEV pDev, PVUSBPIPE pPipe);
void vusbReadAheadStop(VUSBREADAHEAD hReadAhead);
int  vusbUrbQueueAsyncRh(PVUSBURB pUrb);
int  vusbUrbSubmitBufferedRead(PVUSBURB pUrb, VUSBREADAHEAD hReadAhead);
PVUSBURB vusbRhNewUrb(PVUSBROOTHUB pRh, uint8_t DstAddress, uint32_t cbData, uint32_t cTds);


DECLINLINE(void) vusbUrbUnlink(PVUSBURB pUrb)
{
    PVUSBDEV pDev = pUrb->VUsb.pDev;

    RTCritSectEnter(&pDev->CritSectAsyncUrbs);
    *pUrb->VUsb.ppPrev = pUrb->VUsb.pNext;
    if (pUrb->VUsb.pNext)
        pUrb->VUsb.pNext->VUsb.ppPrev = pUrb->VUsb.ppPrev;
    pUrb->VUsb.pNext = NULL;
    pUrb->VUsb.ppPrev = NULL;
    RTCritSectLeave(&pDev->CritSectAsyncUrbs);
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

/**
 * @def VUSBDEV_ASSERT_VALID_STATE
 * Asserts that the give device state is valid.
 */
#define VUSBDEV_ASSERT_VALID_STATE(enmState) \
    AssertMsg((enmState) > VUSB_DEVICE_STATE_INVALID && (enmState) < VUSB_DEVICE_STATE_DESTROYED, ("enmState=%#x\n", enmState));

/** Executes a function synchronously. */
#define VUSB_DEV_IO_THREAD_EXEC_FLAGS_SYNC RT_BIT_32(0)

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


/**
 * Returns the state of the USB device.
 *
 * @returns State of the USB device.
 * @param   pDev    Pointer to the device.
 */
DECLINLINE(VUSBDEVICESTATE) vusbDevGetState(PVUSBDEV pDev)
{
    VUSBDEVICESTATE enmState = (VUSBDEVICESTATE)ASMAtomicReadU32((volatile uint32_t *)&pDev->enmState);
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    return enmState;
}


/**
 * Sets the given state for the USB device.
 *
 * @returns The old state of the device.
 * @param   pDev     Pointer to the device.
 * @param   enmState The new state to set.
 */
DECLINLINE(VUSBDEVICESTATE) vusbDevSetState(PVUSBDEV pDev, VUSBDEVICESTATE enmState)
{
    VUSBDEV_ASSERT_VALID_STATE(enmState);
    VUSBDEVICESTATE enmStateOld = (VUSBDEVICESTATE)ASMAtomicXchgU32((volatile uint32_t *)&pDev->enmState, enmState);
    VUSBDEV_ASSERT_VALID_STATE(enmStateOld);
    return enmStateOld;
}


/**
 * Compare and exchange the states for the given USB device.
 *
 * @returns true if the state was changed.
 * @returns false if the state wasn't changed.
 * @param   pDev           Pointer to the device.
 * @param   enmStateNew    The new state to set.
 * @param   enmStateOld    The old state to compare with.
 */
DECLINLINE(bool) vusbDevSetStateCmp(PVUSBDEV pDev, VUSBDEVICESTATE enmStateNew, VUSBDEVICESTATE enmStateOld)
{
    VUSBDEV_ASSERT_VALID_STATE(enmStateNew);
    VUSBDEV_ASSERT_VALID_STATE(enmStateOld);
    return ASMAtomicCmpXchgU32((volatile uint32_t *)&pDev->enmState, enmStateNew, enmStateOld);
}

/** Strings for the CTLSTAGE enum values. */
extern const char * const g_apszCtlStates[4];
/** Default message pipe. */
extern const VUSBDESCENDPOINTEX g_Endpoint0;
/** Default configuration. */
extern const VUSBDESCCONFIGEX g_Config0;

RT_C_DECLS_END
#endif

