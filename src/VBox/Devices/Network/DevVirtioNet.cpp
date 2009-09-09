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

#include <iprt/ctype.h>
#ifdef IN_RING3
# include <iprt/mem.h>
#endif /* IN_RING3 */
#include <iprt/param.h>
#include <iprt/semaphore.h>
#include <VBox/pdmdev.h>
#include <VBox/tm.h>
#include "../Builtins.h"
#if 0
#include <iprt/crc32.h>
#include <iprt/string.h>
#include <VBox/vm.h>
#endif

// TODO: move declarations to the header file: #include "DevVirtioNet.h"

#define INSTANCE(pState) pState->szInstance
#define IFACE_TO_STATE(pIface, ifaceName) ((VPCISTATE *)((char*)pIface - RT_OFFSETOF(VPCISTATE, ifaceName)))
#if 0
/* Little helpers ************************************************************/
#undef htons
#undef ntohs
#undef htonl
#undef ntohl
#define htons(x) ((((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8))
#define ntohs(x) htons(x)
#define htonl(x) ASMByteSwapU32(x)
#define ntohl(x) htonl(x)

#define E1K_RELOCATE(p, o) *(RTHCUINTPTR *)&p += o

#define E1K_INC_CNT32(cnt) \
do { \
    if (cnt < UINT32_MAX) \
        cnt++; \
} while (0)

#define E1K_ADD_CNT64(cntLo, cntHi, val) \
do { \
    uint64_t u64Cnt = RT_MAKE_U64(cntLo, cntHi); \
    uint64_t tmp  = u64Cnt; \
    u64Cnt += val; \
    if (tmp > u64Cnt ) \
        u64Cnt = UINT64_MAX; \
    cntLo = (uint32_t)u64Cnt; \
    cntHi = (uint32_t)(u64Cnt >> 32); \
} while (0)

#ifdef E1K_INT_STATS
# define E1K_INC_ISTAT_CNT(cnt) ++cnt
#else /* E1K_INT_STATS */
# define E1K_INC_ISTAT_CNT(cnt)
#endif /* E1K_INT_STATS */
#endif

/*****************************************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(uint32_t) vnetGetHostFeatures(void *pState);
PDMBOTHCBDECL(int)      vnetGetConfig(void *pState, uint32_t port, uint32_t cb, void *data);
PDMBOTHCBDECL(int)      vnetSetConfig(void *pState, uint32_t port, uint32_t cb, void *data);
PDMBOTHCBDECL(void)     vnetReset(void *pState);
RT_C_DECLS_END

/*****************************************************************************/

struct VRingDesc
{
    uint64_t u64Addr;
    uint32_t uLen;
    uint16_t u16Flags;
    uint16_t u16Next;
};
typedef struct VRingDesc VRINGDESC;

struct VRing
{
    uint16_t   uSize;
    VRINGDESC *pDescriptors;
};
typedef struct VRing VRING;

struct VQueue
{
    VRING    VRing;
    uint16_t uPageNumber;
    void   (*pfnCallback)(void *pvState, struct VQueue *pQueue);
};
typedef struct VQueue VQUEUE;
typedef VQUEUE *PVQUEUE;

enum VirtioDeviceType
{
    VIRTIO_NET_ID = 0,
    VIRTIO_BLK_ID = 1
};

struct VPCIState_st
{
    /* Read-only part, never changes after initialization. */
    VirtioDeviceType        enmDevType;               /**< Device type: net or blk. */
    char                    szInstance[8];         /**< Instance name, e.g. VNet#1. */

    PDMIBASE                IBase;
    PDMILEDPORTS            ILeds;                               /**< LED interface */
    R3PTRTYPE(PPDMILEDCONNECTORS)    pLedsConnector;

    PPDMDEVINSR3            pDevInsR3;                   /**< Device instance - R3. */
    PPDMDEVINSR0            pDevInsR0;                   /**< Device instance - R0. */
    PPDMDEVINSRC            pDevInsRC;                   /**< Device instance - RC. */

