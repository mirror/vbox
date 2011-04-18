/* $Id$ */
/** @file
 * VBoxBalloonCtrl - VirtualBox Ballooning Control Service.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___H_VBOXBALLOONCTRL
#define ___H_VBOXBALLOONCTRL

#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/ptr.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/com/string.h>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/types.h>
#include <iprt/message.h>
#include <iprt/stream.h>

////////////////////////////////////////////////////////////////////////////////
//
// definitions
//
////////////////////////////////////////////////////////////////////////////////

/** command handler argument */
struct HandlerArg
{
    int argc;
    char **argv;
};

////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//
////////////////////////////////////////////////////////////////////////////////

#endif /* !___H_VBOXBALLOONCTRL */

