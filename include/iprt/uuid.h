/** @file
 * innotek Portable Runtime - Universal Unique Identifiers (UUID).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_uuid_h
#define ___iprt_uuid_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_uuid       RTUuid - Universally Unique Identifiers
 * @ingroup grp_rt
 * @{
 */

/**
 * Generates new UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated uuid.
 */
RTDECL(int)  RTUuidCreate(PRTUUID pUuid);

/**
 * Makes null UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated null uuid.
 */
RTDECL(int)  RTUuidClear(PRTUUID pUuid);

/**
 * Checks if UUID is null.
 *
 * @returns true if UUID is null.
 * @param   pUuid           uuid to check.
 */
RTDECL(int)  RTUuidIsNull(PCRTUUID pUuid);

/**
 * Compares two UUID values.
 *
 * @returns 0 if eq, < 0 or > 0.
 * @param   pUuid1          First value to compare.
 * @param   pUuid2          Second value to compare.
 */
RTDECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2);

/**
 * Converts binary UUID to its string representation.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 * @param   pszString       Where to store result string.
 * @param   cchString       pszString buffer length, must be >= RTUUID_STR_LENGTH.
 */
RTDECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, unsigned cchString);

/**
 * Converts UUID from its string representation to binary format.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store result Uuid.
 * @param   pszString       String with UUID text data.
 */
RTDECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString);

/** @} */

__END_DECLS

#endif

