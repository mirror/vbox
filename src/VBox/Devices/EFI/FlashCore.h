/* $Id$ */
/** @file
 * A simple Flash device
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

#ifndef VBOX_INCLUDED_SRC_EFI_FlashCore_h
#define VBOX_INCLUDED_SRC_EFI_FlashCore_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/vmm/pdmdev.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/file.h>

#include "VBoxDD.h"

RT_C_DECLS_BEGIN

/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The current version of the saved state. */
#define FLASH_SAVED_STATE_VERSION           1


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The flash device core structure.
 */
typedef struct FLASHCORE
{
    /** Owning device instance. */
    PPDMDEVINS          pDevIns;
    /** The current command. */
    uint8_t             bCmd;
    /** The status register. */
    uint8_t             bStatus;
    /** Current bus cycle. */
    uint8_t             cBusCycle;

    uint8_t             uPadding0;

    /* The following state does not change at runtime.*/
    /** Manufacturer (high byte) and device (low byte) ID. */
    uint16_t            u16FlashId;
    /** The configured block size of the device. */
    uint16_t            cbBlockSize;
    /** The flash memory region size.  */
    uint32_t            cbFlashSize;
    /** The actual flash memory data.  */
    uint8_t             *pbFlash;
    /** When set, indicates the state was saved. */
    bool                fStateSaved;
} FLASHCORE;

/** Pointer to the Flash device state. */
typedef FLASHCORE *PFLASHCORE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * Performs a write to the given flash offset.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   off                 Offset to start writing to.
 * @param   pv                  The value to write.
 * @param   cb                  Number of bytes to write.
 */
DECLHIDDEN(int) flashWrite(PFLASHCORE pThis, uint32_t off, const void *pv, size_t cb);

/**
 * Performs a read from the given flash offset.
 *
 * @returns VBox status code.
 * @param   pThis               The UART core instance.
 * @param   off                 Offset to start reading from.
 * @param   pv                  Where to store the read data.
 * @param   cb                  Number of bytes to read.
 */
DECLHIDDEN(int) flashRead(PFLASHCORE pThis, uint32_t off, void *pv, size_t cb);

# ifdef IN_RING3

/**
 * Initialiizes the given flash device instance.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pDevIns             Pointer to the owning device instance.
 * @param   idFlashDev          The flash device ID.
 * @param   GCPhysFlashBase     Base MMIO address where the flash is located.
 * @param   cbFlash             Size of the flash device in bytes.
 * @param   cbBlock             Size of a flash block.
 */
DECLHIDDEN(int) flashR3Init(PFLASHCORE pThis, PPDMDEVINS pDevIns, uint16_t idFlashDev, uint32_t cbFlash, uint16_t cbBlock);

/**
 * Destroys the given flash device instance.
 *
 * @returns nothing.
 * @param   pThis               The flash device core instance.
 */
DECLHIDDEN(void) flashR3Destruct(PFLASHCORE pThis);

/**
 * Loads the flash content from the given file.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pszFilename         The file to load the flash content from.
 */
DECLHIDDEN(int) flashR3LoadFromFile(PFLASHCORE pThis, const char *pszFilename);

/**
 * Loads the flash content from the given buffer.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pvBuf               The buffer to load the content from.
 * @param   cbBuf               Size of the buffer in bytes.
 */
DECLHIDDEN(int) flashR3LoadFromBuf(PFLASHCORE pThis, void const *pvBuf, size_t cbBuf);

/**
 * Saves the flash content to the given file.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pszFilename         The file to save the flash content to.
 */
DECLHIDDEN(int) flashR3SaveToFile(PFLASHCORE pThis, const char *pszFilename);

/**
 * Saves the flash content to the given buffer.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pvBuf               The buffer to save the content to.
 * @param   cbBuf               Size of the buffer in bytes.
 */
DECLHIDDEN(int) flashR3SaveToBuf(PFLASHCORE pThis, void *pvBuf, size_t cbBuf);

/**
 * Resets the dynamic part of the flash device state.
 *
 * @returns nothing.
 * @param   pThis               The flash device core instance.
 */
DECLHIDDEN(void) flashR3Reset(PFLASHCORE pThis);

/**
 * Saves the flash device state to the given SSM handle.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pSSM                The SSM handle to save to.
 */
DECLHIDDEN(int) flashR3SsmSaveExec(PFLASHCORE pThis, PSSMHANDLE pSSM);

/**
 * Loads the flash device state from the given SSM handle.
 *
 * @returns VBox status code.
 * @param   pThis               The flash device core instance.
 * @param   pSSM                The SSM handle to load from.
 */
DECLHIDDEN(int) flashR3SsmLoadExec(PFLASHCORE pThis, PSSMHANDLE pSSM);

# endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_SRC_EFI_FlashCore_h */

