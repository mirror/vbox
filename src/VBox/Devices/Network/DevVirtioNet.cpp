/* $Id$ */
/** @file
 * DevVirtioNet - Virtio Network Device
 *
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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


#define LOG_GROUP LOG_GROUP_DEV_VIRTIO_NET
#define VNET_GC_SUPPORT

#include <VBox/pdmdev.h>
#include <iprt/semaphore.h>
#ifdef IN_RING3
# include <iprt/mem.h>
#endif /* IN_RING3 */
#include "../Builtins.h"
#include "../VirtIO/Virtio.h"


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define INSTANCE(pState) pState->VPCI.szInstance
#define IFACE_TO_STATE(pIface, ifaceName) \
        ((VNETSTATE *)((char*)pIface - RT_OFFSETOF(VNETSTATE, ifaceName)))
#define STATUS pState->config.uStatus

#ifdef IN_RING3

#define VNET_PCI_SUBSYSTEM_ID        1 + VIRTIO_NET_ID
#define VNET_PCI_CLASS               0x0200
#define VNET_N_QUEUES                3
#define VNET_NAME_FMT                "VNet%d"

#if 0
/* Virtio Block Device */
#define VNET_PCI_SUBSYSTEM_ID        1 + VIRTIO_BLK_ID
#define VNET_PCI_CLASS               0x0180
#define VNET_N_QUEUES                2
#define VNET_NAME_FMT                "VBlk%d"
#endif

#endif /* IN_RING3 */

/* Forward declarations ******************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) vnetIOPortIn (PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vnetIOPortOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t u32, unsigned cb);
RT_C_DECLS_END

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */


#define VNET_TX_DELAY           150   /* 150 microseconds */
#define VNET_MAX_FRAME_SIZE     65536  // TODO: Is it the right limit?
#define VNET_MAC_FILTER_LEN     32
#define VNET_MAX_VID            (1 << 12)

/* Virtio net features */
#define VNET_F_CSUM       0x00000001  /* Host handles pkts w/ partial csum */
#define VNET_F_GUEST_CSUM 0x00000002  /* Guest handles pkts w/ partial csum */
#define VNET_F_MAC        0x00000020  /* Host has given MAC address. */
#define VNET_F_GSO        0x00000040  /* Host handles pkts w/ any GSO type */
#define VNET_F_GUEST_TSO4 0x00000080  /* Guest can handle TSOv4 in. */
#define VNET_F_GUEST_TSO6 0x00000100  /* Guest can handle TSOv6 in. */
#define VNET_F_GUEST_ECN  0x00000200  /* Guest can handle TSO[6] w/ ECN in. */
#define VNET_F_GUEST_UFO  0x00000400  /* Guest can handle UFO in. */
#define VNET_F_HOST_TSO4  0x00000800  /* Host can handle TSOv4 in. */
#define VNET_F_HOST_TSO6  0x00001000  /* Host can handle TSOv6 in. */
#define VNET_F_HOST_ECN   0x00002000  /* Host can handle TSO[6] w/ ECN in. */
#define VNET_F_HOST_UFO   0x00004000  /* Host can handle UFO in. */
#define VNET_F_MRG_RXBUF  0x00008000  /* Host can merge receive buffers. */
#define VNET_F_STATUS     0x00010000  /* virtio_net_config.status available */
#define VNET_F_CTRL_VQ    0x00020000  /* Control channel available */
#define VNET_F_CTRL_RX    0x00040000  /* Control channel RX mode support */
#define VNET_F_CTRL_VLAN  0x00080000  /* Control channel VLAN filtering */

#define VNET_S_LINK_UP    1


#ifdef _MSC_VER
struct VNetPCIConfig
#else /* !_MSC_VER */
struct __attribute__ ((__packed__)) VNetPCIConfig
#endif /* !_MSC_VER */
{
    RTMAC    mac;
    uint16_t uStatus;
};
AssertCompileMemberOffset(struct VNetPCIConfig, uStatus, 6);

/**
 * Device state structure. Holds the current state of device.
 */

struct VNetState_st
{
    /* VPCISTATE must be the first member! */
    VPCISTATE               VPCI;

    PDMINETWORKPORT         INetworkPort;
    PDMINETWORKCONFIG       INetworkConfig;
    R3PTRTYPE(PPDMIBASE)    pDrvBase;                 /**< Attached network driver. */
    R3PTRTYPE(PPDMINETWORKCONNECTOR) pDrv;    /**< Connector of attached network driver. */

    R3PTRTYPE(PPDMQUEUE)    pCanRxQueueR3;           /**< Rx wakeup signaller - R3. */
    R0PTRTYPE(PPDMQUEUE)    pCanRxQueueR0;           /**< Rx wakeup signaller - R0. */
    RCPTRTYPE(PPDMQUEUE)    pCanRxQueueRC;           /**< Rx wakeup signaller - RC. */

#if HC_ARCH_BITS == 64
    uint32_t    padding;
#endif

    /** transmit buffer */
    R3PTRTYPE(uint8_t*)     pTxBuf;
    /**< Link Up(/Restore) Timer. */
    PTMTIMERR3              pLinkUpTimer; 
#ifdef VNET_TX_DELAY
    /**< Transmit Delay Timer - R3. */
    PTMTIMERR3              pTxTimerR3; 
    /**< Transmit Delay Timer - R0. */
    PTMTIMERR0              pTxTimerR0; 
    /**< Transmit Delay Timer - GC. */
    PTMTIMERRC              pTxTimerRC; 
#if HC_ARCH_BITS == 64
    uint32_t    padding2;
#endif

#endif /* VNET_TX_DELAY */

    /** PCI config area holding MAC address as well as TBD. */
    struct VNetPCIConfig    config;
    /** MAC address obtained from the configuration. */
    RTMAC                   macConfigured;
    /** True if physical cable is attached in configuration. */
    bool                    fCableConnected;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t                u32PktNo;

    /** N/A: */
    bool volatile           fMaybeOutOfSpace;

    /** Promiscuous mode -- RX filter accepts all packets. */
    bool                    fPromiscuous;
    /** AllMulti mode -- RX filter accepts all multicast packets. */
    bool                    fAllMulti;
    /** The number of actually used slots in aMacTable. */
    uint32_t                nMacFilterEntries;
    /** Array of MAC addresses accepted by RX filter. */
    RTMAC                   aMacFilter[VNET_MAC_FILTER_LEN];
    /** Bit array of VLAN filter, one bit per VLAN ID. */
    uint8_t                 aVlanFilter[VNET_MAX_VID / sizeof(uint8_t)];

#if HC_ARCH_BITS == 64
    uint32_t    padding3;
#endif

