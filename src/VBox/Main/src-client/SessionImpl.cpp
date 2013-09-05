/* $Id$ */
/** @file
 * VBox Client Session COM Class implementation in VBoxC.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SessionImpl.h"
#include "ConsoleImpl.h"
#include "Global.h"
#include "ClientTokenHolder.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/err.h>
#include <iprt/process.h>

/**
 *  Local macro to check whether the session is open and return an error if not.
 *  @note Don't forget to do |Auto[Reader]Lock alock (this);| before using this
 *  macro.
 */
#define CHECK_OPEN() \
    do { \
        if (mState != SessionState_Locked) \
            return setError(E_UNEXPECTED, tr ("The session is not locked (session state: %s)"), Global::stringifySessionState(mState)); \
    } while (0)

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT Session::FinalConstruct()
{
    LogFlowThisFunc(("\n"));

    HRESULT rc = init();

    BaseFinalConstruct();

    return rc;
}

void Session::FinalRelease()
{
    LogFlowThisFunc(("\n"));

    uninit();

    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Session object.
 */
HRESULT Session::init()
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    LogFlowThisFuncEnter();

    mState = SessionState_Unlocked;
    mType = SessionType_Null;

    mClientTokenHolder = NULL;

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
void Session::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
    {
        LogFlowThisFunc(("Already uninitialized.\n"));
        LogFlowThisFuncLeave();
        return;
    }

    /* close() needs write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mState != SessionState_Unlocked)
    {
        Assert(mState == SessionState_Locked ||
               mState == SessionState_Spawning);

        HRESULT rc = unlockMachine(true /* aFinalRelease */, false /* aFromServer */);
        AssertComRC(rc);
    }

    LogFlowThisFuncLeave();
}

// ISession properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::COMGETTER(State)(SessionState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aState = mState;

    return S_OK;
}

STDMETHODIMP Session::COMGETTER(Type)(SessionType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_OPEN();

    *aType = mType;
    return S_OK;
}

STDMETHODIMP Session::COMGETTER(Machine)(IMachine **aMachine)
{
    CheckComArgOutPointerValid(aMachine);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_OPEN();

    HRESULT rc;
#ifndef VBOX_COM_INPROC_API_CLIENT
    if (mConsole)
       rc = mConsole->machine().queryInterfaceTo(aMachine);
    else
#endif
       rc = mRemoteMachine.queryInterfaceTo(aMachine);
    if (FAILED(rc))
    {
        /** @todo VBox 3.3: replace E_FAIL with rc here. */
#ifndef VBOX_COM_INPROC_API_CLIENT
        if (mConsole)
            setError(E_FAIL, tr("Failed to query the session machine (%Rhrc)"), rc);
        else
#endif
        if (FAILED_DEAD_INTERFACE(rc))
            setError(E_FAIL, tr("Peer process crashed"));
        else
            setError(E_FAIL, tr("Failed to query the remote session machine (%Rhrc)"), rc);
    }

    return rc;
}

STDMETHODIMP Session::COMGETTER(Console)(IConsole **aConsole)
{
    CheckComArgOutPointerValid(aConsole);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_OPEN();

    HRESULT rc;
#ifndef VBOX_COM_INPROC_API_CLIENT
    if (mConsole)
        rc = mConsole.queryInterfaceTo(aConsole);
    else
#endif
        rc = mRemoteConsole.queryInterfaceTo(aConsole);

    if (FAILED(rc))
    {
        /** @todo VBox 3.3: replace E_FAIL with rc here. */
#ifndef VBOX_COM_INPROC_API_CLIENT
        if (mConsole)
            setError(E_FAIL, tr("Failed to query the console (%Rhrc)"), rc);
        else
#endif
        if (FAILED_DEAD_INTERFACE(rc))
            setError(E_FAIL, tr("Peer process crashed"));
        else
            setError(E_FAIL, tr("Failed to query the remote console (%Rhrc)"), rc);
    }

    return rc;
}

// ISession methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::UnlockMachine()
{
    LogFlowThisFunc(("mState=%d, mType=%d\n", mState, mType));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* close() needs write lock */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_OPEN();

    return unlockMachine(false /* aFinalRelease */, false /* aFromServer */);
}

