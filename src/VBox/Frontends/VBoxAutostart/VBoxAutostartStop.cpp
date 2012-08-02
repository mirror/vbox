/* $Id$ */
/** @file
 * VBoxAutostart - VirtualBox Autostart service, stop machines during system shutdown.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/log.h>
#include <iprt/assert.h>

#include <algorithm>
#include <list>
#include <string>

#include "VBoxAutostart.h"

using namespace com;

DECLHIDDEN(RTEXITCODE) autostartStopMain(uint32_t uStartupDelay)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;

    AssertMsgFailed(("Not implemented yet!\n"));

    return rcExit;
}