    /** TODO */
    PCIDEVICE   pciDevice;
    /** Base port of I/O space region. */
    RTIOPORT    addrIOPort;

    /* Read/write part, protected with critical section. */
    PDMCRITSECT cs;                  /**< Critical section - what is it protecting? */
    /** Status LED. */
    PDMLED      led;

    uint32_t    uGuestFeatures;
    uint16_t    uQueueSelector; /**< An index in aQueues array. */
    uint8_t     uStatus; /**< Device Status (bits are device-specific). */
    uint8_t     uISR; /**< Interrupt Status Register. */
    PVQUEUE     pQueues;

#if defined(VBOX_WITH_STATISTICS)
    STAMPROFILEADV                      StatIOReadGC;
    STAMPROFILEADV                      StatIOReadHC;
    STAMPROFILEADV                      StatIOWriteGC;
    STAMPROFILEADV                      StatIOWriteHC;
    STAMCOUNTER                         StatIntsRaised;
#endif /* VBOX_WITH_STATISTICS */
};
typedef struct VPCIState_st VPCISTATE;

struct VirtioPCIDevices
{
    uint16_t    uPCIVendorId;
    uint16_t    uPCIDeviceId;
    uint16_t    uPCISubsystemVendorId;
    uint16_t    uPCISubsystemId;
    uint16_t    uPCIClass;
    unsigned    nQueues;
    const char *pcszName;
    const char *pcszNameFmt;
    uint32_t  (*pfnGetHostFeatures)(void *pvState);
    int       (*pfnGetConfig)(void *pvState, uint32_t port, uint32_t cb, void *data);
    int       (*pfnSetConfig)(void *pvState, uint32_t port, uint32_t cb, void *data);
    void      (*pfnReset)(void *pvState);
} g_VPCIDevices[] =
{
    /* Vendor Device SSVendor SubSys             Class   Name */
    { 0x1AF4, 0x1000, 0x1AF4, 1 + VIRTIO_NET_ID, 0x0200, 3, "virtio-net", "vnet%d",
      vnetGetHostFeatures, vnetGetConfig, vnetSetConfig, vnetReset }, /* Virtio Network Device */
    { 0x1AF4, 0x1001, 0x1AF4, 1 + VIRTIO_BLK_ID, 0x0180, 2, "virtio-blk", "vblk%d",
      NULL, NULL, NULL, NULL }, /* Virtio Block Device */
};


/*****************************************************************************/

#define VPCI_HOST_FEATURES  0x0
#define VPCI_GUEST_FEATURES 0x4
#define VPCI_QUEUE_PFN      0x8
#define VPCI_QUEUE_NUM      0xC
#define VPCI_QUEUE_SEL      0xE
#define VPCI_QUEUE_NOTIFY   0x10
#define VPCI_STATUS         0x12
#define VPCI_ISR            0x13
#define VPCI_ISR_QUEUE      0x1
#define VPCI_ISR_CONFIG     0x3
#define VPCI_CONFIG         0x14

/** @todo use+extend RTNETIPV4 */

/** @todo use+extend RTNETTCP */

#define VNET_SAVEDSTATE_VERSION 1

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/* Forward declarations ******************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) vpciIOPortIn (PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vpciIOPortOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t u32, unsigned cb);
RT_C_DECLS_END

/**
 * Arm a timer.
 *
 * @param   pState      Pointer to the device state structure.
 * @param   pTimer      Pointer to the timer.
 * @param   uExpireIn   Expiration interval in microseconds.
 */
DECLINLINE(void) vpciArmTimer(VPCISTATE *pState, PTMTIMER pTimer, uint32_t uExpireIn)
{
    Log2(("%s Arming timer to fire in %d usec...\n",
          INSTANCE(pState), uExpireIn));
    TMTimerSet(pTimer, TMTimerFromMicro(pTimer, uExpireIn) +
               TMTimerGet(pTimer));
}


