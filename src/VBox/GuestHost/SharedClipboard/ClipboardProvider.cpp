/* $Id$ */
/** @file
 * Shared Clipboard - Provider implementation.
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
#include <VBox/GuestHost/SharedClipboard-uri.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>


#include <VBox/log.h>



SharedClipboardProvider::SharedClipboardProvider(void)
    : m_cRefs(0)
{
    LogFlowFuncEnter();
}

SharedClipboardProvider::~SharedClipboardProvider(void)
{
    LogFlowFuncEnter();
    Assert(m_cRefs == 0);
}

/**
 * Creates a Shared Clipboard provider.
 *
 * @returns New Shared Clipboard provider instance.
 * @param   enmSource           Source type to create provider for.
 */
/* static */
SharedClipboardProvider *SharedClipboardProvider::Create(SourceType enmSource)
{
    SharedClipboardProvider *pProvider = NULL;

    switch (enmSource)
    {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_GUEST
        case SourceType_VbglR3:
            pProvider = new SharedClipboardProviderVbglR3();
            break;
#endif

#ifdef VBOX_WITH_SHARED_CLIPBOARD_HOST
        case SourceType_HostService:
            pProvider = new SharedClipboardProviderHostService();
            break;
#endif
        default:
            AssertFailed();
            break;
    }

    return pProvider;
}

/**
 * Adds a reference to a Shared Clipboard provider.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardProvider::AddRef(void)
{
    LogFlowFuncEnter();
    return ASMAtomicIncU32(&m_cRefs);
}

/**
 * Removes a reference from a Shared Clipboard cache.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardProvider::Release(void)
{
    LogFlowFuncEnter();
    Assert(m_cRefs);
    return ASMAtomicDecU32(&m_cRefs);
}

