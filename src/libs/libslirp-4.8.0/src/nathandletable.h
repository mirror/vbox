/** @file
 * libslirp: NAT Hanlde Table Singleton Wrapper
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

#ifdef _WIN32
#ifndef INCLUDED_nathandletable_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/handletable.h>
#include <iprt/mem.h>

# ifndef LOG_GROUP
#  define LOG_GROUP LOG_GROUP_DRV_NAT
#  include <VBox/log.h>
# endif

#include <winsock2.h>

#ifdef __cplusplus
extern "C"
{
#endif

PRTHANDLETABLE pNATHandleTable = NULL;

SOCKET libslirp_wrap_RTHandleTableLookup(int fd);

int libslirp_wrap_RTHandleTableAlloc(SOCKET, uint32_t *);

#ifdef __cplusplus
}
#endif

#endif
#endif