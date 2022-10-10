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

#ifndef VBOX_INCLUDED_SRC_NetLib_IntNetIf_h
#define VBOX_INCLUDED_SRC_NetLib_IntNetIf_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>

#include <iprt/initterm.h>
#include <iprt/cpp/ministring.h>

#include <VBox/sup.h>
#include <VBox/vmm/vmm.h>
#include <VBox/intnet.h>



/**
 * Low-level internal network access helpers to hide away the different variants (R0 SUP or R3 XPC on macOS).
 */
/** Internal networking interface context handle. */
typedef struct INTNETIFCTXINT *INTNETIFCTX;
/** Pointer to an internal networking interface context handle. */
typedef INTNETIFCTX *PINTNETIFCTX;


DECLHIDDEN(int) IntNetR3IfCtxCreate(PINTNETIFCTX phIfCtx, const char *pszNetwork, INTNETTRUNKTYPE enmTrunkType,
                                    const char *pszTrunk, size_t cbSend, size_t cbRecv, uint32_t fFlags);
DECLHIDDEN(int) IntNetR3IfCtxDestroy(INTNETIFCTX hIfCtx);
DECLHIDDEN(int) IntNetR3IfCtxQueryBufferPtr(INTNETIFCTX hIfCtx, PINTNETBUF *ppIfBuf);
DECLHIDDEN(int) IntNetR3IfCtxSetActive(INTNETIFCTX hIfCtx, bool fActive);
DECLHIDDEN(int) IntNetR3IfCtxSetPromiscuous(INTNETIFCTX hIfCtx, bool fPromiscuous);
DECLHIDDEN(int) IntNetR3IfSend(INTNETIFCTX hIfCtx);
DECLHIDDEN(int) IntNetR3IfWait(INTNETIFCTX hIfCtx, uint32_t cMillies);
DECLHIDDEN(int) IntNetR3IfWaitAbort(INTNETIFCTX hIfCtx);


/**
 * Convenience class implementing an IntNet connection.
 */
class IntNetIf
{
public:
    /**
     * User input callback function.
     *
     * @param pvUser    The user specified argument.
     * @param pvFrame   The pointer to the frame data.
     * @param cbFrame   The length of the frame data.
     */
    typedef DECLCALLBACKTYPE(void, FNINPUT,(void *pvUser, void *pvFrame, uint32_t cbFrame));

    /** Pointer to the user input callback function. */
    typedef FNINPUT *PFNINPUT;

    /**
     * User GSO input callback function.
     *
     * @param pvUser    The user specified argument.
     * @param pcGso     The pointer to the GSO context.
     * @param cbFrame   The length of the GSO data.
     */
    typedef DECLCALLBACKTYPE(void, FNINPUTGSO,(void *pvUser, PCPDMNETWORKGSO pcGso, uint32_t cbFrame));

    /** Pointer to the user GSO input callback function. */
    typedef FNINPUTGSO *PFNINPUTGSO;


    /**
     * An output frame in the send ring buffer.
     *
     * Obtained with getOutputFrame().  Caller should copy frame
     * contents to pvFrame and pass the frame structure to ifOutput()
     * to be sent to the network.
     */
    struct Frame {
        PINTNETHDR pHdr;
        void *pvFrame;
    };


private:
    INTNETIFCTX m_hIf;
    PINTNETBUF  m_pIfBuf;

    PFNINPUT m_pfnInput;
    void *m_pvUser;

    PFNINPUTGSO m_pfnInputGSO;
    void *m_pvUserGSO;

public:
    IntNetIf();
    ~IntNetIf();

    int init(const RTCString &strNetwork,
             INTNETTRUNKTYPE enmTrunkType = kIntNetTrunkType_WhateverNone,
             const RTCString &strTrunk = RTCString());
    void uninit();

    int setInputCallback(PFNINPUT pfnInput, void *pvUser);
    int setInputGSOCallback(PFNINPUTGSO pfnInputGSO, void *pvUser);

    int ifSetPromiscuous(bool fPromiscuous = true);

    int ifPump();
    int ifAbort();

    int getOutputFrame(Frame &rFrame, size_t cbFrame);
    int ifOutput(Frame &rFrame);

    int ifClose();

private:
    int ifOpen(const RTCString &strNetwork,
               INTNETTRUNKTYPE enmTrunkType,
               const RTCString &strTrunk);
    int ifGetBuf();
    int ifActivate();

    int ifWait(uint32_t cMillies = RT_INDEFINITE_WAIT);
    int ifProcessInput();

    int ifFlush();
};

#endif /* !VBOX_INCLUDED_SRC_NetLib_IntNetIf_h */