DECLINLINE(int) vpciCsEnter(VPCISTATE *pState, int iBusyRc)
{
    return PDMCritSectEnter(&pState->cs, iBusyRc);
}

DECLINLINE(void) vpciCsLeave(VPCISTATE *pState)
{
    PDMCritSectLeave(&pState->cs);
}

/**
 * Raise interrupt.
 *
 * @param   pState      The device state structure.
 * @param   rcBusy      Status code to return when the critical section is busy.
 * @param   u8IntCause  Interrupt cause bit mask to set in PCI ISR port.
 */
PDMBOTHCBDECL(int) vpciRaiseInterrupt(VPCISTATE *pState, int rcBusy, uint8_t u8IntCause)
{
    int rc = vpciCsEnter(pState, rcBusy);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    pState->uISR |= u8IntCause;
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 1);
    vpciCsLeave(pState);
    return VINF_SUCCESS;
}

/**
 * Lower interrupt.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vpciLowerInterrupt(VPCISTATE *pState)
{
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 0);
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
PDMBOTHCBDECL(int) vpciIOPortIn(PPDMDEVINS pDevIns, void *pvUser,
                                RTIOPORT port, uint32_t *pu32, unsigned cb)
{
    VPCISTATE  *pState = PDMINS_2_DATA(pDevIns, VPCISTATE *);
    int         rc     = VINF_SUCCESS;
    const char *szInst = INSTANCE(pState);
    STAM_PROFILE_ADV_START(&pState->CTXSUFF(StatIORead), a);

    port -= pState->addrIOPort;
    switch (port)
    {
        case VPCI_HOST_FEATURES:
            /* Tell the guest what features we support. */
            *pu32 = g_VPCIDevices[pState->enmDevType].pfnGetHostFeatures(pState);
            break;

        case VPCI_GUEST_FEATURES:
            *pu32 = pState->uGuestFeatures;
            break;

        case VPCI_QUEUE_PFN:
            *pu32 = pState->pQueues[pState->uQueueSelector].uPageNumber;
            break;

        case VPCI_QUEUE_NUM:
            Assert(cb == 2);
            *(uint16_t*)pu32 = pState->pQueues[pState->uQueueSelector].VRing.uSize;
            break;

        case VPCI_QUEUE_SEL:
            Assert(cb == 2);
            *(uint16_t*)pu32 = pState->uQueueSelector;
            break;

        case VPCI_STATUS:
            Assert(cb == 1);
            *(uint8_t*)pu32 = pState->uStatus;
            break;

        case VPCI_ISR:
            Assert(cb == 1);
            *(uint8_t*)pu32 = pState->uISR;
            pState->uISR = 0; /* read clears all interrupts */
            break;

        default:
            if (port >= VPCI_CONFIG)
                rc = g_VPCIDevices[pState->enmDevType].pfnGetConfig(pState, port - VPCI_CONFIG, cb, pu32);
            else
            {
                *pu32 = 0xFFFFFFFF;
                rc = PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "%s virtioIOPortIn: no valid port at offset port=%RTiop cb=%08x\n", szInst, port, cb);
            }
            break;
    }
    Log3(("%s virtioIOPortIn:  At %RTiop in  %0*x\n", szInst, port, cb*2, *pu32));
    STAM_PROFILE_ADV_STOP(&pState->CTXSUFF(StatIORead), a);
    return rc;
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
PDMBOTHCBDECL(int) vpciIOPortOut(PPDMDEVINS pDevIns, void *pvUser,
                                 RTIOPORT port, uint32_t u32, unsigned cb)
{
    VPCISTATE  *pState = PDMINS_2_DATA(pDevIns, VPCISTATE *);
    int         rc     = VINF_SUCCESS;
    const char *szInst = INSTANCE(pState);
    STAM_PROFILE_ADV_START(&pState->CTXSUFF(StatIOWrite), a);

    port -= pState->addrIOPort;
    Log3(("%s virtioIOPortOut: At %RTiop out          %0*x\n", szInst, port, cb*2, u32));

    switch (port)
    {
        case VPCI_GUEST_FEATURES:
            // TODO: Feature negotiation code goes here.
            // The guest may potentially desire features we don't support!
            break;

        case VPCI_QUEUE_PFN:
            /*
             * The guest is responsible for allocating the pages for queues,
             * here it provides us with the page number of descriptor table.
             * Note that we provide the size of the queue to the guest via
             * VIRTIO_PCI_QUEUE_NUM.
             */
            pState->pQueues[pState->uQueueSelector].uPageNumber = u32;
            pState->pQueues[pState->uQueueSelector].VRing.pDescriptors = (VRINGDESC*)(u32 << PAGE_SHIFT);
            break;

        case VPCI_QUEUE_SEL:
            Assert(cb == 2);
            u32 &= 0xFFFF;
            if (u32 < g_VPCIDevices[pState->enmDevType].nQueues)
                pState->uQueueSelector = u32;
            else
                Log3(("%s virtioIOPortOut: Invalid queue selector %08x\n", szInst, u32));
            break;

        case VPCI_QUEUE_NOTIFY:
            // TODO
            break;

        case VPCI_STATUS:
            Assert(cb == 1);
            u32 &= 0xFF;
            pState->uStatus = u32;
            /* Writing 0 to the status port triggers device reset. */
            if (u32 == 0)
                g_VPCIDevices[pState->enmDevType].pfnReset(pState);
            break;

        default:
            if (port >= VPCI_CONFIG)
                rc = g_VPCIDevices[pState->enmDevType].pfnSetConfig(pState, port - VPCI_CONFIG, cb, &u32);
            else
                rc = PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "%s virtioIOPortOut: no valid port at offset port=%RTiop cb=%08x\n", szInst, port, cb);
            break;
    }

    STAM_PROFILE_ADV_STOP(&pState->CTXSUFF(StatIOWrite), a);
    return rc;
}

