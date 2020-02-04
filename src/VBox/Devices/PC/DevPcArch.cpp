/* $Id$ */
/** @file
 * DevPcArch - PC Architecture Device.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PC_ARCH
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * PC Bios instance data structure.
 */
typedef struct DEVPCARCH
{
    /** Pointer back to the device instance. */
    PPDMDEVINS      pDevIns;
} DEVPCARCH, *PDEVPCARCH;



/**
 * @callback_method_impl{FNIOMIOPORTNEWIN, Math coprocessor.}
 * @note offPort is absolute
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchIOPortFPURead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    int rc;
    NOREF(pvUser); NOREF(pDevIns); NOREF(pu32);
    rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d\n", offPort, cb);
    if (rc == VINF_SUCCESS)
        rc = VERR_IOM_IOPORT_UNUSED;
    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT, Math coprocessor.}
 * @note    offPort is absolute
 * @todo Add IGNNE support.
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchIOPortFPUWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser);
    if (cb == 1)
    {
        switch (offPort)
        {
            /*
             * Clear busy latch.
             */
            case 0xf0:
                Log2(("PCARCH: FPU Clear busy latch u32=%#x\n", u32));
/* This is triggered when booting Knoppix (3.7) */
#if 0
                if (!u32)
                    rc = PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d u32=%#x\n", offPort, cb, u32);
#endif
                /* pDevIns->pHlp->pfnPICSetIrq(pDevIns, 13, 0); */
                break;

            /* Reset. */
            case 0xf1:
                Log2(("PCARCH: FPU Reset cb=%d u32=%#x\n", cb, u32));
                /** @todo figure out what the difference between FPU ports 0xf0 and 0xf1 are... */
                /* pDevIns->pHlp->pfnPICSetIrq(pDevIns, 13, 0); */
                break;

            /* opcode transfers */
            case 0xf8:
            case 0xfa:
            case 0xfc:
            default:
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d u32=%#x\n", offPort, cb, u32);
                break;
        }
        /* this works better, but probably not entirely correct. */
        PDMDevHlpISASetIrq(pDevIns, 13, 0);
    }
    else
        rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d u32=%#x\n", offPort, cb, u32);
    return rc;
}


/**
 * @callback_method_impl{FNIOMIOPORTIN, PS/2 system control port A.}
 *
 * @todo    Check if the A20 enable/disable method implemented here in any way
 *          should cooperate with the one implemented in the PS/2 keyboard device.
 *          This probably belongs together in the PS/2 keyboard device (since that
 *          is where the "port B" mentioned by Ralph Brown is implemented).
 *
 * @remark  Ralph Brown and friends have this to say about this port:
 *
 * @verbatim
0092  RW  PS/2 system control port A  (port B is at PORT 0061h) (see #P0415)

Bitfields for PS/2 system control port A:
Bit(s)  Description     (Table P0415)
 7-6    any bit set to 1 turns activity light on
 5      unused
 4      watchdog timout occurred
 3      =0 RTC/CMOS security lock (on password area) unlocked
        =1 CMOS locked (done by POST)
 2      unused
 1      A20 is active
 0      =0 system reset or write
        =1 pulse alternate reset pin (high-speed alternate CPU reset)
Notes:  once set, bit 3 may only be cleared by a power-on reset
        on at least the C&T 82C235, bit 0 remains set through a CPU reset to
          allow the BIOS to determine the reset method
        on the PS/2 30-286 & "Tortuga" the INT 15h/87h memory copy does
          not use this port for A20 control, but instead uses the keyboard
          controller (8042). Reportedly this may cause the system to crash
          when access to the 8042 is disabled in password server mode
          (see #P0398).
SeeAlso: #P0416,#P0417,MSR 00001000h
 * @endverbatim
 * @note    offPort is absolute
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchIOPortPS2SysControlPortARead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    RT_NOREF1(pvUser);
    if (cb == 1)
    {
        *pu32 = PDMDevHlpA20IsEnabled(pDevIns) << 1;
        return VINF_SUCCESS;
    }
    return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d\n", offPort, cb);
}


/**
 * @callback_method_impl{FNIOMIOPORTOUT, PS/2 system control port A.}
 * @see     Remark and todo of pcarchIOPortPS2SysControlPortARead().
 * @note    offPort is absolute
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchIOPortPS2SysControlPortAWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        /*
         * Fast reset?
         */
        if (u32 & 1)
        {
            LogRel(("Reset initiated by system port A\n"));
            return PDMDevHlpVMReset(pDevIns, PDMVMRESET_F_PORT_A);
        }

        /*
         * A20 is the only thing we care about of the other stuff.
         */
        PDMDevHlpA20Set(pDevIns, !!(u32 & 2));
        return VINF_SUCCESS;
    }
    return PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "Port=%#x cb=%d u32=%#x\n", offPort, cb, u32);
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE, Ignores writes to the reserved memory.}
 * @note    off is an absolute address.
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchReservedMemoryWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    Log2(("pcarchReservedMemoryRead: %#RGp LB %#x %.*Rhxs\n", off, cb, RT_MIN(cb, 16), pv));
    RT_NOREF(pDevIns, pvUser, off, pv, cb);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD, The reserved memory reads as 0xff.}
 * @note    off is an absolute address.
 */
