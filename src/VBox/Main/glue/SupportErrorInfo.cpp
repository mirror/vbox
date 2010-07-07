/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * SupportErrorInfo* class family implementations
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/com/SupportErrorInfo.h"

#include "VBox/com/ptr.h"
#include "VBox/com/VirtualBoxErrorInfo.h"

#include "../include/Logging.h"

#include <iprt/thread.h>

#if defined (VBOX_WITH_XPCOM)
# include <nsIServiceManager.h>
# include <nsIExceptionService.h>
#endif /* defined (VBOX_WITH_XPCOM) */

namespace com
{

// MultiResult methods
////////////////////////////////////////////////////////////////////////////////

RTTLS MultiResult::sCounter = NIL_RTTLS;

/*static*/
void MultiResult::incCounter()
{
    if (sCounter == NIL_RTTLS)
    {
        sCounter = RTTlsAlloc();
        AssertReturnVoid(sCounter != NIL_RTTLS);
    }

    uintptr_t counter = (uintptr_t)RTTlsGet(sCounter);
    ++counter;
    RTTlsSet(sCounter, (void*)counter);
}

/*static*/
void MultiResult::decCounter()
{
    uintptr_t counter = (uintptr_t)RTTlsGet(sCounter);
    AssertReturnVoid(counter != 0);
    --counter;
    RTTlsSet(sCounter, (void*)counter);
}

/*static*/
bool MultiResult::isMultiEnabled()
{
    return ((uintptr_t)RTTlsGet(MultiResult::sCounter)) > 0;
}

} /* namespace com */

