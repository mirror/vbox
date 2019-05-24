/* $Id$ */
/** @file
 * Shared Clipboard Service - Host service utility functions.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/path.h>


/**
 * Returns whether a given clipboard data header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataHdr            Clipboard data header to validate.
 */
bool VBoxSvcClipboardDataHdrIsValid(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return true; /** @todo Implement this. */
}

/**
 * Returns whether a given clipboard data chunk is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataChunk          Clipboard data chunk to validate.
 */
bool VBoxSvcClipboardDataChunkIsValid(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    RT_NOREF(pDataChunk);
    return true; /** @todo Implement this. */
}

/**
 * Returns whether given clipboard directory data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDirData            Clipboard directory data to validate.
 */
bool VBoxSvcClipboardDirDataIsValid(PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (   !pDirData->cbPath
        || pDirData->cbPath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pDirData->pszPath))
        return false;

    return true;
}

/**
 * Returns whether a given clipboard file header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileHdr            Clipboard file header to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool VBoxSvcClipboardFileHdrIsValid(PVBOXCLIPBOARDFILEHDR pFileHdr, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (   !pFileHdr->cbFilePath
        || pFileHdr->cbFilePath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pFileHdr->pszFilePath))
        return false;

    if (pFileHdr->cbSize > pDataHdr->cbTotal)
        return false;

    return true;
}

/**
 * Returns whether given clipboard file data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileData           Clipboard file data to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool VBoxSvcClipboardFileDataIsValid(PVBOXCLIPBOARDFILEDATA pFileData, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pFileData, pDataHdr);
    return true;
}

