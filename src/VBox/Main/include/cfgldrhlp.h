/** @file
 *
 * CFGLDRHLP - Configuration Loader Helpers
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

#ifndef __VBox_cfgldrhlp_h__
#define __VBox_cfgldrhlp_h__

#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>

#include <iprt/alloc.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/string.h>
#include <VBox/err.h>

#include <string.h>

#ifndef IN_RING3
# error "There are no configuration loader helper APIs available in Ring-0 Context!"
#else /* IN_RING3 */


//@@TODO (dmik): remove
//__BEGIN_DECLS

int cfgldrhlp_strtouint32(PRTUCS2  puszValue, uint32_t *pvalue);
int cfgldrhlp_strtouint64(PRTUCS2  puszValue, uint64_t *pvalue);
int cfgldrhlp_uint32tostr(uint32_t ulValue, char *pszValue);
int cfgldrhlp_uint64tostr(uint64_t ullValue, char *pszValue);

// converts an UCS2 string of decimal digits to an unsigned integral value.
// UT must be an unsigned integral type.
template<class UT> int cfgldrhlp_ustr_to_uinteger (PRTUCS2 puszValue, UT *pValue)
{
    if (!puszValue || !pValue)
    {
        return VERR_INVALID_POINTER;
    }
    if (!(*puszValue))
    {
        return VERR_CFG_INVALID_FORMAT;
    }

    PRTUCS2 p = puszValue;
    UT v = 0;

    int rc = VINF_SUCCESS;

    while (*p)
    {
        if( *p >= '0' && *p <= '9' )
        {
            register UT vt = v * 10 + (*p - '0');
            if (vt < v)
            {
                rc = VERR_CFG_INVALID_FORMAT;
                break;
            }
            v = vt;
        }
        else
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }
        p++;
    }

    if (VBOX_SUCCESS(rc))
    {
        *pValue = v;
    }

    return rc;
}

// converts an UCS2 string of decimal digits to a signed integral value.
// T must be a signed integral type, UT msut be a corresponding unsigned type.
template<class T, class UT> int cfgldrhlp_ustr_to_integer (PRTUCS2 puszValue, T *pValue )
{
    if (!puszValue || !pValue)
    {
        return VERR_INVALID_POINTER;
    }
    if (!(*puszValue))
    {
        return VERR_CFG_INVALID_FORMAT;
    }

    PRTUCS2 p = puszValue;
    bool negative = false;

    if (*p == '-')
    {
        negative = true;
        p++;
    }
    else if (*p == '+')
    {
        negative = false;
        p++;
    }

    UT uv;

    int rc = cfgldrhlp_ustr_to_uinteger<UT> (p, &uv);

    if (VBOX_SUCCESS(rc))
    {
        register T iv = (T) uv;
        if (iv < 0 )
        {
            if (negative && !(iv << 1))
            {
                *pValue = iv;
            }
            else
            {
                rc = VERR_CFG_INVALID_FORMAT;
            }
        }
        else
        {
            *pValue = negative ? -iv : iv;
        }
    }

    return rc;
}

// converts an unsigned integral value to an UCS2 string of decimal digits.
// UT must be an unsigned integral type. the pointer to the resulting string
// is returned via ppuszValue. if the function returns VBOX_SUCCESS
// the string must be freed by the caller using cfgldrhlp_release_ustr().
// withPlus prepends the string with a plus char.
template<class UT> int cfgldrhlp_uinteger_to_ustr (
    UT value, PRTUCS2 *ppuszValue, bool withPlus = false
)
{
    if (!ppuszValue)
    {
        return VERR_INVALID_POINTER;
    }

    // calculated for octal, which is sufficient for decimal
    const int NumDigitsPlusTwo = ((sizeof(UT) * 8) + 2) / 3 + 2;

    PRTUCS2 puszValue = (PRTUCS2) RTMemTmpAllocZ (sizeof(RTUCS2) * NumDigitsPlusTwo);
    if (!puszValue)
    {
        return VERR_NO_MEMORY;
    }

    PRTUCS2 p = puszValue + NumDigitsPlusTwo;
    *(--p) = 0; // end of string

    do {
        *(--p) = '0' + (value % 10);
        value /= 10;
    }
    while (value);

    memmove(
        puszValue + (withPlus ? 1 : 0), p,
        sizeof(RTUCS2) * (puszValue + NumDigitsPlusTwo - p)
    );

    if (withPlus)
    {
        *puszValue = '+';
    }

    *ppuszValue = puszValue;

    return VINF_SUCCESS;
}

// converts a signed integral value to an UCS2 string of decimal digits.
// T must be a signed integral type, UT msut be a corresponding unsigned type.
// the pointer to the resulting string is returned via ppuszValue.
// if the function returns VBOX_SUCCESS the string must be freed by the
// caller using cfgldrhlp_release_ustr().
template<class T, class UT> int cfgldrhlp_integer_to_ustr (T value, PRTUCS2 *ppuszValue)
{
    UT uv = (UT) value;

    bool negative = false;

    if ( value < 0 )
    {
        negative = true;
        if ( (((UT) value) << 1) )
        {
            uv = -value;
        }
    }

    PRTUCS2 puszValue = 0;

    int rc = cfgldrhlp_uinteger_to_ustr<UT> (uv, &puszValue, negative);

    if (VBOX_SUCCESS(rc))
    {
        if (negative)
        {
            *puszValue = '-';
        }
        *ppuszValue = puszValue;
    }

    return rc;
}

inline void cfgldrhlp_release_ustr (PRTUCS2 pucszValue)
{
}

int cfgldrhlp_strtobin(PCRTUCS2 puszValue, void *pvValue, unsigned cbValue, unsigned *pcbValue);
int cfgldrhlp_bintostr(const void *pvValue, unsigned cbValue, char **ppszValue);
void cfgldrhlp_releasestr(char *pszValue);

#endif /* IN_RING3 */

//@@TODO (dmik): remove
//__END_DECLS

#endif /* __VBox_cfgldrhlp_h__ */
