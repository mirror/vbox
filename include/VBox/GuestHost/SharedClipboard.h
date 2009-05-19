/** @file
 *
 * Shared Clipboard
 */

/*
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___GUESTHOST_VBOXCLIPBOARD__H
#define ___GUESTHOST_VBOXCLIPBOARD__H

#include <iprt/cdefs.h>
#include <iprt/types.h>

enum {
    /** The number of milliseconds before the clipboard times out. */
#ifndef TESTCASE
    CLIPBOARD_TIMEOUT = 5000
#else
    CLIPBOARD_TIMEOUT = 1
#endif
};

/** Opaque data structure for the X11/VBox frontend/glue code. */
struct _VBOXCLIPBOARDCONTEXT;
typedef struct _VBOXCLIPBOARDCONTEXT VBOXCLIPBOARDCONTEXT;

/** Opaque data structure for the X11/VBox backend code. */
struct _CLIPBACKEND;
typedef struct _CLIPBACKEND CLIPBACKEND;

/** Opaque data structure used for asynchronously completing requests by VBox
 * to read the X11 clipboard data. */
struct _CLIPREADX11CBCONTEXT;
typedef struct _CLIPREADX11CBCONTEXT CLIPREADX11CBCONTEXT;

/* APIs exported by the X11 backend */
extern CLIPBACKEND *ClipConstructX11(VBOXCLIPBOARDCONTEXT *pFrontend);
extern void ClipDestructX11(CLIPBACKEND *pBackend);
extern int ClipStartX11(CLIPBACKEND *pBackend);
extern int ClipStopX11(CLIPBACKEND *pBackend);
extern void ClipAnnounceFormatToX11(CLIPBACKEND *pBackend,
                                    uint32_t u32Formats);
extern int ClipRequestDataFromX11(CLIPBACKEND *pBackend, uint32_t u32Format,
                                  void *pv, uint32_t cb,
                                  CLIPREADX11CBCONTEXT *pCtx);

/* APIs exported by the X11/VBox frontend */
extern int ClipRequestDataForX11(VBOXCLIPBOARDCONTEXT *pCtx,
                                        uint32_t u32Format, void **ppv,
                                        uint32_t *pcb);
extern void ClipReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                             uint32_t u32Formats);
extern void ClipCompleteDataRequestFromX11(CLIPREADX11CBCONTEXT *pCtx, int rc,
                                           uint32_t cbActual);
#endif  /* ___GUESTHOST_VBOXCLIPBOARD__H */
