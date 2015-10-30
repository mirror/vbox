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
        rc = eRC;
        delete this;
    }
    catch(std::exception& e)
    {
        rc = E_FAIL;
        delete this;
    }
    catch(...)
    {
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
    if(pvUser == NULL)
        return VERR_INVALID_POINTER;

    ThreadTask *pTask = static_cast<ThreadTask *>(pvUser);
    try
    {
        pTask->handler();
    }
    catch(HRESULT eRC)
    {
        rc = E_FAIL;
    }
    catch(std::exception& e)
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
    return aResultCode;
}

