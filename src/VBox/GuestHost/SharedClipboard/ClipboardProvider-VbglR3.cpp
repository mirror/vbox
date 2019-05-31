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
#include <VBox/VBoxGuestLib.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/log.h>


SharedClipboardProviderVbglR3::SharedClipboardProviderVbglR3(uint32_t uClientID)
    : m_uClientID(uClientID)
{
    LogFlowFunc(("m_uClientID=%RU32\n", m_uClientID));
}

SharedClipboardProviderVbglR3::~SharedClipboardProviderVbglR3(void)
{
    m_URIList.Clear();
}

int SharedClipboardProviderVbglR3::ReadMetaData(uint32_t fFlags /* = 0 */)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    int rc = VbglR3ClipboardReadMetaData(m_uClientID, m_URIList);

#ifdef DEBUG_andy_disabled
    SharedClipboardURIObject *pObj1 = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File, "foobar1.baz");
    pObj1->SetSize(_64M);
    m_URIList.AppendURIObject(pObj1);

    SharedClipboardURIObject *pObj2 = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File, "foobar2.baz");
    pObj2->SetSize(_32M);
    m_URIList.AppendURIObject(pObj2);
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::WriteMetaData(const void *pvBuf, size_t cbBuf, size_t *pcbWritten, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(pcbWritten, fFlags);

    SHAREDCLIPBOARDURILISTFLAGS fURIListFlags = SHAREDCLIPBOARDURILIST_FLAGS_NONE;

    int rc = m_URIList.SetFromURIData(pvBuf, cbBuf, fURIListFlags);
    if (RT_SUCCESS(rc))
        rc = VbglR3ClipboardWriteMetaData(m_uClientID, m_URIList);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::ReadData(void *pvBuf, size_t cbBuf, size_t *pcbRead /* = NULL */)
{
    LogFlowFuncEnter();

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (!pObj)
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    size_t cbToRead = RT_MIN(cbBuf, RT_MIN(123, _4K));

    memset(pvBuf, 0x64, cbToRead);

    *pcbRead = cbToRead;

    if (pObj->IsComplete())
    {
        m_URIList.RemoveFirst();
        pObj = NULL;
    }

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

