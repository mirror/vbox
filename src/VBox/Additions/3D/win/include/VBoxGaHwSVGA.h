/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface.
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

#ifndef GA_INCLUDED_3D_WIN_VBoxGaHwSVGA_h
#define GA_INCLUDED_3D_WIN_VBoxGaHwSVGA_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

/*
 * VBOXGAHWINFOSVGA contains information about the virtual hardware, which is neede dy the user mode
 * Gallium driver. The driver can not query the info at the initialization time, therefore
 * we send the complete info to the driver.
 *
 * Since both FIFO and SVGA_REG_ are expanded over time, we reserve some space.
 * The Gallium user mode driver can figure out which part of au32Regs and au32Fifo
 * is valid from the raw data.
 *
 * VBOXGAHWINFOSVGA struct goes both to 32 and 64 bit user mode binaries, take care of alignment.
 */
#pragma pack(1)
typedef struct VBOXGAHWINFOSVGA
{
    uint32_t cbInfoSVGA;

    /* Copy of SVGA_REG_*, up to 256, currently 58 are used. */
    uint32_t au32Regs[256];

    /* Copy of FIFO registers, up to 1024, currently 290 are used. */
    uint32_t au32Fifo[1024];

    /* Currently SVGA has 244 caps, 512 should be ok for near future.
     * This is a copy of SVGA_REG_DEV_CAP enumeration.
     * Only valid if SVGA_CAP_GBOBJECTS is set in SVGA_REG_CAPABILITIES.
     */
    uint32_t au32Caps[512];
} VBOXGAHWINFOSVGA;
#pragma pack()

#endif /* !GA_INCLUDED_3D_WIN_VBoxGaHwSVGA_h */
