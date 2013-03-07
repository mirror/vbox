/* $Id$ */
/** @file
 * DevLsiLogicSCSI - LsiLogic LSI53c1030 SCSI controller.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_LSILOGICSCSI
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmqueue.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/memcache.h>
# include <iprt/mem.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
# include <iprt/time.h>
#endif

#include "DevLsiLogicSCSI.h"
#include "VBoxSCSI.h"

#include "VBoxDD.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The current saved state version. */
#define LSILOGIC_SAVED_STATE_VERSION          3
/** The saved state version used by VirtualBox before SAS support was added. */
#define LSILOGIC_SAVED_STATE_VERSION_PRE_SAS  2
/** The saved state version used by VirtualBox 3.0 and earlier.  It does not
 * include the device config part. */
#define LSILOGIC_SAVED_STATE_VERSION_VBOX_30  1

/** Maximum number of entries in the release log. */
#define MAX_REL_LOG_ERRORS 1024

#define LSILOGIC_RTGCPHYS_FROM_U32(Hi, Lo)         ( (RTGCPHYS)RT_MAKE_U64(Lo, Hi) )


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Reply data.
 */
typedef struct LSILOGICSCSIREPLY
{
    /** Lower 32 bits of the reply address in memory. */
    uint32_t      u32HostMFALowAddress;
    /** Full address of the reply in guest memory. */
    RTGCPHYS      GCPhysReplyAddress;
    /** Size of the reply. */
    uint32_t      cbReply;
    /** Different views to the reply depending on the request type. */
    MptReplyUnion Reply;
} LSILOGICSCSIREPLY;
/** Pointer to reply data. */
typedef LSILOGICSCSIREPLY *PLSILOGICSCSIREPLY;

/**
 * State of a device attached to the buslogic host adapter.
 *
 * @implements  PDMIBASE
 * @implements  PDMISCSIPORT
 * @implements  PDMILEDPORTS
 */
typedef struct LSILOGICDEVICE
{
    /** Pointer to the owning lsilogic device instance. - R3 pointer */
    R3PTRTYPE(struct LSILOGICSCSI *)  pLsiLogicR3;

    /** LUN of the device. */
    uint32_t                      iLUN;
    /** Number of outstanding tasks on the port. */
    volatile uint32_t             cOutstandingRequests;

#if HC_ARCH_BITS == 64
    uint32_t                      Alignment0;
#endif

    /** Our base interface. */
    PDMIBASE                      IBase;
    /** SCSI port interface. */
    PDMISCSIPORT                  ISCSIPort;
    /** Led interface. */
    PDMILEDPORTS                  ILed;
    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)          pDrvBase;
    /** Pointer to the underlying SCSI connector interface. */
    R3PTRTYPE(PPDMISCSICONNECTOR) pDrvSCSIConnector;
    /** The status LED state for this device. */
    PDMLED                        Led;

} LSILOGICDEVICE;
/** Pointer to a device state. */
typedef LSILOGICDEVICE *PLSILOGICDEVICE;

/** Pointer to a task state. */
typedef struct LSILOGICTASKSTATE *PLSILOGICTASKSTATE;

/**
 * Device instance data for the emulated SCSI controller.
 */
typedef struct LSILOGICSCSI
{
    /** PCI device structure. */
    PCIDEVICE            PciDev;
    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3         pDevInsR3;
    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0         pDevInsR0;
    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC         pDevInsRC;

    /** Flag whether the GC part of the device is enabled. */
    bool                 fGCEnabled;
    /** Flag whether the R0 part of the device is enabled. */
    bool                 fR0Enabled;

    /** The state the controller is currently in. */
    LSILOGICSTATE        enmState;
    /** Who needs to init the driver to get into operational state. */
    LSILOGICWHOINIT      enmWhoInit;
    /** Flag whether we are in doorbell function. */
    bool                 fDoorbellInProgress;
    /** Flag whether diagnostic access is enabled. */
    bool                 fDiagnosticEnabled;

    /** Flag whether a notification was send to R3. */
    bool                 fNotificationSend;

    /** Flag whether the guest enabled event notification from the IOC. */
    bool                 fEventNotificationEnabled;

#if HC_ARCH_BITS == 64
    uint32_t             Alignment0;
#endif

    /** Queue to send tasks to R3. - R3 ptr */
    R3PTRTYPE(PPDMQUEUE) pNotificationQueueR3;
    /** Queue to send tasks to R3. - R0 ptr */
    R0PTRTYPE(PPDMQUEUE) pNotificationQueueR0;
    /** Queue to send tasks to R3. - RC ptr */
    RCPTRTYPE(PPDMQUEUE) pNotificationQueueRC;

#if HC_ARCH_BITS == 64
    uint32_t             Alignment1;
#endif

    /** Number of device states allocated. */
    uint32_t             cDeviceStates;

#if HC_ARCH_BITS == 64
    uint32_t             Alignment2;
#endif

    /** States for attached devices. */
    R3PTRTYPE(PLSILOGICDEVICE) paDeviceStates;

    /** MMIO address the device is mapped to. */
    RTGCPHYS              GCPhysMMIOBase;
    /** I/O port address the device is mapped to. */
    RTIOPORT              IOPortBase;

    /** Interrupt mask. */
    volatile uint32_t     uInterruptMask;
    /** Interrupt status register. */
    volatile uint32_t     uInterruptStatus;

    /** Buffer for messages which are passed
     * through the doorbell using the
     * handshake method. */
    uint32_t              aMessage[sizeof(MptConfigurationRequest)];
    /** Actual position in the buffer. */
    uint32_t              iMessage;
    /** Size of the message which is given in the doorbell message in dwords. */
    uint32_t              cMessage;

    /** Reply buffer. */
    MptReplyUnion         ReplyBuffer;
    /** Next entry to read. */
    uint32_t              uNextReplyEntryRead;
    /** Size of the reply in the buffer in 16bit words. */
    uint32_t              cReplySize;

    /** The fault code of the I/O controller if we are in the fault state. */
    uint16_t              u16IOCFaultCode;

    /** Upper 32 bits of the message frame address to locate requests in guest memory. */
    uint32_t              u32HostMFAHighAddr;
    /** Upper 32 bits of the sense buffer address. */
    uint32_t              u32SenseBufferHighAddr;
    /** Maximum number of devices the driver reported he can handle. */
    uint8_t               cMaxDevices;
    /** Maximum number of buses the driver reported he can handle. */
    uint8_t               cMaxBuses;
    /** Current size of reply message frames in the guest. */
    uint16_t              cbReplyFrame;

    /** Next key to write in the sequence to get access
     *  to diagnostic memory. */
    uint32_t              iDiagnosticAccess;

    /** Number entries allocated for the reply queue. */
    uint32_t              cReplyQueueEntries;
    /** Number entries allocated for the outstanding request queue. */
    uint32_t              cRequestQueueEntries;

    uint32_t              Alignment3;

    /** Critical section protecting the reply post queue. */
    PDMCRITSECT           ReplyPostQueueCritSect;
    /** Critical section protecting the reply free queue. */
    PDMCRITSECT           ReplyFreeQueueCritSect;

    /** Pointer to the start of the reply free queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseR3;
    /** Pointer to the start of the reply post queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pReplyPostQueueBaseR3;
    /** Pointer to the start of the request queue - R3. */
    R3PTRTYPE(volatile uint32_t *) pRequestQueueBaseR3;

    /** Pointer to the start of the reply queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseR0;
    /** Pointer to the start of the reply queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pReplyPostQueueBaseR0;
    /** Pointer to the start of the request queue - R0. */
    R0PTRTYPE(volatile uint32_t *) pRequestQueueBaseR0;

    /** Pointer to the start of the reply queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pReplyFreeQueueBaseRC;
    /** Pointer to the start of the reply queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pReplyPostQueueBaseRC;
    /** Pointer to the start of the request queue - RC. */
    RCPTRTYPE(volatile uint32_t *) pRequestQueueBaseRC;

    /** Next free entry in the reply queue the guest can write a address to. */
    volatile uint32_t              uReplyFreeQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for reply frames from. */
    volatile uint32_t              uReplyFreeQueueNextAddressRead;

    /** Next free entry in the reply queue the guest can write a address to. */
    volatile uint32_t              uReplyPostQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for reply frames from. */
    volatile uint32_t              uReplyPostQueueNextAddressRead;

    /** Next free entry the guest can write a address to a request frame to. */
    volatile uint32_t              uRequestQueueNextEntryFreeWrite;
    /** Next valid entry the controller can read a valid address for request frames from. */
    volatile uint32_t              uRequestQueueNextAddressRead;

    /** Emulated controller type */
    LSILOGICCTRLTYPE               enmCtrlType;
    /** Handle counter */
    uint16_t                       u16NextHandle;

    uint16_t                       u16Alignment4;
    uint32_t                       u32Alignment5;

    /** Number of ports this controller has. */
    uint8_t                        cPorts;

#if HC_ARCH_BITS == 64
    uint32_t                       Alignment6;
#endif

    /** BIOS emulation. */
    VBOXSCSI                       VBoxSCSI;
    /** Cache for allocated tasks. */
    R3PTRTYPE(RTMEMCACHE)          hTaskCache;
    /** Status LUN: The base interface. */
    PDMIBASE                       IBase;
    /** Status LUN: Leds interface. */
    PDMILEDPORTS                   ILeds;
    /** Status LUN: Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)  pLedsConnector;
    /** Pointer to the configuration page area. */
    R3PTRTYPE(PMptConfigurationPagesSupported) pConfigurationPages;

#if HC_ARCH_BITS == 64
    uint32_t                       Alignment7;
#endif

    /** Indicates that PDMDevHlpAsyncNotificationCompleted should be called when
     * a port is entering the idle state. */
    bool volatile                  fSignalIdle;
    /** Flag whether we have tasks which need to be processed again- */
    bool volatile                  fRedo;
    /** List of tasks which can be redone. */
    R3PTRTYPE(volatile PLSILOGICTASKSTATE) pTasksRedoHead;

} LSILOGISCSI;
/** Pointer to the device instance data of the LsiLogic emulation. */
typedef LSILOGICSCSI *PLSILOGICSCSI;

/**
 * Scatter gather list entry data.
 */
typedef struct LSILOGICTASKSTATESGENTRY
{
    /** Flag whether the buffer in the list is from the guest or an
     *  allocated temporary buffer because the segments in the guest
     *  are not sector aligned.
     */
    bool     fGuestMemory;
    /** Flag whether the buffer contains data or is the destination for the transfer. */
    bool     fBufferContainsData;
    /** Pointer to the start of the buffer. */
    void    *pvBuf;
    /** Size of the buffer. */
    uint32_t cbBuf;
    /** Flag dependent data. */
    union
    {
        /** Data to handle direct mappings of guest buffers. */
        PGMPAGEMAPLOCK  PageLock;
        /** The segment in the guest which is not sector aligned. */
        RTGCPHYS        GCPhysAddrBufferUnaligned;
    } u;
} LSILOGICTASKSTATESGENTRY;
/** Pointer to a scatter/gather list entry. */
typedef LSILOGICTASKSTATESGENTRY *PLSILOGICTASKSTATESGENTRY;

/**
 * Task state object which holds all necessary data while
 * processing the request from the guest.
 */
typedef struct LSILOGICTASKSTATE
{
    /** Next in the redo list. */
    PLSILOGICTASKSTATE         pRedoNext;
    /** Target device. */
    PLSILOGICDEVICE            pTargetDevice;
    /** The message request from the guest. */
    MptRequestUnion            GuestRequest;
    /** Reply message if the request produces one. */
    MptReplyUnion              IOCReply;
    /** SCSI request structure for the SCSI driver. */
    PDMSCSIREQUEST             PDMScsiRequest;
    /** Address of the message request frame in guests memory.
     *  Used to read the S/G entries in the second step. */
    RTGCPHYS                   GCPhysMessageFrameAddr;
    /** Number of scatter gather list entries. */
    uint32_t                   cSGListEntries;
    /** How many entries would fit into the sg list. */
    uint32_t                   cSGListSize;
    /** How many times the list was too big. */
    uint32_t                   cSGListTooBig;
    /** Pointer to the first entry of the scatter gather list. */
    PRTSGSEG                   pSGListHead;
    /** How many entries would fit into the sg info list. */
    uint32_t                   cSGInfoSize;
    /** Number of entries for the information entries. */
    uint32_t                   cSGInfoEntries;
    /** How many times the list was too big. */
    uint32_t                   cSGInfoTooBig;
    /** Pointer to the first mapping information entry. */
    PLSILOGICTASKSTATESGENTRY  paSGEntries;
    /** Size of the temporary buffer for unaligned guest segments. */
    uint32_t                   cbBufferUnaligned;
    /** Pointer to the temporary buffer. */
    void                      *pvBufferUnaligned;
    /** Pointer to the sense buffer. */
    uint8_t                    abSenseBuffer[18];
    /** Flag whether the request was issued from the BIOS. */
    bool                       fBIOS;
} LSILOGICTASKSTATE;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
#ifdef IN_RING3
static void lsilogicR3InitializeConfigurationPages(PLSILOGICSCSI pThis);
static void lsilogicR3ConfigurationPagesFree(PLSILOGICSCSI pThis);
static int  lsilogicR3ProcessConfigurationRequest(PLSILOGICSCSI pThis, PMptConfigurationRequest pConfigurationReq,
                                                  PMptConfigurationReply pReply);
#endif
RT_C_DECLS_END


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Key sequence the guest has to write to enable access
 * to diagnostic memory. */
static const uint8_t g_lsilogicDiagnosticAccess[] = {0x04, 0x0b, 0x02, 0x07, 0x0d};

/**
 * Updates the status of the interrupt pin of the device.
 *
 * @returns nothing.
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicUpdateInterrupt(PLSILOGICSCSI pThis)
{
    uint32_t uIntSts;

    LogFlowFunc(("Updating interrupts\n"));

    /* Mask out doorbell status so that it does not affect interrupt updating. */
    uIntSts = (ASMAtomicReadU32(&pThis->uInterruptStatus) & ~LSILOGIC_REG_HOST_INTR_STATUS_DOORBELL_STS);
    /* Check maskable interrupts. */
    uIntSts &= ~(ASMAtomicReadU32(&pThis->uInterruptMask) & ~LSILOGIC_REG_HOST_INTR_MASK_IRQ_ROUTING);

    if (uIntSts)
    {
        LogFlowFunc(("Setting interrupt\n"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, 1);
    }
    else
    {
        LogFlowFunc(("Clearing interrupt\n"));
        PDMDevHlpPCISetIrq(pThis->CTX_SUFF(pDevIns), 0, 0);
    }
}

/**
 * Sets a given interrupt status bit in the status register and
 * updates the interrupt status.
 *
 * @returns nothing.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   uStatus     The status bit to set.
 */
DECLINLINE(void) lsilogicSetInterrupt(PLSILOGICSCSI pThis, uint32_t uStatus)
{
    ASMAtomicOrU32(&pThis->uInterruptStatus, uStatus);
    lsilogicUpdateInterrupt(pThis);
}

/**
 * Clears a given interrupt status bit in the status register and
 * updates the interrupt status.
 *
 * @returns nothing.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   uStatus     The status bit to set.
 */
DECLINLINE(void) lsilogicClearInterrupt(PLSILOGICSCSI pThis, uint32_t uStatus)
{
    ASMAtomicAndU32(&pThis->uInterruptStatus, ~uStatus);
    lsilogicUpdateInterrupt(pThis);
}

/**
 * Sets the I/O controller into fault state and sets the fault code.
 *
 * @returns nothing
 * @param   pThis           Pointer to the LsiLogic device state.
 * @param   uIOCFaultCode   Fault code to set.
 */
DECLINLINE(void) lsilogicSetIOCFaultCode(PLSILOGICSCSI pThis, uint16_t uIOCFaultCode)
{
    if (pThis->enmState != LSILOGICSTATE_FAULT)
    {
        Log(("%s: Setting I/O controller into FAULT state: uIOCFaultCode=%u\n", __FUNCTION__, uIOCFaultCode));
        pThis->enmState        = LSILOGICSTATE_FAULT;
        pThis->u16IOCFaultCode = uIOCFaultCode;
    }
    else
    {
        Log(("%s: We are already in FAULT state\n"));
    }
}

#ifdef IN_RING3

/**
 * Performs a hard reset on the controller.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static int lsilogicR3HardReset(PLSILOGICSCSI pThis)
{
    pThis->enmState = LSILOGICSTATE_RESET;

    /* The interrupts are masked out. */
    pThis->uInterruptMask |= LSILOGIC_REG_HOST_INTR_MASK_DOORBELL
                           | LSILOGIC_REG_HOST_INTR_MASK_REPLY;
    /* Reset interrupt states. */
    pThis->uInterruptStatus = 0;
    lsilogicUpdateInterrupt(pThis);

    /* Reset the queues. */
    pThis->uReplyFreeQueueNextEntryFreeWrite = 0;
    pThis->uReplyFreeQueueNextAddressRead    = 0;
    pThis->uReplyPostQueueNextEntryFreeWrite = 0;
    pThis->uReplyPostQueueNextAddressRead    = 0;
    pThis->uRequestQueueNextEntryFreeWrite   = 0;
    pThis->uRequestQueueNextAddressRead      = 0;

    /* Disable diagnostic access. */
    pThis->iDiagnosticAccess = 0;

    /* Set default values. */
    pThis->cMaxDevices   = pThis->cDeviceStates;
    pThis->cMaxBuses     = 1;
    pThis->cbReplyFrame  = 128; /* @todo Figure out where it is needed. */
    pThis->u16NextHandle = 1;
    /** @todo: Put stuff to reset here. */

    lsilogicR3ConfigurationPagesFree(pThis);
    lsilogicR3InitializeConfigurationPages(pThis);

    /* Mark that we finished performing the reset. */
    pThis->enmState = LSILOGICSTATE_READY;
    return VINF_SUCCESS;
}

/**
 * Frees the configuration pages if allocated.
 *
 * @returns nothing.
 * @param pThis    The LsiLogic controller instance
 */
static void lsilogicR3ConfigurationPagesFree(PLSILOGICSCSI pThis)
{

    if (pThis->pConfigurationPages)
    {
        /* Destroy device list if we emulate a SAS controller. */
        if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
        {
            PMptConfigurationPagesSas pSasPages = &pThis->pConfigurationPages->u.SasPages;
            PMptSASDevice pSASDeviceCurr = pSasPages->pSASDeviceHead;

            while (pSASDeviceCurr)
            {
                PMptSASDevice pFree = pSASDeviceCurr;

                pSASDeviceCurr = pSASDeviceCurr->pNext;
                RTMemFree(pFree);
            }
            if (pSasPages->paPHYs)
                RTMemFree(pSasPages->paPHYs);
            if (pSasPages->pManufacturingPage7)
                RTMemFree(pSasPages->pManufacturingPage7);
            if (pSasPages->pSASIOUnitPage0)
                RTMemFree(pSasPages->pSASIOUnitPage0);
            if (pSasPages->pSASIOUnitPage1)
                RTMemFree(pSasPages->pSASIOUnitPage1);
        }

        RTMemFree(pThis->pConfigurationPages);
    }
}

/**
 * Finishes a context reply.
 *
 * @returns nothing
 * @param   pThis               Pointer to the LsiLogic device state.
 * @param   u32MessageContext   The message context ID to post.
 */
static void lsilogicR3FinishContextReply(PLSILOGICSCSI pThis, uint32_t u32MessageContext)
{
    int rc;

    LogFlowFunc(("pThis=%#p u32MessageContext=%#x\n", pThis, u32MessageContext));

    AssertMsg(!pThis->fDoorbellInProgress, ("We are in a doorbell function\n"));

    /* Write message context ID into reply post queue. */
    rc = PDMCritSectEnter(&pThis->ReplyPostQueueCritSect, VINF_SUCCESS);
    AssertRC(rc);

# if 0
    /* Check for a entry in the queue. */
    if (RT_UNLIKELY(pThis->uReplyPostQueueNextAddressRead != pThis->uReplyPostQueueNextEntryFreeWrite))
    {
        /* Set error code. */
        lsilogicSetIOCFaultCode(pThis, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
        PDMCritSectLeave(&pThis->ReplyPostQueueCritSect);
        return;
    }
# endif

    /* We have a context reply. */
    ASMAtomicWriteU32(&pThis->CTX_SUFF(pReplyPostQueueBase)[pThis->uReplyPostQueueNextEntryFreeWrite], u32MessageContext);
    ASMAtomicIncU32(&pThis->uReplyPostQueueNextEntryFreeWrite);
    pThis->uReplyPostQueueNextEntryFreeWrite %= pThis->cReplyQueueEntries;

    /* Set interrupt. */
    lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);

    PDMCritSectLeave(&pThis->ReplyPostQueueCritSect);
}

static void lsilogicR3TaskStateClear(PLSILOGICTASKSTATE pTaskState)
{
    RTMemFree(pTaskState->pSGListHead);
    RTMemFree(pTaskState->paSGEntries);
    if (pTaskState->pvBufferUnaligned)
        RTMemPageFree(pTaskState->pvBufferUnaligned, pTaskState->cbBufferUnaligned);
    pTaskState->cSGListSize = 0;
    pTaskState->cSGInfoSize = 0;
    pTaskState->cSGInfoEntries = 0;
    pTaskState->cSGListTooBig  = 0;
    pTaskState->pSGListHead = NULL;
    pTaskState->paSGEntries = NULL;
    pTaskState->pvBufferUnaligned = NULL;
    pTaskState->cbBufferUnaligned = 0;
}

/**
 * @callback_method_impl{FNMEMCACHECTOR}
 */
static DECLCALLBACK(int) lsilogicR3TaskStateCtor(RTMEMCACHE hMemCache, void *pvObj, void *pvUser)
{
    memset(pvObj, 0, sizeof(LSILOGICTASKSTATE));
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNMEMCACHEDTOR}
 */
static DECLCALLBACK(void) lsilogicR3TaskStateDtor(RTMEMCACHE hMemCache, void *pvObj, void *pvUser)
{
    PLSILOGICTASKSTATE pTaskState = (PLSILOGICTASKSTATE)pvObj;
    lsilogicR3TaskStateClear(pTaskState);
}

#endif /* IN_RING3 */

/**
 * Takes necessary steps to finish a reply frame.
 *
 * @returns nothing
 * @param   pThis           Pointer to the LsiLogic device state.
 * @param   pReply          Pointer to the reply message.
 * @param   fForceReplyFifo Flag whether the use of the reply post fifo is forced.
 */
static void lsilogicFinishAddressReply(PLSILOGICSCSI pThis, PMptReplyUnion pReply, bool fForceReplyFifo)
{
    /*
     * If we are in a doorbell function we set the reply size now and
     * set the system doorbell status interrupt to notify the guest that
     * we are ready to send the reply.
     */
    if (pThis->fDoorbellInProgress && !fForceReplyFifo)
    {
        /* Set size of the reply in 16bit words. The size in the reply is in 32bit dwords. */
        pThis->cReplySize = pReply->Header.u8MessageLength * 2;
        Log(("%s: cReplySize=%u\n", __FUNCTION__, pThis->cReplySize));
        pThis->uNextReplyEntryRead = 0;
        lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
    }
    else
    {
        /*
         * The reply queues are only used if the request was fetched from the request queue.
         * Requests from the request queue are always transferred to R3. So it is not possible
         * that this case happens in R0 or GC.
         */
#ifdef IN_RING3
        int rc;
        /* Grab a free reply message from the queue. */
        rc = PDMCritSectEnter(&pThis->ReplyFreeQueueCritSect, VINF_SUCCESS);
        AssertRC(rc);

#if 0
        /* Check for a free reply frame. */
        if (RT_UNLIKELY(pThis->uReplyFreeQueueNextAddressRead != pThis->uReplyFreeQueueNextEntryFreeWrite))
        {
            /* Set error code. */
            lsilogicSetIOCFaultCode(pThis, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
            PDMCritSectLeave(&pThis->ReplyFreeQueueCritSect);
            return;
        }
#endif

        uint32_t u32ReplyFrameAddressLow = pThis->CTX_SUFF(pReplyFreeQueueBase)[pThis->uReplyFreeQueueNextAddressRead];

        pThis->uReplyFreeQueueNextAddressRead++;
        pThis->uReplyFreeQueueNextAddressRead %= pThis->cReplyQueueEntries;

        PDMCritSectLeave(&pThis->ReplyFreeQueueCritSect);

        /* Build 64bit physical address. */
        RTGCPHYS GCPhysReplyMessage = LSILOGIC_RTGCPHYS_FROM_U32(pThis->u32HostMFAHighAddr, u32ReplyFrameAddressLow);
        size_t cbReplyCopied = (pThis->cbReplyFrame < sizeof(MptReplyUnion)) ? pThis->cbReplyFrame : sizeof(MptReplyUnion);

        /* Write reply to guest memory. */
        PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), GCPhysReplyMessage, pReply, cbReplyCopied);

        /* Write low 32bits of reply frame into post reply queue. */
        rc = PDMCritSectEnter(&pThis->ReplyPostQueueCritSect, VINF_SUCCESS);
        AssertRC(rc);

#if 0
        /* Check for a entry in the queue. */
        if (RT_UNLIKELY(pThis->uReplyPostQueueNextAddressRead != pThis->uReplyPostQueueNextEntryFreeWrite))
        {
            /* Set error code. */
            lsilogicSetIOCFaultCode(pThis, LSILOGIC_IOCSTATUS_INSUFFICIENT_RESOURCES);
            PDMCritSectLeave(&pThis->ReplyPostQueueCritSect);
            return;
        }
#endif

        /* We have a address reply. Set the 31th bit to indicate that. */
        ASMAtomicWriteU32(&pThis->CTX_SUFF(pReplyPostQueueBase)[pThis->uReplyPostQueueNextEntryFreeWrite],
                          RT_BIT(31) | (u32ReplyFrameAddressLow >> 1));
        ASMAtomicIncU32(&pThis->uReplyPostQueueNextEntryFreeWrite);
        pThis->uReplyPostQueueNextEntryFreeWrite %= pThis->cReplyQueueEntries;

        if (fForceReplyFifo)
        {
            pThis->fDoorbellInProgress = false;
            lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
        }

        /* Set interrupt. */
        lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);

        PDMCritSectLeave(&pThis->ReplyPostQueueCritSect);
#else
        AssertMsgFailed(("This is not allowed to happen.\n"));
#endif
    }
}

#ifdef IN_RING3
/**
 * Processes a given Request from the guest
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   pMessageHdr Pointer to the message header of the request.
 * @param   pReply      Pointer to the reply.
 */
