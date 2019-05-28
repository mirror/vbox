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

int SharedClipboardProviderHostService::ReadMetaData(void *pvData, size_t cbData, uint32_t fFlags /* = 0 */, size_t *pcbRead /* = NULL */)
{
    RT_NOREF(pvData, cbData, pcbRead, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadMetaData(SharedClipboardURIList &URIList, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(URIList, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReadData(void *pvBuf, size_t cbBuf, size_t *pcbRead  /* = NULL */)
{
    RT_NOREF(URIList, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProvider::WriteData(const void *pvBuf, size_t cbBuf, size_t *pcbWritten /* = NULL */)
{
    RT_NOREF(URIList, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

void SharedClipboardProvider::Reset(void)
{
}