// IInternalSessionControl methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Session::GetPID(ULONG *aPid)
{
    AssertReturn(aPid, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPid = (ULONG)RTProcSelf();
    AssertCompile(sizeof(*aPid) == sizeof(RTPROCESS));

    return S_OK;
}

STDMETHODIMP Session::GetRemoteConsole(IConsole **aConsole)
{
    LogFlowThisFuncEnter();
    AssertReturn(aConsole, E_POINTER);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mState != SessionState_Unlocked, VBOX_E_INVALID_VM_STATE);

#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertMsgReturn(mType == SessionType_WriteLock && !!mConsole,
                    ("This is not a direct session!\n"),
                    VBOX_E_INVALID_OBJECT_STATE);

    /* return a failure if the session already transitioned to Closing
     * but the server hasn't processed Machine::OnSessionEnd() yet. */
    if (mState != SessionState_Locked)
        return VBOX_E_INVALID_VM_STATE;

    mConsole.queryInterfaceTo(aConsole);

    LogFlowThisFuncLeave();

    return S_OK;

#else  /* VBOX_COM_INPROC_API_CLIENT */
    return VBOX_E_INVALID_OBJECT_STATE;
#endif /* VBOX_COM_INPROC_API_CLIENT */
}

STDMETHODIMP Session::AssignMachine(IMachine *aMachine, LockType_T aLockType,
                                    IN_BSTR aTokenId)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachine=%p\n", aMachine));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mState == SessionState_Unlocked, VBOX_E_INVALID_VM_STATE);

    if (!aMachine)
    {
        /*
         *  A special case: the server informs us that this session has been
         *  passed to IMachine::launchVMProcess() so this session will become
         *  remote (but not existing) when AssignRemoteMachine() is called.
         */

        AssertReturn(mType == SessionType_Null, VBOX_E_INVALID_OBJECT_STATE);
        mType = SessionType_Remote;
        mState = SessionState_Spawning;

        LogFlowThisFuncLeave();
        return S_OK;
    }

    HRESULT rc = E_FAIL;

    /* query IInternalMachineControl interface */
    mControl = aMachine;
    AssertReturn(!!mControl, E_FAIL);

#ifndef VBOX_COM_INPROC_API_CLIENT
    rc = mConsole.createObject();
    AssertComRCReturn(rc, rc);

    rc = mConsole->init(aMachine, mControl, aLockType);
    AssertComRCReturn(rc, rc);
