/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * SupportErrorInfo* class family implementations
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
        AssertReturnVoid (sCounter != NIL_RTTLS);
    }

    uintptr_t counter = (uintptr_t) RTTlsGet (sCounter);
    ++ counter;
    RTTlsSet (sCounter, (void *) counter);
}

/*static*/
void MultiResult::decCounter()
{
    uintptr_t counter = (uintptr_t) RTTlsGet (sCounter);
    AssertReturnVoid (counter != 0);
    -- counter;
    RTTlsSet (sCounter, (void *) counter);
}

// SupportErrorInfoBase methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Sets error info for the current thread. This is an internal function that
 * gets eventually called by all public setError(), setWarning() and
 * setErrorInfo() variants.
 *
 * The previous error info object (if any) will be preserved if the multi-error
 * mode (see MultiResult) is turned on for the current thread.
 *
 * If @a aWarning is @c true, then the highest (31) bit in the @a aResultCode
 * value which indicates the error severity is reset to zero to make sure the
 * receiver will recognize that the created error info object represents a
 * warning rather than an error.
 *
 * If @a aInfo is not NULL then all other paremeters are ignored and the given
 * error info object is set on the current thread. Note that in this case, the
 * existing error info object (if any) will be preserved by attaching it to the
 * tail of the error chain of the given aInfo object in multi-error mode.
 *
 * If the error info is successfully set then this method returns aResultCode
 * (or the result code returned as an output parameter of the
 * aInfo->GetResultCode() call when @a aInfo is not NULL). This is done for
 * conveinence: this way, setError() and friends may be used in return
 * statements of COM method implementations.
 *
 * If setting error info fails then this method asserts and the failed result
 * code is returned.
 */
/* static */
HRESULT SupportErrorInfoBase::setErrorInternal(HRESULT aResultCode,
                                               const GUID *aIID,
                                               const char *aComponent,
                                               const Utf8Str &strText,
                                               bool aWarning,
                                               IVirtualBoxErrorInfo *aInfo /*= NULL*/)
{
    /* whether multi-error mode is turned on */
    bool preserve = ((uintptr_t) RTTlsGet (MultiResult::sCounter)) > 0;

    LogRel(("ERROR [COM]: aRC=%#08x aIID={%Vuuid} aComponent={%s} aText={%s} aWarning=%RTbool, aInfo=%p, preserve=%RTbool\n",
            aResultCode,
            aIID,
            aComponent,
            strText.c_str(),
            aWarning,
            aInfo,
            preserve));

    if (aInfo == NULL)
    {
        /* these are mandatory, others -- not */
        AssertReturn((!aWarning && FAILED (aResultCode)) ||
                      (aWarning && aResultCode != S_OK),
                      E_FAIL);
        AssertReturn(!strText.isEmpty(), E_FAIL);

        /* reset the error severity bit if it's a warning */
        if (aWarning)
            aResultCode &= ~0x80000000;
    }

    HRESULT rc = S_OK;

    do
    {
        ComPtr<IVirtualBoxErrorInfo> info;

#if !defined (VBOX_WITH_XPCOM)

        ComPtr<IVirtualBoxErrorInfo> curInfo;
        if (preserve)
        {
            /* get the current error info if any */
            ComPtr<IErrorInfo> err;
            rc = ::GetErrorInfo (0, err.asOutParam());
            if (FAILED(rc)) break;
            rc = err.queryInterfaceTo(curInfo.asOutParam());
            if (FAILED (rc))
            {
                /* create a IVirtualBoxErrorInfo wrapper for the native
                 * IErrorInfo object */
                ComObjPtr<VirtualBoxErrorInfo> wrapper;
                rc = wrapper.createObject();
                if (SUCCEEDED(rc))
                {
                    rc = wrapper->init (err);
                    if (SUCCEEDED(rc))
                        curInfo = wrapper;
                }
            }
        }
        /* On failure, curInfo will stay null */
        Assert(SUCCEEDED(rc) || curInfo.isNull());

        /* set the current error info and preserve the previous one if any */
        if (aInfo != NULL)
        {
            if (curInfo.isNull())
            {
                info = aInfo;
            }
            else
            {
                ComObjPtr<VirtualBoxErrorInfoGlue> infoObj;
                rc = infoObj.createObject();
                if (FAILED(rc)) break;

                rc = infoObj->init (aInfo, curInfo);
                if (FAILED(rc)) break;

                info = infoObj;
            }

            /* we want to return the head's result code */
            rc = info->COMGETTER(ResultCode) (&aResultCode);
            if (FAILED(rc)) break;
        }
        else
        {
            ComObjPtr<VirtualBoxErrorInfo> infoObj;
            rc = infoObj.createObject();
            if (FAILED(rc)) break;

            rc = infoObj->init (aResultCode, aIID, aComponent, strText.c_str(), curInfo);
            if (FAILED(rc)) break;

            info = infoObj;
        }

        ComPtr<IErrorInfo> err;
        rc = info.queryInterfaceTo(err.asOutParam());
        if (SUCCEEDED(rc))
            rc = ::SetErrorInfo (0, err);

#else // !defined (VBOX_WITH_XPCOM)

        nsCOMPtr <nsIExceptionService> es;
        es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
        if (NS_SUCCEEDED(rc))
        {
            nsCOMPtr <nsIExceptionManager> em;
            rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
            if (FAILED(rc)) break;

            ComPtr<IVirtualBoxErrorInfo> curInfo;
            if (preserve)
            {
                /* get the current error info if any */
                ComPtr<nsIException> ex;
                rc = em->GetCurrentException (ex.asOutParam());
                if (FAILED(rc)) break;
                rc = ex.queryInterfaceTo(curInfo.asOutParam());
                if (FAILED (rc))
                {
                    /* create a IVirtualBoxErrorInfo wrapper for the native
                     * nsIException object */
                    ComObjPtr<VirtualBoxErrorInfo> wrapper;
                    rc = wrapper.createObject();
                    if (SUCCEEDED(rc))
                    {
                        rc = wrapper->init (ex);
                        if (SUCCEEDED(rc))
                            curInfo = wrapper;
                    }
                }
            }
            /* On failure, curInfo will stay null */
            Assert (SUCCEEDED(rc) || curInfo.isNull());

            /* set the current error info and preserve the previous one if any */
            if (aInfo != NULL)
            {
                if (curInfo.isNull())
                {
                    info = aInfo;
                }
                else
                {
                    ComObjPtr<VirtualBoxErrorInfoGlue> infoObj;
                    rc = infoObj.createObject();
                    if (FAILED(rc)) break;

                    rc = infoObj->init (aInfo, curInfo);
                    if (FAILED(rc)) break;

                    info = infoObj;
                }

                /* we want to return the head's result code */
                PRInt32 lrc;
                rc = info->COMGETTER(ResultCode) (&lrc); aResultCode = lrc;
                if (FAILED(rc)) break;
            }
            else
            {
                ComObjPtr<VirtualBoxErrorInfo> infoObj;
                rc = infoObj.createObject();
                if (FAILED(rc)) break;

                rc = infoObj->init(aResultCode, aIID, aComponent, strText, curInfo);
                if (FAILED(rc)) break;

                info = infoObj;
            }

            ComPtr<nsIException> ex;
            rc = info.queryInterfaceTo(ex.asOutParam());
            if (SUCCEEDED(rc))
                rc = em->SetCurrentException (ex);
        }
        else if (rc == NS_ERROR_UNEXPECTED)
        {
            /*
             *  It is possible that setError() is being called by the object
             *  after the XPCOM shutdown sequence has been initiated
             *  (for example, when XPCOM releases all instances it internally
             *  references, which can cause object's FinalConstruct() and then
             *  uninit()). In this case, do_GetService() above will return
             *  NS_ERROR_UNEXPECTED and it doesn't actually make sense to
             *  set the exception (nobody will be able to read it).
             */
            LogWarningFunc (("Will not set an exception because "
                             "nsIExceptionService is not available "
                             "(NS_ERROR_UNEXPECTED). "
                             "XPCOM is being shutdown?\n"));
            rc = NS_OK;
        }

#endif // !defined (VBOX_WITH_XPCOM)
    }
    while (0);

    AssertComRC (rc);

    return SUCCEEDED(rc) ? aResultCode : rc;
}

