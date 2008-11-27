/* $Id$ */
/** @file
 * sysfs.h - convenience routines for getting values from sysfs on Linux.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ___VBox_sysfs_h
#define ___VBox_sysfs_h

#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>

__BEGIN_DECLS

/**
 * Read a string value from a sysfs file.
 * @returns iprt status value.
 * @returns VERR_BUFFER_OVERFLOW if @a cbBuffer was not sufficient to hold the
 *                               value.
 * @param   pszSysfsPath  Path to the sysfs directory to be read from.
 * @param   pszFile       Name of the file in @a pszSysfsPath to read the value
 *                        from.
 * @param   pszBuffer     Where to write the data read.
 * @param   cbBuffer      The size of the buffer in @pszbuffer.
 * @param   pcbBufActual  On success, the size of the data written.  On buffer
 *                        overflow the size of the buffer needed.  Optional.
 */
inline int VBoxSysfsGetString(const char *pszSysfsPath, const char *pszFile,
                              char *pszBuffer, size_t cbBuffer,
                              size_t *pcbBufActual)
{
    PRTSTREAM pStream;
    size_t cbBufActual;
    int rc = VINF_SUCCESS;

    char *pszFullName = NULL;
    RTMemAutoPtr<char, RTStrFree> FullName;
    RTStrAPrintf(&pszFullName, "%s/%s", pszSysfsPath, pszFile);
    FullName = pszFullName;
    if (!FullName)
        rc = VERR_NO_MEMORY;
    if (RT_SUCCESS(rc))
        rc = RTStrmOpen(pszFullName, "r", &pStream);
    if (RT_SUCCESS(rc))
        rc = RTStrmReadEx(pStream, pszBuffer, cbBuffer, &cbBufActual);
    /* Remove trailing newlines and zero-terminate */
    for (size_t i = 0; RT_SUCCESS(rc) && i < cbBufActual; ++i)
        if (pszBuffer[i] == '\n')
            cbBufActual = i;
    if (RT_SUCCESS(rc))
    {
        if (cbBufActual + 1 > cbBuffer)  /* +1 for the '\0' */
            rc = VERR_BUFFER_OVERFLOW;
        else
            pszBuffer[cbBufActual] = '\0';
    }
    if (pcbBufActual != NULL && (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW))
        *pcbBufActual = cbBufActual + 1;  /* +1 for the '\0' */
    RTStrmClose(pStream);
    return rc;
}

/**
 * Read a uint32 value from a sysfs file.
 * @returns iprt status value.
 * See also return codes from RTStrToUInt32Full().
 * @param   pszSysfsPath  Path to the sysfs directory to be read from.
 * @param   pszFile       Name of the file in @a pszSysfsPath to read the value
 *                        from.
 * @paramm  uBase         The base of the representation used.
 *                        If this is zero, the function will look for known
 *                        prefixes before defaulting to 10.
 * @param   pu32Value     Where to store the value read.
 */
inline int VBoxSysfsGetUInt32(const char *pszSysfsPath, const char *pszFile,
                              unsigned uBase, uint32_t *pu32Value)
{
    char szBuffer[256];
    int rc = VBoxSysfsGetString(pszSysfsPath, pszFile, szBuffer,
                                sizeof(szBuffer), NULL);
    if (RT_SUCCESS(rc))
        rc = RTStrToUInt32Full(szBuffer, uBase, pu32Value);
    return rc;
}

__END_DECLS

#endif