    R3PTRTYPE(PVQUEUE)      pRxQueue;
    R3PTRTYPE(PVQUEUE)      pTxQueue;
    R3PTRTYPE(PVQUEUE)      pCtlQueue;
    /* Receive-blocking-related fields ***************************************/

    /** EMT: Gets signalled when more RX descriptors become available. */
    RTSEMEVENT  hEventMoreRxDescAvail;

    /* Statistic fields ******************************************************/

    STAMCOUNTER             StatReceiveBytes;
    STAMCOUNTER             StatTransmitBytes;
#if defined(VBOX_WITH_STATISTICS)
    STAMPROFILEADV          StatReceive;
    STAMPROFILEADV          StatTransmit;
    STAMPROFILEADV          StatTransmitSend;
    STAMPROFILE             StatRxOverflow;
    STAMCOUNTER             StatRxOverflowWakeup;
#endif /* VBOX_WITH_STATISTICS */

};
typedef struct VNetState_st VNETSTATE;
typedef VNETSTATE *PVNETSTATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define VNETHDR_GSO_NONE 0

struct VNetHdr
{
    uint8_t  u8Flags;
    uint8_t  u8GSOType;
    uint16_t u16HdrLen;
    uint16_t u16GSOSize;
    uint16_t u16CSumStart;
    uint16_t u16CSumOffset;
};
typedef struct VNetHdr VNETHDR;
typedef VNETHDR *PVNETHDR;
AssertCompileSize(VNETHDR, 10);

AssertCompileMemberOffset(VNETSTATE, VPCI, 0);

#define VNET_OK                    0
#define VNET_ERROR                 1
typedef uint8_t VNETCTLACK;

#define VNET_CTRL_CLS_RX_MODE          0
#define VNET_CTRL_CMD_RX_MODE_PROMISC  0
#define VNET_CTRL_CMD_RX_MODE_ALLMULTI 1

#define VNET_CTRL_CLS_MAC              1
#define VNET_CTRL_CMD_MAC_TABLE_SET    0

#define VNET_CTRL_CLS_VLAN             2
#define VNET_CTRL_CMD_VLAN_ADD         0
#define VNET_CTRL_CMD_VLAN_DEL         1


struct VNetCtlHdr
{
    uint8_t  u8Class;
    uint8_t  u8Command;
};
typedef struct VNetCtlHdr VNETCTLHDR;
typedef VNETCTLHDR *PVNETCTLHDR;
AssertCompileSize(VNETCTLHDR, 2);

DECLINLINE(int) vnetCsEnter(PVNETSTATE pState, int rcBusy)
{
    return vpciCsEnter(&pState->VPCI, rcBusy);
}

DECLINLINE(void) vnetCsLeave(PVNETSTATE pState)
{
    vpciCsLeave(&pState->VPCI);
}


PDMBOTHCBDECL(uint32_t) vnetGetHostFeatures(void *pvState)
{
    /* We support:
     * - Host-provided MAC address
     * - Link status reporting in config space
     * - Control queue
     * - RX mode setting
     * - MAC filter table
     * - VLAN filter
     */
    return VNET_F_MAC
        | VNET_F_STATUS
        | VNET_F_CTRL_VQ
        | VNET_F_CTRL_RX
        | VNET_F_CTRL_VLAN;
}

PDMBOTHCBDECL(uint32_t) vnetGetHostMinimalFeatures(void *pvState)
{
    return VNET_F_MAC;
}

PDMBOTHCBDECL(void) vnetSetHostFeatures(void *pvState, uint32_t uFeatures)
{
    // TODO: Nothing to do here yet
    VNETSTATE *pState = (VNETSTATE *)pvState;
    LogFlow(("%s vnetSetHostFeatures: uFeatures=%x\n", INSTANCE(pState), uFeatures));
}

PDMBOTHCBDECL(int) vnetGetConfig(void *pvState, uint32_t port, uint32_t cb, void *data)
{
    VNETSTATE *pState = (VNETSTATE *)pvState;
    if (port + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetGetConfig: Read beyond the config structure is attempted (port=%RTiop cb=%x).\n", INSTANCE(pState), port, cb));
        return VERR_INTERNAL_ERROR;
    }
    memcpy(data, ((uint8_t*)&pState->config) + port, cb);
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) vnetSetConfig(void *pvState, uint32_t port, uint32_t cb, void *data)
{
    VNETSTATE *pState = (VNETSTATE *)pvState;
    if (port + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetGetConfig: Write beyond the config structure is attempted (port=%RTiop cb=%x).\n", INSTANCE(pState), port, cb));
        return VERR_INTERNAL_ERROR;
    }
    memcpy(((uint8_t*)&pState->config) + port, data, cb);
    return VINF_SUCCESS;
}

/**
 * Hardware reset. Revert all registers to initial values.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vnetReset(void *pvState)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Reset triggered\n", INSTANCE(pState)));
    vpciReset(&pState->VPCI);
    // TODO: Implement reset
    if (pState->fCableConnected)
        STATUS = VNET_S_LINK_UP;
    else
        STATUS = 0;
    /*
     * By default we pass all packets up since the older guests cannot control
     * virtio mode.
     */
    pState->fPromiscuous      = true;
    pState->fAllMulti         = false;
    pState->nMacFilterEntries = 0;
    memset(pState->aMacFilter,  0, VNET_MAC_FILTER_LEN * sizeof(RTMAC));
    memset(pState->aVlanFilter, 0, sizeof(pState->aVlanFilter));
}

#ifdef IN_RING3
/**
 * Wakeup the RX thread.
 */
static void vnetWakeupReceive(PPDMDEVINS pDevIns)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE *);
    if (    pState->fMaybeOutOfSpace
        &&  pState->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        STAM_COUNTER_INC(&pState->StatRxOverflowWakeup);
        Log(("%s Waking up Out-of-RX-space semaphore\n",  INSTANCE(pState)));
        RTSemEventSignal(pState->hEventMoreRxDescAvail);
    }
}

/**
 * Link Up Timer handler.
 *
 * @param   pDevIns     Pointer to device instance structure.
 * @param   pTimer      Pointer to the timer.
 * @param   pvUser      NULL.
 * @thread  EMT
 */
static DECLCALLBACK(void) vnetLinkUpTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VNETSTATE *pState = (VNETSTATE *)pvUser;

    STATUS |= VNET_S_LINK_UP;
    vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
    vnetWakeupReceive(pDevIns);
}




/**
 * Handler for the wakeup signaller queue.
 */
static DECLCALLBACK(bool) vnetCanRxQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    vnetWakeupReceive(pDevIns);
    return true;
}

#endif /* IN_RING3 */