#endif

    Utf8Str strTokenId(aTokenId);
    Assert(!strTokenId.isEmpty());
    /* create the machine client token */
    try
    {
        mClientTokenHolder = new ClientTokenHolder(strTokenId);
        if (!mClientTokenHolder->isReady())
        {
            delete mClientTokenHolder;
            mClientTokenHolder = NULL;
            rc = E_FAIL;
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    /*
     *  Reference the VirtualBox object to ensure the server is up
     *  until the session is closed
     */
    if (SUCCEEDED(rc))
       rc = aMachine->COMGETTER(Parent)(mVirtualBox.asOutParam());

    if (SUCCEEDED(rc))
    {
        mType = SessionType_WriteLock;
        mState = SessionState_Locked;
    }
    else
    {
        /* some cleanup */
        mControl.setNull();
#ifndef VBOX_COM_INPROC_API_CLIENT
        if (!mConsole.isNull())
        {
            mConsole->uninit();
            mConsole.setNull();
        }
#endif
    }

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::AssignRemoteMachine(IMachine *aMachine, IConsole *aConsole)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aMachine=%p, aConsole=%p\n", aMachine, aConsole));

    AssertReturn(aMachine && aConsole, E_INVALIDARG);

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mState == SessionState_Unlocked ||
                  mState == SessionState_Spawning, VBOX_E_INVALID_VM_STATE);

    HRESULT rc = E_FAIL;

    /* query IInternalMachineControl interface */
    mControl = aMachine;
    AssertReturn(!!mControl, E_FAIL);

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
    rc = aMachine->COMGETTER(Parent)(mVirtualBox.asOutParam());

    if (SUCCEEDED(rc))
    {
        /*
         *  RemoteSession type can be already set by AssignMachine() when its
         *  argument is NULL (a special case)
         */
        if (mType != SessionType_Remote)
            mType = SessionType_Shared;
        else
            Assert(mState == SessionState_Spawning);

        mState = SessionState_Locked;
    }
    else
    {
        /* some cleanup */
        mControl.setNull();
        mRemoteMachine.setNull();
        mRemoteConsole.setNull();
    }

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::UpdateMachineState(MachineState_T aMachineState)
{
    AutoCaller autoCaller(this);

    if (autoCaller.state() != Ready)
    {
        /*
         *  We might have already entered Session::uninit() at this point, so
         *  return silently (not interested in the state change during uninit)
         */
        LogFlowThisFunc(("Already uninitialized.\n"));
        return S_OK;
    }

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mState == SessionState_Unlocking)
    {
        LogFlowThisFunc(("Already being unlocked.\n"));
        return S_OK;
    }

    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);

    AssertReturn(!mControl.isNull(), E_FAIL);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(!mConsole.isNull(), E_FAIL);

    return mConsole->updateMachineState(aMachineState);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::Uninitialize()
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);

    HRESULT rc = S_OK;

    if (autoCaller.state() == Ready)
    {
        /* close() needs write lock */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        LogFlowThisFunc(("mState=%s, mType=%d\n", Global::stringifySessionState(mState), mType));

        if (mState == SessionState_Unlocking)
        {
            LogFlowThisFunc(("Already being unlocked.\n"));
            return S_OK;
        }

#ifndef DEBUG_andy /* Don't bug me -- now time to fix this at the moment. */
        AssertReturn(mState == SessionState_Locked ||
                     mState == SessionState_Spawning, VBOX_E_INVALID_VM_STATE);
#endif

        /* close ourselves */
        rc = unlockMachine(false /* aFinalRelease */, true /* aFromServer */);
    }
    else if (autoCaller.state() == InUninit)
    {
        /*
         *  We might have already entered Session::uninit() at this point,
         *  return silently
         */
        LogFlowThisFunc(("Already uninitialized.\n"));
    }
    else
    {
        LogWarningThisFunc(("UNEXPECTED uninitialization!\n"));
        rc = autoCaller.rc();
    }

    LogFlowThisFunc(("rc=%08X\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

STDMETHODIMP Session::OnNetworkAdapterChange(INetworkAdapter *networkAdapter, BOOL changeAdapter)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onNetworkAdapterChange(networkAdapter, changeAdapter);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnSerialPortChange(ISerialPort *serialPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onSerialPortChange(serialPort);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnParallelPortChange(IParallelPort *parallelPort)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onParallelPortChange(parallelPort);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnStorageControllerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onStorageControllerChange();
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnMediumChange(IMediumAttachment *aMediumAttachment, BOOL aForce)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onMediumChange(aMediumAttachment, aForce);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnCPUChange(ULONG aCPU, BOOL aRemove)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onCPUChange(aCPU, aRemove);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnCPUExecutionCapChange(ULONG aExecutionCap)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onCPUExecutionCapChange(aExecutionCap);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnVRDEServerChange(BOOL aRestart)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onVRDEServerChange(aRestart);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnVideoCaptureChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onVideoCaptureChange();
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnUSBControllerChange()
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBControllerChange();
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnSharedFolderChange(BOOL aGlobal)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onSharedFolderChange(aGlobal);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnClipboardModeChange(ClipboardMode_T aClipboardMode)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onClipboardModeChange(aClipboardMode);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnDragAndDropModeChange(DragAndDropMode_T aDragAndDropMode)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onDragAndDropModeChange(aDragAndDropMode);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnUSBDeviceAttach(IUSBDevice *aDevice,
                                        IVirtualBoxErrorInfo *aError,
                                        ULONG aMaskedIfs)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBDeviceAttach(aDevice, aError, aMaskedIfs);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnUSBDeviceDetach(IN_BSTR aId,
                                        IVirtualBoxErrorInfo *aError)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onUSBDeviceDetach(aId, aError);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnShowWindow(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);
#endif

    if (mState != SessionState_Locked)
    {
        /* the call from Machine issued when the session is open can arrive
         * after the session starts closing or gets closed. Note that when
         * aCheck is false, we return E_FAIL to indicate that aWinId we return
         * is not valid */
        *aCanShow = FALSE;
        *aWinId = 0;
        return aCheck ? S_OK : E_FAIL;
    }

#ifndef VBOX_COM_INPROC_API_CLIENT
    return mConsole->onShowWindow(aCheck, aCanShow, aWinId);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onBandwidthGroupChange(aBandwidthGroup);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::OnStorageDeviceChange(IMediumAttachment *aMediumAttachment, BOOL aRemove, BOOL aSilent)
{
    LogFlowThisFunc(("\n"));

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->onStorageDeviceChange(aMediumAttachment, aRemove, aSilent);
#else
    return S_OK;
#endif
}

STDMETHODIMP Session::AccessGuestProperty(IN_BSTR aName, IN_BSTR aValue, IN_BSTR aFlags,
                                          BOOL aIsSetter, BSTR *aRetValue, LONG64 *aRetTimestamp, BSTR *aRetFlags)
{
#if defined(VBOX_WITH_GUEST_PROPS) && !defined(VBOX_COM_INPROC_API_CLIENT)
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    if (mState != SessionState_Locked)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Machine is not locked by session (session state: %s)."),
                        Global::stringifySessionState(mState));
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
    CheckComArgStrNotEmptyOrNull(aName);
    if (!aIsSetter && !VALID_PTR(aRetValue))
        return E_POINTER;
    if (!aIsSetter && !VALID_PTR(aRetTimestamp))
        return E_POINTER;
    if (!aIsSetter && !VALID_PTR(aRetFlags))
        return E_POINTER;
    /* aValue can be NULL for a setter call if the property is to be deleted. */
    if (aIsSetter && (aValue != NULL) && !VALID_PTR(aValue))
        return setError(E_INVALIDARG, tr("Invalid value pointer"));
    /* aFlags can be null if it is to be left as is */
    if (aIsSetter && (aFlags != NULL) && !VALID_PTR(aFlags))
        return setError(E_INVALIDARG, tr("Invalid flags pointer"));

    /* If this session is not in a VM process fend off the call. The caller
     * handles this correctly, by doing the operation in VBoxSVC. */
    if (!mConsole)
        return E_ACCESSDENIED;

    if (!aIsSetter)
        return mConsole->getGuestProperty(aName, aRetValue, aRetTimestamp, aRetFlags);
    else
        return mConsole->setGuestProperty(aName, aValue, aFlags);
#else /* VBOX_WITH_GUEST_PROPS not defined */
    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_PROPS not defined */
}

STDMETHODIMP Session::EnumerateGuestProperties(IN_BSTR aPatterns,
                                               ComSafeArrayOut(BSTR, aNames),
                                               ComSafeArrayOut(BSTR, aValues),
                                               ComSafeArrayOut(LONG64, aTimestamps),
                                               ComSafeArrayOut(BSTR, aFlags))
{
#if defined(VBOX_WITH_GUEST_PROPS) && !defined(VBOX_COM_INPROC_API_CLIENT)
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    if (mState != SessionState_Locked)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Machine is not locked by session (session state: %s)."),
                        Global::stringifySessionState(mState));
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
    if (!VALID_PTR(aPatterns) && (aPatterns != NULL))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aNames))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aValues))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aTimestamps))
        return E_POINTER;
    if (ComSafeArrayOutIsNull(aFlags))
        return E_POINTER;

    /* If this session is not in a VM process fend off the call. The caller
     * handles this correctly, by doing the operation in VBoxSVC. */
    if (!mConsole)
        return E_ACCESSDENIED;

    return mConsole->enumerateGuestProperties(aPatterns,
                                              ComSafeArrayOutArg(aNames),
                                              ComSafeArrayOutArg(aValues),
                                              ComSafeArrayOutArg(aTimestamps),
                                              ComSafeArrayOutArg(aFlags));
