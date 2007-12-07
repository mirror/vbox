/* $Id$ */
/** @file
 * PC Architechture Device.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_DEV_PC_ARCH
#include <VBox/pdmdev.h>
#include <VBox/mm.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <VBox/err.h>

#include <string.h>

#include "Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * PC Bios instance data structure.
 */
typedef struct DEVPCARCH
{
    /** Pointer back to the device instance. */
    PPDMDEVINS      pDevIns;
} DEVPCARCH, *PDEVPCARCH;



/**
 * Port I/O Handler for math coprocessor IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) pcarchIOPortFPURead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    int rc;
    NOREF(pvUser); NOREF(pDevIns); NOREF(pu32);
    rc = PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d\n", Port, cb);
    if (rc == VINF_SUCCESS)
        rc = VERR_IOM_IOPORT_UNUSED;
    return rc;
}

/**
 * Port I/O Handler for math coprocessor OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 * @todo Add IGNNE support.
 */
static DECLCALLBACK(int) pcarchIOPortFPUWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser);
    if (cb == 1)
    {
        switch (Port)
        {
            /*
             * Clear busy latch.
             */
            case 0xf0:
                Log2(("PCARCH: FPU Clear busy latch u32=%#x\n", u32));
/* This is triggered when booting Knoppix (3.7) */
#if 0
                if (!u32)
                    rc = PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d u32=%#x\n", Port, cb, u32);
#endif
                /* pDevIns->pDevHlp->pfnPICSetIrq(pDevIns, 13, 0); */
                break;

            /* Reset. */
            case 0xf1:
                Log2(("PCARCH: FPU Reset cb=%d u32=%#x\n", Port, cb, u32));
                /** @todo figure out what the difference between FPU ports 0xf0 and 0xf1 are... */
                /* pDevIns->pDevHlp->pfnPICSetIrq(pDevIns, 13, 0); */
                break;

            /* opcode transfers */
            case 0xf8:
            case 0xfa:
            case 0xfc:
            default:
                rc = PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d u32=%#x\n", Port, cb, u32);
                break;
        }
        /* this works better, but probably not entirely correct. */
        PDMDevHlpISASetIrq(pDevIns, 13, 0);
    }
    else
        rc = PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d u32=%#x\n", Port, cb, u32);
    return rc;
}


/**
 * Port I/O Handler for PS/2 system control port A IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 *
 * @todo    Check if the A20 enable/disable method implemented here in any way
 *          should cooperate with the one implemented in the PS/2 keyboard device.
 *          This probably belongs together in the PS/2 keyboard device (since that
 *          is where the "port B" mentioned by Ralph Brown is implemented).
 *
 * @remark  Ralph Brown and friends have this to say about this port:
 *
 *  0092  RW  PS/2 system control port A  (port B is at PORT 0061h) (see #P0415)
 *
 *  Bitfields for PS/2 system control port A:
 *  Bit(s)	Description	(Table P0415)
 *   7-6	any bit set to 1 turns activity light on
 *   5	unused
 *   4	watchdog timout occurred
 *   3	=0 RTC/CMOS security lock (on password area) unlocked
 *  	=1 CMOS locked (done by POST)
 *   2	unused
 *   1	A20 is active
 *   0	=0 system reset or write
 *  	=1 pulse alternate reset pin (high-speed alternate CPU reset)
 *  Notes:	once set, bit 3 may only be cleared by a power-on reset
 *  	on at least the C&T 82C235, bit 0 remains set through a CPU reset to
 *  	  allow the BIOS to determine the reset method
 *  	on the PS/2 30-286 & "Tortuga" the INT 15h/87h memory copy does
 *  	  not use this port for A20 control, but instead uses the keyboard
 *  	  controller (8042). Reportedly this may cause the system to crash
 *  	  when access to the 8042 is disabled in password server mode
 *  	  (see #P0398).
 *  SeeAlso: #P0416,#P0417,MSR 00001000h
 */
static DECLCALLBACK(int) pcarchIOPortPS2SysControlPortARead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    if (cb == 1)
    {
        *pu32 = PDMDevHlpA20IsEnabled(pDevIns) << 1;
        return VINF_SUCCESS;
    }
    return PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d\n", Port, cb);
}


/**
 * Port I/O Handler for PS/2 system control port A OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 * @see     Remark and todo of pcarchIOPortPS2SysControlPortARead().
 */
static DECLCALLBACK(int) pcarchIOPortPS2SysControlPortAWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 1)
    {
        /*
         * Fast reset?
         */
        if (u32 & 1)
            return PDMDevHlpVMReset(pDevIns);

        /*
         * A20 is the only thing we care about of the other stuff.
         */
        PDMDevHlpA20Set(pDevIns, !!(u32 & 2));
        return VINF_SUCCESS;
    }
    return PDMDeviceDBGFStop(pDevIns, PDM_SRC_POS, "Port=%#x cb=%d u32=%#x\n", Port, cb, u32);
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
 */
static DECLCALLBACK(int)  pcarchConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PDEVPCARCH  pData = PDMINS2DATA(pDevIns, PDEVPCARCH);
    int         rc;
    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * Init the data.
     */
    pData->pDevIns = pDevIns;

    /*
     * Register I/O Ports
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0xF0, 0x10, NULL, pcarchIOPortFPUWrite, pcarchIOPortFPURead, NULL, NULL, "Math Co-Processor (DOS/OS2 mode)");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x92, 1, NULL, pcarchIOPortPS2SysControlPortAWrite, pcarchIOPortPS2SysControlPortARead, NULL, NULL, "PS/2 system control port A (A20 and more)");
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Reserve ROM/MMIO areas:
     * 1. 0x000a0000-0x000fffff
     * 2. 0xfff80000-0xffffffff
     */
    rc = PDMDevHlpPhysReserve(pDevIns, 0x000a0000, 0x50000, "Low ROM Region");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPhysReserve(pDevIns, 0xfff80000, 0x80000, "High ROM Region");
    if (VBOX_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePcArch =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "pcarch",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "PC Architecture Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32,
    /* fClass */
    PDM_DEVREG_CLASS_ARCH,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(DEVPCARCH),
    /* pfnConstruct */
    pcarchConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete. */
    NULL
};

