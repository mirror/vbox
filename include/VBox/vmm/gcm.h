/** @file
 * GCM - Guest Compatibility Manager.
 */

/*
 * Copyright (C) 2022-2023 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_vmm_gcm_h
#define VBOX_INCLUDED_vmm_gcm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_gcm   The Guest Compatibility Manager API
 * @ingroup grp_vmm
 * @{
 */

#ifdef IN_RING3
/** @defgroup grp_gcm_r3  The GCM Host Context Ring-3 API
 * @{
 */
VMMR3_INT_DECL(int)         GCMR3Init(PVM pVM);
VMMR3_INT_DECL(int)         GCMR3Term(PVM pVM);
VMMR3_INT_DECL(void)        GCMR3Reset(PVM pVM);
/** @} */
#endif /* IN_RING3 */

VMM_INT_DECL(bool)          GCMIsInterceptingXcptDE(PVMCPUCC pVCpu);
VMM_INT_DECL(int)           GCMXcptDE(PVMCPUCC pVCpu, PCPUMCTX pCtx);
/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_gcm_h */