#ifdef IN_RING3
// Common
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
static DECLCALLBACK(int) vpciMap(PPCIDEVICE pPciDev, int iRegion,
                                 RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    int       rc;
    VPCISTATE *pState = PDMINS_2_DATA(pPciDev->pDevIns, VPCISTATE*);

    if (enmType != PCI_ADDRESS_SPACE_IO)
    {
        /* We should never get here */
        AssertMsgFailed(("Invalid PCI address space param in map callback"));
        return VERR_INTERNAL_ERROR;
    }

    pState->addrIOPort = (RTIOPORT)GCPhysAddress;
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                 vpciIOPortOut, vpciIOPortIn, NULL, NULL, "VirtioNet");
#if 0
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterR0(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                   "vpciIOPortOut", "vpciIOPortIn", NULL, NULL, "VirtioNet");
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterGC(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                   "vpciIOPortOut", "vpciIOPortIn", NULL, NULL, "VirtioNet");
#endif
    AssertRC(rc);
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
static DECLCALLBACK(void *) vpciQueryInterface(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface)
{
    VPCISTATE *pState = IFACE_TO_STATE(pInterface, IBase);
    Assert(&pState->IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pState->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pState->ILeds;
        default:
            return NULL;
    }
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 * @thread  EMT
 */
static DECLCALLBACK(int) vpciQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    VPCISTATE *pState = IFACE_TO_STATE(pInterface, ILeds);
    int        rc     = VERR_PDM_LUN_NOT_FOUND;

    if (iLUN == 0)
    {
        *ppLed = &pState->led;
        rc     = VINF_SUCCESS;
    }
    return rc;
}

