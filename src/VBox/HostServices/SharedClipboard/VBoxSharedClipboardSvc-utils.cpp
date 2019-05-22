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


bool VBoxSvcClipboardDataHdrIsValid(PVBOXCLIPBOARDDATAHDR pData)
{
    RT_NOREF(pData);
    return true; /** @todo Implement this. */
}

bool VBoxSvcClipboardDataChunkIsValid(VBOXCLIPBOARDDATACHUNK pData)
{
    RT_NOREF(pData);
    return true; /** @todo Implement this. */
}

bool VBoxSvcClipboardDirDataIsValid(PVBOXCLIPBOARDDIRDATA pData)
{
    if (   !pData->cbPath
        || pData->cbPath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pData->pszPath))
        return false;

    return true;
}

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

bool VBoxSvcClipboardFileDataIsValid(PVBOXCLIPBOARDFILEDATA pData, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pData, pDataHdr);
    return true;
}
