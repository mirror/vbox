/* $Id$ */
/** @file
 * innotek Portable Runtime, MZ header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___internal_ldrMZ_h
#define ___internal_ldrMZ_h

#pragma pack(1) /* not required */

#include <iprt/types.h>

typedef struct _IMAGE_DOS_HEADER
{
    uint16_t   e_magic;
    uint16_t   e_cblp;
    uint16_t   e_cp;
    uint16_t   e_crlc;
    uint16_t   e_cparhdr;
    uint16_t   e_minalloc;
    uint16_t   e_maxalloc;
    uint16_t   e_ss;
    uint16_t   e_sp;
    uint16_t   e_csum;
    uint16_t   e_ip;
    uint16_t   e_cs;
    uint16_t   e_lfarlc;
    uint16_t   e_ovno;
    uint16_t   e_res[4];
    uint16_t   e_oemid;
    uint16_t   e_oeminfo;
    uint16_t   e_res2[10];
    uint32_t   e_lfanew;
} IMAGE_DOS_HEADER;
typedef IMAGE_DOS_HEADER *PIMAGE_DOS_HEADER;

#ifndef IMAGE_DOS_SIGNATURE
# define IMAGE_DOS_SIGNATURE ('M' | ('Z' << 8)) /* fix endianness */
#endif

#pragma pack()

#endif

