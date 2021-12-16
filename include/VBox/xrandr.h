/** @file
 * Module to dynamically load libXrandr and load all symbols which are needed by
 * VirtualBox.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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

#ifndef VBOX_INCLUDED_xrandr_h
#define VBOX_INCLUDED_xrandr_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/stdarg.h>

#ifndef __cplusplus
# error "This header requires C++ to avoid name clashes."
#endif

/* Define missing X11/XRandr structures, types and macros. */

#define Bool                        int
#define RRScreenChangeNotifyMask    (1L << 0)
#define RRScreenChangeNotify        0

struct _XDisplay;
typedef struct _XDisplay Display;

typedef unsigned long Atom;
typedef unsigned long XID;
typedef XID RROutput;
typedef XID Window;

struct XRRMonitorInfo
{
    Atom name;
    Bool primary;
    Bool automatic;
    int noutput;
    int x;
    int y;
    int width;
    int height;
    int mwidth;
    int mheight;
    RROutput *outputs;
};
typedef struct XRRMonitorInfo XRRMonitorInfo;


/* Declarations of the functions that we need from libXrandr */
#define VBOX_XRANDR_GENERATE_HEADER

#include <VBox/xrandr-calls.h>

#undef VBOX_XRANDR_GENERATE_HEADER

#endif /* !VBOX_INCLUDED_xrandr_h */

