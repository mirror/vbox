/** @file
* IPRT - defines the function to create ipc pipe name.
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
* @retval  VINF_SUCCESS and *pcbDest if pipe name created successfully.
* @retval  VERR_INVALID_PARAMETER in case of invalid parameter value.
* @retval  VERR_BUFFER_OVERFLOW if the pszDest is too small.
*
* @param   pszPrefix        Pipe name prefix, for example 'VBoxTrayIPC-'
* @param   pszUserName      User name
* @param   pszDest          Destination buffer to store created pipe name
* @param   pcbDest          IN  - size of destination buffer in bytes,
*                           OUT - length of created pipe name in bytes without trailing zero
*/
RTDECL(int) RTLocalIpcMakeNameUniqueUser(const char* pszPrefix, const char* pszUserName, char* pszDest, size_t* pcbDest)
{
    AssertPtrReturn(pszPrefix, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszUserName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);
    AssertReturn(*pcbDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDest, VERR_INVALID_PARAMETER);

    const size_t cbPrefixSize = RTStrNLen(pszPrefix, RTSTR_MAX);
    AssertReturn(*pcbDest > cbPrefixSize + 17, VERR_BUFFER_OVERFLOW);

    const size_t cbSourceSize = RTStrNLen(pszUserName, RTSTR_MAX);
    AssertReturn(cbSourceSize, VERR_INVALID_PARAMETER);

    /* Use the crc of user name instead of user name to avoid localized pipe problem */
    uint64_t uiCrc = RTCrc64(pszUserName, cbSourceSize);
    AssertReturn(uiCrc > 0, VERR_INVALID_PARAMETER);

    RT_BZERO(pszDest, *pcbDest);
    size_t cbRes = RTStrPrintf(pszDest, *pcbDest, "%s%016RX64", pszPrefix, uiCrc);
    AssertReturn(cbRes, VERR_BUFFER_OVERFLOW);

    *pcbDest = cbRes;
    return VINF_SUCCESS;
}
