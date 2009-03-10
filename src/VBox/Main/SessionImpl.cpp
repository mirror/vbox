/** @file
 *
 * VBox Client Session COM Class implementation
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#if defined(RT_OS_WINDOWS)
#elif defined(RT_OS_LINUX)
#endif

#ifdef VBOX_WITH_SYS_V_IPC_SESSION_WATCHER
#   include <errno.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/ipc.h>
#   include <sys/sem.h>
#endif

#include "SessionImpl.h"
#include "ConsoleImpl.h"

#include "Logging.h"

#include <VBox/err.h>
#include <iprt/process.h>

#if defined(RT_OS_WINDOWS) || defined (RT_OS_OS2)
/** VM IPC mutex holder thread */
static DECLCALLBACK(int) IPCMutexHolderThread (RTTHREAD Thread, void *pvUser);
#endif

/**
 *  Local macro to check whether the session is open and return an error if not.
 *  @note Don't forget to do |Auto[Reader]Lock alock (this);| before using this
 *  macro.
 */
#define CHECK_OPEN() \
    do { \
        if (mState != SessionState_Open) \
            return setError (E_UNEXPECTED, \
                tr ("The session is not open")); \
    } while (0)

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT Session::FinalConstruct()
{
    LogFlowThisFunc (("\n"));

    return init();
}

void Session::FinalRelease()
{
    LogFlowThisFunc (("\n"));

    uninit (true /* aFinalRelease */);
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Session object.
 */
HRESULT Session::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();

    mState = SessionState_Closed;
    mType = SessionType_Null;

#if defined(RT_OS_WINDOWS)
    mIPCSem = NULL;
    mIPCThreadSem = NULL;
#elif defined(RT_OS_OS2)
    mIPCThread = NIL_RTTHREAD;
    mIPCThreadSem = NIL_RTSEMEVENT;
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    mIPCSem = -1;
#else
# error "Port me!"
#endif

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 *  Uninitializes the Session object.
 *
 *  @note Locks this object for writing.
 */
void Session::uninit (bool aFinalRelease)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aFinalRelease=%d\n", aFinalRelease));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc (("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    /* close() needs write lock */
    AutoWriteLock alock (this);

    if (mState != SessionState_Closed)
    {
        Assert (mState == SessionState_Open ||
                mState == SessionState_Spawning);

        HRESULT rc = close (aFinalRelease, false /* aFromServer */);
        AssertComRC (rc);
    }

    LogFlowThisFuncLeave();
}

// ISession properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::COMGETTER(State) (SessionState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aState = mState;

    return S_OK;
}

STDMETHODIMP Session::COMGETTER(Type) (SessionType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    CHECK_OPEN();

    *aType = mType;
    return S_OK;
}

STDMETHODIMP Session::COMGETTER(Machine) (IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    CHECK_OPEN();

    HRESULT rc = E_FAIL;

    if (mConsole)
        rc = mConsole->machine().queryInterfaceTo (aMachine);
    else
        rc = mRemoteMachine.queryInterfaceTo (aMachine);
    ComAssertComRC (rc);

    return rc;
}

STDMETHODIMP Session::COMGETTER(Console) (IConsole **aConsole)
{
    CheckComArgOutPointerValid(aConsole);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    CHECK_OPEN();

    HRESULT rc = E_FAIL;

    if (mConsole)
        rc = mConsole.queryInterfaceTo (aConsole);
    else
        rc = mRemoteConsole.queryInterfaceTo (aConsole);
    ComAssertComRC (rc);

    return rc;
}

// ISession methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::Close()
{
    LogFlowThisFunc (("mState=%d, mType=%d\n", mState, mType));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* close() needs write lock */
    AutoWriteLock alock (this);

    CHECK_OPEN();

    return close (false /* aFinalRelease */, false /* aFromServer */);
}

