/*
 * X11 keyboard driver interface
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __H_KEYBOARD_OUTOFWINE
#define __H_KEYBOARD_OUTOFWINE

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

/* debug macros */
#if 0 && defined(DEBUG)
# include <stdio.h>
# define noop printf
#else
#ifdef __cplusplus
inline static void noop(char *, ...)
#else
inline static void noop(char *arg, ...)
#endif
{
}
#endif

#define TRACE noop
/* #define TRACE printf */
#define TRACE_(a) noop
/* #define TRACE_(ch) printf */
#define TRACE_ON(a) 0
#define WARN noop
#define ERR noop

#endif /* __H_KEYBOARD_OUTOFWINE */
