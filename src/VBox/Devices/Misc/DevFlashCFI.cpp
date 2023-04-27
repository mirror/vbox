/* $Id$ */
/** @file
 * DevFlashCFI - A simple Flash device implementing the Common Flash Interface
 * using the sepc from https://ia803103.us.archive.org/30/items/m30l0r7000t0/m30l0r7000t0.pdf
 *
 * Implemented as an MMIO device attached directly to the CPU, not behind any
 * bus. Typically mapped as part of the firmware image.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
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


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @name CFI (Command User Interface) Commands.
 *  @{ */
/** Block erase setup */
#define FLASH_CFI_CMD_BLOCK_ERASE_SETUP         0x20
/** Clear status register. */
#define FLASH_CFI_CMD_CLEAR_STATUS_REG          0x50
/** Read status register. */
#define FLASH_CFI_CMD_READ_STATUS_REG           0x70
/** Read status register. */
#define FLASH_CFI_CMD_READ_DEVICE_ID            0x90
/** Buffered program setup. */
#define FLASH_CFI_CMD_BUFFERED_PROGRAM_SETUP    0xe8
/** Buffered program setup. */
#define FLASH_CFI_CMD_BUFFERED_PROGRAM_CONFIRM  0xd0
/** Read from the flash array. */
#define FLASH_CFI_CMD_ARRAY_READ                0xff
/** @} */

/** @name Status register bits.
 *  @{ */
/** Bank Write/Multiple Word Program Status Bit. */
#define FLASH_CFI_SR_BWS                        RT_BIT(0)
/** Block Protection Status Bit. */
#define FLASH_CFI_SR_BLOCK_PROTETION            RT_BIT(1)
#define FLASH_CFI_SR_PROGRAM_SUSPEND            RT_BIT(2)
#define FLASH_CFI_SR_VPP                        RT_BIT(3)
#define FLASH_CFI_SR_PROGRAM                    RT_BIT(4)
#define FLASH_CFI_SR_ERASE                      RT_BIT(5)
#define FLASH_CFI_SR_ERASE_SUSPEND              RT_BIT(6)
#define FLASH_CFI_SR_PROGRAM_ERASE              RT_BIT(7)
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The flash device, shared state.
 */
typedef struct DEVFLASHCFI
{
    /** The current command. */
    uint16_t                u16Cmd;
    /** The status register. */
    uint8_t                 bStatus;
    /** Current bus cycle. */
    uint8_t                 cBusCycle;

    uint8_t                 cWordsTransfered;

    /** @name The following state does not change at runtime
     * @{ */
    /** When set, indicates the state was saved. */
    bool                    fStateSaved;
    /** Manufacturer (high byte) and device (low byte) ID. */
    uint16_t                u16FlashId;
    /** The configured block size of the device. */
    uint16_t                cbBlockSize;
    /** The actual flash memory data.  */
    R3PTRTYPE(uint8_t *)    pbFlash;
    /** The flash memory region size.  */
    uint32_t                cbFlashSize;
    /** @} */

    /** The file conaining the flash content. */
    char                    *pszFlashFile;
    /** The guest physical memory base address. */
    RTGCPHYS                GCPhysFlashBase;
    /** The handle to the MMIO region. */
    IOMMMIOHANDLE           hMmio;
} DEVFLASHCFI;
/** Pointer to the Flash device state. */
typedef DEVFLASHCFI *PDEVFLASHCFI;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE, Flash memory write}
 */
