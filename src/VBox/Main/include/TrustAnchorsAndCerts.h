/* $Id$ */
/** @file
 * Main - Collection of trust anchors and certificates included in VBoxSVC.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_TrustAnchorsAndCerts_h
#define MAIN_INCLUDED_TrustAnchorsAndCerts_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN

extern const unsigned char g_abUefiMicrosoftKek[];
extern const unsigned g_cbUefiMicrosoftKek;

extern const unsigned char g_abUefiMicrosoftCa[];
extern const unsigned g_cbUefiMicrosoftCa;

extern const unsigned char g_abUefiMicrosoftProPca[];
extern const unsigned g_cbUefiMicrosoftProPca;

extern const unsigned char g_abUefiOracleDefPk[];
extern const unsigned g_cbUefiOracleDefPk;

RT_C_DECLS_END

#endif /* !MAIN_INCLUDED_TrustAnchorsAndCerts_h */

