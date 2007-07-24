/** @file
 * VBoxSeamless - Seamless windows
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
 *
 */

#ifndef __VBOXSERVICESEAMLESS__H
#define __VBOXSERVICESEAMLESS__H

/* The seamless windows service prototypes */
int                VBoxSeamlessInit     (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
unsigned __stdcall VBoxSeamlessThread   (void *pInstance);
void               VBoxSeamlessDestroy  (const VBOXSERVICEENV *pEnv, void *pInstance);


/* custom messages as we must install the hook from the main thread */
#define WM_VBOX_INSTALL_SEAMLESS_HOOK               0x2001
#define WM_VBOX_REMOVE_SEAMLESS_HOOK                0x2002

void VBoxSeamlessInstallHook();
void VBoxSeamlessRemoveHook();

#endif /* __VBOXSERVICESEAMLESS__H */
