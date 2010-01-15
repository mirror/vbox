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
#include "MediumImpl.h"
#include "Global.h"

#include "AutoCaller.h"
#include "Logging.h"

////////////////////////////////////////////////////////////////////////////////
//
// private member data definition
//
////////////////////////////////////////////////////////////////////////////////

struct BackupableMediumAttachmentData
{
    BackupableMediumAttachmentData()
        : lPort(0),
          lDevice(0),
          type(DeviceType_Null),
          fPassthrough(false),
          fImplicit(false)
    { }

    bool operator==(const BackupableMediumAttachmentData &that) const
    {
        return    this == &that
               || (fPassthrough == that.fPassthrough);
    }

    ComObjPtr<Medium>   pMedium;
    /* Since MediumAttachment is not a first class citizen when it
     * comes to managing settings, having a reference to the storage
     * controller will not work - when settings are changed it will point
     * to the old, uninitialized instance. Changing this requires
     * substantial changes to MediumImpl.cpp. */
    const Bstr          bstrControllerName;
    const LONG          lPort;
    const LONG          lDevice;
    const DeviceType_T  type;
    bool                fPassthrough : 1;
    bool                fImplicit : 1;
};

struct MediumAttachment::Data
{
    Data()
    { }

    /** Reference to Machine object, for checking mutable state. */
    const ComObjPtr<Machine, ComWeakRef> pMachine;
    /* later: const ComObjPtr<MediumAttachment> mPeer; */

    Backupable<BackupableMediumAttachmentData> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

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
 * @param aPassthrough Wether accesses are directly passed to the host drive.
 */
HRESULT MediumAttachment::init(Machine *aParent,
                               Medium *aMedium,
                               const Bstr &aControllerName,
                               LONG aPort,
                               LONG aDevice,
                               DeviceType_T aType,
                               bool aPassthrough)
{
    LogFlowThisFuncEnter();
    LogFlowThisFunc(("aParent=%p aMedium=%p aControllerName=%ls aPort=%d aDevice=%d aType=%d aPassthrough=%d\n", aParent, aMedium, aControllerName.raw(), aPort, aDevice, aType, aPassthrough));

    if (aType == DeviceType_HardDisk)
        AssertReturn(aMedium, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data();

    unconst(m->pMachine) = aParent;

    m->bd.allocate();
    m->bd->pMedium = aMedium;
    unconst(m->bd->bstrControllerName) = aControllerName;
    unconst(m->bd->lPort)   = aPort;
    unconst(m->bd->lDevice) = aDevice;
    unconst(m->bd->type)    = aType;

    m->bd->fPassthrough = aPassthrough;
    /* Newly created attachments never have an implicitly created medium
     * associated with them. Implicit diff image creation happens later. */
    m->bd->fImplicit = false;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    /* Construct a short log name for this attachment. */
    Utf8Str ctlName(aControllerName);
    const char *psz = strpbrk(ctlName.c_str(), " \t:-");
    mLogName = Utf8StrFmt("MA%p[%.*s:%u:%u:%s%s]",
                          this,
                          psz ? psz - ctlName.c_str() : 4, ctlName.c_str(),
                          aPort, aDevice, Global::stringifyDeviceType(aType),
                          m->bd->fImplicit ? ":I" : "");

    LogFlowThisFunc(("LEAVE - %s\n", getLogName()));
    return S_OK;
}

/**
 * Uninitializes the instance.
 * Called from FinalRelease().
 */
void MediumAttachment::uninit()
{
    LogFlowThisFunc(("ENTER - %s\n", getLogName()));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pMachine).setNull();

    delete m;
    m = NULL;

    LogFlowThisFuncLeave();
}

// IHardDiskAttachment properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP MediumAttachment::COMGETTER(Medium)(IMedium **aHardDisk)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aHardDisk);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd->pMedium.queryInterfaceTo(aHardDisk);

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Controller)(BSTR *aController)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aController);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* m->controller is constant during life time, no need to lock */
    m->bd->bstrControllerName.cloneTo(aController);

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Port)(LONG *aPort)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aPort);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* m->bd->port is constant during life time, no need to lock */
    *aPort = m->bd->lPort;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Device)(LONG *aDevice)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aDevice);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* m->bd->device is constant during life time, no need to lock */
    *aDevice = m->bd->lDevice;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Type)(DeviceType_T *aType)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* m->bd->type is constant during life time, no need to lock */
    *aType = m->bd->type;

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP MediumAttachment::COMGETTER(Passthrough)(BOOL *aPassthrough)
{
    LogFlowThisFuncEnter();

    CheckComArgOutPointerValid(aPassthrough);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock lock(this COMMA_LOCKVAL_SRC_POS);

    *aPassthrough = m->bd->fPassthrough;

    LogFlowThisFuncLeave();
    return S_OK;
}

/**
 *  @note Locks this object for writing.
 */
bool MediumAttachment::rollback()
{
    LogFlowThisFunc(("ENTER - %s\n", getLogName()));

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    bool changed = false;

    if (m->bd.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = m->bd.hasActualChanges();
        m->bd.rollback();
    }

    LogFlowThisFunc(("LEAVE - %s - returning %RTbool\n", getLogName(), changed));
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

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
        m->bd.commit();

    LogFlowThisFuncLeave();
}

bool MediumAttachment::isImplicit() const
{
    return m->bd->fImplicit;
}

void MediumAttachment::setImplicit(bool aImplicit)
{
    m->bd->fImplicit = aImplicit;
}

const ComObjPtr<Medium>& MediumAttachment::getMedium() const
{
    return m->bd->pMedium;
}

Bstr MediumAttachment::getControllerName() const
{
    return m->bd->bstrControllerName;
}

LONG MediumAttachment::getPort() const
{
    return m->bd->lPort;
}

LONG MediumAttachment::getDevice() const
{
    return m->bd->lDevice;
}

DeviceType_T MediumAttachment::getType() const
{
    return m->bd->type;
}

bool MediumAttachment::getPassthrough() const
{
    AutoReadLock lock(this COMMA_LOCKVAL_SRC_POS);
    return m->bd->fPassthrough;
}

bool MediumAttachment::matches(CBSTR aControllerName, LONG aPort, LONG aDevice)
{
    return (    aControllerName == m->bd->bstrControllerName
             && aPort == m->bd->lPort
             && aDevice == m->bd->lDevice);
}

/** Must be called from under this object's write lock. */
void MediumAttachment::updateMedium(const ComObjPtr<Medium> &aMedium, bool aImplicit)
{
    m->bd.backup();
    m->bd->pMedium = aMedium;
    m->bd->fImplicit = aImplicit;
}

/** Must be called from under this object's write lock. */
void MediumAttachment::updatePassthrough(bool aPassthrough)
{
    m->bd.backup();
    m->bd->fPassthrough = aPassthrough;
}