// IInternalSessionControl methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::GetPID (ULONG *aPid)
{
    AssertReturn (aPid, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

    *aPid = (ULONG) RTProcSelf();
    AssertCompile (sizeof (*aPid) == sizeof (RTPROCESS));

    return S_OK;
}

STDMETHODIMP Session::GetRemoteConsole (IConsole **aConsole)
{
    AssertReturn (aConsole, E_POINTER);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

    AssertReturn (mState != SessionState_Closed, VBOX_E_INVALID_VM_STATE);

    AssertMsgReturn (mType == SessionType_Direct && !!mConsole,
      ("This is not a direct session!\n"), VBOX_E_INVALID_OBJECT_STATE);

    /* return a failure if the session already transitioned to Closing
     * but the server hasn't processed Machine::OnSessionEnd() yet. */
    if (mState != SessionState_Open)
        return VBOX_E_INVALID_VM_STATE;

    mConsole.queryInterfaceTo (aConsole);

    return S_OK;
}

STDMETHODIMP Session::AssignMachine (IMachine *aMachine)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachine=%p\n", aMachine));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mState == SessionState_Closed, VBOX_E_INVALID_VM_STATE);

    if (!aMachine)
    {
        /*
         *  A special case: the server informs us that this session has been
         *  passed to IVirtualBox::OpenRemoteSession() so this session will
         *  become remote (but not existing) when AssignRemoteMachine() is
         *  called.
         */

        AssertReturn (mType == SessionType_Null, VBOX_E_INVALID_OBJECT_STATE);
        mType = SessionType_Remote;
        mState = SessionState_Spawning;

        LogFlowThisFuncLeave();
        return S_OK;
    }

    HRESULT rc = E_FAIL;

    /* query IInternalMachineControl interface */
    mControl = aMachine;
    AssertReturn (!!mControl, E_FAIL);

    rc = mConsole.createObject();
    AssertComRCReturn (rc, rc);

    rc = mConsole->init (aMachine, mControl);
    AssertComRCReturn (rc, rc);

    rc = grabIPCSemaphore();

    /*
     *  Reference the VirtualBox object to ensure the server is up
     *  until the session is closed
     */
    if (SUCCEEDED (rc))
       rc = aMachine->COMGETTER(Parent) (mVirtualBox.asOutParam());

    if (SUCCEEDED (rc))
    {
        mType = SessionType_Direct;
        mState = SessionState_Open;
    }
    else
    {
        /* some cleanup */
        mControl.setNull();
        mConsole->uninit();
        mConsole.setNull();
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::AssignRemoteMachine (IMachine *aMachine, IConsole *aConsole)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aMachine=%p, aConsole=%p\n", aMachine, aConsole));

    AssertReturn (aMachine && aConsole, E_INVALIDARG);

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock (this);

    AssertReturn (mState == SessionState_Closed ||
                  mState == SessionState_Spawning, VBOX_E_INVALID_VM_STATE);

    HRESULT rc = E_FAIL;

    /* query IInternalMachineControl interface */
    mControl = aMachine;
    AssertReturn (!!mControl, E_FAIL); // This test appears to be redundant --JS

    /// @todo (dmik)
    //      currently, the remote session returns the same machine and
    //      console objects as the direct session, thus giving the
    //      (remote) client full control over the direct session. For the
    //      console, it is the desired behavior (the ability to control
    //      VM execution is a must for the remote session). What about
    //      the machine object, we may want to prevent the remote client
    //      from modifying machine data. In this case, we must:
    //      1)  assign the Machine object (instead of the SessionMachine
    //          object that is passed to this method) to mRemoteMachine;
    //      2)  remove GetMachine() property from the IConsole interface
    //          because it always returns the SessionMachine object
    //          (alternatively, we can supply a separate IConsole
    //          implementation that will return the Machine object in
    //          response to GetMachine()).

    mRemoteMachine = aMachine;
    mRemoteConsole = aConsole;

    /*
     *  Reference the VirtualBox object to ensure the server is up
     *  until the session is closed
     */
    rc = aMachine->COMGETTER(Parent) (mVirtualBox.asOutParam());

    if (SUCCEEDED (rc))
    {
        /*
         *  RemoteSession type can be already set by AssignMachine() when its
         *  argument is NULL (a special case)
         */
        if (mType != SessionType_Remote)
            mType = SessionType_Existing;
        else
            Assert (mState == SessionState_Spawning);

        mState = SessionState_Open;
    }
    else
    {
        /* some cleanup */
        mControl.setNull();
        mRemoteMachine.setNull();
        mRemoteConsole.setNull();
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::UpdateMachineState (MachineState_T aMachineState)
{
    AutoCaller autoCaller (this);

    if (autoCaller.state() != Ready)
    {
        /*
         *  We might have already entered Session::uninit() at this point, so
         *  return silently (not interested in the state change during uninit)
         */
        LogFlowThisFunc (("Already uninitialized.\n"));
        return S_OK;
    }

    AutoReadLock alock (this);

    if (mState == SessionState_Closing)
    {
        LogFlowThisFunc (("Already being closed.\n"));
        return S_OK;
    }

    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    AssertReturn (!mControl.isNull(), E_FAIL);
    AssertReturn (!mConsole.isNull(), E_FAIL);

    return mConsole->updateMachineState (aMachineState);
}

STDMETHODIMP Session::Uninitialize()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller (this);

    HRESULT rc = S_OK;

    if (autoCaller.state() == Ready)
    {
        /* close() needs write lock */
        AutoWriteLock alock (this);

        LogFlowThisFunc (("mState=%d, mType=%d\n", mState, mType));

        if (mState == SessionState_Closing)
        {
            LogFlowThisFunc (("Already being closed.\n"));
            return S_OK;
        }

        AssertReturn (mState == SessionState_Open ||
                      mState == SessionState_Spawning, VBOX_E_INVALID_VM_STATE);

        /* close ourselves */
        rc = close (false /* aFinalRelease */, true /* aFromServer */);
    }
    else if (autoCaller.state() == InUninit)
    {
        /*
         *  We might have already entered Session::uninit() at this point,
         *  return silently
         */
        LogFlowThisFunc (("Already uninitialized.\n"));
    }
    else
    {
        LogWarningThisFunc (("UNEXPECTED uninitialization!\n"));
        rc = autoCaller.rc();
    }

    LogFlowThisFunc (("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::OnDVDDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onDVDDriveChange();
}

STDMETHODIMP Session::OnFloppyDriveChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onFloppyDriveChange();
}

STDMETHODIMP Session::OnNetworkAdapterChange(INetworkAdapter *networkAdapter)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onNetworkAdapterChange(networkAdapter);
}

STDMETHODIMP Session::OnSerialPortChange(ISerialPort *serialPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onSerialPortChange(serialPort);
}

STDMETHODIMP Session::OnParallelPortChange(IParallelPort *parallelPort)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onParallelPortChange(parallelPort);
}

STDMETHODIMP Session::OnVRDPServerChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onVRDPServerChange();
}