static int lsilogicR3ProcessMessageRequest(PLSILOGICSCSI pThis, PMptMessageHdr pMessageHdr, PMptReplyUnion pReply)
{
    int rc = VINF_SUCCESS;
    bool fForceReplyPostFifo = false;

# ifdef LOG_ENABLED
    if (pMessageHdr->u8Function < RT_ELEMENTS(g_apszMPTFunctionNames))
        Log(("Message request function: %s\n", g_apszMPTFunctionNames[pMessageHdr->u8Function]));
    else
        Log(("Message request function: <unknown>\n"));
# endif

    memset(pReply, 0, sizeof(MptReplyUnion));

    switch (pMessageHdr->u8Function)
    {
        case MPT_MESSAGE_HDR_FUNCTION_SCSI_TASK_MGMT:
        {
            PMptSCSITaskManagementRequest pTaskMgmtReq = (PMptSCSITaskManagementRequest)pMessageHdr;

            LogFlow(("u8TaskType=%u\n", pTaskMgmtReq->u8TaskType));
            LogFlow(("u32TaskMessageContext=%#x\n", pTaskMgmtReq->u32TaskMessageContext));

            pReply->SCSITaskManagement.u8MessageLength     = 6;     /* 6 32bit dwords. */
            pReply->SCSITaskManagement.u8TaskType          = pTaskMgmtReq->u8TaskType;
            pReply->SCSITaskManagement.u32TerminationCount = 0;
            fForceReplyPostFifo = true;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_IOC_INIT:
        {
            /*
             * This request sets the I/O controller to the
             * operational state.
             */
            PMptIOCInitRequest pIOCInitReq = (PMptIOCInitRequest)pMessageHdr;

            /* Update configuration values. */
            pThis->enmWhoInit             = (LSILOGICWHOINIT)pIOCInitReq->u8WhoInit;
            pThis->cbReplyFrame           = pIOCInitReq->u16ReplyFrameSize;
            pThis->cMaxBuses              = pIOCInitReq->u8MaxBuses;
            pThis->cMaxDevices            = pIOCInitReq->u8MaxDevices;
            pThis->u32HostMFAHighAddr     = pIOCInitReq->u32HostMfaHighAddr;
            pThis->u32SenseBufferHighAddr = pIOCInitReq->u32SenseBufferHighAddr;

            if (pThis->enmState == LSILOGICSTATE_READY)
            {
                pThis->enmState = LSILOGICSTATE_OPERATIONAL;
            }

            /* Return reply. */
            pReply->IOCInit.u8MessageLength = 5;
            pReply->IOCInit.u8WhoInit       = pThis->enmWhoInit;
            pReply->IOCInit.u8MaxDevices    = pThis->cMaxDevices;
            pReply->IOCInit.u8MaxBuses      = pThis->cMaxBuses;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_IOC_FACTS:
        {
            pReply->IOCFacts.u8MessageLength      = 15;     /* 15 32bit dwords. */

            if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
            {
                pReply->IOCFacts.u16MessageVersion    = 0x0102; /* Version from the specification. */
                pReply->IOCFacts.u8NumberOfPorts      = pThis->cPorts;
            }
            else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
            {
                pReply->IOCFacts.u16MessageVersion    = 0x0105; /* Version from the specification. */
                pReply->IOCFacts.u8NumberOfPorts      = pThis->cPorts;
            }
            else
                AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));

            pReply->IOCFacts.u8IOCNumber          = 0;      /* PCI function number. */
            pReply->IOCFacts.u16IOCExceptions     = 0;
            pReply->IOCFacts.u8MaxChainDepth      = LSILOGICSCSI_MAXIMUM_CHAIN_DEPTH;
            pReply->IOCFacts.u8WhoInit            = pThis->enmWhoInit;
            pReply->IOCFacts.u8BlockSize          = 12;     /* Block size in 32bit dwords. This is the largest request we can get (SCSI I/O). */
            pReply->IOCFacts.u8Flags              = 0;      /* Bit 0 is set if the guest must upload the FW prior to using the controller. Obviously not needed here. */
            pReply->IOCFacts.u16ReplyQueueDepth   = pThis->cReplyQueueEntries - 1; /* One entry is always free. */
            pReply->IOCFacts.u16RequestFrameSize  = 128;    /* @todo Figure out where it is needed. */
            pReply->IOCFacts.u16ProductID         = 0xcafe; /* Our own product ID :) */
            pReply->IOCFacts.u32CurrentHostMFAHighAddr = pThis->u32HostMFAHighAddr;
            pReply->IOCFacts.u16GlobalCredits     = pThis->cRequestQueueEntries - 1; /* One entry is always free. */

            pReply->IOCFacts.u8EventState         = 0; /* Event notifications not enabled. */
            pReply->IOCFacts.u32CurrentSenseBufferHighAddr = pThis->u32SenseBufferHighAddr;
            pReply->IOCFacts.u16CurReplyFrameSize = pThis->cbReplyFrame;
            pReply->IOCFacts.u8MaxDevices         = pThis->cMaxDevices;
            pReply->IOCFacts.u8MaxBuses           = pThis->cMaxBuses;
            pReply->IOCFacts.u32FwImageSize       = 0; /* No image needed. */
            pReply->IOCFacts.u32FWVersion         = 0;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_PORT_FACTS:
        {
            PMptPortFactsRequest pPortFactsReq = (PMptPortFactsRequest)pMessageHdr;

            pReply->PortFacts.u8MessageLength = 10;
            pReply->PortFacts.u8PortNumber    = pPortFactsReq->u8PortNumber;

            if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
            {
                /* This controller only supports one bus with bus number 0. */
                if (pPortFactsReq->u8PortNumber >= pThis->cPorts)
                {
                    pReply->PortFacts.u8PortType = 0; /* Not existant. */
                }
                else
                {
                    pReply->PortFacts.u8PortType             = 0x01; /* SCSI Port. */
                    pReply->PortFacts.u16MaxDevices          = LSILOGICSCSI_PCI_SPI_DEVICES_PER_BUS_MAX;
                    pReply->PortFacts.u16ProtocolFlags       = RT_BIT(3) | RT_BIT(0); /* SCSI initiator and LUN supported. */
                    pReply->PortFacts.u16PortSCSIID          = 7; /* Default */
                    pReply->PortFacts.u16MaxPersistentIDs    = 0;
                    pReply->PortFacts.u16MaxPostedCmdBuffers = 0; /* Only applies for target mode which we dont support. */
                    pReply->PortFacts.u16MaxLANBuckets       = 0; /* Only for the LAN controller. */
                }
            }
            else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
            {
                if (pPortFactsReq->u8PortNumber >= pThis->cPorts)
                {
                    pReply->PortFacts.u8PortType = 0; /* Not existant. */
                }
                else
                {
                    pReply->PortFacts.u8PortType             = 0x30; /* SAS Port. */
                    pReply->PortFacts.u16MaxDevices          = pThis->cPorts;
                    pReply->PortFacts.u16ProtocolFlags       = RT_BIT(3) | RT_BIT(0); /* SCSI initiator and LUN supported. */
                    pReply->PortFacts.u16PortSCSIID          = pThis->cPorts;
                    pReply->PortFacts.u16MaxPersistentIDs    = 0;
                    pReply->PortFacts.u16MaxPostedCmdBuffers = 0; /* Only applies for target mode which we dont support. */
                    pReply->PortFacts.u16MaxLANBuckets       = 0; /* Only for the LAN controller. */
                }
            }
            else
                AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_PORT_ENABLE:
        {
            /*
             * The port enable request notifies the IOC to make the port available and perform
             * appropriate discovery on the associated link.
             */
            PMptPortEnableRequest pPortEnableReq = (PMptPortEnableRequest)pMessageHdr;

            pReply->PortEnable.u8MessageLength = 5;
            pReply->PortEnable.u8PortNumber    = pPortEnableReq->u8PortNumber;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_EVENT_NOTIFICATION:
        {
            PMptEventNotificationRequest pEventNotificationReq = (PMptEventNotificationRequest)pMessageHdr;

            if (pEventNotificationReq->u8Switch)
                pThis->fEventNotificationEnabled = true;
            else
                pThis->fEventNotificationEnabled = false;

            pReply->EventNotification.u16EventDataLength = 1; /* 1 32bit D-Word. */
            pReply->EventNotification.u8MessageLength    = 8;
            pReply->EventNotification.u8MessageFlags     = (1 << 7);
            pReply->EventNotification.u8AckRequired      = 0;
            pReply->EventNotification.u32Event           = MPT_EVENT_EVENT_CHANGE;
            pReply->EventNotification.u32EventContext    = 0;
            pReply->EventNotification.u32EventData       = pThis->fEventNotificationEnabled ? 1 : 0;

            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_EVENT_ACK:
        {
            AssertMsgFailed(("todo"));
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_CONFIG:
        {
            PMptConfigurationRequest pConfigurationReq = (PMptConfigurationRequest)pMessageHdr;

            rc = lsilogicR3ProcessConfigurationRequest(pThis, pConfigurationReq, &pReply->Configuration);
            AssertRC(rc);
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_FW_UPLOAD:
        {
            PMptFWUploadRequest pFWUploadReq = (PMptFWUploadRequest)pMessageHdr;

            pReply->FWUpload.u8ImageType        = pFWUploadReq->u8ImageType;
            pReply->FWUpload.u8MessageLength    = 6;
            pReply->FWUpload.u32ActualImageSize = 0;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_FW_DOWNLOAD:
        {
            //PMptFWDownloadRequest pFWDownloadReq = (PMptFWDownloadRequest)pMessageHdr;

            pReply->FWDownload.u8MessageLength    = 5;
            break;
        }
        case MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST: /* Should be handled already. */
        default:
            AssertMsgFailed(("Invalid request function %#x\n", pMessageHdr->u8Function));
    }

    /* Copy common bits from request message frame to reply. */
    pReply->Header.u8Function        = pMessageHdr->u8Function;
    pReply->Header.u32MessageContext = pMessageHdr->u32MessageContext;

    lsilogicFinishAddressReply(pThis, pReply, fForceReplyPostFifo);
    return rc;
}
#endif /* IN_RING3 */

/**
 * Writes a value to a register at a given offset.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   offReg      Offset of the register to write.
 * @param   u32         The value being written.
 */
static int lsilogicRegisterWrite(PLSILOGICSCSI pThis, uint32_t offReg, uint32_t u32)
{
    LogFlowFunc(("pThis=%#p offReg=%#x u32=%#x\n", pThis, offReg, u32));
    switch (offReg)
    {
        case LSILOGIC_REG_REPLY_QUEUE:
        {
            /* Add the entry to the reply free queue. */
            ASMAtomicWriteU32(&pThis->CTX_SUFF(pReplyFreeQueueBase)[pThis->uReplyFreeQueueNextEntryFreeWrite], u32);
            pThis->uReplyFreeQueueNextEntryFreeWrite++;
            pThis->uReplyFreeQueueNextEntryFreeWrite %= pThis->cReplyQueueEntries;
            break;
        }
        case LSILOGIC_REG_REQUEST_QUEUE:
        {
            uint32_t uNextWrite = ASMAtomicReadU32(&pThis->uRequestQueueNextEntryFreeWrite);

            ASMAtomicWriteU32(&pThis->CTX_SUFF(pRequestQueueBase)[uNextWrite], u32);

            /*
             * Don't update the value in place. It can happen that we get preempted
             * after the increment but before the modulo.
             * Another EMT will read the wrong value when processing the queues
             * and hang in an endless loop creating thousands of requests.
             */
            uNextWrite++;
            uNextWrite %= pThis->cRequestQueueEntries;
            ASMAtomicWriteU32(&pThis->uRequestQueueNextEntryFreeWrite, uNextWrite);

            /* Send notification to R3 if there is not one send already. */
            if (!ASMAtomicXchgBool(&pThis->fNotificationSend, true))
            {
                PPDMQUEUEITEMCORE pNotificationItem = PDMQueueAlloc(pThis->CTX_SUFF(pNotificationQueue));
                AssertPtr(pNotificationItem);

                PDMQueueInsert(pThis->CTX_SUFF(pNotificationQueue), pNotificationItem);
            }
            break;
        }
        case LSILOGIC_REG_DOORBELL:
        {
            /*
             * When the guest writes to this register a real device would set the
             * doorbell status bit in the interrupt status register to indicate that the IOP
             * has still to process the message.
             * The guest needs to wait with posting new messages here until the bit is cleared.
             * Because the guest is not continuing execution while we are here we can skip this.
             */
            if (!pThis->fDoorbellInProgress)
            {
                uint32_t uFunction = LSILOGIC_REG_DOORBELL_GET_FUNCTION(u32);

                switch (uFunction)
                {
                    case LSILOGIC_DOORBELL_FUNCTION_IOC_MSG_UNIT_RESET:
                    {
                        pThis->enmState = LSILOGICSTATE_RESET;

                        /* Reset interrupt status. */
                        pThis->uInterruptStatus = 0;
                        lsilogicUpdateInterrupt(pThis);

                        /* Reset the queues. */
                        pThis->uReplyFreeQueueNextEntryFreeWrite = 0;
                        pThis->uReplyFreeQueueNextAddressRead = 0;
                        pThis->uReplyPostQueueNextEntryFreeWrite = 0;
                        pThis->uReplyPostQueueNextAddressRead = 0;
                        pThis->uRequestQueueNextEntryFreeWrite = 0;
                        pThis->uRequestQueueNextAddressRead = 0;
                        pThis->enmState = LSILOGICSTATE_READY;
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_IO_UNIT_RESET:
                    {
                        AssertMsgFailed(("todo\n"));
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_HANDSHAKE:
                    {
                        pThis->cMessage = LSILOGIC_REG_DOORBELL_GET_SIZE(u32);
                        pThis->iMessage = 0;
                        AssertMsg(pThis->cMessage <= RT_ELEMENTS(pThis->aMessage),
                                  ("Message doesn't fit into the buffer, cMessage=%u", pThis->cMessage));
                        pThis->fDoorbellInProgress = true;
                        /* Update the interrupt status to notify the guest that a doorbell function was started. */
                        lsilogicSetInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
                        break;
                    }
                    case LSILOGIC_DOORBELL_FUNCTION_REPLY_FRAME_REMOVAL:
                    {
                        AssertMsgFailed(("todo\n"));
                        break;
                    }
                    default:
                        AssertMsgFailed(("Unknown function %u to perform\n", uFunction));
                }
            }
            else
            {
                /*
                 * We are already performing a doorbell function.
                 * Get the remaining parameters.
                 */
                AssertMsg(pThis->iMessage < RT_ELEMENTS(pThis->aMessage), ("Message is too big to fit into the buffer\n"));
                /*
                 * If the last byte of the message is written, force a switch to R3 because some requests might force
                 * a reply through the FIFO which cannot be handled in GC or R0.
                 */
#ifndef IN_RING3
                if (pThis->iMessage == pThis->cMessage - 1)
                    return VINF_IOM_R3_MMIO_WRITE;
#endif
                pThis->aMessage[pThis->iMessage++] = u32;
#ifdef IN_RING3
                if (pThis->iMessage == pThis->cMessage)
                {
                    int rc = lsilogicR3ProcessMessageRequest(pThis, (PMptMessageHdr)pThis->aMessage, &pThis->ReplyBuffer);
                    AssertRC(rc);
                }
#endif
            }
            break;
        }
        case LSILOGIC_REG_HOST_INTR_STATUS:
        {
            /*
             * Clear the bits the guest wants except the system doorbell interrupt and the IO controller
             * status bit.
             * The former bit is always cleared no matter what the guest writes to the register and
             * the latter one is read only.
             */
            ASMAtomicAndU32(&pThis->uInterruptStatus, ~LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);

            /*
             * Check if there is still a doorbell function in progress. Set the
             * system doorbell interrupt bit again if it is.
             * We do not use lsilogicSetInterrupt here because the interrupt status
             * is updated afterwards anyway.
             */
            if (   (pThis->fDoorbellInProgress)
                && (pThis->cMessage == pThis->iMessage))
            {
                if (pThis->uNextReplyEntryRead == pThis->cReplySize)
                {
                    /* Reply finished. Reset doorbell in progress status. */
                    Log(("%s: Doorbell function finished\n", __FUNCTION__));
                    pThis->fDoorbellInProgress = false;
                }
                ASMAtomicOrU32(&pThis->uInterruptStatus, LSILOGIC_REG_HOST_INTR_STATUS_SYSTEM_DOORBELL);
            }

            lsilogicUpdateInterrupt(pThis);
            break;
        }
        case LSILOGIC_REG_HOST_INTR_MASK:
        {
            ASMAtomicWriteU32(&pThis->uInterruptMask, u32 & LSILOGIC_REG_HOST_INTR_MASK_W_MASK);
            lsilogicUpdateInterrupt(pThis);
            break;
        }
        case LSILOGIC_REG_WRITE_SEQUENCE:
        {
            if (pThis->fDiagnosticEnabled)
            {
                /* Any value will cause a reset and disabling access. */
                pThis->fDiagnosticEnabled = false;
                pThis->iDiagnosticAccess  = 0;
            }
            else if ((u32 & 0xf) == g_lsilogicDiagnosticAccess[pThis->iDiagnosticAccess])
            {
                pThis->iDiagnosticAccess++;
                if (pThis->iDiagnosticAccess == RT_ELEMENTS(g_lsilogicDiagnosticAccess))
                {
                    /*
                     * Key sequence successfully written. Enable access to diagnostic
                     * memory and register.
                     */
                    pThis->fDiagnosticEnabled = true;
                }
            }
            else
            {
                /* Wrong value written - reset to beginning. */
                pThis->iDiagnosticAccess = 0;
            }
            break;
        }
        case LSILOGIC_REG_HOST_DIAGNOSTIC:
        {
#ifndef IN_RING3
            return VINF_IOM_R3_IOPORT_WRITE;
#else
            if (u32 & LSILOGIC_REG_HOST_DIAGNOSTIC_RESET_ADAPTER)
            {
                lsilogicR3HardReset(pThis);
            }
            break;
#endif
        }
        default: /* Ignore. */
        {
            break;
        }
    }
    return VINF_SUCCESS;
}

/**
 * Reads the content of a register at a given offset.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   offReg      Offset of the register to read.
 * @param   pu32        Where to store the content of the register.
 */
static int lsilogicRegisterRead(PLSILOGICSCSI pThis, uint32_t offReg, uint32_t *pu32)
{
    int rc = VINF_SUCCESS;
    uint32_t u32 = 0;
    Assert(!(offReg & 3));

    /* Align to a 4 byte offset. */
    switch (offReg)
    {
        case LSILOGIC_REG_REPLY_QUEUE:
        {
            rc = PDMCritSectEnter(&pThis->ReplyPostQueueCritSect, VINF_IOM_R3_MMIO_READ);
            if (rc != VINF_SUCCESS)
                break;

            uint32_t idxReplyPostQueueWrite = ASMAtomicUoReadU32(&pThis->uReplyPostQueueNextEntryFreeWrite);
            uint32_t idxReplyPostQueueRead  = ASMAtomicUoReadU32(&pThis->uReplyPostQueueNextAddressRead);

            if (idxReplyPostQueueWrite != idxReplyPostQueueRead)
            {
                u32 = pThis->CTX_SUFF(pReplyPostQueueBase)[idxReplyPostQueueRead];
                idxReplyPostQueueRead++;
                idxReplyPostQueueRead %= pThis->cReplyQueueEntries;
                ASMAtomicWriteU32(&pThis->uReplyPostQueueNextAddressRead, idxReplyPostQueueRead);
            }
            else
            {
                /* The reply post queue is empty. Reset interrupt. */
                u32 = UINT32_C(0xffffffff);
                lsilogicClearInterrupt(pThis, LSILOGIC_REG_HOST_INTR_STATUS_REPLY_INTR);
            }
            PDMCritSectLeave(&pThis->ReplyPostQueueCritSect);

            Log(("%s: Returning address %#x\n", __FUNCTION__, u32));
            break;
        }
        case LSILOGIC_REG_DOORBELL:
        {
            u32  = LSILOGIC_REG_DOORBELL_SET_STATE(pThis->enmState);
            u32 |= LSILOGIC_REG_DOORBELL_SET_USED(pThis->fDoorbellInProgress);
            u32 |= LSILOGIC_REG_DOORBELL_SET_WHOINIT(pThis->enmWhoInit);
            /*
             * If there is a doorbell function in progress we pass the return value
             * instead of the status code. We transfer 16bit of the reply
             * during one read.
             */
            if (pThis->fDoorbellInProgress)
            {
                /* Return next 16bit value. */
                u32 |= pThis->ReplyBuffer.au16Reply[pThis->uNextReplyEntryRead++];
            }
            else
            {
                /* We return the status code of the I/O controller. */
                u32 |= pThis->u16IOCFaultCode;
            }
            break;
        }
        case LSILOGIC_REG_HOST_INTR_STATUS:
        {
            u32 = ASMAtomicReadU32(&pThis->uInterruptStatus);
            break;
        }
        case LSILOGIC_REG_HOST_INTR_MASK:
        {
            u32 = ASMAtomicReadU32(&pThis->uInterruptMask);
            break;
        }
        case LSILOGIC_REG_HOST_DIAGNOSTIC:
        {
            if (pThis->fDiagnosticEnabled)
                u32 = LSILOGIC_REG_HOST_DIAGNOSTIC_DRWE;
            else
                u32 = 0;
            break;
        }
        case LSILOGIC_REG_TEST_BASE_ADDRESS: /* The spec doesn't say anything about these registers, so we just ignore them */
        case LSILOGIC_REG_DIAG_RW_DATA:
        case LSILOGIC_REG_DIAG_RW_ADDRESS:
        default: /* Ignore. */
        {
            /** @todo LSILOGIC_REG_DIAG_* should return all F's when accessed by MMIO. We
             *        return 0.  Likely to apply to undefined offsets as well. */
            break;
        }
    }

    *pu32 = u32;
    LogFlowFunc(("pThis=%#p offReg=%#x u32=%#x\n", pThis, offReg, u32));
    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTOUT}
 */
PDMBOTHCBDECL(int) lsilogicIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PLSILOGICSCSI   pThis  = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t        offReg = Port - pThis->IOPortBase;
    int             rc;

    if (!(offReg & 3))
    {
        rc = lsilogicRegisterWrite(pThis, offReg, u32);
        if (rc == VINF_IOM_R3_MMIO_WRITE)
            rc = VINF_IOM_R3_IOPORT_WRITE;
    }
    else
    {
        Log(("lsilogicIOPortWrite: Ignoring misaligned write - offReg=%#x u32=%#x cb=%#x\n", offReg, u32, cb));
        rc = VINF_SUCCESS;
    }

    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTIN}
 */
PDMBOTHCBDECL(int) lsilogicIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PLSILOGICSCSI   pThis   = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t        offReg  = Port - pThis->IOPortBase;

    int rc = lsilogicRegisterRead(pThis, offReg & ~(uint32_t)3, pu32);
    if (rc == VINF_IOM_R3_MMIO_READ)
        rc = VINF_IOM_R3_IOPORT_READ;

    return rc;
}

/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) lsilogicMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PLSILOGICSCSI   pThis  = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t        offReg = GCPhysAddr - pThis->GCPhysMMIOBase;
    uint32_t        u32;
    int             rc;

    /* See comments in lsilogicR3Map regarding size and alignment. */
    if (cb == 4)
        u32 = *(uint32_t const *)pv;
    else
    {
        if (cb > 4)
            u32 = *(uint32_t const *)pv;
        else if (cb >= 2)
            u32 = *(uint16_t const *)pv;
        else
            u32 = *(uint8_t const *)pv;
        Log(("lsilogicMMIOWrite: Non-DWORD write access - offReg=%#x u32=%#x cb=%#x\n", offReg, u32, cb));
    }

    if (!(offReg & 3))
    {
        rc = lsilogicRegisterWrite(pThis, offReg, u32);
        if (rc == VINF_IOM_R3_MMIO_WRITE)
            rc = VINF_IOM_R3_IOPORT_WRITE;
    }
    else
    {
        Log(("lsilogicIOPortWrite: Ignoring misaligned write - offReg=%#x u32=%#x cb=%#x\n", offReg, u32, cb));
        rc = VINF_SUCCESS;
    }
    return rc;
}

/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
PDMBOTHCBDECL(int) lsilogicMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI   pThis  = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    uint32_t        offReg = GCPhysAddr - pThis->GCPhysMMIOBase;
    Assert(!(offReg & 3)); Assert(cb == 4);

    return lsilogicRegisterRead(pThis, offReg, (uint32_t *)pv);
}

PDMBOTHCBDECL(int) lsilogicDiagnosticWrite(PPDMDEVINS pDevIns, void *pvUser,
                                           RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("pThis=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pThis, GCPhysAddr, pv, cb, pv, cb));

    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) lsilogicDiagnosticRead(PPDMDEVINS pDevIns, void *pvUser,
                                          RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("pThis=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pThis, GCPhysAddr, pv, cb, pv, cb));

    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * Copies a contiguous buffer into the scatter gather list provided by the guest.
 *
 * @returns nothing
 * @param   pTaskState    Pointer to the task state which contains the SGL.
 * @param   pvBuf         Pointer to the buffer to copy.
 * @param   cbCopy        Number of bytes to copy.
 */
static void lsilogicR3ScatterGatherListCopyFromBuffer(PLSILOGICTASKSTATE pTaskState, void *pvBuf, size_t cbCopy)
{
    unsigned cSGEntry = 0;
    PRTSGSEG pSGEntry = &pTaskState->pSGListHead[cSGEntry];
    uint8_t *pu8Buf = (uint8_t *)pvBuf;

    while (cSGEntry < pTaskState->cSGListEntries)
    {
        size_t cbToCopy = (cbCopy < pSGEntry->cbSeg) ? cbCopy : pSGEntry->cbSeg;

        memcpy(pSGEntry->pvSeg, pu8Buf, cbToCopy);

        cbCopy -= cbToCopy;
        /* We finished. */
        if (!cbCopy)
            break;

        /* Advance the buffer. */
        pu8Buf += cbToCopy;

        /* Go to the next entry in the list. */
        pSGEntry++;
        cSGEntry++;
    }
}

/**
 * Copy a temporary buffer into a part of the guest scatter gather list
 * described by the given descriptor entry.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to write to which are unaligned.
 */
static void lsilogicR3CopyFromBufferIntoSGList(PPDMDEVINS pDevIns, PLSILOGICTASKSTATESGENTRY pSGInfo)
{
    RTGCPHYS GCPhysBuffer = pSGInfo->u.GCPhysAddrBufferUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    /* Copy into SG entry. */
    PDMDevHlpPCIPhysWrite(pDevIns, GCPhysBuffer, pSGInfo->pvBuf, pSGInfo->cbBuf);

}

/**
 * Copy a part of the guest scatter gather list into a temporary buffer.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to read from which are unaligned.
 */
static void lsilogicR3CopyFromSGListIntoBuffer(PPDMDEVINS pDevIns, PLSILOGICTASKSTATESGENTRY pSGInfo)
{
    RTGCPHYS GCPhysBuffer = pSGInfo->u.GCPhysAddrBufferUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    /* Copy into temporary buffer. */
    PDMDevHlpPhysRead(pDevIns, GCPhysBuffer, pSGInfo->pvBuf, pSGInfo->cbBuf);
}

static int lsilogicR3ScatterGatherListAllocate(PLSILOGICTASKSTATE pTaskState, uint32_t cSGList, uint32_t cSGInfo, uint32_t cbUnaligned)
{
    if (pTaskState->cSGListSize < cSGList)
    {
        /* The entries are not allocated yet or the number is too small. */
        if (pTaskState->cSGListSize)
            RTMemFree(pTaskState->pSGListHead);

        /* Allocate R3 scatter gather list. */
        pTaskState->pSGListHead = (PRTSGSEG)RTMemAllocZ(cSGList * sizeof(RTSGSEG));
        if (!pTaskState->pSGListHead)
            return VERR_NO_MEMORY;

        /* Reset usage statistics. */
        pTaskState->cSGListSize     = cSGList;
        pTaskState->cSGListEntries  = cSGList;
        pTaskState->cSGListTooBig = 0;
    }
    else if (pTaskState->cSGListSize > cSGList)
    {
        /*
         * The list is too big. Increment counter.
         * So that the destroying function can free
         * the list if it is too big too many times
         * in a row.
         */
        pTaskState->cSGListEntries = cSGList;
        pTaskState->cSGListTooBig++;
    }
    else
    {
        /*
         * Needed entries matches current size.
         * Reset counter.
         */
        pTaskState->cSGListEntries = cSGList;
        pTaskState->cSGListTooBig  = 0;
    }

    if (pTaskState->cSGInfoSize < cSGInfo)
    {
        /* The entries are not allocated yet or the number is too small. */
        if (pTaskState->cSGInfoSize)
            RTMemFree(pTaskState->paSGEntries);

        pTaskState->paSGEntries = (PLSILOGICTASKSTATESGENTRY)RTMemAllocZ(cSGInfo * sizeof(LSILOGICTASKSTATESGENTRY));
        if (!pTaskState->paSGEntries)
            return VERR_NO_MEMORY;

        /* Reset usage statistics. */
        pTaskState->cSGInfoSize = cSGInfo;
        pTaskState->cSGInfoEntries  = cSGInfo;
        pTaskState->cSGInfoTooBig = 0;
    }
    else if (pTaskState->cSGInfoSize > cSGInfo)
    {
        /*
         * The list is too big. Increment counter.
         * So that the destroying function can free
         * the list if it is too big too many times
         * in a row.
         */
        pTaskState->cSGInfoEntries = cSGInfo;
        pTaskState->cSGInfoTooBig++;
    }
    else
    {
        /*
         * Needed entries matches current size.
         * Reset counter.
         */
        pTaskState->cSGInfoEntries  = cSGInfo;
        pTaskState->cSGInfoTooBig = 0;
    }


    if (pTaskState->cbBufferUnaligned < cbUnaligned)
    {
        if (pTaskState->pvBufferUnaligned)
            RTMemPageFree(pTaskState->pvBufferUnaligned, pTaskState->cbBufferUnaligned);

        Log(("%s: Allocating buffer for unaligned segments cbUnaligned=%u\n", __FUNCTION__, cbUnaligned));

        pTaskState->pvBufferUnaligned = RTMemPageAlloc(cbUnaligned);
        if (!pTaskState->pvBufferUnaligned)
            return VERR_NO_MEMORY;

        pTaskState->cbBufferUnaligned = cbUnaligned;
    }

    /* Make debugging easier. */
# ifdef LOG_ENABLED
    memset(pTaskState->pSGListHead, 0, pTaskState->cSGListSize * sizeof(RTSGSEG));
    memset(pTaskState->paSGEntries, 0, pTaskState->cSGInfoSize * sizeof(LSILOGICTASKSTATESGENTRY));
    if (pTaskState->pvBufferUnaligned)
        memset(pTaskState->pvBufferUnaligned, 0, pTaskState->cbBufferUnaligned);
# endif
    return VINF_SUCCESS;
}

/**
 * Destroy a scatter gather list.
 *
 * @returns nothing.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   pTaskState  Pointer to the task state.
 */
static void lsilogicR3ScatterGatherListDestroy(PLSILOGICSCSI pThis, PLSILOGICTASKSTATE pTaskState)
{
    PPDMDEVINS                pDevIns     = pThis->CTX_SUFF(pDevIns);
    PLSILOGICTASKSTATESGENTRY pSGInfoCurr = pTaskState->paSGEntries;

    for (unsigned i = 0; i < pTaskState->cSGInfoEntries; i++)
    {
        if (pSGInfoCurr->fGuestMemory)
        {
            /* Release the lock. */
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &pSGInfoCurr->u.PageLock);
        }
        else if (!pSGInfoCurr->fBufferContainsData)
        {
            /* Copy the data into the guest segments now. */
            lsilogicR3CopyFromBufferIntoSGList(pThis->CTX_SUFF(pDevIns), pSGInfoCurr);
        }

        pSGInfoCurr++;
    }

    /* Free allocated memory if the list was too big too many times. */
    if (pTaskState->cSGListTooBig >= LSILOGIC_NR_OF_ALLOWED_BIGGER_LISTS)
        lsilogicR3TaskStateClear(pTaskState);
}

# ifdef LOG_ENABLED
/**
 * Dump an SG entry.
 *
 * @returns nothing.
 * @param   pSGEntry    Pointer to the SG entry to dump
 */
static void lsilogicDumpSGEntry(PMptSGEntryUnion pSGEntry)
{
    if (LogIsEnabled())
    {
        switch (pSGEntry->Simple32.u2ElementType)
        {
            case MPTSGENTRYTYPE_SIMPLE:
            {
                Log(("%s: Dumping info for SIMPLE SG entry:\n", __FUNCTION__));
                Log(("%s: u24Length=%u\n", __FUNCTION__, pSGEntry->Simple32.u24Length));
                Log(("%s: fEndOfList=%d\n", __FUNCTION__, pSGEntry->Simple32.fEndOfList));
                Log(("%s: f64BitAddress=%d\n", __FUNCTION__, pSGEntry->Simple32.f64BitAddress));
                Log(("%s: fBufferContainsData=%d\n", __FUNCTION__, pSGEntry->Simple32.fBufferContainsData));
                Log(("%s: fLocalAddress=%d\n", __FUNCTION__, pSGEntry->Simple32.fLocalAddress));
                Log(("%s: fEndOfBuffer=%d\n", __FUNCTION__, pSGEntry->Simple32.fEndOfBuffer));
                Log(("%s: fLastElement=%d\n", __FUNCTION__, pSGEntry->Simple32.fLastElement));
                Log(("%s: u32DataBufferAddressLow=%u\n", __FUNCTION__, pSGEntry->Simple32.u32DataBufferAddressLow));
                if (pSGEntry->Simple32.f64BitAddress)
                {
                    Log(("%s: u32DataBufferAddressHigh=%u\n", __FUNCTION__, pSGEntry->Simple64.u32DataBufferAddressHigh));
                    Log(("%s: GCDataBufferAddress=%RGp\n", __FUNCTION__,
                         ((uint64_t)pSGEntry->Simple64.u32DataBufferAddressHigh << 32)
                         | pSGEntry->Simple64.u32DataBufferAddressLow));
                }
                else
                    Log(("%s: GCDataBufferAddress=%RGp\n", __FUNCTION__, pSGEntry->Simple32.u32DataBufferAddressLow));

                break;
            }
            case MPTSGENTRYTYPE_CHAIN:
            {
                Log(("%s: Dumping info for CHAIN SG entry:\n", __FUNCTION__));
                Log(("%s: u16Length=%u\n", __FUNCTION__, pSGEntry->Chain.u16Length));
                Log(("%s: u8NExtChainOffset=%d\n", __FUNCTION__, pSGEntry->Chain.u8NextChainOffset));
                Log(("%s: f64BitAddress=%d\n", __FUNCTION__, pSGEntry->Chain.f64BitAddress));
                Log(("%s: fLocalAddress=%d\n", __FUNCTION__, pSGEntry->Chain.fLocalAddress));
                Log(("%s: u32SegmentAddressLow=%u\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressLow));
                Log(("%s: u32SegmentAddressHigh=%u\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressHigh));
                if (pSGEntry->Chain.f64BitAddress)
                    Log(("%s: GCSegmentAddress=%RGp\n", __FUNCTION__,
                        ((uint64_t)pSGEntry->Chain.u32SegmentAddressHigh << 32) | pSGEntry->Chain.u32SegmentAddressLow));
                else
                    Log(("%s: GCSegmentAddress=%RGp\n", __FUNCTION__, pSGEntry->Chain.u32SegmentAddressLow));
                break;
            }
        }
    }
}
# endif /* LOG_ENABLED */

/**
 * Create scatter gather list descriptors.
 *
 * @returns VBox status code.
 * @param   pThis           Pointer to the LsiLogic device state.
 * @param   pTaskState      Pointer to the task state.
 * @param   GCPhysSGLStart  Guest physical address of the first SG entry.
 * @param   uChainOffset    Offset in bytes from the beginning of the SGL segment to the chain element.
 * @thread  EMT
 */
static int lsilogicR3ScatterGatherListCreate(PLSILOGICSCSI pThis, PLSILOGICTASKSTATE pTaskState,
                                             RTGCPHYS GCPhysSGLStart, uint32_t uChainOffset)
{
    int                        rc           = VINF_SUCCESS;
    PPDMDEVINS                 pDevIns      = pThis->CTX_SUFF(pDevIns);
    PVM                        pVM          = PDMDevHlpGetVM(pDevIns);
    bool                       fUnaligned;     /* Flag whether the current buffer is unaligned. */
    uint32_t                   cbUnaligned;    /* Size of the unaligned buffers. */
    uint32_t                   cSGEntriesR3 = 0;
    uint32_t                   cSGInfo      = 0;
    uint32_t                   cbSegment    = 0;
    PLSILOGICTASKSTATESGENTRY  pSGInfoCurr  = NULL;
    uint8_t                   *pu8BufferUnalignedPos = NULL;
    uint8_t                   *pbBufferUnalignedSGInfoPos = NULL;
    uint32_t                   cbUnalignedComplete = 0;
    bool                       fDoMapping = false;
    bool                       fEndOfList;
    RTGCPHYS                   GCPhysSGEntryNext;
    RTGCPHYS                   GCPhysSegmentStart;
    uint32_t                   uChainOffsetNext;

    /*
     * Two passes - one to count needed scatter gather list entries and needed unaligned
     * buffers and one to actually map the SG list into R3.
     */
    for (int i = 0; i < 2; i++)
    {
        fUnaligned      = false;
        cbUnaligned     = 0;
        fEndOfList      = false;

        GCPhysSGEntryNext  = GCPhysSGLStart;
        uChainOffsetNext   = uChainOffset;
        GCPhysSegmentStart = GCPhysSGLStart;

        if (fDoMapping)
        {
            Log(("%s: cSGInfo=%u\n", __FUNCTION__, cSGInfo));

            /* The number of needed SG entries in R3 is known. Allocate needed memory. */
            rc = lsilogicR3ScatterGatherListAllocate(pTaskState, cSGInfo, cSGInfo, cbUnalignedComplete);
            AssertMsgRC(rc, ("Failed to allocate scatter gather array rc=%Rrc\n", rc));

            /* We are now able to map the pages into R3. */
            pSGInfoCurr = pTaskState->paSGEntries;
            /* Initialize first segment to remove the need for additional if checks later in the code. */
            pSGInfoCurr->fGuestMemory= false;
            pu8BufferUnalignedPos = (uint8_t *)pTaskState->pvBufferUnaligned;
            pbBufferUnalignedSGInfoPos = pu8BufferUnalignedPos;
        }

        /* Go through the list until we reach the end. */
        while (!fEndOfList)
        {
            bool fEndOfSegment = false;

            while (!fEndOfSegment)
            {
                MptSGEntryUnion SGEntry;

                Log(("%s: Reading SG entry from %RGp\n", __FUNCTION__, GCPhysSGEntryNext));

                /* Read the entry. */
                PDMDevHlpPhysRead(pDevIns, GCPhysSGEntryNext, &SGEntry, sizeof(MptSGEntryUnion));

# ifdef LOG_ENABLED
                lsilogicDumpSGEntry(&SGEntry);
# endif

                AssertMsg(SGEntry.Simple32.u2ElementType == MPTSGENTRYTYPE_SIMPLE, ("Invalid SG entry type\n"));

                /* Check if this is a zero element. */
                if (   !SGEntry.Simple32.u24Length
                    && SGEntry.Simple32.fEndOfList
                    && SGEntry.Simple32.fEndOfBuffer)
                {
                    pTaskState->cSGListEntries = 0;
                    pTaskState->cSGInfoEntries = 0;
                    return VINF_SUCCESS;
                }

                uint32_t cbDataToTransfer     = SGEntry.Simple32.u24Length;
                bool     fBufferContainsData  = !!SGEntry.Simple32.fBufferContainsData;
                RTGCPHYS GCPhysAddrDataBuffer = SGEntry.Simple32.u32DataBufferAddressLow;

                if (SGEntry.Simple32.f64BitAddress)
                {
                    GCPhysAddrDataBuffer |= ((uint64_t)SGEntry.Simple64.u32DataBufferAddressHigh) << 32;
                    GCPhysSGEntryNext += sizeof(MptSGEntrySimple64);
                }
                else
                    GCPhysSGEntryNext += sizeof(MptSGEntrySimple32);

                if (fDoMapping)
                {
                    pSGInfoCurr->fGuestMemory = false;
                    pSGInfoCurr->fBufferContainsData = fBufferContainsData;
                    pSGInfoCurr->cbBuf = cbDataToTransfer;
                    pSGInfoCurr->pvBuf = pbBufferUnalignedSGInfoPos;
                    pbBufferUnalignedSGInfoPos += cbDataToTransfer;
                    pSGInfoCurr->u.GCPhysAddrBufferUnaligned = GCPhysAddrDataBuffer;
                    if (fBufferContainsData)
                        lsilogicR3CopyFromSGListIntoBuffer(pDevIns, pSGInfoCurr);
                    pSGInfoCurr++;
                }
                else
                {
                    cbUnalignedComplete += cbDataToTransfer;
                    cSGInfo++;
                }

                /* Check if we reached the end of the list. */
                if (SGEntry.Simple32.fEndOfList)
                {
                    /* We finished. */
                    fEndOfSegment = true;
                    fEndOfList = true;
                }
                else if (SGEntry.Simple32.fLastElement)
                {
                    fEndOfSegment = true;
                }
            } /* while (!fEndOfSegment) */

            /* Get next chain element. */
            if (uChainOffsetNext)
            {
                MptSGEntryChain SGEntryChain;

                PDMDevHlpPhysRead(pDevIns, GCPhysSegmentStart + uChainOffsetNext, &SGEntryChain, sizeof(MptSGEntryChain));

                AssertMsg(SGEntryChain.u2ElementType == MPTSGENTRYTYPE_CHAIN, ("Invalid SG entry type\n"));

               /* Set the next address now. */
                GCPhysSGEntryNext = SGEntryChain.u32SegmentAddressLow;
                if (SGEntryChain.f64BitAddress)
                    GCPhysSGEntryNext |= ((uint64_t)SGEntryChain.u32SegmentAddressHigh) << 32;

                GCPhysSegmentStart = GCPhysSGEntryNext;
                uChainOffsetNext   = SGEntryChain.u8NextChainOffset * sizeof(uint32_t);
            }

        } /* while (!fEndOfList) */

        fDoMapping = true;
        if (fUnaligned)
            cbUnalignedComplete += cbUnaligned;
    }

    uint32_t    cSGEntries;
    PRTSGSEG    pSGEntryCurr = pTaskState->pSGListHead;
    pSGInfoCurr              = pTaskState->paSGEntries;

    /* Initialize first entry. */
    pSGEntryCurr->pvSeg = pSGInfoCurr->pvBuf;
    pSGEntryCurr->cbSeg = pSGInfoCurr->cbBuf;
    pSGInfoCurr++;
    cSGEntries = 1;

    /* Construct the scatter gather list. */
    for (unsigned i = 0; i < (pTaskState->cSGInfoEntries-1); i++)
    {
        if (pSGEntryCurr->cbSeg % 512 != 0)
        {
            AssertMsg((uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg == pSGInfoCurr->pvBuf,
                      ("Buffer ist not sector aligned but the buffer addresses are not adjacent\n"));

            pSGEntryCurr->cbSeg += pSGInfoCurr->cbBuf;
        }
        else
        {
            if (((uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg) == pSGInfoCurr->pvBuf)
            {
                pSGEntryCurr->cbSeg += pSGInfoCurr->cbBuf;
            }
            else
            {
                pSGEntryCurr++;
                cSGEntries++;
                pSGEntryCurr->pvSeg = pSGInfoCurr->pvBuf;
                pSGEntryCurr->cbSeg = pSGInfoCurr->cbBuf;
            }
        }

        pSGInfoCurr++;
    }

    pTaskState->cSGListEntries = cSGEntries;

    return rc;
}

/*
 * Disabled because the sense buffer provided by the LsiLogic driver for Windows XP
 * crosses page boundaries.
 */
# if 0
/**
 * Free the sense buffer.
 *
 * @returns nothing.
 * @param   pTaskState   Pointer to the task state.
 */
static void lsilogicFreeGCSenseBuffer(PLSILOGICSCSI pThis, PLSILOGICTASKSTATE pTaskState)
{
    PVM pVM = PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns));

    PGMPhysReleasePageMappingLock(pVM, &pTaskState->PageLockSense);
    pTaskState->pbSenseBuffer = NULL;
}

/**
 * Map the sense buffer into R3.
 *
 * @returns VBox status code.
 * @param   pTaskState    Pointer to the task state.
 * @note Current assumption is that the sense buffer is not scattered and does not cross a page boundary.
 */
static int lsilogicR3MapGCSenseBufferIntoR3(PLSILOGICSCSI pThis, PLSILOGICTASKSTATE pTaskState)
{
    int rc = VINF_SUCCESS;
    PPDMDEVINS pDevIns = pThis->CTX_SUFF(pDevIns);
    RTGCPHYS GCPhysAddrSenseBuffer;

    GCPhysAddrSenseBuffer = pTaskState->GuestRequest.SCSIIO.u32SenseBufferLowAddress;
    GCPhysAddrSenseBuffer |= ((uint64_t)pThis->u32SenseBufferHighAddr << 32);

# ifdef RT_STRICT
    uint32_t cbSenseBuffer = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
# endif
    RTGCPHYS GCPhysAddrSenseBufferBase = PAGE_ADDRESS(GCPhysAddrSenseBuffer);

    AssertMsg(GCPhysAddrSenseBuffer >= GCPhysAddrSenseBufferBase,
              ("Impossible GCPhysAddrSenseBuffer < GCPhysAddrSenseBufferBase\n"));

    /* Sanity checks for the assumption. */
    AssertMsg(((GCPhysAddrSenseBuffer + cbSenseBuffer) <= (GCPhysAddrSenseBufferBase + PAGE_SIZE)),
              ("Sense buffer crosses page boundary\n"));

    rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrSenseBufferBase, (void **)&pTaskState->pbSenseBuffer, &pTaskState->PageLockSense);
    AssertMsgRC(rc, ("Mapping sense buffer failed rc=%Rrc\n", rc));

    /* Correct start address of the sense buffer. */
    pTaskState->pbSenseBuffer += (GCPhysAddrSenseBuffer - GCPhysAddrSenseBufferBase);

    return rc;
}
# endif

# ifdef LOG_ENABLED
static void lsilogicR3DumpSCSIIORequest(PMptSCSIIORequest pSCSIIORequest)
{
    if (LogIsEnabled())
    {
        Log(("%s: u8TargetID=%d\n", __FUNCTION__, pSCSIIORequest->u8TargetID));
        Log(("%s: u8Bus=%d\n", __FUNCTION__, pSCSIIORequest->u8Bus));
        Log(("%s: u8ChainOffset=%d\n", __FUNCTION__, pSCSIIORequest->u8ChainOffset));
        Log(("%s: u8Function=%d\n", __FUNCTION__, pSCSIIORequest->u8Function));
        Log(("%s: u8CDBLength=%d\n", __FUNCTION__, pSCSIIORequest->u8CDBLength));
        Log(("%s: u8SenseBufferLength=%d\n", __FUNCTION__, pSCSIIORequest->u8SenseBufferLength));
        Log(("%s: u8MessageFlags=%d\n", __FUNCTION__, pSCSIIORequest->u8MessageFlags));
        Log(("%s: u32MessageContext=%#x\n", __FUNCTION__, pSCSIIORequest->u32MessageContext));
        for (unsigned i = 0; i < RT_ELEMENTS(pSCSIIORequest->au8LUN); i++)
            Log(("%s: u8LUN[%d]=%d\n", __FUNCTION__, i, pSCSIIORequest->au8LUN[i]));
        Log(("%s: u32Control=%#x\n", __FUNCTION__, pSCSIIORequest->u32Control));
        for (unsigned i = 0; i < RT_ELEMENTS(pSCSIIORequest->au8CDB); i++)
            Log(("%s: u8CDB[%d]=%d\n", __FUNCTION__, i, pSCSIIORequest->au8CDB[i]));
        Log(("%s: u32DataLength=%#x\n", __FUNCTION__, pSCSIIORequest->u32DataLength));
        Log(("%s: u32SenseBufferLowAddress=%#x\n", __FUNCTION__, pSCSIIORequest->u32SenseBufferLowAddress));
    }
}
# endif

static void lsilogicR3WarningDiskFull(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("LsiLogic#%d: Host disk full\n", pDevIns->iInstance));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevLsiLogic_DISKFULL",
                                    N_("Host system reported disk full. VM execution is suspended. You can resume after freeing some space"));
    AssertRC(rc);
}

static void lsilogicR3WarningFileTooBig(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("LsiLogic#%d: File too big\n", pDevIns->iInstance));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevLsiLogic_FILETOOBIG",
                                    N_("Host system reported that the file size limit of the host file system has been exceeded. VM execution is suspended. You need to move your virtual hard disk to a filesystem which allows bigger files"));
    AssertRC(rc);
}

static void lsilogicR3WarningISCSI(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("LsiLogic#%d: iSCSI target unavailable\n", pDevIns->iInstance));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevLsiLogic_ISCSIDOWN",
                                    N_("The iSCSI target has stopped responding. VM execution is suspended. You can resume when it is available again"));
    AssertRC(rc);
}

static void lsilogicR3WarningUnknown(PPDMDEVINS pDevIns, int rc)
{
    int rc2;
    LogRel(("LsiLogic#%d: Unknown but recoverable error has occurred (rc=%Rrc)\n", pDevIns->iInstance, rc));
    rc2 = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevLsiLogic_UNKNOWN",
                                     N_("An unknown but recoverable I/O error has occurred (rc=%Rrc). VM execution is suspended. You can resume when the error is fixed"), rc);
    AssertRC(rc2);
}

static void lsilogicR3RedoSetWarning(PLSILOGICSCSI pThis, int rc)
{
    if (rc == VERR_DISK_FULL)
        lsilogicR3WarningDiskFull(pThis->CTX_SUFF(pDevIns));
    else if (rc == VERR_FILE_TOO_BIG)
        lsilogicR3WarningFileTooBig(pThis->CTX_SUFF(pDevIns));
    else if (rc == VERR_BROKEN_PIPE || rc == VERR_NET_CONNECTION_REFUSED)
    {
        /* iSCSI connection abort (first error) or failure to reestablish
         * connection (second error). Pause VM. On resume we'll retry. */
        lsilogicR3WarningISCSI(pThis->CTX_SUFF(pDevIns));
    }
    else
        lsilogicR3WarningUnknown(pThis->CTX_SUFF(pDevIns), rc);
}

/**
 * Processes a SCSI I/O request by setting up the request
 * and sending it to the underlying SCSI driver.
 * Steps needed to complete request are done in the
 * callback called by the driver below upon completion of
 * the request.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 * @param   pTaskState  Pointer to the task state data.
 */
static int lsilogicR3ProcessSCSIIORequest(PLSILOGICSCSI pThis, PLSILOGICTASKSTATE pTaskState)
{
    int rc = VINF_SUCCESS;

# ifdef LOG_ENABLED
    lsilogicR3DumpSCSIIORequest(&pTaskState->GuestRequest.SCSIIO);
# endif

    pTaskState->fBIOS = false;

    if (RT_LIKELY(   (pTaskState->GuestRequest.SCSIIO.u8TargetID < pThis->cDeviceStates)
                  && (pTaskState->GuestRequest.SCSIIO.u8Bus == 0)))
    {
        PLSILOGICDEVICE pTargetDevice;
        pTargetDevice = &pThis->paDeviceStates[pTaskState->GuestRequest.SCSIIO.u8TargetID];

        if (pTargetDevice->pDrvBase)
        {
            uint32_t uChainOffset;

            /* Create Scatter gather list. */
            uChainOffset = pTaskState->GuestRequest.SCSIIO.u8ChainOffset;

            if (uChainOffset)
                uChainOffset = uChainOffset * sizeof(uint32_t) - sizeof(MptSCSIIORequest);

            rc = lsilogicR3ScatterGatherListCreate(pThis, pTaskState,
                                                 pTaskState->GCPhysMessageFrameAddr + sizeof(MptSCSIIORequest),
                                                 uChainOffset);
            AssertRC(rc);

# if 0
            /* Map sense buffer. */
            rc = lsilogicR3MapGCSenseBufferIntoR3(pThis, pTaskState);
            AssertRC(rc);
# endif

            /* Setup the SCSI request. */
            pTaskState->pTargetDevice                        = pTargetDevice;
            pTaskState->PDMScsiRequest.uLogicalUnit          = pTaskState->GuestRequest.SCSIIO.au8LUN[1];

            uint8_t uDataDirection = MPT_SCSIIO_REQUEST_CONTROL_TXDIR_GET(pTaskState->GuestRequest.SCSIIO.u32Control);

            if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_NONE)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_NONE;
            else if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_WRITE)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_TO_DEVICE;
            else if (uDataDirection == MPT_SCSIIO_REQUEST_CONTROL_TXDIR_READ)
                pTaskState->PDMScsiRequest.uDataDirection    = PDMSCSIREQUESTTXDIR_FROM_DEVICE;

            pTaskState->PDMScsiRequest.cbCDB                 = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
            pTaskState->PDMScsiRequest.pbCDB                 = pTaskState->GuestRequest.SCSIIO.au8CDB;
            pTaskState->PDMScsiRequest.cbScatterGather       = pTaskState->GuestRequest.SCSIIO.u32DataLength;
            pTaskState->PDMScsiRequest.cScatterGatherEntries = pTaskState->cSGListEntries;
            pTaskState->PDMScsiRequest.paScatterGatherHead   = pTaskState->pSGListHead;
            pTaskState->PDMScsiRequest.cbSenseBuffer         = sizeof(pTaskState->abSenseBuffer);
            memset(pTaskState->abSenseBuffer, 0, pTaskState->PDMScsiRequest.cbSenseBuffer);
            pTaskState->PDMScsiRequest.pbSenseBuffer         = pTaskState->abSenseBuffer;
            pTaskState->PDMScsiRequest.pvUser                = pTaskState;

            ASMAtomicIncU32(&pTargetDevice->cOutstandingRequests);
            rc = pTargetDevice->pDrvSCSIConnector->pfnSCSIRequestSend(pTargetDevice->pDrvSCSIConnector, &pTaskState->PDMScsiRequest);
            AssertMsgRC(rc, ("Sending request to SCSI layer failed rc=%Rrc\n", rc));
            return VINF_SUCCESS;
        }
        else
        {
            /* Device is not present report SCSI selection timeout. */
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_DEVICE_NOT_THERE;
        }
    }
    else
    {
        /* Report out of bounds target ID or bus. */
        if (pTaskState->GuestRequest.SCSIIO.u8Bus != 0)
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_BUS;
        else
            pTaskState->IOCReply.SCSIIOError.u16IOCStatus = MPT_SCSI_IO_ERROR_IOCSTATUS_INVALID_TARGETID;
    }

    static int g_cLogged = 0;

    if (g_cLogged++ < MAX_REL_LOG_ERRORS)
    {
        LogRel(("LsiLogic#%d: %d/%d (Bus/Target) doesn't exist\n", pThis->CTX_SUFF(pDevIns)->iInstance,
                pTaskState->GuestRequest.SCSIIO.u8TargetID, pTaskState->GuestRequest.SCSIIO.u8Bus));
        /* Log the CDB too  */
        LogRel(("LsiLogic#%d: Guest issued CDB {%#x",
                pThis->CTX_SUFF(pDevIns)->iInstance, pTaskState->GuestRequest.SCSIIO.au8CDB[0]));
        for (unsigned i = 1; i < pTaskState->GuestRequest.SCSIIO.u8CDBLength; i++)
            LogRel((", %#x", pTaskState->GuestRequest.SCSIIO.au8CDB[i]));
        LogRel(("}\n"));
    }

    /* The rest is equal to both errors. */
    pTaskState->IOCReply.SCSIIOError.u8TargetID          = pTaskState->GuestRequest.SCSIIO.u8TargetID;
    pTaskState->IOCReply.SCSIIOError.u8Bus               = pTaskState->GuestRequest.SCSIIO.u8Bus;
    pTaskState->IOCReply.SCSIIOError.u8MessageLength     = sizeof(MptSCSIIOErrorReply) / 4;
    pTaskState->IOCReply.SCSIIOError.u8Function          = pTaskState->GuestRequest.SCSIIO.u8Function;
    pTaskState->IOCReply.SCSIIOError.u8CDBLength         = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
    pTaskState->IOCReply.SCSIIOError.u8SenseBufferLength = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
    pTaskState->IOCReply.SCSIIOError.u32MessageContext   = pTaskState->GuestRequest.SCSIIO.u32MessageContext;
    pTaskState->IOCReply.SCSIIOError.u8SCSIStatus        = SCSI_STATUS_OK;
    pTaskState->IOCReply.SCSIIOError.u8SCSIState         = MPT_SCSI_IO_ERROR_SCSI_STATE_TERMINATED;
    pTaskState->IOCReply.SCSIIOError.u32IOCLogInfo       = 0;
    pTaskState->IOCReply.SCSIIOError.u32TransferCount    = 0;
    pTaskState->IOCReply.SCSIIOError.u32SenseCount       = 0;
    pTaskState->IOCReply.SCSIIOError.u32ResponseInfo     = 0;

    lsilogicFinishAddressReply(pThis, &pTaskState->IOCReply, false);
    RTMemCacheFree(pThis->hTaskCache, pTaskState);

    return rc;
}


