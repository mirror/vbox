/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * X11 keyboard driver translation tables
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
                
#ifndef ___VBox_keyboard_tables_h
# define ___VBox_keyboard_tables_h

#define MAIN_LEN 50
static const unsigned main_key_scan[MAIN_LEN] =
{
 /* `    1    2    3    4    5    6    7    8    9    0    -    = */
   0x29,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,
 /* q    w    e    r    t    y    u    i    o    p    [    ] */
   0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,
 /* a    s    d    f    g    h    j    k    l    ;    '    \ */
   0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2B,
 /* z    x    c    v    b    n    m    ,    .    / */
   0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,
 /* 102nd key Brazilian key Yen */
   0x56,      0x73,         0x7d
};

/*** DEFINE YOUR NEW LANGUAGE-SPECIFIC MAPPINGS BELOW, SEE EXISTING TABLES */

/* order: Normal, Shift */
/* We recommend you write just what is guaranteed to be correct (i.e. what's
   written on the keycaps), not the bunch of special characters behind AltGr
   and Shift-AltGr if it can vary among different X servers */
/* Remember to also add your new table to the layout index table below! */

#include "keyboard-layouts.h"

/*** Layout table. Add your keyboard mappings to this list */
static const struct {
    const char *comment;
    const char (*key)[MAIN_LEN][2];
} main_key_tab[]={
#include "keyboard-list.h"
 {NULL, NULL} /* sentinel */
};

/* Scan code table for non-character keys */

static const unsigned nonchar_key_scan[256] =
{
    /* unused */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF00 */
    /* special keys */
    0x0E, 0x0F, 0x00, /*?*/ 0, 0x00, 0x1C, 0x00, 0x00,           /* FF08 */
    0x00, 0x00, 0x00, 0x45, 0x46, 0x00, 0x00, 0x00,              /* FF10 */
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,              /* FF18 */
    /* additional Japanese keys */
    0x00, 0x00, 0x7b, 0x79, 0x00, 0x00, 0x00, 0x70,              /* FF20 */
    0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF28 */
    /* additional Korean keys */
    0x00, 0xf2, 0x00, 0x00, 0xf1, 0x00, 0x00, 0x00,              /* FF30 */
    /* unused */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF38 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF40 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF48 */
    /* cursor keys */
    0x147, 0x14B, 0x148, 0x14D, 0x150, 0x149, 0x151, 0x14F,      /* FF50 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF58 */
    /* misc keys */
    /*?*/ 0, 0x137, /*?*/ 0, 0x152, 0x00, 0x00, 0x00, 0x15D,     /* FF60 */
    /*?*/ 0, /*?*/ 0, 0x38, 0x146, 0x00, 0x00, 0x00, 0x00,       /* FF68 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF70 */
    /* keypad keys */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x138, 0x45,             /* FF78 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF80 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x11C, 0x00, 0x00,             /* FF88 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x4B, 0x48,              /* FF90 */
    0x4D, 0x50, 0x49, 0x51, 0x4F, 0x4C, 0x52, 0x53,              /* FF98 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFA0 */
    0x00, 0x00, 0x37, 0x4E, 0x53, 0x4A, 0x7e, 0x135,             /* FFA8 */
    0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47,              /* FFB0 */
    0x48, 0x49, 0x00, 0x00, 0x00, 0x00,                          /* FFB8 */
    /* function keys */
    0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,              /* FFC0 */
    0x57, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFC8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFD0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFD8 */
    /* modifier keys */
    0x00, 0x2A, 0x36, 0x1D, 0x11D, 0x3A, 0x00, 0x38,             /* FFE0 */
    0x138, 0x38, 0x138, 0x15B, 0x15C, 0x00, 0x00, 0x00,          /* FFE8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFF0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x153              /* FFF8 */
};

/* This list was put together using /usr/include/X11/XF86keysym.h and
   http://www.win.tue.nl/~aeb/linux/kbd/scancodes-6.html.  It has not yet
   been extensively tested.  The scancodes are those used by MicroSoft
   keyboards. */
static const unsigned xfree86_vendor_key_scan[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF00 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF08 */
 /*    Vol-   Mute   Vol+   Play   Stop   Track- Track+ */
    0, 0x12e, 0x120, 0x130, 0x122, 0x124, 0x110, 0x119,         /* 1008FF10 */
 /* Home   E-mail    Search */
    0x132, 0x16c, 0, 0x165, 0, 0, 0, 0,                         /* 1008FF18 */
 /* Calndr PwrDown            Back   Forward */
    0x115, 0x15e, 0, 0, 0, 0, 0x16a, 0x169,                     /* 1008FF20 */
 /* Stop   Refresh Power Wake            Sleep */
    0x168, 0x167, 0x15e, 0x163, 0, 0, 0, 0x15f,                 /* 1008FF28 */
 /* Favrts Pause  Media  MyComp */
    0x166, 0x122, 0x16d, 0x16b, 0, 0, 0, 0,                     /* 1008FF30 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF38 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF40 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF48 */
 /* AppL   AppR         Calc      Close  Copy */
    0x109, 0x11e, 0, 0, 0x121, 0, 0x140, 0x118,                 /* 1008FF50 */
 /* Cut          Docmnts Excel */
    0x117, 0, 0, 0x105, 0x114, 0, 0, 0,                         /* 1008FF58 */
 /*    LogOff */
    0, 0x116, 0, 0, 0, 0, 0, 0,                                 /* 1008FF60 */
 /*       OffcHm Open      Paste */
    0, 0, 0x13c, 0x13f, 0, 0x10a, 0, 0,                         /* 1008FF68 */
 /*       Reply  Refresh         Save */
    0, 0, 0x141, 0x167, 0, 0, 0, 0x157,                         /* 1008FF70 */
 /* ScrlUp ScrlDn    Send   Spell        TaskPane */
    0x10b, 0x18b, 0, 0x143, 0x123, 0, 0, 0x13d,                 /* 1008FF78 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF80 */
 /*    Word */
    0, 0x113, 0, 0, 0, 0, 0, 0,                                 /* 1008FF88 */
 /* MailFwd MyPics MyMusic*/
    0x142, 0x164, 0x13c, 0, 0, 0, 0, 0,                         /* 1008FF90 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF98 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFF0 */
    0, 0, 0, 0, 0, 0, 0, 0                                      /* 1008FFF8 */
};

#endif /* ___VBox_keyboard_tables_h */