/**
 * This function is called when the driver becomes ready.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vnetReady(void *pvState)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Driver became ready, waking up RX thread...\n", INSTANCE(pState)));
#ifdef IN_RING3
    vnetWakeupReceive(pState->VPCI.CTX_SUFF(pDevIns));
#else
    PPDMQUEUEITEMCORE pItem = PDMQueueAlloc(pState->CTX_SUFF(pCanRxQueue));
    if (pItem)
        PDMQueueInsert(pState->CTX_SUFF(pCanRxQueue), pItem);
#endif
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      Pointer to the device state structure.
 * @param   port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) vnetIOPortIn(PPDMDEVINS pDevIns, void *pvUser,
                                RTIOPORT port, uint32_t *pu32, unsigned cb)
{
    return vpciIOPortIn(pDevIns, pvUser, port, pu32, cb,
                        vnetGetHostFeatures,
                        vnetGetConfig);
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) vnetIOPortOut(PPDMDEVINS pDevIns, void *pvUser,
                                 RTIOPORT port, uint32_t u32, unsigned cb)
{
    return vpciIOPortOut(pDevIns, pvUser, port, u32, cb,
                         vnetGetHostMinimalFeatures,
                         vnetGetHostFeatures,
                         vnetSetHostFeatures,
                         vnetReset,
                         vnetReady,
                         vnetSetConfig);
}


#ifdef IN_RING3

/**
 * Check if the device can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @remarks As a side effect this function enables queue notification
 *          if it cannot receive because the queue is empty.
 *          It disables notification if it can receive.
 *
 * @returns VERR_NET_NO_BUFFER_SPACE if it cannot.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  RX
 */
static int vnetCanReceive(VNETSTATE *pState)
{
    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    LogFlow(("%s vnetCanReceive\n", INSTANCE(pState)));
    if (!(pState->VPCI.uStatus & VPCI_STATUS_DRV_OK))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (!vqueueIsReady(&pState->VPCI, pState->pRxQueue))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (vqueueIsEmpty(&pState->VPCI, pState->pRxQueue))
    {
        vringSetNotification(&pState->VPCI, &pState->pRxQueue->VRing, true);
        rc = VERR_NET_NO_BUFFER_SPACE;
    }
    else
    {
        vringSetNotification(&pState->VPCI, &pState->pRxQueue->VRing, false);
        rc = VINF_SUCCESS;
    }

    LogFlow(("%s vnetCanReceive -> %Vrc\n", INSTANCE(pState), rc));
    vnetCsLeave(pState);
    return rc;
}

static DECLCALLBACK(int) vnetWaitReceiveAvail(PPDMINETWORKPORT pInterface, unsigned cMillies)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    LogFlow(("%s vnetWaitReceiveAvail(cMillies=%u)\n", INSTANCE(pState), cMillies));
    int rc = vnetCanReceive(pState);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    if (RT_UNLIKELY(cMillies == 0))
        return VERR_NET_NO_BUFFER_SPACE;

    rc = VERR_INTERRUPTED;
    ASMAtomicXchgBool(&pState->fMaybeOutOfSpace, true);
    STAM_PROFILE_START(&pState->StatRxOverflow, a);

    VMSTATE enmVMState;
    while (RT_LIKELY(   (enmVMState = PDMDevHlpVMState(pState->VPCI.CTX_SUFF(pDevIns))) == VMSTATE_RUNNING
                     ||  enmVMState == VMSTATE_RUNNING_LS))
    {
        int rc2 = vnetCanReceive(pState);
        if (RT_SUCCESS(rc2))
        {
            rc = VINF_SUCCESS;
            break;
        }
        Log(("%s vnetWaitReceiveAvail: waiting cMillies=%u...\n",
                INSTANCE(pState), cMillies));
        RTSemEventWait(pState->hEventMoreRxDescAvail, cMillies);
    }
    STAM_PROFILE_STOP(&pState->StatRxOverflow, a);
    ASMAtomicXchgBool(&pState->fMaybeOutOfSpace, false);

    LogFlow(("%s vnetWaitReceiveAvail -> %d\n", INSTANCE(pState), rc));
    return rc;
}


/**
 * Provides interfaces to the driver.
 *
 * @returns Pointer to interface. NULL if the interface is not supported.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  EMT
 */
static DECLCALLBACK(void *) vnetQueryInterface(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, VPCI.IBase);
    Assert(&pState->VPCI.IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_NETWORK_PORT:
            return &pState->INetworkPort;
        case PDMINTERFACE_NETWORK_CONFIG:
            return &pState->INetworkConfig;
        default:
            return vpciQueryInterface(pInterface, enmInterface);
    }
}

/**
 * Returns true if it is a broadcast packet.
 *
 * @returns true if destination address indicates broadcast.
 * @param   pvBuf           The ethernet packet.
 */
DECLINLINE(bool) vnetIsBroadcast(const void *pvBuf)
{
    static const uint8_t s_abBcastAddr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    return memcmp(pvBuf, s_abBcastAddr, sizeof(s_abBcastAddr)) == 0;
}

/**
 * Returns true if it is a multicast packet.
 *
 * @remarks returns true for broadcast packets as well.
 * @returns true if destination address indicates multicast.
 * @param   pvBuf           The ethernet packet.
 */
DECLINLINE(bool) vnetIsMulticast(const void *pvBuf)
{
    return (*(char*)pvBuf) & 1;
}

/**
 * Determines if the packet is to be delivered to upper layer.
 *
 * @returns true if packet is intended for this node.
 * @param   pState          Pointer to the state structure.
 * @param   pvBuf           The ethernet packet.
 * @param   cb              Number of bytes available in the packet.
 */
static bool vnetAddressFilter(PVNETSTATE pState, const void *pvBuf, size_t cb)
{
    if (pState->fPromiscuous)
        return true;

    /* Ignore everything outside of our VLANs */
    uint16_t *u16Ptr = (uint16_t*)pvBuf;
    /* Compare TPID with VLAN Ether Type */
    if (   u16Ptr[6] == RT_H2BE_U16(0x8100)
        && !ASMBitTest(pState->aVlanFilter, RT_BE2H_U16(u16Ptr[7]) & 0xFFF))
        return false;

    if (vnetIsBroadcast(pvBuf))
        return true;

    if (pState->fAllMulti && vnetIsMulticast(pvBuf))
        return true;

    if (!memcmp(pState->config.mac.au8, pvBuf, sizeof(RTMAC)))
        return true;

    for (unsigned i = 0; i < pState->nMacFilterEntries; i++)
        if (!memcmp(&pState->aMacFilter[i], pvBuf, sizeof(RTMAC)))
            return true;

    return false;
}