/**
 * @interface_method_impl{PDMISCSIPORT,pfnSCSIRequestCompleted}
 */
static DECLCALLBACK(int) lsilogicR3DeviceSCSIRequestCompleted(PPDMISCSIPORT pInterface, PPDMSCSIREQUEST pSCSIRequest,
                                                              int rcCompletion, bool fRedo, int rcReq)
{
    PLSILOGICTASKSTATE pTaskState      = (PLSILOGICTASKSTATE)pSCSIRequest->pvUser;
    PLSILOGICDEVICE    pLsiLogicDevice = pTaskState->pTargetDevice;
    PLSILOGICSCSI      pThis       = pLsiLogicDevice->CTX_SUFF(pLsiLogic);

    /* If the task failed but it is possible to redo it again after a suspend
     * add it to the list. */
    if (fRedo)
    {
        if (!pTaskState->fBIOS)
            lsilogicR3ScatterGatherListDestroy(pThis, pTaskState);

        /* Add to the list. */
        do
        {
            pTaskState->pRedoNext = ASMAtomicReadPtrT(&pThis->pTasksRedoHead, PLSILOGICTASKSTATE);
        } while (!ASMAtomicCmpXchgPtr(&pThis->pTasksRedoHead, pTaskState, pTaskState->pRedoNext));

        /* Suspend the VM if not done already. */
        if (!ASMAtomicXchgBool(&pThis->fRedo, true))
            lsilogicR3RedoSetWarning(pThis, rcReq);
    }
    else
    {
        if (RT_UNLIKELY(pTaskState->fBIOS))
        {
            int rc = vboxscsiRequestFinished(&pThis->VBoxSCSI, pSCSIRequest, rcCompletion);
            AssertMsgRC(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc));
        }
        else
        {
# if 0
            lsilogicFreeGCSenseBuffer(pThis, pTaskState);
# else
            RTGCPHYS GCPhysAddrSenseBuffer;

            GCPhysAddrSenseBuffer = pTaskState->GuestRequest.SCSIIO.u32SenseBufferLowAddress;
            GCPhysAddrSenseBuffer |= ((uint64_t)pThis->u32SenseBufferHighAddr << 32);

            /* Copy the sense buffer over. */
            PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), GCPhysAddrSenseBuffer, pTaskState->abSenseBuffer,
                                  RT_UNLIKELY(  pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength
                                              < pTaskState->PDMScsiRequest.cbSenseBuffer)
                                  ? pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength
                                  : pTaskState->PDMScsiRequest.cbSenseBuffer);
