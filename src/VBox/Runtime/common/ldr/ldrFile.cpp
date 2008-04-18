/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Binary Image Loader, The File Oriented Parts.
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
#define LOG_GROUP RTLOGGROUP_LDR
#include <iprt/ldr.h>
#include <iprt/alloc.h>
#include <iprt/file.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/ldr.h"
#include "internal/ldrMZ.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * File Reader instance.
 * This provides raw image bits from a file.
 */
typedef struct RTLDRREADERFILE
{
    /** The core. */
    RTLDRREADER     Core;
    /** The file. */
    RTFILE          File;
    /** The file size. */
    RTFOFF          cbFile;
    /** The current offset. */
    RTFOFF          off;
    /** Number of users or the mapping. */
    RTUINT          cMappings;
    /** Pointer to the in memory mapping. */
    void           *pvMapping;
    /** The filename (variable size). */
    char            szFilename[1];
} RTLDRREADERFILE, *PRTLDRREADERFILE;


/** @copydoc RTLDRREADER::pfnRead */
static DECLCALLBACK(int) rtldrFileRead(PRTLDRREADER pReader, void *pvBuf, size_t cb, RTFOFF off)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;

    /*
     * Seek.
     */
    if (pFileReader->off != off)
    {
        int rc = RTFileSeek(pFileReader->File, off, RTFILE_SEEK_BEGIN, NULL);
        if (RT_FAILURE(rc))
        {
            pFileReader->off = -1;
            return rc;
        }
        pFileReader->off = off;
    }

    /*
     * Read.
     */
    int rc = RTFileRead(pFileReader->File, pvBuf, cb, NULL);
    if (RT_SUCCESS(rc))
        pFileReader->off += cb;
    else
        pFileReader->off = -1;
    return rc;
}


/** @copydoc RTLDRREADER::pfnTell */
static DECLCALLBACK(RTFOFF) rtldrFileTell(PRTLDRREADER pReader)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;
    return pFileReader->off;
}


/** @copydoc RTLDRREADER::pfnSize */
static DECLCALLBACK(RTFOFF) rtldrFileSize(PRTLDRREADER pReader)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;
    return pFileReader->cbFile;
}


/** @copydoc RTLDRREADER::pfnLogName */
static DECLCALLBACK(const char *) rtldrFileLogName(PRTLDRREADER pReader)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;
    return pFileReader->szFilename;
}


/** @copydoc RTLDRREADER::pfnMap */
static DECLCALLBACK(int) rtldrFileMap(PRTLDRREADER pReader, const void **ppvBits)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;

    /*
     * Already mapped?
     */
    if (pFileReader->pvMapping)
    {
        pFileReader->cMappings++;
        *ppvBits = pFileReader->pvMapping;
        return VINF_SUCCESS;
    }

    /*
     * Allocate memory.
     */
    size_t cb = (size_t)pFileReader->cbFile;
    if ((RTFOFF)cb != pFileReader->cbFile)
        return VERR_IMAGE_TOO_BIG;
    pFileReader->pvMapping = RTMemAlloc(cb);
    if (!pFileReader->pvMapping)
        return VERR_NO_MEMORY;
    int rc = rtldrFileRead(pReader, pFileReader->pvMapping, cb, 0);
    if (RT_SUCCESS(rc))
    {
        pFileReader->cMappings = 1;
        *ppvBits = pFileReader->pvMapping;
    }
    else
    {
        RTMemFree(pFileReader->pvMapping);
        pFileReader->pvMapping = NULL;
    }

    return rc;
}


/** @copydoc RTLDRREADER::pfnUnmap */
static DECLCALLBACK(int) rtldrFileUnmap(PRTLDRREADER pReader, const void *pvBits)
{
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;
    AssertReturn(pFileReader->cMappings > 0, VERR_INVALID_PARAMETER);

    if (!--pFileReader->cMappings)
    {
        RTMemFree(pFileReader->pvMapping);
        pFileReader->pvMapping = NULL;
    }

    return VINF_SUCCESS;
}


/** @copydoc RTLDRREADER::pfnDestroy */
static DECLCALLBACK(int) rtldrFileDestroy(PRTLDRREADER pReader)
{
    int rc = VINF_SUCCESS;
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)pReader;
    if (pFileReader->File != NIL_RTFILE)
    {
        rc = RTFileClose(pFileReader->File);
        AssertRC(rc);
        pFileReader->File = NIL_RTFILE;
    }
    RTMemFree(pFileReader);
    return rc;
}


