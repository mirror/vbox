/** @file
 *
 * InnoTek Portable Runtime - Universal Unique Identifiers (UUID).
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __iprt_uuid_h__
#define __iprt_uuid_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

#ifdef IN_RING0
# error "There are no RTUuid APIs available Ring-0 Host Context!"
#endif

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
RTR3DECL(int)  RTUuidCreate(PRTUUID pUuid);

/**
 * Makes null UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated null uuid.
 */
RTR3DECL(int)  RTUuidClear(PRTUUID pUuid);

/**
 * Checks if UUID is null.
 *
 * @returns true if UUID is null.
 * @param   pUuid           uuid to check.
 */
RTR3DECL(int)  RTUuidIsNull(PCRTUUID pUuid);

/**
 * Compares two UUID values.
 *
 * @returns 0 if eq, < 0 or > 0.
 * @param   pUuid1          First value to compare.
 * @param   pUuid2          Second value to compare.
 */
RTR3DECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2);

/**
 * Converts binary UUID to its string representation.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 * @param   pszString       Where to store result string.
 * @param   cchString       pszString buffer length, must be >= RTUUID_STR_LENGTH.
 */
RTR3DECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, unsigned cchString);

/**
 * Converts UUID from its string representation to binary format.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store result Uuid.
 * @param   pszString       String with UUID text data.
 */
RTR3DECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString);

/** @} */

__END_DECLS

#endif

