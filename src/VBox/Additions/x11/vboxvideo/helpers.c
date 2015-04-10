/* $Id$ */
/** @file
 * VirtualBox X11 Additions graphics driver X server helper functions
 *
 * This file contains helpers which call back into the X server.  The longer-
 * term idea is to eliminate X server version dependencies in as many files as
 * possible inside the driver code.  Ideally most files should not directly
 * depend on X server symbols at all.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "vboxvideo.h"
#include <os.h>
#include <propertyst.h>
#include <windowstr.h>
#include <xf86.h>
#include <X11/Xatom.h>
#ifdef XORG_7X
# include <string.h>
#endif

void vbvxMsg(const char *pszFormat, ...)
{
    va_list args;

    va_start(args, pszFormat);
    VErrorF(pszFormat, args);
    va_end(args);
}

void vbvxMsgV(const char *pszFormat, va_list args)
{
    VErrorF(pszFormat, args);
}

void vbvxAbortServer(void)
{
    FatalError("Assertion");
}

VBOXPtr vbvxGetRec(ScrnInfoPtr pScrn)
{
    return ((VBOXPtr)pScrn->driverPrivate);
}

/* TESTING: if this is broken, dynamic resizing will not work on old X servers (1.2 and older). */
int vbvxGetIntegerPropery(ScrnInfoPtr pScrn, char *pszName, size_t *pcData, int32_t **ppaData)
{
    Atom atom;
    PropertyPtr prop;

    /* We can get called early, before the root window is created. */
    if (!ROOT_WINDOW(pScrn))
        return VERR_NOT_FOUND;
    atom = MakeAtom(pszName, strlen(pszName), FALSE);
    if (atom == BAD_RESOURCE)
        return VERR_NOT_FOUND;
    for (prop = wUserProps(ROOT_WINDOW(pScrn));
         prop != NULL && prop->propertyName != atom; prop = prop->next);
    if (prop == NULL)
        return VERR_NOT_FOUND;
    if (prop->type != XA_INTEGER || prop->format != 32)
        return VERR_NOT_FOUND;
    *pcData = prop->size;
    *ppaData = (int32_t *)prop->data;
    return VINF_SUCCESS;
}

void vbvxReprobeCursor(ScrnInfoPtr pScrn)
{
    if (ROOT_WINDOW(pScrn) == NULL)
        return;
#ifdef XF86_SCRN_INTERFACE
    pScrn->EnableDisableFBAccess(pScrn, FALSE);
    pScrn->EnableDisableFBAccess(pScrn, TRUE);
#else
    pScrn->EnableDisableFBAccess(pScrn->scrnIndex, FALSE);
    pScrn->EnableDisableFBAccess(pScrn->scrnIndex, TRUE);
#endif
}