/**
 * Sets 8-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u16Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) virtioPCICfgSetU8(PCIDEVICE& refPciDev, uint32_t uOffset, uint8_t u8Value)
{
    Assert(uOffset < sizeof(refPciDev.config));
    refPciDev.config[uOffset] = u8Value;
}

/**
 * Sets 16-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u16Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) virtioPCICfgSetU16(PCIDEVICE& refPciDev, uint32_t uOffset, uint16_t u16Value)
{
    Assert(uOffset+sizeof(u16Value) <= sizeof(refPciDev.config));
    *(uint16_t*)&refPciDev.config[uOffset] = u16Value;
}

/**
 * Sets 32-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u32Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) virtioPCICfgSetU32(PCIDEVICE& refPciDev, uint32_t uOffset, uint32_t u32Value)
{
    Assert(uOffset+sizeof(u32Value) <= sizeof(refPciDev.config));
    *(uint32_t*)&refPciDev.config[uOffset] = u32Value;
}


/**
 * Set PCI configuration space registers.
 *
 * @param   pci         Reference to PCI device structure.
 * @thread  EMT
 */
static DECLCALLBACK(void) vpciConfigure(PCIDEVICE& pci, VirtioDeviceType enmType)
{
    Assert(enmType < (int)RT_ELEMENTS(g_VPCIDevices));
    /* Configure PCI Device, assume 32-bit mode ******************************/
    PCIDevSetVendorId(&pci, g_VPCIDevices[enmType].uPCIVendorId);
    PCIDevSetDeviceId(&pci, g_VPCIDevices[enmType].uPCIDeviceId);
    virtioPCICfgSetU16(pci, VBOX_PCI_SUBSYSTEM_VENDOR_ID, g_VPCIDevices[enmType].uPCISubsystemVendorId);
    virtioPCICfgSetU16(pci, VBOX_PCI_SUBSYSTEM_ID, g_VPCIDevices[enmType].uPCISubsystemId);

    /* ABI version, must be equal 0 as of 2.6.30 kernel. */
    virtioPCICfgSetU8( pci, VBOX_PCI_REVISION_ID,          0x00);
    /* Ethernet adapter */
    virtioPCICfgSetU8( pci, VBOX_PCI_CLASS_PROG,           0x00);
    virtioPCICfgSetU16(pci, VBOX_PCI_CLASS_DEVICE, g_VPCIDevices[enmType].uPCIClass);
    /* Interrupt Pin: INTA# */
    virtioPCICfgSetU8( pci, VBOX_PCI_INTERRUPT_PIN,        0x01);
}

// TODO: header
DECLCALLBACK(int) vpciConstruct(PPDMDEVINS pDevIns, VPCISTATE *pState,
                                int iInstance, VirtioDeviceType enmDevType,
                                unsigned uConfigSize)
{
    int rc = VINF_SUCCESS;
    /* Init handles and log related stuff. */
    RTStrPrintf(pState->szInstance, sizeof(pState->szInstance), g_VPCIDevices[enmDevType].pcszNameFmt, iInstance);
    pState->enmDevType   = enmDevType;

    /* Allocate queues */
    pState->pQueues      = (VQUEUE*)RTMemAllocZ(sizeof(VQUEUE) * g_VPCIDevices[enmDevType].nQueues);

    pState->pDevInsR3    = pDevIns;
    pState->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pState->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pState->led.u32Magic = PDMLED_MAGIC;

    pState->ILeds.pfnQueryStatusLed          = vpciQueryStatusLed;

    /* Initialize critical section. */
    rc = PDMDevHlpCritSectInit(pDevIns, &pState->cs, pState->szInstance);
    if (RT_FAILURE(rc))
        return rc;

    /* Set PCI config registers */
    vpciConfigure(pState->pciDevice, VIRTIO_NET_ID);
    /* Register PCI device */
    rc = PDMDevHlpPCIRegister(pDevIns, &pState->pciDevice);
    if (RT_FAILURE(rc))
        return rc;

    /* Map our ports to IO space. */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, VPCI_CONFIG + uConfigSize,
                                      PCI_ADDRESS_SPACE_IO, vpciMap);
    if (RT_FAILURE(rc))
        return rc;

    /* Status driver */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pState->IBase, &pBase, "Status Port");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));
    pState->pLedsConnector = (PPDMILEDCONNECTORS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);

