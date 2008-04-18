/** @file
 *
 * VBoxHook -- Global windows hook dll
 *
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
#ifndef __VBoxHook_h__
#define __VBoxHook_h__

/* custom messages as we must install the hook from the main thread */
#define WM_VBOX_INSTALL_SEAMLESS_HOOK               0x2001
#define WM_VBOX_REMOVE_SEAMLESS_HOOK                0x2002
#define WM_VBOX_SEAMLESS_UPDATE                     0x2003


#define VBOXHOOK_DLL_NAME           "VBoxHook.dll"
#define VBOXHOOK_GLOBAL_EVENT_NAME  "Local\\VBoxHookNotifyEvent"

/* Install the global message hook */
BOOL VBoxInstallHook(HMODULE hDll);

/* Remove the global message hook */
BOOL VBoxRemoveHook();

#endif /* __VBoxHook_h__ */
