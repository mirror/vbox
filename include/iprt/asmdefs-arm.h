/** @file
 * IPRT - ARM Specific Assembly Macros.
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

#ifndef IPRT_INCLUDED_asmdefs_arm_h
#define IPRT_INCLUDED_asmdefs_arm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if !defined(RT_ARCH_ARM64) && !defined(RT_ARCH_ARM32)
# error "Not on ARM64 or ARM32"
#endif

/** @defgroup grp_rt_asmdefs_arm  ARM Specific ASM (Clang and gcc) Macros
 * @ingroup grp_rt_asm
 * @{
 */

/** @todo Should the selection be done by object format (ELF, MachO, ...) maybe?. */

/** Marks the beginning of a code section. */
#if defined(__clang__)
# define BEGINCODE .section __TEXT,__text,regular,pure_instructions
#elif defined(__GNUC__)
# define BEGINCODE .section .text
#else
# error "Port me!"
#endif

/** Marks the end of a code section. */
#if defined(__clang__)
# define ENDCODE
#elif defined(__GNUC__)
# define ENDCODE
#else
# error "Port me!"
#endif


/** Marks the beginning of a data section. */
#if defined(__clang__)
# define BEGINDATA .section __DATA,__data
#elif defined(__GNUC__)
# define BEGINDATA .section .data
#else
# error "Port me!"
#endif

/** Marks the end of a data section. */
#if defined(__clang__)
# define ENDDATA
#elif defined(__GNUC__)
# define ENDDATA
#else
# error "Port me!"
#endif


/** Marks the beginning of a readonly data section. */
#if defined(__clang__)
# define BEGINCONST .section __RODATA,__rodata
#elif defined(__GNUC__)
# define BEGINCONST .section .rodata
#else
# error "Port me!"
#endif

/** Marks the end of a readonly data section. */
#if defined(__clang__)
# define ENDCONST
#elif defined(__GNUC__)
# define ENDCONST
#else
# error "Port me!"
#endif


/**
 * Returns the page address of the given symbol (used with the adrp instruction primarily).
 *
 * @returns Page aligned address of the given symbol
 * @param   a_Symbol    The symbol to get the page address from.
 */
#if defined(__clang__)
# define PAGE(a_Symbol) a_Symbol ## @PAGE
#elif defined(__GNUC__)
# define PAGE(a_Symbol) a_Symbol
#else
# error "Port me!"
#endif

/**
 * Returns the offset inside the page of the given symbol.
 *
 * @returns Page offset of the given symbol inside a page.
 * @param   a_Symbol    The symbol to get the page offset from.
 */
#if defined(__clang__)
# define PAGEOFF(a_Symbol) a_Symbol ## @PAGEOFF
#elif defined(__GNUC__)
# define PAGEOFF(a_Symbol) :lo12: ## a_Symbol
#else
# error "Port me!"
#endif


/** @} */
#endif /* !IPRT_INCLUDED_asmdefs_arm_h */

