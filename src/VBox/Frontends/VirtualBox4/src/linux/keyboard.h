/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard driver interface
 *
 */

/*
 * Copyright (C) 2008 innotek GmbH
 *
 * This library is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This library is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation, in version 2.1 as it comes in the "COPYING.LIB" file of the
 * VirtualBox OSE distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef __H_KEYBOARD
#define __H_KEYBOARD

#include <X11/Xlib.h>

/* Exported definitions */
#undef CCALL
#ifdef __cplusplus
# define CCALL "C"
#else
# define CCALL
#endif
#ifdef VBOX_HAVE_VISIBILITY_HIDDEN
extern CCALL __attribute__((visibility("default"))) int X11DRV_InitKeyboard(Display *dpy);
extern CCALL __attribute__((visibility("default"))) unsigned X11DRV_KeyEvent(KeyCode code);
#else
extern CCALL int X11DRV_InitKeyboard(Display *dpy);
extern CCALL unsigned X11DRV_KeyEvent(KeyCode code);
#endif

#endif /* __H_KEYBOARD */
