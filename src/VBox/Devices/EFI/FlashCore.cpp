/* $Id$ */
/** @file
 * DevFlash - A simple Flash device
 *
 * A simple non-volatile byte-wide (x8) memory device modeled after Intel 28F008
 * FlashFile. See 28F008SA datasheet, Intel order number 290429-007.
 *
 * Implemented as an MMIO device attached directly to the CPU, not behind any
 * bus. Typically mapped as part of the firmware image.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_FLASH
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/file.h>

#include "VBoxDD.h"
#include "FlashCore.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @name CUI (Command User Interface) Commands.
 *  @{ */
#define FLASH_CMD_ALT_WRITE             0x10
#define FLASH_CMD_ERASE_SETUP           0x20
#define FLASH_CMD_WRITE                 0x40
#define FLASH_CMD_STS_CLEAR             0x50
#define FLASH_CMD_STS_READ              0x70
#define FLASH_CMD_READ_ID               0x90
#define FLASH_CMD_ERASE_SUS_RES         0xB0
#define FLASH_CMD_ERASE_CONFIRM         0xD0
#define FLASH_CMD_ARRAY_READ            0xFF
/** @} */

/** @name Status register bits.
 *  @{ */
#define FLASH_STATUS_WSMS               0x80    /* Write State Machine Status, 1=Ready */
#define FLASH_STATUS_ESS                0x40    /* Erase Suspend Status, 1=Suspended */
#define FLASH_STATUS_ES                 0x20    /* Erase Status, 1=Error */
#define FLASH_STATUS_BWS                0x10    /* Byte Write Status, 1=Error */
#define FLASH_STATUS_VPPS               0x08    /* Vpp Status, 1=Low Vpp */
/* The remaining bits 0-2 are reserved/unused */
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE


#ifdef IN_RING3 /* for now */

static int flashMemWriteByte(PFLASHCORE pThis, uint32_t off, uint8_t bCmd)
{
    int rc = VINF_SUCCESS;
    unsigned uOffset;

    /* NB: Older datasheets (e.g. 28F008SA) suggest that for two-cycle commands like byte write or
     * erase setup, the address is significant in both cycles, but do not explain what happens
     * should the addresses not match. Newer datasheets (e.g. 28F008B3) clearly say that the address
     * in the first byte cycle never matters. We prefer the latter interpretation.
     */

    if (pThis->cBusCycle == 0)
    {
        /* First bus write cycle, start processing a new command. Address is ignored. */
        switch (bCmd)
        {
            case FLASH_CMD_ARRAY_READ:
            case FLASH_CMD_STS_READ:
            case FLASH_CMD_ERASE_SUS_RES:
            case FLASH_CMD_READ_ID:
                /* Single-cycle write commands, only change the current command. */
                pThis->bCmd = bCmd;
                break;
            case FLASH_CMD_STS_CLEAR:
                /* Status clear continues in read mode. */
                pThis->bStatus = 0;
                pThis->bCmd = FLASH_CMD_ARRAY_READ;
                break;
            case FLASH_CMD_WRITE:
            case FLASH_CMD_ALT_WRITE:
            case FLASH_CMD_ERASE_SETUP:
                /* Two-cycle commands, advance the bus write cycle. */
                pThis->bCmd = bCmd;
                pThis->cBusCycle++;
                break;
            default:
                LogFunc(("1st cycle command %02X, current cmd %02X\n", bCmd, pThis->bCmd));
                break;
        }
    }
    else
    {
        /* Second write of a two-cycle command. */
        Assert(pThis->cBusCycle == 1);
        switch (pThis->bCmd)
        {
            case FLASH_CMD_WRITE:
            case FLASH_CMD_ALT_WRITE:
                uOffset = off & (pThis->cbFlashSize - 1);
                if (uOffset < pThis->cbFlashSize)
                {
                    pThis->pbFlash[uOffset] = bCmd;
                    /* NB: Writes are instant and never fail. */
                    LogFunc(("wrote byte to flash at %08RX32: %02X\n", off, bCmd));
                }
                else
                    LogFunc(("ignoring write at %08RX32: %02X\n", off, bCmd));
                break;
            case FLASH_CMD_ERASE_SETUP:
                if (bCmd == FLASH_CMD_ERASE_CONFIRM)
                {
                    /* The current address determines the block to erase. */
                    uOffset = off & (pThis->cbFlashSize - 1);
                    uOffset = uOffset & ~(pThis->cbBlockSize - 1);
                    memset(pThis->pbFlash + uOffset, 0xff, pThis->cbBlockSize);
                    LogFunc(("Erasing block at offset %u\n", uOffset));
                }
                else
                {
                    /* Anything else is a command erorr. Transition to status read mode. */
                    LogFunc(("2st cycle erase command is %02X, should be confirm (%02X)\n", bCmd, FLASH_CMD_ERASE_CONFIRM));
                    pThis->bCmd = FLASH_CMD_STS_READ;
                    pThis->bStatus |= FLASH_STATUS_BWS | FLASH_STATUS_ES;
                }
                break;
            default:
                LogFunc(("2st cycle bad command %02X, current cmd %02X\n", bCmd, pThis->bCmd));
                break;
        }
        pThis->cBusCycle = 0;
    }
    LogFlow(("flashMemWriteByte: write access at %08RX32: %#x rc=%Rrc\n", off, bCmd, rc));
    return rc;
}


