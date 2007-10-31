/* $Id$ */
/** @file
 * VBoxDDU - For dragging in library objects.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/types.h>
#ifdef VBOX_WITH_USB
# include <VBox/usbfilter.h>
#endif

/** Just a dummy global structure containing a bunch of
 * function pointers to code which is wanted in the link.
 */
PFNRT g_apfnDeps[] =
{
#ifdef VBOX_WITH_USB
    (PFNRT)USBFilterInit,
#endif
    NULL
};
