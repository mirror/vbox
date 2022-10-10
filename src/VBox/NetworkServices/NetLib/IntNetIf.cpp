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

IntNetIf::IntNetIf()
  : m_hIf(NULL),
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
    AssertReturn(m_hIf == NULL, VERR_GENERAL_FAILURE);

    if (enmTrunkType == kIntNetTrunkType_Invalid)
        enmTrunkType = kIntNetTrunkType_WhateverNone;

    return IntNetR3IfCtxCreate(&m_hIf, strNetwork.c_str(), enmTrunkType,
                               strTrunk.c_str(), _128K /*cbSend*/, _256K /*cbRecv*/,
                               0 /*fFlags*/);
}


/**
 * Set promiscuous mode on the interface.
 */
int
IntNetIf::ifSetPromiscuous(bool fPromiscuous)
{
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfCtxSetPromiscuous(m_hIf, fPromiscuous);
}


/**
 * Obtain R3 send/receive ring buffers for the internal network.
 * Performs VMMR0_DO_INTNET_IF_GET_BUFFER_PTRS.
 * @return iprt status code.
 */
int
IntNetIf::ifGetBuf()
{
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf == NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfCtxQueryBufferPtr(m_hIf, &m_pIfBuf);
}


/**
 * Activate the network interface.
 * Performs VMMR0_DO_INTNET_IF_SET_ACTIVE.
 * @return iprt status code.
 */
int
IntNetIf::ifActivate()
{
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);
    AssertReturn(m_pIfBuf != NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfCtxSetActive(m_hIf, true /*fActive*/);
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
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfWait(m_hIf, cMillies);
}


/**
 * Abort pending ifWait(), prevent any further attempts to wait.
 */
int
IntNetIf::ifAbort()
{
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfWaitAbort(m_hIf);
}


/**
 * Process input available in the receive ring buffer.
 * Feeds input frames to the user callback.
 * @return iprt status code.
 */
int
IntNetIf::ifProcessInput()
{
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
    AssertReturn(m_hIf != NULL, VERR_GENERAL_FAILURE);

    return IntNetR3IfSend(m_hIf);
}


/**
 * Close the connection to the network.
 * Performs VMMR0_DO_INTNET_IF_CLOSE.
 */
int
IntNetIf::ifClose()
{
    if (m_hIf == NULL)
        return VINF_SUCCESS;

    int rc = IntNetR3IfCtxDestroy(m_hIf);
    m_hIf = NULL;
    return rc;
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
    int rc = ifOpen(strNetwork, enmTrunkType, strTrunk);
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