/**
 * Pad and store received packet.
 *
 * @remarks Make sure that the packet appears to upper layer as one coming
 *          from real Ethernet: pad it and insert FCS.
 *
 * @returns VBox status code.
 * @param   pState          The device state structure.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static int vnetHandleRxPacket(PVNETSTATE pState, const void *pvBuf, size_t cb)
{
    VNETHDR hdr;

    hdr.u8Flags   = 0;
    hdr.u8GSOType = VNETHDR_GSO_NONE;

    unsigned int uOffset = 0;
    for (unsigned int nElem = 0; uOffset < cb; nElem++)
    {
        VQUEUEELEM elem;
        unsigned int nSeg = 0, uElemSize = 0;

        if (!vqueueGet(&pState->VPCI, pState->pRxQueue, &elem))
        {
            Log(("%s vnetHandleRxPacket: Suddenly there is no space in receive queue!\n", INSTANCE(pState)));
            return VERR_INTERNAL_ERROR;
        }

        if (elem.nIn < 1)
        {
            Log(("%s vnetHandleRxPacket: No writable descriptors in receive queue!\n", INSTANCE(pState)));
            return VERR_INTERNAL_ERROR;
        }

        if (nElem == 0)
        {
            /* The very first segment of the very first element gets the header. */
            if (elem.aSegsIn[nSeg].cb != sizeof(VNETHDR))
            {
                Log(("%s vnetHandleRxPacket: The first descriptor does match the header size!\n", INSTANCE(pState)));
                return VERR_INTERNAL_ERROR;
            }

            elem.aSegsIn[nSeg++].pv = &hdr;
            uElemSize += sizeof(VNETHDR);
        }

        while (nSeg < elem.nIn && uOffset < cb)
        {
            unsigned int uSize = RT_MIN(elem.aSegsIn[nSeg].cb, cb - uOffset);
            elem.aSegsIn[nSeg++].pv = (uint8_t*)pvBuf + uOffset;
            uOffset += uSize;
            uElemSize += uSize;
        }
        vqueuePut(&pState->VPCI, pState->pRxQueue, &elem, uElemSize);
    }
    vqueueSync(&pState->VPCI, pState->pRxQueue);

    return VINF_SUCCESS;
}

/**
 * Receive data from the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static DECLCALLBACK(int) vnetReceive(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    int        rc = VINF_SUCCESS;

    Log2(("%s vnetReceive: pvBuf=%p cb=%u\n", INSTANCE(pState), pvBuf, cb));
    rc = vnetCanReceive(pState);
    if (RT_FAILURE(rc))
        return rc;

    /* Drop packets if VM is not running or cable is disconnected. */
    VMSTATE enmVMState = PDMDevHlpVMState(pState->VPCI.CTX_SUFF(pDevIns));
    if ((   enmVMState != VMSTATE_RUNNING
         && enmVMState != VMSTATE_RUNNING_LS)
        || !(STATUS & VNET_S_LINK_UP))
        return VINF_SUCCESS;

    STAM_PROFILE_ADV_START(&pState->StatReceive, a);
    vpciSetReadLed(&pState->VPCI, true);
    if (vnetAddressFilter(pState, pvBuf, cb))
    {
        rc = vnetHandleRxPacket(pState, pvBuf, cb);
        STAM_REL_COUNTER_ADD(&pState->StatReceiveBytes, cb);
    }
    vpciSetReadLed(&pState->VPCI, false);
    STAM_PROFILE_ADV_STOP(&pState->StatReceive, a);

    return rc;
}

/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    memcpy(pMac, pState->config.mac.au8, sizeof(RTMAC));
    return VINF_SUCCESS;
}

/**
 * Gets the new link state.
 *
 * @returns The current link state.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) vnetGetLinkState(PPDMINETWORKCONFIG pInterface)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    if (STATUS & VNET_S_LINK_UP)
        return PDMNETWORKLINKSTATE_UP;
    return PDMNETWORKLINKSTATE_DOWN;
}


/**
 * Sets the new link state.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmState        The new link state
 */
static DECLCALLBACK(int) vnetSetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    bool fOldUp = !!(STATUS & VNET_S_LINK_UP);
    bool fNewUp = enmState == PDMNETWORKLINKSTATE_UP;

    if (fNewUp != fOldUp)
    {
        if (fNewUp)
        {
            Log(("%s Link is up\n", INSTANCE(pState)));
            STATUS |= VNET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        else
        {
            Log(("%s Link is down\n", INSTANCE(pState)));
            STATUS &= ~VNET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        if (pState->pDrv)
            pState->pDrv->pfnNotifyLinkChanged(pState->pDrv, enmState);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vnetQueueReceive(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Receive buffers has been added, waking up receive thread.\n", INSTANCE(pState)));
    vnetWakeupReceive(pState->VPCI.CTX_SUFF(pDevIns));
}

static DECLCALLBACK(void) vnetTransmitPendingPackets(PVNETSTATE pState, PVQUEUE pQueue)
{
    if ((pState->VPCI.uStatus & VPCI_STATUS_DRV_OK) == 0)
    {
        Log(("%s Ignoring transmit requests from non-existent driver (status=0x%x).\n",
             INSTANCE(pState), pState->VPCI.uStatus));
        return;
    }

    vpciSetWriteLed(&pState->VPCI, true);

    VQUEUEELEM elem;
    while (vqueueGet(&pState->VPCI, pQueue, &elem))
    {
        unsigned int uOffset = 0;
        if (elem.nOut < 2 || elem.aSegsOut[0].cb != sizeof(VNETHDR))
        {
            Log(("%s vnetQueueTransmit: The first segment is not the header! (%u < 2 || %u != %u).\n",
                 INSTANCE(pState), elem.nOut, elem.aSegsOut[0].cb, sizeof(VNETHDR)));
            break; /* For now we simply ignore the header, but it must be there anyway! */
        }
        else
        {
            STAM_PROFILE_ADV_START(&pState->StatTransmit, a);
            /* Assemble a complete frame. */
            for (unsigned int i = 1; i < elem.nOut && uOffset < VNET_MAX_FRAME_SIZE; i++)
            {
                unsigned int uSize = elem.aSegsOut[i].cb;
                if (uSize > VNET_MAX_FRAME_SIZE - uOffset)
                {
                    Log(("%s vnetQueueTransmit: Packet is too big (>64k), truncating...\n", INSTANCE(pState)));
                    uSize = VNET_MAX_FRAME_SIZE - uOffset;
                }
                PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns), elem.aSegsOut[i].addr,
                                  pState->pTxBuf + uOffset, uSize);
                uOffset += uSize;
            }
            STAM_PROFILE_ADV_START(&pState->StatTransmitSend, a);
            int rc = pState->pDrv->pfnSend(pState->pDrv, pState->pTxBuf, uOffset);
            STAM_PROFILE_ADV_STOP(&pState->StatTransmitSend, a);
            STAM_REL_COUNTER_ADD(&pState->StatTransmitBytes, uOffset);
        }
        vqueuePut(&pState->VPCI, pQueue, &elem, sizeof(VNETHDR) + uOffset);
        vqueueSync(&pState->VPCI, pQueue);
        STAM_PROFILE_ADV_STOP(&pState->StatTransmit, a);
    }
    vpciSetWriteLed(&pState->VPCI, false);
}

#ifdef VNET_TX_DELAY
static DECLCALLBACK(void) vnetQueueTransmit(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;

    if (TMTimerIsActive(pState->CTX_SUFF(pTxTimer)))
    {
        int rc = TMTimerStop(pState->CTX_SUFF(pTxTimer));
        vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, true);
        Log3(("%s vnetQueueTransmit: Got kicked with notification disabled, "
              "re-enable notification and flush TX queue\n", INSTANCE(pState)));
        vnetTransmitPendingPackets(pState, pQueue);
    }
    else
    {
        TMTimerSetMicro(pState->CTX_SUFF(pTxTimer), VNET_TX_DELAY);
        vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, false);
    }
}