#if defined(VBOX_WITH_STATISTICS)
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOReadGC,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in GC",           "/Devices/VNet%d/IO/ReadGC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOReadHC,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in HC",           "/Devices/VNet%d/IO/ReadHC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOWriteGC,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in GC",          "/Devices/VNet%d/IO/WriteGC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOWriteHC,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in HC",          "/Devices/VNet%d/IO/WriteHC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIntsRaised,         STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of raised interrupts",        "/Devices/VNet%d/Interrupts/Raised", iInstance);
#endif /* VBOX_WITH_STATISTICS */

    return rc;
}

/**
 * Destruct PCI-related part of device.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status.
 * @param   pState      The device state structure.
 */
static DECLCALLBACK(int) vpciDestruct(VPCISTATE* pState)
{
    Log(("%s Destroying PCI instance\n", INSTANCE(pState)));
    
    if (pState->pQueues)
        RTMemFree(pState->pQueues);

    if (PDMCritSectIsInitialized(&pState->cs))
    {
        PDMR3CritSectDelete(&pState->cs);
    }
    return VINF_SUCCESS;
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
static DECLCALLBACK(void) vpciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    VPCISTATE* pState = PDMINS_2_DATA(pDevIns, VPCISTATE*);
    pState->pDevInsRC     = PDMDEVINS_2_RCPTR(pDevIns);
    // TBD
}

PVQUEUE vpciAddQueue(VPCISTATE* pState, unsigned uSize,
                     void (*pfnCallback)(void *pvState, PVQUEUE pQueue))
{
    PVQUEUE pQueue = NULL;
    /* Find an empty queue slot */
    for (unsigned i = 0; i < g_VPCIDevices[pState->enmDevType].nQueues; i++)
    {
        if (pState->pQueues[i].VRing.uSize == 0)
        {
            pQueue = &pState->pQueues[i];
            break;
        }
    }

    if (!pQueue)
    {
        Log(("%s Too many queues being added, no empty slots available!\n", INSTANCE(pState)));
    }
    else
    {
        pQueue->VRing.uSize = uSize;
        pQueue->VRing.pDescriptors = NULL;
        pQueue->uPageNumber = 0;
        pQueue->pfnCallback = pfnCallback;
    }

    return pQueue;
}

#endif /* IN_RING3 */


//------------------------- Tear off here: vnet -------------------------------

/* Virtio net features */
#define NET_F_CSUM       0x00000001  /* Host handles pkts w/ partial csum */
#define NET_F_GUEST_CSUM 0x00000002  /* Guest handles pkts w/ partial csum */
#define NET_F_MAC        0x00000020  /* Host has given MAC address. */
#define NET_F_GSO        0x00000040  /* Host handles pkts w/ any GSO type */
#define NET_F_GUEST_TSO4 0x00000080  /* Guest can handle TSOv4 in. */
#define NET_F_GUEST_TSO6 0x00000100  /* Guest can handle TSOv6 in. */
#define NET_F_GUEST_ECN  0x00000200  /* Guest can handle TSO[6] w/ ECN in. */
#define NET_F_GUEST_UFO  0x00000400  /* Guest can handle UFO in. */
#define NET_F_HOST_TSO4  0x00000800  /* Host can handle TSOv4 in. */
#define NET_F_HOST_TSO6  0x00001000  /* Host can handle TSOv6 in. */
#define NET_F_HOST_ECN   0x00002000  /* Host can handle TSO[6] w/ ECN in. */
#define NET_F_HOST_UFO   0x00004000  /* Host can handle UFO in. */
#define NET_F_MRG_RXBUF  0x00008000  /* Host can merge receive buffers. */
#define NET_F_STATUS     0x00010000  /* virtio_net_config.status available */
#define NET_F_CTRL_VQ    0x00020000  /* Control channel available */
#define NET_F_CTRL_RX    0x00040000  /* Control channel RX mode support */
#define NET_F_CTRL_VLAN  0x00080000  /* Control channel VLAN filtering */

