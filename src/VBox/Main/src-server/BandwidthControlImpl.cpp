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

#include "BandwidthControlImpl.h"
#include "BandwidthGroupImpl.h"
#include "MachineImpl.h"
#include "Global.h"

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/cpp/utils.h>

// defines
/////////////////////////////////////////////////////////////////////////////

typedef std::list< ComObjPtr<BandwidthGroup> > BandwidthGroupList;

struct BandwidthControl::Data
{
    Data(Machine *pMachine)
        : pParent(pMachine)
    { }

    ~Data()
    {};

    Machine * const                 pParent;

    // peer machine's bandwidth control
    const ComObjPtr<BandwidthControl>  pPeer;

    // the following fields need special backup/rollback/commit handling,
    // so they cannot be a part of BackupableData
    Backupable<BandwidthGroupList>    llBandwidthGroups;
};

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT BandwidthControl::FinalConstruct()
{
    return BaseFinalConstruct();
}

void BandwidthControl::FinalRelease()
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
HRESULT BandwidthControl::init(Machine *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* m->pPeer is left null */

    m->llBandwidthGroups.allocate();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the object given another object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for writing if @a aReshare is @c true, or for
 *  reading if @a aReshare is false.
 */
HRESULT BandwidthControl::init(Machine *aParent,
                               BandwidthControl *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* sanity */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnRC(thatCaller.rc());

    unconst(m->pPeer) = aThat;
    AutoWriteLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);

    /* create copies of all groups */
    m->llBandwidthGroups.allocate();
    BandwidthGroupList::const_iterator it = aThat->m->llBandwidthGroups->begin();
    while (it != aThat->m->llBandwidthGroups->end())
    {
        ComObjPtr<BandwidthGroup> group;
        group.createObject();
        group->init(this, *it);
        m->llBandwidthGroups->push_back(group);
        ++ it;
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
HRESULT BandwidthControl::initCopy(Machine *aParent, BandwidthControl *aThat)
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

    /* create copies of all groups */
    m->llBandwidthGroups.allocate();
    BandwidthGroupList::const_iterator it = aThat->m->llBandwidthGroups->begin();
    while (it != aThat->m->llBandwidthGroups->end())
    {
        ComObjPtr<BandwidthGroup> group;
        group.createObject();
        group->init(this, *it);
        m->llBandwidthGroups->push_back(group);
        ++ it;
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void BandwidthControl::copyFrom (BandwidthControl *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* even more sanity */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid (adep.rc());
    /* Machine::copyFrom() may not be called when the VM is running */
    AssertReturnVoid (!Global::IsOnline (adep.machineState()));

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* create private copies of all filters */
    m->llBandwidthGroups.backup();
    m->llBandwidthGroups->clear();
    for (BandwidthGroupList::const_iterator it = aThat->m->llBandwidthGroups->begin();
        it != aThat->m->llBandwidthGroups->end();
        ++ it)
    {
        ComObjPtr<BandwidthGroup> group;
        group.createObject();
        group->initCopy (this, *it);
        m->llBandwidthGroups->push_back (group);
    }
}

/** @note Locks objects for writing! */
void BandwidthControl::rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* we need the machine state */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (!m->llBandwidthGroups.isNull())
    {
        if (m->llBandwidthGroups.isBackedUp())
        {
            /* unitialize all new groups (absent in the backed up list). */
            BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
            BandwidthGroupList *backedList = m->llBandwidthGroups.backedUpData();
            while (it != m->llBandwidthGroups->end())
            {
                if (   std::find(backedList->begin(), backedList->end(), *it)
                    == backedList->end()
                   )
                {
                    (*it)->uninit();
                }
                ++it;
            }

            /* restore the list */
            m->llBandwidthGroups.rollback();
        }

        /* rollback any changes to groups after restoring the list */
        BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
        while (it != m->llBandwidthGroups->end())
        {
            (*it)->rollback();
            ++it;
        }
    }
}

void BandwidthControl::commit()
{
    bool commitBandwidthGroups = false;

    if (m->llBandwidthGroups.isBackedUp())
    {
        m->llBandwidthGroups.commit();

        if (m->pPeer)
        {
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);

            /* Commit all changes to new controllers (this will reshare data with
             * peers for those who have peers) */
            BandwidthGroupList *newList = new BandwidthGroupList();
            BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
            while (it != m->llBandwidthGroups->end())
            {
                (*it)->commit();

                /* look if this group has a peer group */
                ComObjPtr<BandwidthGroup> peer = (*it)->getPeer();
                if (!peer)
                {
                    /* no peer means the device is a newly created one;
                     * create a peer owning data this device share it with */
                    peer.createObject();
                    peer->init(m->pPeer, *it, true /* aReshare */);
                }
                else
                {
                    /* remove peer from the old list */
                    m->pPeer->m->llBandwidthGroups->remove(peer);
                }
                /* and add it to the new list */
                newList->push_back(peer);

                ++it;
            }

            /* uninit old peer's controllers that are left */
            it = m->pPeer->m->llBandwidthGroups->begin();
            while (it != m->pPeer->m->llBandwidthGroups->end())
            {
                (*it)->uninit();
                ++it;
            }

            /* attach new list of controllers to our peer */
            m->pPeer->m->llBandwidthGroups.attach(newList);
        }
        else
        {
            /* we have no peer (our parent is the newly created machine);
             * just commit changes to devices */
            commitBandwidthGroups = true;
        }
    }
    else
    {
        /* the list of groups itself is not changed,
         * just commit changes to controllers themselves */
        commitBandwidthGroups = true;
    }

    if (commitBandwidthGroups)
    {
        BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
        while (it != m->llBandwidthGroups->end())
        {
            (*it)->commit();
            ++it;
        }
    }
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void BandwidthControl::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    // uninit all groups on the list (it's a standard std::list not an ObjectsList
    // so we must uninit() manually)
    for (BandwidthGroupList::iterator it = m->llBandwidthGroups->begin();
         it != m->llBandwidthGroups->end();
         ++it)
        (*it)->uninit();

    m->llBandwidthGroups.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

    delete m;
    m = NULL;
}

/**
 * Returns a storage controller object with the given name.
 *
 *  @param aName                 storage controller name to find
 *  @param aStorageController    where to return the found storage controller
 *  @param aSetError             true to set extended error info on failure
 */
HRESULT BandwidthControl::getBandwidthGroupByName(const Utf8Str &aName,
                                                  ComObjPtr<BandwidthGroup> &aBandwidthGroup,
                                                  bool aSetError /* = false */)
{
    AssertReturn(!aName.isEmpty(), E_INVALIDARG);

    for (BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
         it != m->llBandwidthGroups->end();
         ++it)
    {
        if ((*it)->getName() == aName)
        {
            aBandwidthGroup = (*it);
            return S_OK;
        }
    }

    if (aSetError)
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Could not find a bandwidth group named '%s'"),
                        aName.c_str());
    return VBOX_E_OBJECT_NOT_FOUND;
}

