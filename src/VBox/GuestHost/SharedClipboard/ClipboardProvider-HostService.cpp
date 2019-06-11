/* $Id$ */
/** @file
 * Shared Clipboard - Provider implementation for host service (host side).
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
#include <VBox/GuestHost/SharedClipboard-uri.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/log.h>


SharedClipboardProviderHostService::SharedClipboardProviderHostService(void)
{
    LogFlowFuncEnter();
}

SharedClipboardProviderHostService::~SharedClipboardProviderHostService(void)
{
}

int SharedClipboardProviderHostService::ReadDataHdr(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadMetaData(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvMeta, uint32_t cbMeta,
                                                     uint32_t *pcbRead, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(pDataHdr, pvMeta, cbMeta, pcbRead, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::WriteMetaData(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvMeta, uint32_t cbMeta,
                                                      uint32_t *pcbWritten, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(pDataHdr, pvMeta, cbMeta, pcbWritten, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadDirectory(PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pDirData);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pDirData);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::ReadFileHdr(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pFileHdr);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pFileHdr);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::ReadFileData(PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbRead)
{
    RT_NOREF(pFileData, pcbRead);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::WriteFileData(const PVBOXCLIPBOARDFILEDATA pFileData, uint32_t *pcbWritten)
{
    RT_NOREF(pFileData, pcbWritten);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void SharedClipboardProviderHostService::Reset(void)
{
}