# endif
            lsilogicR3ScatterGatherListDestroy(pThis, pTaskState);


            if (RT_LIKELY(rcCompletion == SCSI_STATUS_OK))
                lsilogicR3FinishContextReply(pThis, pTaskState->GuestRequest.SCSIIO.u32MessageContext);
            else
            {
                /* The SCSI target encountered an error during processing post a reply. */
                memset(&pTaskState->IOCReply, 0, sizeof(MptReplyUnion));
                pTaskState->IOCReply.SCSIIOError.u8TargetID          = pTaskState->GuestRequest.SCSIIO.u8TargetID;
                pTaskState->IOCReply.SCSIIOError.u8Bus               = pTaskState->GuestRequest.SCSIIO.u8Bus;
                pTaskState->IOCReply.SCSIIOError.u8MessageLength     = 8;
                pTaskState->IOCReply.SCSIIOError.u8Function          = pTaskState->GuestRequest.SCSIIO.u8Function;
                pTaskState->IOCReply.SCSIIOError.u8CDBLength         = pTaskState->GuestRequest.SCSIIO.u8CDBLength;
                pTaskState->IOCReply.SCSIIOError.u8SenseBufferLength = pTaskState->GuestRequest.SCSIIO.u8SenseBufferLength;
                pTaskState->IOCReply.SCSIIOError.u8MessageFlags      = pTaskState->GuestRequest.SCSIIO.u8MessageFlags;
                pTaskState->IOCReply.SCSIIOError.u32MessageContext   = pTaskState->GuestRequest.SCSIIO.u32MessageContext;
                pTaskState->IOCReply.SCSIIOError.u8SCSIStatus        = rcCompletion;
                pTaskState->IOCReply.SCSIIOError.u8SCSIState         = MPT_SCSI_IO_ERROR_SCSI_STATE_AUTOSENSE_VALID;
                pTaskState->IOCReply.SCSIIOError.u16IOCStatus        = 0;
                pTaskState->IOCReply.SCSIIOError.u32IOCLogInfo       = 0;
                pTaskState->IOCReply.SCSIIOError.u32TransferCount    = 0;
                pTaskState->IOCReply.SCSIIOError.u32SenseCount       = sizeof(pTaskState->abSenseBuffer);
                pTaskState->IOCReply.SCSIIOError.u32ResponseInfo     = 0;

                lsilogicFinishAddressReply(pThis, &pTaskState->IOCReply, true);
            }
        }

        RTMemCacheFree(pThis->hTaskCache, pTaskState);
    }

    ASMAtomicDecU32(&pLsiLogicDevice->cOutstandingRequests);

    if (pLsiLogicDevice->cOutstandingRequests == 0 && pThis->fSignalIdle)
        PDMDevHlpAsyncNotificationCompleted(pThis->pDevInsR3);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMISCSIPORT,pfnQueryDeviceLocation}
 */
static DECLCALLBACK(int) lsilogicR3QueryDeviceLocation(PPDMISCSIPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PLSILOGICDEVICE pLsiLogicDevice = RT_FROM_MEMBER(pInterface, LSILOGICDEVICE, ISCSIPort);
    PPDMDEVINS pDevIns = pLsiLogicDevice->CTX_SUFF(pLsiLogic)->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piLUN, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piLUN = pLsiLogicDevice->iLUN;

    return VINF_SUCCESS;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationIOUnitPageGetFromNumber(PLSILOGICSCSI pThis,
                                                          PMptConfigurationPagesSupported pPages,
                                                          uint8_t u8PageNumber,
                                                          PMptConfigurationPageHeader *ppPageHeader,
                                                          uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertPtr(ppPageHeader); Assert(ppbPageData);

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->IOUnitPage0.u.fields.Header;
            *ppbPageData  =  pPages->IOUnitPage0.u.abPageData;
            *pcbPage      = sizeof(pPages->IOUnitPage0);
            break;
        case 1:
            *ppPageHeader = &pPages->IOUnitPage1.u.fields.Header;
            *ppbPageData  =  pPages->IOUnitPage1.u.abPageData;
            *pcbPage      = sizeof(pPages->IOUnitPage1);
            break;
        case 2:
            *ppPageHeader = &pPages->IOUnitPage2.u.fields.Header;
            *ppbPageData  =  pPages->IOUnitPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->IOUnitPage2);
            break;
        case 3:
            *ppPageHeader = &pPages->IOUnitPage3.u.fields.Header;
            *ppbPageData  =  pPages->IOUnitPage3.u.abPageData;
            *pcbPage      = sizeof(pPages->IOUnitPage3);
            break;
        case 4:
            *ppPageHeader = &pPages->IOUnitPage4.u.fields.Header;
            *ppbPageData  =  pPages->IOUnitPage4.u.abPageData;
            *pcbPage      = sizeof(pPages->IOUnitPage4);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationIOCPageGetFromNumber(PLSILOGICSCSI pThis,
                                                       PMptConfigurationPagesSupported pPages,
                                                       uint8_t u8PageNumber,
                                                       PMptConfigurationPageHeader *ppPageHeader,
                                                       uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertPtr(ppPageHeader); Assert(ppbPageData);

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->IOCPage0.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage0.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage0);
            break;
        case 1:
            *ppPageHeader = &pPages->IOCPage1.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage1.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage1);
            break;
        case 2:
            *ppPageHeader = &pPages->IOCPage2.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage2);
            break;
        case 3:
            *ppPageHeader = &pPages->IOCPage3.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage3.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage3);
            break;
        case 4:
            *ppPageHeader = &pPages->IOCPage4.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage4.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage4);
            break;
        case 6:
            *ppPageHeader = &pPages->IOCPage6.u.fields.Header;
            *ppbPageData  =  pPages->IOCPage6.u.abPageData;
            *pcbPage      = sizeof(pPages->IOCPage6);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationManufacturingPageGetFromNumber(PLSILOGICSCSI pThis,
                                                                 PMptConfigurationPagesSupported pPages,
                                                                 uint8_t u8PageNumber,
                                                                 PMptConfigurationPageHeader *ppPageHeader,
                                                                 uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertPtr(ppPageHeader); Assert(ppbPageData);

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->ManufacturingPage0.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage0.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage0);
            break;
        case 1:
            *ppPageHeader = &pPages->ManufacturingPage1.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage1.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage1);
            break;
        case 2:
            *ppPageHeader = &pPages->ManufacturingPage2.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage2);
            break;
        case 3:
            *ppPageHeader = &pPages->ManufacturingPage3.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage3.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage3);
            break;
        case 4:
            *ppPageHeader = &pPages->ManufacturingPage4.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage4.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage4);
            break;
        case 5:
            *ppPageHeader = &pPages->ManufacturingPage5.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage5.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage5);
            break;
        case 6:
            *ppPageHeader = &pPages->ManufacturingPage6.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage6.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage6);
            break;
        case 7:
            if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
            {
                *ppPageHeader = &pPages->u.SasPages.pManufacturingPage7->u.fields.Header;
                *ppbPageData  =  pPages->u.SasPages.pManufacturingPage7->u.abPageData;
                *pcbPage      = pPages->u.SasPages.cbManufacturingPage7;
            }
            else
                rc = VERR_NOT_FOUND;
            break;
        case 8:
            *ppPageHeader = &pPages->ManufacturingPage8.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage8.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage8);
            break;
        case 9:
            *ppPageHeader = &pPages->ManufacturingPage9.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage9.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage9);
            break;
        case 10:
            *ppPageHeader = &pPages->ManufacturingPage10.u.fields.Header;
            *ppbPageData  =  pPages->ManufacturingPage10.u.abPageData;
            *pcbPage      = sizeof(pPages->ManufacturingPage10);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationBiosPageGetFromNumber(PLSILOGICSCSI pThis,
                                                        PMptConfigurationPagesSupported pPages,
                                                        uint8_t u8PageNumber,
                                                        PMptConfigurationPageHeader *ppPageHeader,
                                                        uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    AssertPtr(ppPageHeader); Assert(ppbPageData);

    switch (u8PageNumber)
    {
        case 1:
            *ppPageHeader = &pPages->BIOSPage1.u.fields.Header;
            *ppbPageData  =  pPages->BIOSPage1.u.abPageData;
            *pcbPage      = sizeof(pPages->BIOSPage1);
            break;
        case 2:
            *ppPageHeader = &pPages->BIOSPage2.u.fields.Header;
            *ppbPageData  =  pPages->BIOSPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->BIOSPage2);
            break;
        case 4:
            *ppPageHeader = &pPages->BIOSPage4.u.fields.Header;
            *ppbPageData  =  pPages->BIOSPage4.u.abPageData;
            *pcbPage      = sizeof(pPages->BIOSPage4);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationSCSISPIPortPageGetFromNumber(PLSILOGICSCSI pThis,
                                                               PMptConfigurationPagesSupported pPages,
                                                               uint8_t u8Port,
                                                               uint8_t u8PageNumber,
                                                               PMptConfigurationPageHeader *ppPageHeader,
                                                               uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;
    AssertPtr(ppPageHeader); Assert(ppbPageData);


    if (u8Port >= RT_ELEMENTS(pPages->u.SpiPages.aPortPages))
        return VERR_NOT_FOUND;

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage0.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage0.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage0);
            break;
        case 1:
            *ppPageHeader = &pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage1.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage1.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage1);
            break;
        case 2:
            *ppPageHeader = &pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage2.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aPortPages[u8Port].SCSISPIPortPage2);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Return the configuration page header and data
 * which matches the given page type and number.
 *
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   u8PageNumber  Number of the page to get.
 * @param   ppPageHeader  Where to store the pointer to the page header.
 * @param   ppbPageData   Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationSCSISPIDevicePageGetFromNumber(PLSILOGICSCSI pThis,
                                                                 PMptConfigurationPagesSupported pPages,
                                                                 uint8_t u8Bus,
                                                                 uint8_t u8TargetID, uint8_t u8PageNumber,
                                                                 PMptConfigurationPageHeader *ppPageHeader,
                                                                 uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;
    AssertPtr(ppPageHeader); Assert(ppbPageData);

    if (u8Bus >= RT_ELEMENTS(pPages->u.SpiPages.aBuses))
        return VERR_NOT_FOUND;

    if (u8TargetID >= RT_ELEMENTS(pPages->u.SpiPages.aBuses[u8Bus].aDevicePages))
        return VERR_NOT_FOUND;

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage0);
            break;
        case 1:
            *ppPageHeader = &pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage1);
            break;
        case 2:
            *ppPageHeader = &pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage2);
            break;
        case 3:
            *ppPageHeader = &pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3.u.fields.Header;
            *ppbPageData  =  pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SpiPages.aBuses[u8Bus].aDevicePages[u8TargetID].SCSISPIDevicePage3);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

