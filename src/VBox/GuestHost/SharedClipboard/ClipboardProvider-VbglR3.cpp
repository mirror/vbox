/* $Id$ */
/** @file
 * Shared Clipboard - Provider implementation for VbglR3 (guest side).
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

SharedClipboardProviderVbglR3::SharedClipboardProviderVbglR3(void)
{
    LogFlowFuncEnter();

    m_cToRead = _64M;
}

SharedClipboardProviderVbglR3::~SharedClipboardProviderVbglR3(void)
{
}

int SharedClipboardProviderVbglR3::ReadMetaData(void *pvData, size_t cbData, uint32_t fFlags /* = 0 */, size_t *pcbRead /* = NULL */)
{
    RT_NOREF(pvData, cbData, pcbRead, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderVbglR3::ReadMetaData(SharedClipboardURIList &URIList, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(URIList, fFlags);

    LogFlowFuncEnter();

#ifdef DEBUG_andy
    SharedClipboardURIObject *pObj1 = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File, "foobar1.baz");
    pObj1->SetSize(_64M);
    URIList.AppendURIObject(pObj1);

    SharedClipboardURIObject *pObj2 = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File, "foobar2.baz");
    pObj2->SetSize(_32M);
    URIList.AppendURIObject(pObj2);
#endif

    return VINF_SUCCESS;
}

int SharedClipboardProviderVbglR3::ReadData(void *pvBuf, size_t cbBuf, size_t *pcbRead  /* = NULL */)
{
    RT_NOREF(pvBuf, cbBuf, pcbRead);

    LogFlowFuncEnter();

    size_t cbToRead = RT_MIN(cbBuf, RT_MIN(m_cToRead, _4K));

    memset(pvBuf, 0x64, cbToRead);

    m_cToRead -= cbToRead;

    *pcbRead = cbToRead;

    return VINF_SUCCESS;
}

int SharedClipboardProviderVbglR3::WriteData(const void *pvBuf, size_t cbBuf, size_t *pcbWritten /* = NULL */)
{
    RT_NOREF(pvBuf, cbBuf, pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}

void SharedClipboardProviderVbglR3::Reset(void)
{
}

