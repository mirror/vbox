/** @file
 * ARMv8 Generic Interrupt Controller Architecture v3 (GICv3) definitions.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_gic_h
#define VBOX_INCLUDED_gic_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/armv8.h>


/** @name GICD - GIC Distributor registers.
 * @{ */
/** Size of the distributor register frame. */
#define GIC_DIST_REG_FRAME_SIZE                         _64K
/** Distributor Control Register - RW. */
#define GIC_DIST_REG_CTLR_OFF                           0x0000
/** Interrupt Controller Type Register - RO. */
#define GIC_DIST_REG_TYPER_OFF                          0x0004
/** Distributor Implementer Identification Register - RO. */
#define GIC_DIST_REG_IIDR_OFF                           0x0008
/** Interrupt Controller Type Register 2 - RO. */
#define GIC_DIST_REG_TYPER2_OFF                         0x000c
/** Error Reporting Status Register (optional) - RW. */
#define GIC_DIST_RG_STATUSR_OFF                         0x0010
/** Set SPI Register - WO. */
#define GIC_DIST_REG_SETSPI_NSR_OFF                     0x0040
/** Clear SPI Register - WO. */
#define GIC_DIST_REG_CLRSPI_NSR_OFF                     0x0048
/** Set SPI, Secure Register - WO. */
#define GIC_DIST_REG_SETSPI_SR_OFF                      0x0050
/** Clear SPI, Secure Register - WO. */
#define GIC_DIST_REG_CLRSPI_SR_OFF                      0x0058

/** Interrupt Group Registers, start offset - RW. */
#define GIC_DIST_REG_IGROUPRn_OFF_START                 0x0080
/** Interrupt Group Registers, last offset - RW. */
#define GIC_DIST_REG_IGROUPRn_OFF_LAST                  0x00fc

/** Interrupt Set Enable Registers, start offset - RW. */
#define GIC_DIST_REG_ISENABLERn_OFF_START               0x0100
/** Interrupt Set Enable Registers, last offset - RW. */
#define GIC_DIST_REG_ISENABLERn_OFF_LAST                0x017c
/** Interrupt Clear Enable Registers, start offset - RW. */
#define GIC_DIST_REG_ICENABLERn_OFF_START               0x0180
/** Interrupt Clear Enable Registers, last offset - RW. */
#define GIC_DIST_REG_ICENABLERn_OFF_LAST                0x01fc

/** Interrupt Set Pending Registers, start offset - RW. */
#define GIC_DIST_REG_ISPENDRn_OFF_START                 0x0200
/** Interrupt Set Pending Registers, last offset - RW. */
#define GIC_DIST_REG_ISPENDRn_OFF_LAST                  0x027c
/** Interrupt Clear Pending Registers, start offset - RW. */
#define GIC_DIST_REG_ICPENDRn_OFF_START                 0x0280
/** Interrupt Clear Pending Registers, last offset - RW. */
#define GIC_DIST_REG_ICPENDRn_OFF_LAST                  0x02fc

/** Interrupt Set Active Registers, start offset - RW. */
#define GIC_DIST_REG_ISACTIVERn_OFF_START               0x0300
/** Interrupt Set Active Registers, last offset - RW. */
#define GIC_DIST_REG_ISACTIVERn_OFF_LAST                0x037c
/** Interrupt Clear Active Registers, start offset - RW. */
#define GIC_DIST_REG_ICACTIVERn_OFF_START               0x0380
/** Interrupt Clear Active Registers, last offset - RW. */
#define GIC_DIST_REG_ICACTIVERn_OFF_LAST                0x03fc

/** Interrupt Priority Registers, start offset - RW. */
#define GIC_DIST_REG_IPRIORITYn_OFF_START               0x0400
/** Interrupt Priority Registers, last offset - RW. */
#define GIC_DIST_REG_IPRIORITYn_OFF_LAST                0x07f8

/** Interrupt Processor Targets Registers, start offset - RO/RW. */
#define GIC_DIST_REG_ITARGETSRn_OFF_START               0x0800
/** Interrupt Processor Targets Registers, last offset - RO/RW. */
#define GIC_DIST_REG_ITARGETSRn_OFF_LAST                0x0bf8

/** Interrupt Configuration Registers, start offset - RW. */
#define GIC_DIST_REG_ICFGRn_OFF_START                   0x0c00
/** Interrupt Configuration Registers, last offset - RW. */
#define GIC_DIST_REG_ICFGRn_OFF_LAST                    0x0cfc

/** Interrupt Group Modifier Registers, start offset - RW. */
#define GIC_DIST_REG_IGRPMODRn_OFF_START                0x0d00
/** Interrupt Group Modifier Registers, last offset - RW. */
#define GIC_DIST_REG_IGRPMODRn_OFF_LAST                 0x0d7c

/** Non-secure Access Control Registers, start offset - RW. */
#define GIC_DIST_REG_NSACRn_OFF_START                   0x0e00
/** Non-secure Access Control Registers, last offset - RW. */
#define GIC_DIST_REG_NSACRn_OFF_LAST                    0x0efc