static int lsilogicR3ConfigurationSASIOUnitPageGetFromNumber(PLSILOGICSCSI pThis,
                                                             PMptConfigurationPagesSupported pPages,
                                                             uint8_t u8PageNumber,
                                                             PMptExtendedConfigurationPageHeader *ppPageHeader,
                                                             uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    switch (u8PageNumber)
    {
        case 0:
            *ppPageHeader = &pPages->u.SasPages.pSASIOUnitPage0->u.fields.ExtHeader;
            *ppbPageData  = pPages->u.SasPages.pSASIOUnitPage0->u.abPageData;
            *pcbPage      = pPages->u.SasPages.cbSASIOUnitPage0;
            break;
        case 1:
            *ppPageHeader = &pPages->u.SasPages.pSASIOUnitPage1->u.fields.ExtHeader;
            *ppbPageData  =  pPages->u.SasPages.pSASIOUnitPage1->u.abPageData;
            *pcbPage      = pPages->u.SasPages.cbSASIOUnitPage1;
            break;
        case 2:
            *ppPageHeader = &pPages->u.SasPages.SASIOUnitPage2.u.fields.ExtHeader;
            *ppbPageData  =  pPages->u.SasPages.SASIOUnitPage2.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SasPages.SASIOUnitPage2);
            break;
        case 3:
            *ppPageHeader = &pPages->u.SasPages.SASIOUnitPage3.u.fields.ExtHeader;
            *ppbPageData  =  pPages->u.SasPages.SASIOUnitPage3.u.abPageData;
            *pcbPage      = sizeof(pPages->u.SasPages.SASIOUnitPage3);
            break;
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

static int lsilogicR3ConfigurationSASPHYPageGetFromNumber(PLSILOGICSCSI pThis,
                                                          PMptConfigurationPagesSupported pPages,
                                                          uint8_t u8PageNumber,
                                                          MptConfigurationPageAddress PageAddress,
                                                          PMptExtendedConfigurationPageHeader *ppPageHeader,
                                                          uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;
    uint8_t uAddressForm = MPT_CONFIGURATION_PAGE_ADDRESS_GET_SAS_FORM(PageAddress);
    PMptConfigurationPagesSas pPagesSas = &pPages->u.SasPages;
    PMptPHY pPHYPages = NULL;

    Log(("Address form %d\n", uAddressForm));

    if (uAddressForm == 0) /* PHY number */
    {
        uint8_t u8PhyNumber = PageAddress.SASPHY.Form0.u8PhyNumber;

        Log(("PHY number %d\n", u8PhyNumber));

        if (u8PhyNumber >= pPagesSas->cPHYs)
            return VERR_NOT_FOUND;

        pPHYPages = &pPagesSas->paPHYs[u8PhyNumber];
    }
    else if (uAddressForm == 1) /* Index form */
    {
        uint16_t u16Index = PageAddress.SASPHY.Form1.u16Index;

        Log(("PHY index %d\n", u16Index));

        if (u16Index >= pPagesSas->cPHYs)
            return VERR_NOT_FOUND;

        pPHYPages = &pPagesSas->paPHYs[u16Index];
    }
    else
        rc = VERR_NOT_FOUND; /* Correct? */

    if (pPHYPages)
    {
        switch (u8PageNumber)
        {
            case 0:
                *ppPageHeader = &pPHYPages->SASPHYPage0.u.fields.ExtHeader;
                *ppbPageData  = pPHYPages->SASPHYPage0.u.abPageData;
                *pcbPage      = sizeof(pPHYPages->SASPHYPage0);
                break;
            case 1:
                *ppPageHeader = &pPHYPages->SASPHYPage1.u.fields.ExtHeader;
                *ppbPageData  =  pPHYPages->SASPHYPage1.u.abPageData;
                *pcbPage      = sizeof(pPHYPages->SASPHYPage1);
                break;
            default:
                rc = VERR_NOT_FOUND;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

static int lsilogicR3ConfigurationSASDevicePageGetFromNumber(PLSILOGICSCSI pThis,
                                                             PMptConfigurationPagesSupported pPages,
                                                             uint8_t u8PageNumber,
                                                             MptConfigurationPageAddress PageAddress,
                                                             PMptExtendedConfigurationPageHeader *ppPageHeader,
                                                             uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;
    uint8_t uAddressForm = MPT_CONFIGURATION_PAGE_ADDRESS_GET_SAS_FORM(PageAddress);
    PMptConfigurationPagesSas pPagesSas = &pPages->u.SasPages;
    PMptSASDevice pSASDevice = NULL;

    Log(("Address form %d\n", uAddressForm));

    if (uAddressForm == 0)
    {
        uint16_t u16Handle = PageAddress.SASDevice.Form0And2.u16Handle;

        Log(("Get next handle %#x\n", u16Handle));

        pSASDevice = pPagesSas->pSASDeviceHead;

        /* Get the first device? */
        if (u16Handle != 0xffff)
        {
            /* No, search for the right one. */

            while (   pSASDevice
                   && pSASDevice->SASDevicePage0.u.fields.u16DevHandle != u16Handle)
                pSASDevice = pSASDevice->pNext;

            if (pSASDevice)
                pSASDevice = pSASDevice->pNext;
        }
    }
    else if (uAddressForm == 1)
    {
        uint8_t u8TargetID = PageAddress.SASDevice.Form1.u8TargetID;
        uint8_t u8Bus      = PageAddress.SASDevice.Form1.u8Bus;

        Log(("u8TargetID=%d u8Bus=%d\n", u8TargetID, u8Bus));

        pSASDevice = pPagesSas->pSASDeviceHead;

        while (   pSASDevice
               && (   pSASDevice->SASDevicePage0.u.fields.u8TargetID != u8TargetID
                   || pSASDevice->SASDevicePage0.u.fields.u8Bus != u8Bus))
            pSASDevice = pSASDevice->pNext;
    }
    else if (uAddressForm == 2)
    {
        uint16_t u16Handle = PageAddress.SASDevice.Form0And2.u16Handle;

        Log(("Handle %#x\n", u16Handle));

        pSASDevice = pPagesSas->pSASDeviceHead;

        while (   pSASDevice
               && pSASDevice->SASDevicePage0.u.fields.u16DevHandle != u16Handle)
            pSASDevice = pSASDevice->pNext;
    }

    if (pSASDevice)
    {
        switch (u8PageNumber)
        {
            case 0:
                *ppPageHeader = &pSASDevice->SASDevicePage0.u.fields.ExtHeader;
                *ppbPageData  =  pSASDevice->SASDevicePage0.u.abPageData;
                *pcbPage      = sizeof(pSASDevice->SASDevicePage0);
                break;
            case 1:
                *ppPageHeader = &pSASDevice->SASDevicePage1.u.fields.ExtHeader;
                *ppbPageData  =  pSASDevice->SASDevicePage1.u.abPageData;
                *pcbPage      = sizeof(pSASDevice->SASDevicePage1);
                break;
            case 2:
                *ppPageHeader = &pSASDevice->SASDevicePage2.u.fields.ExtHeader;
                *ppbPageData  =  pSASDevice->SASDevicePage2.u.abPageData;
                *pcbPage      = sizeof(pSASDevice->SASDevicePage2);
                break;
            default:
                rc = VERR_NOT_FOUND;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}

/**
 * Returns the extended configuration page header and data.
 * @returns VINF_SUCCESS if successful
 *          VERR_NOT_FOUND if the requested page could be found.
 * @param   pThis               Pointer to the LsiLogic device state.
 * @param   pConfigurationReq   The configuration request.
 * @param   u8PageNumber        Number of the page to get.
 * @param   ppPageHeader        Where to store the pointer to the page header.
 * @param   ppbPageData         Where to store the pointer to the page data.
 */
static int lsilogicR3ConfigurationPageGetExtended(PLSILOGICSCSI pThis, PMptConfigurationRequest pConfigurationReq,
                                                  PMptExtendedConfigurationPageHeader *ppPageHeader,
                                                  uint8_t **ppbPageData, size_t *pcbPage)
{
    int rc = VINF_SUCCESS;

    Log(("Extended page requested:\n"));
    Log(("u8ExtPageType=%#x\n", pConfigurationReq->u8ExtPageType));
    Log(("u8ExtPageLength=%d\n", pConfigurationReq->u16ExtPageLength));

    switch (pConfigurationReq->u8ExtPageType)
    {
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASIOUNIT:
        {
            rc = lsilogicR3ConfigurationSASIOUnitPageGetFromNumber(pThis,
                                                                 pThis->pConfigurationPages,
                                                                 pConfigurationReq->u8PageNumber,
                                                                 ppPageHeader, ppbPageData, pcbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASPHYS:
        {
            rc = lsilogicR3ConfigurationSASPHYPageGetFromNumber(pThis,
                                                              pThis->pConfigurationPages,
                                                              pConfigurationReq->u8PageNumber,
                                                              pConfigurationReq->PageAddress,
                                                              ppPageHeader, ppbPageData, pcbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASDEVICE:
        {
            rc = lsilogicR3ConfigurationSASDevicePageGetFromNumber(pThis,
                                                                 pThis->pConfigurationPages,
                                                                 pConfigurationReq->u8PageNumber,
                                                                 pConfigurationReq->PageAddress,
                                                                 ppPageHeader, ppbPageData, pcbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASEXPANDER: /* No expanders supported */
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_ENCLOSURE: /* No enclosures supported */
        default:
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

/**
 * Processes a Configuration request.
 *
 * @returns VBox status code.
 * @param   pThis               Pointer to the LsiLogic device state.
 * @param   pConfigurationReq   Pointer to the request structure.
 * @param   pReply              Pointer to the reply message frame
 */
static int lsilogicR3ProcessConfigurationRequest(PLSILOGICSCSI pThis, PMptConfigurationRequest pConfigurationReq,
                                                 PMptConfigurationReply pReply)
{
    int                                 rc             = VINF_SUCCESS;
    uint8_t                            *pbPageData     = NULL;
    PMptConfigurationPageHeader         pPageHeader    = NULL;
    PMptExtendedConfigurationPageHeader pExtPageHeader = NULL;
    uint8_t                             u8PageType;
    uint8_t                             u8PageAttribute;
    size_t                              cbPage = 0;

    LogFlowFunc(("pThis=%#p\n", pThis));

    u8PageType = MPT_CONFIGURATION_PAGE_TYPE_GET(pConfigurationReq->u8PageType);
    u8PageAttribute = MPT_CONFIGURATION_PAGE_ATTRIBUTE_GET(pConfigurationReq->u8PageType);

    Log(("GuestRequest:\n"));
    Log(("u8Action=%#x\n", pConfigurationReq->u8Action));
    Log(("u8PageType=%#x\n", u8PageType));
    Log(("u8PageNumber=%d\n", pConfigurationReq->u8PageNumber));
    Log(("u8PageLength=%d\n", pConfigurationReq->u8PageLength));
    Log(("u8PageVersion=%d\n", pConfigurationReq->u8PageVersion));

    /* Copy common bits from the request into the reply. */
    pReply->u8MessageLength   = 6; /* 6 32bit D-Words. */
    pReply->u8Action          = pConfigurationReq->u8Action;
    pReply->u8Function        = pConfigurationReq->u8Function;
    pReply->u32MessageContext = pConfigurationReq->u32MessageContext;

    switch (u8PageType)
    {
        case MPT_CONFIGURATION_PAGE_TYPE_IO_UNIT:
        {
            /* Get the page data. */
            rc = lsilogicR3ConfigurationIOUnitPageGetFromNumber(pThis,
                                                              pThis->pConfigurationPages,
                                                              pConfigurationReq->u8PageNumber,
                                                              &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_IOC:
        {
            /* Get the page data. */
            rc = lsilogicR3ConfigurationIOCPageGetFromNumber(pThis,
                                                           pThis->pConfigurationPages,
                                                           pConfigurationReq->u8PageNumber,
                                                           &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_MANUFACTURING:
        {
            /* Get the page data. */
            rc = lsilogicR3ConfigurationManufacturingPageGetFromNumber(pThis,
                                                                     pThis->pConfigurationPages,
                                                                     pConfigurationReq->u8PageNumber,
                                                                     &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT:
        {
            /* Get the page data. */
            rc = lsilogicR3ConfigurationSCSISPIPortPageGetFromNumber(pThis,
                                                                   pThis->pConfigurationPages,
                                                                   pConfigurationReq->PageAddress.MPIPortNumber.u8PortNumber,
                                                                   pConfigurationReq->u8PageNumber,
                                                                   &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE:
        {
            /* Get the page data. */
            rc = lsilogicR3ConfigurationSCSISPIDevicePageGetFromNumber(pThis,
                                                                     pThis->pConfigurationPages,
                                                                     pConfigurationReq->PageAddress.BusAndTargetId.u8Bus,
                                                                     pConfigurationReq->PageAddress.BusAndTargetId.u8TargetID,
                                                                     pConfigurationReq->u8PageNumber,
                                                                     &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_BIOS:
        {
            rc = lsilogicR3ConfigurationBiosPageGetFromNumber(pThis,
                                                            pThis->pConfigurationPages,
                                                            pConfigurationReq->u8PageNumber,
                                                            &pPageHeader, &pbPageData, &cbPage);
            break;
        }
        case MPT_CONFIGURATION_PAGE_TYPE_EXTENDED:
        {
            rc = lsilogicR3ConfigurationPageGetExtended(pThis,
                                                      pConfigurationReq,
                                                      &pExtPageHeader, &pbPageData, &cbPage);
            break;
        }
        default:
            rc = VERR_NOT_FOUND;
    }

    if (rc == VERR_NOT_FOUND)
    {
        Log(("Page not found\n"));
        pReply->u8PageType    = pConfigurationReq->u8PageType;
        pReply->u8PageNumber  = pConfigurationReq->u8PageNumber;
        pReply->u8PageLength  = pConfigurationReq->u8PageLength;
        pReply->u8PageVersion = pConfigurationReq->u8PageVersion;
        pReply->u16IOCStatus  = MPT_IOCSTATUS_CONFIG_INVALID_PAGE;
        return VINF_SUCCESS;
    }

    if (u8PageType == MPT_CONFIGURATION_PAGE_TYPE_EXTENDED)
    {
        pReply->u8PageType       = pExtPageHeader->u8PageType;
        pReply->u8PageNumber     = pExtPageHeader->u8PageNumber;
        pReply->u8PageVersion    = pExtPageHeader->u8PageVersion;
        pReply->u8ExtPageType    = pExtPageHeader->u8ExtPageType;
        pReply->u16ExtPageLength = pExtPageHeader->u16ExtPageLength;

        for (int i = 0; i < pExtPageHeader->u16ExtPageLength; i++)
            LogFlowFunc(("PageData[%d]=%#x\n", i, ((uint32_t *)pbPageData)[i]));
    }
    else
    {
        pReply->u8PageType    = pPageHeader->u8PageType;
        pReply->u8PageNumber  = pPageHeader->u8PageNumber;
        pReply->u8PageLength  = pPageHeader->u8PageLength;
        pReply->u8PageVersion = pPageHeader->u8PageVersion;

        for (int i = 0; i < pReply->u8PageLength; i++)
            LogFlowFunc(("PageData[%d]=%#x\n", i, ((uint32_t *)pbPageData)[i]));
    }

    /*
     * Don't use the scatter gather handling code as the configuration request always have only one
     * simple element.
     */
    switch (pConfigurationReq->u8Action)
    {
        case MPT_CONFIGURATION_REQUEST_ACTION_DEFAULT: /* Nothing to do. We are always using the defaults. */
        case MPT_CONFIGURATION_REQUEST_ACTION_HEADER:
        {
            /* Already copied above nothing to do. */
            break;
        }
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_NVRAM:
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_CURRENT:
        case MPT_CONFIGURATION_REQUEST_ACTION_READ_DEFAULT:
        {
            uint32_t cbBuffer = pConfigurationReq->SimpleSGElement.u24Length;
            if (cbBuffer != 0)
            {
                RTGCPHYS GCPhysAddrPageBuffer = pConfigurationReq->SimpleSGElement.u32DataBufferAddressLow;
                if (pConfigurationReq->SimpleSGElement.f64BitAddress)
                    GCPhysAddrPageBuffer |= (uint64_t)pConfigurationReq->SimpleSGElement.u32DataBufferAddressHigh << 32;

                PDMDevHlpPCIPhysWrite(pThis->CTX_SUFF(pDevIns), GCPhysAddrPageBuffer, pbPageData, RT_MIN(cbBuffer, cbPage));
            }
            break;
        }
        case MPT_CONFIGURATION_REQUEST_ACTION_WRITE_CURRENT:
        case MPT_CONFIGURATION_REQUEST_ACTION_WRITE_NVRAM:
        {
            uint32_t cbBuffer = pConfigurationReq->SimpleSGElement.u24Length;
            if (cbBuffer != 0)
            {
                RTGCPHYS GCPhysAddrPageBuffer = pConfigurationReq->SimpleSGElement.u32DataBufferAddressLow;
                if (pConfigurationReq->SimpleSGElement.f64BitAddress)
                    GCPhysAddrPageBuffer |= (uint64_t)pConfigurationReq->SimpleSGElement.u32DataBufferAddressHigh << 32;

                LogFlow(("cbBuffer=%u cbPage=%u\n", cbBuffer, cbPage));

                PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), GCPhysAddrPageBuffer, pbPageData,
                                  RT_MIN(cbBuffer, cbPage));
            }
            break;
        }
        default:
            AssertMsgFailed(("todo\n"));
    }

    return VINF_SUCCESS;
}

/**
 * Initializes the configuration pages for the SPI SCSI controller.
 *
 * @returns nothing
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicR3InitializeConfigurationPagesSpi(PLSILOGICSCSI pThis)
{
    PMptConfigurationPagesSpi pPages = &pThis->pConfigurationPages->u.SpiPages;

    AssertMsg(pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI, ("Controller is not the SPI SCSI one\n"));

    LogFlowFunc(("pThis=%#p\n", pThis));

    /* Clear everything first. */
    memset(pPages, 0, sizeof(MptConfigurationPagesSpi));

    for (unsigned i = 0; i < RT_ELEMENTS(pPages->aPortPages); i++)
    {
        /* SCSI-SPI port page 0. */
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageNumber = 0;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort0) / 4;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fInformationUnitTransfersCapable = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fDTCapable                       = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fQASCapable                      = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u8MinimumSynchronousTransferPeriod =  0;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u8MaximumSynchronousOffset         = 0xff;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fWide                              = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.fAIPCapable                        = true;
        pPages->aPortPages[i].SCSISPIPortPage0.u.fields.u2SignalingType                    = 0x3; /* Single Ended. */

        /* SCSI-SPI port page 1. */
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageNumber = 1;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort1) / 4;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u8SCSIID                  = 7;
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u16PortResponseIDsBitmask = (1 << 7);
        pPages->aPortPages[i].SCSISPIPortPage1.u.fields.u32OnBusTimerValue        =  0;

        /* SCSI-SPI port page 2. */
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_PORT;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageNumber = 2;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIPort2) / 4;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.u4HostSCSIID           = 7;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.u2InitializeHBA        = 0x3;
        pPages->aPortPages[i].SCSISPIPortPage2.u.fields.fTerminationDisabled   = true;
        for (unsigned iDevice = 0; iDevice < RT_ELEMENTS(pPages->aPortPages[i].SCSISPIPortPage2.u.fields.aDeviceSettings); iDevice++)
        {
            pPages->aPortPages[i].SCSISPIPortPage2.u.fields.aDeviceSettings[iDevice].fBootChoice   = true;
        }
        /* Everything else 0 for now. */
    }

    for (unsigned uBusCurr = 0; uBusCurr < RT_ELEMENTS(pPages->aBuses); uBusCurr++)
    {
        for (unsigned uDeviceCurr = 0; uDeviceCurr < RT_ELEMENTS(pPages->aBuses[uBusCurr].aDevicePages); uDeviceCurr++)
        {
            /* SCSI-SPI device page 0. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageNumber = 0;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage0.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice0) / 4;
            /* Everything else 0 for now. */

            /* SCSI-SPI device page 1. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageNumber = 1;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage1.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice1) / 4;
            /* Everything else 0 for now. */

            /* SCSI-SPI device page 2. */
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageNumber = 2;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage2.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice2) / 4;
            /* Everything else 0 for now. */

            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageType   =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                                                                 | MPT_CONFIGURATION_PAGE_TYPE_SCSI_SPI_DEVICE;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageNumber = 3;
            pPages->aBuses[uBusCurr].aDevicePages[uDeviceCurr].SCSISPIDevicePage3.u.fields.Header.u8PageLength = sizeof(MptConfigurationPageSCSISPIDevice3) / 4;
            /* Everything else 0 for now. */
        }
    }
}

/**
 * Generates a handle.
 *
 * @returns the handle.
 * @param   pThis       Pointer to the LsiLogic device state.
 */
DECLINLINE(uint16_t) lsilogicGetHandle(PLSILOGICSCSI pThis)
{
    uint16_t u16Handle = pThis->u16NextHandle++;
    return u16Handle;
}

/**
 * Generates a SAS address (WWID)
 *
 * @returns nothing.
 * @param   pSASAddress Pointer to an unitialised SAS address.
 * @param   iId         iId which will go into the address.
 *
 * @todo Generate better SAS addresses. (Request a block from SUN probably)
 */
void lsilogicSASAddressGenerate(PSASADDRESS pSASAddress, unsigned iId)
{
    pSASAddress->u8Address[0] = (0x5 << 5);
    pSASAddress->u8Address[1] = 0x01;
    pSASAddress->u8Address[2] = 0x02;
    pSASAddress->u8Address[3] = 0x03;
    pSASAddress->u8Address[4] = 0x04;
    pSASAddress->u8Address[5] = 0x05;
    pSASAddress->u8Address[6] = 0x06;
    pSASAddress->u8Address[7] = iId;
}

/**
 * Initializes the configuration pages for the SAS SCSI controller.
 *
 * @returns nothing
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicR3InitializeConfigurationPagesSas(PLSILOGICSCSI pThis)
{
    PMptConfigurationPagesSas pPages = &pThis->pConfigurationPages->u.SasPages;

    AssertMsg(pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS, ("Controller is not the SAS SCSI one\n"));

    LogFlowFunc(("pThis=%#p\n", pThis));

    /* Manufacturing Page 7 - Connector settings. */
    pPages->cbManufacturingPage7 = LSILOGICSCSI_MANUFACTURING7_GET_SIZE(pThis->cPorts);
    PMptConfigurationPageManufacturing7 pManufacturingPage7 = (PMptConfigurationPageManufacturing7)RTMemAllocZ(pPages->cbManufacturingPage7);
    AssertPtr(pManufacturingPage7);
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(pManufacturingPage7,
                                              0, 7,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);
    /* Set size manually. */
    if (pPages->cbManufacturingPage7 / 4 > 255)
        pManufacturingPage7->u.fields.Header.u8PageLength = 255;
    else
        pManufacturingPage7->u.fields.Header.u8PageLength = pPages->cbManufacturingPage7 / 4;
    pManufacturingPage7->u.fields.u8NumPhys = pThis->cPorts;
    pPages->pManufacturingPage7 = pManufacturingPage7;

    /* SAS I/O unit page 0 - Port specific information. */
    pPages->cbSASIOUnitPage0 = LSILOGICSCSI_SASIOUNIT0_GET_SIZE(pThis->cPorts);
    PMptConfigurationPageSASIOUnit0 pSASPage0 = (PMptConfigurationPageSASIOUnit0)RTMemAllocZ(pPages->cbSASIOUnitPage0);
    AssertPtr(pSASPage0);

    MPT_CONFIG_EXTENDED_PAGE_HEADER_INIT(pSASPage0, pPages->cbSASIOUnitPage0,
                                         0, MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY,
                                         MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASIOUNIT);
    pSASPage0->u.fields.u8NumPhys = pThis->cPorts;
    pPages->pSASIOUnitPage0 = pSASPage0;

    /* SAS I/O unit page 1 - Port specific settings. */
    pPages->cbSASIOUnitPage1 = LSILOGICSCSI_SASIOUNIT1_GET_SIZE(pThis->cPorts);
    PMptConfigurationPageSASIOUnit1 pSASPage1 = (PMptConfigurationPageSASIOUnit1)RTMemAllocZ(pPages->cbSASIOUnitPage1);
    AssertPtr(pSASPage1);

    MPT_CONFIG_EXTENDED_PAGE_HEADER_INIT(pSASPage1, pPages->cbSASIOUnitPage1,
                                         1, MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE,
                                         MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASIOUNIT);
    pSASPage1->u.fields.u8NumPhys = pSASPage0->u.fields.u8NumPhys;
    pSASPage1->u.fields.u16ControlFlags = 0;
    pSASPage1->u.fields.u16AdditionalControlFlags = 0;
    pPages->pSASIOUnitPage1 = pSASPage1;

    /* SAS I/O unit page 2 - Port specific information. */
    pPages->SASIOUnitPage2.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                 | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
    pPages->SASIOUnitPage2.u.fields.ExtHeader.u8PageNumber     = 2;
    pPages->SASIOUnitPage2.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASIOUNIT;
    pPages->SASIOUnitPage2.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASIOUnit2) / 4;

    /* SAS I/O unit page 3 - Port specific information. */
    pPages->SASIOUnitPage3.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                 | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
    pPages->SASIOUnitPage3.u.fields.ExtHeader.u8PageNumber     = 3;
    pPages->SASIOUnitPage3.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASIOUNIT;
    pPages->SASIOUnitPage3.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASIOUnit3) / 4;

    pPages->cPHYs  = pThis->cPorts;
    pPages->paPHYs = (PMptPHY)RTMemAllocZ(pPages->cPHYs * sizeof(MptPHY));
    AssertPtr(pPages->paPHYs);

    /* Initialize the PHY configuration */
    for (unsigned i = 0; i < pThis->cPorts; i++)
    {
        PMptPHY pPHYPages = &pPages->paPHYs[i];
        uint16_t u16ControllerHandle = lsilogicGetHandle(pThis);

        pManufacturingPage7->u.fields.aPHY[i].u8Location = LSILOGICSCSI_MANUFACTURING7_LOCATION_AUTO;

        pSASPage0->u.fields.aPHY[i].u8Port      = i;
        pSASPage0->u.fields.aPHY[i].u8PortFlags = 0;
        pSASPage0->u.fields.aPHY[i].u8PhyFlags  = 0;
        pSASPage0->u.fields.aPHY[i].u8NegotiatedLinkRate = LSILOGICSCSI_SASIOUNIT0_NEGOTIATED_RATE_FAILED;
        pSASPage0->u.fields.aPHY[i].u32ControllerPhyDeviceInfo = LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_SET(LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_NO);
        pSASPage0->u.fields.aPHY[i].u16ControllerDevHandle     = u16ControllerHandle;
        pSASPage0->u.fields.aPHY[i].u16AttachedDevHandle       = 0; /* No device attached. */
        pSASPage0->u.fields.aPHY[i].u32DiscoveryStatus         = 0; /* No errors */

        pSASPage1->u.fields.aPHY[i].u8Port           = i;
        pSASPage1->u.fields.aPHY[i].u8PortFlags      = 0;
        pSASPage1->u.fields.aPHY[i].u8PhyFlags       = 0;
        pSASPage1->u.fields.aPHY[i].u8MaxMinLinkRate =   LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MIN_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_15GB)
                                                       | LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MAX_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_30GB);
        pSASPage1->u.fields.aPHY[i].u32ControllerPhyDeviceInfo = LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_SET(LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_NO);

        /* SAS PHY page 0. */
        pPHYPages->SASPHYPage0.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                          | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
        pPHYPages->SASPHYPage0.u.fields.ExtHeader.u8PageNumber     = 0;
        pPHYPages->SASPHYPage0.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASPHYS;
        pPHYPages->SASPHYPage0.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASPHY0) / 4;
        pPHYPages->SASPHYPage0.u.fields.u8AttachedPhyIdentifier    = i;
        pPHYPages->SASPHYPage0.u.fields.u32AttachedDeviceInfo      = LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_SET(LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_NO);
        pPHYPages->SASPHYPage0.u.fields.u8ProgrammedLinkRate       =   LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MIN_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_15GB)
                                                                     | LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MAX_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_30GB);
        pPHYPages->SASPHYPage0.u.fields.u8HwLinkRate               =   LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MIN_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_15GB)
                                                                     | LSILOGICSCSI_SASIOUNIT1_LINK_RATE_MAX_SET(LSILOGICSCSI_SASIOUNIT1_LINK_RATE_30GB);

        /* SAS PHY page 1. */
        pPHYPages->SASPHYPage1.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                     | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
        pPHYPages->SASPHYPage1.u.fields.ExtHeader.u8PageNumber     = 1;
        pPHYPages->SASPHYPage1.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASPHYS;
        pPHYPages->SASPHYPage1.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASPHY1) / 4;

        /* Settings for present devices. */
        if (pThis->paDeviceStates[i].pDrvBase)
        {
            uint16_t u16DeviceHandle = lsilogicGetHandle(pThis);
            SASADDRESS SASAddress;
            PMptSASDevice pSASDevice = (PMptSASDevice)RTMemAllocZ(sizeof(MptSASDevice));
            AssertPtr(pSASDevice);

            memset(&SASAddress, 0, sizeof(SASADDRESS));
            lsilogicSASAddressGenerate(&SASAddress, i);

            pSASPage0->u.fields.aPHY[i].u8NegotiatedLinkRate       = LSILOGICSCSI_SASIOUNIT0_NEGOTIATED_RATE_SET(LSILOGICSCSI_SASIOUNIT0_NEGOTIATED_RATE_30GB);
            pSASPage0->u.fields.aPHY[i].u32ControllerPhyDeviceInfo =   LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_SET(LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_END)
                                                                     | LSILOGICSCSI_SASIOUNIT0_DEVICE_SSP_TARGET;
            pSASPage0->u.fields.aPHY[i].u16AttachedDevHandle       = u16DeviceHandle;
            pSASPage1->u.fields.aPHY[i].u32ControllerPhyDeviceInfo =   LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_SET(LSILOGICSCSI_SASIOUNIT0_DEVICE_TYPE_END)
                                                                     | LSILOGICSCSI_SASIOUNIT0_DEVICE_SSP_TARGET;
            pSASPage0->u.fields.aPHY[i].u16ControllerDevHandle     = u16DeviceHandle;

            pPHYPages->SASPHYPage0.u.fields.u32AttachedDeviceInfo  = LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_SET(LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_END);
            pPHYPages->SASPHYPage0.u.fields.SASAddress             = SASAddress;
            pPHYPages->SASPHYPage0.u.fields.u16OwnerDevHandle      = u16DeviceHandle;
            pPHYPages->SASPHYPage0.u.fields.u16AttachedDevHandle   = u16DeviceHandle;

            /* SAS device page 0. */
            pSASDevice->SASDevicePage0.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                             | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
            pSASDevice->SASDevicePage0.u.fields.ExtHeader.u8PageNumber     = 0;
            pSASDevice->SASDevicePage0.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASDEVICE;
            pSASDevice->SASDevicePage0.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASDevice0) / 4;
            pSASDevice->SASDevicePage0.u.fields.SASAddress                 = SASAddress;
            pSASDevice->SASDevicePage0.u.fields.u16ParentDevHandle         = u16ControllerHandle;
            pSASDevice->SASDevicePage0.u.fields.u8PhyNum                   = i;
            pSASDevice->SASDevicePage0.u.fields.u8AccessStatus             = LSILOGICSCSI_SASDEVICE0_STATUS_NO_ERRORS;
            pSASDevice->SASDevicePage0.u.fields.u16DevHandle               = u16DeviceHandle;
            pSASDevice->SASDevicePage0.u.fields.u8TargetID                 = i;
            pSASDevice->SASDevicePage0.u.fields.u8Bus                      = 0;
            pSASDevice->SASDevicePage0.u.fields.u32DeviceInfo              =   LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_SET(LSILOGICSCSI_SASPHY0_DEV_INFO_DEVICE_TYPE_END)
                                                                             | LSILOGICSCSI_SASIOUNIT0_DEVICE_SSP_TARGET;
            pSASDevice->SASDevicePage0.u.fields.u16Flags                   =   LSILOGICSCSI_SASDEVICE0_FLAGS_DEVICE_PRESENT
                                                                             | LSILOGICSCSI_SASDEVICE0_FLAGS_DEVICE_MAPPED_TO_BUS_AND_TARGET_ID
                                                                             | LSILOGICSCSI_SASDEVICE0_FLAGS_DEVICE_MAPPING_PERSISTENT;
            pSASDevice->SASDevicePage0.u.fields.u8PhysicalPort             = i;

            /* SAS device page 1. */
            pSASDevice->SASDevicePage1.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                             | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
            pSASDevice->SASDevicePage1.u.fields.ExtHeader.u8PageNumber     = 1;
            pSASDevice->SASDevicePage1.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASDEVICE;
            pSASDevice->SASDevicePage1.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASDevice1) / 4;
            pSASDevice->SASDevicePage1.u.fields.SASAddress                 = SASAddress;
            pSASDevice->SASDevicePage1.u.fields.u16DevHandle               = u16DeviceHandle;
            pSASDevice->SASDevicePage1.u.fields.u8TargetID                 = i;
            pSASDevice->SASDevicePage1.u.fields.u8Bus                      = 0;

            /* SAS device page 2. */
            pSASDevice->SASDevicePage2.u.fields.ExtHeader.u8PageType       =   MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY
                                                                              | MPT_CONFIGURATION_PAGE_TYPE_EXTENDED;
            pSASDevice->SASDevicePage2.u.fields.ExtHeader.u8PageNumber     = 2;
            pSASDevice->SASDevicePage2.u.fields.ExtHeader.u8ExtPageType    = MPT_CONFIGURATION_PAGE_TYPE_EXTENDED_SASDEVICE;
            pSASDevice->SASDevicePage2.u.fields.ExtHeader.u16ExtPageLength = sizeof(MptConfigurationPageSASDevice2) / 4;
            pSASDevice->SASDevicePage2.u.fields.SASAddress                 = SASAddress;

            /* Link into device list. */
            if (!pPages->cDevices)
            {
                pPages->pSASDeviceHead = pSASDevice;
                pPages->pSASDeviceTail = pSASDevice;
                pPages->cDevices = 1;
            }
            else
            {
                pSASDevice->pPrev = pPages->pSASDeviceTail;
                pPages->pSASDeviceTail->pNext = pSASDevice;
                pPages->pSASDeviceTail = pSASDevice;
                pPages->cDevices++;
            }
        }
    }
}

/**
 * Initializes the configuration pages.
 *
 * @returns nothing
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicR3InitializeConfigurationPages(PLSILOGICSCSI pThis)
{
    /* Initialize the common pages. */
    PMptConfigurationPagesSupported pPages = (PMptConfigurationPagesSupported)RTMemAllocZ(sizeof(MptConfigurationPagesSupported));

    pThis->pConfigurationPages = pPages;

    LogFlowFunc(("pThis=%#p\n", pThis));

    /* Clear everything first. */
    memset(pPages, 0, sizeof(MptConfigurationPagesSupported));

    /* Manufacturing Page 0. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage0,
                                              MptConfigurationPageManufacturing0, 0,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abChipName,          "VBox MPT Fusion", 16);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abChipRevision,      "1.0", 8);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardName,         "VBox MPT Fusion", 16);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardAssembly,     "SUN", 8);
    strncpy((char *)pPages->ManufacturingPage0.u.fields.abBoardTracerNumber, "CAFECAFECAFECAFE", 16);

    /* Manufacturing Page 1 - I don't know what this contains so we leave it 0 for now. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage1,
                                              MptConfigurationPageManufacturing1, 1,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);

    /* Manufacturing Page 2. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage2,
                                              MptConfigurationPageManufacturing2, 2,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
    {
        pPages->ManufacturingPage2.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_SPI_DEVICE_ID;
        pPages->ManufacturingPage2.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_SPI_REVISION_ID;
    }
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
    {
        pPages->ManufacturingPage2.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_SAS_DEVICE_ID;
        pPages->ManufacturingPage2.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_SAS_REVISION_ID;
    }

    /* Manufacturing Page 3. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage3,
                                              MptConfigurationPageManufacturing3, 3,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
    {
        pPages->ManufacturingPage3.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_SPI_DEVICE_ID;
        pPages->ManufacturingPage3.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_SPI_REVISION_ID;
    }
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
    {
        pPages->ManufacturingPage3.u.fields.u16PCIDeviceID = LSILOGICSCSI_PCI_SAS_DEVICE_ID;
        pPages->ManufacturingPage3.u.fields.u8PCIRevisionID = LSILOGICSCSI_PCI_SAS_REVISION_ID;
    }

    /* Manufacturing Page 4 - I don't know what this contains so we leave it 0 for now. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage4,
                                              MptConfigurationPageManufacturing4, 4,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);

    /* Manufacturing Page 5 - WWID settings. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage5,
                                              MptConfigurationPageManufacturing5, 5,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT_READONLY);

    /* Manufacturing Page 6 - Product specific settings. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage6,
                                              MptConfigurationPageManufacturing6, 6,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* Manufacturing Page 8 -  Product specific settings. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage8,
                                              MptConfigurationPageManufacturing8, 8,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* Manufacturing Page 9 -  Product specific settings. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage9,
                                              MptConfigurationPageManufacturing9, 9,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* Manufacturing Page 10 -  Product specific settings. */
    MPT_CONFIG_PAGE_HEADER_INIT_MANUFACTURING(&pPages->ManufacturingPage10,
                                              MptConfigurationPageManufacturing10, 10,
                                              MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* I/O Unit page 0. */
    MPT_CONFIG_PAGE_HEADER_INIT_IO_UNIT(&pPages->IOUnitPage0,
                                        MptConfigurationPageIOUnit0, 0,
                                        MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    pPages->IOUnitPage0.u.fields.u64UniqueIdentifier = 0xcafe;

    /* I/O Unit page 1. */
    MPT_CONFIG_PAGE_HEADER_INIT_IO_UNIT(&pPages->IOUnitPage1,
                                        MptConfigurationPageIOUnit1, 1,
                                        MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    pPages->IOUnitPage1.u.fields.fSingleFunction         = true;
    pPages->IOUnitPage1.u.fields.fAllPathsMapped         = false;
    pPages->IOUnitPage1.u.fields.fIntegratedRAIDDisabled = true;
    pPages->IOUnitPage1.u.fields.f32BitAccessForced      = false;

    /* I/O Unit page 2. */
    MPT_CONFIG_PAGE_HEADER_INIT_IO_UNIT(&pPages->IOUnitPage2,
                                        MptConfigurationPageIOUnit2, 2,
                                        MPT_CONFIGURATION_PAGE_ATTRIBUTE_PERSISTENT);
    pPages->IOUnitPage2.u.fields.fPauseOnError       = false;
    pPages->IOUnitPage2.u.fields.fVerboseModeEnabled = false;
    pPages->IOUnitPage2.u.fields.fDisableColorVideo  = false;
    pPages->IOUnitPage2.u.fields.fNotHookInt40h      = false;
    pPages->IOUnitPage2.u.fields.u32BIOSVersion      = 0xcafecafe;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].fAdapterEnabled = true;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].fAdapterEmbedded = true;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].u8PCIBusNumber = 0;
    pPages->IOUnitPage2.u.fields.aAdapterOrder[0].u8PCIDevFn     = pThis->PciDev.devfn;

    /* I/O Unit page 3. */
    MPT_CONFIG_PAGE_HEADER_INIT_IO_UNIT(&pPages->IOUnitPage3,
                                        MptConfigurationPageIOUnit3, 3,
                                        MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);
    pPages->IOUnitPage3.u.fields.u8GPIOCount = 0;

    /* I/O Unit page 4. */
    MPT_CONFIG_PAGE_HEADER_INIT_IO_UNIT(&pPages->IOUnitPage4,
                                        MptConfigurationPageIOUnit4, 4,
                                        MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* IOC page 0. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage0,
                                    MptConfigurationPageIOC0, 0,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    pPages->IOCPage0.u.fields.u32TotalNVStore      = 0;
    pPages->IOCPage0.u.fields.u32FreeNVStore       = 0;

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
    {
        pPages->IOCPage0.u.fields.u16VendorId          = LSILOGICSCSI_PCI_VENDOR_ID;
        pPages->IOCPage0.u.fields.u16DeviceId          = LSILOGICSCSI_PCI_SPI_DEVICE_ID;
        pPages->IOCPage0.u.fields.u8RevisionId         = LSILOGICSCSI_PCI_SPI_REVISION_ID;
        pPages->IOCPage0.u.fields.u32ClassCode         = LSILOGICSCSI_PCI_SPI_CLASS_CODE;
        pPages->IOCPage0.u.fields.u16SubsystemVendorId = LSILOGICSCSI_PCI_SPI_SUBSYSTEM_VENDOR_ID;
        pPages->IOCPage0.u.fields.u16SubsystemId       = LSILOGICSCSI_PCI_SPI_SUBSYSTEM_ID;
    }
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
    {
        pPages->IOCPage0.u.fields.u16VendorId          = LSILOGICSCSI_PCI_VENDOR_ID;
        pPages->IOCPage0.u.fields.u16DeviceId          = LSILOGICSCSI_PCI_SAS_DEVICE_ID;
        pPages->IOCPage0.u.fields.u8RevisionId         = LSILOGICSCSI_PCI_SAS_REVISION_ID;
        pPages->IOCPage0.u.fields.u32ClassCode         = LSILOGICSCSI_PCI_SAS_CLASS_CODE;
        pPages->IOCPage0.u.fields.u16SubsystemVendorId = LSILOGICSCSI_PCI_SAS_SUBSYSTEM_VENDOR_ID;
        pPages->IOCPage0.u.fields.u16SubsystemId       = LSILOGICSCSI_PCI_SAS_SUBSYSTEM_ID;
    }

    /* IOC page 1. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage1,
                                    MptConfigurationPageIOC1, 1,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);
    pPages->IOCPage1.u.fields.fReplyCoalescingEnabled = false;
    pPages->IOCPage1.u.fields.u32CoalescingTimeout    = 0;
    pPages->IOCPage1.u.fields.u8CoalescingDepth       = 0;

    /* IOC page 2. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage2,
                                    MptConfigurationPageIOC2, 2,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    /* Everything else here is 0. */

    /* IOC page 3. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage3,
                                    MptConfigurationPageIOC3, 3,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    /* Everything else here is 0. */

    /* IOC page 4. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage4,
                                    MptConfigurationPageIOC4, 4,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    /* Everything else here is 0. */

    /* IOC page 6. */
    MPT_CONFIG_PAGE_HEADER_INIT_IOC(&pPages->IOCPage6,
                                    MptConfigurationPageIOC6, 6,
                                    MPT_CONFIGURATION_PAGE_ATTRIBUTE_READONLY);
    /* Everything else here is 0. */

    /* BIOS page 1. */
    MPT_CONFIG_PAGE_HEADER_INIT_BIOS(&pPages->BIOSPage1,
                                     MptConfigurationPageBIOS1, 1,
                                     MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* BIOS page 2. */
    MPT_CONFIG_PAGE_HEADER_INIT_BIOS(&pPages->BIOSPage2,
                                     MptConfigurationPageBIOS2, 2,
                                     MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    /* BIOS page 4. */
    MPT_CONFIG_PAGE_HEADER_INIT_BIOS(&pPages->BIOSPage4,
                                     MptConfigurationPageBIOS4, 4,
                                     MPT_CONFIGURATION_PAGE_ATTRIBUTE_CHANGEABLE);

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
        lsilogicR3InitializeConfigurationPagesSpi(pThis);
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
        lsilogicR3InitializeConfigurationPagesSas(pThis);
    else
        AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));
}

/**
 * @callback_method_impl{FNPDMQUEUEDEV, Transmit queue consumer.}
 */
static DECLCALLBACK(bool) lsilogicR3NotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pDevIns=%#p pItem=%#p\n", pDevIns, pItem));

    /* Reset notification event. */
    ASMAtomicXchgBool(&pThis->fNotificationSend, false);

    /* Only process request which arrived before we received the notification. */
    uint32_t uRequestQueueNextEntryWrite = ASMAtomicReadU32(&pThis->uRequestQueueNextEntryFreeWrite);

    /* Go through the messages now and process them. */
    while (   RT_LIKELY(pThis->enmState == LSILOGICSTATE_OPERATIONAL)
           && (pThis->uRequestQueueNextAddressRead != uRequestQueueNextEntryWrite))
    {
        uint32_t  u32RequestMessageFrameDesc = pThis->CTX_SUFF(pRequestQueueBase)[pThis->uRequestQueueNextAddressRead];
        RTGCPHYS  GCPhysMessageFrameAddr = LSILOGIC_RTGCPHYS_FROM_U32(pThis->u32HostMFAHighAddr,
                                                                      (u32RequestMessageFrameDesc & ~0x07));

        PLSILOGICTASKSTATE pTaskState;

        /* Get new task state. */
        rc = RTMemCacheAllocEx(pThis->hTaskCache, (void **)&pTaskState);
        AssertRC(rc);

        pTaskState->GCPhysMessageFrameAddr = GCPhysMessageFrameAddr;

        /* Read the message header from the guest first. */
        PDMDevHlpPhysRead(pDevIns, GCPhysMessageFrameAddr, &pTaskState->GuestRequest, sizeof(MptMessageHdr));

        /* Determine the size of the request. */
        uint32_t cbRequest = 0;

        switch (pTaskState->GuestRequest.Header.u8Function)
        {
            case MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST:
                cbRequest = sizeof(MptSCSIIORequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_SCSI_TASK_MGMT:
                cbRequest = sizeof(MptSCSITaskManagementRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_IOC_INIT:
                cbRequest = sizeof(MptIOCInitRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_IOC_FACTS:
                cbRequest = sizeof(MptIOCFactsRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_CONFIG:
                cbRequest = sizeof(MptConfigurationRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_PORT_FACTS:
                cbRequest = sizeof(MptPortFactsRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_PORT_ENABLE:
                cbRequest = sizeof(MptPortEnableRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_EVENT_NOTIFICATION:
                cbRequest = sizeof(MptEventNotificationRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_EVENT_ACK:
                AssertMsgFailed(("todo\n"));
                //cbRequest = sizeof(MptEventAckRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_FW_DOWNLOAD:
                cbRequest = sizeof(MptFWDownloadRequest);
                break;
            case MPT_MESSAGE_HDR_FUNCTION_FW_UPLOAD:
                cbRequest = sizeof(MptFWUploadRequest);
                break;
            default:
                AssertMsgFailed(("Unknown function issued %u\n", pTaskState->GuestRequest.Header.u8Function));
                lsilogicSetIOCFaultCode(pThis, LSILOGIC_IOCSTATUS_INVALID_FUNCTION);
        }

        if (cbRequest != 0)
        {
            /* Read the complete message frame from guest memory now. */
            PDMDevHlpPhysRead(pDevIns, GCPhysMessageFrameAddr, &pTaskState->GuestRequest, cbRequest);

            /* Handle SCSI I/O requests now. */
            if (pTaskState->GuestRequest.Header.u8Function == MPT_MESSAGE_HDR_FUNCTION_SCSI_IO_REQUEST)
            {
               rc = lsilogicR3ProcessSCSIIORequest(pThis, pTaskState);
               AssertRC(rc);
            }
            else
            {
                MptReplyUnion Reply;
                rc = lsilogicR3ProcessMessageRequest(pThis, &pTaskState->GuestRequest.Header, &Reply);
                AssertRC(rc);
                RTMemCacheFree(pThis->hTaskCache, pTaskState);
            }

            pThis->uRequestQueueNextAddressRead++;
            pThis->uRequestQueueNextAddressRead %= pThis->cRequestQueueEntries;
        }
    }

    return true;
}

/**
 * Sets the emulated controller type from a given string.
 *
 * @returns VBox status code.
 *
 * @param   pThis           Pointer to the LsiLogic device state.
 * @param   pcszCtrlType    The string to use.
 */
static int lsilogicR3GetCtrlTypeFromString(PLSILOGICSCSI pThis, const char *pcszCtrlType)
{
    int rc = VERR_INVALID_PARAMETER;

    if (!RTStrCmp(pcszCtrlType, LSILOGICSCSI_PCI_SPI_CTRLNAME))
    {
        pThis->enmCtrlType = LSILOGICCTRLTYPE_SCSI_SPI;
        rc = VINF_SUCCESS;
    }
    else if (!RTStrCmp(pcszCtrlType, LSILOGICSCSI_PCI_SAS_CTRLNAME))
    {
        pThis->enmCtrlType = LSILOGICCTRLTYPE_SCSI_SAS;
        rc = VINF_SUCCESS;
    }

    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTIN, Legacy ISA port.}
 */
static DECLCALLBACK(int) lsilogicR3IsaIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    Assert(cb == 1);

    uint8_t iRegister = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                      ? Port - LSILOGIC_BIOS_IO_PORT
                      : Port - LSILOGIC_SAS_BIOS_IO_PORT;
    int rc = vboxscsiReadRegister(&pThis->VBoxSCSI, iRegister, pu32);

    Log2(("%s: pu32=%p:{%.*Rhxs} iRegister=%d rc=%Rrc\n",
          __FUNCTION__, pu32, 1, pu32, iRegister, rc));

    return rc;
}

/**
 * Prepares a request from the BIOS.
 *
 * @returns VBox status code.
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static int lsilogicR3PrepareBiosScsiRequest(PLSILOGICSCSI pThis)
{
    int rc;
    PLSILOGICTASKSTATE pTaskState;
    uint32_t           uTargetDevice;

    rc = RTMemCacheAllocEx(pThis->hTaskCache, (void **)&pTaskState);
    AssertMsgRCReturn(rc, ("Getting task from cache failed rc=%Rrc\n", rc), rc);

    pTaskState->fBIOS = true;

    rc = vboxscsiSetupRequest(&pThis->VBoxSCSI, &pTaskState->PDMScsiRequest, &uTargetDevice);
    AssertMsgRCReturn(rc, ("Setting up SCSI request failed rc=%Rrc\n", rc), rc);

    pTaskState->PDMScsiRequest.pvUser = pTaskState;

    if (uTargetDevice < pThis->cDeviceStates)
    {
        pTaskState->pTargetDevice = &pThis->paDeviceStates[uTargetDevice];

        if (pTaskState->pTargetDevice->pDrvBase)
        {
            ASMAtomicIncU32(&pTaskState->pTargetDevice->cOutstandingRequests);

            rc = pTaskState->pTargetDevice->pDrvSCSIConnector->pfnSCSIRequestSend(pTaskState->pTargetDevice->pDrvSCSIConnector,
                                                                                  &pTaskState->PDMScsiRequest);
            AssertMsgRCReturn(rc, ("Sending request to SCSI layer failed rc=%Rrc\n", rc), rc);
            return VINF_SUCCESS;
        }
    }

    /* Device is not present. */
    AssertMsg(pTaskState->PDMScsiRequest.pbCDB[0] == SCSI_INQUIRY,
                ("Device is not present but command is not inquiry\n"));

    SCSIINQUIRYDATA ScsiInquiryData;

    memset(&ScsiInquiryData, 0, sizeof(SCSIINQUIRYDATA));
    ScsiInquiryData.u5PeripheralDeviceType = SCSI_INQUIRY_DATA_PERIPHERAL_DEVICE_TYPE_UNKNOWN;
    ScsiInquiryData.u3PeripheralQualifier = SCSI_INQUIRY_DATA_PERIPHERAL_QUALIFIER_NOT_CONNECTED_NOT_SUPPORTED;

    memcpy(pThis->VBoxSCSI.pbBuf, &ScsiInquiryData, 5);

    rc = vboxscsiRequestFinished(&pThis->VBoxSCSI, &pTaskState->PDMScsiRequest, SCSI_STATUS_OK);
    AssertMsgRCReturn(rc, ("Finishing BIOS SCSI request failed rc=%Rrc\n", rc), rc);

    RTMemCacheFree(pThis->hTaskCache, pTaskState);
    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTOUT, Legacy ISA port.}
 */
static DECLCALLBACK(int) lsilogicR3IsaIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    Log2(("#%d %s: pvUser=%#p cb=%d u32=%#x Port=%#x\n", pDevIns->iInstance, __FUNCTION__, pvUser, cb, u32, Port));

    Assert(cb == 1);

    uint8_t iRegister = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                      ? Port - LSILOGIC_BIOS_IO_PORT
                      : Port - LSILOGIC_SAS_BIOS_IO_PORT;
    int rc = vboxscsiWriteRegister(&pThis->VBoxSCSI, iRegister, (uint8_t)u32);
    if (rc == VERR_MORE_DATA)
    {
        rc = lsilogicR3PrepareBiosScsiRequest(pThis);
        AssertRC(rc);
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Writing BIOS register failed %Rrc\n", rc));

    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTOUTSTRING,
 * Port I/O Handler for primary port range OUT string operations.}
 */
static DECLCALLBACK(int) lsilogicR3IsaIOPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port,
                                                     PRTGCPTR pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    Log2(("#%d %s: pvUser=%#p cb=%d Port=%#x\n", pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    uint8_t iRegister = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                      ? Port - LSILOGIC_BIOS_IO_PORT
                      : Port - LSILOGIC_SAS_BIOS_IO_PORT;
    int rc = vboxscsiWriteString(pDevIns, &pThis->VBoxSCSI, iRegister, pGCPtrSrc, pcTransfer, cb);
    if (rc == VERR_MORE_DATA)
    {
        rc = lsilogicR3PrepareBiosScsiRequest(pThis);
        AssertRC(rc);
    }
    else if (RT_FAILURE(rc))
        AssertMsgFailed(("Writing BIOS register failed %Rrc\n", rc));

    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTINSTRING,
 * Port I/O Handler for primary port range IN string operations.}
 */
static DECLCALLBACK(int) lsilogicR3IsaIOPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst,
                                                  PRTGCUINTREG pcTransfer, unsigned cb)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    LogFlowFunc(("#%d %s: pvUser=%#p cb=%d Port=%#x\n",
                 pDevIns->iInstance, __FUNCTION__, pvUser, cb, Port));

    uint8_t iRegister = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                      ? Port - LSILOGIC_BIOS_IO_PORT
                      : Port - LSILOGIC_SAS_BIOS_IO_PORT;
    return vboxscsiReadString(pDevIns, &pThis->VBoxSCSI, iRegister, pGCPtrDst, pcTransfer, cb);
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) lsilogicR3Map(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion,
                                       RTGCPHYS GCPhysAddress, uint32_t cb,
                                       PCIADDRESSSPACE enmType)
{
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    PLSILOGICSCSI  pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int   rc = VINF_SUCCESS;
    const char *pcszCtrl = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                           ? "LsiLogic"
                           : "LsiLogicSas";
    const char *pcszDiag = pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                           ? "LsiLogicDiag"
                           : "LsiLogicSasDiag";

    Log2(("%s: registering area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    AssertMsg(   (enmType == PCI_ADDRESS_SPACE_MEM && cb >= LSILOGIC_PCI_SPACE_MEM_SIZE)
              || (enmType == PCI_ADDRESS_SPACE_IO  && cb >= LSILOGIC_PCI_SPACE_IO_SIZE),
              ("PCI region type and size do not match\n"));

    if (enmType == PCI_ADDRESS_SPACE_MEM && iRegion == 1)
    {
        /*
         * Non-4-byte read access to LSILOGIC_REG_REPLY_QUEUE may cause real strange behavior
         * because the data is part of a physical guest address.  But some drivers use 1-byte
         * access to scan for SCSI controllers.  So, we simplify our code by telling IOM to
         * read DWORDs.
         *
         * Regarding writes, we couldn't find anything specific in the specs about what should
         * happen. So far we've ignored unaligned writes and assumed the missing bytes of
         * byte and word access to be zero. We suspect that IOMMMIO_FLAGS_WRITE_ONLY_DWORD
         * or IOMMMIO_FLAGS_WRITE_DWORD_ZEROED would be the most appropriate here, but since we
         * don't have real hw to test one, the old behavior is kept exactly like it used to be.
         */
        /** @todo Check out unaligned writes and non-dword writes on real LsiLogic
         *        hardware. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   lsilogicMMIOWrite, lsilogicMMIORead, pcszCtrl);
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, NIL_RTR0PTR /*pvUser*/,
                                         "lsilogicMMIOWrite", "lsilogicMMIORead");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/,
                                         "lsilogicMMIOWrite", "lsilogicMMIORead");
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->GCPhysMMIOBase = GCPhysAddress;
    }
    else if (enmType == PCI_ADDRESS_SPACE_MEM && iRegion == 2)
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   lsilogicDiagnosticWrite, lsilogicDiagnosticRead, pcszDiag);
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, NIL_RTR0PTR /*pvUser*/,
                                         "lsilogicDiagnosticWrite", "lsilogicDiagnosticRead");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/,
                                         "lsilogicDiagnosticWrite", "lsilogicDiagnosticRead");
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    else if (enmType == PCI_ADDRESS_SPACE_IO)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                     NULL, lsilogicIOPortWrite, lsilogicIOPortRead, NULL, NULL, pcszCtrl);
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                           0, "lsilogicIOPortWrite", "lsilogicIOPortRead", NULL, NULL, pcszCtrl);
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterRC(pDevIns, (RTIOPORT)GCPhysAddress, LSILOGIC_PCI_SPACE_IO_SIZE,
                                           0, "lsilogicIOPortWrite", "lsilogicIOPortRead", NULL, NULL, pcszCtrl);
            if (RT_FAILURE(rc))
                return rc;
        }

        pThis->IOPortBase = (RTIOPORT)GCPhysAddress;
    }
    else
        AssertMsgFailed(("Invalid enmType=%d iRegion=%d\n", enmType, iRegion));

    return rc;
}

/**
 * @callback_method_impl{PFNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) lsilogicR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    bool          fVerbose = false;

    /*
     * Parse args.
     */
    if (pszArgs)
        fVerbose = strstr(pszArgs, "verbose") != NULL;

    /*
     * Show info.
     */
    pHlp->pfnPrintf(pHlp,
                    "%s#%d: port=%RTiop mmio=%RGp max-devices=%u GC=%RTbool R0=%RTbool\n",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance,
                    pThis->IOPortBase, pThis->GCPhysMMIOBase,
                    pThis->cDeviceStates,
                    pThis->fGCEnabled ? true : false,
                    pThis->fR0Enabled ? true : false);

    /*
     * Show general state.
     */
    pHlp->pfnPrintf(pHlp, "enmState=%u\n", pThis->enmState);
    pHlp->pfnPrintf(pHlp, "enmWhoInit=%u\n", pThis->enmWhoInit);
    pHlp->pfnPrintf(pHlp, "fDoorbellInProgress=%RTbool\n", pThis->fDoorbellInProgress);
    pHlp->pfnPrintf(pHlp, "fDiagnosticEnabled=%RTbool\n", pThis->fDiagnosticEnabled);
    pHlp->pfnPrintf(pHlp, "fNotificationSend=%RTbool\n", pThis->fNotificationSend);
    pHlp->pfnPrintf(pHlp, "fEventNotificationEnabled=%RTbool\n", pThis->fEventNotificationEnabled);
    pHlp->pfnPrintf(pHlp, "uInterruptMask=%#x\n", pThis->uInterruptMask);
    pHlp->pfnPrintf(pHlp, "uInterruptStatus=%#x\n", pThis->uInterruptStatus);
    pHlp->pfnPrintf(pHlp, "u16IOCFaultCode=%#06x\n", pThis->u16IOCFaultCode);
    pHlp->pfnPrintf(pHlp, "u32HostMFAHighAddr=%#x\n", pThis->u32HostMFAHighAddr);
    pHlp->pfnPrintf(pHlp, "u32SenseBufferHighAddr=%#x\n", pThis->u32SenseBufferHighAddr);
    pHlp->pfnPrintf(pHlp, "cMaxDevices=%u\n", pThis->cMaxDevices);
    pHlp->pfnPrintf(pHlp, "cMaxBuses=%u\n", pThis->cMaxBuses);
    pHlp->pfnPrintf(pHlp, "cbReplyFrame=%u\n", pThis->cbReplyFrame);
    pHlp->pfnPrintf(pHlp, "cReplyQueueEntries=%u\n", pThis->cReplyQueueEntries);
    pHlp->pfnPrintf(pHlp, "cRequestQueueEntries=%u\n", pThis->cRequestQueueEntries);
    pHlp->pfnPrintf(pHlp, "cPorts=%u\n", pThis->cPorts);

    /*
     * Show queue status.
     */
    pHlp->pfnPrintf(pHlp, "uReplyFreeQueueNextEntryFreeWrite=%u\n", pThis->uReplyFreeQueueNextEntryFreeWrite);
    pHlp->pfnPrintf(pHlp, "uReplyFreeQueueNextAddressRead=%u\n", pThis->uReplyFreeQueueNextAddressRead);
    pHlp->pfnPrintf(pHlp, "uReplyPostQueueNextEntryFreeWrite=%u\n", pThis->uReplyPostQueueNextEntryFreeWrite);
    pHlp->pfnPrintf(pHlp, "uReplyPostQueueNextAddressRead=%u\n", pThis->uReplyPostQueueNextAddressRead);
    pHlp->pfnPrintf(pHlp, "uRequestQueueNextEntryFreeWrite=%u\n", pThis->uRequestQueueNextEntryFreeWrite);
    pHlp->pfnPrintf(pHlp, "uRequestQueueNextAddressRead=%u\n", pThis->uRequestQueueNextAddressRead);

    /*
     * Show queue content if verbose
     */
    if (fVerbose)
    {
        for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
            pHlp->pfnPrintf(pHlp, "RFQ[%u]=%#x\n", i, pThis->pReplyFreeQueueBaseR3[i]);

        pHlp->pfnPrintf(pHlp, "\n");

        for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
            pHlp->pfnPrintf(pHlp, "RPQ[%u]=%#x\n", i, pThis->pReplyPostQueueBaseR3[i]);

        pHlp->pfnPrintf(pHlp, "\n");

        for (unsigned i = 0; i < pThis->cRequestQueueEntries; i++)
            pHlp->pfnPrintf(pHlp, "ReqQ[%u]=%#x\n", i, pThis->pRequestQueueBaseR3[i]);
    }

    /*
     * Print the device status.
     */
    for (unsigned i = 0; i < pThis->cDeviceStates; i++)
    {
        PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[i];

        pHlp->pfnPrintf(pHlp, "\n");

        pHlp->pfnPrintf(pHlp, "Device[%u]: device-attached=%RTbool cOutstandingRequests=%u\n",
                        i, pDevice->pDrvBase != NULL, pDevice->cOutstandingRequests);
    }
}

