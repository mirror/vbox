/* $Id$ */

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

#include "FloppyDriveImpl.h"

#include "MachineImpl.h"
#include "HostImpl.h"
#include "HostFloppyDriveImpl.h"
#include "VirtualBoxImpl.h"

#include "Global.h"

#include "Logging.h"

#include <iprt/string.h>
#include <iprt/cpputils.h>

#include <VBox/settings.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (FloppyDrive)

HRESULT FloppyDrive::FinalConstruct()
{
    return S_OK;
}

void FloppyDrive::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the Floppy drive object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT FloppyDrive::init (Machine *aParent)
{
    LogFlowThisFunc (("aParent=%p\n", aParent));

    ComAssertRet (aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    m.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the Floppy drive object given another Floppy drive object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT FloppyDrive::init (Machine *aParent, FloppyDrive *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    unconst (mPeer) = aThat;

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReadLock thatLock (aThat);
    m.share (aThat->m);

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
HRESULT FloppyDrive::initCopy (Machine *aParent, FloppyDrive *aThat)
{
    LogFlowThisFunc (("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet (aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mParent) = aParent;
    /* mPeer is left null */

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC (thatCaller.rc());

    AutoReadLock thatLock (aThat);
    m.attachCopy (aThat->m);

    /* at present, this must be a snapshot machine */
    Assert (!aParent->snapshotId().isEmpty());

    if (m->state == DriveState_ImageMounted)
    {
        /* associate the DVD image media with the snapshot */
        HRESULT rc = m->image->attachTo (aParent->id(),
                                         aParent->snapshotId());
        AssertComRC (rc);
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void FloppyDrive::uninit()
{
    LogFlowThisFunc (("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    if ((mParent->type() == Machine::IsMachine ||
         mParent->type() == Machine::IsSnapshotMachine) &&
        m->state == DriveState_ImageMounted)
    {
        /* Deassociate the DVD image (only when mParent is a real Machine or a
         * SnapshotMachine instance; SessionMachine instances
         * refer to real Machine hard disks). This is necessary for a clean
         * re-initialization of the VM after successfully re-checking the
         * accessibility state. */
        HRESULT rc = m->image->detachFrom (mParent->id(),
                                           mParent->snapshotId());
        AssertComRC (rc);
    }

    m.free();

    unconst (mPeer).setNull();
    unconst (mParent).setNull();
}

// IFloppyDrive properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aEnabled = m->enabled;

    return S_OK;
}

STDMETHODIMP FloppyDrive::COMSETTER(Enabled) (BOOL aEnabled)
{
    LogFlowThisFunc (("aEnabled=%RTbool\n", aEnabled));

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (m->enabled != aEnabled)
    {
        m.backup();
        m->enabled = aEnabled;

        /* leave the lock before informing callbacks */
        alock.unlock();

        mParent->onFloppyDriveChange();
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::COMGETTER(State) (DriveState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aState = m->state;

    return S_OK;
}

// IFloppyDrive methods
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP FloppyDrive::MountImage (IN_GUID aImageId)
{
    Guid imageId = aImageId;
    CheckComArgExpr(aImageId, !imageId.isEmpty());

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    HRESULT rc = E_FAIL;

    /* Our lifetime is bound to mParent's lifetime, so we don't add caller.
     * We also don't lock mParent since its mParent field is const. */

    ComObjPtr<FloppyImage> image;
    rc = mParent->virtualBox()->findFloppyImage(&imageId, NULL,
                                                true /* aSetError */, &image);

    if (SUCCEEDED (rc))
    {
        if (m->state != DriveState_ImageMounted ||
            !m->image.equalsTo (image))
        {
            rc = image->attachTo (mParent->id(), mParent->snapshotId());
            if (SUCCEEDED (rc))
            {
                /* umount() will backup data */
                rc = unmount();

                if (SUCCEEDED (rc))
                {
                    /* lock the image for reading if the VM is online. It will
                     * be unlocked either when unmounted from this drive or by
                     * SessionMachine::setMachineState() when the VM is
                     * terminated */
                    if (Global::IsOnline (adep.machineState()))
                        rc = image->LockRead (NULL);
                }

                if (SUCCEEDED (rc))
                {
                    m->image = image;
                    m->state = DriveState_ImageMounted;

                    /* leave the lock before informing callbacks */
                    alock.unlock();

                    mParent->onFloppyDriveChange();
                }
            }
        }
    }

    return rc;
}

STDMETHODIMP FloppyDrive::CaptureHostDrive (IHostFloppyDrive *aHostFloppyDrive)
{
    CheckComArgNotNull(aHostFloppyDrive);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (m->state != DriveState_HostDriveCaptured ||
        !m->hostDrive.equalsTo (aHostFloppyDrive))
    {
        /* umount() will backup data */
        HRESULT rc = unmount();
        if (SUCCEEDED (rc))
        {
            m->hostDrive = aHostFloppyDrive;
            m->state = DriveState_HostDriveCaptured;

            /* leave the lock before informing callbacks */
            alock.unlock();

            mParent->onFloppyDriveChange();
        }
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::Unmount()
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* the machine needs to be mutable */
    Machine::AutoMutableStateDependency adep (mParent);
    CheckComRCReturnRC (adep.rc());

    AutoWriteLock alock (this);

    if (m->state != DriveState_NotMounted)
    {
        /* umount() will backup data */
        HRESULT rc = unmount();
        if (SUCCEEDED (rc))
        {
            m->state = DriveState_NotMounted;

            /* leave the lock before informing callbacks */
            alock.unlock();

            mParent->onFloppyDriveChange();
        }
    }

    return S_OK;
}

STDMETHODIMP FloppyDrive::GetImage(IFloppyImage **aFloppyImage)
{
    CheckComArgOutPointerValid(aFloppyImage);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m->image.queryInterfaceTo (aFloppyImage);

    return S_OK;
}

STDMETHODIMP FloppyDrive::GetHostDrive (IHostFloppyDrive **aHostDrive)
{
    CheckComArgOutPointerValid(aHostDrive);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    m->hostDrive.queryInterfaceTo (aHostDrive);

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Loads settings from the given machine node. May be called once right after
 * this object creation.
 *
 * @param aMachineNode  <Machine> node.
 *
 * @note Locks this object for writing.
 */
HRESULT FloppyDrive::loadSettings (const settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    HRESULT rc = S_OK;

    /* Floppy drive (required, contains either Image or HostDrive or nothing) */
    Key floppyDriveNode = aMachineNode.key ("FloppyDrive");

    /* optional, defaults to true */
    m->enabled = floppyDriveNode.value <bool> ("enabled");

    Key typeNode;

    if (!(typeNode = floppyDriveNode.findKey ("Image")).isNull())
    {
        Guid uuid = typeNode.value <Guid> ("uuid");
        rc = MountImage (uuid);
        CheckComRCReturnRC (rc);
    }
    else if (!(typeNode = floppyDriveNode.findKey ("HostDrive")).isNull())
    {

        Bstr src = typeNode.stringValue ("src");

        /* find the corresponding object */
        ComObjPtr <Host> host = mParent->virtualBox()->host();

        com::SafeIfaceArray <IHostFloppyDrive> coll;
        rc = host->COMGETTER(FloppyDrives) (ComSafeArrayAsOutParam(coll));
        AssertComRC (rc);

        ComPtr <IHostFloppyDrive> drive;
        rc = host->FindHostFloppyDrive (src, drive.asOutParam());

        if (SUCCEEDED (rc))
        {
            rc = CaptureHostDrive (drive);
            CheckComRCReturnRC (rc);
        }
        else if (rc == E_INVALIDARG)
        {
            /* the host DVD drive is not currently available. we
             * assume it will be available later and create an
             * extra object now */
            ComObjPtr <HostFloppyDrive> hostDrive;
            hostDrive.createObject();
            rc = hostDrive->init (src);
            AssertComRC (rc);
            rc = CaptureHostDrive (hostDrive);
            CheckComRCReturnRC (rc);
        }
        else
            AssertComRC (rc);
    }

    return S_OK;
}

/**
 * Saves settings to the given machine node.
 *
 * @param aMachineNode  <Machine> node.
 *
 * @note Locks this object for reading.
 */
HRESULT FloppyDrive::saveSettings (settings::Key &aMachineNode)
{
    using namespace settings;

    AssertReturn (!aMachineNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Key node = aMachineNode.createKey ("FloppyDrive");

    node.setValue <bool> ("enabled", !!m->enabled);

    switch (m->state)
    {
        case DriveState_ImageMounted:
        {
            Assert (!m->image.isNull());

            Guid id;
            HRESULT rc = m->image->COMGETTER(Id) (id.asOutParam());
            AssertComRC (rc);
            Assert (!id.isEmpty());

            Key imageNode = node.createKey ("Image");
            imageNode.setValue <Guid> ("uuid", id);
            break;
        }
        case DriveState_HostDriveCaptured:
        {
            Assert (!m->hostDrive.isNull());

            Bstr name;
            HRESULT  rc = m->hostDrive->COMGETTER(Name) (name.asOutParam());
            AssertComRC (rc);
            Assert (!name.isEmpty());

            Key hostDriveNode = node.createKey ("HostDrive");
            hostDriveNode.setValue <Bstr> ("src", name);
            break;
        }
        case DriveState_NotMounted:
            /* do nothing, i.e.leave the drive node empty */
            break;
        default:
            ComAssertMsgFailedRet (("Invalid drive state: %d", m->state),
                                    E_FAIL);
    }

    return S_OK;
}

/**
 * @note Locks this object for writing.
 */
bool FloppyDrive::rollback()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    /* we need adep for the state check */
    Machine::AutoAnyStateDependency adep (mParent);
    AssertComRCReturn (adep.rc(), false);

    AutoWriteLock alock (this);

    bool changed = false;

    if (m.isBackedUp())
    {
        /* we need to check all data to see whether anything will be changed
         * after rollback */
        changed = m.hasActualChanges();

        if (changed)
        {
            Data *oldData = m.backedUpData();

            if (!m->image.isNull() &&
                !oldData->image.equalsTo (m->image))
            {
                /* detach the current image that will go away after rollback */
                m->image->detachFrom (mParent->id(), mParent->snapshotId());

                /* unlock the image for reading if the VM is online */
                if (Global::IsOnline (adep.machineState()))
                {
                    HRESULT rc = m->image->UnlockRead (NULL);
                    AssertComRC (rc);
                }
            }
        }

        m.rollback();
        if (!m->image.isNull())
        {
            /* Reattach from the old image. */
            HRESULT rc = m->image->attachTo(mParent->id(), mParent->snapshotId());
            AssertComRC (rc);
            if (Global::IsOnline (adep.machineState()))
            {
                /* Lock from the old image. */
                rc = m->image->LockRead (NULL);
                AssertComRC (rc);
            }
        }
    }

    return changed;
}

/**
 * @note Locks this object for writing, together with the peer object (also for
 *       writing) if there is one.
 */
void FloppyDrive::commit()
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock (mPeer, this);

    if (m.isBackedUp())
    {
        m.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->m.attach (m);
        }
    }
}

/**
 * @note Locks this object for writing, together with the peer object (locked
 *       for reading) if there is one.
 */
void FloppyDrive::copyFrom (FloppyDrive *aThat)
{
    /* sanity */
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoMultiLock2 alock (aThat->rlock(), this->wlock());

    /* this will back up current data */
    m.assignCopy (aThat->m);
}

/**
 * Helper to unmount a drive.
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT FloppyDrive::unmount()
{
    AssertReturn (isWriteLockOnCurrentThread(), E_FAIL);

    /* we need adep for the state check */
    Machine::AutoAnyStateDependency adep (mParent);
    AssertComRCReturn (adep.rc(), false);

    if (!m->image.isNull())
    {
        HRESULT rc = m->image->detachFrom (mParent->id(), mParent->snapshotId());
        AssertComRC (rc);
        if (Global::IsOnline (adep.machineState()))
        {
            rc = m->image->UnlockRead (NULL);
            AssertComRC (rc);
        }
    }

    m.backup();

    if (m->image)
        m->image.setNull();
    if (m->hostDrive)
        m->hostDrive.setNull();

    m->state = DriveState_NotMounted;

    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
