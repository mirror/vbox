/** @file
 * VBoxGuestLib - Central calls header.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#ifndef ___VBoxGuestR0LibCrOgl_cpp__
#define ___VBoxGuestR0LibCrOgl_cpp__

#include <VBox/VBoxGuestLib.h>

typedef VBGLHGCMHANDLE HVBOXCRCTL;

DECLVBGL(int) vboxCrCtlCreate(HVBOXCRCTL *phCtl);
DECLVBGL(int) vboxCrCtlDestroy(HVBOXCRCTL hCtl);
DECLVBGL(int) vboxCrCtlConConnect(HVBOXCRCTL hCtl, uint32_t *pu32ClientID);
DECLVBGL(int) vboxCrCtlConDisconnect(HVBOXCRCTL hCtl, uint32_t u32ClientID);
DECLVBGL(int) vboxCrCtlConCall(HVBOXCRCTL hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo);
DECLVBGL(int) vboxCrCtlConCallUserData(HVBOXCRCTL hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo);

#endif /* #ifndef ___VBoxGuestR0LibCrOgl_cpp__ */