/**
 * Allocate the queues.
 *
 * @returns VBox status code.
 *
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static int lsilogicR3QueuesAlloc(PLSILOGICSCSI pThis)
{
    PVM pVM = PDMDevHlpGetVM(pThis->pDevInsR3);
    uint32_t cbQueues;

    Assert(!pThis->pReplyFreeQueueBaseR3);

    cbQueues  = 2*pThis->cReplyQueueEntries * sizeof(uint32_t);
    cbQueues += pThis->cRequestQueueEntries * sizeof(uint32_t);
    int rc = MMHyperAlloc(pVM, cbQueues, 1, MM_TAG_PDM_DEVICE_USER,
                          (void **)&pThis->pReplyFreeQueueBaseR3);
    if (RT_FAILURE(rc))
        return VERR_NO_MEMORY;
    pThis->pReplyFreeQueueBaseR0 = MMHyperR3ToR0(pVM, (void *)pThis->pReplyFreeQueueBaseR3);
    pThis->pReplyFreeQueueBaseRC = MMHyperR3ToRC(pVM, (void *)pThis->pReplyFreeQueueBaseR3);

    pThis->pReplyPostQueueBaseR3 = pThis->pReplyFreeQueueBaseR3 + pThis->cReplyQueueEntries;
    pThis->pReplyPostQueueBaseR0 = MMHyperR3ToR0(pVM, (void *)pThis->pReplyPostQueueBaseR3);
    pThis->pReplyPostQueueBaseRC = MMHyperR3ToRC(pVM, (void *)pThis->pReplyPostQueueBaseR3);

    pThis->pRequestQueueBaseR3   = pThis->pReplyPostQueueBaseR3 + pThis->cReplyQueueEntries;
    pThis->pRequestQueueBaseR0   = MMHyperR3ToR0(pVM, (void *)pThis->pRequestQueueBaseR3);
    pThis->pRequestQueueBaseRC   = MMHyperR3ToRC(pVM, (void *)pThis->pRequestQueueBaseR3);

    return VINF_SUCCESS;
}

/**
 * Free the hyper memory used or the queues.
 *
 * @returns nothing.
 *
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicR3QueuesFree(PLSILOGICSCSI pThis)
{
    PVM pVM = PDMDevHlpGetVM(pThis->pDevInsR3);
    int rc = VINF_SUCCESS;

    AssertPtr(pThis->pReplyFreeQueueBaseR3);

    rc = MMHyperFree(pVM, (void *)pThis->pReplyFreeQueueBaseR3);
    AssertRC(rc);

    pThis->pReplyFreeQueueBaseR3 = NULL;
    pThis->pReplyPostQueueBaseR3 = NULL;
    pThis->pRequestQueueBaseR3   = NULL;
}

/**
 * Kicks the controller to process pending tasks after the VM was resumed
 * or loaded from a saved state.
 *
 * @returns nothing.
 * @param   pThis       Pointer to the LsiLogic device state.
 */