/**
 * Transmit Delay Timer handler.
 *
 * @remarks We only get here when the timer expires.
 *
 * @param   pDevIns     Pointer to device instance structure.
 * @param   pTimer      Pointer to the timer.
 * @param   pvUser      NULL.
 * @thread  EMT
 */
static DECLCALLBACK(void) vnetTxTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VNETSTATE *pState = (VNETSTATE*)pvUser;

    vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, true);
    Log3(("%s vnetTxTimer: Expired, %d packets pending\n", INSTANCE(pState), 
          vringReadAvailIndex(&pState->VPCI, &pState->pTxQueue->VRing) - pState->pTxQueue->uNextAvailIndex));
    vnetTransmitPendingPackets(pState, pState->pTxQueue);
}

#else /* !VNET_TX_DELAY */
static DECLCALLBACK(void) vnetQueueTransmit(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;

    vnetTransmitPendingPackets(pState, pQueue);
}
#endif /* !VNET_TX_DELAY */

static uint8_t vnetControlRx(PVNETSTATE pState, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint8_t u8Ack = VNET_OK;
    uint8_t fOn;
    PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &fOn, sizeof(fOn));
    Log(("%s vnetControlRx: uCommand=%u fOn=%u\n", INSTANCE(pState), pCtlHdr->u8Command, fOn));
    switch (pCtlHdr->u8Command)
    {
        case VNET_CTRL_CMD_RX_MODE_PROMISC:
            pState->fPromiscuous = !!fOn;
            break;
        case VNET_CTRL_CMD_RX_MODE_ALLMULTI:
            pState->fAllMulti = !!fOn;
            break;
        default:
            u8Ack = VNET_ERROR;
    }

    return u8Ack;
}

static uint8_t vnetControlMac(PVNETSTATE pState, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint32_t nMacs = 0;

    if (pCtlHdr->u8Command != VNET_CTRL_CMD_MAC_TABLE_SET
        || pElem->nOut != 3
        || pElem->aSegsOut[1].cb < sizeof(nMacs)
        || pElem->aSegsOut[2].cb < sizeof(nMacs))
    {
        Log(("%s vnetControlMac: Segment layout is wrong "
             "(u8Command=%u nOut=%u cb1=%u cb2=%u)\n", INSTANCE(pState),
             pCtlHdr->u8Command, pElem->nOut,
             pElem->aSegsOut[1].cb, pElem->aSegsOut[2].cb));
        return VNET_ERROR;
    }

    /* Load unicast addresses */
    PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &nMacs, sizeof(nMacs));

    if (pElem->aSegsOut[1].cb < nMacs * sizeof(RTMAC) + sizeof(nMacs))
    {
        Log(("%s vnetControlMac: The unicast mac segment is too small "
             "(nMacs=%u cb=%u)\n", INSTANCE(pState), pElem->aSegsOut[1].cb));
        return VNET_ERROR;
    }

    if (nMacs > VNET_MAC_FILTER_LEN)
    {
        Log(("%s vnetControlMac: MAC table is too big, have to use promiscuous"
             " mode (nMacs=%u)\n", INSTANCE(pState), nMacs));
        pState->fPromiscuous = true;
    }
    else
    {
        if (nMacs)
            PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                              pElem->aSegsOut[1].addr + sizeof(nMacs),
                              pState->aMacFilter, nMacs * sizeof(RTMAC));
        pState->nMacFilterEntries = nMacs;
#ifdef DEBUG
        Log(("%s vnetControlMac: unicast macs:\n", INSTANCE(pState)));
        for(unsigned i = 0; i < nMacs; i++)
            Log(("         %RTmac\n", &pState->aMacFilter[i]));
#endif /* DEBUG */
    }

    /* Load multicast addresses */
    PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[2].addr,
                      &nMacs, sizeof(nMacs));

    if (pElem->aSegsOut[2].cb < nMacs * sizeof(RTMAC) + sizeof(nMacs))
    {
        Log(("%s vnetControlMac: The multicast mac segment is too small "
             "(nMacs=%u cb=%u)\n", INSTANCE(pState), pElem->aSegsOut[2].cb));
        return VNET_ERROR;
    }

    if (nMacs > VNET_MAC_FILTER_LEN - pState->nMacFilterEntries)
    {
        Log(("%s vnetControlMac: MAC table is too big, have to use allmulti"
             " mode (nMacs=%u)\n", INSTANCE(pState), nMacs));
        pState->fAllMulti = true;
    }
    else
    {
        if (nMacs)
            PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                              pElem->aSegsOut[2].addr + sizeof(nMacs),
                              &pState->aMacFilter[pState->nMacFilterEntries],
                              nMacs * sizeof(RTMAC));
#ifdef DEBUG
        Log(("%s vnetControlMac: multicast macs:\n", INSTANCE(pState)));
        for(unsigned i = 0; i < nMacs; i++)
            Log(("         %RTmac\n",
                 &pState->aMacFilter[i+pState->nMacFilterEntries]));
#endif /* DEBUG */
        pState->nMacFilterEntries += nMacs;
    }

    return VNET_OK;
}

