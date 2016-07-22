/* $Id$ */
/** @file
 * IPRT - RTFileModeToFlags.
 */

/*
 * Copyright (C) 2013-2016 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include "internal/iprt.h"


RTR3DECL(int) RTFileModeToFlags(const char *pszMode, uint64_t *puMode)
{
    AssertPtrReturn(pszMode, VERR_INVALID_POINTER);
    AssertPtrReturn(puMode, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    const char *pszCur = pszMode;
    uint64_t uMode = 0;
    char chPrev = 0;

    if (*pszCur == '\0')
        return VERR_INVALID_PARAMETER;

    while (    pszCur
           && *pszCur != '\0')
    {
        bool fSkip = false;
        switch (*pszCur)
        {
            /* Opens an existing file for writing and places the
             * file pointer at the end of the file. The file is
             * created if it does not exist. */
            case 'a':
                if ((uMode & RTFILE_O_ACTION_MASK) == 0)
                {
                    uMode |=   RTFILE_O_OPEN_CREATE
                             | RTFILE_O_WRITE
                             | RTFILE_O_APPEND;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                break;

            case 'b': /* Binary mode. */
                /* Just skip as being valid. */
                fSkip = true;
                break;

            /* Creates a file or open an existing one for
             * writing only. The file pointer will be placed
             * at the beginning of the file.*/
            case 'c':
                if ((uMode & RTFILE_O_ACTION_MASK) == 0)
                {
                    uMode |=   RTFILE_O_OPEN_CREATE
                             | RTFILE_O_WRITE;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                break;

            /* Opens an existing file for reading and places the
             * file pointer at the beginning of the file. If the
             * file does not exist an error will be returned. */
            case 'r':
                if ((uMode & RTFILE_O_ACTION_MASK) == 0)
                {
                    uMode |=   RTFILE_O_OPEN
                             | RTFILE_O_READ;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                break;

            case 't': /* Text mode. */
                /* Just skip as being valid. */
                fSkip = true;
                break;

            /* Creates a new file or replaces an existing one
             * for writing. Places the file pointer at the beginning.
             * An existing file will be truncated to 0 bytes. */
            case 'w':
                if ((uMode & RTFILE_O_ACTION_MASK) == 0)
                {
                    uMode |=   RTFILE_O_CREATE_REPLACE
                             | RTFILE_O_WRITE
                             | RTFILE_O_TRUNCATE;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                break;

            /* Creates a new file and opens it for writing. Places
             * the file pointer at the beginning. If the file
             * exists an error will be returned. */
            case 'x':
                if ((uMode & RTFILE_O_ACTION_MASK) == 0)
                {
                    uMode |=   RTFILE_O_CREATE
                             | RTFILE_O_WRITE;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                break;

            case '+':
            {
                switch (chPrev)
                {
                    case 'a':
                    case 'c':
                    case 'w':
                    case 'x':
                        /* Also open / create file with read access. */
                        uMode |= RTFILE_O_READ;
                        break;

                    case 'r':
                        /* Also open / create file with write access. */
                        uMode |= RTFILE_O_WRITE;
                        break;

                    case 'b':
                    case 't':
                        /* Silently eat skipped parameters. */
                        fSkip = true;
                        break;

                    case 0: /* No previous character yet. */
                    case '+':
                        /* Eat plusses which don't belong to a command. */
                        fSkip = true;
                        break;

                    default:
                        rc = VERR_INVALID_PARAMETER;
                        break;
                }

                break;
            }

            default:
                rc = VERR_INVALID_PARAMETER;
                break;
        }

        if (RT_FAILURE(rc))
            break;

        if (!fSkip)
            chPrev = *pszCur;
        pszCur++;
    }

    /* No action mask set? */
    if (   RT_SUCCESS(rc)
        && (uMode & RTFILE_O_ACTION_MASK) == 0)
        rc = VERR_INVALID_PARAMETER;

    /** @todo Handle sharing mode. */
    uMode |= RTFILE_O_DENY_NONE;

    if (RT_SUCCESS(rc))
        *puMode = uMode;

    return rc;
}
RT_EXPORT_SYMBOL(RTFileModeToFlags);


RTR3DECL(int) RTFileModeToFlagsEx(const char *pszAccess, const char *pszDisposition,
                                  const char *pszSharing, uint64_t *puMode)
{
    AssertPtrReturn(pszAccess, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDisposition, VERR_INVALID_POINTER);
    /* pszSharing is not used yet. */
    AssertPtrReturn(puMode, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    const char *pszCur = pszAccess;
    uint64_t uMode = 0;
    char chPrev = 0;

    if (*pszCur == '\0')
        return VERR_INVALID_PARAMETER;

    /*
     * Handle access mode.
     */
    while (    pszCur
           && *pszCur != '\0')
    {
        bool fSkip = false;
        switch (*pszCur)
        {
            case 'b': /* Binary mode. */
                /* Just skip as being valid. */
                fSkip = true;
                break;

            case 'r': /* Read. */
                uMode |= RTFILE_O_READ;
                break;

            case 't': /* Text mode. */
                /* Just skip as being valid. */
                fSkip = true;
                break;

            case 'w': /* Write. */
                uMode |= RTFILE_O_WRITE;
                break;

            case '+':
            {
                switch (chPrev)
                {
                    case 'w':
                        /* Also use read access in write mode. */
                        uMode |= RTFILE_O_READ;
                        break;

                    case 'r':
                        /* Also use write access in read mode. */
                        uMode |= RTFILE_O_WRITE;
                        break;

                    case 'b':
                    case 't':
                        /* Silently eat skipped parameters. */
                        fSkip = true;
                        break;

                    case 0: /* No previous character yet. */
                    case '+':
                        /* Eat plusses which don't belong to a command. */
                        fSkip = true;
                        break;

                    default:
                        rc = VERR_INVALID_PARAMETER;
                        break;
                }

                break;
            }

            default:
                rc = VERR_INVALID_PARAMETER;
                break;
        }

        if (RT_FAILURE(rc))
            break;

        if (!fSkip)
            chPrev = *pszCur;
        pszCur++;
    }

    if (RT_FAILURE(rc))
        return rc;

    /*
     * Handle disposition.
     */
    pszCur = pszDisposition;

    /* Create a new file, always, overwrite an existing file. */
    if (!RTStrCmp(pszCur, "ca"))
        uMode |= RTFILE_O_CREATE_REPLACE;
    /* Create a new file if it does not exist, fail if exist. */
    else if (!RTStrCmp(pszCur, "ce"))
        uMode |= RTFILE_O_CREATE;
    /* Open existing file, create file if does not exist. */
    else if (!RTStrCmp(pszCur, "oc"))
        uMode |= RTFILE_O_OPEN_CREATE;
    /* Open existing file and place the file pointer at
     * the end of the file, if opened with write access.
     * Create the file if does not exist. */
    else if (!RTStrCmp(pszCur, "oa"))
        uMode |= RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND;
    /* Open existing, fail if does not exist. */
    else if (!RTStrCmp(pszCur, "oe"))
        uMode |= RTFILE_O_OPEN;
    /* Open and truncate existing, fail of not exist. */
    else if (!RTStrCmp(pszCur, "ot"))
        uMode |= RTFILE_O_OPEN | RTFILE_O_TRUNCATE;
    else
        rc = VERR_INVALID_PARAMETER;

    /* No action mask set? */
    if (   RT_SUCCESS(rc)
        && (uMode & RTFILE_O_ACTION_MASK) == 0)
        rc = VERR_INVALID_PARAMETER;

    /** @todo Handle sharing mode. */
    uMode |= RTFILE_O_DENY_NONE;

    if (RT_SUCCESS(rc))
        *puMode = uMode;

    return rc;
}
RT_EXPORT_SYMBOL(RTFileModeToFlagsEx);

