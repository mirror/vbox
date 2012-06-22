/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "BandwidthGroupImpl.h"
#include "MachineImpl.h"
#include "Global.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/cpp/utils.h>

////////////////////////////////////////////////////////////////////////////////
//
// private member data definition
//
////////////////////////////////////////////////////////////////////////////////

struct BackupableBandwidthGroupData
{
    BackupableBandwidthGroupData()
        : enmType(BandwidthGroupType_Null),
          aMaxBytesPerSec(0),
          cReferences(0)
    { }

    Utf8Str                 strName;
    BandwidthGroupType_T    enmType;
    LONG64                  aMaxBytesPerSec;
    ULONG                   cReferences;
};

struct BandwidthGroup::Data
{
    Data(BandwidthControl * const aBandwidthControl)
        : pParent(aBandwidthControl),
          pPeer(NULL)
    { }

    BandwidthControl * const    pParent;
    ComObjPtr<BandwidthGroup>   pPeer;

    // use the XML settings structure in the members for simplicity
    Backupable<BackupableBandwidthGroupData> bd;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT BandwidthGroup::FinalConstruct()
{
    return BaseFinalConstruct();
}

void BandwidthGroup::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the bandwidth group object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aName         Name of the storage controller.
 * @param aInstance     Instance number of the storage controller.
 */
HRESULT BandwidthGroup::init(BandwidthControl *aParent,
                             const Utf8Str &aName,
                             BandwidthGroupType_T aType,
                             LONG64 aMaxBytesPerSec)
{
    LogFlowThisFunc(("aParent=%p aName=\"%s\"\n",
                     aParent, aName.c_str()));

    ComAssertRet(aParent && !aName.isEmpty(), E_INVALIDARG);
    if (   (aType <= BandwidthGroupType_Null)
        || (aType >  BandwidthGroupType_Network))
        return setError(E_INVALIDARG,
                        tr("Invalid bandwidth group type type"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* m->pPeer is left null */

    m->bd.allocate();

    m->bd->strName = aName;
    m->bd->enmType = aType;
    m->bd->cReferences = 0;
    m->bd->aMaxBytesPerSec = aMaxBytesPerSec;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @param  aReshare
 *      When false, the original object will remain a data owner.
 *      Otherwise, data ownership will be transferred from the original
 *      object to this one.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for writing if @a aReshare is @c true, or for
 *  reading if @a aReshare is false.
 */
HRESULT BandwidthGroup::init(BandwidthControl *aParent,
                             BandwidthGroup *aThat,
                             bool aReshare /* = false */)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p, aReshare=%RTbool\n",
                      aParent, aThat, aReshare));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* sanity */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    if (aReshare)
    {
        AutoWriteLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

        unconst(aThat->m->pPeer) = this;
        m->bd.attach (aThat->m->bd);
    }
    else
    {
        unconst(m->pPeer) = aThat;

        AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
        m->bd.share (aThat->m->bd);
    }

    /* Confirm successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the storage controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT BandwidthGroup::initCopy(BandwidthControl *aParent, BandwidthGroup *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);
    /* m->pPeer is left null */

    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatlock(aThat COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(aThat->m->bd);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void BandwidthGroup::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

    delete m;
    m = NULL;
}

STDMETHODIMP BandwidthGroup::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* mName is constant during life time, no need to lock */
    m->bd.data()->strName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP BandwidthGroup::COMGETTER(Type)(BandwidthGroupType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* type is constant during life time, no need to lock */
    *aType = m->bd->enmType;

    return S_OK;
}

STDMETHODIMP BandwidthGroup::COMGETTER(Reference)(ULONG *aReferences)
{
    CheckComArgOutPointerValid(aReferences);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aReferences = m->bd->cReferences;

    return S_OK;
}

STDMETHODIMP BandwidthGroup::COMGETTER(MaxBytesPerSec)(LONG64 *aMaxBytesPerSec)
{
    CheckComArgOutPointerValid(aMaxBytesPerSec);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMaxBytesPerSec = m->bd->aMaxBytesPerSec;

    return S_OK;
}

STDMETHODIMP BandwidthGroup::COMSETTER(MaxBytesPerSec)(LONG64 aMaxBytesPerSec)
{
    if (aMaxBytesPerSec < 0)
        return setError(E_INVALIDARG,
                        tr("Bandwidth group limit cannot be negative"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.backup();
    m->bd->aMaxBytesPerSec = aMaxBytesPerSec;

    /* inform direct session if any. */
    ComObjPtr<Machine> pMachine = m->pParent->getMachine();
    alock.release();
    pMachine->onBandwidthGroupChange(this);

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/** @note Locks objects for writing! */
void BandwidthGroup::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void BandwidthGroup::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (m->pPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (m->pPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            // attach new data to the peer and reshare it
            m->pPeer->m->bd.attach (m->bd);
        }
    }
}


/**
 *  Cancels sharing (if any) by making an independent copy of data.
 *  This operation also resets this object's peer to NULL.
 *
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void BandwidthGroup::unshare()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (m->pPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* peer is not modified, lock it for reading (m->pPeer is "master" so locked
     * first) */
    AutoReadLock rl(m->pPeer COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isShared())
    {
        if (!m->bd.isBackedUp())
            m->bd.backup();

        m->bd.commit();
    }

    unconst(m->pPeer) = NULL;
}

ComObjPtr<BandwidthGroup> BandwidthGroup::getPeer()
{
    return m->pPeer;
}

const Utf8Str& BandwidthGroup::getName() const
{
    return m->bd->strName;
}

BandwidthGroupType_T BandwidthGroup::getType() const
{
    return m->bd->enmType;
}

LONG64 BandwidthGroup::getMaxBytesPerSec() const
{
    return m->bd->aMaxBytesPerSec;
}

ULONG BandwidthGroup::getReferences() const
{
    return m->bd->cReferences;
}

void BandwidthGroup::reference()
{
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);
    m->bd.backup();
    m->bd->cReferences++;
}

void BandwidthGroup::release()
{
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);
    m->bd.backup();
    m->bd->cReferences--;
}

