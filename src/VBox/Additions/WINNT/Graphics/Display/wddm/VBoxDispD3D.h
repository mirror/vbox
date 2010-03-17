/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#ifndef ___VBoxDispD3D_h___
#define ___VBoxDispD3D_h___

#ifdef DEBUG
# define VBOXWDDMDISP_DEBUG
#endif

typedef struct VBOXWDDMDISP_ADAPTER
{
    HANDLE hAdapter;
    UINT uIfVersion;
    UINT uRtVersion;
    D3DDDI_ADAPTERCALLBACKS RtCallbacks;
} VBOXWDDMDISP_ADAPTER, *PVBOXWDDMDISP_ADAPTER;

#ifdef VBOXWDDMDISP_DEBUG
VOID vboxVDbgDoPrint(LPCSTR szString, ...);

#define vboxVDbgBreak() AssertBreakpoint()
#define vboxVDbgPrint(_m) \
    do { \
        vboxVDbgDoPrint _m ; \
    } while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#else
#define vboxVDbgBreak() do {} while (0)
#define vboxVDbgPrint(_m)  do {} while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#endif

#endif /* #ifndef ___VBoxDispD3D_h___ */