/**
 * Opens a loader file reader.
 *
 * @returns iprt status code.
 * @param   ppReader        Where to store the reader instance on success.
 * @param   pszFilename     The file to open.
 */
static int rtldrFileCreate(PRTLDRREADER *ppReader, const char *pszFilename)
{
    size_t cchFilename = strlen(pszFilename);
    int rc = VERR_NO_MEMORY;
    PRTLDRREADERFILE pFileReader = (PRTLDRREADERFILE)RTMemAlloc(sizeof(*pFileReader) + cchFilename);
    if (pFileReader)
    {
        memcpy(pFileReader->szFilename, pszFilename, cchFilename + 1);
        rc = RTFileOpen(&pFileReader->File, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileGetSize(pFileReader->File, (uint64_t *)&pFileReader->cbFile);
            if (RT_SUCCESS(rc))
            {
                pFileReader->Core.pfnRead    = rtldrFileRead;
                pFileReader->Core.pfnTell    = rtldrFileTell;
                pFileReader->Core.pfnSize    = rtldrFileSize;
                pFileReader->Core.pfnLogName = rtldrFileLogName;
                pFileReader->Core.pfnMap     = rtldrFileMap;
                pFileReader->Core.pfnUnmap   = rtldrFileUnmap;
                pFileReader->Core.pfnDestroy = rtldrFileDestroy;
                pFileReader->off       = 0;
                pFileReader->cMappings = 0;
                pFileReader->pvMapping = NULL;
                *ppReader = &pFileReader->Core;
                return VINF_SUCCESS;
            }
            RTFileClose(pFileReader->File);
        }
        RTMemFree(pFileReader);
    }
    *ppReader = NULL;
    return rc;
}


/**
 * Open a binary image file.
 *
 * @returns iprt status code.
 * @param   pszFilename Image filename.
 * @param   phLdrMod    Where to store the handle to the loaded module.
 */
RTDECL(int) RTLdrOpen(const char *pszFilename, PRTLDRMOD phLdrMod)
{
    LogFlow(("RTLdrOpen: pszFilename=%p:{%s} phLdrMod=%p\n",
             pszFilename, pszFilename, phLdrMod));

    /*
     * Create file reader & invoke worker which identifies and calls the image interpreter.
     */
    PRTLDRREADER pReader;
    int rc = rtldrFileCreate(&pReader, pszFilename);
    if (RT_SUCCESS(rc))
    {
        rc = rtldrOpenWithReader(pReader, phLdrMod);
        if (RT_SUCCESS(rc))
        {
            LogFlow(("RTLdrOpen: return %Rrc *phLdrMod\n", rc, *phLdrMod));
            return rc;
        }
        pReader->pfnDestroy(pReader);
    }
    *phLdrMod = NIL_RTLDRMOD;
    LogFlow(("RTLdrOpen: return %Rrc\n", rc));
    return rc;
}


/**
 * Opens a binary image file using kLdr.
 *
 * @returns iprt status code.
 * @param   pszFilename     Image filename.
 * @param   phLdrMod        Where to store the handle to the loaded module.
 * @remark  Primarily for testing the loader.
 */
RTDECL(int) RTLdrOpenkLdr(const char *pszFilename, PRTLDRMOD phLdrMod)
{
#ifdef LDR_WITH_KLDR
    LogFlow(("RTLdrOpenkLdr: pszFilename=%p:{%s} phLdrMod=%p\n",
             pszFilename, pszFilename, phLdrMod));

    /*
     * Create file reader & invoke worker which identifies and calls the image interpreter.
     */
    PRTLDRREADER pReader;
    int rc = rtldrFileCreate(&pReader, pszFilename);
    if (RT_SUCCESS(rc))
    {
        rc = rtldrkLdrOpen(pReader, phLdrMod);
        if (RT_SUCCESS(rc))
        {
            LogFlow(("RTLdrOpenkLdr: return %Rrc *phLdrMod\n", rc, *phLdrMod));
            return rc;
        }
        pReader->pfnDestroy(pReader);
    }
    *phLdrMod = NIL_RTLDRMOD;
    LogFlow(("RTLdrOpenkLdr: return %Rrc\n", rc));
    return rc;

#else
    return RTLdrOpen(pszFilename, phLdrMod);
#endif
}

