/** @file
 * DevPCNet - Private guest interface for the PCNet device.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_DevPCNet_h
#define ___VBox_DevPCNet_h

#include <iprt/types.h>

/** @defgroup grp_devpcnet  AMD PCnet-PCI II / PCnet-FAST III (Am79C970A / Am79C973) Ethernet Controller Emulation.
 * {
 */

#define PCNET_GUEST_INTERFACE_VERSION         (1)
#define PCNET_GUEST_SHARED_MEMORY_SIZE        _512K
#define PCNET_GUEST_TX_DESCRIPTOR_SIZE        16
#define PCNET_GUEST_RX_DESCRIPTOR_SIZE        16
#define PCNET_GUEST_MAX_TX_DESCRIPTORS        128
#define PCNET_GUEST_MAX_RX_DESCRIPTORS        256
#define PCNET_GUEST_NIC_BUFFER_SIZE           1536

/* 256*16 + 128*16 + 256*1536 + 128*1536 = 582KB */

/**
 * The header of the PCNet shared memory (VBox specific).
 */
#pragma pack(1) /* paranoia */
typedef struct
{
    /** The size of the shared memory that's being used.
     * (This is <= PCNET_GUEST_SHARED_MEMORY_SIZE.) */
    uint32_t cbUsed;
    /** Version (PCNET_GUEST_INTERFACE_VERSION). */
    uint32_t u32Version;
    /** Flags (See PCNET_GUEST_FLAGS_*). */
    uint32_t fFlags;
    /** Align the following members to 64 bit. */
    uint32_t u32Alignment;

    union
    {
        struct
        {
            /** The size (in bytes) of the transmit descriptor array. */
            uint32_t    cbTxDescriptors;
            /** The size (in bytes) of the receive descriptor array. */
            uint32_t    cbRxDescriptors;
            /** Offset of the transmit descriptors relative to this header. */
            uint32_t    offTxDescriptors;
            /** Offset of the receive descriptors relative to this header. */
            uint32_t    offRxDescriptors;
            /** Offset of the transmit buffers relative to this header. */
            uint32_t    offTxBuffers;
            /** Offset of the receive buffers relative to this header. */
            uint32_t    offRxBuffers;
        } V1;
    } V;

} PCNETGUESTSHAREDMEMORY;
#pragma pack()
/** Pointer to the PCNet shared memory header. */
typedef PCNETGUESTSHAREDMEMORY *PPCNETGUESTSHAREDMEMORY;
/** Const pointer to the PCNet shared memory header. */
typedef const PCNETGUESTSHAREDMEMORY *PCPCNETGUESTSHAREDMEMORY;

/** @name fFlags definitions
 * @{
 */
/** Host admits existence private PCNet interface. */
#define PCNET_GUEST_FLAGS_ADMIT_HOST        RT_BIT(0)
/** Guest admits using the private PCNet interface. */
#define PCNET_GUEST_FLAGS_ADMIT_GUEST       RT_BIT(1)
/** @} */

/** @} */

#endif

