/* $Id$ */
/** @file
 * Shared Clipboard - Provider base class implementation.
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
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include <VBox/log.h>


SharedClipboardProvider::Event::Event(uint32_t uMsg)
    : mMsg(uMsg)
    , mpvData(NULL)
    , mcbData(0)
{
    int rc2 = RTSemEventCreate(&mEvent);
    AssertRC(rc2);
}

SharedClipboardProvider::Event::~Event()
{
    Reset();

    int rc2 = RTSemEventDestroy(mEvent);
    AssertRC(rc2);
}

/**
 * Resets an event by clearing the (allocated) data.
 */
void SharedClipboardProvider::Event::Reset(void)
{
    LogFlowFuncEnter();

    if (mpvData)
    {
        Assert(mcbData);

        RTMemFree(mpvData);
        mpvData = NULL;
    }

    mcbData = 0;
}

/**
 * Sets user data associated to an event.
 *
 * @returns VBox status code.
 * @param   pvData              Pointer to user data to set.
 * @param   cbData              Size (in bytes) of user data to set.
 */
int SharedClipboardProvider::Event::SetData(const void *pvData, uint32_t cbData)
{
    Reset();

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
int SharedClipboardProvider::Event::Wait(RTMSINTERVAL uTimeoutMs)
{
    LogFlowFunc(("mMsg=%RU32, uTimeoutMs=%RU32\n", mMsg, uTimeoutMs));

    int rc = RTSemEventWait(mEvent, uTimeoutMs);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Lets the caller adopt (transfer ownership) the returned event data.
 * The caller is responsible of free'ing the data accordingly.
 *
 * @returns Pointer to the adopted event's raw data.
 */
void *SharedClipboardProvider::Event::DataAdopt(void)
{
    void *pvData = mpvData;

    mpvData = NULL;
    mcbData = 0;

    return pvData;
}

/**
 * Returns the event's (raw) data (mutable).
 *
 * @returns Pointer to the event's raw data.
 */
void *SharedClipboardProvider::Event::DataRaw(void)
{
    return mpvData;
}

/**
 * Returns the event's data size (in bytes).
 *
 * @returns Data size (in bytes).
 */
uint32_t SharedClipboardProvider::Event::DataSize(void)
{
    return mcbData;
}

SharedClipboardProvider::SharedClipboardProvider(void)
    : m_cRefs(0)
    , m_uTimeoutMs(30 * 1000 /* 30s timeout by default */)
{
    LogFlowFuncEnter();
}

SharedClipboardProvider::~SharedClipboardProvider(void)
{
    LogFlowFuncEnter();

    Assert(m_cRefs == 0);

    eventUnregisterAll();
}

/**
 * Creates a Shared Clipboard provider.
 *
 * @returns New Shared Clipboard provider instance.
 * @param   pCtx                Pointer to creation context.
 */
/* static */
SharedClipboardProvider *SharedClipboardProvider::Create(PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCtx)
{
    AssertPtrReturn(pCtx, NULL);

    SharedClipboardProvider *pProvider = NULL;

    switch (pCtx->enmSource)
    {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_GUEST
        case SHAREDCLIPBOARDPROVIDERSOURCE_VBGLR3:
            pProvider = new SharedClipboardProviderVbglR3(pCtx->u.VbglR3.uClientID);
            break;
#endif

#ifdef VBOX_WITH_SHARED_CLIPBOARD_HOST
        case SHAREDCLIPBOARDPROVIDERSOURCE_HOSTSERVICE:
            pProvider = new SharedClipboardProviderHostService(pCtx->u.HostService.pArea);
            break;
#endif
        default:
            AssertFailed();
            break;
    }

    if (pProvider)
        pProvider->SetCallbacks(pCtx->pCallbacks);

    return pProvider;
}

/**
 * Adds a reference to a Shared Clipboard provider.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardProvider::AddRef(void)
{
    LogFlowFuncEnter();
    return ASMAtomicIncU32(&m_cRefs);
}

/**
 * Removes a reference from a Shared Clipboard cache.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardProvider::Release(void)
{
    LogFlowFuncEnter();
    Assert(m_cRefs);
    return ASMAtomicDecU32(&m_cRefs);
}

/**
 * Sets or unsets the callback table to be used for a clipboard provider.
 *
 * @returns VBox status code.
 * @param   pCallbacks          Pointer to callback table to set. Specify NULL to unset existing callbacks.
 */
void SharedClipboardProvider::SetCallbacks(PSHAREDCLIPBOARDPROVIDERCALLBACKS pCallbacks)
{
    /* pCallbacks might be NULL to unset callbacks. */

    LogFlowFunc(("pCallbacks=%p\n", pCallbacks));

    if (pCallbacks)
    {
        m_Callbacks = *pCallbacks;
    }
    else
        RT_ZERO(m_Callbacks);
}

/*
 * Stubs.
 */

int SharedClipboardProvider::ReadDataHdr(PVBOXCLIPBOARDDATAHDR *ppDataHdr)
{
    RT_NOREF(ppDataHdr);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProvider::WriteDataHdr(const PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProvider::ReadDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, void *pvChunk, uint32_t cbChunk,
                                           uint32_t fFlags /* = 0 */, uint32_t *pcbRead /* = NULL*/)
{
    RT_NOREF(pDataHdr, pvChunk, cbChunk, fFlags, pcbRead);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProvider::WriteDataChunk(const PVBOXCLIPBOARDDATAHDR pDataHdr, const void *pvChunk, uint32_t cbChunk,
                                            uint32_t fFlags /* = 0 */, uint32_t *pcbWritten /* = NULL */)
{
    RT_NOREF(pDataHdr, pvChunk, cbChunk, fFlags, pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}

int SharedClipboardProvider::ReadDirectory(PVBOXCLIPBOARDDIRDATA *ppDirData)
{
    RT_NOREF(ppDirData);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::WriteDirectory(const PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_NOREF(pDirData);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::ReadFileHdr(PVBOXCLIPBOARDFILEHDR *ppFileHdr)
{
    RT_NOREF(ppFileHdr);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::WriteFileHdr(const PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_NOREF(pFileHdr);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::ReadFileData(void *pvData, uint32_t cbData, uint32_t fFlags /* = 0 */,
                                          uint32_t *pcbRead /* = NULL */)
{
    RT_NOREF(pvData, cbData, fFlags, pcbRead);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::WriteFileData(void *pvData, uint32_t cbData, uint32_t fFlags /* = 0 */,
                                           uint32_t *pcbWritten /* = NULL */)
{
    RT_NOREF(pvData, cbData, fFlags, pcbWritten);

    LogFlowFuncEnter();

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void SharedClipboardProvider::Reset(void)
{
}

int SharedClipboardProvider::OnRead(PSHAREDCLIPBOARDPROVIDERREADPARMS pParms)
{
    RT_NOREF(pParms);

    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardProvider::OnWrite(PSHAREDCLIPBOARDPROVIDERWRITEPARMS pParms)
{
    RT_NOREF(pParms);

    int rc = VERR_NOT_IMPLEMENTED;

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
int SharedClipboardProvider::eventRegister(uint32_t uMsg)
{
    LogFlowFunc(("uMsg=%RU32\n", uMsg));

    int rc;

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent == m_mapEvents.end())
    {
        SharedClipboardProvider::Event *pEvent = new SharedClipboardProvider::Event(uMsg);
        if (pEvent) /** @todo Can this throw? */
        {
            m_mapEvents[uMsg] = pEvent; /** @todo Ditto. */

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
int SharedClipboardProvider::eventUnregister(uint32_t uMsg)
{
    LogFlowFunc(("uMsg=%RU32\n", uMsg));

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
 * Unregisters all registered events.
 *
 * @returns VBox status code.
 */
int SharedClipboardProvider::eventUnregisterAll(void)
{
    int rc = VINF_SUCCESS;

    LogFlowFuncEnter();

    EventMap::const_iterator itEvent = m_mapEvents.begin();
    while (itEvent != m_mapEvents.end())
    {
        delete itEvent->second;
        m_mapEvents.erase(itEvent);

        itEvent = m_mapEvents.begin();
    }

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
int SharedClipboardProvider::eventSignal(uint32_t uMsg)
{
    LogFlowFunc(("uMsg=%RU32\n", uMsg));

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
SharedClipboardProvider::Event *SharedClipboardProvider::eventGet(uint32_t uMsg)
{
    LogFlowFuncEnter();

    EventMap::const_iterator itEvent = m_mapEvents.find(uMsg);
    if (itEvent != m_mapEvents.end())
        return itEvent->second;

    return NULL;
}

/**
 * Waits for an event to get signalled and optionally returns related event data on success.
 *
 * @returns VBox status code.
 * @param   uMsg                Message ID of event to wait for.
 * @param   pfnCallback         Callback to use before waiting for event. Specify NULL if not needed.
 * @param   uTimeoutMs          Timeout (in ms) to wait for.
 * @param   ppvData             Where to store the related event data. Optioanl.
 * @param   pcbData             Where to store the size (in bytes) of the related event data. Optioanl.
 */
int SharedClipboardProvider::eventWait(uint32_t uMsg, PFNSSHAREDCLIPBOARDPROVIDERCALLBACK pfnCallback,
                                       RTMSINTERVAL uTimeoutMs, void **ppvData, uint32_t *pcbData /* = NULL */)
{
    LogFlowFunc(("uMsg=%RU32, uTimeoutMs=%RU32\n", uMsg, uTimeoutMs));

    int rc = VINF_SUCCESS;

    if (pfnCallback)
    {
        SHAREDCLIPBOARDPROVIDERCALLBACKDATA data = { this, m_Callbacks.pvUser };
        rc = pfnCallback(&data);
    }

    if (RT_SUCCESS(rc))
    {
        Event *pEvent = eventGet(uMsg);
        if (pEvent)
        {
            rc = pEvent->Wait(m_uTimeoutMs);
            if (RT_SUCCESS(rc))
            {
                if (pcbData)
                    *pcbData = pEvent->DataSize();

                if (ppvData)
                    *ppvData = pEvent->DataAdopt();

                pEvent->Reset();
            }
        }
        else
            rc = VERR_NOT_FOUND;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

