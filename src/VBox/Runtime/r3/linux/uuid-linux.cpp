/* $Id$ */
/** @file
 * InnoTek Portable Runtime - UUID, LINUX.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/uuid.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <uuid/uuid.h>
#include <sys/types.h>  /* for BYTE_ORDER */


/**
 * Converts the byte order of the first 32 bit UUID component and next two 16
 * bit components from BIG_ENDIAN to LITTLE_ENDIAN and vice versa.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 */

static void rtuuid_convert_byteorder(PRTUUID pUuid)
{
    uint8_t *pu8 = &pUuid->au8[0];
    uint8_t u8;
    // 32 bit component
    u8 = pu8[0];
    pu8[0] = pu8[3];
    pu8[3] = u8;
    u8 = pu8[1];
    pu8[1] = pu8[2];
    pu8[2] = u8;
    // two 16 bit components
    u8 = pu8[4];
    pu8[4] = pu8[5];
    pu8[5] = u8;
    u8 = pu8[6];
    pu8[6] = pu8[7];
    pu8[7] = u8;
}

#if BYTE_ORDER == LITTLE_ENDIAN
#define RTUUID_CONVERT_BYTEORDER(pUuid) rtuuid_convert_byteorder(pUuid)
#else
#define RTUUID_CONVERT_BYTEORDER(pUuid) do {} while (0)
#endif

/**
 * Generates a new UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated uuid.
 */
RTR3DECL(int)  RTUuidCreate(PRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return VERR_INVALID_PARAMETER;
    }

    uuid_generate(&pUuid->au8[0]);
    RTUUID_CONVERT_BYTEORDER(pUuid);

    return VINF_SUCCESS;
}

/**
 * Makes a null UUID value.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store generated null uuid.
 */
RTR3DECL(int)  RTUuidClear(PRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return VERR_INVALID_PARAMETER;
    }

    uuid_clear(&pUuid->au8[0]);

    return VINF_SUCCESS;
}

/**
 * Checks if UUID is null.
 *
 * @returns true if UUID is null.
 * @param   pUuid           uuid to check.
 */
RTR3DECL(int)  RTUuidIsNull(PCRTUUID pUuid)
{
    /* check params */
    if (pUuid == NULL)
    {
        AssertMsgFailed(("pUuid=NULL\n"));
        return true;
    }

    return uuid_is_null(&pUuid->au8[0]);
}

/**
 * Compares two UUID values.
 *
 * @returns 0 if eq, < 0 or > 0.
 * @param   pUuid1          First value to compare.
 * @param   pUuid2          Second value to compare.
 */
RTR3DECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2)
{
    /* check params */
    if ((pUuid1 == NULL) || (pUuid2 == NULL))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return 1;
    }

    return uuid_compare(pUuid1->aUuid, pUuid2->aUuid);
}

/**
 * Converts binary UUID to its string representation.
 *
 * @returns iprt status code.
 * @param   pUuid           Uuid to convert.
 * @param   pszString       Where to store result string.
 * @param   cchString       pszString buffer length, must be >= RTUUID_STR_LENGTH.
 */
RTR3DECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, unsigned cchString)
{
    /* check params */
    if ((pUuid == NULL) || (pszString == NULL) || (cchString < RTUUID_STR_LENGTH))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return VERR_INVALID_PARAMETER;
    }

    static RTUUID uuid;
    memcpy(&uuid, pUuid, sizeof(RTUUID));
    RTUUID_CONVERT_BYTEORDER(&uuid);
    uuid_unparse(uuid.aUuid, pszString);

    return VINF_SUCCESS;
}

/**
 * Converts UUID from its string representation to binary format.
 *
 * @returns iprt status code.
 * @param   pUuid           Where to store result Uuid.
 * @param   pszString       String with UUID text data.
 */
RTR3DECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString)
{
    /* check params */
    if ((pUuid == NULL) || (pszString == NULL))
    {
        AssertMsgFailed(("Invalid parameters\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (uuid_parse(pszString, &pUuid->au8[0]) == 0)
    {
        RTUUID_CONVERT_BYTEORDER(pUuid);
        return VINF_SUCCESS;
    }

    return VERR_INVALID_UUID_FORMAT;
}
