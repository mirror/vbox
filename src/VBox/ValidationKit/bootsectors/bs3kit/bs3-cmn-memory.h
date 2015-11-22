/* $Id$ */
/** @file
 * BS3Kit - Internal Memory Structures, Variables and Functions.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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
 */

#ifndef ___bs3_cmn_memory_h
#define ___bs3_cmn_memory_h

#include "bs3kit.h"
#include <iprt/asm.h>

RT_C_DECLS_BEGIN;


typedef union BS3SLABCTLLOW
{
    BS3SLABCTL  Core;
    uint32_t    au32Alloc[(sizeof(BS3SLABCTL) + (0xA0000 / _4K / 8) ) / 4];
} BS3SLABCTLLOW;
extern BS3SLABCTLLOW            BS3_DATA_NM(g_Bs3Mem4KLow);


typedef union BS3SLABCTLUPPERTILED
{
    BS3SLABCTL  Core;
    uint32_t    au32Alloc[(sizeof(BS3SLABCTL) + ((BS3_SEL_TILED_AREA_SIZE - _1M) / _4K / 8) ) / 4];
} BS3SLABCTLUPPERTILED;
extern BS3SLABCTLUPPERTILED     BS3_DATA_NM(g_Bs3Mem4KUpperTiled);


/** The number of chunk sizes used by the slab list arrays
 * (g_aBs3LowSlabLists, g_aBs3UpperTiledSlabLists, more?). */
#define BS3_MEM_SLAB_LIST_COUNT 6

extern uint8_t const            BS3_DATA_NM(g_aiBs3SlabListsByPowerOfTwo)[12];
extern uint16_t const           BS3_DATA_NM(g_acbBs3SlabLists)[BS3_MEM_SLAB_LIST_COUNT];
extern BS3SLABHEAD              BS3_DATA_NM(g_aBs3LowSlabLists)[BS3_MEM_SLAB_LIST_COUNT];
extern BS3SLABHEAD              BS3_DATA_NM(g_aBs3UpperTiledSlabLists)[BS3_MEM_SLAB_LIST_COUNT];
extern uint16_t const           BS3_DATA_NM(g_cbBs3SlabCtlSizesforLists)[BS3_MEM_SLAB_LIST_COUNT];


/**
 * Translates a allocation request size to a slab list index.
 *
 * @returns Slab list index if small request, UINT8_MAX if large.
 * @param   cbRequest   The number of bytes requested.
 */
DECLINLINE(uint8_t) bs3MemSizeToSlabListIndex(size_t cbRequest)
{
    if (cbRequest <= BS3_DATA_NM(g_acbBs3SlabLists)[BS3_MEM_SLAB_LIST_COUNT - 1])
    {
        unsigned idx = cbRequest ? ASMBitLastSetU16((uint16_t)(cbRequest - 1)) : 0;
        return BS3_MSC64_FIXUP_HACK(uint8_t const, BS3_DATA_NM(g_aiBs3SlabListsByPowerOfTwo))[idx];
    }
    return UINT8_MAX;
}


RT_C_DECLS_END;

#endif

