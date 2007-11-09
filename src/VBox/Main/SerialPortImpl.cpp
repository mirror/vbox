/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SerialPortImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SerialPort)

HRESULT SerialPort::FinalConstruct()
{
    return S_OK;
}

void SerialPort::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Serial Port object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT SerialPort::init (Machine *aParent, ULONG aSlot)
{
    LogFlowThisFunc (("aParent=%p, aSlot=%d\n", aParent, aSlot));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    mData.allocate();

    /* initialize data */
    mData->mSlot = aSlot;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the Serial Port object given another serial port object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT SerialPort::init (Machine *aParent, SerialPort *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT SerialPort::initCopy (Machine *aParent, SerialPort *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_UNEXPECTED);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReaderLock thatLock (aThat);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void SerialPort::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/** 
 *  @note Locks this object for writing.
 */
bool SerialPort::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoLock alock (this);

    bool changed = false;

    if (mData.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = mData.hasActualChanges();
        mData.rollback();
    }

    return changed;
}

/** 
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void SerialPort::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* lock both for writing since we modify both */
    AutoMultiLock <2> alock (this->wlock(), AutoLock::maybeWlock (mPeer));

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/** 
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void SerialPort::copyFrom (SerialPort *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (mPeer);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading */
    AutoMultiLock <2> alock (this->wlock(), aThat->rlock());

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

HRESULT SerialPort::loadSettings (CFGNODE aNode, ULONG aSlot)
{
    LogFlowThisFunc (("aMachine=%p\n", aNode));

    AssertReturn (aNode, E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoLock alock (this);

    CFGNODE portNode = NULL;
    CFGLDRGetChildNode (aNode, "Port", aSlot, &portNode);

    /* slot number (required) */
    /* slot unicity is guaranteed by XML Schema */
    uint32_t uSlot = 0;
    CFGLDRQueryUInt32 (portNode, "slot", &uSlot);
    /* enabled (required) */
    bool fEnabled = false;
    CFGLDRQueryBool (portNode, "enabled", &fEnabled);
    uint32_t uIOBase;
    /* I/O base (required) */
    CFGLDRQueryUInt32 (portNode, "IOBase", &uIOBase);
    /* IRQ (required) */
    uint32_t uIRQ;
    CFGLDRQueryUInt32 (portNode, "IRQ", &uIRQ);
    /* host mode (required) */
    Bstr mode;
    CFGLDRQueryBSTR (portNode, "hostMode", mode.asOutParam());
    if (mode == L"HostPipe")
        mData->mHostMode = PortMode_HostPipePort;
    else if (mode == L"HostDevice")
        mData->mHostMode = PortMode_HostDevicePort;
    else
        mData->mHostMode = PortMode_DisconnectedPort;
    /* pipe/device path */
    Bstr path;
    CFGLDRQueryBSTR(portNode, "path", path.asOutParam());
    /* server mode */
    bool fServer = true;
    CFGLDRQueryBool (portNode, "server", &fServer);

    mData->mEnabled  = fEnabled;
    mData->mSlot     = uSlot;
    mData->mIOBase   = uIOBase;
    mData->mIRQ      = uIRQ;
    mData->mPath     = path;
    mData->mServer   = fServer;

    return S_OK;
}

HRESULT SerialPort::saveSettings (CFGNODE aNode)
{
    AssertReturn (aNode, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    CFGNODE portNode = 0;
    int vrc = CFGLDRAppendChildNode (aNode, "Port", &portNode);
    ComAssertRCRet (vrc, E_FAIL);

    const char *mode;
    switch (mData->mHostMode)
    {
        default:
        case PortMode_DisconnectedPort:
            mode = "Disconnected";
            break;
        case PortMode_HostPipePort:
            mode = "HostPipe";
            break;
        case PortMode_HostDevicePort:
            mode = "HostDevice";
            break;
    }
    CFGLDRSetUInt32   (portNode, "slot",     mData->mSlot);
    CFGLDRSetBool     (portNode, "enabled",  !!mData->mEnabled);
    CFGLDRSetUInt32Ex (portNode, "IOBase",   mData->mIOBase, 16);
    CFGLDRSetUInt32   (portNode, "IRQ",      mData->mIRQ);
    CFGLDRSetString   (portNode, "hostMode", mode);
    if (mData->mHostMode != PortMode_DisconnectedPort)
    {
        CFGLDRSetBSTR (portNode, "path",    mData->mPath);
        if (mData->mHostMode == PortMode_HostPipePort)
            CFGLDRSetBool (portNode, "server",  !!mData->mServer);
    }

    return S_OK;
}

// ISerialPort properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SerialPort::COMGETTER(Enabled) (BOOL *aEnabled)
{
    if (!aEnabled)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onSerialPortChange (this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(HostMode) (PortMode_T *aHostMode)
{
    if (!aHostMode)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aHostMode = mData->mHostMode;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(HostMode) (PortMode_T aHostMode)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    if (mData->mHostMode != aHostMode)
    {
        mData.backup();
        mData->mHostMode = aHostMode;
        if (aHostMode == PortMode_DisconnectedPort)
        {
            mData->mPath.setNull();
            mData->mServer = false;
        }
        emitChangeEvent = true;
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onSerialPortChange (this);
    }

    return rc;
}

STDMETHODIMP SerialPort::COMGETTER(Slot) (ULONG *aSlot)
{
    if (!aSlot)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aSlot = mData->mSlot;

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(IRQ) (ULONG *aIRQ)
{
    if (!aIRQ)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aIRQ = mData->mIRQ;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(IRQ)(ULONG aIRQ)
{
    /* check IRQ limits
     * (when changing this, make sure it corresponds to XML schema */
    if (aIRQ > 255)
        return setError (E_INVALIDARG,
            tr ("Invalid IRQ number: %lu (must be in range [0, %lu])"),
                aIRQ, 255);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    if (mData->mIRQ != aIRQ)
    {
        mData.backup();
        mData->mIRQ = aIRQ;
        emitChangeEvent = true;
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onSerialPortChange (this);
    }

    return rc;
}

STDMETHODIMP SerialPort::COMGETTER(IOBase) (ULONG *aIOBase)
{
    if (!aIOBase)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aIOBase = mData->mIOBase;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(IOBase)(ULONG aIOBase)
{
    /* check IOBase limits
     * (when changing this, make sure it corresponds to XML schema */
    if (aIOBase > 0xFFFF)
        return setError (E_INVALIDARG,
            tr ("Invalid I/O port base address: %lu (must be in range [0, 0x%X])"),
                aIOBase, 0, 0xFFFF);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    HRESULT rc = S_OK;
    bool emitChangeEvent = false;

    if (mData->mIOBase != aIOBase)
    {
        mData.backup();
        mData->mIOBase = aIOBase;
        emitChangeEvent = true;
    }

    if (emitChangeEvent)
    {
        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onSerialPortChange (this);
    }

    return rc;
}

STDMETHODIMP SerialPort::COMGETTER(Path) (BSTR *aPath)
{
    if (!aPath)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    mData->mPath.cloneTo (aPath);

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Path) (INPTR BSTR aPath)
{
    if (!aPath)
        return E_POINTER;

    if (!*aPath)
        return setError (E_INVALIDARG,
            tr ("Serial port path cannot be empty"));


    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mPath != aPath)
    {
        mData.backup();
        mData->mPath = aPath;

        /* leave the lock before informing callbacks */
        alock.unlock();

        return mParent->onSerialPortChange (this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(Server) (BOOL *aServer)
{
    if (!aServer)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReaderLock alock (this);

    *aServer = mData->mServer;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Server) (BOOL aServer)
{
    LogFlowThisFunc (("aServer=%RTbool\n", aServer));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoLock alock (this);

    if (mData->mServer != aServer)
    {
        mData.backup();
        mData->mServer = aServer;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onSerialPortChange (this);
    }

    return S_OK;
}
