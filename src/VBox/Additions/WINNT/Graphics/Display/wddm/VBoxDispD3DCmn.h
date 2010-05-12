/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxDispD3DCmn_h___
#define ___VBoxDispD3DCmn_h___

#ifdef DEBUG
# define VBOXWDDMDISP_DEBUG
#endif

#ifdef VBOXWDDMDISP_DEBUG
VOID vboxVDbgDoPrint(LPCSTR szString, ...);

#define vboxVDbgBreak() AssertBreakpoint()
#define vboxVDbgPrint(_m) \
    do { \
        vboxVDbgDoPrint _m ; \
    } while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#define vboxVDbgPrintF vboxVDbgPrint
#else
#define vboxVDbgBreak() do {} while (0)
#define vboxVDbgPrint(_m)  do {} while (0)
#define vboxVDbgPrintR vboxVDbgPrint
#define vboxVDbgPrintF vboxVDbgPrint
#endif

#endif /* #ifndef ___VBoxDispD3DCmn_h___ */