STDMETHODIMP Session::OnUSBControllerChange()
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBControllerChange();
}

STDMETHODIMP Session::OnSharedFolderChange (BOOL aGlobal)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onSharedFolderChange (aGlobal);
}

STDMETHODIMP Session::OnUSBDeviceAttach (IUSBDevice *aDevice,
                                         IVirtualBoxErrorInfo *aError,
                                         ULONG aMaskedIfs)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBDeviceAttach (aDevice, aError, aMaskedIfs);
}

STDMETHODIMP Session::OnUSBDeviceDetach (IN_GUID aId,
                                         IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc (("\n"));

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);
    AssertReturn (mState == SessionState_Open, VBOX_E_INVALID_VM_STATE);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBDeviceDetach (aId, aError);
}

STDMETHODIMP Session::OnShowWindow (BOOL aCheck, BOOL *aCanShow, ULONG64 *aWinId)
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock (this);

    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);

    if (mState != SessionState_Open)
    {
        /* the call from Machine issued when the session is open can arrive
         * after the session starts closing or gets closed. Note that when
         * aCheck is false, we return E_FAIL to indicate that aWinId we return
         * is not valid */
        *aCanShow = FALSE;
        *aWinId = 0;
        return aCheck ? S_OK : E_FAIL;
    }

    return mConsole->onShowWindow (aCheck, aCanShow, aWinId);
}