static DECLCALLBACK(VBOXSTRICTRC) flashMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    PDEVFLASHCFI pThis = PDMDEVINS_2_DATA(pDevIns, PDEVFLASHCFI);
    RT_NOREF1(pvUser);

    /** @todo For now we only emulate a x32 device using two x16 chips so the command must be send to both chips at the same time
     * (and we don't support sending different commands to both devices). */
    Assert(cb == sizeof(uint32_t));
    uint32_t u32Val = *(const uint32_t *)pv;

    LogFlowFunc(("off=%RGp u32Val=%#x cb=%u\n", off, u32Val, cb));

    if (pThis->cBusCycle == 0)
    {
        /* Writes new command, the comand must be sent to both chips at the same time. */
        Assert((u32Val >> 16) == (u32Val & 0xffff));
        switch (u32Val & 0xffff)
        {
            case FLASH_CFI_CMD_READ_STATUS_REG:
            case FLASH_CFI_CMD_ARRAY_READ:
            case FLASH_CFI_CMD_READ_DEVICE_ID:
                pThis->u16Cmd = u32Val;
                break;
            case FLASH_CFI_CMD_BLOCK_ERASE_SETUP:
                pThis->u16Cmd = u32Val;
                pThis->cBusCycle++;
                break;
            case FLASH_CFI_CMD_BUFFERED_PROGRAM_SETUP:
                Assert(pThis->bStatus & FLASH_CFI_SR_PROGRAM_ERASE);
                pThis->u16Cmd = u32Val;
                pThis->cBusCycle++;
                break;
            case FLASH_CFI_CMD_CLEAR_STATUS_REG:
                pThis->bStatus &= ~(FLASH_CFI_SR_BLOCK_PROTETION | FLASH_CFI_SR_VPP | FLASH_CFI_SR_PROGRAM | FLASH_CFI_SR_ERASE);
                pThis->u16Cmd  = FLASH_CFI_CMD_ARRAY_READ;
                break;
            default:
                AssertReleaseFailed();
        }
    }
    else if (pThis->cBusCycle == 1)
    {
        switch (pThis->u16Cmd)
        {
            case FLASH_CFI_CMD_BLOCK_ERASE_SETUP:
            {
                /* This contains the address. */
                pThis->u16Cmd = FLASH_CFI_CMD_READ_STATUS_REG;
                pThis->cBusCycle = 0;
                break;
            }
            case FLASH_CFI_CMD_BUFFERED_PROGRAM_SETUP:
            {
                /* Receives the number of words to be transfered. */
                pThis->cWordsTransfered = u32Val & 0xffff;
                pThis->cBusCycle++;
                break;
            }
        }
    }
    else if (pThis->cBusCycle == 2)
    {
        switch (pThis->u16Cmd)
        {
            case FLASH_CFI_CMD_BUFFERED_PROGRAM_SETUP:
            {
                /* Receives the address and data. */
                if (!pThis->cWordsTransfered)
                {
                    /* Should be the confirm now. */
                    if ((u32Val & 0xffff) == FLASH_CFI_CMD_BUFFERED_PROGRAM_CONFIRM)
                    {
                        /** @todo Write out data to flash. */
                        /* Reset to read array. */
                        pThis->cBusCycle = 0;
                        pThis->u16Cmd    = FLASH_CFI_CMD_ARRAY_READ;
                    }
                }
                else
                    pThis->cWordsTransfered--;
                break;
            }
        }
    }
    else
        AssertReleaseFailed();

    LogFlow(("flashWrite: completed write at %08RX32 (LB %u)\n", off, cb));
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD, Flash memory read}
 */
static DECLCALLBACK(VBOXSTRICTRC) flashMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PDEVFLASHCFI pThis = PDMDEVINS_2_DATA(pDevIns, PDEVFLASHCFI);
    RT_NOREF1(pvUser);

    LogFlowFunc(("off=%RGp cb=%u\n", off, cb));

    if (pThis->u16Cmd == FLASH_CFI_CMD_ARRAY_READ)
    {
        size_t cbThisRead = RT_MIN(cb, pThis->cbFlashSize - off);
        size_t cbSetFf    = cb - cbThisRead;
        if (cbThisRead)
            memcpy(pv, &pThis->pbFlash[off], cbThisRead);
        if (cbSetFf)
            memset((uint8_t *)pv + cbThisRead, 0xff, cbSetFf);
    }
    else
    {
        /** @todo For now we only emulate a x32 device using two x16 chips so the command must be send to both chips at the same time. */
        Assert(cb == sizeof(uint32_t));

        uint32_t *pu32 = (uint32_t *)pv;
        switch (pThis->u16Cmd)
        {
            case FLASH_CFI_CMD_READ_DEVICE_ID:
                *pu32 = 0; /** @todo */
                break;
            case FLASH_CFI_CMD_READ_STATUS_REG:
            case FLASH_CFI_CMD_BUFFERED_PROGRAM_SETUP:
                *pu32 = ((uint32_t)pThis->bStatus << 16) | pThis->bStatus;
                break;
            default:
                AssertReleaseFailed();
        }
    }

    LogFlow(("flashRead: completed read at %08RX32 (LB %u)\n", off, cb));
    return VINF_SUCCESS;
}


#if 0 /** @todo Later */
/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) flashSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PDEVFLASH pThis = PDMDEVINS_2_DATA(pDevIns, PDEVFLASH);
    return flashR3SaveExec(&pThis->Core, pDevIns, pSSM);
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) flashLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PDEVFLASH pThis = PDMDEVINS_2_DATA(pDevIns, PDEVFLASH);
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /* Fend off unsupported versions. */
    if (uVersion != FLASH_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    return flashR3LoadExec(&pThis->Core, pDevIns, pSSM);
}
#endif

