/* $Id$ */
/** @file
 * DnD - Common utility functions.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/GuestHost/DragAndDrop.h>

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/log.h>


/**
 * Converts a VBOXDNDACTION to a string.
 *
 * @returns Stringified version of VBOXDNDACTION
 * @param   uAction             DnD action to convert.
 */
const char *DnDActionToStr(VBOXDNDACTION uAction)
{
    switch (uAction)
    {
        case VBOX_DND_ACTION_IGNORE: return "ignore";
        case VBOX_DND_ACTION_COPY:   return "copy";
        case VBOX_DND_ACTION_MOVE:   return "move";
        case VBOX_DND_ACTION_LINK:   return "link";
        default:
            break;
    }
    AssertMsgFailedReturn(("Unknown uAction=%d\n", uAction), "bad");
}

