/** @file
 *
 * VBox storage devices:
 * PIIX3 ATA busmaster controller definitions
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __PIIX3ATABmDma_h__
#define __PIIX3ATABmDma_h__


/** @defgroup grp_piix3atabmdma     PIIX3 ATA Bus Master DMA
 * @{
 */

/** @name BM_STATUS
 * @{
 */
/** Currently performing a DMA operation. */
#define BM_STATUS_DMAING 0x01
/** An error occurred during the DMA operation. */
#define BM_STATUS_ERROR  0x02
/** The DMA unit has raised the IDE interrupt line. */
#define BM_STATUS_INT    0x04
/** User-defined bit 0, commonly used to signal that drive 0 supports DMA. */
#define BM_STATUS_D0DMA  0x20
/** User-defined bit 1, commonly used to signal that drive 1 supports DMA. */
#define BM_STATUS_D1DMA  0x40
/** @} */

/** @name BM_CMD
 * @{
 */
/** Start the DMA operation. */
#define BM_CMD_START     0x01
/** Data transfer direction: from device to memory if set. */
#define BM_CMD_WRITE     0x08
/** @} */


/** PIIX3 Bus Master DMA unit state. */
typedef struct BMDMAState {
    /** Command register. */
    uint8_t u8Cmd;
    /** Status register. */
    uint8_t u8Status;
    /** Address of the MMIO region in the guest's memory space. */
    RTGCPHYS pvAddr;
} BMDMAState;


/** @} */


#endif /* !__PIIX3ATABmDma_h__ */
