/** @file
 * Implementation of ThreadTask
 */

/*
 * Copyright (C) 2015 Oracle Corporation
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

/** @todo r=bird: DOCUMENTATION!! 'ING DOCUMENTATION!! THIS 'ING FUNCTION
 *        CONSUMES THE this OBJECTED AND YOU AREN'T EXACTLY 'ING TELLING
 *        ANYBODY.  THAT IS REALLY NICE OF YOU. */
HRESULT ThreadTask::createThread(PRTTHREAD pThread, RTTHREADTYPE enmType)
{
    HRESULT rc = S_OK;
    try
    {
        m_pThread = pThread;
        int vrc = RTThreadCreate(m_pThread,
                                 taskHandler,
                                 (void *)this,
                                 0,
                                 enmType,
                                 0,
                                 this->getTaskName().c_str());
        if (RT_FAILURE(vrc))
        {
            throw E_FAIL;
        }
    }
    catch(HRESULT eRC)
    {
        /** @todo r=bird: only happens in your above throw call.
         *  Makes you wonder why you don't just do if (RT_SUCCESS(vrc)) return S_OK;
         *  else {delete pTask; return E_FAIL;}   */
        rc = eRC;
        delete this;
    }
    catch(std::exception& )
    {
        /** @todo r=bird: can't happen, RTThreadCreate+Utf8Str doesn't do this. */
        rc = E_FAIL;
        delete this;
    }
    catch(...)
    {
        /** @todo r=bird: can't happen, RTThreadCreate+Utf8Str doesn't do this. */
        rc = E_FAIL;
        delete this;
    }

    return rc;
}

/**
 * Static method that can get passed to RTThreadCreate to have a
 * thread started for a Task.
 */
/* static */ DECLCALLBACK(int) ThreadTask::taskHandler(RTTHREAD /* thread */, void *pvUser)
{
    HRESULT rc = S_OK;
    if (pvUser == NULL)
        return VERR_INVALID_POINTER;

    ThreadTask *pTask = static_cast<ThreadTask *>(pvUser);
    try
    {
        pTask->handler();
    }
    /** @todo r=bird: This next stuff here is utterly and totally pointless, as
     *        the handler() method _must_ and _shall_ do this, otherwise there is
     *        no 'ing way we can know about it, since you (a) you always return
     *        VINF_SUCCESS (aka '0'), and (b) the thread isn't waitable even if
     *        you wanted to return anything.  It is much preferred to _crash_ and
     *        _burn_ if we fail to catch stuff than quietly ignore it, most
     *        likely the state of the objects involved is butchered by this.  At
     *        least AssertLogRel()! Gee. */
    catch(HRESULT eRC)
    {
        rc = eRC;
    }
    catch(std::exception& )
    {
        rc = E_FAIL;
    }
    catch(...)
    {
        rc = E_FAIL;
    }

    delete pTask;

    return 0;
}

/*static*/ HRESULT ThreadTask::setErrorStatic(HRESULT aResultCode,
                                    const Utf8Str &aText)
{
    NOREF(aText);
    return aResultCode;
}

