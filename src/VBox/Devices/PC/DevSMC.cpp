/* $Id$ */
/** @file
 * DevSMC - SMC device emulation.
 *
 * @todo Rainy day: Rewrite from scratch!!
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 *  Apple SMC controller
 *
 *  Copyright (c) 2007 Alexander Graf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * *****************************************************************
 *
 * In all Intel-based Apple hardware there is an SMC chip to control the
 * backlight, fans and several other generic device parameters. It also
 * contains the magic keys used to dongle Mac OS X to the device.
 *
 * This driver was mostly created by looking at the Linux AppleSMC driver
 * implementation and does not support IRQ.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_SMC
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/vmm/stam.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#ifdef IN_RING0
# include <iprt/asm-amd64-x86.h>
# include <iprt/once.h>
# include <iprt/thread.h>
#endif

#include "VBoxDD2.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* data port used by Apple SMC */
#define APPLESMC_DATA_PORT      0x300
/* command/status port used by Apple SMC */
#define APPLESMC_CMD_PORT       0x304
#define APPLESMC_NR_PORTS       32 /* 0x300-0x31f */
#define APPLESMC_MAX_DATA_LENGTH 32

#define APPLESMC_READ_CMD       0x10
#define APPLESMC_WRITE_CMD      0x11
#define APPLESMC_GET_KEY_BY_INDEX_CMD   0x12
#define APPLESMC_GET_KEY_TYPE_CMD       0x13

/** The version of the saved state. */
#define SMC_SAVED_STATE_VERSION 1

/** The ring-0 operation number that attempts to get OSK0 and OSK1 from the real
 *  SMC. */
#define SMC_CALLR0_READ_OSK     1

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct AppleSMCData
{
    uint8_t       len;
    const char    *key;
    const char    *data;
} AppleSMCData;


typedef struct
{
    PPDMDEVINSR3   pDevIns;

    uint8_t cmd;
    uint8_t status;
    uint8_t key[4];
    uint8_t read_pos;
    uint8_t data_len;
    uint8_t data_pos;
    uint8_t data[255];

    /** The OSK0 value. This is currently only used in the constructor. */
    uint8_t abOsk0[32];
    /** The OSK1 value. This is currently only used in the constructor */
    uint8_t abOsk1[32];
} SMCState;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef IN_RING3
static char osk[64];

/* See http://www.mactel-linux.org/wiki/AppleSMC */
static struct AppleSMCData data[] =
{
    {6, "REV ", "\0x01\0x13\0x0f\0x00\0x00\0x03"},
    {32,"OSK0", osk },
    {32,"OSK1", osk+32 },
    {1, "NATJ",  "\0" },
    {1, "MSSP",  "\0" },
    {1, "MSSD",  "\0x3" },
    {1, "NTOK",  "\0"},
    {0, NULL,    NULL }
};
#endif /* IN_RING3 */
#ifdef IN_RING0
/** Do once for the SMC ring-0 static data (g_abOsk0, g_abOsk1, g_fHaveOsk).  */
static RTONCE   g_SmcR0Once = RTONCE_INITIALIZER;
/** Indicates whether we've successfully queried the OSK* keys. */
static bool     g_fHaveOsk = false;
/** The OSK0 value.   */
static uint8_t  g_abOsk0[32];
/** The OSK1 value.  */
static uint8_t  g_abOsk1[32];
#endif /* IN_RING0 */

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifdef IN_RING0

/**
 * Waits for the specified status on the host SMC.
 *
 * @returns success indicator.
 * @param   bStatus             The desired status.
 * @param   pszWhat             What we're currently doing. For the log.
 */
static bool devR0SmcWaitHostStatus(uint8_t bStatus, const char *pszWhat)
{
    uint8_t bCurStatus;
    for (uint32_t cMsSleep = 1; cMsSleep <= 64; cMsSleep <<= 1)
    {
        RTThreadSleep(cMsSleep);
        bCurStatus = ASMInU8(APPLESMC_CMD_PORT);
        if ((bCurStatus & 0xf) == bStatus)
            return true;
    }

    LogRel(("devR0Smc: %s: bCurStatus=%#x, wanted %#x.\n", pszWhat, bCurStatus, bStatus));
    return false;
}

/**
 * Reads a key by name from the host SMC.
 *
 * @returns success indicator.
 * @param   pszName             The key name, must be exactly 4 chars long.
 * @param   pbBuf               The output buffer.
 * @param   cbBuf               The buffer size. Max 32 bytes.
 */
