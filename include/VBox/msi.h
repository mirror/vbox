/** @file
 * MSI - Message signalled interrupts support.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef ___VBox_msi_h
#define ___VBox_msi_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/assert.h>

#include <VBox/pci.h>

/* Constants for Intel APIC MSI messages */
#define VBOX_MSI_DATA_VECTOR_SHIFT           0
#define VBOX_MSI_DATA_VECTOR_MASK            0x000000ff
#define VBOX_MSI_DATA_VECTOR(v)              (((v) << VBOX_MSI_DATA_VECTOR_SHIFT) & \
                                                  VBOX_MSI_DATA_VECTOR_MASK)
#define VBOX_MSI_DATA_DELIVERY_MODE_SHIFT    8
#define  VBOX_MSI_DATA_DELIVERY_FIXED        (0 << VBOX_MSI_DATA_DELIVERY_MODE_SHIFT)
#define  VBOX_MSI_DATA_DELIVERY_LOWPRI       (1 << VBOX_MSI_DATA_DELIVERY_MODE_SHIFT)

#define VBOX_MSI_DATA_LEVEL_SHIFT            14
#define  VBOX_MSI_DATA_LEVEL_DEASSERT        (0 << VBOX_MSI_DATA_LEVEL_SHIFT)
#define  VBOX_MSI_DATA_LEVEL_ASSERT          (1 << VBOX_MSI_DATA_LEVEL_SHIFT)

#define VBOX_MSI_DATA_TRIGGER_SHIFT          15
#define  VBOX_MSI_DATA_TRIGGER_EDGE          (0 << VBOX_MSI_DATA_TRIGGER_SHIFT)
#define  VBOX_MSI_DATA_TRIGGER_LEVEL         (1 << VBOX_MSI_DATA_TRIGGER_SHIFT)

/**
 * MSI region, actually same as LAPIC MMIO region, but listens on bus,
 * not CPU, accesses. 
 * Works for us (as we don't differentiate who originated access)
 * as MSI registers are reserved in LAPIC, and vice versa, so we 
 * can just use same handler.
 */
#define VBOX_MSI_ADDR_BASE                   0xfee00000
#define VBOX_MSI_ADDR_SIZE                   0x100000

#define VBOX_MSI_ADDR_DEST_MODE_SHIFT        2
#define  VBOX_MSI_ADDR_DEST_MODE_PHYSICAL    (0 << VBOX_MSI_ADDR_DEST_MODE_SHIFT)
#define  VBOX_MSI_ADDR_DEST_MODE_LOGICAL     (1 << VBOX_MSI_ADDR_DEST_MODE_SHIFT)

#define VBOX_MSI_ADDR_REDIRECTION_SHIFT      3
#define  VBOX_MSI_ADDR_REDIRECTION_CPU       (0 << VBOX_MSI_ADDR_REDIRECTION_SHIFT)
                                        /* dedicated cpu */
#define  VBOX_MSI_ADDR_REDIRECTION_LOWPRI    (1 << VBOX_MSI_ADDR_REDIRECTION_SHIFT)
                                        /* lowest priority */

#define VBOX_MSI_ADDR_DEST_ID_SHIFT          12
#define  VBOX_MSI_ADDR_DEST_ID_MASK          0x00ffff0
#define  VBOX_MSI_ADDR_DEST_ID(dest)         (((dest) << VBOX_MSI_ADDR_DEST_ID_SHIFT) & \
                                         VBOX_MSI_ADDR_DEST_ID_MASK)
#define VBOX_MSI_ADDR_EXT_DEST_ID(dest)      ((dest) & 0xffffff00)

#define VBOX_MSI_ADDR_IR_EXT_INT             (1 << 4)
#define VBOX_MSI_ADDR_IR_SHV                 (1 << 3)
#define VBOX_MSI_ADDR_IR_INDEX1(index)       ((index & 0x8000) >> 13)
#define VBOX_MSI_ADDR_IR_INDEX2(index)       ((index & 0x7fff) << 5)

#endif
