/* $Id$ */
/**
 * @file
 * DevSMC - SMC device emulation.
 */
/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
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
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "../Builtins.h"

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

static char osk[64];

/** The version of the saved state. */
#define SMC_SAVED_STATE_VERSION 1

typedef struct AppleSMCData
{
    uint8_t       len;
    const char    *key;
    const char    *data;
} AppleSMCData;

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

    char*    pszDeviceKey;
} SMCState;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

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
 * Relocation notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @param   offDelta    The delta relative to the old address.
 */
static DECLCALLBACK(void) smcRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    SMCState *pThis = PDMINS_2_DATA(pDevIns, SMCState *);
    /* SMC device lives only in R3 now, thus nothing to relocate yet */
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
    int         rc;
    Assert(iInstance == 0);

    /*
     * Store state.
     */
    pThis->pDevIns = pDevIns;

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "DeviceKey\0"
                              ))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("Configuration error: Invalid config value(s) for the SMC device"));

    /*
     * Query device key
     */
    rc = CFGMR3QueryStringAlloc(pCfg, "DeviceKey", &pThis->pszDeviceKey);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        pThis->pszDeviceKey = RTStrDup("Invalid");
        LogRel(("Invalid SMC device key\n"));
        if (!pThis->pszDeviceKey)
            return VERR_NO_MEMORY;

        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Configuration error: Querying \"DeviceKey\" as a string failed"));

    memcpy(osk, pThis->pszDeviceKey, RTStrNLen(pThis->pszDeviceKey, 64));

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
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) smcDestruct(PPDMDEVINS pDevIns)
{
    SMCState*  pThis = PDMINS_2_DATA(pDevIns, SMCState*);

    /*
     * Free MM heap pointers.
     */
    if (pThis->pszDeviceKey)
    {
        MMR3HeapFree(pThis->pszDeviceKey);
        pThis->pszDeviceKey = NULL;
    }

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
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    " System Management Controller (SMC) Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(SMCState),
    /* pfnConstruct */
    smcConstruct,
    /* pfnDestruct */
    smcDestruct,
    /* pfnRelocate */
    smcRelocate,
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
