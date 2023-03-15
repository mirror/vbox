/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_vmm_iem_armv8_h
#define VBOX_INCLUDED_vmm_iem_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

RT_C_DECLS_BEGIN

/** @addtogroup grp_iem
 * @{ */


/** The CPUMCTX_EXTRN_XXX mask required to be cleared when interpreting anything.
 * IEM will ASSUME the caller of IEM APIs has ensured these are already present. */
#define IEM_CPUMCTX_EXTRN_MUST_MASK                (  CPUMCTX_EXTRN_GPRS_MASK \
                                                    | CPUMCTX_EXTRN_PC \
                                                    | CPUMCTX_EXTRN_SPSR \
                                                    | CPUMCTX_EXTRN_ELR \
                                                    | CPUMCTX_EXTRN_SP \
                                                    | CPUMCTX_EXTRN_PSTATE )
/** The CPUMCTX_EXTRN_XXX mask needed when injecting an exception/interrupt.
 * IEM will import missing bits, callers are encouraged to make these registers
 * available prior to injection calls if fetching state anyway.  */
#define IEM_CPUMCTX_EXTRN_XCPT_MASK                (  IEM_CPUMCTX_EXTRN_MUST_MASK ) /** @todo */
/** The CPUMCTX_EXTRN_XXX mask required to be cleared when calling any
 * IEMExecDecoded API not using memory.  IEM will ASSUME the caller of IEM
 * APIs has ensured these are already present.
 * @note ASSUMES execution engine has checked for instruction breakpoints
 *       during decoding. */
#define IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK (  CPUMCTX_EXTRN_PC \
                                                    | CPUMCTX_EXTRN_PSTATE )
/** The CPUMCTX_EXTRN_XXX mask required to be cleared when calling any
 * IEMExecDecoded API using memory.  IEM will ASSUME the caller of IEM
 * APIs has ensured these are already present.
 * @note ASSUMES execution engine has checked for instruction breakpoints
 *       during decoding. */
#define IEM_CPUMCTX_EXTRN_EXEC_DECODED_MEM_MASK    (  IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK ) /** @todo */

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_iem_armv8_h */

