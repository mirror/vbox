/** @file
 * CPUM - CPU Monitor(/ Manager), Context Structures.
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

#ifndef VBOX_INCLUDED_vmm_cpumctx_h
#define VBOX_INCLUDED_vmm_cpumctx_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @defgroup grp_cpum_ctx  The CPUM Context Structures
 * @ingroup grp_cpum
 * @{
 */

/** @def CPUM_UNION_NM
 * For compilers (like DTrace) that does not grok nameless unions, we have a
 * little hack to make them palatable.
 */
/** @def CPUM_STRUCT_NM
 * For compilers (like DTrace) that does not grok nameless structs (it is
 * non-standard C++), we have a little hack to make them palatable.
 */
#ifdef VBOX_FOR_DTRACE_LIB
# define CPUM_UNION_NM(a_Nm)  a_Nm
# define CPUM_STRUCT_NM(a_Nm) a_Nm
#elif defined(IPRT_WITHOUT_NAMED_UNIONS_AND_STRUCTS)
# define CPUM_UNION_NM(a_Nm)  a_Nm
# define CPUM_STRUCT_NM(a_Nm) a_Nm
#else
# define CPUM_UNION_NM(a_Nm)
# define CPUM_STRUCT_NM(a_Nm)
#endif
/** @def CPUM_UNION_STRUCT_NM
 * Combines CPUM_UNION_NM and CPUM_STRUCT_NM to avoid hitting the right side of
 * the screen in the compile time assertions.
 */
#define CPUM_UNION_STRUCT_NM(a_UnionNm, a_StructNm) CPUM_UNION_NM(a_UnionNm .) CPUM_STRUCT_NM(a_StructNm)

#ifdef VBOX_VMM_TARGET_ARMV8
# include <VBox/vmm/cpumctx-armv8.h>
#else
# include <VBox/vmm/cpumctx-x86-amd64.h>
#endif

/** @} */

RT_C_DECLS_BEGIN

/** @todo More common stuff for both x86/amd64 and arm?. */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_vmm_cpumctx_h */