STDMETHODIMP Session::AccessGuestProperty (IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags,
                                           BOOL aIsSetter, BSTR *aRetValue, ULONG64 *aRetTimestamp, BSTR *aRetFlags)
{
#ifdef VBOX_WITH_GUEST_PROPS
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    if (mState != SessionState_Open)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Machine session is not open (session state: %d)."),
            mState);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);
    CheckComArgNotNull(aName);
    if (!aIsSetter && !VALID_PTR (aRetValue))
        return E_POINTER;
    if (!aIsSetter && !VALID_PTR (aRetTimestamp))
        return E_POINTER;
    if (!aIsSetter && !VALID_PTR (aRetFlags))
        return E_POINTER;
    /* aValue can be NULL for a setter call if the property is to be deleted. */
    if (aIsSetter && (aValue != NULL) && !VALID_PTR (aValue))
        return E_INVALIDARG;
    /* aFlags can be null if it is to be left as is */
    if (aIsSetter && (aFlags != NULL) && !VALID_PTR (aFlags))
        return E_INVALIDARG;
    if (!aIsSetter)
        return mConsole->getGuestProperty (aName, aRetValue, aRetTimestamp, aRetFlags);
    else
        return mConsole->setGuestProperty (aName, aValue, aFlags);
#else /* VBOX_WITH_GUEST_PROPS not defined */
    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_PROPS not defined */
}

STDMETHODIMP Session::EnumerateGuestProperties (IN_BSTR aPatterns,
                                                ComSafeArrayOut(BSTR, aNames),
                                                ComSafeArrayOut(BSTR, aValues),
                                                ComSafeArrayOut(ULONG64, aTimestamps),
                                                ComSafeArrayOut(BSTR, aFlags))
{
#ifdef VBOX_WITH_GUEST_PROPS
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), autoCaller.rc());

    if (mState != SessionState_Open)
        return setError (VBOX_E_INVALID_VM_STATE,
            tr ("Machine session is not open (session state: %d)."),
            mState);
    AssertReturn (mType == SessionType_Direct, VBOX_E_INVALID_OBJECT_STATE);
    if (!VALID_PTR (aPatterns) && (aPatterns != NULL))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aNames))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aValues))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aTimestamps))
        return E_POINTER;
    if (ComSafeArrayOutIsNull (aFlags))
        return E_POINTER;
    return mConsole->enumerateGuestProperties(aPatterns,
                                              ComSafeArrayOutArg(aNames),
                                              ComSafeArrayOutArg(aValues),
                                              ComSafeArrayOutArg(aTimestamps),
                                              ComSafeArrayOutArg(aFlags));
#else /* VBOX_WITH_GUEST_PROPS not defined */
    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_PROPS not defined */
}

// private methods
///////////////////////////////////////////////////////////////////////////////

/**
 *  Closes the current session.
 *
 *  @param aFinalRelease    called as a result of FinalRelease()
 *  @param aFromServer      called as a result of Uninitialize()
 *
 *  @note To be called only from #uninit(), #Close() or #Uninitialize().
 *  @note Locks this object for writing.
 */