static bool devR0SmcQueryHostKey(const char *pszName, uint8_t *pbBuf, size_t cbBuf)
{
    Assert(strlen(pszName) == 4);
    Assert(cbBuf <= 32);
    Assert(cbBuf > 0);

    /*
     * Issue the READ command.
     */
    uint32_t cMsSleep = 1;
    for (;;)
    {
        ASMOutU8(APPLESMC_CMD_PORT, APPLESMC_READ_CMD);
        RTThreadSleep(cMsSleep);
        uint8_t bCurStatus = ASMInU8(APPLESMC_CMD_PORT);
        if ((bCurStatus & 0xf) == 0xc)
            break;
        cMsSleep <<= 1;
        if (cMsSleep > 64)
        {
            LogRel(("devR0Smc: %s: bCurStatus=%#x, wanted %#x.\n", "cmd", bCurStatus, 0xc));
            return false;
        }
    }

    /*
     * Send it the key.
     */
    for (unsigned off = 0; off < 4; off++)
    {
        ASMOutU8(APPLESMC_DATA_PORT, pszName[off]);
        if (!devR0SmcWaitHostStatus(4, "key"))
            return false;
    }

    /*
     * The desired amount of output.
     */
    ASMOutU8(APPLESMC_DATA_PORT, (uint8_t)cbBuf);

    /*
     * Read the output.
     */
    for (size_t off = 0; off < cbBuf; off++)
    {
        if (!devR0SmcWaitHostStatus(5, off ? "data" : "len"))
            return false;
        pbBuf[off] = ASMInU8(APPLESMC_DATA_PORT);
    }

    return true;
}

/**
 * RTOnce callback that initializes g_fHaveOsk, g_abOsk0 and g_abOsk1.
 *
 * @returns VINF_SUCCESS.
 * @param   pvUserIgnored     Ignored.
 */
static DECLCALLBACK(int) devR0SmcInitOnce(void *pvUserIgnored)
{
    g_fHaveOsk = devR0SmcQueryHostKey("OSK0", &g_abOsk0[0], sizeof(g_abOsk0))
              && devR0SmcQueryHostKey("OSK1", &g_abOsk1[0], sizeof(g_abOsk1));

    NOREF(pvUserIgnored);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{FNPDMDEVREQHANDLERR0}
 */
PDMBOTHCBDECL(int) devR0SmcReqHandler(PPDMDEVINS pDevIns, uint32_t uOperation, uint64_t u64Arg)
{
    SMCState *pThis = PDMINS_2_DATA(pDevIns, SMCState *);
    int       rc    = VERR_INVALID_FUNCTION;

    if (uOperation == SMC_CALLR0_READ_OSK)
    {
        rc = RTOnce(&g_SmcR0Once, devR0SmcInitOnce, NULL);
        if (   RT_SUCCESS(rc)
            && g_fHaveOsk)
        {
            AssertCompile(sizeof(g_abOsk0) == sizeof(pThis->abOsk0));
            AssertCompile(sizeof(g_abOsk1) == sizeof(pThis->abOsk1));
            memcpy(pThis->abOsk0, g_abOsk0, sizeof(pThis->abOsk0));
            memcpy(pThis->abOsk1, g_abOsk1, sizeof(pThis->abOsk1));
        }
    }
    return rc;
}

#endif /* IN_RING0 */
#ifdef IN_RING3

/**
 * Saves a state of the SMC device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) smcSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    SMCState *pThis = PDMINS_2_DATA(pDevIns, SMCState *);

    /** @todo: implement serialization */
    return VINF_SUCCESS;
}


/**
 * Loads a SMC device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) smcLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t uVersion, uint32_t uPass)
{
    SMCState *pThis = PDMINS_2_DATA(pDevIns, SMCState *);

    if (uVersion != SMC_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /** @todo: implement serialization */
    return VINF_SUCCESS;
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) smcReset(PPDMDEVINS pDevIns)
{
    SMCState *pThis = PDMINS_2_DATA(pDevIns, SMCState *);
    LogFlow(("smcReset: \n"));
}


/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) smcInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    SMCState   *pThis = PDMINS_2_DATA(pDevIns, SMCState *);
}


