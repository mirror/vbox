/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPGaUtils.h"

#include <iprt/asm.h>
#include <iprt/assert.h>

volatile uint32_t g_fu32GaLogControl =
      GALOG_GROUP_RELEASE
#ifdef DEBUG
    | GALOG_GROUP_TEST
//    | GALOG_GROUP_PRESENT
//    | GALOG_GROUP_DXGK
//    | GALOG_GROUP_SVGA
//    | GALOG_GROUP_SVGA_FIFO
#endif
    ;

/*
 * Helpers.
 */

void *GaMemAlloc(uint32_t cbSize)
{
    return ExAllocatePoolWithTag(NonPagedPool, cbSize, 'AGBV');
}

void *GaMemAllocZero(uint32_t cbSize)
{
    void *pvMem = GaMemAlloc(cbSize);
    if (pvMem)
        memset(pvMem, 0, cbSize);
    return pvMem;
}

void GaMemFree(void *pvMem)
{
    ExFreePool(pvMem);
}

NTSTATUS GaIdAlloc(uint32_t *pu32Bits,
                   uint32_t cbBits,
                   uint32_t u32Limit,
                   uint32_t *pu32Id)
{
    /* Find the first zero bit. */
    const int32_t i32Id = ASMBitFirstClear(pu32Bits, cbBits * 8);
    if (0 <= i32Id && i32Id < (int32_t)u32Limit)
    {
        ASMBitSet(pu32Bits, i32Id);
        *pu32Id = (uint32_t)i32Id;
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_RESOURCES;
}

NTSTATUS GaIdFree(uint32_t *pu32Bits,
                  uint32_t cbBits,
                  uint32_t u32Limit,
                  uint32_t u32Id)
{
    AssertReturn(u32Limit <= cbBits * 8, STATUS_INVALID_PARAMETER);
    AssertReturn(u32Id < u32Limit, STATUS_INVALID_PARAMETER);

    /* Clear the corresponding bit. */
    ASMBitClear(pu32Bits, (int32_t)u32Id);

    return STATUS_SUCCESS;
}
