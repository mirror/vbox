/** @file
 * libslirp: NAT Hanlde Table Wrapper
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

#ifdef VBOX
#ifdef _WIN32
#include "nathandletable.h"

SOCKET libslirp_wrap_RTHandleTableLookup(int fd)
{
    if (pNATHandleTable == NULL)
    {
        pNATHandleTable = RTMemAlloc(sizeof(RTHANDLETABLE));
        int rc = RTHandleTableCreate(pNATHandleTable);
        AssertRC(rc);
    }

    LogFlowFunc(("Looking up %d on %p\n", fd, (void *)pNATHandleTable));
    SOCKET s = (SOCKET)RTHandleTableLookup(*pNATHandleTable, fd);
    return s;
}

int libslirp_wrap_RTHandleTableAlloc(SOCKET uSocket, uint32_t *pHandle)
{
    if (pNATHandleTable == NULL)
    {
        pNATHandleTable = RTMemAlloc(sizeof(RTHANDLETABLE));
        int rc = RTHandleTableCreate(pNATHandleTable);
        AssertRC(rc);
    }

    LogFlowFunc(("Creating sock %llu on %p\n", uSocket, (void *)pNATHandleTable));
    return RTHandleTableAlloc(*pNATHandleTable, (void *)uSocket, pHandle);
}

#endif
#endif