#define NET_S_LINK_UP    1


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

    PTMTIMERR3  pLinkUpTimer;                             /**< Link Up(/Restore) Timer. */

    /** PCI config area holding MAC address as well as TBD. */
    struct VNetPCIConfig config;
    /** True if physical cable is attached in configuration. */
    bool        fCableConnected;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t    u32PktNo;

    /** Locked state -- no state alteration possible. */
    bool        fLocked;

    PVQUEUE     pRxQueue;
    PVQUEUE     pTxQueue;
    PVQUEUE     pCtlQueue;
    /* Receive-blocking-related fields ***************************************/

    /** N/A: */
    bool volatile fMaybeOutOfSpace;
    /** EMT: Gets signalled when more RX descriptors become available. */
    RTSEMEVENT  hEventMoreRxDescAvail;

    R3PTRTYPE(PPDMQUEUE)    pCanRxQueueR3;           /**< Rx wakeup signaller - R3. */
    R0PTRTYPE(PPDMQUEUE)    pCanRxQueueR0;           /**< Rx wakeup signaller - R0. */
    RCPTRTYPE(PPDMQUEUE)    pCanRxQueueRC;           /**< Rx wakeup signaller - RC. */

    /* Statistic fields ******************************************************/

    STAMCOUNTER                         StatReceiveBytes;
    STAMCOUNTER                         StatTransmitBytes;
#if defined(VBOX_WITH_STATISTICS)
    STAMPROFILEADV                      StatReceive;
    STAMPROFILEADV                      StatTransmit;
    STAMPROFILEADV                      StatTransmitSend;
    STAMPROFILE                         StatRxOverflow;
    STAMCOUNTER                         StatRxOverflowWakeup;
#endif /* VBOX_WITH_STATISTICS */

};
typedef struct VNetState_st VNETSTATE;

AssertCompileMemberOffset(VNETSTATE, VPCI, 0);

#undef INSTANCE
#define INSTANCE(pState) pState->VPCI.szInstance
#undef IFACE_TO_STATE
#define IFACE_TO_STATE(pIface, ifaceName) ((VNETSTATE *)((char*)pIface - RT_OFFSETOF(VNETSTATE, ifaceName)))
#define STATUS pState->config.uStatus

