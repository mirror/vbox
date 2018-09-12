/* $Id$ */
/** @file
 * Drag and Drop manager: Handling of DnD messages on the host side.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
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

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND

#include "dndmanager.h"

#include <VBox/log.h>
#include <iprt/file.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/uri.h>


/*********************************************************************************************************************************
*   DnDManager                                                                                                                   *
*********************************************************************************************************************************/

/**
 * Adds a DnD message to the manager's queue.
 *
 * @returns IPRT status code.
 * @param   pMsg                Pointer to DnD message to add. The queue then owns the pointer.
 * @param   fAppend             Whether to append or prepend the message to the queue.
 */
int DnDManager::AddMsg(DnDMessage *pMsg, bool fAppend /* = true */)
{
    AssertPtrReturn(pMsg, VERR_INVALID_POINTER);

    LogFlowFunc(("uMsg=%RU32, cParms=%RU32, fAppend=%RTbool\n", pMsg->GetType(), pMsg->GetParamCount(), fAppend));

    if (fAppend)
        m_queueMsg.append(pMsg);
    else
        m_queueMsg.prepend(pMsg);

    /** @todo Catch / handle OOM? */

    return VINF_SUCCESS;
}

/**
 * Adds a DnD message to the manager's queue.
 *
 * @returns IPRT status code.
 * @param   uMsg                Type (function number) of message to add.
 * @param   cParms              Number of parameters of message to add.
 * @param   paParms             Array of parameters of message to add.
 * @param   fAppend             Whether to append or prepend the message to the queue.
 */
int DnDManager::AddMsg(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fAppend /* = true */)
{
    int rc;

    try
    {
        DnDMessage *pMsg = new DnDGenericMessage(uMsg, cParms, paParms);
        rc = AddMsg(pMsg, fAppend);
    }
    catch(std::bad_alloc &)
    {
        rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Retrieves information about the next message in the queue.
 *
 * @returns IPRT status code. VERR_NO_DATA if no next message is available.
 * @param   puType              Where to store the message type.
 * @param   pcParms             Where to store the message parameter count.
 */
int DnDManager::GetNextMsgInfo(uint32_t *puType, uint32_t *pcParms)
{
    AssertPtrReturn(puType, VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    int rc;

    if (m_queueMsg.isEmpty())
    {
        rc = VERR_NO_DATA;
    }
    else
    {
        DnDMessage *pMsg = m_queueMsg.first();
        AssertPtr(pMsg);

        *puType  = pMsg->GetType();
        *pcParms = pMsg->GetParamCount();

        rc = VINF_SUCCESS;
    }

    LogFlowFunc(("Returning puMsg=%RU32, pcParms=%RU32, rc=%Rrc\n", *puType, *pcParms, rc));
    return rc;
}

/**
 * Retrieves the next queued up message and removes it from the queue on success.
 * Will return VERR_NO_DATA if no next message is available.
 *
 * @returns IPRT status code.
 * @param   uMsg                Message type to retrieve.
 * @param   cParms              Number of parameters the \@a paParms array can store.
 * @param   paParms             Where to store the message parameters.
 */
int DnDManager::GetNextMsg(uint32_t uMsg, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("uMsg=%RU32, cParms=%RU32\n", uMsg, cParms));

    /* Check for pending messages in our queue. */
    if (m_queueMsg.isEmpty())
        return VERR_NO_DATA;

    /* Get the current message. */
    DnDMessage *pMsg = m_queueMsg.first();
    AssertPtr(pMsg);

    m_queueMsg.removeFirst(); /* Remove the current message from the queue. */

    /* Fetch the current message info. */
    int rc = pMsg->GetData(uMsg, cParms, paParms);

    /*
     * If there was an error handling the current message or the user has canceled
     * the operation, we need to cleanup all pending events and inform the progress
     * callback about our exit.
     */
    if (RT_FAILURE(rc))
    {
        /* Clear any pending messages. */
        Reset();

        /* Create a new cancel message to inform the guest + call
         * the host whether the current transfer was canceled or aborted
         * due to an error. */
        try
        {
            if (rc == VERR_CANCELLED)
                LogFlowFunc(("Operation was cancelled\n"));

            DnDHGCancelMessage *pMsgCancel = new DnDHGCancelMessage();

            int rc2 = AddMsg(pMsgCancel, false /* Prepend */);
            AssertRC(rc2);

            if (m_pfnProgressCallback)
            {
                LogFlowFunc(("Notifying host about aborting operation (%Rrc) ...\n", rc));
                m_pfnProgressCallback(  rc == VERR_CANCELLED
                                      ? DragAndDropSvc::DND_PROGRESS_CANCELLED
                                      : DragAndDropSvc::DND_PROGRESS_ERROR,
                                      100 /* Percent */, rc,
                                      m_pvProgressUser);
            }
        }
        catch(std::bad_alloc &)
        {
            rc = VERR_NO_MEMORY;
        }
    }

    LogFlowFunc(("Message processed with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Resets the manager by clearing the message queue and internal state.
 */
void DnDManager::Reset(void)
{
    LogFlowFuncEnter();

    while (!m_queueMsg.isEmpty())
    {
        delete m_queueMsg.last();
        m_queueMsg.removeLast();
    }
}