STDMETHODIMP BandwidthControl::CreateBandwidthGroup(IN_BSTR aName, BandwidthGroupType_T aType, LONG64 aMaxBytesPerSec)
{
    if (aMaxBytesPerSec < 0)
        return setError(E_INVALIDARG,
                        tr("Bandwidth group limit cannot be negative"));

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* try to find one with the name first. */
    ComObjPtr<BandwidthGroup> group;

    HRESULT rc = getBandwidthGroupByName(aName, group, false /* aSetError */);
    if (SUCCEEDED(rc))
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Bandwidth group named '%ls' already exists"),
                        aName);

    group.createObject();

    rc = group->init(this, aName, aType, aMaxBytesPerSec);
    if (FAILED(rc)) return rc;

    m->pParent->setModified(Machine::IsModified_BandwidthControl);
    m->llBandwidthGroups.backup();
    m->llBandwidthGroups->push_back(group);

    return S_OK;
}

STDMETHODIMP BandwidthControl::DeleteBandwidthGroup(IN_BSTR aName)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<BandwidthGroup> group;
    HRESULT rc = getBandwidthGroupByName(aName, group, true /* aSetError */);
    if (FAILED(rc)) return rc;

    if (group->getReferences() != 0)
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("The bandwidth group '%ls' is still in use"), aName);

    /* We can remove it now. */
    m->pParent->setModified(Machine::IsModified_BandwidthControl);
    m->llBandwidthGroups.backup();

    group->unshare();

    m->llBandwidthGroups->remove(group);

    /* inform the direct session if any */
    alock.release();
    //onStorageControllerChange(); @todo

    return S_OK;
}

STDMETHODIMP BandwidthControl::COMGETTER(NumGroups)(ULONG *aGroups)
{
    CheckComArgNotNull(aGroups);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aGroups = m->llBandwidthGroups->size();

    return S_OK;
}

STDMETHODIMP BandwidthControl::GetBandwidthGroup(IN_BSTR aName, IBandwidthGroup **aBandwidthGroup)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<BandwidthGroup> group;

    HRESULT rc = getBandwidthGroupByName(aName, group, true /* aSetError */);
    if (SUCCEEDED(rc))
        group.queryInterfaceTo(aBandwidthGroup);

    return rc;
}

STDMETHODIMP BandwidthControl::GetAllBandwidthGroups(ComSafeArrayOut(IBandwidthGroup *, aBandwidthGroups))
{
    CheckComArgOutSafeArrayPointerValid(aBandwidthGroups);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IBandwidthGroup> collection (*m->llBandwidthGroups.data());
    collection.detachTo(ComSafeArrayOutArg(aBandwidthGroups));

    return S_OK;
}

HRESULT BandwidthControl::loadSettings(const settings::IOSettings &data)
{
    HRESULT rc = S_OK;

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    for (settings::BandwidthGroupList::const_iterator it = data.llBandwidthGroups.begin();
        it != data.llBandwidthGroups.end();
        ++it)
    {
        const settings::BandwidthGroup &gr = *it;
        rc = CreateBandwidthGroup(Bstr(gr.strName).raw(), gr.enmType, gr.cMaxBytesPerSec);
        if (FAILED(rc)) break;
    }

    return rc;
}

HRESULT BandwidthControl::saveSettings(settings::IOSettings &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.llBandwidthGroups.clear();

    for (BandwidthGroupList::const_iterator it = m->llBandwidthGroups->begin();
         it != m->llBandwidthGroups->end();
         ++it)
    {
        AutoWriteLock groupLock(*it COMMA_LOCKVAL_SRC_POS);
        settings::BandwidthGroup group;

        group.strName      = (*it)->getName();
        group.enmType      = (*it)->getType();
        group.cMaxBytesPerSec = (*it)->getMaxBytesPerSec();

        data.llBandwidthGroups.push_back(group);
    }

    return S_OK;
}

Machine * BandwidthControl::getMachine() const
{
    return m->pParent;
}

