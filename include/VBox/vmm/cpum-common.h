/** @file
 * CPUM - CPU Monitor(/ Manager).
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_vmm_cpum_common_h
#define VBOX_INCLUDED_vmm_cpum_common_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>
#include <VBox/vmm/vmapi.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_cpum      The CPU Monitor / Manager API
 * @ingroup grp_vmm
 * @{
 */

#ifndef VBOX_FOR_DTRACE_LIB

/** @name Externalized State Helpers.
 * @{ */
/** @def CPUM_ASSERT_NOT_EXTRN
 * Macro for asserting that @a a_fNotExtrn are present.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fNotExtrn     Mask of CPUMCTX_EXTRN_XXX bits to check.
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_ASSERT_NOT_EXTRN(a_pVCpu, a_fNotExtrn) \
    AssertMsg(!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fNotExtrn)), \
              ("%#RX64; a_fNotExtrn=%#RX64\n", (a_pVCpu)->cpum.GstCtx.fExtrn, (a_fNotExtrn)))

/** @def CPUMCTX_ASSERT_NOT_EXTRN
 * Macro for asserting that @a a_fNotExtrn are present in @a a_pCtx.
 *
 * @param   a_pCtx          The CPU context of the calling EMT.
 * @param   a_fNotExtrn     Mask of CPUMCTX_EXTRN_XXX bits to check.
 */
#define CPUMCTX_ASSERT_NOT_EXTRN(a_pCtx, a_fNotExtrn) \
    AssertMsg(!((a_pCtx)->fExtrn & (a_fNotExtrn)), \
              ("%#RX64; a_fNotExtrn=%#RX64\n", (a_pCtx)->fExtrn, (a_fNotExtrn)))

/** @def CPUM_IMPORT_EXTRN_RET
 * Macro for making sure the state specified by @a fExtrnImport is present,
 * calling CPUMImportGuestStateOnDemand() to get it if necessary.
 *
 * Will return if CPUMImportGuestStateOnDemand() fails.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fExtrnImport  Mask of CPUMCTX_EXTRN_XXX bits to get.
 * @thread  EMT(a_pVCpu)
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_IMPORT_EXTRN_RET(a_pVCpu, a_fExtrnImport) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* already present, consider this likely */ } \
        else \
        { \
            int rcCpumImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertRCReturn(rcCpumImport, rcCpumImport); \
        } \
    } while (0)

/** @def CPUM_IMPORT_EXTRN_RCSTRICT
 * Macro for making sure the state specified by @a fExtrnImport is present,
 * calling CPUMImportGuestStateOnDemand() to get it if necessary.
 *
 * Will update a_rcStrict if CPUMImportGuestStateOnDemand() fails.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fExtrnImport  Mask of CPUMCTX_EXTRN_XXX bits to get.
 * @param   a_rcStrict      Strict status code variable to update on failure.
 * @thread  EMT(a_pVCpu)
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_IMPORT_EXTRN_RCSTRICT(a_pVCpu, a_fExtrnImport, a_rcStrict) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* already present, consider this likely */ } \
        else \
        { \
            int rcCpumImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertStmt(RT_SUCCESS(rcCpumImport) || RT_FAILURE_NP(a_rcStrict), a_rcStrict = rcCpumImport); \
        } \
    } while (0)

VMM_INT_DECL(int) CPUMImportGuestStateOnDemand(PVMCPUCC pVCpu, uint64_t fExtrnImport);
/** @} */

#endif /* !VBOX_FOR_DTRACE_LIB */
/** @} */
RT_C_DECLS_END


#endif /* !VBOX_INCLUDED_vmm_cpum_common_h */

