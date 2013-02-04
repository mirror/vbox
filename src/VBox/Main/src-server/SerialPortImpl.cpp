/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SerialPortImpl.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/settings.h>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
//
// SerialPort private data definition
//
////////////////////////////////////////////////////////////////////////////////

struct SerialPort::Data
{
    Data()
        : fModified(false),
          pMachine(NULL)
    { }

    bool                                fModified;

    Machine * const                     pMachine;
    const ComObjPtr<SerialPort>         pPeer;

    Backupable<settings::SerialPort>    bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (SerialPort)

HRESULT SerialPort::FinalConstruct()
{
    return BaseFinalConstruct();
}

void SerialPort::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Serial Port object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT SerialPort::init(Machine *aParent, ULONG aSlot)
{
    LogFlowThisFunc(("aParent=%p, aSlot=%d\n", aParent, aSlot));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;
    /* m->pPeer is left null */

    m->bd.allocate();

    /* initialize data */
    m->bd->ulSlot = aSlot;

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
HRESULT SerialPort::init(Machine *aParent, SerialPort *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;
    unconst(m->pPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    m->bd.share (aThat->m->bd);

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
HRESULT SerialPort::initCopy(Machine *aParent, SerialPort *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;
    /* pPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy (aThat->m->bd);

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
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pMachine) = NULL;

    delete m;
    m = NULL;
}

// ISerialPort properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP SerialPort::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->bd->fEnabled;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc(("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fEnabled != !!aEnabled)
    {
        m->bd.backup();
        m->bd->fEnabled = !!aEnabled;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(HostMode) (PortMode_T *aHostMode)
{
    CheckComArgOutPointerValid(aHostMode);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aHostMode = m->bd->portMode;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(HostMode) (PortMode_T aHostMode)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->portMode != aHostMode)
    {
        switch (aHostMode)
        {
            case PortMode_RawFile:
                if (m->bd->strPath.isEmpty())
                    return setError(E_INVALIDARG,
                                    tr("Cannot set the raw file mode of the serial port %d "
                                       "because the file path is empty or null"),
                                    m->bd->ulSlot);
                break;
            case PortMode_HostPipe:
                if (m->bd->strPath.isEmpty())
                    return setError(E_INVALIDARG,
                                    tr("Cannot set the host pipe mode of the serial port %d "
                                       "because the pipe path is empty or null"),
                                    m->bd->ulSlot);
                break;
            case PortMode_HostDevice:
                if (m->bd->strPath.isEmpty())
                    return setError(E_INVALIDARG,
                                    tr("Cannot set the host device mode of the serial port %d "
                                       "because the device path is empty or null"),
                                    m->bd->ulSlot);
                break;
            case PortMode_Disconnected:
                break;
        }

        m->bd.backup();
        m->bd->portMode = aHostMode;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(Slot) (ULONG *aSlot)
{
    CheckComArgOutPointerValid(aSlot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSlot = m->bd->ulSlot;

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(IRQ) (ULONG *aIRQ)
{
    CheckComArgOutPointerValid(aIRQ);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIRQ = m->bd->ulIRQ;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(IRQ)(ULONG aIRQ)
{
    /* check IRQ limits
     * (when changing this, make sure it corresponds to XML schema */
    if (aIRQ > 255)
        return setError(E_INVALIDARG,
                        tr("Invalid IRQ number of the serial port %d: %lu (must be in range [0, %lu])"),
                        m->bd->ulSlot, aIRQ, 255);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->ulIRQ != aIRQ)
    {
        m->bd.backup();
        m->bd->ulIRQ = aIRQ;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(IOBase) (ULONG *aIOBase)
{
    CheckComArgOutPointerValid(aIOBase);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aIOBase = m->bd->ulIOBase;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(IOBase)(ULONG aIOBase)
{
    /* check IOBase limits
     * (when changing this, make sure it corresponds to XML schema */
    if (aIOBase > 0xFFFF)
        return setError(E_INVALIDARG,
                        tr("Invalid I/O port base address of the serial port %d: %lu (must be in range [0, 0x%X])"),
                        m->bd->ulSlot, aIOBase, 0, 0xFFFF);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    if (m->bd->ulIOBase != aIOBase)
    {
        m->bd.backup();
        m->bd->ulIOBase = aIOBase;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return rc;
}

STDMETHODIMP SerialPort::COMGETTER(Path) (BSTR *aPath)
{
    CheckComArgOutPointerValid(aPath);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd->strPath.cloneTo(aPath);

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Path) (IN_BSTR aPath)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* we treat empty as null when e.g. saving to XML, do the same here */
    if (aPath && *aPath == '\0')
        aPath = NULL;

    Utf8Str str(aPath);
    if (str != m->bd->strPath)
    {
        HRESULT rc = checkSetPath(str);
        if (FAILED(rc)) return rc;

        m->bd.backup();
        m->bd->strPath = str;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return S_OK;
}

STDMETHODIMP SerialPort::COMGETTER(Server) (BOOL *aServer)
{
    CheckComArgOutPointerValid(aServer);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aServer = m->bd->fServer;

    return S_OK;
}

STDMETHODIMP SerialPort::COMSETTER(Server) (BOOL aServer)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pMachine);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->fServer != !!aServer)
    {
        m->bd.backup();
        m->bd->fServer = !!aServer;

        m->fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(m->pMachine COMMA_LOCKVAL_SRC_POS);
        m->pMachine->setModified(Machine::IsModified_SerialPorts);
        mlock.release();

        m->pMachine->onSerialPortChange(this);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given port node.
 *  May be called once right after this object creation.
 *
 *  @param aPortNode <Port> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT SerialPort::loadSettings(const settings::SerialPort &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // simply copy
    *m->bd.data() = data;

    return S_OK;
}

/**
 *  Saves the port settings to the given port node.
 *
 *  Note that the given Port node is completely empty on input.
 *
 *  @param aPortNode <Port> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT SerialPort::saveSettings(settings::SerialPort &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    // simply copy
    data = *m->bd.data();

    return S_OK;
}

/**
 * Returns true if any setter method has modified settings of this instance.
 * @return
 */
bool SerialPort::isModified()
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    return m->fModified;
}

/**
 *  @note Locks this object for writing.
 */
void SerialPort::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void SerialPort::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (pPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            /* attach new data to the peer and reshare it */
            m->pPeer->m->bd.attach(m->bd);
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
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy (aThat->m->bd);
}

void SerialPort::applyDefaults (GuestOSType *aOsType)
{
    AssertReturnVoid (aOsType != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Set some more defaults.
     * Note that the default value for COM1 (slot 0) is set in the constructor
     * of bd. So slot 0 is correct already. */
    switch (m->bd->ulSlot)
    {
        case 1:
        {
            m->bd->ulIOBase = 0x2F8;
            m->bd->ulIRQ = 3;
            break;
        }
        case 2:
        {
            m->bd->ulIOBase = 0x3e8;
            m->bd->ulIRQ = 4;
            break;
        }
        case 3:
        {
            m->bd->ulIOBase = 0x2e8;
            m->bd->ulIRQ = 3;
            break;
        }
        default: break;
    }

    uint32_t numSerialEnabled = aOsType->numSerialEnabled();

    /* Enable port if requested */
    if (m->bd->ulSlot < numSerialEnabled)
    {
        m->bd->fEnabled = true;
    }
}

/**
 *  Validates COMSETTER(Path) arguments.
 */
HRESULT SerialPort::checkSetPath(const Utf8Str &str)
{
    AssertReturn(isWriteLockOnCurrentThread(), E_FAIL);

    if (    (    m->bd->portMode == PortMode_HostDevice
              || m->bd->portMode == PortMode_HostPipe
              || m->bd->portMode == PortMode_RawFile
            ) && str.isEmpty()
       )
        return setError(E_INVALIDARG,
                        tr("Path of the serial port %d may not be empty or null in "
                           "host pipe or host device mode"),
                        m->bd->ulSlot);

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
