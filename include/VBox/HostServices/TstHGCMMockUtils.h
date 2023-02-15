/** @file
 * TstHGCMMockUtils.h - Utility functions for the HGCM Mocking framework.
 *
 * The utility functions are optional to the actual HGCM Mocking framework and
 * can support testcases which require a more advanced setup.
 *
 * With this one can setup host and guest side threads, which in turn can simulate
 * specific host (i.e. HGCM service) + guest (i.e. like in the Guest Addditions
 * via VbglR3) scenarios.
 *
 * Glossary:
 *
 * Host thread:
 *   - The host thread is used as part of the actual HGCM service being tested and
 *     provides callbacks (@see TSTHGCMUTILSHOSTCALLBACKS) for the unit test.
 * Guest thread:
 *    - The guest thread is used as part of the guest side and mimics
 *      VBoxClient / VBoxTray / VBoxService parts. (i.e. for VbglR3 calls).
 * Task:
 *    - A task is the simplest unit of test execution and used between the guest
 *      and host mocking threads.
 *
 ** @todo Add TstHGCMSimpleHost / TstHGCMSimpleGuest wrappers along those lines:
 *              Callback.pfnOnClientConnected = tstOnHostClientConnected()
 *              TstHGCMSimpleHostInitAndStart(&Callback)
 *              Callback.pfnOnConnected = tstOnGuestConnected()
 *              TstHGCMSimpleClientInitAndStart(&Callback)
 */

/*
 * Copyright (C) 2022-2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_HostServices_TstHGCMMockUtils_h
#define VBOX_INCLUDED_HostServices_TstHGCMMockUtils_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/HostServices/TstHGCMMock.h>

/** Pointer to a HGCM Mock utils context. */
typedef struct TSTHGCMUTILSCTX *PTSTHGCMUTILSCTX;

/**
 * Structure for keeping a HGCM Mock utils host service callback table.
 */
typedef struct TSTHGCMUTILSHOSTCALLBACKS
{
    DECLCALLBACKMEMBER(int, pfnOnClientConnected,(PTSTHGCMUTILSCTX pCtx, PTSTHGCMMOCKCLIENT pClient, void *pvUser));
} TSTHGCMUTILSHOSTCALLBACKS;
/** Pointer to a HGCM Mock utils host callbacks table. */
typedef TSTHGCMUTILSHOSTCALLBACKS *PTSTHGCMUTILSHOSTCALLBACKS;

/**
 * Structure for keeping a generic HGCM Mock utils task.
 *
 * A task is a single test unit / entity.
 */
typedef struct TSTHGCMUTILSTASK
{
    /** Completion event. */
    RTSEMEVENT                    hEvent;
    /** Completion rc.
     *  Set to VERR_IPE_UNINITIALIZED_STATUS if not completed yet. */
    int                           rcCompleted;
    /** Expected completion rc. */
    int                           rcExpected;
    /** Pointer to opaque (testcase-specific) task parameters.
     *  Might be NULL if not needed / used. */
    void                         *pvUser;
} TSTHGCMUTILSTASK;
/** Pointer to a HGCM Mock utils task. */
typedef TSTHGCMUTILSTASK *PTSTHGCMUTILSTASK;

/** Callback function for HGCM Mock utils threads. */
typedef DECLCALLBACKTYPE(int, FNTSTHGCMUTILSTHREAD,(PTSTHGCMUTILSCTX pCtx, void *pvUser));
/** Pointer to a HGCM Mock utils guest thread callback. */
typedef FNTSTHGCMUTILSTHREAD *PFNTSTHGCMUTILSTHREAD;

/**
 * Structure for keeping a HGCM Mock utils context.
 */
typedef struct TSTHGCMUTILSCTX
{
    /** Pointer to the HGCM Mock service instance to use. */
    PTSTHGCMMOCKSVC               pSvc;
    /** Currently we only support one task at a time. */
    TSTHGCMUTILSTASK              Task;
    struct
    {
        RTTHREAD                  hThread;
        volatile bool             fShutdown;
        PFNTSTHGCMUTILSTHREAD     pfnThread;
        void                     *pvUser;
    } Guest;
    struct
    {
        RTTHREAD                  hThread;
        volatile bool             fShutdown;
        TSTHGCMUTILSHOSTCALLBACKS Callbacks;
        void                     *pvUser;
    } Host;
} TSTHGCMUTILSCTX;


/** @name Context handling.
 * @{ */
void TstHGCMUtilsCtxInit(PTSTHGCMUTILSCTX pCtx, PTSTHGCMMOCKSVC pSvc);
/** @} */

/** @name Task handling.
 * @{ */
PTSTHGCMUTILSTASK TstHGCMUtilsTaskGetCurrent(PTSTHGCMUTILSCTX pCtx);
int TstHGCMUtilsTaskInit(PTSTHGCMUTILSTASK pTask);
void TstHGCMUtilsTaskDestroy(PTSTHGCMUTILSTASK pTask);
int TstHGCMUtilsTaskWait(PTSTHGCMUTILSTASK pTask, RTMSINTERVAL msTimeout);
bool TstHGCMUtilsTaskOk(PTSTHGCMUTILSTASK pTask);
bool TstHGCMUtilsTaskCompleted(PTSTHGCMUTILSTASK pTask);
void TstHGCMUtilsTaskSignal(PTSTHGCMUTILSTASK pTask, int rc);
/** @} */

/** @name Threading.
 * @{ */
int TstHGCMUtilsGuestThreadStart(PTSTHGCMUTILSCTX pCtx, PFNTSTHGCMUTILSTHREAD pFnThread, void *pvUser);
int TstHGCMUtilsGuestThreadStop(PTSTHGCMUTILSCTX pCtx);
int TstHGCMUtilsHostThreadStart(PTSTHGCMUTILSCTX pCtx, PTSTHGCMUTILSHOSTCALLBACKS pCallbacks, void *pvUser);
int TstHGCMUtilsHostThreadStop(PTSTHGCMUTILSCTX pCtx);
/** @} */


#endif /* !VBOX_INCLUDED_HostServices_TstHGCMMockUtils_h */

