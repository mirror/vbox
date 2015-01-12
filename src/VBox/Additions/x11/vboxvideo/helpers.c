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
