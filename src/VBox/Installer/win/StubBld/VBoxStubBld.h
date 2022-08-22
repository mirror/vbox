/* $Id$ */
/** @file
 * VBoxStubBld - VirtualBox's Windows installer stub builder.
 */

/*
 * Copyright (C) 2009-2022 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_SRC_StubBld_VBoxStubBld_h
#define VBOX_INCLUDED_SRC_StubBld_VBoxStubBld_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define VBOXSTUB_MAX_PACKAGES 128

typedef struct VBOXSTUBPKGHEADER
{
    /** Some magic string not defined by this header? Turns out it's a write only
     *  field... */
    char    szMagic[9];
    /* Inbetween szMagic and dwVersion there are 3 bytes of implicit padding. */
    /** Some version number not defined by this header? Also write only field.
     *  Should be a uint32_t, not DWORD. */
    DWORD   dwVersion;
    /** Number of packages following the header. byte is prefixed 'b', not 'by'!
     *  Use uint8_t instead of BYTE. */
    BYTE    byCntPkgs;
    /* There are 3 bytes of implicit padding here. */
} VBOXSTUBPKGHEADER;
typedef VBOXSTUBPKGHEADER *PVBOXSTUBPKGHEADER;

typedef enum VBOXSTUBPKGARCH
{
    VBOXSTUBPKGARCH_ALL = 0,
    VBOXSTUBPKGARCH_X86,
    VBOXSTUBPKGARCH_AMD64
} VBOXSTUBPKGARCH;

typedef struct VBOXSTUBPKG
{
    BYTE byArch;
    /** Probably the name of the PE resource or something, read the source to
     *  find out for sure.  Don't use _MAX_PATH, define your own max lengths! */
    char szResourceName[_MAX_PATH];
    char szFileName[_MAX_PATH];
} VBOXSTUBPKG;
typedef VBOXSTUBPKG *PVBOXSTUBPKG;

/* Only for construction. */
/* Since it's only used by VBoxStubBld.cpp, why not just keep it there? */

typedef struct VBOXSTUBBUILDPKG
{
    char szSourcePath[_MAX_PATH];
    BYTE byArch;
} VBOXSTUBBUILDPKG;
typedef VBOXSTUBBUILDPKG *PVBOXSTUBBUILDPKG;

#endif /* !VBOX_INCLUDED_SRC_StubBld_VBoxStubBld_h */