#else /* VBOX_WITH_GUEST_PROPS not defined */
    ReturnComNotImplemented();
#endif /* VBOX_WITH_GUEST_PROPS not defined */
}

STDMETHODIMP Session::OnlineMergeMedium(IMediumAttachment *aMediumAttachment,
                                        ULONG aSourceIdx, ULONG aTargetIdx,
                                        IProgress *aProgress)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    if (mState != SessionState_Locked)
        return setError(VBOX_E_INVALID_VM_STATE,
                        tr("Machine is not locked by session (session state: %s)."),
                        Global::stringifySessionState(mState));
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);
    CheckComArgNotNull(aMediumAttachment);

    return mConsole->onlineMergeMedium(aMediumAttachment,
                                       aSourceIdx, aTargetIdx,
                                       aProgress);
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Session::EnableVMMStatistics(BOOL aEnable)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    mConsole->enableVMMStatistics(aEnable);

    return S_OK;
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Session::PauseWithReason(Reason_T aReason)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->pause(aReason);
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Session::ResumeWithReason(Reason_T aReason)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->resume(aReason);
#else
    return E_NOTIMPL;
#endif
}

STDMETHODIMP Session::SaveStateWithReason(Reason_T aReason, IProgress **aProgress)
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    AssertReturn(mState == SessionState_Locked, VBOX_E_INVALID_VM_STATE);
    AssertReturn(mType == SessionType_WriteLock, VBOX_E_INVALID_OBJECT_STATE);