/** Software Generated Interrupt Register - RW. */
#define GIC_DIST_REG_SGIR_OFF                           0x0f00

/** SGI Clear Pending Registers, start offset - RW. */
#define GIC_DIST_REG_CPENDSGIRn_OFF_START               0x0f10
/** SGI Clear Pending Registers, last offset - RW. */
#define GIC_DIST_REG_CPENDSGIRn_OFF_LAST                0x0f1c
/** SGI Set Pending Registers, start offset - RW. */
#define GIC_DIST_REG_SPENDSGIRn_OFF_START               0x0f20
/** SGI Set Pending Registers, last offset - RW. */
#define GIC_DIST_REG_SPENDSGIRn_OFF_LAST                0x0f2c

/** Non-maskable Interrupt Registers, start offset - RW. */
#define GIC_DIST_REG_INMIn_OFF_START                    0x0f80
/** Non-maskable Interrupt Registers, last offset - RW. */
#define GIC_DIST_REG_INMIn_OFF_LAST                     0x0ffc


/** Interrupt Group Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_IGROUPRnE_OFF_START                0x1000
/** Interrupt Group Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_IGROUPRnE_OFF_LAST                 0x107c

/** Interrupt Set Enable Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ISENABLERnE_OFF_START              0x1200
/** Interrupt Set Enable Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ISENABLERnE_OFF_LAST               0x127c
/** Interrupt Clear Enable Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ICENABLERnE_OFF_START              0x1400
/** Interrupt Clear Enable Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ICENABLERnE_OFF_LAST               0x147c

/** Interrupt Set Pending Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ISPENDRnE_OFF_START                0x1600
/** Interrupt Set Pending Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ISPENDRnE_OFF_LAST                 0x167c
/** Interrupt Clear Pending Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ICPENDRnE_OFF_START                0x1800
/** Interrupt Clear Pending Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ICPENDRnE_OFF_LAST                 0x187c

/** Interrupt Set Active Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ISACTIVERnE_OFF_START              0x1a00
/** Interrupt Set Active Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ISACTIVERnE_OFF_LAST               0x1a7c
/** Interrupt Clear Active Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ICACTIVERnE_OFF_START              0x1c00
/** Interrupt Clear Active Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ICACTIVERnE_OFF_LAST               0x1c7c

/** Interrupt Priority Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_IPRIORITYnE_OFF_START              0x2000
/** Interrupt Priority Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_IPRIORITYnE_OFF_LAST               0x23fc

/** Interrupt Configuration Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_ICFGRnE_OFF_START                  0x3000
/** Interrupt Configuration Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_ICFGRnE_OFF_LAST                   0x30fc

/** Interrupt Group Modifier Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_IGRPMODRnE_OFF_START               0x3400
/** Interrupt Group Modifier Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_IGRPMODRnE_OFF_LAST                0x347c

/** Non-secure Access Control Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_NSACRnE_OFF_START                  0x3600
/** Non-secure Access Control Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_NSACRnE_OFF_LAST                   0x367c

/** Non-maskable Interrupt Registers for extended SPIs, start offset - RW. */
#define GIC_DIST_REG_INMInE_OFF_START                   0x3b00
/** Non-maskable Interrupt Registers for extended SPIs, last offset - RW. */
#define GIC_DIST_REG_INMInE_OFF_LAST                    0x3b7c

/** Interrupt Routing Registers, start offset - RW. */
#define GIC_DIST_REG_IROUTERn_OFF_START                 0x6100
/** Interrupt Routing Registers, last offset - RW. */
#define GIC_DIST_REG_IROUTERn_OFF_LAST                  0x7fd8
/** Interrupt Routing Registers for extended SPI range, start offset - RW. */
#define GIC_DIST_REG_IROUTERnE_OFF_START                0x8000
/** Interrupt Routing Registers for extended SPI range, last offset - RW. */
#define GIC_DIST_REG_IROUTERnE_OFF_LAST                 0x9ffc

/** Distributor Peripheral ID2 Register - RO. */
#define GIC_DIST_REG_PIDR2_OFF                          0xffe8
/** @} */


/** @name GICD - GIC Redistributor registers.
 * @{ */
