/* $Id$ */
/** @file
 * VBoxStubBld - VirtualBox's Windows installer stub builder.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#pragma once

#define VBOXSTUB_MAX_PACKAGES 128

typedef struct
{
    char szMagic[9];
    DWORD dwVersion;
    BYTE byCntPkgs;

} VBOXSTUBPKGHEADER, *PVBOXSTUBPKGHEADER;

enum VBOXSTUBPKGARCH
{
    VBOXSTUBPKGARCH_ALL = 0,
    VBOXSTUBPKGARCH_X86 = 1,
    VBOXSTUBPKGARCH_AMD64 = 2
};

typedef struct
{
    BYTE byArch;
    char szResourceName[_MAX_PATH];
    char szFileName[_MAX_PATH];
} VBOXSTUBPKG, *PVBOXSTUBPKG;

/* Only for construction. */

typedef struct
{
    char szSourcePath[_MAX_PATH];
    BYTE byArch;
} VBOXSTUBBUILDPKG, *PVBOXSTUBBUILDPKG;

