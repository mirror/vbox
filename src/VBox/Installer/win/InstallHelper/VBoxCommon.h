/* $Id$ */
/** @file
 * VBoxCommon - Misc helper routines for install helper.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_InstallHelper_VBoxCommon_h
#define VBOX_INCLUDED_SRC_InstallHelper_VBoxCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if (_MSC_VER < 1400) /* Provide _stprintf_s to VC < 8.0. */
int swprintf_s(WCHAR *buffer, size_t cbBuffer, const WCHAR *format, ...);
#endif

UINT VBoxGetMsiProp(MSIHANDLE hMsi, WCHAR *pwszName, WCHAR *pwszValue, DWORD dwSize);
int  VBoxGetMsiPropUtf8(MSIHANDLE hMsi, const char *pcszName, char **ppszValue);
UINT VBoxSetMsiProp(MSIHANDLE hMsi, WCHAR *pwszName, WCHAR *pwszValue);

#endif /* !VBOX_INCLUDED_SRC_InstallHelper_VBoxCommon_h */

