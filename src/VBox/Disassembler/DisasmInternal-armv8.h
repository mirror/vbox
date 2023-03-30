/* $Id$ */
/** @file
 * VBox disassembler - Internal header.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef VBOX_INCLUDED_SRC_DisasmInternal_armv8_h
#define VBOX_INCLUDED_SRC_DisasmInternal_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/dis.h>
#include <VBox/log.h>

#include <iprt/param.h>
#include "DisasmInternal.h"


/** @addtogroup grp_dis_int Internals.
 * @ingroup grp_dis
 * @{
 */

/** @name Index into g_apfnFullDisasm.
 * @{ */
enum IDX_Parse
{
  IDX_ParseNop = 0,
  IDX_ParseMax
};
AssertCompile(IDX_ParseMax < 64 /* Packed DISOPCODE assumption. */);
/** @}  */

/** @} */
#endif /* !VBOX_INCLUDED_SRC_DisasmInternal_armv8_h */

