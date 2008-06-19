/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * CInterface implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "COMDefs.h"

#if !defined (VBOX_WITH_XPCOM)

#else /* !defined (VBOX_WITH_XPCOM) */

/* Qt includes */
#include <QObject>
#include <QSocketNotifier>

#include <nsEventQueueUtils.h>
#include <nsIEventQueue.h>

// for exception fetching
#include <nsIExceptionService.h>

#include <iprt/env.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/err.h>

/* Mac OS X (Carbon mode) and OS/2 will notify the native queue
   internally in plevent.c. Because moc doesn't seems to respect
   #ifdefs, we still have to include the definition of the class.
   very silly. */
# if !defined (Q_OS_MAC)  && !defined (Q_OS_OS2)
XPCOMEventQSocketListener *COMBase::sSocketListener = 0;

# endif

/**
 *  Internal class to asyncronously handle IPC events on the GUI thread
 *  using the event queue socket FD and QSocketNotifier.
 */
class XPCOMEventQSocketListener : public QObject
{
    Q_OBJECT

public:

    XPCOMEventQSocketListener (nsIEventQueue *eq)
    {
        mEventQ = eq;
        mNotifier = new QSocketNotifier (mEventQ->GetEventQueueSelectFD(),
                                         QSocketNotifier::Read, this);
        QObject::connect (mNotifier, SIGNAL (activated (int)),
                          this, SLOT (processEvents()));
    }

    virtual ~XPCOMEventQSocketListener()
    {
        delete mNotifier;
    }

public slots:

    void processEvents() { mEventQ->ProcessPendingEvents(); }

private:

    QSocketNotifier *mNotifier;
    nsCOMPtr <nsIEventQueue> mEventQ;
};

#endif /* !defined (VBOX_WITH_XPCOM) */

/**
 *  Initializes COM/XPCOM.
 */
HRESULT COMBase::InitializeCOM()
{
    LogFlowFuncEnter();

    /* Note: On Win32, Qt somehow calls CoInitialize[Ex]() during creation of
     * the QApplication instance (didn't explore deeply why it does so) with
     * different flags which is incompatible with our multithreaded
     * apartment. com::Initialize() will properly care of this situation. */

    HRESULT rc = com::Initialize();

#if defined (VBOX_WITH_XPCOM)

#if !defined (RT_OS_DARWIN) && !defined (RT_OS_OS2)

    if (NS_SUCCEEDED (rc))
    {
        nsCOMPtr <nsIEventQueue> eventQ;
        rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
        if (NS_SUCCEEDED (rc))
        {
#ifdef DEBUG
            BOOL isNative = FALSE;
            eventQ->IsQueueNative (&isNative);
            AssertMsg (isNative, ("The event queue must be native"));
#endif
            BOOL isOnMainThread = FALSE;
            rc = eventQ->IsOnCurrentThread (&isOnMainThread);
            if (NS_SUCCEEDED (rc) && isOnMainThread)
            {
                sSocketListener = new XPCOMEventQSocketListener (eventQ);
            }
        }
    }

#endif

#endif /* defined (VBOX_WITH_XPCOM) */

    if (FAILED (rc))
        CleanupCOM();

    AssertComRC (rc);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;

}

/**
 *  Cleans up COM/XPCOM.
 */
HRESULT COMBase::CleanupCOM()
{
    LogFlowFuncEnter();

    HRESULT rc = S_OK;

#if defined (VBOX_WITH_XPCOM)

    /* scope the code to make smart references are released before calling
     * com::Shutdown() */
    {
        nsCOMPtr <nsIEventQueue> eventQ;
        rc = NS_GetMainEventQ (getter_AddRefs (eventQ));
        if (NS_SUCCEEDED (rc))
        {
            BOOL isOnMainThread = FALSE;
            rc = eventQ->IsOnCurrentThread (&isOnMainThread);
            if (NS_SUCCEEDED (rc) && isOnMainThread)
            {
# if !defined (RT_OS_DARWIN) && !defined (RT_OS_OS2)
                if (sSocketListener)
                {
                    delete sSocketListener;
                    sSocketListener = NULL;
                }
# endif
            }
        }
    }

#endif /* defined (VBOX_WITH_XPCOM) */

    HRESULT rc2 = com::Shutdown();
    if (SUCCEEDED (rc))
        rc = rc2;

    AssertComRC (rc);

    LogFlowFunc (("rc=%08X\n", rc));
    LogFlowFuncLeave();
    return rc;
}

/* static */
void COMBase::ToSafeArray (const QVector <QString> &aVec,
                           com::SafeArray <BSTR> &aArr)
{
    aArr.reset (aVec.size());
    for (int i = 0; i < aVec.size(); ++ i)
        aArr [i] = SysAllocString ((const OLECHAR *)
            (aVec.at (i).isNull() ? 0 : aVec.at (i).utf16()));
}

/* static */
void COMBase::FromSafeArray (const com::SafeArray <BSTR> &aArr,
                             QVector <QString> &aVec)
{
    aVec.resize (aArr.size());
    for (int i = 0; i < aVec.size(); ++i)
        aVec [i] = QString::fromUtf16 (aArr [i]);
}

////////////////////////////////////////////////////////////////////////////////