static void lsilogicR3Kick(PLSILOGICSCSI pThis)
{
    if (pThis->fNotificationSend)
    {
        /* Send a notifier to the PDM queue that there are pending requests. */
        PPDMQUEUEITEMCORE pItem = PDMQueueAlloc(pThis->CTX_SUFF(pNotificationQueue));
        AssertMsg(pItem, ("Allocating item for queue failed\n"));
        PDMQueueInsert(pThis->CTX_SUFF(pNotificationQueue), (PPDMQUEUEITEMCORE)pItem);
    }
    else if (pThis->VBoxSCSI.fBusy)
    {
        /* The BIOS had a request active when we got suspended. Resume it. */
        int rc = lsilogicR3PrepareBiosScsiRequest(pThis);
        AssertRC(rc);
    }

}


/*
 * Saved state.
 */

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) lsilogicR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    SSMR3PutU32(pSSM, pThis->enmCtrlType);
    SSMR3PutU32(pSSM, pThis->cDeviceStates);
    SSMR3PutU32(pSSM, pThis->cPorts);

    /* Save the device config. */
    for (unsigned i = 0; i < pThis->cDeviceStates; i++)
        SSMR3PutBool(pSSM, pThis->paDeviceStates[i].pDrvBase != NULL);

    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) lsilogicR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    /* Every device first. */
    lsilogicR3LiveExec(pDevIns, pSSM, SSM_PASS_FINAL);
    for (unsigned i = 0; i < pThis->cDeviceStates; i++)
    {
        PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        SSMR3PutU32(pSSM, pDevice->cOutstandingRequests);
    }
    /* Now the main device state. */
    SSMR3PutU32   (pSSM, pThis->enmState);
    SSMR3PutU32   (pSSM, pThis->enmWhoInit);
    SSMR3PutBool  (pSSM, pThis->fDoorbellInProgress);
    SSMR3PutBool  (pSSM, pThis->fDiagnosticEnabled);
    SSMR3PutBool  (pSSM, pThis->fNotificationSend);
    SSMR3PutBool  (pSSM, pThis->fEventNotificationEnabled);
    SSMR3PutU32   (pSSM, pThis->uInterruptMask);
    SSMR3PutU32   (pSSM, pThis->uInterruptStatus);
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aMessage); i++)
        SSMR3PutU32   (pSSM, pThis->aMessage[i]);
    SSMR3PutU32   (pSSM, pThis->iMessage);
    SSMR3PutU32   (pSSM, pThis->cMessage);
    SSMR3PutMem   (pSSM, &pThis->ReplyBuffer, sizeof(pThis->ReplyBuffer));
    SSMR3PutU32   (pSSM, pThis->uNextReplyEntryRead);
    SSMR3PutU32   (pSSM, pThis->cReplySize);
    SSMR3PutU16   (pSSM, pThis->u16IOCFaultCode);
    SSMR3PutU32   (pSSM, pThis->u32HostMFAHighAddr);
    SSMR3PutU32   (pSSM, pThis->u32SenseBufferHighAddr);
    SSMR3PutU8    (pSSM, pThis->cMaxDevices);
    SSMR3PutU8    (pSSM, pThis->cMaxBuses);
    SSMR3PutU16   (pSSM, pThis->cbReplyFrame);
    SSMR3PutU32   (pSSM, pThis->iDiagnosticAccess);
    SSMR3PutU32   (pSSM, pThis->cReplyQueueEntries);
    SSMR3PutU32   (pSSM, pThis->cRequestQueueEntries);
    SSMR3PutU32   (pSSM, pThis->uReplyFreeQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pThis->uReplyFreeQueueNextAddressRead);
    SSMR3PutU32   (pSSM, pThis->uReplyPostQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pThis->uReplyPostQueueNextAddressRead);
    SSMR3PutU32   (pSSM, pThis->uRequestQueueNextEntryFreeWrite);
    SSMR3PutU32   (pSSM, pThis->uRequestQueueNextAddressRead);

    for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
        SSMR3PutU32(pSSM, pThis->pReplyFreeQueueBaseR3[i]);
    for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
        SSMR3PutU32(pSSM, pThis->pReplyPostQueueBaseR3[i]);
    for (unsigned i = 0; i < pThis->cRequestQueueEntries; i++)
        SSMR3PutU32(pSSM, pThis->pRequestQueueBaseR3[i]);

    SSMR3PutU16   (pSSM, pThis->u16NextHandle);

    PMptConfigurationPagesSupported pPages = pThis->pConfigurationPages;

    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage0, sizeof(MptConfigurationPageManufacturing0));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage1, sizeof(MptConfigurationPageManufacturing1));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage2, sizeof(MptConfigurationPageManufacturing2));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage3, sizeof(MptConfigurationPageManufacturing3));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage4, sizeof(MptConfigurationPageManufacturing4));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage5, sizeof(MptConfigurationPageManufacturing5));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage6, sizeof(MptConfigurationPageManufacturing6));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage8, sizeof(MptConfigurationPageManufacturing8));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage9, sizeof(MptConfigurationPageManufacturing9));
    SSMR3PutMem   (pSSM, &pPages->ManufacturingPage10, sizeof(MptConfigurationPageManufacturing10));
    SSMR3PutMem   (pSSM, &pPages->IOUnitPage0, sizeof(MptConfigurationPageIOUnit0));
    SSMR3PutMem   (pSSM, &pPages->IOUnitPage1, sizeof(MptConfigurationPageIOUnit1));
    SSMR3PutMem   (pSSM, &pPages->IOUnitPage2, sizeof(MptConfigurationPageIOUnit2));
    SSMR3PutMem   (pSSM, &pPages->IOUnitPage3, sizeof(MptConfigurationPageIOUnit3));
    SSMR3PutMem   (pSSM, &pPages->IOUnitPage4, sizeof(MptConfigurationPageIOUnit4));
    SSMR3PutMem   (pSSM, &pPages->IOCPage0, sizeof(MptConfigurationPageIOC0));
    SSMR3PutMem   (pSSM, &pPages->IOCPage1, sizeof(MptConfigurationPageIOC1));
    SSMR3PutMem   (pSSM, &pPages->IOCPage2, sizeof(MptConfigurationPageIOC2));
    SSMR3PutMem   (pSSM, &pPages->IOCPage3, sizeof(MptConfigurationPageIOC3));
    SSMR3PutMem   (pSSM, &pPages->IOCPage4, sizeof(MptConfigurationPageIOC4));
    SSMR3PutMem   (pSSM, &pPages->IOCPage6, sizeof(MptConfigurationPageIOC6));
    SSMR3PutMem   (pSSM, &pPages->BIOSPage1, sizeof(MptConfigurationPageBIOS1));
    SSMR3PutMem   (pSSM, &pPages->BIOSPage2, sizeof(MptConfigurationPageBIOS2));
    SSMR3PutMem   (pSSM, &pPages->BIOSPage4, sizeof(MptConfigurationPageBIOS4));

    /* Device dependent pages */
    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
    {
        PMptConfigurationPagesSpi pSpiPages = &pPages->u.SpiPages;

        SSMR3PutMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage0, sizeof(MptConfigurationPageSCSISPIPort0));
        SSMR3PutMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage1, sizeof(MptConfigurationPageSCSISPIPort1));
        SSMR3PutMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage2, sizeof(MptConfigurationPageSCSISPIPort2));

        for (unsigned i = 0; i < RT_ELEMENTS(pSpiPages->aBuses[0].aDevicePages); i++)
        {
            SSMR3PutMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage0, sizeof(MptConfigurationPageSCSISPIDevice0));
            SSMR3PutMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage1, sizeof(MptConfigurationPageSCSISPIDevice1));
            SSMR3PutMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage2, sizeof(MptConfigurationPageSCSISPIDevice2));
            SSMR3PutMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage3, sizeof(MptConfigurationPageSCSISPIDevice3));
        }
    }
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
    {
        PMptConfigurationPagesSas pSasPages = &pPages->u.SasPages;

        SSMR3PutU32(pSSM, pSasPages->cbManufacturingPage7);
        SSMR3PutU32(pSSM, pSasPages->cbSASIOUnitPage0);
        SSMR3PutU32(pSSM, pSasPages->cbSASIOUnitPage1);

        SSMR3PutMem(pSSM, pSasPages->pManufacturingPage7, pSasPages->cbManufacturingPage7);
        SSMR3PutMem(pSSM, pSasPages->pSASIOUnitPage0, pSasPages->cbSASIOUnitPage0);
        SSMR3PutMem(pSSM, pSasPages->pSASIOUnitPage1, pSasPages->cbSASIOUnitPage1);

        SSMR3PutMem(pSSM, &pSasPages->SASIOUnitPage2, sizeof(MptConfigurationPageSASIOUnit2));
        SSMR3PutMem(pSSM, &pSasPages->SASIOUnitPage3, sizeof(MptConfigurationPageSASIOUnit3));

        SSMR3PutU32(pSSM, pSasPages->cPHYs);
        for (unsigned i = 0; i < pSasPages->cPHYs; i++)
        {
            SSMR3PutMem(pSSM, &pSasPages->paPHYs[i].SASPHYPage0, sizeof(MptConfigurationPageSASPHY0));
            SSMR3PutMem(pSSM, &pSasPages->paPHYs[i].SASPHYPage1, sizeof(MptConfigurationPageSASPHY1));
        }

        /* The number of devices first. */
        SSMR3PutU32(pSSM, pSasPages->cDevices);

        PMptSASDevice pCurr = pSasPages->pSASDeviceHead;

        while (pCurr)
        {
            SSMR3PutMem(pSSM, &pCurr->SASDevicePage0, sizeof(MptConfigurationPageSASDevice0));
            SSMR3PutMem(pSSM, &pCurr->SASDevicePage1, sizeof(MptConfigurationPageSASDevice1));
            SSMR3PutMem(pSSM, &pCurr->SASDevicePage2, sizeof(MptConfigurationPageSASDevice2));

            pCurr = pCurr->pNext;
        }
    }
    else
        AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));

    /* Now the data for the BIOS interface. */
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.regIdentify);
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.uTargetDevice);
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.uTxDir);
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.cbCDB);
    SSMR3PutMem   (pSSM, pThis->VBoxSCSI.abCDB, sizeof(pThis->VBoxSCSI.abCDB));
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.iCDB);
    SSMR3PutU32   (pSSM, pThis->VBoxSCSI.cbBuf);
    SSMR3PutU32   (pSSM, pThis->VBoxSCSI.iBuf);
    SSMR3PutBool  (pSSM, pThis->VBoxSCSI.fBusy);
    SSMR3PutU8    (pSSM, pThis->VBoxSCSI.enmState);
    if (pThis->VBoxSCSI.cbBuf)
        SSMR3PutMem(pSSM, pThis->VBoxSCSI.pbBuf, pThis->VBoxSCSI.cbBuf);

    return SSMR3PutU32(pSSM, ~0);
}

/**
 * @callback_method_impl{FNSSMDEVLOADDONE}
 */
static DECLCALLBACK(int) lsilogicR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    lsilogicR3Kick(pThis);
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) lsilogicR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PLSILOGICSCSI   pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int             rc;

    if (    uVersion != LSILOGIC_SAVED_STATE_VERSION
        &&  uVersion != LSILOGIC_SAVED_STATE_VERSION_PRE_SAS
        &&  uVersion != LSILOGIC_SAVED_STATE_VERSION_VBOX_30)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* device config */
    if (uVersion > LSILOGIC_SAVED_STATE_VERSION_PRE_SAS)
    {
        LSILOGICCTRLTYPE enmCtrlType;
        uint32_t cDeviceStates, cPorts;

        rc = SSMR3GetU32(pSSM, (uint32_t *)&enmCtrlType);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &cDeviceStates);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &cPorts);
        AssertRCReturn(rc, rc);

        if (enmCtrlType != pThis->enmCtrlType)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Target config mismatch (Controller type): config=%d state=%d"),
                                    pThis->enmCtrlType, enmCtrlType);
        if (cDeviceStates != pThis->cDeviceStates)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Target config mismatch (Device states): config=%u state=%u"),
                                    pThis->cDeviceStates, cDeviceStates);
        if (cPorts != pThis->cPorts)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Target config mismatch (Ports): config=%u state=%u"),
                                    pThis->cPorts, cPorts);
    }
    if (uVersion > LSILOGIC_SAVED_STATE_VERSION_VBOX_30)
    {
        for (unsigned i = 0; i < pThis->cDeviceStates; i++)
        {
            bool fPresent;
            rc = SSMR3GetBool(pSSM, &fPresent);
            AssertRCReturn(rc, rc);
            if (fPresent != (pThis->paDeviceStates[i].pDrvBase != NULL))
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Target %u config mismatch: config=%RTbool state=%RTbool"),
                                         i, pThis->paDeviceStates[i].pDrvBase != NULL, fPresent);
        }
    }
    if (uPass != SSM_PASS_FINAL)
        return VINF_SUCCESS;

    /* Every device first. */
    for (unsigned i = 0; i < pThis->cDeviceStates; i++)
    {
        PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[i];

        AssertMsg(!pDevice->cOutstandingRequests,
                  ("There are still outstanding requests on this device\n"));
        SSMR3GetU32(pSSM, (uint32_t *)&pDevice->cOutstandingRequests);
    }
    /* Now the main device state. */
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->enmState);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->enmWhoInit);
    SSMR3GetBool  (pSSM, &pThis->fDoorbellInProgress);
    SSMR3GetBool  (pSSM, &pThis->fDiagnosticEnabled);
    SSMR3GetBool  (pSSM, &pThis->fNotificationSend);
    SSMR3GetBool  (pSSM, &pThis->fEventNotificationEnabled);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uInterruptMask);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uInterruptStatus);
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aMessage); i++)
        SSMR3GetU32   (pSSM, &pThis->aMessage[i]);
    SSMR3GetU32   (pSSM, &pThis->iMessage);
    SSMR3GetU32   (pSSM, &pThis->cMessage);
    SSMR3GetMem   (pSSM, &pThis->ReplyBuffer, sizeof(pThis->ReplyBuffer));
    SSMR3GetU32   (pSSM, &pThis->uNextReplyEntryRead);
    SSMR3GetU32   (pSSM, &pThis->cReplySize);
    SSMR3GetU16   (pSSM, &pThis->u16IOCFaultCode);
    SSMR3GetU32   (pSSM, &pThis->u32HostMFAHighAddr);
    SSMR3GetU32   (pSSM, &pThis->u32SenseBufferHighAddr);
    SSMR3GetU8    (pSSM, &pThis->cMaxDevices);
    SSMR3GetU8    (pSSM, &pThis->cMaxBuses);
    SSMR3GetU16   (pSSM, &pThis->cbReplyFrame);
    SSMR3GetU32   (pSSM, &pThis->iDiagnosticAccess);

    uint32_t cReplyQueueEntries, cRequestQueueEntries;
    SSMR3GetU32   (pSSM, &cReplyQueueEntries);
    SSMR3GetU32   (pSSM, &cRequestQueueEntries);

    if (   cReplyQueueEntries != pThis->cReplyQueueEntries
        || cRequestQueueEntries != pThis->cRequestQueueEntries)
    {
        LogFlow(("Reallocating queues cReplyQueueEntries=%u cRequestQueuEntries=%u\n",
                 cReplyQueueEntries, cRequestQueueEntries));
        lsilogicR3QueuesFree(pThis);
        pThis->cReplyQueueEntries = cReplyQueueEntries;
        pThis->cRequestQueueEntries = cRequestQueueEntries;
        rc = lsilogicR3QueuesAlloc(pThis);
        if (RT_FAILURE(rc))
            return rc;
    }

    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uReplyFreeQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uReplyFreeQueueNextAddressRead);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uReplyPostQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uReplyPostQueueNextAddressRead);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uRequestQueueNextEntryFreeWrite);
    SSMR3GetU32   (pSSM, (uint32_t *)&pThis->uRequestQueueNextAddressRead);

    PMptConfigurationPagesSupported pPages = pThis->pConfigurationPages;

    if (uVersion <= LSILOGIC_SAVED_STATE_VERSION_PRE_SAS)
    {
        PMptConfigurationPagesSpi pSpiPages = &pPages->u.SpiPages;
        MptConfigurationPagesSupported_SSM_V2 ConfigPagesV2;

        if (pThis->enmCtrlType != LSILOGICCTRLTYPE_SCSI_SPI)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch: Expected SPI SCSI controller"));

        SSMR3GetMem(pSSM, &ConfigPagesV2,
                    sizeof(MptConfigurationPagesSupported_SSM_V2));

        pPages->ManufacturingPage0 = ConfigPagesV2.ManufacturingPage0;
        pPages->ManufacturingPage1 = ConfigPagesV2.ManufacturingPage1;
        pPages->ManufacturingPage2 = ConfigPagesV2.ManufacturingPage2;
        pPages->ManufacturingPage3 = ConfigPagesV2.ManufacturingPage3;
        pPages->ManufacturingPage4 = ConfigPagesV2.ManufacturingPage4;
        pPages->IOUnitPage0 = ConfigPagesV2.IOUnitPage0;
        pPages->IOUnitPage1 = ConfigPagesV2.IOUnitPage1;
        pPages->IOUnitPage2 = ConfigPagesV2.IOUnitPage2;
        pPages->IOUnitPage3 = ConfigPagesV2.IOUnitPage3;
        pPages->IOCPage0 = ConfigPagesV2.IOCPage0;
        pPages->IOCPage1 = ConfigPagesV2.IOCPage1;
        pPages->IOCPage2 = ConfigPagesV2.IOCPage2;
        pPages->IOCPage3 = ConfigPagesV2.IOCPage3;
        pPages->IOCPage4 = ConfigPagesV2.IOCPage4;
        pPages->IOCPage6 = ConfigPagesV2.IOCPage6;

        pSpiPages->aPortPages[0].SCSISPIPortPage0 = ConfigPagesV2.aPortPages[0].SCSISPIPortPage0;
        pSpiPages->aPortPages[0].SCSISPIPortPage1 = ConfigPagesV2.aPortPages[0].SCSISPIPortPage1;
        pSpiPages->aPortPages[0].SCSISPIPortPage2 = ConfigPagesV2.aPortPages[0].SCSISPIPortPage2;

        for (unsigned i = 0; i < RT_ELEMENTS(pPages->u.SpiPages.aBuses[0].aDevicePages); i++)
        {
            pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage0 = ConfigPagesV2.aBuses[0].aDevicePages[i].SCSISPIDevicePage0;
            pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage1 = ConfigPagesV2.aBuses[0].aDevicePages[i].SCSISPIDevicePage1;
            pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage2 = ConfigPagesV2.aBuses[0].aDevicePages[i].SCSISPIDevicePage2;
            pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage3 = ConfigPagesV2.aBuses[0].aDevicePages[i].SCSISPIDevicePage3;
        }
    }
    else
    {
        /* Queue content */
        for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->pReplyFreeQueueBaseR3[i]);
        for (unsigned i = 0; i < pThis->cReplyQueueEntries; i++)
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->pReplyPostQueueBaseR3[i]);
        for (unsigned i = 0; i < pThis->cRequestQueueEntries; i++)
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->pRequestQueueBaseR3[i]);

        SSMR3GetU16(pSSM, &pThis->u16NextHandle);

        /* Configuration pages */
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage0, sizeof(MptConfigurationPageManufacturing0));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage1, sizeof(MptConfigurationPageManufacturing1));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage2, sizeof(MptConfigurationPageManufacturing2));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage3, sizeof(MptConfigurationPageManufacturing3));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage4, sizeof(MptConfigurationPageManufacturing4));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage5, sizeof(MptConfigurationPageManufacturing5));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage6, sizeof(MptConfigurationPageManufacturing6));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage8, sizeof(MptConfigurationPageManufacturing8));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage9, sizeof(MptConfigurationPageManufacturing9));
        SSMR3GetMem(pSSM, &pPages->ManufacturingPage10, sizeof(MptConfigurationPageManufacturing10));
        SSMR3GetMem(pSSM, &pPages->IOUnitPage0, sizeof(MptConfigurationPageIOUnit0));
        SSMR3GetMem(pSSM, &pPages->IOUnitPage1, sizeof(MptConfigurationPageIOUnit1));
        SSMR3GetMem(pSSM, &pPages->IOUnitPage2, sizeof(MptConfigurationPageIOUnit2));
        SSMR3GetMem(pSSM, &pPages->IOUnitPage3, sizeof(MptConfigurationPageIOUnit3));
        SSMR3GetMem(pSSM, &pPages->IOUnitPage4, sizeof(MptConfigurationPageIOUnit4));
        SSMR3GetMem(pSSM, &pPages->IOCPage0, sizeof(MptConfigurationPageIOC0));
        SSMR3GetMem(pSSM, &pPages->IOCPage1, sizeof(MptConfigurationPageIOC1));
        SSMR3GetMem(pSSM, &pPages->IOCPage2, sizeof(MptConfigurationPageIOC2));
        SSMR3GetMem(pSSM, &pPages->IOCPage3, sizeof(MptConfigurationPageIOC3));
        SSMR3GetMem(pSSM, &pPages->IOCPage4, sizeof(MptConfigurationPageIOC4));
        SSMR3GetMem(pSSM, &pPages->IOCPage6, sizeof(MptConfigurationPageIOC6));
        SSMR3GetMem(pSSM, &pPages->BIOSPage1, sizeof(MptConfigurationPageBIOS1));
        SSMR3GetMem(pSSM, &pPages->BIOSPage2, sizeof(MptConfigurationPageBIOS2));
        SSMR3GetMem(pSSM, &pPages->BIOSPage4, sizeof(MptConfigurationPageBIOS4));

        /* Device dependent pages */
        if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
        {
            PMptConfigurationPagesSpi pSpiPages = &pPages->u.SpiPages;

            SSMR3GetMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage0, sizeof(MptConfigurationPageSCSISPIPort0));
            SSMR3GetMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage1, sizeof(MptConfigurationPageSCSISPIPort1));
            SSMR3GetMem(pSSM, &pSpiPages->aPortPages[0].SCSISPIPortPage2, sizeof(MptConfigurationPageSCSISPIPort2));

            for (unsigned i = 0; i < RT_ELEMENTS(pSpiPages->aBuses[0].aDevicePages); i++)
            {
                SSMR3GetMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage0, sizeof(MptConfigurationPageSCSISPIDevice0));
                SSMR3GetMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage1, sizeof(MptConfigurationPageSCSISPIDevice1));
                SSMR3GetMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage2, sizeof(MptConfigurationPageSCSISPIDevice2));
                SSMR3GetMem(pSSM, &pSpiPages->aBuses[0].aDevicePages[i].SCSISPIDevicePage3, sizeof(MptConfigurationPageSCSISPIDevice3));
            }
        }
        else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
        {
            uint32_t cbPage0, cbPage1, cPHYs, cbManufacturingPage7;
            PMptConfigurationPagesSas pSasPages = &pPages->u.SasPages;

            SSMR3GetU32(pSSM, &cbManufacturingPage7);
            SSMR3GetU32(pSSM, &cbPage0);
            SSMR3GetU32(pSSM, &cbPage1);

            if (   (cbPage0 != pSasPages->cbSASIOUnitPage0)
                || (cbPage1 != pSasPages->cbSASIOUnitPage1)
                || (cbManufacturingPage7 != pSasPages->cbManufacturingPage7))
                return VERR_SSM_LOAD_CONFIG_MISMATCH;

            AssertPtr(pSasPages->pManufacturingPage7);
            AssertPtr(pSasPages->pSASIOUnitPage0);
            AssertPtr(pSasPages->pSASIOUnitPage1);

            SSMR3GetMem(pSSM, pSasPages->pManufacturingPage7, pSasPages->cbManufacturingPage7);
            SSMR3GetMem(pSSM, pSasPages->pSASIOUnitPage0, pSasPages->cbSASIOUnitPage0);
            SSMR3GetMem(pSSM, pSasPages->pSASIOUnitPage1, pSasPages->cbSASIOUnitPage1);

            SSMR3GetMem(pSSM, &pSasPages->SASIOUnitPage2, sizeof(MptConfigurationPageSASIOUnit2));
            SSMR3GetMem(pSSM, &pSasPages->SASIOUnitPage3, sizeof(MptConfigurationPageSASIOUnit3));

            SSMR3GetU32(pSSM, &cPHYs);
            if (cPHYs != pSasPages->cPHYs)
                return VERR_SSM_LOAD_CONFIG_MISMATCH;

            AssertPtr(pSasPages->paPHYs);
            for (unsigned i = 0; i < pSasPages->cPHYs; i++)
            {
                SSMR3GetMem(pSSM, &pSasPages->paPHYs[i].SASPHYPage0, sizeof(MptConfigurationPageSASPHY0));
                SSMR3GetMem(pSSM, &pSasPages->paPHYs[i].SASPHYPage1, sizeof(MptConfigurationPageSASPHY1));
            }

            /* The number of devices first. */
            SSMR3GetU32(pSSM, &pSasPages->cDevices);

            PMptSASDevice pCurr = pSasPages->pSASDeviceHead;

            for (unsigned i = 0; i < pSasPages->cDevices; i++)
            {
                SSMR3GetMem(pSSM, &pCurr->SASDevicePage0, sizeof(MptConfigurationPageSASDevice0));
                SSMR3GetMem(pSSM, &pCurr->SASDevicePage1, sizeof(MptConfigurationPageSASDevice1));
                SSMR3GetMem(pSSM, &pCurr->SASDevicePage2, sizeof(MptConfigurationPageSASDevice2));

                pCurr = pCurr->pNext;
            }

            Assert(!pCurr);
        }
        else
            AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));
    }

    /* Now the data for the BIOS interface. */
    SSMR3GetU8  (pSSM, &pThis->VBoxSCSI.regIdentify);
    SSMR3GetU8  (pSSM, &pThis->VBoxSCSI.uTargetDevice);
    SSMR3GetU8  (pSSM, &pThis->VBoxSCSI.uTxDir);
    SSMR3GetU8  (pSSM, &pThis->VBoxSCSI.cbCDB);
    SSMR3GetMem (pSSM, pThis->VBoxSCSI.abCDB, sizeof(pThis->VBoxSCSI.abCDB));
    SSMR3GetU8  (pSSM, &pThis->VBoxSCSI.iCDB);
    SSMR3GetU32 (pSSM, &pThis->VBoxSCSI.cbBuf);
    SSMR3GetU32 (pSSM, &pThis->VBoxSCSI.iBuf);
    SSMR3GetBool(pSSM, (bool *)&pThis->VBoxSCSI.fBusy);
    SSMR3GetU8  (pSSM, (uint8_t *)&pThis->VBoxSCSI.enmState);
    if (pThis->VBoxSCSI.cbBuf)
    {
        pThis->VBoxSCSI.pbBuf = (uint8_t *)RTMemAllocZ(pThis->VBoxSCSI.cbBuf);
        if (!pThis->VBoxSCSI.pbBuf)
        {
            LogRel(("LsiLogic: Out of memory during restore.\n"));
            return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY,
                                    N_("LsiLogic: Out of memory during restore\n"));
        }
        SSMR3GetMem(pSSM, pThis->VBoxSCSI.pbBuf, pThis->VBoxSCSI.cbBuf);
    }

    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    AssertMsgReturn(u32 == ~0U, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

    return VINF_SUCCESS;
}