static DECLCALLBACK(VBOXSTRICTRC)
pcarchReservedMemoryRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    Log2(("pcarchReservedMemoryRead: %#RGp LB %#x\n", off, cb));
    RT_NOREF(pDevIns, pvUser, off);
    memset(pv, 0xff, cb);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnInitComplete,
 *      Turn RAM pages between 0xa0000 and 0xfffff into reserved memory.}
 */
static DECLCALLBACK(int) pcarchInitComplete(PPDMDEVINS pDevIns)
{
    PVM             pVM       = PDMDevHlpGetVM(pDevIns);
    int             iRegion   = 0;
    RTGCPHYS const  GCPhysEnd = 0x100000;
    RTGCPHYS        GCPhysCur = 0x0a0000;
    do
    {
        if (!PGMPhysIsGCPhysNormal(pVM, GCPhysCur))
            GCPhysCur += X86_PAGE_SIZE;
        else
        {
            RTGCPHYS const GCPhysStart = GCPhysCur;
            do
                GCPhysCur += X86_PAGE_SIZE;
            while (GCPhysCur < GCPhysEnd && PGMPhysIsGCPhysNormal(pVM, GCPhysCur));

            IOMMMIOHANDLE hMmioRegion;
            int rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysStart, GCPhysCur - GCPhysStart,
                                               pcarchReservedMemoryWrite, pcarchReservedMemoryRead,
                                               IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU | IOMMMIO_FLAGS_ABS,
                                               MMR3HeapAPrintf(pVM, MM_TAG_PGM_PHYS /* bad bird*/, "PC Arch Reserved #%u", iRegion),
                                               &hMmioRegion);
            AssertLogRelRCReturn(rc, rc);
            iRegion++;
        }
    } while (GCPhysCur < GCPhysEnd);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)  pcarchConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVPCARCH  pThis = PDMDEVINS_2_DATA(pDevIns, PDEVPCARCH);
    int         rc;
    RT_NOREF(iInstance, pCfg);
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "", "");

    /*
     * Init the data.
     */
    pThis->pDevIns = pDevIns;

    /*
     * Register I/O Ports
     */
    IOMIOPORTHANDLE hIoPorts;
    rc = PDMDevHlpIoPortCreateFlagsAndMap(pDevIns, 0xf0 /*uPort*/, 0x10 /*cPorts*/, IOM_IOPORT_F_ABS,
                                          pcarchIOPortFPUWrite, pcarchIOPortFPURead,
                                          "Math Co-Processor (DOS/OS2 mode)", NULL /*paExtDescs*/, &hIoPorts);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIoPortCreateFlagsAndMap(pDevIns, 0x92 /*uPort*/, 1 /*cPorts*/, IOM_IOPORT_F_ABS,
                                          pcarchIOPortPS2SysControlPortAWrite, pcarchIOPortPS2SysControlPortARead,
                                          "PS/2 system control port A (A20 and more)", NULL /*paExtDescs*/, &hIoPorts);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePcArch =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "pcarch",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_ARCH,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVPCARCH),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "PC Architecture Device",
#if defined(IN_RING3)
    /* .pszRCMod = */               "",
    /* .pszR0Mod = */               "",
    /* .pfnConstruct = */           pcarchConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               NULL,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        pcarchInitComplete,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           NULL,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

