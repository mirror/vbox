/** @file
 * VirtualBox ThreadTask class definition
 */

/*
 * Copyright (C) 2015-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_THREADTASK
#define ____H_THREADTASK

#include "VBox/com/string.h"

/**
 * The class ThreadVoidData is used as a base class for any data which we want to pass into a thread
 */
struct ThreadVoidData
{
public:
    ThreadVoidData() { }
    virtual ~ThreadVoidData() { }
};


class ThreadTask
{
public:
    ThreadTask(const Utf8Str &t) : m_hThread(NIL_RTTHREAD), m_strTaskName(t)
    { }

    virtual ~ThreadTask()
    { }

    HRESULT createThread(void);
    HRESULT createThreadWithType(RTTHREADTYPE enmType);
    HRESULT createThreadWithRaceCondition(PRTTHREAD pThread);

    virtual void handler() = 0;
    inline Utf8Str getTaskName() const { return m_strTaskName; }

protected:
    HRESULT createThreadInternal(RTTHREADTYPE enmType, PRTTHREAD pThread);
    static DECLCALLBACK(int) taskHandlerThreadProc(RTTHREAD thread, void *pvUser);

    ThreadTask() : m_hThread(NIL_RTTHREAD), m_strTaskName("GenericTask")
    { }

    /** The worker thread handle (may be invalid if the thread has shut down). */
    RTTHREAD m_hThread;
    Utf8Str m_strTaskName;
};

#endif