static uint8_t vnetControlVlan(PVNETSTATE pState, PVNETCTLHDR pCtlHdr, PVQUEUEELEM pElem)
{
    uint8_t  u8Ack = VNET_OK;
    uint16_t u16Vid;

    if (pElem->nOut != 2 || pElem->aSegsOut[1].cb != sizeof(u16Vid))
    {
        Log(("%s vnetControlVlan: Segment layout is wrong "
             "(u8Command=%u nOut=%u cb=%u)\n", INSTANCE(pState),
             pCtlHdr->u8Command, pElem->nOut, pElem->aSegsOut[1].cb));
        return VNET_ERROR;
    }

    PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                      pElem->aSegsOut[1].addr,
                      &u16Vid, sizeof(u16Vid));

    if (u16Vid >= VNET_MAX_VID)
    {
        Log(("%s vnetControlVlan: VLAN ID is out of range "
             "(VID=%u)\n", INSTANCE(pState), u16Vid));
        return VNET_ERROR;
    }

    Log(("%s vnetControlVlan: uCommand=%u VID=%u\n", INSTANCE(pState),
         pCtlHdr->u8Command, u16Vid));

    switch (pCtlHdr->u8Command)
    {
        case VNET_CTRL_CMD_VLAN_ADD:
            ASMBitSet(pState->aVlanFilter, u16Vid);
            break;
        case VNET_CTRL_CMD_VLAN_DEL:
            ASMBitClear(pState->aVlanFilter, u16Vid);
            break;
        default:
            u8Ack = VNET_ERROR;
    }

    return u8Ack;
}


static DECLCALLBACK(void) vnetQueueControl(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    uint8_t u8Ack;
    VQUEUEELEM elem;
    while (vqueueGet(&pState->VPCI, pQueue, &elem))
    {
        unsigned int uOffset = 0;
        if (elem.nOut < 1 || elem.aSegsOut[0].cb < sizeof(VNETCTLHDR))
        {
            Log(("%s vnetQueueControl: The first 'out' segment is not the "
                 "header! (%u < 1 || %u < %u).\n", INSTANCE(pState), elem.nOut,
                 elem.aSegsOut[0].cb,sizeof(VNETCTLHDR)));
            break; /* Skip the element and hope the next one is good. */
        }
        else if (   elem.nIn < 1
                 || elem.aSegsIn[elem.nIn - 1].cb < sizeof(VNETCTLACK))
        {
            Log(("%s vnetQueueControl: The last 'in' segment is too small "
                 "to hold the acknowledge! (%u < 1 || %u < %u).\n",
                 INSTANCE(pState), elem.nIn, elem.aSegsIn[elem.nIn - 1].cb,
                 sizeof(VNETCTLACK)));
            break; /* Skip the element and hope the next one is good. */
        }
        else
        {
            VNETCTLHDR CtlHdr;
            PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns),
                              elem.aSegsOut[0].addr,
                              &CtlHdr, sizeof(CtlHdr));
            switch (CtlHdr.u8Class)
            {
                case VNET_CTRL_CLS_RX_MODE:
                    u8Ack = vnetControlRx(pState, &CtlHdr, &elem);
                    break;
                case VNET_CTRL_CLS_MAC:
                    u8Ack = vnetControlMac(pState, &CtlHdr, &elem);
                    break;
                case VNET_CTRL_CLS_VLAN:
                    u8Ack = vnetControlVlan(pState, &CtlHdr, &elem);
                    break;
                default:
                    u8Ack = VNET_ERROR;
            }
            Log(("%s Processed control message %u, ack=%u.\n", INSTANCE(pState),
                 CtlHdr.u8Class, u8Ack));
            PDMDevHlpPhysWrite(pState->VPCI.CTX_SUFF(pDevIns),
                               elem.aSegsIn[elem.nIn - 1].addr,
                               &u8Ack, sizeof(u8Ack));
        }
        vqueuePut(&pState->VPCI, pQueue, &elem, sizeof(u8Ack));
        vqueueSync(&pState->VPCI, pQueue);
    }
}

/**
 * Saves the configuration.
 *
 * @param   pState      The VNET state.
 * @param   pSSM        The handle to the saved state.
 */
static void vnetSaveConfig(VNETSTATE *pState, PSSMHANDLE pSSM)
{
    SSMR3PutMem(pSSM, &pState->macConfigured, sizeof(pState->macConfigured));
}

/**
 * Live save - save basic configuration.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 * @param   uPass
 */
static DECLCALLBACK(int) vnetLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    vnetSaveConfig(pState, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Prepares for state saving.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetSavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;
    vnetCsLeave(pState);
    return VINF_SUCCESS;
}

/**
 * Saves the state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    /* Save config first */
    vnetSaveConfig(pState, pSSM);

    /* Save the common part */
    int rc = vpciSaveExec(&pState->VPCI, pSSM);
    AssertRCReturn(rc, rc);
    /* Save device-specific part */
    rc = SSMR3PutMem( pSSM, pState->config.mac.au8, sizeof(pState->config.mac));
    AssertRCReturn(rc, rc);
    rc = SSMR3PutBool(pSSM, pState->fPromiscuous);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutBool(pSSM, pState->fAllMulti);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32( pSSM, pState->nMacFilterEntries);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutMem( pSSM, pState->aMacFilter,
                      pState->nMacFilterEntries * sizeof(RTMAC));
    AssertRCReturn(rc, rc);
    rc = SSMR3PutMem( pSSM, pState->aVlanFilter, sizeof(pState->aVlanFilter));
    AssertRCReturn(rc, rc);
    Log(("%s State has been saved\n", INSTANCE(pState)));
    return VINF_SUCCESS;
}


/**
 * Serializes the receive thread, it may be working inside the critsect.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;
    vnetCsLeave(pState);
    return VINF_SUCCESS;
}

/* Takes down the link temporarily if it's current status is up.
 *
 * This is used during restore and when replumbing the network link.
 *
 * The temporary link outage is supposed to indicate to the OS that all network
 * connections have been lost and that it for instance is appropriate to
 * renegotiate any DHCP lease.
 *
 * @param  pThis        The PCNet instance data.
 */
static void vnetTempLinkDown(PVNETSTATE pState)
{
    if (STATUS & VNET_S_LINK_UP)
    {
        STATUS &= ~VNET_S_LINK_UP;
        vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        /* Restore the link back in 5 seconds. */
        int rc = TMTimerSetMillies(pState->pLinkUpTimer, 5000);
        AssertRC(rc);
    }
}


