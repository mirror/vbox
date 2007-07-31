/*
 * X11 keyboard driver definitions to use it outside wine
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

#define HAVE_X11_XKBLIB_H

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_X11_XKBLIB_H
#include <X11/XKBlib.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

/** Our structure used to return keyboard event information */
typedef struct _WINEKEYBOARDINFO
{
    unsigned short wVk;
    unsigned short wScan;
    unsigned long dwFlags;
    unsigned long time;
} WINEKEYBOARDINFO;

/* Exported definitions */
#ifdef VBOX_HAVE_VISIBILITY_HIDDEN
extern __attribute__((visibility("default"))) void X11DRV_InitKeyboard(Display *dpy);
extern __attribute__((visibility("default"))) void X11DRV_KeyEvent(Display *dpy, XEvent *event,
                                                                   WINEKEYBOARDINFO *wKbInfo);
extern __attribute__((visibility("default"))) int X11DRV_GetKeysymsPerKeycode(void);
#else
extern void X11DRV_InitKeyboard(Display *dpy);
extern void X11DRV_KeyEvent(Display *dpy, XEvent *event,
                                                                   WINEKEYBOARDINFO *wKbInfo);
extern int X11DRV_GetKeysymsPerKeycode(void);
#endif

/* type definitions */
typedef unsigned char BYTE, *LPBYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long BOOL;
typedef DWORD LCID;
typedef void *HWND;

#define FALSE 0
#define TRUE 1

/* debug macros */
inline static void noop(char *arg, ...)
{
}

#define TRACE noop
/* #define TRACE printf */
#define TRACE_(a) noop
/* #define TRACE_(ch) printf */
#define TRACE_ON(a) 0
#define WARN noop
#define ERR noop

/* @@@AH do we need semaphore protection? */
#define wine_tsx11_lock(a)
#define wine_tsx11_unlock(a)

/* global wine defines */
#define HAVE_XKB

/*
 * x11drv.h
 */


/*
 * user.h
 */


/*
 * winuser.h
 */


/* WM_KEYUP/DOWN/CHAR HIWORD(lParam) flags */
#define KF_EXTENDED         0x0100
#define KF_DLGMODE          0x0800
#define KF_MENUMODE         0x1000
#define KF_ALTDOWN          0x2000
#define KF_REPEAT           0x4000
#define KF_UP               0x8000

/* Virtual key codes */
#define VK_LBUTTON          0x01
#define VK_RBUTTON          0x02
#define VK_CANCEL           0x03
#define VK_MBUTTON          0x04
#define VK_XBUTTON1         0x05
#define VK_XBUTTON2         0x06
/*                          0x07  Undefined */
#define VK_BACK             0x08
#define VK_TAB              0x09
/*                          0x0A-0x0B  Undefined */
#define VK_CLEAR            0x0C
#define VK_RETURN           0x0D
/*                          0x0E-0x0F  Undefined */
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_MENU             0x12
#define VK_PAUSE            0x13
#define VK_CAPITAL          0x14

#define VK_KANA             0x15
#define VK_HANGEUL          0x15
#define VK_HANGUL           0x15
#define VK_JUNJA            0x17
#define VK_FINAL            0x18
#define VK_HANJA            0x19
#define VK_KANJI            0x19

/*                          0x1A       Undefined */
#define VK_ESCAPE           0x1B

#define VK_CONVERT          0x1C
#define VK_NONCONVERT       0x1D
#define VK_ACCEPT           0x1E
#define VK_MODECHANGE       0x1F