/* static */
HRESULT SupportErrorInfoBase::setError (HRESULT aResultCode, const GUID &aIID,
                                        const char *aComponent, const char *aText,
                                        ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setErrorV (aResultCode, aIID, aComponent, aText, args);
    va_end (args);
    return rc;
}

/* static */
HRESULT SupportErrorInfoBase::setWarning (HRESULT aResultCode, const GUID &aIID,
                                          const char *aComponent, const char *aText,
                                          ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setWarningV (aResultCode, aIID, aComponent, aText, args);
    va_end (args);
    return rc;
}

HRESULT SupportErrorInfoBase::setError (HRESULT aResultCode, const char *aText, ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setErrorV (aResultCode, mainInterfaceID(), componentName(),
                            aText, args);
    va_end (args);
    return rc;
}

HRESULT SupportErrorInfoBase::setError (HRESULT aResultCode, const Utf8Str &strText)
{
    HRESULT rc = setError(aResultCode,
                          mainInterfaceID(),
                          componentName(),
                          strText);
    return rc;
}

HRESULT SupportErrorInfoBase::setWarning (HRESULT aResultCode, const char *aText, ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setWarningV (aResultCode, mainInterfaceID(), componentName(),
                              aText, args);
    va_end (args);
    return rc;
}

HRESULT SupportErrorInfoBase::setError (HRESULT aResultCode, const GUID &aIID,
                                        const char *aText, ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setErrorV (aResultCode, aIID, componentName(), aText, args);
    va_end (args);
    return rc;
}

HRESULT SupportErrorInfoBase::setWarning (HRESULT aResultCode, const GUID &aIID,
                                          const char *aText, ...)
{
    va_list args;
    va_start (args, aText);
    HRESULT rc = setWarningV (aResultCode, aIID, componentName(), aText, args);
    va_end (args);
    return rc;
}

} /* namespace com */

