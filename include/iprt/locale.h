/** @file
 * IPRT - Locale and Related Info.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_locale_h
#define ___iprt_locale_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_time   RTLocale - Locale and Related Info
 * @ingroup grp_rt
 * @{
 */

/**
 * Returns the setlocale(LC_ALL,NULL) return value.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if not supported.
 * @param   pszName         Where to return the name.
 * @param   cbName          The size of the name buffer.
 */
RTDECL(int) RTLocaleQueryLocaleName(char *pszName, size_t cbName);


/**
 * Gets the two letter country code (ISO 3166-1 alpha-2) for the current user.
 *
 * This is not necessarily the country from the locale name, when possible the
 * source is a different setting (host specific).
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if not supported.
 * @param   pszCountryCode  Pointer buffer that's at least three bytes in size.
 *                          The country code will be returned here on success.
 */
RTDECL(int) RTLocaleQueryUserCountryCode(char pszCountryCode[3]);

/** @} */

RT_C_DECLS_END

#endif


