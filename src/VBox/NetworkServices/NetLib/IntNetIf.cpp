/* $Id$ */
/** @file
 * IntNetIf - Convenience class implementing an IntNet connection.
 */

/*
 * Copyright (C) 2009-2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "IntNetIf.h"

#include <iprt/path.h>

#include <VBox/intnetinline.h>
#include <VBox/vmm/pdmnetinline.h>

#define CALL_VMMR0(op, req) \
    (SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, (op), 0, &(req).Hdr))



IntNetIf::IntNetIf()
  : m_pSession(NIL_RTR0PTR),
    m_hIf(INTNET_HANDLE_INVALID),
    m_pIfBuf(NULL),
    m_pfnInput(NULL),
    m_pvUser(NULL),
    m_pfnInputGSO(NULL),
    m_pvUserGSO(NULL)
{
    return;
}


IntNetIf::~IntNetIf()
{
    uninit();
}



/*
 * SUPDrv and VMM initialization and finalization.
 */

int
IntNetIf::r3Init()
{
    AssertReturn(m_pSession == NIL_RTR0PTR, VERR_GENERAL_FAILURE);

    int rc = SUPR3Init(&m_pSession);
    return rc;
}


void
IntNetIf::r3Fini()
{
    if (m_pSession == NIL_RTR0PTR)
        return;

    SUPR3Term();
    m_pSession = NIL_RTR0PTR;
}


int
IntNetIf::vmmInit()
{
    char szPathVMMR0[RTPATH_MAX];
    int rc;

    rc = RTPathExecDir(szPathVMMR0, sizeof(szPathVMMR0));
    if (RT_FAILURE(rc))
        return rc;

    rc = RTPathAppend(szPathVMMR0, sizeof(szPathVMMR0), "VMMR0.r0");
    if (RT_FAILURE(rc))
        return rc;

    rc = SUPR3LoadVMM(szPathVMMR0, /* :pErrInfo */ NULL);
    return rc;
}



/*
 * Wrappers for VMM ioctl requests and low-level intnet operations.
 */

/**
 * Open the specified internal network.
 * Perform VMMR0_DO_INTNET_OPEN.
 *
 * @param strNetwork    The name of the network.
 * @param enmTrunkType  The trunk type.
 * @param strTrunk      The trunk name, its meaning is specific to the type.
 * @return              iprt status code.
 */
int
IntNetIf::ifOpen(const RTCString &strNetwork,
                 INTNETTRUNKTYPE enmTrunkType,
                 const RTCString &strTrunk)
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf == INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    INTNETOPENREQ OpenReq;
    RT_ZERO(OpenReq);

    OpenReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    OpenReq.Hdr.cbReq = sizeof(OpenReq);
    OpenReq.pSession = m_pSession;

    int rc = RTStrCopy(OpenReq.szNetwork, sizeof(OpenReq.szNetwork), strNetwork.c_str());
    AssertRCReturn(rc, rc);

    rc = RTStrCopy(OpenReq.szTrunk, sizeof(OpenReq.szTrunk), strTrunk.c_str());
    AssertRCReturn(rc, rc);

    if (enmTrunkType != kIntNetTrunkType_Invalid)
        OpenReq.enmTrunkType = enmTrunkType;
    else
        OpenReq.enmTrunkType = kIntNetTrunkType_WhateverNone;

    OpenReq.fFlags = 0;
    OpenReq.cbSend = _128K;
    OpenReq.cbRecv = _256K;

    OpenReq.hIf = INTNET_HANDLE_INVALID;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_OPEN, OpenReq);
    if (RT_FAILURE(rc))
        return rc;

    m_hIf = OpenReq.hIf;
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    return VINF_SUCCESS;
}


/**
 * Set promiscuous mode on the interface.
 */
int
IntNetIf::ifSetPromiscuous(bool fPromiscuous)
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    INTNETIFSETPROMISCUOUSMODEREQ SetPromiscuousModeReq;
    int rc;

    SetPromiscuousModeReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SetPromiscuousModeReq.Hdr.cbReq = sizeof(SetPromiscuousModeReq);
    SetPromiscuousModeReq.pSession = m_pSession;
    SetPromiscuousModeReq.hIf = m_hIf;

    SetPromiscuousModeReq.fPromiscuous = fPromiscuous;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE, SetPromiscuousModeReq);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * Obtain R3 send/receive ring buffers for the internal network.
 * Performs VMMR0_DO_INTNET_IF_GET_BUFFER_PTRS.
 * @return iprt status code.
 */
