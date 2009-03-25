/***************************************************************************\
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
/*
* Based in part on Microsoft DDK sample code
*
*                           *******************
*                           * GDI SAMPLE CODE *
*                           *******************
*
* Module Name: debug.h
*
* Commonly used debugging macros.
*
* Copyright (c) 1992-1998 Microsoft Corporation
\***************************************************************************/

//VBOX
#ifdef LOG_ENABLED
VOID
DebugPrint(
    ULONG DebugPrintLevel,
    PCHAR DebugMessage,
    ...
    );

#define DISPDBG(arg) DebugPrint arg
#define RIP(x) { DebugPrint(0, x); }
#else
#define DISPDBG(arg)
#define RIP(x)
#endif

// #if DBG
// 
// VOID
// DebugPrint(
//     ULONG DebugPrintLevel,
//     PCHAR DebugMessage,
//     ...
//     );
// 
// #define DISPDBG(arg) DebugPrint arg
// #define RIP(x) { DebugPrint(0, x); EngDebugBreak();}
// 
// #else
// 
// #define DISPDBG(arg)
// #define RIP(x)
// 
// #endif