void COMErrorInfo::init (const CVirtualBoxErrorInfo &info)
{
    AssertReturnVoid (!info.isNull());

    bool gotSomething = false;
    bool gotAll = true;

    mResultCode = info.GetResultCode();
    gotSomething |= info.isOk();
    gotAll &= info.isOk();

    mInterfaceID = info.GetInterfaceID();
    gotSomething |= info.isOk();
    gotAll &= info.isOk();
    if (info.isOk())
        mInterfaceName = getInterfaceNameFromIID (mInterfaceID);

    mComponent = info.GetComponent();
    gotSomething |= info.isOk();
    gotAll &= info.isOk();

    mText = info.GetText();
    gotSomething |= info.isOk();
    gotAll &= info.isOk();

    CVirtualBoxErrorInfo next = info.GetNext();
    if (info.isOk() && !next.isNull())
    {
        mNext.reset (new COMErrorInfo (next));
        Assert (mNext.get());
    }
    else
        mNext.reset();
    gotSomething |= info.isOk();
    gotAll &= info.isOk();

    mIsBasicAvailable = gotSomething;
    mIsFullAvailable = gotAll;

    mIsNull = gotSomething;

    AssertMsg (gotSomething, ("Nothing to fetch!\n"));
}

/**
 *  Fetches error info from the current thread.
 *  If callee is NULL, then error info is fetched in "interfaceless"
 *  manner (so calleeIID() and calleeName() will return null).
 *
 *  @param  callee
 *      pointer to the interface whose method returned an error
 *  @param  calleeIID
 *      UUID of the callee's interface. Ignored when callee is NULL
 */
void COMErrorInfo::fetchFromCurrentThread (IUnknown *callee, const GUID *calleeIID)
{
    mIsNull = true;
    mIsFullAvailable = mIsBasicAvailable = false;

    AssertReturn (!callee || calleeIID, (void) 0);

    HRESULT rc = E_FAIL;

#if !defined (VBOX_WITH_XPCOM)

    if (callee)
    {
        CComPtr <IUnknown> iface = callee;
        CComQIPtr <ISupportErrorInfo> serr;
        serr = callee;
        if (!serr)
            return;
        rc = serr->InterfaceSupportsErrorInfo (*calleeIID);
        if (!SUCCEEDED (rc))
            return;
    }

    CComPtr <IErrorInfo> err;
    rc = ::GetErrorInfo (0, &err);
    if (rc == S_OK && err)
    {
        CComPtr <IVirtualBoxErrorInfo> info;
        info = err;
        if (info)
            init (CVirtualBoxErrorInfo (info));

        if (!mIsFullAvailable)
        {
            bool gotSomething = false;

            rc = err->GetGUID (COMBase::GUIDOut (mInterfaceID));
            gotSomething |= SUCCEEDED (rc);
            if (SUCCEEDED (rc))
                mInterfaceName = getInterfaceNameFromIID (mInterfaceID);

            rc = err->GetSource (COMBase::BSTROut (mComponent));
            gotSomething |= SUCCEEDED (rc);

            rc = err->GetDescription (COMBase::BSTROut (mText));
            gotSomething |= SUCCEEDED (rc);

            if (gotSomething)
                mIsBasicAvailable = true;

            mIsNull = gotSomething;

            AssertMsg (gotSomething, ("Nothing to fetch!\n"));
        }
    }

#else /* !defined (VBOX_WITH_XPCOM) */

    nsCOMPtr <nsIExceptionService> es;
    es = do_GetService (NS_EXCEPTIONSERVICE_CONTRACTID, &rc);
    if (NS_SUCCEEDED (rc))
    {
        nsCOMPtr <nsIExceptionManager> em;
        rc = es->GetCurrentExceptionManager (getter_AddRefs (em));
        if (NS_SUCCEEDED (rc))
        {
            nsCOMPtr <nsIException> ex;
            rc = em->GetCurrentException (getter_AddRefs(ex));
            if (NS_SUCCEEDED (rc) && ex)
            {
                nsCOMPtr <IVirtualBoxErrorInfo> info;
                info = do_QueryInterface (ex, &rc);
                if (NS_SUCCEEDED (rc) && info)
                    init (CVirtualBoxErrorInfo (info));

                if (!mIsFullAvailable)
                {
                    bool gotSomething = false;

                    rc = ex->GetResult (&mResultCode);
                    gotSomething |= NS_SUCCEEDED (rc);

                    char *message = NULL; // utf8
                    rc = ex->GetMessage (&message);
                    gotSomething |= NS_SUCCEEDED (rc);
                    if (NS_SUCCEEDED (rc) && message)
                    {
                        mText = QString::fromUtf8 (message);
                        nsMemory::Free (message);
                    }

                    if (gotSomething)
                        mIsBasicAvailable = true;

                    mIsNull = gotSomething;

                    AssertMsg (gotSomething, ("Nothing to fetch!\n"));
                }

                // set the exception to NULL (to emulate Win32 behavior)
                em->SetCurrentException (NULL);

                rc = NS_OK;
            }
        }
    }

    AssertComRC (rc);

#endif /* !defined (VBOX_WITH_XPCOM) */

    if (callee && calleeIID && mIsBasicAvailable)
    {
        mCalleeIID = COMBase::ToQUuid (*calleeIID);
        mCalleeName = getInterfaceNameFromIID (mCalleeIID);
    }
}

// static
QString COMErrorInfo::getInterfaceNameFromIID (const QUuid &id)
{
    QString name;

    com::GetInterfaceNameByIID (COMBase::GUIDIn (id), COMBase::BSTROut (name));

    return name;
}

#if defined (VBOX_WITH_XPCOM)
#include "COMDefs.moc"
#endif