static int flashMemReadByte(PFLASHCORE pThis, uint32_t off, uint8_t *pbData)
{
    uint8_t bValue;
    unsigned uOffset;
    int rc = VINF_SUCCESS;

    /*
     * Reads are only defined in three states: Array read, status register read,
     * and ID read.
     */
    switch (pThis->bCmd)
    {
        case FLASH_CMD_ARRAY_READ:
            uOffset = off & (pThis->cbFlashSize - 1);
            bValue = pThis->pbFlash[uOffset];
            LogFunc(("read byte at %08RX32: %02X\n", off, bValue));
            break;
        case FLASH_CMD_STS_READ:
            bValue = pThis->bStatus;
            break;
        case FLASH_CMD_READ_ID:
            bValue = off & 1 ?  RT_HI_U8(pThis->u16FlashId) : RT_LO_U8(pThis->u16FlashId);
            break;
        default:
            bValue = 0xff;
            break;
    }
    *pbData = bValue;

    LogFlow(("flashMemReadByte: read access at %08RX: %02X (cmd=%02X) rc=%Rrc\n", bValue, pThis->bCmd, rc));
    return rc;
}

DECLHIDDEN(int) flashWrite(PFLASHCORE pThis, uint32_t off, const void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    const uint8_t *pu8Mem = (const uint8_t *)pv;

#ifndef IN_RING3
    if (cb > 1)
        return VINF_IOM_R3_IOPORT_WRITE;
#endif

    for (uint32_t uOffset = 0; uOffset < cb; ++uOffset)
    {
        rc = flashMemWriteByte(pThis, off + uOffset, pu8Mem[uOffset]);
        if (!RT_SUCCESS(rc))
            break;
    }

    LogFlow(("flashWrite: completed write at %08RX32 (LB %u): rc=%Rrc\n", off, cb, rc));
    return rc;
}

DECLHIDDEN(int) flashRead(PFLASHCORE pThis, uint32_t off, void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    uint8_t *pu8Mem = (uint8_t *)pv;

    /*
     * Reading can always be done witout going back to R3. Reads do not
     * change the device state and we always have the data.
     */
    for (uint32_t uOffset = 0; uOffset < cb; ++uOffset, ++pu8Mem)
    {
        rc = flashMemReadByte(pThis, off + uOffset, pu8Mem);
        if (!RT_SUCCESS(rc))
            break;
    }

    LogFlow(("flashRead: completed read at %08RX32 (LB %u): rc=%Rrc\n", off, cb, rc));
    return rc;
}

#endif /* IN_RING3 for now */

#ifdef IN_RING3

DECLHIDDEN(int) flashR3Init(PFLASHCORE pThis, PPDMDEVINS pDevIns, uint16_t idFlashDev, uint32_t cbFlash, uint16_t cbBlock)
{
    pThis->pDevIns     = pDevIns;
    pThis->u16FlashId  = idFlashDev;
    pThis->cbBlockSize = cbBlock;
    pThis->cbFlashSize = cbFlash;

    /* Set up the flash data. */
    pThis->pbFlash = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, pThis->cbFlashSize);
    if (!pThis->pbFlash)
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("Failed to allocate heap memory"));

    /* Default value for empty flash. */
    memset(pThis->pbFlash, 0xff, pThis->cbFlashSize);

    /* Reset the dynamic state.*/
    flashR3Reset(pThis);
    return VINF_SUCCESS;
}

DECLHIDDEN(void) flashR3Destruct(PFLASHCORE pThis)
{
    if (pThis->pbFlash)
    {
        PDMDevHlpMMHeapFree(pThis->pDevIns, pThis->pbFlash);
        pThis->pbFlash = NULL;
    }
}

