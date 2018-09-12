/** @file
 * Base class for a host-guest service.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include <VBox/log.h>
#include <VBox/hgcmsvc.h>

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/cpp/utils.h>

#include <VBox/HostServices/Service.h>

using namespace HGCM;

Client::Client(uint32_t uClientID)
    : m_uClientID(uClientID)
    , m_uProtocolVer(0)
    , m_fDeferred(false)
{
    RT_ZERO(m_Deferred);
    RT_ZERO(m_SvcCtx);
}

Client::~Client(void)
{

}

/**
 * Completes a guest call by returning the control back to the guest side,
 * together with a status code, internal version.
 *
 * @returns IPRT status code.
 * @param   hHandle             Call handle to complete guest call for.
 * @param   rcOp                Return code to return to the guest side.
 */
int Client::completeInternal(VBOXHGCMCALLHANDLE hHandle, int rcOp)
{
    LogFlowThisFunc(("uClientID=%RU32\n", m_uClientID));

    if (   m_SvcCtx.pHelpers
        && m_SvcCtx.pHelpers->pfnCallComplete)
    {
        m_SvcCtx.pHelpers->pfnCallComplete(hHandle, rcOp);

        reset();
        return VINF_SUCCESS;
    }

    return VERR_NOT_AVAILABLE;
}

/**
 * Resets the client's internal state.
 */
void Client::reset(void)
{
   m_fDeferred = false;

   RT_ZERO(m_Deferred);
}

/**
 * Completes a guest call by returning the control back to the guest side,
 * together with a status code.
 *
 * @returns IPRT status code.
 * @param   hHandle             Call handle to complete guest call for.
 * @param   rcOp                Return code to return to the guest side.
 */
int Client::Complete(VBOXHGCMCALLHANDLE hHandle, int rcOp /* = VINF_SUCCESS */)
{
    return completeInternal(hHandle, rcOp);
}

/**
 * Completes a deferred guest call by returning the control back to the guest side,
 * together with a status code.
 *
 * @returns IPRT status code. VERR_INVALID_STATE if the client is not in deferred mode.
 * @param   rcOp                Return code to return to the guest side.
 */
int Client::CompleteDeferred(int rcOp)
{
    if (m_fDeferred)
    {
        Assert(m_Deferred.hHandle != NULL);

        int rc = completeInternal(m_Deferred.hHandle, rcOp);
        if (RT_SUCCESS(rc))
            m_fDeferred = false;

        return rc;
    }

    AssertMsg(m_fDeferred, ("Client %RU32 is not in deferred mode\n", m_uClientID));
    return VERR_INVALID_STATE;
}

/**
 * Returns the HGCM call handle of the client.
 *
 * @returns HGCM handle.
 */
VBOXHGCMCALLHANDLE Client::GetHandle(void) const
{
    return m_Deferred.hHandle;
}

/**
 * Returns the HGCM call handle of the client.
 *
 * @returns HGCM handle.
 */
uint32_t Client::GetMsgType(void) const
{
    return m_Deferred.uType;
}

uint32_t Client::GetMsgParamCount(void) const
{
    return m_Deferred.cParms;
}

/**
 * Returns the client's (HGCM) ID.
 *
 * @returns The client's (HGCM) ID.
 */
uint32_t Client::GetClientID(void) const
{
    return m_uClientID;
}

/**
 * Returns the client's used protocol version.
 *
 * @returns Protocol version, or 0 if not set.
 */
uint32_t Client::GetProtocolVer(void) const
{
    return m_uProtocolVer;
}

/**
 * Returns whether the client currently is in deferred mode or not.
 *
 * @returns \c True if in deferred mode, \c False if not.
 */
bool Client::IsDeferred(void) const
{
    return m_fDeferred;
}

/**
 * Set the client's status to deferred, meaning that it does not return to the caller
 * until CompleteDeferred() has been called.
 */
void Client::SetDeferred(VBOXHGCMCALLHANDLE hHandle, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowThisFunc(("uClient=%RU32\n", m_uClientID));

    AssertMsg(m_fDeferred == false, ("Client already in deferred mode\n"));
    m_fDeferred = true;

    m_Deferred.hHandle = hHandle;
    m_Deferred.uType   = u32Function;
    m_Deferred.cParms  = cParms;
    m_Deferred.paParms = paParms;
}

/**
 * Sets the client's protocol version. The protocol version is purely optional and bound
 * to a specific HGCM service.
 *
 * @param   uVersion            Version number to set.
 */
void Client::SetProtocolVer(uint32_t uVersion)
{
    m_uProtocolVer = uVersion;
}

/**
 * Sets the HGCM service context.
 *
 * @param   SvcCtx              Service context to set.
 */
void Client::SetSvcContext(const VBOXHGCMSVCTX &SvcCtx)
{
    m_SvcCtx = SvcCtx;
}

/**
 * Sets the deferred parameters to a specific message type and
 * required parameters. That way the client can re-request that message with
 * the right amount of parameters from the service.
 *
 * @returns IPRT status code.
 * @param   uMsg                Message type (number) to set.
 * @param   cParms              Number of parameters the message needs.
 */
int Client::SetDeferredMsgInfo(uint32_t uMsg, uint32_t cParms)
{
    if (m_fDeferred)
    {
        if (m_Deferred.cParms < 2)
            return VERR_INVALID_PARAMETER;

        AssertPtrReturn(m_Deferred.paParms, VERR_BUFFER_OVERFLOW);

        m_Deferred.paParms[0].setUInt32(uMsg);
        m_Deferred.paParms[1].setUInt32(cParms);

        return VINF_SUCCESS;
    }

    AssertFailed();
    return VERR_INVALID_STATE;
}

/**
 * Sets the deferred parameters to a specific message type and
 * required parameters. That way the client can re-request that message with
 * the right amount of parameters from the service.
 *
 * @returns IPRT status code.
 * @param   pMessage            Message to get message type and required parameters from.
 */
int Client::SetDeferredMsgInfo(const Message *pMessage)
{
    AssertPtrReturn(pMessage, VERR_INVALID_POINTER);
    return SetDeferredMsgInfo(pMessage->GetType(), pMessage->GetParamCount());
}

