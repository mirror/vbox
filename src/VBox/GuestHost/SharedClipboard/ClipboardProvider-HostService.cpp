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
#include <iprt/semaphore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/log.h>



SharedClipboardProviderHostService::Event::Event(uint32_t uMsg)
    : mMsg(uMsg)
    , mpvData(NULL)
    , mcbData(0)
{
    int rc2 = RTSemEventCreate(&mEvent);
    AssertRC(rc2);
}

SharedClipboardProviderHostService::Event::~Event()
{
    if (mpvData)
    {
        Assert(mcbData);

        RTMemFree(mpvData);
        mpvData = NULL;
    }

    int rc2 = RTSemEventDestroy(mEvent);
    AssertRC(rc2);
}

/**
 * Sets user data associated to an event.
 *
 * @returns VBox status code.
 * @param   pvData              Pointer to user data to set.
 * @param   cbData              Size (in bytes) of user data to set.
 */
int SharedClipboardProviderHostService::Event::SetData(const void *pvData, uint32_t cbData)
{
    Assert(mpvData == NULL);
    Assert(mcbData == 0);

    if (!cbData)
        return VINF_SUCCESS;

    mpvData = RTMemDup(pvData, cbData);
    if (!mpvData)
        return VERR_NO_MEMORY;

    mcbData = cbData;

    return VINF_SUCCESS;
}

/**
 * Waits for an event to get signalled.
 * Will return VERR_NOT_FOUND if no event has been found.
 *
 * @returns VBox status code.
 * @param   uTimeoutMs          Timeout (in ms) to wait.
 */
int SharedClipboardProviderHostService::Event::Wait(RTMSINTERVAL uTimeoutMs)
{
    LogFlowFuncEnter();

    int rc = RTSemEventWait(mEvent, uTimeoutMs);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void *SharedClipboardProviderHostService::Event::DataRaw()
{
    return mpvData;
}

uint32_t SharedClipboardProviderHostService::Event::DataSize()
{
    return mcbData;
}

SharedClipboardProviderHostService::SharedClipboardProviderHostService(void)
    : m_uTimeoutMs(30 * 1000 /* 30s timeout by default */)
{
    LogFlowFuncEnter();
}

SharedClipboardProviderHostService::~SharedClipboardProviderHostService(void)
{
    Reset();
}

int SharedClipboardProviderHostService::ReadDataHdr(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    int rc;

    Event *pEvent = eventGet(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR);
    if (pEvent)
    {
        rc = pEvent->Wait(m_uTimeoutMs);
        if (RT_SUCCESS(rc))
            memcpy(pDataHdr, pEvent->DataRaw(), pEvent->DataSize());
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::ReaaDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk,
                                                      uint32_t *pcbRead, uint32_t fFlags /* = 0 */)
{
    RT_NOREF(pDataHdr, pvChunk, cbChunk, pcbRead, fFlags);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProviderHostService::WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvMeta, uint32_t cbMeta,
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
    LogFlowFuncEnter();

    EventMap::const_iterator itEvent = m_mapEvents.begin();
    while (itEvent != m_mapEvents.end())
    {
        delete itEvent->second;
        m_mapEvents.erase(itEvent);

        itEvent = m_mapEvents.begin();
    }

    int rc2 = eventRegister(VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR);
    AssertRC(rc2);
}

int SharedClipboardProviderHostService::OnRead(PSHAREDCLIPBOARDPROVIDERREADPARMS pParms)
{
    RT_NOREF(pParms);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProviderHostService::OnWrite(PSHAREDCLIPBOARDPROVIDERWRITEPARMS pParms)
{
    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    switch (pParms->u.HostService.uMsg)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR:
        {
            VBOXCLIPBOARDDATAHDR dataHdr;
            rc = VBoxSvcClipboardURIReadDataHdr(pParms->u.HostService.cParms, pParms->u.HostService.paParms,
                                                &dataHdr);
            if (RT_SUCCESS(rc))
            {
                Event *pEvent = eventGet(pParms->u.HostService.uMsg);
                if (pEvent)
                {
                    rc = pEvent->SetData(&dataHdr, sizeof(dataHdr));
                }
            }
            break;
        }
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_CHUNK:
        {
            break;
        }
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_ERROR:
        {

        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        rc = eventSignal(pParms->u.HostService.uMsg);
        if (rc == VERR_NOT_FOUND) /* Having an event is optional, so don't fail here. */
            rc = VINF_SUCCESS;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Registers a new event.
 * Will fail if an event with the same message ID already exists.
 *
 * @returns VBox status code.
 * @param   uMsg                Message ID to register event for.
 */
int SharedClipboardProviderHostService::eventRegister(uint32_t uMsg)
{
    LogFlowFuncEnter();

    int rc;

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent == m_mapEvents.end())
    {
        SharedClipboardProviderHostService::Event *pEvent = new SharedClipboardProviderHostService::Event(uMsg);
        if (pEvent) /** @todo Can this throw? */
        {
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_ALREADY_EXISTS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Unregisters an existing event.
 * Will return VERR_NOT_FOUND if no event has been found.
 *
 * @returns VBox status code.
 * @param   uMsg                Message ID to unregister event for.
 */
int SharedClipboardProviderHostService::eventUnregister(uint32_t uMsg)
{
    LogFlowFuncEnter();

    int rc;

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent != m_mapEvents.end())
    {
        delete itEvent->second;
        m_mapEvents.erase(itEvent);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Signals an event.
 * Will return VERR_NOT_FOUND if no event has been found.
 *
 * @returns VBox status code.
 * @param   uMsg                Message ID of event to signal.
 */
int SharedClipboardProviderHostService::eventSignal(uint32_t uMsg)
{
    LogFlowFuncEnter();

    int rc;

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent != m_mapEvents.end())
    {
        rc = RTSemEventSignal(itEvent->second->mEvent);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the event for a specific message ID.
 *
 * @returns Pointer to event if found, or NULL if no event has been found
 * @param   uMsg                Messagae ID to return event for.
 */
SharedClipboardProviderHostService::Event *SharedClipboardProviderHostService::eventGet(uint32_t uMsg)
{
    LogFlowFuncEnter();

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent != m_mapEvents.end())
        return itEvent->second;

    return NULL;
}