HRESULT Session::close (bool aFinalRelease, bool aFromServer)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc (("aFinalRelease=%d, isFromServer=%d\n",
                      aFinalRelease, aFromServer));

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    LogFlowThisFunc (("mState=%d, mType=%d\n", mState, mType));

    if (mState != SessionState_Open)
    {
        Assert (mState == SessionState_Spawning);

        /* The session object is going to be uninitialized before it has been
         * assigned a direct console of the machine the client requested to open
         * a remote session to using IVirtualBox:: openRemoteSession(). It is OK
         * only if this close reqiest comes from the server (for example, it
         * detected that the VM process it started terminated before opening a
         * direct session). Otherwise, it means that the client is too fast and
         * trying to close the session before waiting for the progress object it
         * got from IVirtualBox:: openRemoteSession() to complete, so assert. */
        Assert (aFromServer);

        mState = SessionState_Closed;
        mType = SessionType_Null;
#if defined(RT_OS_WINDOWS)
        Assert (!mIPCSem && !mIPCThreadSem);
#elif defined(RT_OS_OS2)
        Assert (mIPCThread == NIL_RTTHREAD &&
                mIPCThreadSem == NIL_RTSEMEVENT);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
        Assert (mIPCSem == -1);
#else
# error "Port me!"
#endif
        LogFlowThisFuncLeave();
        return S_OK;
    }

    /* go to the closing state */
    mState =  SessionState_Closing;

    if (mType == SessionType_Direct)
    {
        mConsole->uninit();
        mConsole.setNull();
    }
    else
    {
        mRemoteMachine.setNull();
        mRemoteConsole.setNull();
    }

    ComPtr <IProgress> progress;

    if (!aFinalRelease && !aFromServer)
    {
        /*
         *  We trigger OnSessionEnd() only when the session closes itself using
         *  Close(). Note that if isFinalRelease = TRUE here, this means that
         *  the client process has already initialized the termination procedure
         *  without issuing Close() and the IPC channel is no more operational --
         *  so we cannot call the server's method (it will definitely fail). The
         *  server will instead simply detect the abnormal client death (since
         *  OnSessionEnd() is not called) and reset the machine state to Aborted.
         */

        /*
         *  while waiting for OnSessionEnd() to complete one of our methods
         *  can be called by the server (for example, Uninitialize(), if the
         *  direct session has initiated a closure just a bit before us) so
         *  we need to release the lock to avoid deadlocks. The state is already
         *  SessionState_Closing here, so it's safe.
         */
        alock.leave();

        LogFlowThisFunc (("Calling mControl->OnSessionEnd()...\n"));
        HRESULT rc = mControl->OnSessionEnd (this, progress.asOutParam());
        LogFlowThisFunc (("mControl->OnSessionEnd()=%08X\n", rc));

        alock.enter();

        /*
         *  If we get E_UNEXPECTED this means that the direct session has already
         *  been closed, we're just too late with our notification and nothing more
         */
        if (mType != SessionType_Direct && rc == E_UNEXPECTED)
            rc = S_OK;

        AssertComRC (rc);
    }

    mControl.setNull();

    if (mType == SessionType_Direct)
    {
        releaseIPCSemaphore();
        if (!aFinalRelease && !aFromServer)
        {
            /*
             *  Wait for the server to grab the semaphore and destroy the session
             *  machine (allowing us to open a new session with the same machine
             *  once this method returns)
             */
            Assert (!!progress);
            if (progress)
                progress->WaitForCompletion (-1);
        }
    }

    mState = SessionState_Closed;
    mType = SessionType_Null;

    /* release the VirtualBox instance as the very last step */
    mVirtualBox.setNull();

    LogFlowThisFuncLeave();
    return S_OK;
}

