/** @file
 *
 * CFGLDRHLP - Configuration Loader Helpers
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifdef RT_OS_WINDOWS
# if _MSC_VER < 1400
/// @todo Without this I cannot get the stuff to
// link. I don't know where it should come from.
// bird: This is probably a __declspec(import) type var; thing. So, using the right
// compiler options (i.e. template) should cure the problem I guess.
#  include <ctype.h>
extern "C"
{
    int __mb_cur_max = 1;
    int errno;
}
# endif
#endif /* RT_OS_WINDOWS */

#include "cfgldrhlp.h"

#include <VBox/err.h>

#include <stdio.h>
#include <ctype.h>
#include <limits.h>


inline unsigned char fromhex(RTUCS2 hexdigit)
{
    if (hexdigit >= '0' && hexdigit <= '9')
        return hexdigit - '0';
    if (hexdigit >= 'A' && hexdigit <= 'F')
        return hexdigit - 'A' + 0xA;
    if (hexdigit >= 'a' && hexdigit <= 'f')
        return hexdigit - 'a' + 0xa;

    return 0xFF; // error indicator
}

inline char tohex (unsigned char ch)
{
    return (ch < 0xA) ? ch + '0' : ch - 0xA + 'A';
}

static int _strtouint64 (PRTUCS2 puszValue, uint64_t *pvalue)
{
    int rc = VINF_SUCCESS;

    PRTUCS2 p = puszValue;

    // skip leading spaces
    while (*p <= 0xFF && isspace ((char)*p))
    {
        p++;
    }

    // autodetect base
    unsigned base = 10;

    if (*p == '0')
    {
        p++;

        if (*p == 'x' || *p == 'X')
        {
            p++;
            base = 16;
        }
        else
        {
            base = 8;
        }
    }

    // p points to the first character in the string, start conversion
    uint64_t value = 0;

    while (*p)
    {
        unsigned digit;

        // check the unicode character to be within ASCII range
        if (*p > 0x7F)
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (isdigit ((char)*p))
        {
            digit = *p - '0';
        }
        else if ('a' <= *p && *p <= 'f')
        {
            digit = *p - 'a' + 0xA;
        }
        else if ('A' <= *p && *p <= 'F')
        {
            digit = *p - 'F' + 0xA;
        }
        else
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (digit >= base)
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        /// @todo overflow check
        value = value * base + digit;

        p++;
    }

    if (VBOX_SUCCESS(rc))
    {
        *pvalue = value;
    }

    return rc;
}

static int _uint64tostr (uint64_t ullValue, char *pszValue, unsigned base)
{
    int rc = VINF_SUCCESS;

    char *p = pszValue;

    if (base == 16)
    {
        *p++ = '0';
        *p++ = 'x';
    }

    // remember start of the digit string
    char *s = p;

    for (;;)
    {
        unsigned digit = (unsigned)ullValue % base;

        if (base == 16)
        {
            *p++ = tohex (digit);
        }
        else
        {
            *p++ = digit + '0';
        }

        ullValue /= base;

        if (ullValue == 0)
        {
            break;
        }
    }

    *p = 0;

    // reverse the resulting string
    p--; // skip trailing nul

    while (s < p)
    {
        char tmp = *s;
        *s++ = *p;
        *p-- = tmp;
    }

    return rc;
}

int cfgldrhlp_strtouint32 (PRTUCS2 puszValue, uint32_t *pvalue)
{
    uint64_t intermediate;

    int rc = _strtouint64 (puszValue, &intermediate);

    if (VBOX_SUCCESS(rc))
    {
        if (intermediate > (uint64_t)ULONG_MAX)
        {
            rc = VERR_CFG_INVALID_FORMAT;
        }
        else
        {
            *pvalue = (uint32_t)intermediate;
        }
    }

    return rc;
}

int cfgldrhlp_strtouint64 (PRTUCS2 puszValue, uint64_t *pvalue)
{
    return _strtouint64 (puszValue, pvalue);
}

int cfgldrhlp_uint32tostr (uint32_t ulValue, char *pszValue)
{
    return _uint64tostr (ulValue, pszValue, 10);
}

// Note: 64 bit values are converted to hex notation
int cfgldrhlp_uint64tostr (uint64_t ullValue, char *pszValue)
{
    return _uint64tostr (ullValue, pszValue, 16);
}

// converts a string of hex digits to memory bytes
int cfgldrhlp_strtobin (PCRTUCS2 puszValue, void *pvValue, unsigned cbValue, unsigned *pcbValue)
{
    int rc = VINF_SUCCESS;

    unsigned count = 0; // number of bytes those can be converted successfully from the puszValue
    unsigned char *dst = (unsigned char *)pvValue;

    while (*puszValue)
    {
        unsigned char b = fromhex (*puszValue);

        if (b == 0xFF)
        {
            // it was not a valid hex digit
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (count < cbValue)
        {
            *dst = b;
        }

        puszValue++;

        if (!*puszValue)
        {
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        b = fromhex (*puszValue++);

        if (b == 0xFF)
        {
            // it was not a valid hex digit
            rc = VERR_CFG_INVALID_FORMAT;
            break;
        }

        if (count < cbValue)
        {
            *dst = ((*dst) << 4) + b;
            dst++;
        }

        count++;
    }

    *pcbValue = count;

    return rc;
}

// convert memory bytes to UTF8 nul terminated string of hex values
int cfgldrhlp_bintostr (const void *pvValue, unsigned cbValue, char **ppszValue)
{
    int rc = VINF_SUCCESS;

    // each byte will produce two hex digits and there will be nul terminator
    char *pszValue = (char *)RTMemTmpAlloc (cbValue * 2 + 1);

    if (!pszValue)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        unsigned         i = 0;
        unsigned char *src = (unsigned char *)pvValue;
        char *dst          = pszValue;

        for (; i < cbValue; i++, src++)
        {
            *dst++ = tohex((*src) >> 4);
            *dst++ = tohex((*src) & 0xF);
        }

        *dst = 0;

        *ppszValue = pszValue;
    }

    return rc;
}

// releases memory buffer previously allocated by cfgldrhlp_bintostr
void cfgldrhlp_releasestr (char *pszValue)
{
    if (pszValue)
    {
        RTMemTmpFree (pszValue);
    }
}
