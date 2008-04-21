/* $Id$ */
/** @file
 * IPRT - UTF-8 helpers, POSIX.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>

#include <errno.h>
#include <locale.h>
#include <iconv.h>
#include <wctype.h>

#ifdef RT_OS_SOLARIS
#include <langinfo.h>
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int rtstrConvert(const void *pvInput, size_t cbInput, const char *pszInputCS, void **ppvOutput, size_t cbOutput, const char *pszOutputCS, unsigned cFactor);


/**
 * Converts a string from one charset to another.
 *
 * @returns iprt status code.
 * @param   pvInput         Pointer to intput string.
 * @param   cbInput         Size (in bytes) of input string. Excludes any terminators.
 * @param   pszInputCS      Codeset of the input string.
 * @param   ppvOutput       Pointer to pointer to output buffer if cbOutput > 0.
 *                          If cbOutput is 0 this is where the pointer to the allocated
 *                          buffer is stored.
 * @param   cbOutput        Size of the passed in buffer.
 * @param   pszOutputCS     Codeset of the input string.
 * @param   cFactor         Input vs. output size factor.
 */
static int rtstrConvert(const void *pvInput, size_t cbInput, const char *pszInputCS, void **ppvOutput, size_t cbOutput, const char *pszOutputCS, unsigned cFactor)
{
    /*
     * Allocate buffer
     */
    void   *pvOutput;
    size_t  cbOutput2;
    if (!cbOutput)
    {
        cbOutput2 = cbInput * cFactor;
        pvOutput = RTMemTmpAlloc(cbOutput2 + sizeof(RTUTF16));
        if (!pvOutput)
            return VERR_NO_TMP_MEMORY;
    }
    else
    {
        pvOutput = *ppvOutput;
        cbOutput2 = cbOutput - (!strcmp(pszOutputCS, "UCS-2") ? sizeof(RTUTF16) : 1);
        if (cbOutput2 > cbOutput)
            return VERR_BUFFER_OVERFLOW;
    }

    /*
     * Use a loop here to retry with bigger buffers.
     */
    for (unsigned cTries = 10; cTries > 0; cTries--)
    {
        /*
         * Create conversion object.
         */
#ifdef RT_OS_SOLARIS
        /* Solaris doesn't grok empty codeset strings, so help it find the current codeset. */
        if (!*pszInputCS)
            pszInputCS = nl_langinfo(CODESET);
        if (!*pszOutputCS)
            pszOutputCS = nl_langinfo(CODESET);
#endif
        iconv_t icHandle = iconv_open(pszOutputCS, pszInputCS);
        if (icHandle != (iconv_t)-1)
        {
            /*
             * Do the conversion.
             */
            size_t      cbInLeft = cbInput;
            size_t      cbOutLeft = cbOutput2;
            const void *pvInputLeft = pvInput;
            void       *pvOutputLeft = pvOutput;
#if defined(RT_OS_LINUX) || (defined(RT_OS_DARWIN) && defined(_DARWIN_FEATURE_UNIX_CONFORMANCE)) /* there are different opinions about the constness of the input buffer. */
            if (iconv(icHandle, (char **)&pvInputLeft, &cbInLeft, (char **)&pvOutputLeft, &cbOutLeft) != (size_t)-1)
#else
            if (iconv(icHandle, (const char **)&pvInputLeft, &cbInLeft, (char **)&pvOutputLeft, &cbOutLeft) != (size_t)-1)
#endif
            {
                if (!cbInLeft)
                {
                    /*
                     * We're done, just add the terminator and return.
                     * (Two terminators to support UCS-2 output, too.)
                     */
                    iconv_close(icHandle);
                    if (!cbOutput || !strcmp(pszOutputCS, "UCS-2"))
                        *(PRTUTF16)pvOutputLeft = '\0';
                    else
                        *(char *)pvOutputLeft = '\0';
                    *ppvOutput = pvOutput;
                    return VINF_SUCCESS;
                }
                else
                    errno = E2BIG;
            }
            iconv_close(icHandle);

            /*
             * If we failed because of output buffer space we'll
             * increase the output buffer size and retry.
             */
            if (errno == E2BIG)
            {
                if (!cbOutput)
                {
                    RTMemTmpFree(pvOutput);
                    cbOutput2 *= 2;
                    pvOutput = RTMemTmpAlloc(cbOutput2);
                    if (!pvOutput)
                        return VERR_NO_TMP_MEMORY;
                    continue;
                }
                return VERR_BUFFER_OVERFLOW;
            }
        }
        break;
    }

    /* failure */
    if (!cbOutput)
        RTMemTmpFree(pvOutput);
    return VERR_NO_TRANSLATION;
}


/**
 * Allocates tmp buffer, translates pszString from UTF8 to current codepage.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated native CP string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to convert.
 */
RTR3DECL(int)  RTStrUtf8ToCurrentCP(char **ppszString, const char *pszString)
{
    Assert(ppszString);
    Assert(pszString);
    *ppszString = NULL;

    /*
     * Assume result string length is not longer than UTF-8 string.
     */
    size_t cch = strlen(pszString);
    if (cch <= 0)
    {
        /* zero length string passed. */
        *ppszString = (char *)RTMemTmpAllocZ(sizeof(char));
        if (*ppszString)
            return VINF_SUCCESS;
        return VERR_NO_TMP_MEMORY;
    }
    return rtstrConvert(pszString, cch, "UTF-8", (void **)ppszString, 0, "", 1);
}


/**
 * Allocates tmp buffer, translates pszString from current codepage to UTF-8.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       Native string to convert.
 */
RTR3DECL(int)  RTStrCurrentCPToUtf8(char **ppszString, const char *pszString)
{
    Assert(ppszString);
    Assert(pszString);
    *ppszString = NULL;

    /*
     * Attempt with UTF-8 length of 2x the native lenght.
     */
    size_t cch = strlen(pszString);
    if (cch <= 0)
    {
        /* zero length string passed. */
        *ppszString = (char *)RTMemTmpAllocZ(sizeof(char));
        if (*ppszString)
            return VINF_SUCCESS;
        return VERR_NO_TMP_MEMORY;
    }
    return rtstrConvert(pszString, cch, "", (void **)ppszString, 0, "UTF-8", 2);
}


