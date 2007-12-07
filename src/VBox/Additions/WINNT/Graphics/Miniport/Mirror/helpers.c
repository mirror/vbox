/** @file
 *
 * vrdpvd.sys - VirtualBox Windows NT/2000/XP guest mirror video driver
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * *
 */

#include <ntddk.h>

#include "vrdpvd.h"

winVersion_t vboxQueryWinVersion (void)
{
    static winVersion_t winVersion = UNKNOWN_WINVERSION;

    ULONG majorVersion;
    ULONG minorVersion;
    ULONG buildNumber;

    if (winVersion != UNKNOWN_WINVERSION)
    {
        return winVersion;
    }

    PsGetVersion(&majorVersion, &minorVersion, &buildNumber, NULL);

    dprintf(("vrdpvd.sys::vboxQueryWinVersion: Windows NT version %d.%d, build %d\n",
             majorVersion, minorVersion, buildNumber));

    if (majorVersion >= 5)
    {
        if (majorVersion == 5 && minorVersion == 0)
        {
            winVersion = WIN2K;
        }
        else
        {
            winVersion = WINXP;
        }
    }
    else if (majorVersion == 4)
    {
        winVersion = WINNT4;
    }
    else
    {
        dprintf(("vrdpvd.sys::vboxQueryWinVersion: NT4 required!\n"));
    }

    return winVersion;
}