static void applesmc_fill_data(SMCState *s)
{
    struct AppleSMCData *d;
    for (d=data; d->len; d++)
    {
        uint32_t key_data = *((uint32_t*)d->key);
        uint32_t key_current = *((uint32_t*)s->key);
        if (key_data == key_current)
        {
            Log(("APPLESMC: Key matched (%s Len=%d Data=%s)\n", d->key, d->len, d->data));
            memcpy(s->data, d->data, d->len);
            return;
        }
    }
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) smcIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    SMCState * s = PDMINS_2_DATA(pDevIns, SMCState *);
    uint8_t retval = 0;

    NOREF(pvUser);
    Log(("SMC port read: %x (%d)\n", Port, cb));

    /** @todo: status code? */
    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    switch (Port)
    {
        case APPLESMC_CMD_PORT:
        {
            retval = s->status;
            break;
        }
        case APPLESMC_DATA_PORT:
        {
            switch (s->cmd) {
                case APPLESMC_READ_CMD:
                    if(s->data_pos < s->data_len)
                    {
                        retval = s->data[s->data_pos];
                        Log(("APPLESMC: READ_DATA[%d] = %#hhx\n", s->data_pos, retval));
                        s->data_pos++;
                        if(s->data_pos == s->data_len)
                        {
                            s->status = 0x00;
                            Log(("APPLESMC: EOF\n"));
                        }
                        else
                            s->status = 0x05;
                    }
            }
            break;
        }
    }

    *pu32 = retval;

    return VINF_SUCCESS;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) smcIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    SMCState* s = PDMINS_2_DATA(pDevIns, SMCState *);

    NOREF(pvUser);

    Log(("SMC port write: %x (%d) %x\n", Port, cb, u32));
    /** @todo: status code? */
    if (cb != 1)
        return VINF_SUCCESS;

    switch (Port)
    {
        case APPLESMC_CMD_PORT:
        {
            switch (u32)
            {
                case APPLESMC_READ_CMD:
                    s->status = 0x0c;
                    break;
            }
            s->cmd = u32;
            s->read_pos = 0;
            s->data_pos = 0;
            break;
        }
        case APPLESMC_DATA_PORT:
        {
            switch(s->cmd)
            {
                case APPLESMC_READ_CMD:
                    if (s->read_pos < 4)
                    {
                        s->key[s->read_pos] = u32;
                        s->status = 0x04;
                    }
                    else
                    if (s->read_pos == 4)
                    {
                        s->data_len = u32;
                        s->status = 0x05;
                        s->data_pos = 0;
                        Log(("APPLESMC: Key = %c%c%c%c Len = %d\n", s->key[0], s->key[1], s->key[2], s->key[3], u32));
                        applesmc_fill_data(s);
                    }
                    s->read_pos++;
                    break;
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) smcConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    SMCState   *pThis = PDMINS_2_DATA(pDevIns, SMCState *);
    Assert(iInstance == 0);

    /*
     * Store state.
     */
    pThis->pDevIns = pDevIns;

    /*
     * Validate and read the configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DeviceKey|GetKeyFromRealSMC", "");

    /*
     * Read the DeviceKey config value.
     */
    char *pszDeviceKey;
    int rc = CFGMR3QueryStringAllocDef(pCfg, "DeviceKey", &pszDeviceKey, "");
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"DeviceKey\" as a string failed"));

    size_t cchDeviceKey = strlen(pszDeviceKey);
    if (cchDeviceKey > 0)
        memcpy(&pThis->abOsk0[0], pszDeviceKey, RT_MIN(cchDeviceKey, sizeof(pThis->abOsk0)));
    if (cchDeviceKey > sizeof(pThis->abOsk0))
        memcpy(&pThis->abOsk1[0], &pszDeviceKey[sizeof(pThis->abOsk0)],
               RT_MIN(cchDeviceKey - sizeof(pThis->abOsk0), sizeof(pThis->abOsk1)));

    MMR3HeapFree(pszDeviceKey);

    /*
     * Query the key from the real hardware if asked to do so.
     */
    bool fGetKeyFromRealSMC;
    rc = CFGMR3QueryBoolDef(pCfg, "GetKeyFromRealSMC", &fGetKeyFromRealSMC, false);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"GetKeyFromRealSMC\" as a boolean failed"));
    if (fGetKeyFromRealSMC)
    {
        rc = PDMDevHlpCallR0(pDevIns, SMC_CALLR0_READ_OSK, 0 /*u64Arg*/);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Failed to query SMC value from the host"));
    }

    /*
     * For practical/historical reasons, the OSK[0|1] data is stored in a
     * global buffer in ring-3.
     */
    AssertCompile(sizeof(osk) == sizeof(pThis->abOsk0) + sizeof(pThis->abOsk1));
    AssertCompile(sizeof(char) == sizeof(uint8_t));
    memcpy(osk, pThis->abOsk0, sizeof(pThis->abOsk0));
    memcpy(&osk[sizeof(pThis->abOsk0)], pThis->abOsk1, sizeof(pThis->abOsk1));

    /*
     * Register the IO ports.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, APPLESMC_DATA_PORT, 1, NULL,
                                 smcIOPortWrite, smcIOPortRead,
                                 NULL, NULL, "SMC Data");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, APPLESMC_CMD_PORT, 1, NULL,
                                 smcIOPortWrite, smcIOPortRead,
                                 NULL, NULL, "SMC Commands");
    if (RT_FAILURE(rc))
        return rc;

    /* Register saved state management */
    rc = PDMDevHlpSSMRegister(pDevIns, SMC_SAVED_STATE_VERSION, sizeof(*pThis), smcSaveExec, smcLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the device state.
     */
    smcReset(pDevIns);

    /**
     * @todo: Register statistics.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "smc", "Display SMC status. (no arguments)", smcInfo);

    return VINF_SUCCESS;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceSMC =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "smc",
    /* szRCMod */
    "VBoxDD2GC.gc",
    /* szR0Mod */
    "VBoxDD2R0.r0",
    /* pszDescription */
    "System Management Controller (SMC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36| PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(SMCState),
    /* pfnConstruct */
    smcConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    smcReset,
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
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
