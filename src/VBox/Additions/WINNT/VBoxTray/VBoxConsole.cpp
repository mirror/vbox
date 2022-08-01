/* $Id$ */
/** @file
 * VBoxConsole.cpp - Console APIs.
 */

/*
 * Copyright (C) 2013-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxTray.h"
#include "VBoxTrayInternal.h"


/** @todo r=andy Lacks documentation / Doxygen headers. What does this, and why? */

BOOL VBoxConsoleIsAllowed()
{
    return vboxDtIsInputDesktop() && vboxStIsActiveConsole();
}

void VBoxConsoleEnable(BOOL fEnable)
{
    if (fEnable)
        VBoxCapsAcquireAllSupported();
    else
        VBoxCapsReleaseAll();
}

void VBoxConsoleCapSetSupported(uint32_t iCap, BOOL fSupported)
{
    if (fSupported)
    {
        VBoxCapsEntryFuncStateSet(iCap, VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);

        if (VBoxConsoleIsAllowed())
            VBoxCapsEntryAcquire(iCap);
    }
    else
    {
        VBoxCapsEntryFuncStateSet(iCap, VBOXCAPS_ENTRY_FUNCSTATE_UNSUPPORTED);

        VBoxCapsEntryRelease(iCap);
    }
}

