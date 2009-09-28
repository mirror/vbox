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
    LogFlowThisFunc(("\n"));
    return S_OK;
}

void MediumAttachment::FinalRelease()
{
    LogFlowThisFuncEnter();
    uninit();
    LogFlowThisFuncLeave();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the medium attachment object.
 *
 * @param aParent     Machine object.
 * @param aMedium     Medium object.
 * @param aController Controller the hard disk is attached to.
 * @param aPort       Port number.
 * @param aDevice     Device number on the port.
 * @param aImplicit   Wether the attachment contains an implicitly created diff.
 */
HRESULT MediumAttachment::init(Machine *aParent,
                               Medium *aMedium,
                               CBSTR aController,
                               LONG aPort,
                               LONG aDevice,
                               DeviceType_T aType,
                               bool aImplicit /*= false*/)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent=%p aMedium=%p aController=%ls aPort=%d aDevice=%d aType=%d aImplicit=%d\n", aParent, aMedium, aController, aPort, aDevice, aType, aImplicit));

    if (aType == DeviceType_HardDisk)
        AssertReturn(aMedium, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

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

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void MediumAttachment::uninit()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m.free();

    unconst(mParent).setNull();

    LogFlowThisFuncLeave();
}

/**
 *  @note Locks this object for writing.
 */
bool MediumAttachment::rollback()
{
    LogFlowThisFuncEnter();

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

    LogFlowThisFuncLeave();
    return changed;
}

/**
 *  @note Locks this object for writing.
 */
void MediumAttachment::commit()
{
    LogFlowThisFuncEnter();

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this);

    if (m.isBackedUp())
        m.commit();

    LogFlowThisFuncLeave();
}


// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumAttachment::COMGETTER(Medium)(IMedium **aHardDisk)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this);

    m->medium.queryInterfaceTo(aHardDisk);

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Controller)(BSTR *aController)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aController);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->controller is constant during life time, no need to lock */
    m->controller.cloneTo(aController);

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Port)(LONG *aPort)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->port is constant during life time, no need to lock */
    *aPort = m->port;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Device)(LONG *aDevice)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->device is constant during life time, no need to lock */
    *aDevice = m->device;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Type)(DeviceType_T *aType)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* m->type is constant during life time, no need to lock */
    *aType = m->type;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMSETTER(Passthrough)(BOOL aPassthrough)
{
    LogFlowThisFuncEnter();

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    /* the machine need to be mutable */
    Machine::AutoMutableStateDependency adep(mParent);
    CheckComRCReturnRC(adep.rc());

    AutoWriteLock lock(this);

    if (m->passthrough != !!aPassthrough)
    {
        m.backup();
        m->passthrough = !!aPassthrough;
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Passthrough)(BOOL *aPassthrough)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aPassthrough);

    AutoCaller autoCaller(this);
    CheckComRCReturnRC(autoCaller.rc());

    AutoReadLock lock(this);

    *aPassthrough = m->passthrough;

    LogFlowThisFuncLeave();
    return S_OK;
}