int
IntNetIf::ifGetBuf()
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf == NULL, VERR_GENERAL_FAILURE);

    INTNETIFGETBUFFERPTRSREQ GetBufferPtrsReq;
    int rc;

    GetBufferPtrsReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    GetBufferPtrsReq.Hdr.cbReq = sizeof(GetBufferPtrsReq);
    GetBufferPtrsReq.pSession = m_pSession;
    GetBufferPtrsReq.hIf = m_hIf;

    GetBufferPtrsReq.pRing0Buf = NIL_RTR0PTR;
    GetBufferPtrsReq.pRing3Buf = NULL;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_GET_BUFFER_PTRS, GetBufferPtrsReq);
    if (RT_FAILURE(rc))
        return rc;

    m_pIfBuf = GetBufferPtrsReq.pRing3Buf;
    AssertReturn(m_pIfBuf != NULL, VERR_GENERAL_FAILURE);

    return VINF_SUCCESS;
}


/**
 * Activate the network interface.
 * Performs VMMR0_DO_INTNET_IF_SET_ACTIVE.
 * @return iprt status code.
 */
int
IntNetIf::ifActivate()
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf != NULL, VERR_GENERAL_FAILURE);

    INTNETIFSETACTIVEREQ ActiveReq;
    int rc;

    ActiveReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    ActiveReq.Hdr.cbReq = sizeof(ActiveReq);
    ActiveReq.pSession = m_pSession;
    ActiveReq.hIf = m_hIf;

    ActiveReq.fActive = 1;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_SET_ACTIVE, ActiveReq);
    return rc;
}


/**
 * Wait for input frame(s) to become available in the receive ring
 * buffer.  Performs VMMR0_DO_INTNET_IF_WAIT.
 *
 * @param cMillies      Timeout, defaults to RT_INDEFINITE_WAIT.
 * @return              iprt status code.
 */
int
IntNetIf::ifWait(uint32_t cMillies)
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    INTNETIFWAITREQ WaitReq;
    int rc;

    WaitReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    WaitReq.Hdr.cbReq = sizeof(WaitReq);
    WaitReq.pSession = m_pSession;
    WaitReq.hIf = m_hIf;

    WaitReq.cMillies = cMillies;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_WAIT, WaitReq);
    return rc;
}


/**
 * Abort pending ifWait(), prevent any further attempts to wait.
 */
int
IntNetIf::ifAbort()
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    INTNETIFABORTWAITREQ AbortReq;
    int rc;

    AbortReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    AbortReq.Hdr.cbReq = sizeof(AbortReq);
    AbortReq.pSession = m_pSession;
    AbortReq.hIf = m_hIf;

    AbortReq.fNoMoreWaits = true;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_ABORT_WAIT, AbortReq);
    return rc;
}


/**
 * Process input available in the receive ring buffer.
 * Feeds input frames to the user callback.
 * @return iprt status code.
 */
int
IntNetIf::ifProcessInput()
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf != NULL, VERR_GENERAL_FAILURE);
    AssertReturn(m_pfnInput != NULL, VERR_GENERAL_FAILURE);

    PCINTNETHDR pHdr = IntNetRingGetNextFrameToRead(&m_pIfBuf->Recv);
    while (pHdr)
    {
        const uint8_t u8Type = pHdr->u8Type;
        void *pvSegFrame;
        uint32_t cbSegFrame;

        if (u8Type == INTNETHDR_TYPE_FRAME)
        {
            pvSegFrame = IntNetHdrGetFramePtr(pHdr, m_pIfBuf);
            cbSegFrame = pHdr->cbFrame;

            /* pass the frame to the user callback */
            (*m_pfnInput)(m_pvUser, pvSegFrame, cbSegFrame);
        }
        else if (u8Type == INTNETHDR_TYPE_GSO)
        {
            size_t cbGso = pHdr->cbFrame;
            size_t cbFrame = cbGso - sizeof(PDMNETWORKGSO);

            PCPDMNETWORKGSO pcGso = IntNetHdrGetGsoContext(pHdr, m_pIfBuf);
            if (PDMNetGsoIsValid(pcGso, cbGso, cbFrame))
            {
                if (m_pfnInputGSO != NULL)
                {
                    /* pass the frame to the user GSO input callback if set */
                    (*m_pfnInputGSO)(m_pvUserGSO, pcGso, (uint32_t)cbFrame);
                }
                else
                {
                    const uint32_t cSegs = PDMNetGsoCalcSegmentCount(pcGso, cbFrame);
                    for (uint32_t i = 0; i < cSegs; ++i)
                    {
                        uint8_t abHdrScratch[256];
                        pvSegFrame = PDMNetGsoCarveSegmentQD(pcGso, (uint8_t *)(pcGso + 1), cbFrame,
                                                             abHdrScratch,
                                                             i, cSegs,
                                                             &cbSegFrame);

                        /* pass carved frames to the user input callback */
                        (*m_pfnInput)(m_pvUser, pvSegFrame, (uint32_t)cbSegFrame);
                    }
                }
            }
        }

        /* advance to the next input frame */
        IntNetRingSkipFrame(&m_pIfBuf->Recv);
        pHdr = IntNetRingGetNextFrameToRead(&m_pIfBuf->Recv);
    }

    return VINF_SUCCESS;
}


