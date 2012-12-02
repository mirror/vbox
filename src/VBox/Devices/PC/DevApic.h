/* $Id$ */
/** @file
 * Advanced Programmable Interrupt Controller (APIC) Device Definitions.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * apic.c revision 1.5  @@OSETODO
 *
 *  APIC support
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ___PC_DevApic_h
#define ___PC_DevApic_h

/* APIC Local Vector Table */
#define APIC_LVT_TIMER   0
#define APIC_LVT_THERMAL 1
#define APIC_LVT_PERFORM 2
#define APIC_LVT_LINT0   3
#define APIC_LVT_LINT1   4
#define APIC_LVT_ERROR   5
#define APIC_LVT_NB      6

/* APIC delivery modes */
#define APIC_DM_FIXED   0
#define APIC_DM_LOWPRI  1
#define APIC_DM_SMI     2
#define APIC_DM_NMI     4
#define APIC_DM_INIT    5
#define APIC_DM_SIPI    6
#define APIC_DM_EXTINT  7

/* APIC destination mode */
#define APIC_DESTMODE_FLAT      0xf
#define APIC_DESTMODE_CLUSTER   0x0

#define APIC_TRIGGER_EDGE  0
#define APIC_TRIGGER_LEVEL 1

#define APIC_LVT_TIMER_PERIODIC         (1 << 17)
#define APIC_LVT_MASKED                 (1 << 16)
#define APIC_LVT_LEVEL_TRIGGER          (1 << 15)
#define APIC_LVT_REMOTE_IRR             (1 << 14)
#define APIC_INPUT_POLARITY             (1 << 13)
#define APIC_SEND_PENDING               (1 << 12)

#endif /* !___PC_DevApic_h */

