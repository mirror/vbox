/** @file
 *
 * VBox Guest/VMM/host communication:
 * HGCM - Host-Guest Communication Manager header
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VMMDevHGCM_h__
#define __VMMDevHGCM_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>

#include <VBox/VBoxGuest.h>

#include "VMMDevState.h"

__BEGIN_DECLS
DECLCALLBACK(int) vmmdevHGCMConnect (VMMDevState *pVMMDevState, VMMDevHGCMConnect *pHGCMConnect);
DECLCALLBACK(int) vmmdevHGCMDisconnect (VMMDevState *pVMMDevState, VMMDevHGCMDisconnect *pHGCMDisconnect);
DECLCALLBACK(int) vmmdevHGCMCall (VMMDevState *pVMMDevState, VMMDevHGCMCall *pHGCMCall);

DECLCALLBACK(void) hgcmCompleted (PPDMIHGCMPORT pInterface, int32_t result, PVBOXHGCMCMD pCmdPtr);
__END_DECLS

#endif /* __VMMDevHGCM_h__ */
