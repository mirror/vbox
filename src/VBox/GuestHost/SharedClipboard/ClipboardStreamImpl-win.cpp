/* $Id$ */
/** @file
 * ClipboardStreamImpl-win.cpp - Shared Clipboard IStream object implementation (for CF_HDROP).
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/GuestHost/SharedClipboard-win.h>

#include <iprt/asm.h>
#include <iprt/ldr.h>

#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/SharedClipboard-win.h>
#include <strsafe.h>

#include <VBox/log.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Static variables                                                                                                             *
*********************************************************************************************************************************/

