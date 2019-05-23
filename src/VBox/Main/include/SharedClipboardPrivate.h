/* $Id$ */
/** @file
 * Private Shared Clipboard code. */

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

#ifndef MAIN_INCLUDED_SharedClipboardPrivate_h
#define MAIN_INCLUDED_SharedClipboardPrivate_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <VBox/hgcmsvc.h> /* For PVBOXHGCMSVCPARM. */
#include <VBox/HostServices/VBoxClipboardSvc.h>


/* Prototypes. */
class Console;

/**
 * Private singleton class for the Shared Clipboard
 * implementation. Can't be instanciated directly, only via
 * the factory pattern.
 */
class SharedClipboard
{
public:

    static SharedClipboard *createInstance(const ComObjPtr<Console>& pConsole)
    {
        Assert(NULL == SharedClipboard::s_pInstance);
        SharedClipboard::s_pInstance = new SharedClipboard(pConsole);
        return SharedClipboard::s_pInstance;
    }

    static void destroyInstance(void)
    {
        if (SharedClipboard::s_pInstance)
        {
            delete SharedClipboard::s_pInstance;
            SharedClipboard::s_pInstance = NULL;
        }
    }

    static inline SharedClipboard *getInstance(void)
    {
        AssertPtr(SharedClipboard::s_pInstance);
        return SharedClipboard::s_pInstance;
    }

protected:

    SharedClipboard(const ComObjPtr<Console>& pConsole);
    virtual ~SharedClipboard(void);

public:

    /** @name Public helper functions.
     * @{ */
    int hostCall(uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms) const;
    /** @}  */

public:

    /** @name Static low-level HGCM callback handler.
     * @{ */
    static DECLCALLBACK(int) hostServiceCallback(void *pvExtension, uint32_t u32Function, void *pvParms, uint32_t cbParms);
    /** @}  */

protected:

    /** @name Singleton properties.
     * @{ */
    /** Pointer to console implementation. */
    const ComObjPtr<Console>   m_pConsole;
    /** @}  */

private:

    /** Staic pointer to singleton instance. */
    static SharedClipboard    *s_pInstance;
};

/** Access to the Shared Clipboard's singleton instance. */
#define SHAREDCLIPBOARDINST() SharedClipboard::getInstance()

#endif /* !MAIN_INCLUDED_SharedClipboardPrivate_h */

