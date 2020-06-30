/* $Id$ */
/** @file
 * DnD - Path handling.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/GuestHost/DragAndDrop.h>

#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>


/**
 * Sanitizes the file name component so that unsupported characters
 * will be replaced by an underscore ("_").
 *
 * @return  IPRT status code.
 * @param   pszPath             Path to sanitize.
 * @param   cbPath              Size (in bytes) of path to sanitize.
 */
int DnDPathSanitizeFilename(char *pszPath, size_t cbPath)
{
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    RT_NOREF1(cbPath);
    /* Replace out characters not allowed on Windows platforms, put in by RTTimeSpecToString(). */
    /** @todo Use something like RTPathSanitize() if available later some time. */
    static const RTUNICP s_uszValidRangePairs[] =
    {
        ' ', ' ',
        '(', ')',
        '-', '.',
        '0', '9',
        'A', 'Z',
        'a', 'z',
        '_', '_',
        0xa0, 0xd7af,
        '\0'
    };
    char *pszFilename = RTPathFilename(pszPath);
    if (pszFilename)
    {
        ssize_t cReplaced = RTStrPurgeComplementSet(pszFilename, s_uszValidRangePairs, '_' /* chReplacement */);
        if (cReplaced < 0)
            rc = VERR_INVALID_UTF8_ENCODING;
    }
    else
        rc = VERR_INVALID_PARAMETER;
#else
    RT_NOREF2(pszPath, cbPath);
#endif
    return rc;
}

/**
 * Validates whether a given path matches our set of rules or not.
 *
 * @returns VBox status code.
 * @param   pcszPath            Path to validate.
 * @param   fMustExist          Whether the path to validate also must exist.
 * @sa      shClTransferValidatePath().
 */
int DnDPathValidate(const char *pcszPath, bool fMustExist)
{
    int rc = VINF_SUCCESS;

    if (!strlen(pcszPath))
        rc = VERR_INVALID_PARAMETER;

    if (   RT_SUCCESS(rc)
        && !RTStrIsValidEncoding(pcszPath))
    {
        rc = VERR_INVALID_UTF8_ENCODING;
    }

    if (   RT_SUCCESS(rc)
        && RTStrStr(pcszPath, ".."))
    {
        rc = VERR_INVALID_PARAMETER;
    }

    if (   RT_SUCCESS(rc)
        && fMustExist)
    {
        RTFSOBJINFO objInfo;
        rc = RTPathQueryInfo(pcszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
            {
                if (!RTDirExists(pcszPath)) /* Path must exist. */
                    rc = VERR_PATH_NOT_FOUND;
            }
            else if (RTFS_IS_FILE(objInfo.Attr.fMode))
            {
                if (!RTFileExists(pcszPath)) /* File must exist. */
                    rc = VERR_FILE_NOT_FOUND;
            }
            else /* Everything else (e.g. symbolic links) are not supported. */
                rc = VERR_NOT_SUPPORTED;
        }
    }

    return rc;
}

/**
 * Converts a DnD path.
 *
 * @returns VBox status code.
 * @param   pszPath             Path to convert.
 * @param   cbPath              Size (in bytes) of path to convert.
 * @param   fFlags              Conversion flags of type DNDPATHCONVERT_FLAGS_.
 */
int DnDPathConvert(char *pszPath, size_t cbPath, DNDPATHCONVERTFLAGS fFlags)
{
    RT_NOREF(cbPath);
    AssertReturn(!(fFlags & ~DNDPATHCONVERT_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    if (fFlags & DNDPATHCONVERT_FLAGS_TO_NATIVE)
        RTPathChangeToDosSlashes(pszPath, true);
    else
#else
    RT_NOREF(fFlags);
#endif
    {
        RTPathChangeToUnixSlashes(pszPath, true /* fForce */);
    }

    return VINF_SUCCESS;
}

