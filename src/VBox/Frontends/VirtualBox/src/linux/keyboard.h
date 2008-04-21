/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard driver interface
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
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
