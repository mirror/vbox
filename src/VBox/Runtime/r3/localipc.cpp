/* $Id$ */
/** @file
 * IPRT - Implements RTLocalIpcMakeNameUniqueUser
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
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

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/localipc.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/crc.h>

/**
 * Make the IPC pipe name unique for user
 * in a form like 'VBoxTrayIPC-6a4500cb7c726949'
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS and *pcbDst if pipe name created successfully.
 * @retval  VERR_INVALID_PARAMETER in case of invalid parameter value.
 * @retval  VERR_BUFFER_OVERFLOW if the pszDst is too small.
 *
 * @param   pszPrefix        Pipe name prefix, for example 'VBoxTrayIPC-'
 * @param   pszUsername      Username.
 * @param   pszDst           Destination buffer to store created pipe name
 * @param   pcbDst           IN  - size of destination buffer in bytes,
 *                           OUT - length of created pipe name in bytes without trailing zero
 *
 * @todo    r=bird: 'Unique' is a misnomer here.  Doing CRC64 on a string does
 *          not guarentee uniqueness, not even using cryptographic hashing will
 *          achive this.  There can be two @a pszUsername strings with the same
 *          CRC64 value, while still being different.
 *
 *          Rename the function to something like RTLocalIpcMakeAsciiName and
 *          use a safe escape-like recoding of it.
 *
 *          Also, pcbDst should be two parameters.
 */
RTDECL(int) RTLocalIpcMakeNameUniqueUser(const char *pszPrefix, const char *pszUsername, char *pszDst, size_t *pcbDst)
{
    AssertPtrReturn(pszPrefix, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszUsername, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDst, VERR_INVALID_PARAMETER);
    AssertReturn(*pcbDst, VERR_BUFFER_OVERFLOW);
    AssertPtrReturn(pszDst, VERR_INVALID_PARAMETER);

    const size_t cchPrefix = strlen(pszPrefix);
    AssertReturn(*pcbDst > cchPrefix + 17, VERR_BUFFER_OVERFLOW);

    const size_t cchSrc = strlen(pszUsername);
    AssertReturn(cchSrc, VERR_INVALID_PARAMETER);

    /* Use the crc of user name instead of user name to avoid localized pipe problem */
    uint64_t uCrc = RTCrc64(pszUsername, cchSrc);
    AssertReturn(uCrc > 0, VERR_INVALID_PARAMETER);

    RT_BZERO(pszDst, *pcbDst);
    size_t cbRes = RTStrPrintf(pszDst, *pcbDst, "%s%016RX64", pszPrefix, uCrc);
    AssertReturn(cbRes, VERR_BUFFER_OVERFLOW); /** @todo r=bird: WRONG. RTStrPrintf is a very very stupid API which is almost impossible to check for overflows */

    *pcbDst = cbRes;
    return VINF_SUCCESS;
}