#define VK_SPACE            0x20
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_END              0x23
#define VK_HOME             0x24
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_SELECT           0x29
#define VK_PRINT            0x2A /* OEM specific in Windows 3.1 SDK */
#define VK_EXECUTE          0x2B
#define VK_SNAPSHOT         0x2C
#define VK_INSERT           0x2D
#define VK_DELETE           0x2E
#define VK_HELP             0x2F
/* VK_0 - VK-9              0x30-0x39  Use ASCII instead */
/*                          0x3A-0x40  Undefined */
/* VK_A - VK_Z              0x41-0x5A  Use ASCII instead */
#define VK_LWIN             0x5B
#define VK_RWIN             0x5C
#define VK_APPS             0x5D
/*                          0x5E Unassigned */
#define VK_SLEEP            0x5F
#define VK_NUMPAD0          0x60
#define VK_NUMPAD1          0x61
#define VK_NUMPAD2          0x62
#define VK_NUMPAD3          0x63
#define VK_NUMPAD4          0x64
#define VK_NUMPAD5          0x65
#define VK_NUMPAD6          0x66
#define VK_NUMPAD7          0x67
#define VK_NUMPAD8          0x68
#define VK_NUMPAD9          0x69
#define VK_MULTIPLY         0x6A
#define VK_ADD              0x6B
#define VK_SEPARATOR        0x6C
#define VK_SUBTRACT         0x6D
#define VK_DECIMAL          0x6E
#define VK_DIVIDE           0x6F
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73
#define VK_F5               0x74
#define VK_F6               0x75
#define VK_F7               0x76
#define VK_F8               0x77
#define VK_F9               0x78
#define VK_F10              0x79
#define VK_F11              0x7A
#define VK_F12              0x7B
#define VK_F13              0x7C
#define VK_F14              0x7D
#define VK_F15              0x7E
#define VK_F16              0x7F
#define VK_F17              0x80
#define VK_F18              0x81
#define VK_F19              0x82
#define VK_F20              0x83
#define VK_F21              0x84
#define VK_F22              0x85
#define VK_F23              0x86
#define VK_F24              0x87
/*                          0x88-0x8F  Unassigned */
#define VK_NUMLOCK          0x90
#define VK_SCROLL           0x91
#define VK_OEM_NEC_EQUAL    0x92
#define VK_OEM_FJ_JISHO     0x92
#define VK_OEM_FJ_MASSHOU   0x93
#define VK_OEM_FJ_TOUROKU   0x94
#define VK_OEM_FJ_LOYA      0x95
#define VK_OEM_FJ_ROYA      0x96
/*                          0x97-0x9F  Unassigned */
/*
 * differencing between right and left shift/control/alt key.
 * Used only by GetAsyncKeyState() and GetKeyState().
 */
#define VK_LSHIFT           0xA0
#define VK_RSHIFT           0xA1
#define VK_LCONTROL         0xA2
#define VK_RCONTROL         0xA3
#define VK_LMENU            0xA4
#define VK_RMENU            0xA5

#define VK_BROWSER_BACK        0xA6
#define VK_BROWSER_FORWARD     0xA7
#define VK_BROWSER_REFRESH     0xA8
#define VK_BROWSER_STOP        0xA9
#define VK_BROWSER_SEARCH      0xAA
#define VK_BROWSER_FAVORITES   0xAB
#define VK_BROWSER_HOME        0xAC
#define VK_VOLUME_MUTE         0xAD
#define VK_VOLUME_DOWN         0xAE
#define VK_VOLUME_UP           0xAF
#define VK_MEDIA_NEXT_TRACK    0xB0
#define VK_MEDIA_PREV_TRACK    0xB1
#define VK_MEDIA_STOP          0xB2
#define VK_MEDIA_PLAY_PAUSE    0xB3
#define VK_LAUNCH_MAIL         0xB4
#define VK_LAUNCH_MEDIA_SELECT 0xB5
#define VK_LAUNCH_APP1         0xB6
#define VK_LAUNCH_APP2         0xB7

/*                          0xB8-0xB9  Unassigned */
#define VK_OEM_1            0xBA
#define VK_OEM_PLUS         0xBB
#define VK_OEM_COMMA        0xBC
#define VK_OEM_MINUS        0xBD
#define VK_OEM_PERIOD       0xBE
#define VK_OEM_2            0xBF
#define VK_OEM_3            0xC0
/*                          0xC1-0xDA  Unassigned */
#define VK_OEM_4            0xDB
#define VK_OEM_5            0xDC
#define VK_OEM_6            0xDD
#define VK_OEM_7            0xDE
#define VK_OEM_8            0xDF
/*                          0xE0       OEM specific */
#define VK_OEM_AX           0xE1  /* "AX" key on Japanese AX keyboard */
#define VK_OEM_102          0xE2  /* "<>" or "\|" on RT 102-key keyboard */
#define VK_ICO_HELP         0xE3  /* Help key on ICO */
#define VK_ICO_00           0xE4  /* 00 key on ICO */
#define VK_PROCESSKEY       0xE5

/*                          0xE6       OEM specific */
/*                          0xE7-0xE8  Unassigned */
/*                          0xE9-0xF5  OEM specific */

#define VK_ATTN             0xF6
#define VK_CRSEL            0xF7
#define VK_EXSEL            0xF8
#define VK_EREOF            0xF9
#define VK_PLAY             0xFA
#define VK_ZOOM             0xFB
#define VK_NONAME           0xFC
#define VK_PA1              0xFD
#define VK_OEM_CLEAR        0xFE

/* ... */

  /* keybd_event flags */
#define KEYEVENTF_EXTENDEDKEY        0x0001
#define KEYEVENTF_KEYUP              0x0002

/* end of winuser.h */


#endif /* __H_KEYBOARD_OUTOFWINE */