/**
 * Restore previously saved state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) vnetLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    int       rc;

    /* config checks */
    RTMAC macConfigured;
    rc = SSMR3GetMem(pSSM, &macConfigured, sizeof(macConfigured));
    AssertRCReturn(rc, rc);
    if (memcmp(&macConfigured, &pState->macConfigured, sizeof(macConfigured))
        && (uPass == 0 || !PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns)))
        LogRel(("%s: The mac address differs: config=%RTmac saved=%RTmac\n", INSTANCE(pState), &pState->macConfigured, &macConfigured));

    rc = vpciLoadExec(&pState->VPCI, pSSM, uVersion, uPass, VNET_N_QUEUES);
    AssertRCReturn(rc, rc);

    if (uPass == SSM_PASS_FINAL)
    {
        rc = SSMR3GetMem( pSSM, pState->config.mac.au8,
                          sizeof(pState->config.mac));
        AssertRCReturn(rc, rc);
        if (uVersion > VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1)
        {
            rc = SSMR3GetBool(pSSM, &pState->fPromiscuous);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetBool(pSSM, &pState->fAllMulti);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetU32(pSSM, &pState->nMacFilterEntries);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetMem(pSSM, pState->aMacFilter,
                             pState->nMacFilterEntries * sizeof(RTMAC));
            AssertRCReturn(rc, rc);
            /* Clear the rest. */
            if (pState->nMacFilterEntries < VNET_MAC_FILTER_LEN)
                memset(&pState->aMacFilter[pState->nMacFilterEntries],
                       0,
                       (VNET_MAC_FILTER_LEN - pState->nMacFilterEntries)
                       * sizeof(RTMAC));
            rc = SSMR3GetMem(pSSM, pState->aVlanFilter,
                             sizeof(pState->aVlanFilter));
            AssertRCReturn(rc, rc);
        }
        else
        {
            pState->fPromiscuous = true;
            pState->fAllMulti = false;
            pState->nMacFilterEntries = 0;
            memset(pState->aMacFilter, 0, VNET_MAC_FILTER_LEN * sizeof(RTMAC));
            memset(pState->aVlanFilter, 0, sizeof(pState->aVlanFilter));
        }
    }

    return rc;
}

/**
 * Link status adjustments after loading.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetLoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    /*
     * Indicate link down to the guest OS that all network connections have
     * been lost, unless we've been teleported here.
     */
    if (!PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns))
        vnetTempLinkDown(pState);

    return VINF_SUCCESS;
}

/**
 * Map PCI I/O region.
 *
 * @return  VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   cb              Region size.
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetMap(PPCIDEVICE pPciDev, int iRegion,
                                 RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    int       rc;
    VNETSTATE *pState = PDMINS_2_DATA(pPciDev->pDevIns, VNETSTATE*);

    if (enmType != PCI_ADDRESS_SPACE_IO)
    {
        /* We should never get here */
        AssertMsgFailed(("Invalid PCI address space param in map callback"));
        return VERR_INTERNAL_ERROR;
    }

    pState->VPCI.addrIOPort = (RTIOPORT)GCPhysAddress;
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, pState->VPCI.addrIOPort,
                                 cb, 0, vnetIOPortOut, vnetIOPortIn,
                                 NULL, NULL, "VirtioNet");
#ifdef VNET_GC_SUPPORT
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterR0(pPciDev->pDevIns, pState->VPCI.addrIOPort,
                                   cb, 0, "vnetIOPortOut", "vnetIOPortIn",
                                   NULL, NULL, "VirtioNet");
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterGC(pPciDev->pDevIns, pState->VPCI.addrIOPort,
                                   cb, 0, "vnetIOPortOut", "vnetIOPortIn",
                                   NULL, NULL, "VirtioNet");
#endif
    AssertRC(rc);
    return rc;
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    int        rc;

    /* Initialize PCI part first. */
    pState->VPCI.IBase.pfnQueryInterface     = vnetQueryInterface;
    rc = vpciConstruct(pDevIns, &pState->VPCI, iInstance,
                       VNET_NAME_FMT, VNET_PCI_SUBSYSTEM_ID,
                       VNET_PCI_CLASS, VNET_N_QUEUES);
    pState->pRxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetQueueReceive,  "RX ");
    pState->pTxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetQueueTransmit, "TX ");
    pState->pCtlQueue = vpciAddQueue(&pState->VPCI, 16,  vnetQueueControl,  "CTL");

    Log(("%s Constructing new instance\n", INSTANCE(pState)));

    pState->hEventMoreRxDescAvail = NIL_RTSEMEVENT;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "MAC\0" "CableConnected\0" "LineSpeed\0"))
                    return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                            N_("Invalid configuration for VirtioNet device"));

    /* Get config params */
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", pState->macConfigured.au8,
                          sizeof(pState->macConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get MAC address"));
    rc = CFGMR3QueryBool(pCfgHandle, "CableConnected", &pState->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the value of 'CableConnected'"));

    /* Initialize PCI config space */
    memcpy(pState->config.mac.au8, pState->macConfigured.au8, sizeof(pState->config.mac.au8));
    pState->config.uStatus = 0;

    /* Initialize state structure */
    pState->u32PktNo     = 1;

    /* Interfaces */
    pState->INetworkPort.pfnWaitReceiveAvail = vnetWaitReceiveAvail;
    pState->INetworkPort.pfnReceive          = vnetReceive;
    pState->INetworkConfig.pfnGetMac         = vnetGetMac;
    pState->INetworkConfig.pfnGetLinkState   = vnetGetLinkState;
    pState->INetworkConfig.pfnSetLinkState   = vnetSetLinkState;

    pState->pTxBuf = (uint8_t *)RTMemAllocZ(VNET_MAX_FRAME_SIZE);
    AssertMsgReturn(pState->pTxBuf, 
                    ("Cannot allocate TX buffer for virtio-net device\n"), VERR_NO_MEMORY);

    /* Map our ports to IO space. */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0,
                                      VPCI_CONFIG + sizeof(VNetPCIConfig),
                                      PCI_ADDRESS_SPACE_IO, vnetMap);
    if (RT_FAILURE(rc))
        return rc;


    /* Register save/restore state handlers. */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIO_SAVEDSTATE_VERSION, sizeof(VNETSTATE), NULL,
                                NULL,         vnetLiveExec, NULL,
                                vnetSavePrep, vnetSaveExec, NULL,
                                vnetLoadPrep, vnetLoadExec, vnetLoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /* Create the RX notifier signaller. */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 1, 0,
                                 vnetCanRxQueueConsumer, true, "VNet-Rcv", &pState->pCanRxQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pState->pCanRxQueueR0 = PDMQueueR0Ptr(pState->pCanRxQueueR3);
    pState->pCanRxQueueRC = PDMQueueRCPtr(pState->pCanRxQueueR3);

    /* Create Link Up Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetLinkUpTimer, pState,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, /** @todo check locking here. */
                                "VirtioNet Link Up Timer", &pState->pLinkUpTimer);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VNET_TX_DELAY
    /* Create Transmit Delay Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetTxTimer, pState,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, /** @todo check locking here. */
                                "VirtioNet TX Delay Timer", &pState->pTxTimerR3);
    if (RT_FAILURE(rc))
        return rc;
    pState->pTxTimerR0 = TMTimerR0Ptr(pState->pTxTimerR3);
    pState->pTxTimerRC = TMTimerRCPtr(pState->pTxTimerR3);