/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) flashR3Reset(PPDMDEVINS pDevIns)
{
    PDEVFLASHCFI pThis = PDMDEVINS_2_DATA(pDevIns, PDEVFLASHCFI);

    /*
     * Initialize the device state.
     */
    pThis->u16Cmd    = FLASH_CFI_CMD_ARRAY_READ;
    pThis->bStatus   = FLASH_CFI_SR_PROGRAM_ERASE; /* Prgram/Erase controller is inactive. */
    pThis->cBusCycle = 0;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) flashR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    PDEVFLASHCFI pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVFLASHCFI);

    if (pThis->pszFlashFile)
    {
        RTFILE hFlashFile = NIL_RTFILE;

        int rc = RTFileOpen(&hFlashFile, pThis->pszFlashFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE);
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to open flash file"));

        rc = RTFileWrite(hFlashFile, pThis->pbFlash, pThis->cbFlashSize, NULL);
        RTFileClose(hFlashFile);
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to write flash file"));

        PDMDevHlpMMHeapFree(pDevIns, pThis->pszFlashFile);
        pThis->pszFlashFile = NULL;
    }

    if (pThis->pbFlash)
    {
        PDMDevHlpMMHeapFree(pDevIns, pThis->pbFlash);
        pThis->pbFlash = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) flashR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PDEVFLASHCFI    pThis   = PDMDEVINS_2_DATA(pDevIns, PDEVFLASHCFI);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;

    Assert(iInstance == 0); RT_NOREF1(iInstance);

    /*
     * Validate configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DeviceId|BaseAddress|Size|BlockSize|FlashFile", "");

    /*
     * Read configuration.
     */

    uint16_t u16FlashId = 0;
    int rc = pHlp->pfnCFGMQueryU16Def(pCfg, "DeviceId", &u16FlashId, 0xA289);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"DeviceId\" as an integer failed"));

    /* The default base address is 2MB below 4GB. */
    rc = pHlp->pfnCFGMQueryU64Def(pCfg, "BaseAddress", &pThis->GCPhysFlashBase, 0xFFE00000);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"BaseAddress\" as an integer failed"));

    /* The default flash device size is 128K. */
    uint32_t cbFlash = 0;
    rc = pHlp->pfnCFGMQueryU32Def(pCfg, "Size", &cbFlash, 128 * _1K);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"Size\" as an integer failed"));

    /* The default flash device block size is 4K. */
    uint16_t cbBlock = 0;
    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "BlockSize", &cbBlock, _4K);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"BlockSize\" as an integer failed"));

    rc = pHlp->pfnCFGMQueryStringAlloc(pCfg, "FlashFile", &pThis->pszFlashFile);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"FlashFile\" as a string failed"));

    /*
     * Initialize the flash core.
     */
    pThis->u16FlashId  = u16FlashId;
    pThis->cbBlockSize = cbBlock;
    pThis->cbFlashSize = cbFlash;

    /* Set up the flash data. */
    pThis->pbFlash = (uint8_t *)PDMDevHlpMMHeapAlloc(pDevIns, pThis->cbFlashSize);
    if (!pThis->pbFlash)
        return PDMDEV_SET_ERROR(pDevIns, VERR_NO_MEMORY, N_("Failed to allocate heap memory"));

    /* Default value for empty flash. */
    memset(pThis->pbFlash, 0xff, pThis->cbFlashSize);

    /* Reset the dynamic state.*/
    flashR3Reset(pDevIns);

    RTFILE hFlashFile = NIL_RTFILE;
    rc = RTFileOpen(&hFlashFile, pThis->pszFlashFile, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to open flash file"));

    size_t cbRead = 0;
    rc = RTFileRead(hFlashFile, pThis->pbFlash, pThis->cbFlashSize, &cbRead);
    RTFileClose(hFlashFile);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to read flash file"));
    LogRel(("flash-cfi#%u: Read %zu bytes from file (asked for %u)\n.", cbRead, pThis->cbFlashSize, iInstance));


    /*
     * Register MMIO region.
     */
    rc = PDMDevHlpMmioCreateExAndMap(pDevIns, pThis->GCPhysFlashBase, cbFlash,
                                     IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU, NULL, UINT32_MAX,
                                     flashMMIOWrite, flashMMIORead, NULL, NULL, "Flash Memory", &pThis->hMmio);
    AssertRCReturn(rc, rc);
    LogRel(("Registered %uKB flash at %RGp\n", pThis->cbFlashSize / _1K, pThis->GCPhysFlashBase));

#if 0 /** @todo Later */
    /*
     * Register saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, FLASH_SAVED_STATE_VERSION, sizeof(*pThis), flashSaveExec, flashLoadExec);
    AssertRCReturn(rc, rc);
#endif

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceFlashCFI =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "flash-cfi",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_ARCH,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(DEVFLASHCFI),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Flash Memory Device",
#if defined(IN_RING3)
    /* .pszRCMod = */               "",
    /* .pszR0Mod = */               "",
    /* .pfnConstruct = */           flashR3Construct,
    /* .pfnDestruct = */            flashR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               flashR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
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

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