DECLHIDDEN(int) flashR3LoadFromFile(PFLASHCORE pThis, const char *pszFilename)
{
    RTFILE hFlashFile = NIL_RTFILE;

    int rc = RTFileOpen(&hFlashFile, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pThis->pDevIns, rc, N_("Failed to open flash file"));

    size_t cbRead = 0;
    rc = RTFileRead(hFlashFile, pThis->pbFlash, pThis->cbFlashSize, &cbRead);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pThis->pDevIns, rc, N_("Failed to read flash file"));
    Log(("Read %zu bytes from file (asked for %u)\n.", cbRead, pThis->cbFlashSize));

    RTFileClose(hFlashFile);
    return VINF_SUCCESS;
}

DECLHIDDEN(int) flashR3LoadFromBuf(PFLASHCORE pThis, void *pvBuf, size_t cbBuf)
{
    AssertReturn(pThis->cbFlashSize >= cbBuf, VERR_BUFFER_OVERFLOW);

    memcpy(pThis->pbFlash, pvBuf, RT_MIN(cbBuf, pThis->cbFlashSize));
    return VINF_SUCCESS;
}

DECLHIDDEN(int) flashR3SaveToFile(PFLASHCORE pThis, const char *pszFilename)
{
    RTFILE hFlashFile = NIL_RTFILE;

    int rc = RTFileOpen(&hFlashFile, pszFilename, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pThis->pDevIns, rc, N_("Failed to open flash file"));

    rc = RTFileWrite(hFlashFile, pThis->pbFlash, pThis->cbFlashSize, NULL);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pThis->pDevIns, rc, N_("Failed to write flash file"));

    RTFileClose(hFlashFile);
    return VINF_SUCCESS;
}

DECLHIDDEN(int) flashR3SaveToBuf(PFLASHCORE pThis, void *pvBuf, size_t cbBuf)
{
    AssertReturn(pThis->cbFlashSize <= cbBuf, VERR_BUFFER_OVERFLOW);

    memcpy(pvBuf, pThis->pbFlash, RT_MIN(cbBuf, pThis->cbFlashSize));
    return VINF_SUCCESS;
}

DECLHIDDEN(void) flashR3Reset(PFLASHCORE pThis)
{
    /*
     * Initialize the device state.
     */
    pThis->bCmd      = FLASH_CMD_ARRAY_READ;
    pThis->bStatus   = 0;
    pThis->cBusCycle = 0;
}

DECLHIDDEN(int) flashR3SsmSaveExec(PFLASHCORE pThis, PSSMHANDLE pSSM)
{
    SSMR3PutU32(pSSM, FLASH_SAVED_STATE_VERSION);

    /* Save the device state. */
    SSMR3PutU8(pSSM, pThis->bCmd);
    SSMR3PutU8(pSSM, pThis->bStatus);
    SSMR3PutU8(pSSM, pThis->cBusCycle);

    /* Save the current configuration for validation purposes. */
    SSMR3PutU16(pSSM, pThis->cbBlockSize);
    SSMR3PutU16(pSSM, pThis->u16FlashId);

    /* Save the current flash contents. */
    SSMR3PutU32(pSSM, pThis->cbFlashSize);
    SSMR3PutMem(pSSM, pThis->pbFlash, pThis->cbFlashSize);

    return VINF_SUCCESS;
}

DECLHIDDEN(int) flashR3SsmLoadExec(PFLASHCORE pThis, PSSMHANDLE pSSM)
{
    uint32_t uVersion = FLASH_SAVED_STATE_VERSION;
    int rc = SSMR3GetU32(pSSM, &uVersion);
    AssertRCReturn(rc, rc);

    /*
     * Do the actual restoring.
     */
    if (uVersion == FLASH_SAVED_STATE_VERSION)
    {
        uint16_t    u16Val;
        uint32_t    u32Val;

        SSMR3GetU8(pSSM, &pThis->bCmd);
        SSMR3GetU8(pSSM, &pThis->bStatus);
        SSMR3GetU8(pSSM, &pThis->cBusCycle);

        /* Make sure configuration didn't change behind our back. */
        SSMR3GetU16(pSSM, &u16Val);
        if (u16Val != pThis->cbBlockSize)
            return VERR_SSM_LOAD_CONFIG_MISMATCH;
        SSMR3GetU16(pSSM, &u16Val);
        if (u16Val != pThis->u16FlashId)
            return VERR_SSM_LOAD_CONFIG_MISMATCH;
        SSMR3GetU32(pSSM, &u32Val);
        if (u16Val != pThis->cbFlashSize)
            return VERR_SSM_LOAD_CONFIG_MISMATCH;

        /* Suck in the flash contents. */
        SSMR3GetMem(pSSM, pThis->pbFlash, pThis->cbFlashSize);
    }
    else
        rc = VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    return rc;
}

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