/** @note To be called only from #AssignMachine() */
HRESULT Session::grabIPCSemaphore()
{
    HRESULT rc = E_FAIL;

    /* open the IPC semaphore based on the sessionId and try to grab it */
    Bstr ipcId;
    rc = mControl->GetIPCId (ipcId.asOutParam());
    AssertComRCReturnRC (rc);

    LogFlowThisFunc (("ipcId='%ls'\n", ipcId.raw()));

#if defined(RT_OS_WINDOWS)

    /*
     *  Since Session is an MTA object, this method can be executed on
     *  any thread, and this thread will not necessarily match the thread on
     *  which close() will be called later. Therefore, we need a separate
     *  thread to hold the IPC mutex and then release it in close().
     */

    mIPCThreadSem = ::CreateEvent (NULL, FALSE, FALSE, NULL);
    AssertMsgReturn (mIPCThreadSem,
                     ("Cannot create an event sem, err=%d", ::GetLastError()),
                     E_FAIL);

    void *data [3];
    data [0] = (void *) (BSTR) ipcId;
    data [1] = (void *) mIPCThreadSem;
    data [2] = 0; /* will get an output from the thread */

    /* create a thread to hold the IPC mutex until signalled to release it */
    RTTHREAD tid;
    int vrc = RTThreadCreate (&tid, IPCMutexHolderThread, (void *) data,
                              0, RTTHREADTYPE_MAIN_WORKER, 0, "IPCHolder");
    AssertRCReturn (vrc, E_FAIL);

    /* wait until thread init is completed */
    DWORD wrc = ::WaitForSingleObject (mIPCThreadSem, INFINITE);
    AssertMsg (wrc == WAIT_OBJECT_0, ("Wait failed, err=%d\n", ::GetLastError()));
    Assert (data [2]);

    if (wrc == WAIT_OBJECT_0 && data [2])
    {
        /* memorize the event sem we should signal in close() */
        mIPCSem = (HANDLE) data [2];
        rc = S_OK;
    }
    else
    {
        ::CloseHandle (mIPCThreadSem);
        mIPCThreadSem = NULL;
        rc = E_FAIL;
    }

#elif defined(RT_OS_OS2)

    /* We use XPCOM where any message (including close()) can arrive on any
     * worker thread (which will not necessarily match this thread that opens
     * the mutex). Therefore, we need a separate thread to hold the IPC mutex
     * and then release it in close(). */

    int vrc = RTSemEventCreate (&mIPCThreadSem);
    AssertRCReturn (vrc, E_FAIL);

    void *data [3];
    data [0] = (void *) ipcId.raw();
    data [1] = (void *) mIPCThreadSem;
    data [2] = (void *) false; /* will get the thread result here */

    /* create a thread to hold the IPC mutex until signalled to release it */
    vrc = RTThreadCreate (&mIPCThread, IPCMutexHolderThread, (void *) data,
                          0, RTTHREADTYPE_MAIN_WORKER, 0, "IPCHolder");
    AssertRCReturn (vrc, E_FAIL);

    /* wait until thread init is completed */
    vrc = RTThreadUserWait (mIPCThread, RT_INDEFINITE_WAIT);
    AssertReturn (RT_SUCCESS (vrc) || vrc == VERR_INTERRUPTED, E_FAIL);

    /* the thread must succeed */
    AssertReturn ((bool) data [2], E_FAIL);

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

# ifdef VBOX_WITH_NEW_SYS_V_KEYGEN
    Utf8Str ipcKey = ipcId;
    key_t key = RTStrToUInt32(ipcKey.raw());
    AssertMsgReturn (key != 0,
                    ("Key value of 0 is not valid for IPC semaphore"),
                    E_FAIL);
# else /* !VBOX_WITH_NEW_SYS_V_KEYGEN */
    Utf8Str semName = ipcId;
    char *pszSemName = NULL;
    RTStrUtf8ToCurrentCP (&pszSemName, semName);
    key_t key = ::ftok (pszSemName, 'V');
    RTStrFree (pszSemName);
# endif /* !VBOX_WITH_NEW_SYS_V_KEYGEN */

    mIPCSem = ::semget (key, 0, 0);
    AssertMsgReturn (mIPCSem >= 0,
                    ("Cannot open IPC semaphore, errno=%d", errno),
                    E_FAIL);

    /* grab the semaphore */
    ::sembuf sop = { 0,  -1, SEM_UNDO };
    int rv = ::semop (mIPCSem, &sop, 1);
    AssertMsgReturn (rv == 0,
                    ("Cannot grab IPC semaphore, errno=%d", errno),
                    E_FAIL);

#else
# error "Port me!"
#endif

    return rc;
}

/** @note To be called only from #close() */
void Session::releaseIPCSemaphore()
{
    /* release the IPC semaphore */
#if defined(RT_OS_WINDOWS)

    if (mIPCSem && mIPCThreadSem)
    {
        /*
         *  tell the thread holding the IPC mutex to release it;
         *  it will close mIPCSem handle
         */
        ::SetEvent (mIPCSem);
        /* wait for the thread to finish */
        ::WaitForSingleObject (mIPCThreadSem, INFINITE);
        ::CloseHandle (mIPCThreadSem);

        mIPCThreadSem = NULL;
        mIPCSem = NULL;
    }

#elif defined(RT_OS_OS2)

    if (mIPCThread != NIL_RTTHREAD)
    {
        Assert (mIPCThreadSem != NIL_RTSEMEVENT);

        /* tell the thread holding the IPC mutex to release it */
        int vrc = RTSemEventSignal (mIPCThreadSem);
        AssertRC (vrc == NO_ERROR);

        /* wait for the thread to finish */
        vrc = RTThreadUserWait (mIPCThread, RT_INDEFINITE_WAIT);
        Assert (RT_SUCCESS (vrc) || vrc == VERR_INTERRUPTED);

        mIPCThread = NIL_RTTHREAD;
    }

    if (mIPCThreadSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy (mIPCThreadSem);
        mIPCThreadSem = NIL_RTSEMEVENT;
    }

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

    if (mIPCSem >= 0)
    {
        ::sembuf sop = { 0, 1, SEM_UNDO };
        ::semop (mIPCSem, &sop, 1);

        mIPCSem = -1;
    }

#else
# error "Port me!"
#endif
}

