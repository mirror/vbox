/* $Id$ */
/** @file
 * innotek Portable Runtime UUID (unique identifiers) handling (win32 host).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_UUID
#include <Windows.h>

#include <iprt/uuid.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>


/**
 * Generates a new UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated uuid.
 */
RTDECL(int)  RTUuidCreate(PRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return VERR_INVALID_PARAMETER;
    }

    RPC_STATUS rc = UuidCreate((UUID *)pUuid);
    if ((rc == RPC_S_OK) || (rc == RPC_S_UUID_LOCAL_ONLY))
        return VINF_SUCCESS;

    /* error exit */
    return RTErrConvertFromWin32(rc);
}

/**
 * Makes null UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated null uuid.
 */
RTDECL(int)  RTUuidClear(PRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return VERR_INVALID_PARAMETER;
    }

    return RTErrConvertFromWin32(UuidCreateNil((UUID *)pUuid));
}

/**
 * Checks if UUID is null.
 *
 * @returns true if UUID is null.
 * @param   pUuid           uuid to check.
 */
RTDECL(int)  RTUuidIsNull(PCRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return TRUE;
    }

    RPC_STATUS status;
    return UuidIsNil((UUID *)pUuid, &status);
}

/**
 * Compares two UUID values.
 *
 * @returns 0 if eq, < 0 or > 0.
 * @param   pUuid1          First value to compare.
 * @param   pUuid2          Second value to compare.
 */
RTDECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2)
{
    /* check params */
    if ((pUuid1 == NULL) || (pUuid2 == NULL))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return 1;
    }

    RPC_STATUS status;
    return UuidCompare((UUID *)pUuid1, (UUID *)pUuid2, &status);
}

/**
 * Converts binary UUID to its string representation.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 * @param   pszString       Where to store result string.
 * @param   cchString       pszString buffer length, must be >= RTUUID_STR_LENGTH.
 */
RTDECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, unsigned cchString)
{
    /* check params */
    if ((pUuid == NULL) || (pszString == NULL) || (cchString < RTUUID_STR_LENGTH))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return VERR_INVALID_PARAMETER;
    }

    RPC_STATUS rc;
    char *pStr = NULL;
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    rc = UuidToStringA((UUID *)pUuid, (unsigned char **)&pStr);
#else
    rc = UuidToString((UUID *)pUuid, (unsigned char **)&pStr);
#endif
    if (rc != RPC_S_OK)
        return RTErrConvertFromWin32(rc);

    if (strlen(pStr) >= cchString)
    {
        /* out of buffer space */
#ifdef RPC_UNICODE_SUPPORTED
        /* always use ASCII version! */
        RpcStringFreeA((unsigned char **)&pStr);
#else
        RpcStringFree((unsigned char **)&pStr);
#endif
        AssertMsgFailed(("Buffer overflow\n"));
        return ERROR_BUFFER_OVERFLOW;
    }

    /* copy str to user buffer */
    pszString[0] = '\0';
    strncat(pszString, pStr, cchString);

    /* free buffer */
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    RpcStringFreeA((unsigned char **)&pStr);
#else
    RpcStringFree((unsigned char **)&pStr);
#endif

    /* all done */
    return VINF_SUCCESS;
}

/**
 * Converts UUID from its string representation to binary format.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store result Uuid.
 * @param   pszString       String with UUID text data.
 */
RTDECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString)
{
    /* check params */
    if ((pUuid == NULL) || (pszString == NULL))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return VERR_INVALID_PARAMETER;
    }

    RPC_STATUS rc;
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    rc = UuidFromStringA((unsigned char *)pszString, (UUID *)pUuid);
#else
    rc = UuidFromString((unsigned char *)pszString, (UUID *)pUuid);
#endif

    return RTErrConvertFromWin32(rc);
}

