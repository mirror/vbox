/* $Id$ */
/** @file
 * Frontend shared bits - Password file and console input helpers.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Common_PasswordInput_h
#define VBOX_INCLUDED_SRC_Common_PasswordInput_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/com/com.h>
#include <VBox/com/ptr.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>


RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd);
RTEXITCODE readPasswordFromConsole(com::Utf8Str *pPassword, const char *pszPrompt, ...);
RTEXITCODE settingsPasswordFile(ComPtr<IVirtualBox> virtualBox, const char *pszFilename);

#endif /* !VBOX_INCLUDED_SRC_Common_PasswordInput_h */
