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

int SharedClipboardProviderVbglR3::ReadDirectory(PVBOXCLIPBOARDDIRDATA pDirData)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        rc = VbglR3ClipboardReadDir(m_uClientID, pDirData->pszPath, pDirData->cbPath, &pDirData->cbPath, &pDirData->fMode);
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        rc = VbglR3ClipboardWriteDir(m_uClientID, pDirData->pszPath, pDirData->cbPath, pDirData->fMode);
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::ReadFileHdr(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        rc = VbglR3ClipboardReadFileHdr(m_uClientID, pFileHdr->pszFilePath, pFileHdr->cbFilePath,
                                        &pFileHdr->fFlags, &pFileHdr->fMode, &pFileHdr->cbSize);
        if (RT_SUCCESS(rc))
        {
            rc = pObj->SetFileData(pFileHdr->pszFilePath, SharedClipboardURIObject::View_Target,
                                   RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE, pFileHdr->fMode);
            if (RT_SUCCESS(rc))
                rc = pObj->SetSize(pFileHdr->cbSize);
        }
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        rc = VbglR3ClipboardWriteFileHdr(m_uClientID, pFileHdr->pszFilePath, pFileHdr->cbFilePath,
                                         pFileHdr->fFlags, pFileHdr->fMode, pFileHdr->cbSize);
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::ReadFileData(PVBOXCLIPBOARDFILEDATA pFileData)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        rc = VbglR3ClipboardReadFileData(m_uClientID, pFileData->pvData, pFileData->cbData, &pFileData->cbData);
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderVbglR3::WriteFileData(const PVBOXCLIPBOARDFILEDATA pFileData)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURIObject *pObj = m_URIList.First();
    if (pObj)
    {
        uint32_t cbWrittenIgnored;
        rc = VbglR3ClipboardWriteFileData(m_uClientID, pFileData->pvData, pFileData->cbData, &cbWrittenIgnored);
    }
    else
        rc = VERR_WRONG_ORDER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void SharedClipboardProviderVbglR3::Reset(void)
{
    LogFlowFuncEnter();

    m_URIList.Clear();

    /* Don't clear the refcount here. */
}

