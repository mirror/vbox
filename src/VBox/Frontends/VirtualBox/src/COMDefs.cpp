/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * CInterface implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "COMDefs.h"

#if defined (Q_OS_WIN32)

// for CComPtr/CComQIPtr
#include <atlcomcli.h>
#include <VBox/com/assert.h>

#else // !defined (Q_OS_WIN32)

#include <qobject.h>
#include <qapplication.h>
#include <qfile.h>
#include <qsocketnotifier.h>
#ifdef DEBUG
    #include <qfileinfo.h>
#endif

#include <nsXPCOMGlue.h>
#include <nsIServiceManager.h>
#include <nsIComponentRegistrar.h>
// for NS_InitXPCOM2 with bin dir parameter
#include <nsEmbedString.h>
#include <nsIFile.h>
#include <nsILocalFile.h>
// for dconnect
#include <ipcIService.h>
#include <ipcCID.h>
// XPCOM headers still do not define this, so define by hand
#define IPC_DCONNECTSERVICE_CONTRACTID \
    "@mozilla.org/ipc/dconnect-service;1"
// for event queue management
#include <nsEventQueueUtils.h>
#include <nsIEventQueue.h>

// for IID to name resolution
#include <nsIInterfaceInfo.h>
#include <nsIInterfaceInfoManager.h>

// for exception fetching
#include <nsIExceptionService.h>

#undef ASSERT
#include <VBox/com/assert.h>

nsIComponentManager *COMBase::gComponentManager = nsnull;
nsIEventQueue* COMBase::gEventQ = nsnull;
ipcIDConnectService *COMBase::gDConnectService = nsnull;
PRUint32 COMBase::gVBoxServerID = 0;

/* Mac OS X (Carbon mode) and OS/2 will notify the native queue
   internally in plevent.c. Because moc doesn't seems to respect
   #ifdefs, we still have to include the definition of the class.
   very silly. */
# if !defined (Q_OS_MAC)  && !defined (Q_OS_OS2)
XPCOMEventQSocketListener *COMBase::gSocketListener = 0;
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
                                         QSocketNotifier::Read, this,
                                         "XPCOMEventQSocketNotifier");
        QObject::connect (mNotifier, SIGNAL (activated (int)),
                          this, SLOT (processEvents()));
    }

public slots:

    void processEvents() { mEventQ->ProcessPendingEvents(); }

private:

    QSocketNotifier *mNotifier;
    nsIEventQueue *mEventQ;
};

#endif // !defined (Q_OS_WIN32)

/**
 *  Initializes COM/XPCOM.
 */
HRESULT COMBase::initializeCOM()
{
    LogFlowFuncEnter();

#if defined (Q_OS_WIN32)

    /* disable this damn CoInitialize* somehow made by Qt during
     * creation of the QApplication instance (didn't explore deeply
     * why does it do this) */
    CoUninitialize();
    CoInitializeEx (NULL, COINIT_MULTITHREADED |
                          COINIT_DISABLE_OLE1DDE |
                          COINIT_SPEED_OVER_MEMORY);

    LogFlowFuncLeave();
    return S_OK;

#else

    if (gComponentManager)
    {
        LogFlowFuncLeave();
        return S_OK;
    }

    HRESULT rc;
    XPCOMGlueStartup (nsnull);

    nsCOMPtr <nsIServiceManager> serviceManager;

    /* create a file object containing the path to the executable */
    QCString appDir;
#ifdef DEBUG
    appDir = getenv ("VIRTUALBOX_APP_HOME");
    if (!appDir.isNull())
        appDir = QFile::encodeName (QFileInfo (QFile::decodeName (appDir)).absFilePath());
    else
#endif
    appDir = QFile::encodeName (qApp->applicationDirPath());
    nsCOMPtr <nsILocalFile> lfAppDir;
    rc = NS_NewNativeLocalFile (nsEmbedCString (appDir.data()), PR_FALSE,
                                getter_AddRefs (lfAppDir));
    if (SUCCEEDED (rc))
    {
        nsCOMPtr <nsIFile> fAppDir = do_QueryInterface (lfAppDir, &rc);
        if (SUCCEEDED( rc ))
        {
            /* initialize XPCOM and get the service manager */
            rc = NS_InitXPCOM2 (getter_AddRefs (serviceManager), fAppDir, nsnull);
        }
    }

    if (SUCCEEDED (rc))
    {
        /* get the registrar */
        nsCOMPtr <nsIComponentRegistrar> registrar =
            do_QueryInterface (serviceManager, &rc);
        if (SUCCEEDED (rc))
        {
            /* autoregister components from a component directory */
            registrar->AutoRegister (nsnull);

            /* get the component manager */
            rc = registrar->QueryInterface (NS_GET_IID (nsIComponentManager),
                                            (void**) &gComponentManager);
            if (SUCCEEDED (rc))
            {
                /* get the main thread's event queue (afaik, the
                 * dconnect service always gets created upon XPCOM
                 * startup, so it will use the main (this) thread's
                 * event queue to receive IPC events) */
                rc = NS_GetMainEventQ (&gEventQ);
#ifdef DEBUG
                BOOL isNative = FALSE;
                gEventQ->IsQueueNative (&isNative);
                AssertMsg (isNative, ("The event queue must be native"));
#endif
# if !defined (__DARWIN__) && !defined (__OS2__)
                gSocketListener = new XPCOMEventQSocketListener (gEventQ);
# endif

                /* get the IPC service */
                nsCOMPtr <ipcIService> ipcServ =
                    do_GetService (IPC_SERVICE_CONTRACTID, serviceManager, &rc);
                if (SUCCEEDED (rc))
                {
                    /* get the VirtualBox out-of-proc server ID */
                    rc = ipcServ->ResolveClientName ("VirtualBoxServer",
                                                     &gVBoxServerID);
                    if (SUCCEEDED (rc))
                    {
                        /* get the DConnect service */
                        rc = serviceManager->
                            GetServiceByContractID (IPC_DCONNECTSERVICE_CONTRACTID,
                                                    NS_GET_IID (ipcIDConnectService),
                                                    (void **) &gDConnectService);
                    }
                }
            }
        }
    }

    if (FAILED (rc))
        cleanupCOM();

    LogFlowFuncLeave();
    return rc;

#endif
}

