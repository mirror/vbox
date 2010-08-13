/* $Id$ */
/** @file
 * VBoxStubBld - VirtualBox's Windows installer stub builder.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
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