#if defined(RT_OS_WINDOWS)
/** VM IPC mutex holder thread */
DECLCALLBACK(int) IPCMutexHolderThread (RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    Assert (pvUser);
    void **data = (void **) pvUser;

    BSTR sessionId = (BSTR) data [0];
    HANDLE initDoneSem = (HANDLE) data [1];

    HANDLE ipcMutex = ::OpenMutex (MUTEX_ALL_ACCESS, FALSE, sessionId);
    AssertMsg (ipcMutex, ("cannot open IPC mutex, err=%d\n", ::GetLastError()));

    if (ipcMutex)
    {
        /* grab the mutex */
        DWORD wrc = ::WaitForSingleObject (ipcMutex, 0);
        AssertMsg (wrc == WAIT_OBJECT_0, ("cannot grab IPC mutex, err=%d\n", wrc));
        if (wrc == WAIT_OBJECT_0)
        {
            HANDLE finishSem = ::CreateEvent (NULL, FALSE, FALSE, NULL);
            AssertMsg (finishSem, ("cannot create event sem, err=%d\n", ::GetLastError()));
            if (finishSem)
            {
                data [2] = (void *) finishSem;
                /* signal we're done with init */
                ::SetEvent (initDoneSem);
                /* wait until we're signaled to release the IPC mutex */
                ::WaitForSingleObject (finishSem, INFINITE);
                /* release the IPC mutex */
                LogFlow (("IPCMutexHolderThread(): releasing IPC mutex...\n"));
                BOOL success = ::ReleaseMutex (ipcMutex);
                AssertMsg (success, ("cannot release mutex, err=%d\n", ::GetLastError()));
                ::CloseHandle (ipcMutex);
                ::CloseHandle (finishSem);
            }
        }
    }

    /* signal we're done */
    ::SetEvent (initDoneSem);

    LogFlowFuncLeave();

    return 0;
}
#endif

#if defined(RT_OS_OS2)
/** VM IPC mutex holder thread */
DECLCALLBACK(int) IPCMutexHolderThread (RTTHREAD Thread, void *pvUser)
{
    LogFlowFuncEnter();

    Assert (pvUser);
    void **data = (void **) pvUser;

    Utf8Str ipcId = (BSTR) data [0];
    RTSEMEVENT finishSem = (RTSEMEVENT) data [1];

    LogFlowFunc (("ipcId='%s', finishSem=%p\n", ipcId.raw(), finishSem));

    HMTX ipcMutex = NULLHANDLE;
    APIRET arc = ::DosOpenMutexSem ((PSZ) ipcId.raw(), &ipcMutex);
    AssertMsg (arc == NO_ERROR, ("cannot open IPC mutex, arc=%ld\n", arc));

    if (arc == NO_ERROR)
    {
        /* grab the mutex */
        LogFlowFunc (("grabbing IPC mutex...\n"));
        arc = ::DosRequestMutexSem (ipcMutex, SEM_IMMEDIATE_RETURN);
        AssertMsg (arc == NO_ERROR, ("cannot grab IPC mutex, arc=%ld\n", arc));
        if (arc == NO_ERROR)
        {
            /* store the answer */
            data [2] = (void *) true;
            /* signal we're done */
            int vrc = RTThreadUserSignal (Thread);
            AssertRC (vrc);

            /* wait until we're signaled to release the IPC mutex */
            LogFlowFunc (("waiting for termination signal..\n"));
            vrc = RTSemEventWait (finishSem, RT_INDEFINITE_WAIT);
            Assert (arc == ERROR_INTERRUPT || ERROR_TIMEOUT);

            /* release the IPC mutex */
            LogFlowFunc (("releasing IPC mutex...\n"));
            arc = ::DosReleaseMutexSem (ipcMutex);
            AssertMsg (arc == NO_ERROR, ("cannot release mutex, arc=%ld\n", arc));
        }

        ::DosCloseMutexSem (ipcMutex);
    }

    /* store the answer */
    data [1] = (void *) false;
    /* signal we're done */
    int vrc = RTThreadUserSignal (Thread);
    AssertRC (vrc);

    LogFlowFuncLeave();

    return 0;
}
#endif
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