PDMBOTHCBDECL(uint32_t) vnetGetHostFeatures(void *pvState)
{
    // TODO: implement
    return NET_F_MAC | NET_F_STATUS;
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
    // TODO: Implement reset
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

    STATUS |= NET_S_LINK_UP;
    vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
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


#ifdef IN_RING3

/**
 * Check if the device can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @returns Number of bytes the device can receive.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static int vnetCanReceive(VNETSTATE *pState)
{
    size_t cb;

    cb = 0; // TODO
    return cb > 0 ? VINF_SUCCESS : VERR_NET_NO_BUFFER_SPACE;
}

static DECLCALLBACK(int) vnetWaitReceiveAvail(PPDMINETWORKPORT pInterface, unsigned cMillies)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    int rc = vnetCanReceive(pState);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    if (RT_UNLIKELY(cMillies == 0))
        return VERR_NET_NO_BUFFER_SPACE;

    rc = VERR_INTERRUPTED;
    ASMAtomicXchgBool(&pState->fMaybeOutOfSpace, true);
    STAM_PROFILE_START(&pState->StatRxOverflow, a);
    while (RT_LIKELY(PDMDevHlpVMState(pState->VPCI.CTX_SUFF(pDevIns)) == VMSTATE_RUNNING))
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
 * Receive data from the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  ???
 */
static DECLCALLBACK(int) vnetReceive(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    int        rc = VINF_SUCCESS;
    // TODO: Implement
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
    if (STATUS & NET_S_LINK_UP)
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
    bool fOldUp = !!(STATUS & NET_S_LINK_UP);
    bool fNewUp = enmState == PDMNETWORKLINKSTATE_UP;

    if (fNewUp != fOldUp)
    {
        if (fNewUp)
        {
            Log(("%s Link is up\n", INSTANCE(pState)));
            STATUS |= NET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        else
        {
            Log(("%s Link is down\n", INSTANCE(pState)));
            STATUS &= ~NET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        if (pState->pDrv)
            pState->pDrv->pfnNotifyLinkChanged(pState->pDrv, enmState);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vnetReceive(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
}

static DECLCALLBACK(void) vnetTransmit(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
}

static DECLCALLBACK(void) vnetControl(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
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
    rc = vpciConstruct(pDevIns, &pState->VPCI, iInstance, VIRTIO_NET_ID, sizeof(VNetPCIConfig));
    pState->pRxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetReceive);
    pState->pTxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetTransmit);
    pState->pCtlQueue = vpciAddQueue(&pState->VPCI, 16,  vnetControl);

    Log(("%s Constructing new instance\n", INSTANCE(pState)));

    pState->hEventMoreRxDescAvail = NIL_RTSEMEVENT;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "MAC\0" "CableConnected\0" "LineSpeed\0"))
                    return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                            N_("Invalid configuraton for VirtioNet device"));

    /* Get config params */
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", pState->config.mac.au8,
                          sizeof(pState->config.mac.au8));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get MAC address"));
    rc = CFGMR3QueryBool(pCfgHandle, "CableConnected", &pState->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the value of 'CableConnected'"));

    /* Initialize state structure */
    pState->fLocked      = false;
    pState->u32PktNo     = 1;

    /* Interfaces */
    pState->INetworkPort.pfnWaitReceiveAvail = vnetWaitReceiveAvail;
    pState->INetworkPort.pfnReceive          = vnetReceive;
    pState->INetworkConfig.pfnGetMac         = vnetGetMac;
    pState->INetworkConfig.pfnGetLinkState   = vnetGetLinkState;
    pState->INetworkConfig.pfnSetLinkState   = vnetSetLinkState;

    /* Register save/restore state handlers. */
    // TODO:
    /*
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VNET_SAVEDSTATE_VERSION, sizeof(VNETSTATE), NULL,
                                NULL, NULL, NULL,
                                NULL, vnetSaveExec, NULL,
                                NULL, vnetLoadExec, vnetLoadDone);
    if (RT_FAILURE(rc))
    return rc;*/


    /* Create the RX notifier signaller. */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 1, 0,
                                 vnetCanRxQueueConsumer, true, "VNet-rcv", &pState->pCanRxQueueR3);
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

    vpciCsEnter(&pState->VPCI, VERR_SEM_BUSY);

    /*
     * Zero some important members.
     */
    pState->pDrvBase = NULL;
    pState->pDrv = NULL;

    vpciCsLeave(&pState->VPCI);
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

    vpciCsEnter(&pState->VPCI, VERR_SEM_BUSY);

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
    if ((STATUS & NET_S_LINK_UP) && RT_SUCCESS(rc))
    {
        STATUS &= ~NET_S_LINK_UP;
        vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        /* Restore the link back in 5 seconds. */
        vpciArmTimer(&pState->VPCI, pState->pLinkUpTimer, 5000000);
    }

    vpciCsLeave(&pState->VPCI);
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
    PDM_DEVREG_FLAGS_DEFAULT_BITS, // | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
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

