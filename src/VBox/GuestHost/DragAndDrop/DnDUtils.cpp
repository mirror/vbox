/* $Id$ */
/** @file
 * DnD - Common utility functions.
 */

/*
 * Copyright (C) 2022 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/GuestHost/DragAndDrop.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>


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

