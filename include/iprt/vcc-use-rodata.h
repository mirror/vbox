/** @file
 * IPRT - Visual C++ .rdata segment override to .rodata.
 *
 * This is a trick employed when building code that needs to run on NT 3.1 where
 * the IAT needs to be writable, but we wish to keep as little as possible other
 * data writable.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_vcc_use_rodata_h
#define IPRT_INCLUDED_vcc_use_rodata_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Switch the const data section from .rdata to .rodata. */
#pragma const_seg(".rodata")
/* Try merge as many other readonly segments into that section, so we leave as
   little as possible in the .rdata section.  Unfortunately, it doesn't seem to
   be possible to move ___safe_se_handler_table, ___volatile_metadata and the
   PDB filename. */
#pragma comment(linker, "/MERGE:.gfids=.rodata /MERGE:.xdata=.rodata /MERGE:.edata=.rodata /MERGE:.pdata=.rodata")

/* This is for other files issuing /MERGE directives to the linker involving .rdata. */
#define IPRT_VCC_USING_RODATA_AS_CONST_SEG

#endif /* !IPRT_INCLUDED_vcc_use_rodata_h */