/**
 * Flush output frames from the send ring buffer to the network.
 * Performs VMMR0_DO_INTNET_IF_SEND.
 */
int
IntNetIf::ifFlush()
{
    AssertReturn(m_pSession != NIL_RTR0PTR, VERR_GENERAL_FAILURE);
    AssertReturn(m_hIf != INTNET_HANDLE_INVALID, VERR_GENERAL_FAILURE);

    INTNETIFSENDREQ SendReq;
    int rc;

    SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SendReq.Hdr.cbReq = sizeof(SendReq);
    SendReq.pSession = m_pSession;
    SendReq.hIf = m_hIf;

    rc = CALL_VMMR0(VMMR0_DO_INTNET_IF_SEND, SendReq);
    return rc;
}


/**
 * Close the connection to the network.
 * Performs VMMR0_DO_INTNET_IF_CLOSE.
 */
int
IntNetIf::ifClose()
{
    if (m_hIf == INTNET_HANDLE_INVALID)
        return VINF_SUCCESS;

    INTNETIFCLOSEREQ CloseReq;

    CloseReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    CloseReq.Hdr.cbReq = sizeof(CloseReq);
    CloseReq.pSession = m_pSession;
    CloseReq.hIf = m_hIf;

    m_hIf = INTNET_HANDLE_INVALID;
    m_pIfBuf = NULL;

    CALL_VMMR0(VMMR0_DO_INTNET_IF_CLOSE, CloseReq);
    return VINF_SUCCESS;
}



/*
 * Public high-level user interface.
 */

/**
 * Connect to the specified internal network.
 *
 * @param strNetwork    The name of the network.
 * @param enmTrunkType  The trunk type.  Defaults to kIntNetTrunkType_WhateverNone.
 * @param strTrunk      The trunk name, its meaning is specific to the type.
 *                      Defaults to an empty string.
 * @return              iprt status code.
 */
int
IntNetIf::init(const RTCString &strNetwork,
               INTNETTRUNKTYPE enmTrunkType,
               const RTCString &strTrunk)
{
    int rc;

    rc = r3Init();
    if (RT_FAILURE(rc))
        return rc;

    rc = vmmInit();
    if (RT_FAILURE(rc))
        return rc;

    rc = ifOpen(strNetwork, enmTrunkType, strTrunk);
    if (RT_FAILURE(rc))
        return rc;

    rc = ifGetBuf();
    if (RT_FAILURE(rc))
        return rc;

    rc = ifActivate();
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


void
IntNetIf::uninit()
{
    ifClose();
    r3Fini();
}


/**
 * Set the user input callback function.
 *
 * @param pfnInput      User input callback.
 * @param pvUser        The user specified argument to the callback.
 * @return              iprt status code.
 */
int
IntNetIf::setInputCallback(PFNINPUT pfnInput, void *pvUser)
{
    AssertReturn(pfnInput != NULL, VERR_INVALID_STATE);

    m_pfnInput = pfnInput;
    m_pvUser = pvUser;
    return VINF_SUCCESS;
}


/**
 * Set the user GSO input callback function.
 *
 * @param pfnInputGSO   User input callback.
 * @param pvUserGSO     The user specified argument to the callback.
 * @return              iprt status code.
 */
int
IntNetIf::setInputGSOCallback(PFNINPUTGSO pfnInputGSO, void *pvUserGSO)
{
    AssertReturn(pfnInputGSO != NULL, VERR_INVALID_STATE);

    m_pfnInputGSO = pfnInputGSO;
    m_pvUserGSO = pvUserGSO;
    return VINF_SUCCESS;
}


/**
 * Process incoming packets forever.
 *
 * User call this method on its receive thread.  The packets are
 * passed to the user inpiut callbacks.  If the GSO input callback is
 * not registered, a GSO input frame is carved into normal frames and
 * those frames are passed to the normal input callback.
 */
int
IntNetIf::ifPump()
{
    AssertReturn(m_pfnInput != NULL, VERR_GENERAL_FAILURE);

    int rc;
    for (;;)
    {
        rc = ifWait();
        if (RT_SUCCESS(rc) || rc == VERR_INTERRUPTED || rc == VERR_TIMEOUT)
            ifProcessInput();
        else
            break;
    }
    return rc;
}


int
IntNetIf::getOutputFrame(IntNetIf::Frame &rFrame, size_t cbFrame)
{
    int rc;

    rc = IntNetRingAllocateFrame(&m_pIfBuf->Send, (uint32_t)cbFrame,
                                 &rFrame.pHdr, &rFrame.pvFrame);
    return rc;
}


int
IntNetIf::ifOutput(IntNetIf::Frame &rFrame)
{
    int rc;

    IntNetRingCommitFrame(&m_pIfBuf->Send, rFrame.pHdr);

    rc = ifFlush();
    return rc;
}
