/* $Id$ */
/** @file
 * VBoxDev - HGCM - Host-Guest Communication Manager, internal header.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VMMDev_VMMDevHGCM_h
#define ___VMMDev_VMMDevHGCM_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VMMDevState.h"

RT_C_DECLS_BEGIN
int vmmdevHGCMConnect(VMMDevState *pVMMDevState, const VMMDevHGCMConnect *pHGCMConnect, RTGCPHYS GCPtr);
int vmmdevHGCMDisconnect(VMMDevState *pVMMDevState, const VMMDevHGCMDisconnect *pHGCMDisconnect, RTGCPHYS GCPtr);
int vmmdevHGCMCall(VMMDevState *pVMMDevState, const VMMDevHGCMCall *pHGCMCall, uint32_t cbHGCMCall, RTGCPHYS GCPtr,
                   VMMDevRequestType enmRequestType, uint64_t tsArrival, PVMMDEVREQLOCK *ppLock);
int vmmdevHGCMCancel(VMMDevState *pVMMDevState, const VMMDevHGCMCancel *pHGCMCancel, RTGCPHYS GCPtr);
int vmmdevHGCMCancel2(VMMDevState *pVMMDevState, RTGCPHYS GCPtr);

DECLCALLBACK(int)  hgcmCompleted(PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmdPtr);
DECLCALLBACK(bool) hgcmIsCmdRestored(PPDMIHGCMPORT pInterface, PVBOXHGCMCMD pCmd);
DECLCALLBACK(bool) hgcmIsCmdCancelled(PPDMIHGCMPORT pInterface, PVBOXHGCMCMD pCmd);
DECLCALLBACK(uint32_t) hgcmGetRequestor(PPDMIHGCMPORT pInterface, PVBOXHGCMCMD pCmd);
DECLCALLBACK(uint64_t) hgcmGetVMMDevSessionId(PPDMIHGCMPORT pInterface);

int vmmdevHGCMSaveState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM);
int vmmdevHGCMLoadState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM, uint32_t u32Version);
int vmmdevHGCMLoadStateDone(VMMDevState *pVMMDevState);

void vmmdevHGCMDestroy(PVMMDEV pThis);
int  vmmdevHGCMInit(PVMMDEV pThis);
RT_C_DECLS_END

#endif /* !___VMMDev_VMMDevHGCM_h */

