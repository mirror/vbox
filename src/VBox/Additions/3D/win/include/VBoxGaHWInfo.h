/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxGaHWInfo_h__
#define ___VBoxGaHWInfo_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>

#include <VBoxGaHwSVGA.h>

/* Gallium virtual hardware supported by the miniport. */
#define VBOX_GA_HW_TYPE_UNKNOWN 0
#define VBOX_GA_HW_TYPE_VMSVGA  1

/*
 * VBOXGAHWINFO contains information about the virtual hardware, which is passed
 * to the user mode Gallium driver. The driver can not query the info at the initialization time,
 * therefore we send the complete info to the driver.
 *
 * VBOXGAHWINFO struct goes both to 32 and 64 bit user mode binaries, take care of alignment.
 */
#pragma pack(1)
typedef struct VBOXGAHWINFO
{
    uint32_t u32HwType; /* VBOX_GA_HW_TYPE_* */
    uint32_t u32Reserved;
    union
    {
        VBOXGAHWINFOSVGA svga;
        uint8_t au8Raw[65536];
    } u;
} VBOXGAHWINFO;
#pragma pack()

AssertCompile(RT_SIZEOFMEMB(VBOXGAHWINFO, u) <= RT_SIZEOFMEMB(VBOXGAHWINFO, u.au8Raw));

#endif
