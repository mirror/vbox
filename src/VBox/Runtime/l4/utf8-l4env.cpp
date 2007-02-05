/* $Id$ */
/** @file
 * InnoTek Portable Runtime - UTF-8 helpers, l4env.
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

 /**
  * Since the L4 microkernel does not actually deal with text,
  * we obviously do not have a default character set.  Some of
  * the l4env utilities may do, but for now we will take ASCII
  * as the default.  Perhaps we can change that to UTF-8 sometime?
  * @note Do a lot of testing of these functions as they are
  * bound to be buggy for the moment!
  */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>

#include <l4/sys/l4int.h>
#include <stdio.h>


/**
 * Allocates tmp buffer, translates pszString from UTF8 to current codepage.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated native CP string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to convert.
 * In this case, the conversion is a simple one to ASCII, where we
 * replace any "unknown" characters with '-'.
 */
RTR3DECL(int)  RTStrUtf8ToCurrentCP(char **ppszString, const char *pszString)
{
    Assert(ppszString);
    Assert(pszString);
    size_t cch = strlen(pszString);
    if (cch < 0) return VERR_INVALID_PARAMETER;  /* very strange :-) */
    *ppszString = (char *) RTMemTmpAlloc((cch + 1) * sizeof(uint8_t));
    if (!*ppszString)
        return VERR_NO_TMP_MEMORY;
    const char *pIn = pszString;
    char *pOut = *ppszString;
    /* Translate all ascii characters to ascii, all others to '-' */
    for (; *pIn != 0; pOut++) {
        if (*pIn > 0) {
            *pOut = *pIn;
            pIn++;
        } else {
            while (*pIn < 0)
                pIn++;
            *pOut = '-';
        }
    }
    *(pOut) = 0;
    return VINF_SUCCESS;
}


/**
 * Allocates tmp buffer, translates pszString from current codepage to UTF-8.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       Native string to convert.
 * Again, we are converting from ASCII, so that there is even less to
 * do than in the last function.
 */
RTR3DECL(int)  RTStrCurrentCPToUtf8(char **ppszString, const char *pszString)
{
    return RTStrDupEx(ppszString, pszString);
}

