/** @file
 *
 * VBoxHook -- Global windows hook dll
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
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
