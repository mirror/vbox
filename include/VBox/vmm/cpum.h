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

#ifndef VBOX_INCLUDED_vmm_cpum_h
#define VBOX_INCLUDED_vmm_cpum_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/cpum-common.h>

#ifndef VBOX_VMM_TARGET_ARMV8
# include <VBox/vmm/cpum-x86-amd64.h>
#else
# include <VBox/vmm/cpum-armv8.h>
#endif

RT_C_DECLS_BEGIN

/** @defgroup grp_cpum      The CPU Monitor / Manager API
 * @ingroup grp_vmm
 * @{
 */

#ifndef VBOX_FOR_DTRACE_LIB

VMMDECL(PCPUMCTX)       CPUMQueryGuestCtxPtr(PVMCPU pVCpu);
VMMDECL(CPUMMODE)       CPUMGetGuestMode(PVMCPU pVCpu);

/** @name Guest Register Getters.
 * @{ */
VMMDECL(uint64_t)       CPUMGetGuestFlatPC(PVMCPU pVCpu);
VMMDECL(uint64_t)       CPUMGetGuestFlatSP(PVMCPU pVCpu);
VMMDECL(CPUMCPUVENDOR)  CPUMGetGuestCpuVendor(PVM pVM);
VMMDECL(CPUMMICROARCH)  CPUMGetGuestMicroarch(PCVM pVM);
VMMDECL(void)           CPUMGetGuestAddrWidths(PCVM pVM, uint8_t *pcPhysAddrWidth, uint8_t *pcLinearAddrWidth);
/** @} */

/** @name Misc Guest Predicate Functions.
 * @{  */
VMMDECL(bool)           CPUMIsGuestIn64BitCode(PVMCPU pVCpu);
/** @} */

VMMDECL(CPUMCPUVENDOR)  CPUMGetHostCpuVendor(PVM pVM);
VMMDECL(CPUMMICROARCH)  CPUMGetHostMicroarch(PCVM pVM);

#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPUM ring-3 API
 * @{
 */

VMMR3DECL(int)          CPUMR3Init(PVM pVM);
VMMR3DECL(int)          CPUMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3DECL(void)         CPUMR3LogCpuIdAndMsrFeatures(PVM pVM);
VMMR3DECL(void)         CPUMR3Relocate(PVM pVM);
VMMR3DECL(int)          CPUMR3Term(PVM pVM);
VMMR3DECL(void)         CPUMR3Reset(PVM pVM);
VMMR3DECL(void)         CPUMR3ResetCpu(PVM pVM, PVMCPU pVCpu);
VMMDECL(bool)           CPUMR3IsStateRestorePending(PVM pVM);
VMMDECL(const char *)       CPUMMicroarchName(CPUMMICROARCH enmMicroarch);
VMMR3DECL(const char *)     CPUMCpuVendorName(CPUMCPUVENDOR enmVendor);

VMMR3DECL(uint32_t)         CPUMR3DbGetEntries(void);
/** Pointer to CPUMR3DbGetEntries. */
typedef DECLCALLBACKPTR(uint32_t, PFNCPUMDBGETENTRIES, (void));
VMMR3DECL(PCCPUMDBENTRY)    CPUMR3DbGetEntryByIndex(uint32_t idxCpuDb);
/** Pointer to CPUMR3DbGetEntryByIndex. */
typedef DECLCALLBACKPTR(PCCPUMDBENTRY, PFNCPUMDBGETENTRYBYINDEX, (uint32_t idxCpuDb));
VMMR3DECL(PCCPUMDBENTRY)    CPUMR3DbGetEntryByName(const char *pszName);
/** Pointer to CPUMR3DbGetEntryByName. */
typedef DECLCALLBACKPTR(PCCPUMDBENTRY, PFNCPUMDBGETENTRYBYNAME, (const char *pszName));

VMMR3_INT_DECL(void)    CPUMR3NemActivateGuestDebugState(PVMCPUCC pVCpu);
VMMR3_INT_DECL(void)    CPUMR3NemActivateHyperDebugState(PVMCPUCC pVCpu);
/** @} */
#endif /* IN_RING3 */

#endif /* !VBOX_FOR_DTRACE_LIB */
/** @} */
RT_C_DECLS_END


#endif /* !VBOX_INCLUDED_vmm_cpum_h */

