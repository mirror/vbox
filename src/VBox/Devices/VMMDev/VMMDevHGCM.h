/* $Id: */
/** @file
 * VBoxDev - HGCM - Host-Guest Communication Manager, internal header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VMMDev_VMMDevHGCM_h
#define ___VMMDev_VMMDevHGCM_h

#include "VMMDevState.h"

RT_C_DECLS_BEGIN
DECLCALLBACK(int) vmmdevHGCMConnect (VMMDevState *pVMMDevState, VMMDevHGCMConnect *pHGCMConnect, RTGCPHYS GCPtr);
DECLCALLBACK(int) vmmdevHGCMDisconnect (VMMDevState *pVMMDevState, VMMDevHGCMDisconnect *pHGCMDisconnect, RTGCPHYS GCPtr);
DECLCALLBACK(int) vmmdevHGCMCall (VMMDevState *pVMMDevState, VMMDevHGCMCall *pHGCMCall, uint32_t cbHGCMCall, RTGCPHYS GCPtr, bool f64Bits);
DECLCALLBACK(int) vmmdevHGCMCancel (VMMDevState *pVMMDevState, VMMDevHGCMCancel *pHGCMCancel, RTGCPHYS GCPtr);

DECLCALLBACK(void) hgcmCompleted (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmdPtr);

int vmmdevHGCMSaveState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM);
int vmmdevHGCMLoadState(VMMDevState *pVMMDevState, PSSMHANDLE pSSM, uint32_t u32Version);
int vmmdevHGCMLoadStateDone(VMMDevState *pVMMDevState, PSSMHANDLE pSSM);
RT_C_DECLS_END

#endif /* !___VMMDev_VMMDevHGCM_h */