/**
 *  Initializes COM/XPCOM.
 */
HRESULT COMBase::cleanupCOM()
{
    LogFlowFuncEnter();

#if defined (Q_OS_WIN32)
    CoUninitialize();
#else
    if (gComponentManager)
    {
        PRBool isOnCurrentThread = true;
        if (gEventQ)
            gEventQ->IsOnCurrentThread (&isOnCurrentThread);

        if (isOnCurrentThread)
        {
            LogFlowFunc (("Doing cleanup...\n"));
# if !defined (__DARWIN__) && !defined (__OS2__)
            if (gSocketListener)
                delete gSocketListener;
# endif
            if (gDConnectService)
            {
                gDConnectService->Release();
                gDConnectService = nsnull;
            }
            if (gEventQ)
            {
                gEventQ->Release();
                gEventQ = nsnull;
            }
            gComponentManager->Release();
            gComponentManager = nsnull;
            /* note: gComponentManager = nsnull indicates that we're
             * cleaned up */
            NS_ShutdownXPCOM (nsnull);
            XPCOMGlueShutdown();
        }
    }
#endif

    LogFlowFuncLeave();
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////

void COMErrorInfo::init (const CVirtualBoxErrorInfo &info)
{
    Assert (!info.isNull());
    if (info.isNull())
        return;

    bool gotSomething = false;

    mResultCode = info.GetResultCode();
    gotSomething |= info.isOk();
    mInterfaceID = info.GetInterfaceID();
    gotSomething |= info.isOk();
    if (info.isOk())
        mInterfaceName = getInterfaceNameFromIID (mInterfaceID);
    mComponent = info.GetComponent();
    gotSomething |= info.isOk();
    mText = info.GetText();
    gotSomething |= info.isOk();

    if (gotSomething)
        mIsFullAvailable = mIsBasicAvailable = true;

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

#if defined (__WIN__)

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

#else // !defined (__WIN__)

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

#endif // !defined (__WIN__)

    if (callee && calleeIID && mIsBasicAvailable)
    {
        mCalleeIID = COMBase::toQUuid (*calleeIID);
        mCalleeName = getInterfaceNameFromIID (mCalleeIID);
    }
}

// static
QString COMErrorInfo::getInterfaceNameFromIID (const QUuid &id)
{
    QString name;

#if defined (__WIN__)

    LONG rc;
    LPOLESTR iidStr = NULL;
    if (StringFromIID (id, &iidStr) == S_OK)
    {
        HKEY ifaceKey;
        rc = RegOpenKeyExW (HKEY_CLASSES_ROOT, L"Interface", 0, KEY_QUERY_VALUE, &ifaceKey);
        if (rc == ERROR_SUCCESS)
        {
            HKEY iidKey;
            rc = RegOpenKeyExW (ifaceKey, iidStr, 0, KEY_QUERY_VALUE, &iidKey);
            if (rc == ERROR_SUCCESS)
            {
                // determine the size and type
                DWORD sz, type;
                rc = RegQueryValueExW (iidKey, NULL, NULL, &type, NULL, &sz);
                if (rc == ERROR_SUCCESS && type == REG_SZ)
                {
                    // query the value to BSTR
                    BSTR bstrName = SysAllocStringLen (NULL, (sz + 1) / sizeof (TCHAR) + 1);
                    rc = RegQueryValueExW (iidKey, NULL, NULL, NULL, (LPBYTE) bstrName, &sz);
                    if (rc == ERROR_SUCCESS)
                    {
                        name = QString::fromUcs2 (bstrName);
                    }
                    SysFreeString (bstrName);
                }
                RegCloseKey (iidKey);
            }
            RegCloseKey (ifaceKey);
        }
        CoTaskMemFree (iidStr);
    }

#else

    nsresult rv;
    nsCOMPtr <nsIInterfaceInfoManager> iim =
        do_GetService (NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED (rv))
    {
        nsCOMPtr <nsIInterfaceInfo> iinfo;
        rv = iim->GetInfoForIID (&COMBase::GUIDIn (id), getter_AddRefs (iinfo));
        if (NS_SUCCEEDED (rv))
        {
            const char *iname = NULL;
            iinfo->GetNameShared (&iname);
            name = QString::fromLocal8Bit (iname);
        }
    }

#endif

    return name;
}

#if !defined (Q_OS_WIN32)
#include "COMDefs.moc"
#endif