#ifndef VBOX_COM_INPROC_API_CLIENT
    AssertReturn(mConsole, VBOX_E_INVALID_OBJECT_STATE);

    return mConsole->saveState(aReason, aProgress);
#else
    return E_NOTIMPL;
#endif
}

// private methods
///////////////////////////////////////////////////////////////////////////////

/**
 *  Unlocks a machine associated with the current session.
 *
 *  @param aFinalRelease    called as a result of FinalRelease()
 *  @param aFromServer      called as a result of Uninitialize()
 *
 *  @note To be called only from #uninit(), #UnlockMachine() or #Uninitialize().
 *  @note Locks this object for writing.
 */
HRESULT Session::unlockMachine(bool aFinalRelease, bool aFromServer)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aFinalRelease=%d, isFromServer=%d\n",
                      aFinalRelease, aFromServer));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("mState=%s, mType=%d\n", Global::stringifySessionState(mState), mType));

    if (mState != SessionState_Locked)
    {
        Assert(mState == SessionState_Spawning);

        /* The session object is going to be uninitialized before it has been
         * assigned a direct console of the machine the client requested to open
         * a remote session to using IVirtualBox:: openRemoteSession(). It is OK
         * only if this close request comes from the server (for example, it
         * detected that the VM process it started terminated before opening a
         * direct session). Otherwise, it means that the client is too fast and
         * trying to close the session before waiting for the progress object it
         * got from IVirtualBox:: openRemoteSession() to complete, so assert. */
        Assert(aFromServer);

        mState = SessionState_Unlocked;
        mType = SessionType_Null;

        Assert(!mClientTokenHolder);

        LogFlowThisFuncLeave();
        return S_OK;
    }

    /* go to the closing state */
    mState = SessionState_Unlocking;

    if (mType == SessionType_WriteLock)
    {
#ifndef VBOX_COM_INPROC_API_CLIENT
        if (!mConsole.isNull())
        {
            mConsole->uninit();
            mConsole.setNull();
        }
#endif
    }
    else
    {
        mRemoteMachine.setNull();
#ifndef VBOX_COM_INPROC_API_CLIENT
        mRemoteConsole.setNull();
#endif
    }

    ComPtr<IProgress> progress;

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
        alock.release();

        LogFlowThisFunc(("Calling mControl->OnSessionEnd()...\n"));
        HRESULT rc = mControl->OnSessionEnd(this, progress.asOutParam());
        LogFlowThisFunc(("mControl->OnSessionEnd()=%08X\n", rc));

        alock.acquire();

        /*
         *  If we get E_UNEXPECTED this means that the direct session has already
         *  been closed, we're just too late with our notification and nothing more
         *
         *  bird: Seems E_ACCESSDENIED is what gets returned these days; see
         *        VirtualBoxBase::addCaller.
         */
        if (mType != SessionType_WriteLock && (rc == E_UNEXPECTED || rc == E_ACCESSDENIED))
            rc = S_OK;

#ifndef DEBUG_bird /* I don't want clients crashing on me just because VBoxSVC went belly up. */
        AssertComRC(rc);
#endif
    }

    mControl.setNull();

    if (mType == SessionType_WriteLock)
    {
        if (mClientTokenHolder)
        {
            delete mClientTokenHolder;
            mClientTokenHolder = NULL;
        }

        if (!aFinalRelease && !aFromServer)
        {
            /*
             *  Wait for the server to grab the semaphore and destroy the session
             *  machine (allowing us to open a new session with the same machine
             *  once this method returns)
             */
            Assert(!!progress);
            if (progress)
                progress->WaitForCompletion(-1);
        }
    }

    mState = SessionState_Unlocked;
    mType = SessionType_Null;

    /* release the VirtualBox instance as the very last step */
    mVirtualBox.setNull();

    LogFlowThisFuncLeave();
    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
