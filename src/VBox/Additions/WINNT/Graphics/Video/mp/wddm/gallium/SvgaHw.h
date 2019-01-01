/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver VMSVGA hardware access helpers.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___SvgaHw_h__
#define ___SvgaHw_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Svga.h"

#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>

DECLINLINE(RTIOPORT) SVGAPort(PVBOXWDDM_EXT_VMSVGA pSvga, uint16_t u16Offset)
{
    return pSvga->ioportBase + u16Offset;
}

DECLINLINE(void) SVGAPortWrite(PVBOXWDDM_EXT_VMSVGA pSvga, uint16_t u16Offset, uint32_t u32Value)
{
    ASMOutU32(SVGAPort(pSvga, u16Offset), u32Value);
}

DECLINLINE(uint32_t) SVGAPortRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint16_t u16Offset)
{
    const uint32_t u32Value = ASMInU32(SVGAPort(pSvga, u16Offset));
    return u32Value;
}

DECLINLINE(void) SVGARegWrite(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset, uint32_t u32Value)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

    ASMOutU32(SVGAPort(pSvga, SVGA_INDEX_PORT), u32Offset);
    ASMOutU32(SVGAPort(pSvga, SVGA_VALUE_PORT), u32Value);

    KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
}

DECLINLINE(uint32_t) SVGARegRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pSvga->HwSpinLock, &OldIrql);

    ASMOutU32(SVGAPort(pSvga, SVGA_INDEX_PORT), u32Offset);
    const uint32_t u32Value = ASMInU32(SVGAPort(pSvga, SVGA_VALUE_PORT));

    KeReleaseSpinLock(&pSvga->HwSpinLock, OldIrql);
    return u32Value;
}

DECLINLINE(volatile void *) SVGAFifoPtrFromOffset(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Offset)
{
    return (volatile uint8_t *)pSvga->pu32FIFO + u32Offset;
}

DECLINLINE(volatile void *) SVGAFifoPtrFromIndex(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index)
{
    return pSvga->pu32FIFO + u32Index;
}

DECLINLINE(uint32_t) SVGAFifoRead(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index)
{
    volatile uint32_t *pu32 = &pSvga->pu32FIFO[u32Index];
    return ASMAtomicReadU32(pu32);
}

DECLINLINE(void) SVGAFifoWrite(PVBOXWDDM_EXT_VMSVGA pSvga, uint32_t u32Index, uint32_t u32Value)
{
    volatile uint32_t *pu32 = &pSvga->pu32FIFO[u32Index];
    ASMAtomicWriteU32(pu32, u32Value);
    ASMCompilerBarrier();
}

#endif