/*
 * The device level IBASE and LED interfaces.
 */

/**
 * @interface_method_impl{PDMILEDPORTS,pfnQueryInterface, For a SCSI device.}
 *
 * @remarks Called by the scsi driver, proxying the main calls.
 */
static DECLCALLBACK(int) lsilogicR3DeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PLSILOGICDEVICE pDevice = RT_FROM_MEMBER(pInterface, LSILOGICDEVICE, ILed);
    if (iLUN == 0)
    {
        *ppLed = &pDevice->Led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) lsilogicR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PLSILOGICDEVICE pDevice = RT_FROM_MEMBER(pInterface, LSILOGICDEVICE, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDevice->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISCSIPORT, &pDevice->ISCSIPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pDevice->ILed);
    return NULL;
}


/*
 * The controller level IBASE and LED interfaces.
 */

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) lsilogicR3StatusQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PLSILOGICSCSI pThis = RT_FROM_MEMBER(pInterface, LSILOGICSCSI, ILeds);
    if (iLUN < pThis->cDeviceStates)
    {
        *ppLed = &pThis->paDeviceStates[iLUN].Led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) lsilogicR3StatusQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PLSILOGICSCSI pThis = RT_FROM_MEMBER(pInterface, LSILOGICSCSI, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThis->ILeds);
    return NULL;
}


/*
 * The PDM device interface and some helpers.
 */

/**
 * Checks if all asynchronous I/O is finished.
 *
 * Used by lsilogicR3Reset, lsilogicR3Suspend and lsilogicR3PowerOff.
 *
 * @returns true if quiesced, false if busy.
 * @param   pDevIns         The device instance.
 */
static bool lsilogicR3AllAsyncIOIsFinished(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    for (uint32_t i = 0; i < pThis->cDeviceStates; i++)
    {
        PLSILOGICDEVICE pThisDevice = &pThis->paDeviceStates[i];
        if (pThisDevice->pDrvBase)
        {
            if (pThisDevice->cOutstandingRequests != 0)
                return false;
        }
    }

    return true;
}

/**
 * @callback_method_impl{FNPDMDEVASYNCNOTIFY,
 * Callback employed by lsilogicR3Suspend and lsilogicR3PowerOff.}
 */
static DECLCALLBACK(bool) lsilogicR3IsAsyncSuspendOrPowerOffDone(PPDMDEVINS pDevIns)
{
    if (!lsilogicR3AllAsyncIOIsFinished(pDevIns))
        return false;

    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    ASMAtomicWriteBool(&pThis->fSignalIdle, false);
    return true;
}

/**
 * Common worker for ahciR3Suspend and ahciR3PowerOff.
 */
static void lsilogicR3SuspendOrPowerOff(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
    if (!lsilogicR3AllAsyncIOIsFinished(pDevIns))
        PDMDevHlpSetAsyncNotification(pDevIns, lsilogicR3IsAsyncSuspendOrPowerOffDone);
    else
    {
        ASMAtomicWriteBool(&pThis->fSignalIdle, false);

        AssertMsg(!pThis->fNotificationSend, ("The PDM Queue should be empty at this point\n"));

        if (pThis->fRedo)
        {
            /*
             * We have tasks which we need to redo. Put the message frame addresses
             * into the request queue (we save the requests).
             * Guest execution is suspended at this point so there is no race between us and
             * lsilogicRegisterWrite.
             */
            PLSILOGICTASKSTATE pTaskState = pThis->pTasksRedoHead;

            pThis->pTasksRedoHead = NULL;

            while (pTaskState)
            {
                PLSILOGICTASKSTATE pFree;

                if (!pTaskState->fBIOS)
                {
                    /* Write only the lower 32bit part of the address. */
                    ASMAtomicWriteU32(&pThis->CTX_SUFF(pRequestQueueBase)[pThis->uRequestQueueNextEntryFreeWrite],
                                      pTaskState->GCPhysMessageFrameAddr & UINT32_C(0xffffffff));

                    pThis->uRequestQueueNextEntryFreeWrite++;
                    pThis->uRequestQueueNextEntryFreeWrite %= pThis->cRequestQueueEntries;

                    pThis->fNotificationSend = true;
                }
                else
                {
                    AssertMsg(!pTaskState->pRedoNext, ("Only one BIOS task can be active!\n"));
                    vboxscsiSetRequestRedo(&pThis->VBoxSCSI, &pTaskState->PDMScsiRequest);
                }

                pFree = pTaskState;
                pTaskState = pTaskState->pRedoNext;

                RTMemCacheFree(pThis->hTaskCache, pFree);
            }
            pThis->fRedo = false;
        }
    }
}

/**
 * @interface_method_impl{PDMDEVREG,pfnSuspend}
 */
static DECLCALLBACK(void) lsilogicR3Suspend(PPDMDEVINS pDevIns)
{
    Log(("lsilogicR3Suspend\n"));
    lsilogicR3SuspendOrPowerOff(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnResume}
 */
static DECLCALLBACK(void) lsilogicR3Resume(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    Log(("lsilogicR3Resume\n"));

    lsilogicR3Kick(pThis);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 */
static DECLCALLBACK(void) lsilogicR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PLSILOGICSCSI   pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[iLUN];

    if (iLUN >= pThis->cDeviceStates)
        return;

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("LsiLogic: Device does not support hotplugging\n"));

    Log(("%s:\n", __FUNCTION__));

    /*
     * Zero some important members.
     */
    pDevice->pDrvBase = NULL;
    pDevice->pDrvSCSIConnector = NULL;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int)  lsilogicR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PLSILOGICSCSI   pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[iLUN];
    int rc;

    if (iLUN >= pThis->cDeviceStates)
        return VERR_PDM_LUN_NOT_FOUND;

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("LsiLogic: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pDevice->pDrvBase);
    AssertRelease(!pDevice->pDrvSCSIConnector);
    Assert(pDevice->iLUN == iLUN);

    /*
     * Try attach the block device and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
    {
        /* Get SCSI connector interface. */
        pDevice->pDrvSCSIConnector = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMISCSICONNECTOR);
        AssertMsgReturn(pDevice->pDrvSCSIConnector, ("Missing SCSI interface below\n"), VERR_PDM_MISSING_INTERFACE);
    }
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pDevice->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pDevice->pDrvBase = NULL;
        pDevice->pDrvSCSIConnector = NULL;
    }
    return rc;
}

/**
 * Common reset worker.
 *
 * @param   pDevIns     The device instance data.
 */
static void lsilogicR3ResetCommon(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int rc;

    rc = lsilogicR3HardReset(pThis);
    AssertRC(rc);

    vboxscsiInitialize(&pThis->VBoxSCSI);
}

/**
 * @callback_method_impl{FNPDMDEVASYNCNOTIFY,
 * Callback employed by lsilogicR3Reset.}
 */
static DECLCALLBACK(bool) lsilogicR3IsAsyncResetDone(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    if (!lsilogicR3AllAsyncIOIsFinished(pDevIns))
        return false;
    ASMAtomicWriteBool(&pThis->fSignalIdle, false);

    lsilogicR3ResetCommon(pDevIns);
    return true;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) lsilogicR3Reset(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
    if (!lsilogicR3AllAsyncIOIsFinished(pDevIns))
        PDMDevHlpSetAsyncNotification(pDevIns, lsilogicR3IsAsyncResetDone);
    else
    {
        ASMAtomicWriteBool(&pThis->fSignalIdle, false);
        lsilogicR3ResetCommon(pDevIns);
    }
}

/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) lsilogicR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);

    pThis->pDevInsRC        = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pNotificationQueueRC = PDMQueueRCPtr(pThis->pNotificationQueueR3);

    /* Relocate queues. */
    pThis->pReplyFreeQueueBaseRC += offDelta;
    pThis->pReplyPostQueueBaseRC += offDelta;
    pThis->pRequestQueueBaseRC   += offDelta;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnPowerOff}
 */
static DECLCALLBACK(void) lsilogicR3PowerOff(PPDMDEVINS pDevIns)
{
    Log(("lsilogicR3PowerOff\n"));
    lsilogicR3SuspendOrPowerOff(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) lsilogicR3Destruct(PPDMDEVINS pDevIns)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    PDMR3CritSectDelete(&pThis->ReplyFreeQueueCritSect);
    PDMR3CritSectDelete(&pThis->ReplyPostQueueCritSect);

    RTMemFree(pThis->paDeviceStates);
    pThis->paDeviceStates = NULL;

    /* Destroy task cache. */
    if (pThis->hTaskCache != NIL_RTMEMCACHE)
    {
        int rc = RTMemCacheDestroy(pThis->hTaskCache); AssertRC(rc);
        pThis->hTaskCache = NIL_RTMEMCACHE;
    }

    lsilogicR3ConfigurationPagesFree(pThis);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) lsilogicR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PLSILOGICSCSI pThis = PDMINS_2_DATA(pDevIns, PLSILOGICSCSI);
    int           rc    = VINF_SUCCESS;
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Initialize enought of the state to make the destructure not trip up.
     */
    pThis->hTaskCache = NIL_RTMEMCACHE;

    /*
     * Validate and read configuration.
     */
    rc = CFGMR3AreValuesValid(pCfg, "GCEnabled\0"
                                    "R0Enabled\0"
                                    "ReplyQueueDepth\0"
                                    "RequestQueueDepth\0"
                                    "ControllerType\0"
                                    "NumPorts\0"
                                    "Bootable\0");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("LsiLogic configuration error: unknown option specified"));
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &pThis->fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, pThis->fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, pThis->fR0Enabled));

    rc = CFGMR3QueryU32Def(pCfg, "ReplyQueueDepth",
                           &pThis->cReplyQueueEntries,
                           LSILOGICSCSI_REPLY_QUEUE_DEPTH_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read ReplyQueue as integer"));
    Log(("%s: ReplyQueueDepth=%u\n", __FUNCTION__, pThis->cReplyQueueEntries));

    rc = CFGMR3QueryU32Def(pCfg, "RequestQueueDepth",
                           &pThis->cRequestQueueEntries,
                           LSILOGICSCSI_REQUEST_QUEUE_DEPTH_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read RequestQueue as integer"));
    Log(("%s: RequestQueueDepth=%u\n", __FUNCTION__, pThis->cRequestQueueEntries));

    char *pszCtrlType;
    rc = CFGMR3QueryStringAllocDef(pCfg, "ControllerType",
                                   &pszCtrlType, LSILOGICSCSI_PCI_SPI_CTRLNAME);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read ControllerType as string"));
    Log(("%s: ControllerType=%s\n", __FUNCTION__, pszCtrlType));

    rc = lsilogicR3GetCtrlTypeFromString(pThis, pszCtrlType);
    MMR3HeapFree(pszCtrlType);

    char szDevTag[20];
    RTStrPrintf(szDevTag, sizeof(szDevTag), "LSILOGIC%s-%u",
                pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI ? "SPI" : "SAS",
                iInstance);


    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to determine controller type from string"));

    rc = CFGMR3QueryU8(pCfg, "NumPorts",
                       &pThis->cPorts);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
            pThis->cPorts = LSILOGICSCSI_PCI_SPI_PORTS_MAX;
        else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
            pThis->cPorts = LSILOGICSCSI_PCI_SAS_PORTS_DEFAULT;
        else
            AssertMsgFailed(("Invalid controller type: %d\n", pThis->enmCtrlType));
    }
    else if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read NumPorts as integer"));

    bool fBootable;
    rc = CFGMR3QueryBoolDef(pCfg, "Bootable", &fBootable, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("LsiLogic configuration error: failed to read Bootable as boolean"));
    Log(("%s: Bootable=%RTbool\n", __FUNCTION__, fBootable));

    /* Init static parts. */
    PCIDevSetVendorId(&pThis->PciDev, LSILOGICSCSI_PCI_VENDOR_ID); /* LsiLogic */

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
    {
        PCIDevSetDeviceId         (&pThis->PciDev, LSILOGICSCSI_PCI_SPI_DEVICE_ID); /* LSI53C1030 */
        PCIDevSetSubSystemVendorId(&pThis->PciDev, LSILOGICSCSI_PCI_SPI_SUBSYSTEM_VENDOR_ID);
        PCIDevSetSubSystemId      (&pThis->PciDev, LSILOGICSCSI_PCI_SPI_SUBSYSTEM_ID);
    }
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
    {
        PCIDevSetDeviceId         (&pThis->PciDev, LSILOGICSCSI_PCI_SAS_DEVICE_ID); /* SAS1068 */
        PCIDevSetSubSystemVendorId(&pThis->PciDev, LSILOGICSCSI_PCI_SAS_SUBSYSTEM_VENDOR_ID);
        PCIDevSetSubSystemId      (&pThis->PciDev, LSILOGICSCSI_PCI_SAS_SUBSYSTEM_ID);
    }
    else
        AssertMsgFailed(("Invalid controller type: %d\n", pThis->enmCtrlType));

    PCIDevSetClassProg   (&pThis->PciDev,   0x00); /* SCSI */
    PCIDevSetClassSub    (&pThis->PciDev,   0x00); /* SCSI */
    PCIDevSetClassBase   (&pThis->PciDev,   0x01); /* Mass storage */
    PCIDevSetInterruptPin(&pThis->PciDev,   0x01); /* Interrupt pin A */

# ifdef VBOX_WITH_MSI_DEVICES
    PCIDevSetStatus(&pThis->PciDev,   VBOX_PCI_STATUS_CAP_LIST);
    PCIDevSetCapabilityList(&pThis->PciDev, 0x80);
# endif

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->IBase.pfnQueryInterface = lsilogicR3StatusQueryInterface;
    pThis->ILeds.pfnQueryStatusLed = lsilogicR3StatusQueryStatusLed;

    /*
     * Create critical sections protecting the reply post and free queues.
     * Note! We do our own syncronization, so NOP the default crit sect for the device.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->ReplyFreeQueueCritSect, RT_SRC_POS, "%sRFQ", szDevTag);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic: cannot create critical section for reply free queue"));

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->ReplyPostQueueCritSect, RT_SRC_POS, "%sRPQ", szDevTag);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic: cannot create critical section for reply post queue"));

    /*
     * Register the PCI device, it's I/O regions.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;

# ifdef VBOX_WITH_MSI_DEVICES
    PDMMSIREG MsiReg;
    RT_ZERO(MsiReg);
    /* use this code for MSI-X support */
#  if 0
    MsiReg.cMsixVectors    = 1;
    MsiReg.iMsixCapOffset  = 0x80;
    MsiReg.iMsixNextOffset = 0x00;
    MsiReg.iMsixBar        = 3;
#  else
    MsiReg.cMsiVectors     = 1;
    MsiReg.iMsiCapOffset   = 0x80;
    MsiReg.iMsiNextOffset  = 0x00;
#  endif
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
    if (RT_FAILURE (rc))
    {
        LogRel(("Chipset cannot do MSI: %Rrc\n", rc));
        /* That's OK, we can work without MSI */
        PCIDevSetCapabilityList(&pThis->PciDev, 0x0);
    }
# endif

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, LSILOGIC_PCI_SPACE_IO_SIZE, PCI_ADDRESS_SPACE_IO, lsilogicR3Map);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, LSILOGIC_PCI_SPACE_MEM_SIZE, PCI_ADDRESS_SPACE_MEM, lsilogicR3Map);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, LSILOGIC_PCI_SPACE_MEM_SIZE, PCI_ADDRESS_SPACE_MEM, lsilogicR3Map);
    if (RT_FAILURE(rc))
        return rc;

    /* Initialize task queue. (Need two items to handle SMP guest concurrency.) */
    char szTaggedText[64];
    RTStrPrintf(szTaggedText, sizeof(szTaggedText), "%s-Task", szDevTag);
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 2, 0,
                              lsilogicR3NotifyQueueConsumer, true,
                              szTaggedText,
                              &pThis->pNotificationQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotificationQueueR0 = PDMQueueR0Ptr(pThis->pNotificationQueueR3);
    pThis->pNotificationQueueRC = PDMQueueRCPtr(pThis->pNotificationQueueR3);

    /*
     * We need one entry free in the queue.
     */
    pThis->cReplyQueueEntries++;
    pThis->cRequestQueueEntries++;

    /*
     * Allocate memory for the queues.
     */
    rc = lsilogicR3QueuesAlloc(pThis);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate task cache.
     */
    rc = RTMemCacheCreate(&pThis->hTaskCache, sizeof(LSILOGICTASKSTATE), 0, UINT32_MAX,
                          lsilogicR3TaskStateCtor, lsilogicR3TaskStateDtor, NULL, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Cannot create task cache"));

    if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
        pThis->cDeviceStates = pThis->cPorts * LSILOGICSCSI_PCI_SPI_DEVICES_PER_BUS_MAX;
    else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
        pThis->cDeviceStates = pThis->cPorts * LSILOGICSCSI_PCI_SAS_DEVICES_PER_PORT_MAX;
    else
        AssertMsgFailed(("Invalid controller type: %d\n", pThis->enmCtrlType));

    /*
     * Allocate device states.
     */
    pThis->paDeviceStates = (PLSILOGICDEVICE)RTMemAllocZ(sizeof(LSILOGICDEVICE) * pThis->cDeviceStates);
    if (!pThis->paDeviceStates)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to allocate memory for device states"));

    for (unsigned i = 0; i < pThis->cDeviceStates; i++)
    {
        char szName[24];
        PLSILOGICDEVICE pDevice = &pThis->paDeviceStates[i];

        /* Initialize static parts of the device. */
        pDevice->iLUN                              = i;
        pDevice->pLsiLogicR3                       = pThis;
        pDevice->Led.u32Magic                      = PDMLED_MAGIC;
        pDevice->IBase.pfnQueryInterface           = lsilogicR3DeviceQueryInterface;
        pDevice->ISCSIPort.pfnSCSIRequestCompleted = lsilogicR3DeviceSCSIRequestCompleted;
        pDevice->ISCSIPort.pfnQueryDeviceLocation  = lsilogicR3QueryDeviceLocation;
        pDevice->ILed.pfnQueryStatusLed            = lsilogicR3DeviceQueryStatusLed;

        RTStrPrintf(szName, sizeof(szName), "Device%u", i);

        /* Attach SCSI driver. */
        rc = PDMDevHlpDriverAttach(pDevIns, pDevice->iLUN, &pDevice->IBase, &pDevice->pDrvBase, szName);
        if (RT_SUCCESS(rc))
        {
            /* Get SCSI connector interface. */
            pDevice->pDrvSCSIConnector = PDMIBASE_QUERY_INTERFACE(pDevice->pDrvBase, PDMISCSICONNECTOR);
            AssertMsgReturn(pDevice->pDrvSCSIConnector, ("Missing SCSI interface below\n"), VERR_PDM_MISSING_INTERFACE);
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pDevice->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("LsiLogic: no driver attached to device %s\n", szName));
        }
        else
        {
            AssertLogRelMsgFailed(("LsiLogic: Failed to attach %s\n", szName));
            return rc;
        }
    }

    /*
     * Attach status driver (optional).
     */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot attach to status driver"));
    }

    /* Initialize the SCSI emulation for the BIOS. */
    rc = vboxscsiInitialize(&pThis->VBoxSCSI);
    AssertRC(rc);

    /*
     * Register I/O port space in ISA region for BIOS access
     * if the controller is marked as bootable.
     */
    if (fBootable)
    {
        if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI)
            rc = PDMDevHlpIOPortRegister(pDevIns, LSILOGIC_BIOS_IO_PORT, 4, NULL,
                                         lsilogicR3IsaIOPortWrite, lsilogicR3IsaIOPortRead,
                                         lsilogicR3IsaIOPortWriteStr, lsilogicR3IsaIOPortReadStr,
                                         "LsiLogic BIOS");
        else if (pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SAS)
            rc = PDMDevHlpIOPortRegister(pDevIns, LSILOGIC_SAS_BIOS_IO_PORT, 4, NULL,
                                         lsilogicR3IsaIOPortWrite, lsilogicR3IsaIOPortRead,
                                         lsilogicR3IsaIOPortWriteStr, lsilogicR3IsaIOPortReadStr,
                                         "LsiLogic SAS BIOS");
        else
            AssertMsgFailed(("Invalid controller type %d\n", pThis->enmCtrlType));

        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot register legacy I/O handlers"));
    }

    /* Register save state handlers. */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, LSILOGIC_SAVED_STATE_VERSION, sizeof(*pThis), NULL,
                                NULL, lsilogicR3LiveExec, NULL,
                                NULL, lsilogicR3SaveExec, NULL,
                                NULL, lsilogicR3LoadExec, lsilogicR3LoadDone);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("LsiLogic cannot register save state handlers"));

    pThis->enmWhoInit = LSILOGICWHOINIT_SYSTEM_BIOS;

    /*
     * Register the info item.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%u", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp,
                              pThis->enmCtrlType == LSILOGICCTRLTYPE_SCSI_SPI
                              ? "LsiLogic SPI info."
                              : "LsiLogic SAS info.", lsilogicR3Info);

    /* Perform hard reset. */
    rc = lsilogicR3HardReset(pThis);
    AssertRC(rc);

    return rc;
}

/**
 * The device registration structure - SPI SCSI controller.
 */
const PDMDEVREG g_DeviceLsiLogicSCSI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "lsilogicscsi",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "LSI Logic 53c1030 SCSI controller.\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0 |
    PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(LSILOGICSCSI),
    /* pfnConstruct */
    lsilogicR3Construct,
    /* pfnDestruct */
    lsilogicR3Destruct,
    /* pfnRelocate */
    lsilogicR3Relocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    lsilogicR3Reset,
    /* pfnSuspend */
    lsilogicR3Suspend,
    /* pfnResume */
    lsilogicR3Resume,
    /* pfnAttach */
    lsilogicR3Attach,
    /* pfnDetach */
    lsilogicR3Detach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    lsilogicR3PowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

/**
 * The device registration structure - SAS controller.
 */
const PDMDEVREG g_DeviceLsiLogicSAS =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "lsilogicsas",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "LSI Logic SAS1068 controller.\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0 |
    PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION |
    PDM_DEVREG_FLAGS_FIRST_RESET_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(LSILOGICSCSI),
    /* pfnConstruct */
    lsilogicR3Construct,
    /* pfnDestruct */
    lsilogicR3Destruct,
    /* pfnRelocate */
    lsilogicR3Relocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    lsilogicR3Reset,
    /* pfnSuspend */
    lsilogicR3Suspend,
    /* pfnResume */
    lsilogicR3Resume,
    /* pfnAttach */
    lsilogicR3Attach,
    /* pfnDetach */
    lsilogicR3Detach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    lsilogicR3PowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
