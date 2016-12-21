/** @file
 * Implementation of ThreadTask
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

#include <iprt/thread.h>

#include "VirtualBoxBase.h"
#include "ThreadTask.h"


/**
 * Starts the task (on separate thread), consuming @a this.
 *
 * The function takes ownership of "this" instance (object instance which calls
 * this function). And the function is responsible for deletion of "this"
 * pointer in all cases.
 *
 * Possible way of usage:
 * @code{.cpp}
        HRESULT hr;
        SomeTaskInheritedFromThreadTask *pTask = NULL;
        try
        {
            pTask = new SomeTaskInheritedFromThreadTask(this);
            if (!pTask->Init()) // some init procedure
                throw E_FAIL;
        }
        catch (...)
        {
            if (pTask);
                delete pTask;
            return E_FAIL;
        }
        return pTask->createThread(); // pTask is always consumed
   @endcode
 *
 * @sa createThreadWithType, createThreadWithRaceCondition
 */
HRESULT ThreadTask::createThread(void)
{
    return createThreadInternal(RTTHREADTYPE_MAIN_WORKER, NULL /*phThread*/);
}


/**
 * Same ThreadTask::createThread(), except it takes a thread type parameter.
 *
 * @param   enmType     The thread type.
 */
HRESULT ThreadTask::createThreadWithType(RTTHREADTYPE enmType)
{
    return createThreadInternal(enmType, NULL /*phThread*/);
}


/**
 * Same ThreadTask::createThread(), except it returns a thread handle.
 *
 * If the task thread is incorrectly mananged, the caller may easily race the
 * completion and termination of the task thread!  Use with care!
 *
 * @param   phThread    Handle of the worker thread.
 */
HRESULT ThreadTask::createThreadWithRaceCondition(PRTTHREAD phThread)
{
    return createThreadInternal(RTTHREADTYPE_MAIN_WORKER, phThread);
}


/**
 * Internal worker for ThreadTask::createThread,
 * ThreadTask::createThreadWithType, ThreadTask::createThreadwithRaceCondition.
 *
 * @note Always consumes @a this!
 */
HRESULT ThreadTask::createThreadInternal(RTTHREADTYPE enmType, PRTTHREAD phThread)
{
    RTTHREAD hThread;
    int vrc = RTThreadCreate(&hThread,
                             taskHandlerThreadProc,
                             (void *)this,
                             0,
                             enmType,
                             0,
                             this->getTaskName().c_str());
    if (RT_SUCCESS(vrc))
    {
        mAsync = true;
        if (phThread)
            *phThread = hThread;
        return S_OK;
    }

    delete this;
    return E_FAIL;
}


/**
 * Static method that can get passed to RTThreadCreate to have a
 * thread started for a Task.
 */
/* static */ DECLCALLBACK(int) ThreadTask::taskHandlerThreadProc(RTTHREAD /* thread */, void *pvUser)
{
    if (pvUser == NULL)
        return VERR_INVALID_POINTER; /* nobody cares */

    ThreadTask *pTask = static_cast<ThreadTask *>(pvUser);

    /*
     *  handler shall catch and process all possible cases as errors and exceptions.
     */
    pTask->handler();

    delete pTask;
    return VINF_SUCCESS;
}

