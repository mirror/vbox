/** @file
 *
 * VirtualBox X11 Additions mouse driver utility functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/assert.h>
#include <VBox/VBoxGuest.h>
#include "VBoxUtils.h"

#include "xf86.h"
#define NEED_XF86_TYPES
#include "xf86_ansic.h"
#include "compiler.h"

int VBoxMouseInit(void)
{
    int rc = VbglR3Init();
    if (RT_FAILURE(rc))
    {
        ErrorF("VbglR3Init failed.\n");
        return 1;
    }

    rc = VbglR3SetMouseStatus(VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE /* | VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR */);
    if (VBOX_FAILURE(rc))
    {
        ErrorF("Error sending mouse pointer capabilities to VMM! rc = %d (%s)\n",
               errno, strerror(errno));
        return 1;
    }
    xf86Msg(X_INFO, "VirtualBox mouse pointer integration available.\n");
    return 0;
}


int VBoxMouseQueryPosition(unsigned int *puAbsXPos, unsigned int *puAbsYPos)
{
    int rc;
    uint32_t pointerXPos;
    uint32_t pointerYPos;

    AssertPtrReturn(puAbsXPos, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puAbsYPos, VERR_INVALID_PARAMETER);
    rc = VbglR3GetMouseStatus(NULL, &pointerXPos, &pointerYPos);
    if (VBOX_SUCCESS(rc))
    {
        *puAbsXPos = pointerXPos;
        *puAbsYPos = pointerYPos;
        return 0;
    }
    ErrorF("Error querying host mouse position! rc = %d\n", rc);
    return 2;
}


int VBoxMouseFini(void)
{
    int rc = VbglR3SetMouseStatus(0);
    VbglR3Term();
    return rc;
}