#endif /* VNET_TX_DELAY */

    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pState->VPCI.IBase, &pState->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
        }
        pState->pDrv = (PPDMINETWORKCONNECTOR)
            pState->pDrvBase->pfnQueryInterface(pState->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pState->pDrv)
        {
            AssertMsgFailed(("%s Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s This adapter is not attached to any network!\n", INSTANCE(pState)));
    }
    else
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the network LUN"));

    rc = RTSemEventCreate(&pState->hEventMoreRxDescAvail);
    if (RT_FAILURE(rc))
        return rc;

    vnetReset(pState);

    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatReceiveBytes,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data received",            "/Devices/VNet%d/ReceiveBytes", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmitBytes,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data transmitted",         "/Devices/VNet%d/TransmitBytes", iInstance);
#if defined(VBOX_WITH_STATISTICS)
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatReceive,            STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling receive",                  "/Devices/VNet%d/Receive/Total", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatRxOverflow,         STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling RX overflows",        "/Devices/VNet%d/RxOverflow", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatRxOverflowWakeup,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Nr of RX overflow wakeups",          "/Devices/VNet%d/RxOverflowWakeup", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmit,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling transmits in HC",          "/Devices/VNet%d/Transmit/Total", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmitSend,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling send transmit in HC",      "/Devices/VNet%d/Transmit/Send", iInstance);
#endif /* VBOX_WITH_STATISTICS */

    return VINF_SUCCESS;
}

/**
 * Destruct a device instance.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetDestruct(PPDMDEVINS pDevIns)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    Log(("%s Destroying instance\n", INSTANCE(pState)));
    if (pState->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        RTSemEventSignal(pState->hEventMoreRxDescAvail);
        RTSemEventDestroy(pState->hEventMoreRxDescAvail);
        pState->hEventMoreRxDescAvail = NIL_RTSEMEVENT;
    }

    if (pState->pTxBuf)
    {
        RTMemFree(pState->pTxBuf);
        pState->pTxBuf = NULL;
    }

    return vpciDestruct(&pState->VPCI);
}

/**
 * Device relocation callback.
 *
 * When this callback is called the device instance data, and if the
 * device have a GC component, is being relocated, or/and the selectors
 * have been changed. The device must use the chance to perform the
 * necessary pointer relocations and data updates.
 *
 * Before the GC code is executed the first time, this function will be
 * called with a 0 delta so GC pointer calculations can be one in one place.
 *
 * @param   pDevIns     Pointer to the device instance.
 * @param   offDelta    The relocation delta relative to the old location.
 *
 * @remark  A relocation CANNOT fail.
 */
static DECLCALLBACK(void) vnetRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    vpciRelocate(pDevIns, offDelta);
    pState->pCanRxQueueRC = PDMQueueRCPtr(pState->pCanRxQueueR3);
#ifdef VNET_TX_DELAY
    pState->pTxTimerRC    = TMTimerRCPtr(pState->pTxTimerR3);
#endif /* VNET_TX_DELAY */
    // TBD
}

/**
 * @copydoc FNPDMDEVSUSPEND
 */
static DECLCALLBACK(void) vnetSuspend(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetWakeupReceive(pDevIns);
}


#ifdef VBOX_DYNAMIC_NET_ATTACH
/**
 * Detach notification.
 *
 * One port on the network card has been disconnected from the network.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) vnetDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    Log(("%s vnetDetach:\n", INSTANCE(pState)));

    AssertLogRelReturnVoid(iLUN == 0);

    vnetCsEnter(pState, VERR_SEM_BUSY);

    /*
     * Zero some important members.
     */
    pState->pDrvBase = NULL;
    pState->pDrv = NULL;

    vnetCsLeave(pState);
}


/**
 * Attach the Network attachment.
 *
 * One port on the network card has been connected to a network.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being attached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 *
 * @remarks This code path is not used during construction.
 */
static DECLCALLBACK(int) vnetAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    LogFlow(("%s vnetAttach:\n",  INSTANCE(pState)));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    vnetCsEnter(pState, VERR_SEM_BUSY);

    /*
     * Attach the driver.
     */
    int rc = PDMDevHlpDriverAttach(pDevIns, 0, &pState->VPCI.IBase, &pState->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
#ifdef RT_OS_LINUX
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Please check your /etc/resolv.conf for <tt>nameserver</tt> entries. Either add one manually (<i>man resolv.conf</i>) or ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#else
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#endif
        }
        pState->pDrv = (PPDMINETWORKCONNECTOR)pState->pDrvBase->pfnQueryInterface(pState->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pState->pDrv)
        {
            AssertMsgFailed(("Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            rc = VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log(("%s No attached driver!\n", INSTANCE(pState)));


    /*
     * Temporary set the link down if it was up so that the guest
     * will know that we have change the configuration of the
     * network card
     */
    if (RT_SUCCESS(rc))
        vnetTempLinkDown(pState);

    vnetCsLeave(pState);
    return rc;

}
#endif /* VBOX_DYNAMIC_NET_ATTACH */


/**
 * @copydoc FNPDMDEVPOWEROFF
 */
static DECLCALLBACK(void) vnetPowerOff(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetWakeupReceive(pDevIns);
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioNet =
{
    /* Structure version. PDM_DEVREG_VERSION defines the current version. */
    PDM_DEVREG_VERSION,
    /* Device name. */
    "virtio-net",
    /* Name of guest context module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_RC is set. */
    "VBoxDDGC.gc",
    /* Name of ring-0 module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_RC is set. */
    "VBoxDDR0.r0",
    /* The description of the device. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    "Virtio Ethernet.\n",

    /* Flags, combination of the PDM_DEVREG_FLAGS_* \#defines. */
#ifdef VNET_GC_SUPPORT
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
#else
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
#endif
    /* Device class(es), combination of the PDM_DEVREG_CLASS_* \#defines. */
    PDM_DEVREG_CLASS_NETWORK,
    /* Maximum number of instances (per VM). */
    8,
    /* Size of the instance data. */
    sizeof(VNETSTATE),

    /* Construct instance - required. */
    vnetConstruct,
    /* Destruct instance - optional. */
    vnetDestruct,
    /* Relocation command - optional. */
    vnetRelocate,
    /* I/O Control interface - optional. */
    NULL,
    /* Power on notification - optional. */
    NULL,
    /* Reset notification - optional. */
    NULL,
    /* Suspend notification  - optional. */
    vnetSuspend,
    /* Resume notification - optional. */
    NULL,
#ifdef VBOX_DYNAMIC_NET_ATTACH
    /* Attach command - optional. */
    vnetAttach,
    /* Detach notification - optional. */
    vnetDetach,
#else /* !VBOX_DYNAMIC_NET_ATTACH */
    /* Attach command - optional. */
    NULL,
    /* Detach notification - optional. */
    NULL,
#endif /* !VBOX_DYNAMIC_NET_ATTACH */
    /* Query a LUN base interface - optional. */
    NULL,
    /* Init complete notification - optional. */
    NULL,
    /* Power off notification - optional. */
    vnetPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
