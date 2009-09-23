/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "MediumAttachmentImpl.h"
#include "MachineImpl.h"

#include "Logging.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(MediumAttachment)

HRESULT MediumAttachment::FinalConstruct()
{
    return S_OK;
}

void MediumAttachment::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the medium attachment object.
 *
 * @param aMachine    Machine object.
 * @param aMedium     Medium object.
 * @param aController Controller the hard disk is attached to.
 * @param aPort       Port number.
 * @param aDevice     Device number on the port.
 * @param aImplicit   Wether the attachment contains an implicitly created diff.
 */
HRESULT MediumAttachment::init(Machine *aMachine,
                               Medium *aMedium,
                               CBSTR aController,
                               LONG aPort,
                               LONG aDevice,
                               DeviceType_T aType,
                               bool aImplicit /*= false*/)
{
    if (aType == DeviceType_HardDisk)
        AssertReturn(aMedium, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mMachine) = aMachine;
    m.allocate();
    m->medium = aMedium;
    unconst(m->controller) = aController;
    unconst(m->port)   = aPort;
    unconst(m->device) = aDevice;
    unconst(m->type)   = aType;
    unconst(m->passthrough) = false;

    m->implicit = aImplicit;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void MediumAttachment::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

/**
 *  @note Locks this object for writing.
 */
bool MediumAttachment::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoWriteLock alock(this);

    bool changed = false;

    if (m.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = m.hasActualChanges();
        m.rollback();
    }

    return changed;
}

/**
 *  @note Locks this object for writing.
 */
void MediumAttachment::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this);

    if (m.isBackedUp())
        m.commit();
}


// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumAttachment::COMGETTER(Medium)(IMedium **aHardDisk)
{
    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->medium.queryInterfaceTo(aHardDisk);

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Controller)(BSTR *aController)
{
    CheckComArgOutPointerValid(aController);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->controller is constant during life time, no need to lock */
    m->controller.cloneTo(aController);

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Port)(LONG *aPort)
{
    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->port is constant during life time, no need to lock */
    *aPort = m->port;

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Device)(LONG *aDevice)
{
    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->device is constant during life time, no need to lock */
    *aDevice = m->device;

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Type)(DeviceType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->type is constant during life time, no need to lock */
    *aType = m->type;

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMSETTER(Passthrough)(BOOL aPassthrough)
{
    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* the machine need to be mutable */
    Machine::AutoMutableStateDependency adep(mMachine);
    CheckComRCReturnRC(adep.rc());

    AutoWriteLock lock(this);

    if (m->passthrough != !!aPassthrough)
    {
        m.backup();
        m->passthrough = !!aPassthrough;
    }

    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Passthrough)(BOOL *aPassthrough)
{
    CheckComArgOutPointerValid(aPassthrough);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock lock(this);

    *aPassthrough = m->passthrough;
    return S_OK;
}