/** Size of the redistributor register frame. */
#define GIC_REDIST_REG_FRAME_SIZE                       _64K
/** Redistributor Control Register - RW. */
#define GIC_REDIST_REG_CTLR_OFF                         0x0000
/** Implementer Identification Register - RO. */
#define GIC_REDIST_REG_IIDR_OFF                         0x0004
/** Redistributor Type Register - RO. */
#define GIC_REDIST_REG_TYPER_OFF                        0x0008
/** Redistributor Error Reporting Status Register (optional) - RW. */
#define GIC_REDIST_REG_STATUSR_OFF                      0x0010
/** Redistributor Wake Register - RW. */
#define GIC_REDIST_REG_WAKER_OFF                        0x0014
/** Redistributor Report maximum PARTID and PMG Register - RO. */
#define GIC_REDIST_REG_MPAMIDR_OFF                      0x0018
/** Redistributor Set PARTID and PMG Register - RW. */
#define GIC_REDIST_REG_PARTIDR_OFF                      0x001c
/** Redistributor Set LPI Pending Register - WO. */
#define GIC_REDIST_REG_SETLPIR_OFF                      0x0040
/** Redistributor Clear LPI Pending Register - WO. */
#define GIC_REDIST_REG_CLRLPIR_OFF                      0x0048
/** Redistributor Properties Base Address Register - RW. */
#define GIC_REDIST_REG_PROPBASER_OFF                    0x0070
/** Redistributor LPI Pending Table Base Address Register - RW. */
#define GIC_REDIST_REG_PENDBASER_OFF                    0x0078
/** Redistributor Invalidate LPI Register - WO. */
#define GIC_REDIST_REG_INVLPIR_OFF                      0x00a0
/** Redistributor Invalidate All Register - WO. */
#define GIC_REDIST_REG_INVALLR_OFF                      0x00b0
/** Redistributor Synchronize Register - RO. */
#define GIC_REDIST_REG_SYNCR_OFF                        0x00c0

/** Redistributor Peripheral ID2 Register - RO. */
#define GIC_REDIST_REG_PIDR2_OFF                        0xffe8
/** @} */


/** @name GICD - GIC SGI and PPI Redistributor registers (Adjacent to the GIC Redistributor register space).
 * @{ */
/** Size of the SGI and PPI redistributor register frame. */
#define GIC_REDIST_SGI_PPI_REG_FRAME_SIZE               _64K

/** Interrupt Group Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGROUPR0_OFF             0x0080
/** Interrupt Group Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGROUPR1E_OFF            0x0084
/** Interrupt Group Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGROUPR2E_OFF            0x0084

/** Interrupt Set Enable Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISENABLER0_OFF           0x0100
/** Interrupt Set Enable Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISENABLER1E_OFF          0x0104
/** Interrupt Set Enable Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISENABLER2E_OFF          0x0108

/** Interrupt Clear Enable Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICENABLER0_OFF           0x0100
/** Interrupt Clear Enable Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICENABLER1E_OFF          0x0104
/** Interrupt Clear Enable Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICENABLER2E_OFF          0x0108

/** Interrupt Set Pend Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISPENDR0_OFF             0x0200
/** Interrupt Set Pend Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISPENDR1E_OFF            0x0204
/** Interrupt Set Pend Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISPENDR2E_OFF            0x0208

/** Interrupt Clear Pend Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICPENDR0_OFF             0x0280
/** Interrupt Clear Pend Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICPENDR1E_OFF            0x0284
/** Interrupt Clear Pend Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICPENDR2E_OFF            0x0288

/** Interrupt Set Active Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISACTIVER0_OFF           0x0300
/** Interrupt Set Active Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISACTIVER1E_OFF          0x0304
/** Interrupt Set Active Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ISACTIVER2E_OFF          0x0308

/** Interrupt Clear Active Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICACTIVER0_OFF           0x0380
/** Interrupt Clear Active Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICACTIVER1E_OFF          0x0384
/** Interrupt Clear Active Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICACTIVER2E_OFF          0x0388

/** Interrupt Priority Registers, start offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_START     0x0400
/** Interrupt Priority Registers, last offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_IPRIORITYn_OFF_LAST      0x041c
/** Interrupt Priority Registers for extended PPI range, start offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_IPRIORITYnE_OFF_START    0x0420
/** Interrupt Priority Registers for extended PPI range, last offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_IPRIORITYnE_OFF_LAST     0x045c

/** SGI Configuration Register - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICFGR0_OFF               0x0c00
/** PPI Configuration Register - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICFGR1_OFF               0x0c04
/** Extended PPI Configuration Register, start offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICFGRnE_OFF_START        0x0c08
/** Extended PPI Configuration Register, last offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_ICFGRnE_OFF_LAST         0x0c14

/** Interrupt Group Modifier Register 0 - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGRPMODR0_OFF            0x0d00
/** Interrupt Group Modifier Register 1 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGRPMODR1E_OFF           0x0d04
/** Interrupt Group Modifier Register 2 for extended PPI range - RW. */
#define GIC_REDIST_SGI_PPI_REG_IGRPMODR2E_OFF           0x0d08

/** Non Secure Access Control Register - RW. */
#define GIC_REDIST_SGI_PPI_REG_NSACR_OFF                0x0e00

/** Non maskable Interrupt Register for PPIs - RW. */
#define GIC_REDIST_SGI_PPI_REG_INMIR0_OFF               0x0f80
/** Non maskable Interrupt Register for Extended PPIs, start offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_INMIRnE_OFF_START        0x0f84
/** Non maskable Interrupt Register for Extended PPIs, last offset - RW. */
#define GIC_REDIST_SGI_PPI_REG_INMIRnE_OFF_LAST         0x0ffc
/** @} */


#endif /* !VBOX_INCLUDED_gic_h */

