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
    CLIPBOARDTIMEOUT = 5000
};

/** Opaque data structure for the X11/VBox frontend/glue code. */
struct _VBOXCLIPBOARDCONTEXT;
typedef struct _VBOXCLIPBOARDCONTEXT VBOXCLIPBOARDCONTEXT;

/** Opaque data structure for the X11/VBox backend code. */
struct _VBOXCLIPBOARDCONTEXTX11;
typedef struct _VBOXCLIPBOARDCONTEXTX11 VBOXCLIPBOARDCONTEXTX11;

/** Does X11 or VBox currently own the clipboard? */
/** @todo This is ugly, get rid of it. */
enum g_eOwner { NONE = 0, X11, VB };

/** A structure containing information about where to store a request
 * for the X11 clipboard contents. */
struct _VBOXCLIPBOARDREQUEST 
{
    /** The buffer to write X11 clipboard data to (valid during a request
     * for the clipboard contents) */
    void *pv;
    /** The size of the buffer to write X11 clipboard data to (valid during
     * a request for the clipboard contents) */
    unsigned cb;
    /** The size of the X11 clipboard data written to the buffer (valid
     * during a request for the clipboard contents) */
    uint32_t *pcbActual;
    /** The clipboard context this request is associated with */
    VBOXCLIPBOARDCONTEXTX11 *pCtx;
};

typedef struct _VBOXCLIPBOARDREQUEST VBOXCLIPBOARDREQUEST;

/* APIs exported by the X11 backend */
extern VBOXCLIPBOARDCONTEXTX11 *VBoxX11ClipboardConstructX11
                                        (VBOXCLIPBOARDCONTEXT *pFrontend);
extern void VBoxX11ClipboardDestructX11(VBOXCLIPBOARDCONTEXTX11 *pBackend);
extern int VBoxX11ClipboardStartX11(VBOXCLIPBOARDCONTEXTX11 *pBackend,
                                    bool fOwnsClipboard);
extern int VBoxX11ClipboardStopX11(VBOXCLIPBOARDCONTEXTX11 *pBackend);
extern void VBoxX11ClipboardRequestSyncX11(VBOXCLIPBOARDCONTEXTX11 *pBackend);
extern void VBoxX11ClipboardAnnounceVBoxFormat(VBOXCLIPBOARDCONTEXTX11
                                               *pBackend, uint32_t u32Formats);
extern int VBoxX11ClipboardReadX11Data(VBOXCLIPBOARDCONTEXTX11 *pBackend,
                                       uint32_t u32Format,
                                       VBOXCLIPBOARDREQUEST *pRequest);

/* APIs exported by the X11/VBox frontend */
extern int VBoxX11ClipboardReadVBoxData(VBOXCLIPBOARDCONTEXT *pCtx,
                                        uint32_t u32Format, void **ppv,
                                        uint32_t *pcb);
extern void VBoxX11ClipboardReportX11Formats(VBOXCLIPBOARDCONTEXT *pCtx,
                                             uint32_t u32Formats);
#endif  /* ___GUESTHOST_VBOXCLIPBOARD__H */
