/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "HardDiskImpl.h"

#include "ProgressImpl.h"
#include "SystemPropertiesImpl.h"

#include "Logging.h"

#include <VBox/com/array.h>
#include <VBox/com/SupportErrorInfo.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/file.h>
#include <iprt/tcp.h>

#include <list>
#include <memory>

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////

/**
 * Asynchronous task thread parameter bucket.
 *
 * Note that instances of this class must be created using new() because the
 * task thread function will delete them when the task is complete!
 *
 * @note The constructor of this class adds a caller on the managed HardDisk
 *       object which is automatically released upon destruction.
 */
struct HardDisk::Task : public com::SupportErrorInfoBase
{
    enum Operation { CreateDynamic, CreateFixed, CreateDiff,
                     Merge, Clone, Delete, Reset };

    HardDisk *that;
    VirtualBoxBaseProto::AutoCaller autoCaller;

    ComObjPtr <Progress> progress;
    Operation operation;

    /** Where to save the result when executed using #runNow(). */
    HRESULT rc;

    Task (HardDisk *aThat, Progress *aProgress, Operation aOperation)
        : that (aThat), autoCaller (aThat)
        , progress (aProgress)
        , operation (aOperation)
        , rc (S_OK) {}

    ~Task();

    void setData (HardDisk *aTarget)
    {
        d.target = aTarget;
        HRESULT rc = d.target->addCaller();
        AssertComRC (rc);
    }

    void setData (MergeChain *aChain)
    {
        AssertReturnVoid (aChain != NULL);
        d.chain.reset (aChain);
    }

    HRESULT startThread();
    HRESULT runNow();

    struct Data
    {
        Data() : size (0) {}

        /* CreateDynamic, CreateStatic */

        uint64_t size;

        /* CreateDiff */

        ComObjPtr<HardDisk> target;

        /* Merge */

        /** Hard disks to merge, in {parent,child} order */
        std::auto_ptr <MergeChain> chain;
    }
    d;

protected:

    // SupportErrorInfoBase interface
    const GUID &mainInterfaceID() const { return COM_IIDOF (IHardDisk); }
    const char *componentName() const { return HardDisk::ComponentName(); }
};

HardDisk::Task::~Task()
{
    /* remove callers added by setData() */
    if (!d.target.isNull())
        d.target->releaseCaller();
}

/**
 * Starts a new thread driven by the HardDisk::taskThread() function and passes
 * this Task instance as an argument.
 *
 * Note that if this method returns success, this Task object becomes an ownee
 * of the started thread and will be automatically deleted when the thread
 * terminates.
 *
 * @note When the task is executed by this method, IProgress::notifyComplete()
 *       is automatically called for the progress object associated with this
 *       task when the task is finished to signal the operation completion for
 *       other threads asynchronously waiting for it.
 */
HRESULT HardDisk::Task::startThread()
{
    int vrc = RTThreadCreate (NULL, HardDisk::taskThread, this,
                              0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                              "HardDisk::Task");
    ComAssertMsgRCRet (vrc,
        ("Could not create HardDisk::Task thread (%Rrc)\n", vrc), E_FAIL);

    return S_OK;
}

/**
 * Runs HardDisk::taskThread() by passing it this Task instance as an argument
 * on the current thread instead of creating a new one.
 *
 * This call implies that it is made on another temporary thread created for
 * some asynchronous task. Avoid calling it from a normal thread since the task
 * operatinos are potentially lengthy and will block the calling thread in this
 * case.
 *
 * Note that this Task object will be deleted by taskThread() when this method
 * returns!
 *
 * @note When the task is executed by this method, IProgress::notifyComplete()
 *       is not called for the progress object associated with this task when
 *       the task is finished. Instead, the result of the operation is returned
 *       by this method directly and it's the caller's responsibility to
 *       complete the progress object in this case.
 */
HRESULT HardDisk::Task::runNow()
{
    HardDisk::taskThread (NIL_RTTHREAD, this);

    return rc;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * Helper class for merge operations.
 *
 * @note It is assumed that when modifying methods of this class are called,
 *       HardDisk::treeLock() is held in read mode.
 */
class HardDisk::MergeChain : public HardDisk::List,
                             public com::SupportErrorInfoBase
{
public:

    MergeChain (bool aForward, bool aIgnoreAttachments)
        : mForward (aForward)
        , mIgnoreAttachments (aIgnoreAttachments) {}

    ~MergeChain()
    {
        for (iterator it = mChildren.begin(); it != mChildren.end(); ++ it)
        {
            HRESULT rc = (*it)->UnlockWrite (NULL);
            AssertComRC (rc);

            (*it)->releaseCaller();
        }

        for (iterator it = begin(); it != end(); ++ it)
        {
            AutoWriteLock alock (*it);
            Assert ((*it)->m.state == MediaState_LockedWrite ||
                    (*it)->m.state == MediaState_Deleting);
            if ((*it)->m.state == MediaState_LockedWrite)
                (*it)->UnlockWrite (NULL);
            else
                (*it)->m.state = MediaState_Created;

            (*it)->releaseCaller();
        }

        if (!mParent.isNull())
            mParent->releaseCaller();
    }

    HRESULT addSource (HardDisk *aHardDisk)
    {
        HRESULT rc = aHardDisk->addCaller();
        CheckComRCReturnRC (rc);

        AutoWriteLock alock (aHardDisk);

        if (mForward)
        {
            rc = checkChildrenAndAttachmentsAndImmutable (aHardDisk);
            if (FAILED (rc))
            {
                aHardDisk->releaseCaller();
                return rc;
            }
        }

        /* go to Deleting */
        switch (aHardDisk->m.state)
        {
            case MediaState_Created:
                aHardDisk->m.state = MediaState_Deleting;
                break;
            default:
                aHardDisk->releaseCaller();
                return aHardDisk->setStateError();
        }

        push_front (aHardDisk);

        if (mForward)
        {
            /* we will need parent to reparent target */
            if (!aHardDisk->mParent.isNull())
            {
                rc = aHardDisk->mParent->addCaller();
                CheckComRCReturnRC (rc);

                mParent = aHardDisk->mParent;
            }
        }
        else
        {
            /* we will need to reparent children */
            for (List::const_iterator it = aHardDisk->children().begin();
                 it != aHardDisk->children().end(); ++ it)
            {
                rc = (*it)->addCaller();
                CheckComRCReturnRC (rc);

                rc = (*it)->LockWrite (NULL);
                if (FAILED (rc))
                {
                    (*it)->releaseCaller();
                    return rc;
                }

                mChildren.push_back (*it);
            }
        }

        return S_OK;
    }

    HRESULT addTarget (HardDisk *aHardDisk)
    {
        HRESULT rc = aHardDisk->addCaller();
        CheckComRCReturnRC (rc);

        AutoWriteLock alock (aHardDisk);

        if (!mForward)
        {
            rc = checkChildrenAndImmutable (aHardDisk);
            if (FAILED (rc))
            {
                aHardDisk->releaseCaller();
                return rc;
            }
        }

        /* go to LockedWrite */
        rc = aHardDisk->LockWrite (NULL);
        if (FAILED (rc))
        {
            aHardDisk->releaseCaller();
            return rc;
        }

        push_front (aHardDisk);

        return S_OK;
    }

    HRESULT addIntermediate (HardDisk *aHardDisk)
    {
        HRESULT rc = aHardDisk->addCaller();
        CheckComRCReturnRC (rc);

        AutoWriteLock alock (aHardDisk);

        rc = checkChildrenAndAttachments (aHardDisk);
        if (FAILED (rc))
        {
            aHardDisk->releaseCaller();
            return rc;
        }

        /* go to Deleting */
        switch (aHardDisk->m.state)
        {
            case MediaState_Created:
                aHardDisk->m.state = MediaState_Deleting;
                break;
            default:
                aHardDisk->releaseCaller();
                return aHardDisk->setStateError();
        }

        push_front (aHardDisk);

        return S_OK;
    }

    bool isForward() const { return mForward; }
    HardDisk *parent() const { return mParent; }
    const List &children() const { return mChildren; }

    HardDisk *source() const
    { AssertReturn (size() > 0, NULL); return mForward ? front() : back(); }

    HardDisk *target() const
    { AssertReturn (size() > 0, NULL); return mForward ? back() : front(); }

protected:

    // SupportErrorInfoBase interface
    const GUID &mainInterfaceID() const { return COM_IIDOF (IHardDisk); }
    const char *componentName() const { return HardDisk::ComponentName(); }

private:

    HRESULT check (HardDisk *aHardDisk, bool aChildren, bool aAttachments,
                   bool aImmutable)
    {
        if (aChildren)
        {
            /* not going to multi-merge as it's too expensive */
            if (aHardDisk->children().size() > 1)
            {
                return setError (E_FAIL,
                    tr ("Hard disk '%ls' involved in the merge operation "
                        "has more than one child hard disk (%d)"),
                    aHardDisk->m.locationFull.raw(),
                    aHardDisk->children().size());
            }
        }

        if (aAttachments && !mIgnoreAttachments)
        {
            if (aHardDisk->m.backRefs.size() != 0)
                return setError (E_FAIL,
                    tr ("Hard disk '%ls' is attached to %d virtual machines"),
                        aHardDisk->m.locationFull.raw(),
                        aHardDisk->m.backRefs.size());
        }

        if (aImmutable)
        {
            if (aHardDisk->mm.type == HardDiskType_Immutable)
                return setError (E_FAIL,
                    tr ("Hard disk '%ls' is immutable"),
                        aHardDisk->m.locationFull.raw());
        }

        return S_OK;
    }

    HRESULT checkChildren (HardDisk *aHardDisk)
    { return check (aHardDisk, true, false, false); }

    HRESULT checkChildrenAndImmutable (HardDisk *aHardDisk)
    { return check (aHardDisk, true, false, true); }

    HRESULT checkChildrenAndAttachments (HardDisk *aHardDisk)
    { return check (aHardDisk, true, true, false); }

    HRESULT checkChildrenAndAttachmentsAndImmutable (HardDisk *aHardDisk)
    { return check (aHardDisk, true, true, true); }

    /** true if forward merge, false if backward */
    bool mForward : 1;
    /** true to not perform attachment checks */
    bool mIgnoreAttachments : 1;

    /** Parent of the source when forward merge (if any) */
    ComObjPtr <HardDisk> mParent;
    /** Children of the source when backward merge (if any) */
    List mChildren;
};

////////////////////////////////////////////////////////////////////////////////
// HardDisk class
////////////////////////////////////////////////////////////////////////////////

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HardDisk)

HRESULT HardDisk::FinalConstruct()
{
    /* Initialize the callbacks of the VD error interface */
    mm.vdIfCallsError.cbSize = sizeof (VDINTERFACEERROR);
    mm.vdIfCallsError.enmInterface = VDINTERFACETYPE_ERROR;
    mm.vdIfCallsError.pfnError = vdErrorCall;

    /* Initialize the callbacks of the VD progress interface */
    mm.vdIfCallsProgress.cbSize = sizeof (VDINTERFACEPROGRESS);
    mm.vdIfCallsProgress.enmInterface = VDINTERFACETYPE_PROGRESS;
    mm.vdIfCallsProgress.pfnProgress = vdProgressCall;

    /* Initialize the callbacks of the VD config interface */
    mm.vdIfCallsConfig.cbSize = sizeof (VDINTERFACECONFIG);
    mm.vdIfCallsConfig.enmInterface = VDINTERFACETYPE_CONFIG;
    mm.vdIfCallsConfig.pfnAreKeysValid = vdConfigAreKeysValid;
    mm.vdIfCallsConfig.pfnQuerySize = vdConfigQuerySize;
    mm.vdIfCallsConfig.pfnQuery = vdConfigQuery;

    /* Initialize the callbacks of the VD TCP interface (we always use the host
     * IP stack for now) */
    mm.vdIfCallsTcpNet.cbSize = sizeof (VDINTERFACETCPNET);
    mm.vdIfCallsTcpNet.enmInterface = VDINTERFACETYPE_TCPNET;
    mm.vdIfCallsTcpNet.pfnClientConnect = RTTcpClientConnect;
    mm.vdIfCallsTcpNet.pfnClientClose = RTTcpClientClose;
    mm.vdIfCallsTcpNet.pfnSelectOne = RTTcpSelectOne;
    mm.vdIfCallsTcpNet.pfnRead = RTTcpRead;
    mm.vdIfCallsTcpNet.pfnWrite = RTTcpWrite;
    mm.vdIfCallsTcpNet.pfnFlush = RTTcpFlush;

    /* Initialize the per-disk interface chain */
    int vrc;
    vrc = VDInterfaceAdd (&mm.vdIfError,
                          "HardDisk::vdInterfaceError",
                          VDINTERFACETYPE_ERROR,
                          &mm.vdIfCallsError, this, &mm.vdDiskIfaces);
    AssertRCReturn (vrc, E_FAIL);

    vrc = VDInterfaceAdd (&mm.vdIfProgress,
                          "HardDisk::vdInterfaceProgress",
                          VDINTERFACETYPE_PROGRESS,
                          &mm.vdIfCallsProgress, this, &mm.vdDiskIfaces);
    AssertRCReturn (vrc, E_FAIL);

    vrc = VDInterfaceAdd (&mm.vdIfConfig,
                          "HardDisk::vdInterfaceConfig",
                          VDINTERFACETYPE_CONFIG,
                          &mm.vdIfCallsConfig, this, &mm.vdDiskIfaces);
    AssertRCReturn (vrc, E_FAIL);

    vrc = VDInterfaceAdd (&mm.vdIfTcpNet,
                          "HardDisk::vdInterfaceTcpNet",
                          VDINTERFACETYPE_TCPNET,
                          &mm.vdIfCallsTcpNet, this, &mm.vdDiskIfaces);
    AssertRCReturn (vrc, E_FAIL);

    return S_OK;
}

void HardDisk::FinalRelease()
{
    uninit();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the hard disk object without creating or opening an associated
 * storage unit.
 *
 * For hard disks that don't have the VD_CAP_CREATE_FIXED or
 * VD_CAP_CREATE_DYNAMIC capability (and therefore cannot be created or deleted
 * with the means of VirtualBox) the associated storage unit is assumed to be
 * ready for use so the state of the hard disk object will be set to Created.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aLocaiton     Storage unit location.
 */
HRESULT HardDisk::init (VirtualBox *aVirtualBox, CBSTR aFormat,
                        CBSTR aLocation)
{
    AssertReturn (aVirtualBox != NULL, E_FAIL);
    AssertReturn (aFormat != NULL && *aFormat != '\0', E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst (mVirtualBox) = aVirtualBox;

    /* register with VirtualBox early, since uninit() will
     * unconditionally unregister on failure */
    aVirtualBox->addDependentChild (this);

    /* no storage yet */
    m.state = MediaState_NotCreated;

    /* No storage unit is created yet, no need to queryInfo() */

    rc = setFormat (aFormat);
    CheckComRCReturnRC (rc);

    if (mm.formatObj->capabilities() & HardDiskFormatCapabilities_File)
    {
        rc = setLocation (aLocation);
        CheckComRCReturnRC (rc);
    }
    else
    {
        rc = setLocation (aLocation);
        CheckComRCReturnRC (rc);

        /// @todo later we may want to use a pfnComposeLocation backend info
        /// callback to generate a well-formed location value (based on the hard
        /// disk properties we have) rather than allowing each caller to invent
        /// its own (pseudo-)location.
    }

    if (!(mm.formatObj->capabilities() &
          (HardDiskFormatCapabilities_CreateFixed |
           HardDiskFormatCapabilities_CreateDynamic)))
    {
        /* storage for hard disks of this format can neither be explicitly
         * created by VirtualBox nor deleted, so we place the hard disk to
         * Created state here and also add it to the registry */
        m.state = MediaState_Created;
        unconst (m.id).create();
        rc = mVirtualBox->registerHardDisk (this);

        /// @todo later we may want to use a pfnIsConfigSufficient backend info
        /// callback that would tell us when we have enough properties to work
        /// with the hard disk and this information could be used to actually
        /// move such hard disks from NotCreated to Created state. Instead of
        /// pfnIsConfigSufficient we can use HardDiskFormat property
        /// descriptions to see which properties are mandatory
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the hard disk object by opening the storage unit at the specified
 * location.
 *
 * Note that the UUID, format and the parent of this hard disk will be
 * determined when reading the hard disk storage unit. If the detected parent is
 * not known to VirtualBox, then this method will fail.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aLocaiton     Storage unit location.
 */
HRESULT HardDisk::init (VirtualBox *aVirtualBox, CBSTR aLocation)
{
    AssertReturn (aVirtualBox, E_INVALIDARG);
    AssertReturn (aLocation, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst (mVirtualBox) = aVirtualBox;

    /* register with VirtualBox early, since uninit() will
     * unconditionally unregister on failure */
    aVirtualBox->addDependentChild (this);

    /* there must be a storage unit */
    m.state = MediaState_Created;

    rc = setLocation (aLocation);
    CheckComRCReturnRC (rc);

    /* get all the information about the medium from the storage unit */
    rc = queryInfo();
    if (SUCCEEDED (rc))
    {
        /* if the storage unit is not accessible, it's not acceptable for the
         * newly opened media so convert this into an error */
        if (m.state == MediaState_Inaccessible)
        {
            Assert (!m.lastAccessError.isEmpty());
            rc = setError (E_FAIL, Utf8Str (m.lastAccessError));
        }
        else
        {
            /* storage format must be detected by queryInfo() if the medium is
            * accessible */
            AssertReturn (!m.id.isEmpty() && !mm.format.isNull(), E_FAIL);
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the hard disk object by loading its data from the given settings
 * node.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aParent       Parent hard disk or NULL for a root (base) hard disk.
 * @param aNode         <HardDisk> settings node.
 *
 * @note Locks VirtualBox lock for writing, treeLock() for writing.
 */
HRESULT HardDisk::init (VirtualBox *aVirtualBox, HardDisk *aParent,
                        const settings::Key &aNode)
{
    using namespace settings;

    AssertReturn (aVirtualBox, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox and parent weakly */
    unconst (mVirtualBox) = aVirtualBox;

    /* register with VirtualBox/parent early, since uninit() will
     * unconditionally unregister on failure */
    if (aParent == NULL)
        aVirtualBox->addDependentChild (this);
    else
    {
        /* we set mParent */
        AutoWriteLock treeLock (this->treeLock());

        mParent = aParent;
        aParent->addDependentChild (this);
    }

    /* see below why we don't call queryInfo() (and therefore treat the medium
     * as inaccessible for now */
    m.state = MediaState_Inaccessible;
    m.lastAccessError = tr ("Accessibility check was not yet performed");

    /* required */
    unconst (m.id) = aNode.value <Guid> ("uuid");

    /* optional */
    {
        settings::Key descNode = aNode.findKey ("Description");
        if (!descNode.isNull())
            m.description = descNode.keyStringValue();
    }

    /* required */
    Bstr format = aNode.stringValue ("format");
    AssertReturn (!format.isNull(), E_FAIL);
    rc = setFormat (format);
    CheckComRCReturnRC (rc);

    /* optional, only for diffs, default is false */
    if (aParent != NULL)
        mm.autoReset = aNode.value <bool> ("autoReset");
    else
        mm.autoReset = false;

    /* properties (after setting the format as it populates the map). Note that
     * if some properties are not supported but preseint in the settings file,
     * they will still be read and accessible (for possible backward
     * compatibility; we can also clean them up from the XML upon next
     * XML format versino change if we wish) */
    Key::List properties = aNode.keys ("Property");
    for (Key::List::const_iterator it = properties.begin();
         it != properties.end(); ++ it)
    {
        mm.properties [Bstr (it->stringValue ("name"))] =
            Bstr (it->stringValue ("value"));
    }

    /* required */
    Bstr location = aNode.stringValue ("location");
    rc = setLocation (location);
    CheckComRCReturnRC (rc);

    /* type is only for base hard disks */
    if (mParent.isNull())
    {
        const char *type = aNode.stringValue ("type");
        if      (strcmp (type, "Normal") == 0)
            mm.type = HardDiskType_Normal;
        else if (strcmp (type, "Immutable") == 0)
            mm.type = HardDiskType_Immutable;
        else if (strcmp (type, "Writethrough") == 0)
            mm.type = HardDiskType_Writethrough;
        else
            AssertFailed();
    }

    LogFlowThisFunc (("m.locationFull='%ls', mm.format=%ls, m.id={%RTuuid}\n",
                      m.locationFull.raw(), mm.format.raw(), m.id.raw()));

    /* Don't call queryInfo() for registered media to prevent the calling
     * thread (i.e. the VirtualBox server startup thread) from an unexpected
     * freeze but mark it as initially inaccessible instead. The vital UUID,
     * location and format properties are read from the registry file above; to
     * get the actual state and the rest of the data, the user will have to call
     * COMGETTER(State). */

    /* load all children */
    Key::List hardDisks = aNode.keys ("HardDisk");
    for (Key::List::const_iterator it = hardDisks.begin();
         it != hardDisks.end(); ++ it)
    {
        ComObjPtr<HardDisk> hardDisk;
        hardDisk.createObject();
        rc = hardDisk->init (aVirtualBox, this, *it);
        CheckComRCBreakRC (rc);

        rc = mVirtualBox->registerHardDisk(hardDisk, false /* aSaveRegistry */);
        CheckComRCBreakRC (rc);
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED (rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Uninitializes the instance.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 *
 * @note All children of this hard disk get uninitialized by calling their
 *       uninit() methods.
 *
 * @note Locks treeLock() for writing, VirtualBox for writing.
 */
void HardDisk::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

    if (!mm.formatObj.isNull())
    {
        /* remove the caller reference we added in setFormat() */
        mm.formatObj->releaseCaller();
        mm.formatObj.setNull();
    }

    if (m.state == MediaState_Deleting)
    {
        /* we are being uninitialized after've been deleted by merge.
         * Reparenting has already been done so don't touch it here (we are
         * now orphans and remoeDependentChild() will assert) */

        Assert (mParent.isNull());
    }
    else
    {
        /* we uninit children and reset mParent
         * and VirtualBox::removeDependentChild() needs a write lock */
        AutoMultiWriteLock2 alock (mVirtualBox->lockHandle(), this->treeLock());

        uninitDependentChildren();

        if (!mParent.isNull())
        {
            mParent->removeDependentChild (this);
            mParent.setNull();
        }
        else
            mVirtualBox->removeDependentChild (this);
    }

    unconst (mVirtualBox).setNull();
}

// IHardDisk properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDisk::COMGETTER(Format) (BSTR *aFormat)
{
    if (aFormat == NULL)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* no need to lock, mm.format is const */
    mm.format.cloneTo (aFormat);

    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Type) (HardDiskType_T *aType)
{
    if (aType == NULL)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    *aType = mm.type;

    return S_OK;
}

STDMETHODIMP HardDisk::COMSETTER(Type) (HardDiskType_T aType)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    if (mm.type == aType)
    {
        /* Nothing to do */
        return S_OK;
    }

    /* we access mParent & children() */
    AutoReadLock treeLock (this->treeLock());

    /* cannot change the type of a differencing hard disk */
    if (!mParent.isNull())
        return setError (E_FAIL,
            tr ("Hard disk '%ls' is a differencing hard disk"),
                m.locationFull.raw());

    /* cannot change the type of a hard disk being in use */
    if (m.backRefs.size() != 0)
        return setError (E_FAIL,
            tr ("Hard disk '%ls' is attached to %d virtual machines"),
                m.locationFull.raw(), m.backRefs.size());

    switch (aType)
    {
        case HardDiskType_Normal:
        case HardDiskType_Immutable:
        {
            /* normal can be easily converted to imutable and vice versa even
             * if they have children as long as they are not attached to any
             * machine themselves */
            break;
        }
        case HardDiskType_Writethrough:
        {
            /* cannot change to writethrough if there are children */
            if (children().size() != 0)
                return setError (E_FAIL,
                    tr ("Hard disk '%ls' has %d child hard disks"),
                        children().size());
            break;
        }
        default:
            AssertFailedReturn (E_FAIL);
    }

    mm.type = aType;

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP HardDisk::COMGETTER(Parent) (IHardDisk **aParent)
{
    if (aParent == NULL)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* we access mParent */
    AutoReadLock treeLock (this->treeLock());

    mParent.queryInterfaceTo (aParent);

    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Children) (ComSafeArrayOut (IHardDisk *, aChildren))
{
    if (ComSafeArrayOutIsNull (aChildren))
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* we access children */
    AutoReadLock treeLock (this->treeLock());

    SafeIfaceArray<IHardDisk> children (this->children());
    children.detachTo (ComSafeArrayOutArg (aChildren));

    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(Root)(IHardDisk **aRoot)
{
    if (aRoot == NULL)
        return E_POINTER;

    /* root() will do callers/locking */

    root().queryInterfaceTo (aRoot);

    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(ReadOnly) (BOOL *aReadOnly)
{
    if (aReadOnly == NULL)
        return E_POINTER;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* isRadOnly() will do locking */

    *aReadOnly = isReadOnly();

    return S_OK;
}

STDMETHODIMP HardDisk::COMGETTER(LogicalSize) (ULONG64 *aLogicalSize)
{
    CheckComArgOutPointerValid (aLogicalSize);

    {
        AutoCaller autoCaller (this);
        CheckComRCReturnRC (autoCaller.rc());

        AutoReadLock alock (this);

        /* we access mParent */
        AutoReadLock treeLock (this->treeLock());

        if (mParent.isNull())
        {
            *aLogicalSize = mm.logicalSize;

            return S_OK;
        }
    }

    /* We assume that some backend may decide to return a meaningless value in
     * response to VDGetSize() for differencing hard disks and therefore
     * always ask the base hard disk ourselves. */

    /* root() will do callers/locking */

    return root()->COMGETTER (LogicalSize) (aLogicalSize);
}

STDMETHODIMP HardDisk::COMGETTER(AutoReset) (BOOL *aAutoReset)
{
    CheckComArgOutPointerValid (aAutoReset);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    if (mParent.isNull())
        *aAutoReset = FALSE;

    *aAutoReset = mm.autoReset;

    return S_OK;
}

STDMETHODIMP HardDisk::COMSETTER(AutoReset) (BOOL aAutoReset)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    if (mParent.isNull())
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Hard disk '%ls' is not differencing"),
            m.locationFull.raw());

    if (mm.autoReset != aAutoReset)
    {
        mm.autoReset = aAutoReset;

        return mVirtualBox->saveSettings();
    }

    return S_OK;
}

// IHardDisk methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP HardDisk::GetProperty (IN_BSTR aName, BSTR *aValue)
{
    CheckComArgStrNotEmptyOrNull (aName);
    CheckComArgOutPointerValid (aValue);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Data::PropertyMap::const_iterator it = mm.properties.find (Bstr (aName));
    if (it == mm.properties.end())
        return setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("Property '%ls' does not exist"), aName);

    it->second.cloneTo (aValue);

    return S_OK;
}

STDMETHODIMP HardDisk::SetProperty (IN_BSTR aName, IN_BSTR aValue)
{
    CheckComArgStrNotEmptyOrNull (aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    Data::PropertyMap::iterator it = mm.properties.find (Bstr (aName));
    if (it == mm.properties.end())
        return setError (VBOX_E_OBJECT_NOT_FOUND,
            tr ("Property '%ls' does not exist"), aName);

    it->second = aValue;

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP HardDisk::GetProperties(IN_BSTR aNames,
                                     ComSafeArrayOut (BSTR, aReturnNames),
                                     ComSafeArrayOut (BSTR, aReturnValues))
{
    CheckComArgOutSafeArrayPointerValid (aReturnNames);
    CheckComArgOutSafeArrayPointerValid (aReturnValues);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /// @todo make use of aNames according to the documentation
    NOREF (aNames);

    com::SafeArray <BSTR> names (mm.properties.size());
    com::SafeArray <BSTR> values (mm.properties.size());
    size_t i = 0;

    for (Data::PropertyMap::const_iterator it = mm.properties.begin();
          it != mm.properties.end(); ++ it)
    {
        it->first.cloneTo (&names [i]);
        it->second.cloneTo (&values [i]);
        ++ i;
    }

    names.detachTo (ComSafeArrayOutArg (aReturnNames));
    values.detachTo (ComSafeArrayOutArg (aReturnValues));

    return S_OK;
}

STDMETHODIMP HardDisk::SetProperties(ComSafeArrayIn (IN_BSTR, aNames),
                                      ComSafeArrayIn (IN_BSTR, aValues))
{
    CheckComArgSafeArrayNotNull (aNames);
    CheckComArgSafeArrayNotNull (aValues);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    com::SafeArray <IN_BSTR> names (ComSafeArrayInArg (aNames));
    com::SafeArray <IN_BSTR> values (ComSafeArrayInArg (aValues));

    /* first pass: validate names */
    for (size_t i = 0; i < names.size(); ++ i)
    {
        if (mm.properties.find (Bstr (names [i])) == mm.properties.end())
            return setError (VBOX_E_OBJECT_NOT_FOUND,
                tr ("Property '%ls' does not exist"), names [i]);
    }

    /* second pass: assign */
    for (size_t i = 0; i < names.size(); ++ i)
    {
        Data::PropertyMap::iterator it = mm.properties.find (Bstr (names [i]));
        AssertReturn (it != mm.properties.end(), E_FAIL);

        it->second = values [i];
    }

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP HardDisk::CreateDynamicStorage(ULONG64 aLogicalSize,
                                            IProgress **aProgress)
{
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!(mm.formatObj->capabilities() &
          HardDiskFormatCapabilities_CreateDynamic))
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Hard disk format '%ls' does not support dynamic storage "
                "creation"), mm.format.raw());

    switch (m.state)
    {
        case MediaState_NotCreated:
            break;
        default:
            return setStateError();
    }

    ComObjPtr <Progress> progress;
    progress.createObject();
    HRESULT rc = progress->init (mVirtualBox, static_cast<IHardDisk*>(this),
        BstrFmt (tr ("Creating dynamic hard disk storage unit '%ls'"),
                 m.locationFull.raw()),
        FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr <Task> task (new Task (this, progress, Task::CreateDynamic));
    AssertComRCReturnRC (task->autoCaller.rc());

    task->d.size = aLogicalSize;

    rc = task->startThread();
    CheckComRCReturnRC (rc);

    /* go to Creating state on success */
    m.state = MediaState_Creating;

    /* task is now owned by taskThread() so release it */
    task.release();

    /* return progress to the caller */
    progress.queryInterfaceTo (aProgress);

    return S_OK;
}

STDMETHODIMP HardDisk::CreateFixedStorage(ULONG64 aLogicalSize,
                                          IProgress **aProgress)
{
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (!(mm.formatObj->capabilities() &
          HardDiskFormatCapabilities_CreateFixed))
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Hard disk format '%ls' does not support fixed storage "
                "creation"), mm.format.raw());

    switch (m.state)
    {
        case MediaState_NotCreated:
            break;
        default:
            return setStateError();
    }

    ComObjPtr <Progress> progress;
    progress.createObject();
    HRESULT rc = progress->init (mVirtualBox, static_cast<IHardDisk*>(this),
        BstrFmt (tr ("Creating fixed hard disk storage unit '%ls'"),
                 m.locationFull.raw()),
        FALSE /* aCancelable */);
    CheckComRCReturnRC (rc);

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr <Task> task (new Task (this, progress, Task::CreateFixed));
    AssertComRCReturnRC (task->autoCaller.rc());

    task->d.size = aLogicalSize;

    rc = task->startThread();
    CheckComRCReturnRC (rc);

    /* go to Creating state on success */
    m.state = MediaState_Creating;

    /* task is now owned by taskThread() so release it */
    task.release();

    /* return progress to the caller */
    progress.queryInterfaceTo (aProgress);

    return S_OK;
}

STDMETHODIMP HardDisk::DeleteStorage (IProgress **aProgress)
{
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <Progress> progress;

    HRESULT rc = deleteStorageNoWait (progress);
    if (SUCCEEDED (rc))
    {
        /* return progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }

    return rc;
}

STDMETHODIMP HardDisk::CreateDiffStorage (IHardDisk *aTarget, IProgress **aProgress)
{
    CheckComArgNotNull (aTarget);
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr<HardDisk> diff;
    HRESULT rc = mVirtualBox->cast (aTarget, diff);
    CheckComRCReturnRC (rc);

    AutoWriteLock alock (this);

    if (mm.type == HardDiskType_Writethrough)
        return setError (E_FAIL,
            tr ("Hard disk '%ls' is Writethrough"),
            m.locationFull.raw());

    /* We want to be locked for reading as long as our diff child is being
     * created */
    rc = LockRead (NULL);
    CheckComRCReturnRC (rc);

    ComObjPtr <Progress> progress;

    rc = createDiffStorageNoWait (diff, progress);
    if (FAILED (rc))
    {
        HRESULT rc2 = UnlockRead (NULL);
        AssertComRC (rc2);
        /* Note: on success, taskThread() will unlock this */
    }
    else
    {
        /* return progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }

    return rc;
}

STDMETHODIMP HardDisk::MergeTo (IN_GUID aTargetId, IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ReturnComNotImplemented();
}

STDMETHODIMP HardDisk::CloneTo (IHardDisk *aTarget, IProgress **aProgress)
{
    CheckComArgNotNull (aTarget);
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ComObjPtr <HardDisk> target;
    HRESULT rc = mVirtualBox->cast (aTarget, target);
    CheckComRCReturnRC (rc);

    AutoMultiWriteLock2 alock (this, target);

    /* We want to be locked for reading as long as the clone hard disk is being
     * created*/
    rc = LockRead (NULL);
    CheckComRCReturnRC (rc);

    ComObjPtr <Progress> progress;

    try
    {
        if (target->m.state != MediaState_NotCreated)
            throw target->setStateError();

        progress.createObject();
        rc = progress->init (mVirtualBox, static_cast <IHardDisk *> (this),
            BstrFmt (tr ("Creating clone hard disk '%ls'"),
                     target->m.locationFull.raw()),
            FALSE /* aCancelable */);
        CheckComRCThrowRC (rc);

        /* setup task object and thread to carry out the operation
         * asynchronously */

        std::auto_ptr <Task> task (new Task (this, progress, Task::Clone));
        AssertComRCThrowRC (task->autoCaller.rc());

        task->setData (target);

        rc = task->startThread();
        CheckComRCThrowRC (rc);

        /* go to Creating state before leaving the lock */
        target->m.state = MediaState_Creating;

        /* task is now owned (or already deleted) by taskThread() so release it */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (FAILED (rc))
    {
        HRESULT rc2 = UnlockRead (NULL);
        AssertComRC (rc2);
        /* Note: on success, taskThread() will unlock this */
    }
    else
    {
        /* return progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }

    return rc;
}

STDMETHODIMP HardDisk::FlattenTo (IHardDisk *aTarget, IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ReturnComNotImplemented();
}

STDMETHODIMP HardDisk::Compact (IProgress **aProgress)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ReturnComNotImplemented();
}

STDMETHODIMP HardDisk::Reset (IProgress **aProgress)
{
    CheckComArgOutPointerValid (aProgress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoWriteLock alock (this);

    if (mParent.isNull())
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Hard disk '%ls' is not differencing"),
            m.locationFull.raw());

    HRESULT rc = canClose();
    CheckComRCReturnRC (rc);

    rc = LockWrite (NULL);
    CheckComRCReturnRC (rc);

    ComObjPtr <Progress> progress;

    try
    {
        progress.createObject();
        rc = progress->init (mVirtualBox, static_cast <IHardDisk *> (this),
            BstrFmt (tr ("Resetting differencing hard disk '%ls'"),
                     m.locationFull.raw()),
            FALSE /* aCancelable */);
        CheckComRCThrowRC (rc);

        /* setup task object and thread to carry out the operation
         * asynchronously */

        std::auto_ptr <Task> task (new Task (this, progress, Task::Reset));
        AssertComRCThrowRC (task->autoCaller.rc());

        rc = task->startThread();
        CheckComRCThrowRC (rc);

        /* task is now owned (or already deleted) by taskThread() so release it */
        task.release();
    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    if (FAILED (rc))
    {
        HRESULT rc2 = UnlockWrite (NULL);
        AssertComRC (rc2);
        /* Note: on success, taskThread() will unlock this */
    }
    else
    {
        /* return progress to the caller */
        progress.queryInterfaceTo (aProgress);
    }

    return rc;
}

// public methods for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 * Checks if the given change of \a aOldPath to \a aNewPath affects the location
 * of this hard disk or any its child and updates the paths if necessary to
 * reflect the new location.
 *
 * @param aOldPath  Old path (full).
 * @param aNewPath  New path (full).
 *
 * @note Locks treeLock() for reading, this object and all children for writing.
 */
void HardDisk::updatePaths (const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid (aOldPath);
    AssertReturnVoid (aNewPath);

    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock (this);

    /* we access children() */
    AutoReadLock treeLock (this->treeLock());

    updatePath (aOldPath, aNewPath);

    /* update paths of all children */
    for (List::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        (*it)->updatePaths (aOldPath, aNewPath);
    }
}

/**
 * Returns the base hard disk of the hard disk chain this hard disk is part of.
 *
 * The root hard disk is found by walking up the parent-child relationship axis.
 * If the hard disk doesn't have a parent (i.e. it's a base hard disk), it
 * returns itself in response to this method.
 *
 * @param aLevel    Where to store the number of ancestors of this hard disk
 *                  (zero for the root), may be @c NULL.
 *
 * @note Locks treeLock() for reading.
 */
ComObjPtr <HardDisk> HardDisk::root (uint32_t *aLevel /*= NULL*/)
{
    ComObjPtr <HardDisk> root;
    uint32_t level;

    AutoCaller autoCaller (this);
    AssertReturn (autoCaller.isOk(), root);

    /* we access mParent */
    AutoReadLock treeLock (this->treeLock());

    root = this;
    level = 0;

    if (!mParent.isNull())
    {
        for (;;)
        {
            AutoCaller rootCaller (root);
            AssertReturn (rootCaller.isOk(), root);

            if (root->mParent.isNull())
                break;

            root = root->mParent;
            ++ level;
        }
    }

    if (aLevel != NULL)
        *aLevel = level;

    return root;
}

/**
 * Returns @c true if this hard disk cannot be modified because it has
 * dependants (children) or is part of the snapshot. Related to the hard disk
 * type and posterity, not to the current media state.
 *
 * @note Locks this object and treeLock() for reading.
 */
bool HardDisk::isReadOnly()
{
    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), false);

    AutoReadLock alock (this);

    /* we access children */
    AutoReadLock treeLock (this->treeLock());

    switch (mm.type)
    {
        case HardDiskType_Normal:
        {
            if (children().size() != 0)
                return true;

            for (BackRefList::const_iterator it = m.backRefs.begin();
                 it != m.backRefs.end(); ++ it)
                if (it->snapshotIds.size() != 0)
                    return true;

            return false;
        }
        case HardDiskType_Immutable:
        {
            return true;
        }
        case HardDiskType_Writethrough:
        {
            return false;
        }
        default:
            break;
    }

    AssertFailedReturn (false);
}

/**
 * Saves hard disk data by appending a new <HardDisk> child node to the given
 * parent node which can be either <HardDisks> or <HardDisk>.
 *
 * @param aaParentNode  Parent <HardDisks> or <HardDisk> node.
 *
 * @note Locks this object, treeLock() and children for reading.
 */
HRESULT HardDisk::saveSettings (settings::Key &aParentNode)
{
    using namespace settings;

    AssertReturn (!aParentNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    /* we access mParent */
    AutoReadLock treeLock (this->treeLock());

    Key diskNode = aParentNode.appendKey ("HardDisk");
    /* required */
    diskNode.setValue <Guid> ("uuid", m.id);
    /* required (note: the original locaiton, not full) */
    diskNode.setValue <Bstr> ("location", m.location);
    /* required */
    diskNode.setValue <Bstr> ("format", mm.format);
    /* optional, only for diffs, default is false */
    if (!mParent.isNull())
        diskNode.setValueOr <bool> ("autoReset", !!mm.autoReset, false);
    /* optional */
    if (!m.description.isNull())
    {
        Key descNode = diskNode.createKey ("Description");
        descNode.setKeyValue <Bstr> (m.description);
    }

    /* optional properties */
    for (Data::PropertyMap::const_iterator it = mm.properties.begin();
         it != mm.properties.end(); ++ it)
    {
        /* only save properties that have non-default values */
        if (!it->second.isNull())
        {
            Key propNode = diskNode.appendKey ("Property");
            propNode.setValue <Bstr> ("name", it->first);
            propNode.setValue <Bstr> ("value", it->second);
        }
    }

    /* only for base hard disks */
    if (mParent.isNull())
    {
        const char *type =
            mm.type == HardDiskType_Normal ? "Normal" :
            mm.type == HardDiskType_Immutable ? "Immutable" :
            mm.type == HardDiskType_Writethrough ? "Writethrough" : NULL;
        Assert (type != NULL);
        diskNode.setStringValue ("type", type);
    }

    /* save all children */
    for (List::const_iterator it = children().begin();
         it != children().end();
         ++ it)
    {
        HRESULT rc = (*it)->saveSettings (diskNode);
        AssertComRCReturnRC (rc);
    }

    return S_OK;
}

/**
 * Compares the location of this hard disk to the given location.
 *
 * The comparison takes the location details into account. For example, if the
 * location is a file in the host's filesystem, a case insensitive comparison
 * will be performed for case insensitive filesystems.
 *
 * @param aLocation     Location to compare to (as is).
 * @param aResult       Where to store the result of comparison: 0 if locations
 *                      are equal, 1 if this object's location is greater than
 *                      the specified location, and -1 otherwise.
 */
HRESULT HardDisk::compareLocationTo (const char *aLocation, int &aResult)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Utf8Str locationFull (m.locationFull);

    /// @todo NEWMEDIA delegate the comparison to the backend?

    if (mm.formatObj->capabilities() & HardDiskFormatCapabilities_File)
    {
        Utf8Str location (aLocation);

        /* For locations represented by files, append the default path if
         * only the name is given, and then get the full path. */
        if (!RTPathHavePath (aLocation))
        {
            AutoReadLock propsLock (mVirtualBox->systemProperties());
            location = Utf8StrFmt ("%ls%c%s",
                mVirtualBox->systemProperties()->defaultHardDiskFolder().raw(),
                RTPATH_DELIMITER, aLocation);
        }

        int vrc = mVirtualBox->calculateFullPath (location, location);
        if (RT_FAILURE (vrc))
            return setError (E_FAIL,
                tr ("Invalid hard disk storage file location '%s' (%Rrc)"),
                location.raw(), vrc);

        aResult = RTPathCompare (locationFull, location);
    }
    else
        aResult = locationFull.compare (aLocation);

    return S_OK;
}

/**
 * Returns a short version of the location attribute.
 *
 * Reimplements MediumBase::name() to specially treat non-FS-path locations.
 *
 * @note Must be called from under this object's read or write lock.
 */
Utf8Str HardDisk::name()
{
    /// @todo NEWMEDIA treat non-FS-paths specially! (may require to requiest
    /// this information from the VD backend)

    Utf8Str location (m.locationFull);

    Utf8Str name = RTPathFilename (location);
    return name;
}

/**
 * Checks that this hard disk may be discarded and performs necessary state
 * changes.
 *
 * This method is to be called prior to calling the #discrad() to perform
 * necessary consistency checks and place involved hard disks to appropriate
 * states. If #discard() is not called or fails, the state modifications
 * performed by this method must be undone by #cancelDiscard().
 *
 * See #discard() for more info about discarding hard disks.
 *
 * @param aChain        Where to store the created merge chain (may return NULL
 *                      if no real merge is necessary).
 *
 * @note Locks treeLock() for reading. Locks this object, aTarget and all
 *       intermediate hard disks for writing.
 */
HRESULT HardDisk::prepareDiscard (MergeChain * &aChain)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    aChain = NULL;

    AutoWriteLock alock (this);

    /* we access mParent & children() */
    AutoReadLock treeLock (this->treeLock());

    AssertReturn (mm.type == HardDiskType_Normal, E_FAIL);

    if (children().size() == 0)
    {
        /* special treatment of the last hard disk in the chain: */

        if (mParent.isNull())
        {
            /* lock only, to prevent any usage; discard() will unlock */
            return LockWrite (NULL);
        }

        /* the differencing hard disk w/o children will be deleted, protect it
         * from attaching to other VMs (this is why Deleting) */

        switch (m.state)
        {
            case MediaState_Created:
                m.state = MediaState_Deleting;
                break;
            default:
                return setStateError();
        }

        /* aChain is intentionally NULL here */

        return S_OK;
    }

    /* not going multi-merge as it's too expensive */
    if (children().size() > 1)
        return setError (E_FAIL,
            tr ("Hard disk '%ls' has more than one child hard disk (%d)"),
            m.locationFull.raw(), children().size());

    /* this is a read-only hard disk with children; it must be associated with
     * exactly one snapshot (when the snapshot is being taken, none of the
     * current VM's hard disks may be attached to other VMs). Note that by the
     * time when discard() is called, there must be no any attachments at all
     * (the code calling prepareDiscard() should detach). */
    AssertReturn (m.backRefs.size() == 1 &&
                  !m.backRefs.front().inCurState &&
                  m.backRefs.front().snapshotIds.size() == 1, E_FAIL);

    ComObjPtr<HardDisk> child = children().front();

    /* we keep this locked, so lock the affected child to make sure the lock
     * order is correct when calling prepareMergeTo() */
    AutoWriteLock childLock (child);

    /* delegate the rest to the profi */
    if (mParent.isNull())
    {
        /* base hard disk, backward merge */

        Assert (child->m.backRefs.size() == 1);
        if (child->m.backRefs.front().machineId != m.backRefs.front().machineId)
        {
            /* backward merge is too tricky, we'll just detach on discard, so
             * lock only, to prevent any usage; discard() will only unlock
             * (since we return NULL in aChain) */
            return LockWrite (NULL);
        }

        return child->prepareMergeTo (this, aChain,
                                      true /* aIgnoreAttachments */);
    }
    else
    {
        /* forward merge */
        return prepareMergeTo (child, aChain,
                               true /* aIgnoreAttachments */);
    }
}

/**
 * Discards this hard disk.
 *
 * Discarding the hard disk is merging its contents to its differencing child
 * hard disk (forward merge) or contents of its child hard disk to itself
 * (backward merge) if this hard disk is a base hard disk. If this hard disk is
 * a differencing hard disk w/o children, then it will be simply deleted.
 * Calling this method on a base hard disk w/o children will do nothing and
 * silently succeed. If this hard disk has more than one child, the method will
 * currently return an error (since merging in this case would be too expensive
 * and result in data duplication).
 *
 * When the backward merge takes place (i.e. this hard disk is a target) then,
 * on success, this hard disk will automatically replace the differencing child
 * hard disk used as a source (which will then be deleted) in the attachment
 * this child hard disk is associated with. This will happen only if both hard
 * disks belong to the same machine because otherwise such a replace would be
 * too tricky and could be not expected by the other machine. Same relates to a
 * case when the child hard disk is not associated with any machine at all. When
 * the backward merge is not applied, the method behaves as if the base hard
 * disk were not attached at all -- i.e. simply detaches it from the machine but
 * leaves the hard disk chain intact.
 *
 * This method is basically a wrapper around #mergeTo() that selects the correct
 * merge direction and performs additional actions as described above and.
 *
 * Note that this method will not return until the merge operation is complete
 * (which may be quite time consuming depending on the size of the merged hard
 * disks).
 *
 * Note that #prepareDiscard() must be called before calling this method. If
 * this method returns a failure, the caller must call #cancelDiscard(). On
 * success, #cancelDiscard() must not be called (this method will perform all
 * necessary steps such as resetting states of all involved hard disks and
 * deleting @a aChain).
 *
 * @param aChain        Merge chain created by #prepareDiscard() (may be NULL if
 *                      no real merge takes place).
 *
 * @note Locks the hard disks from the chain for writing. Locks the machine
 *       object when the backward merge takes place. Locks treeLock() lock for
 *       reading or writing.
 */
HRESULT HardDisk::discard (ComObjPtr <Progress> &aProgress, MergeChain *aChain)
{
    AssertReturn (!aProgress.isNull(), E_FAIL);

    ComObjPtr <HardDisk> hdFrom;

    HRESULT rc = S_OK;

    {
        AutoCaller autoCaller (this);
        AssertComRCReturnRC (autoCaller.rc());

        aProgress->advanceOperation (BstrFmt (
            tr ("Discarding hard disk '%s'"), name().raw()));

        if (aChain == NULL)
        {
            AutoWriteLock alock (this);

            /* we access mParent & children() */
            AutoReadLock treeLock (this->treeLock());

            Assert (children().size() == 0);

            /* special treatment of the last hard disk in the chain: */

            if (mParent.isNull())
            {
                rc = UnlockWrite (NULL);
                AssertComRC (rc);
                return rc;
            }

            /* delete the differencing hard disk w/o children */

            Assert (m.state == MediaState_Deleting);

            /* go back to Created since deleteStorage() expects this state */
            m.state = MediaState_Created;

            hdFrom = this;

            rc = deleteStorageAndWait (&aProgress);
        }
        else
        {
            hdFrom = aChain->source();

            rc = hdFrom->mergeToAndWait (aChain, &aProgress);
        }
    }

    if (SUCCEEDED (rc))
    {
        /* mergeToAndWait() cannot uninitialize the initiator because of
         * possible AutoCallers on the current thread, deleteStorageAndWait()
         * doesn't do it either; do it ourselves */
        hdFrom->uninit();
    }

    return rc;
}

/**
 * Undoes what #prepareDiscard() did. Must be called if #discard() is not called
 * or fails. Frees memory occupied by @a aChain.
 *
 * @param aChain        Merge chain created by #prepareDiscard() (may be NULL if
 *                      no real merge takes place).
 *
 * @note Locks the hard disks from the chain for writing. Locks treeLock() for
 *       reading.
 */
void HardDisk::cancelDiscard (MergeChain *aChain)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    if (aChain == NULL)
    {
        AutoWriteLock alock (this);

        /* we access mParent & children() */
        AutoReadLock treeLock (this->treeLock());

        Assert (children().size() == 0);

        /* special treatment of the last hard disk in the chain: */

        if (mParent.isNull())
        {
            HRESULT rc = UnlockWrite (NULL);
            AssertComRC (rc);
            return;
        }

        /* the differencing hard disk w/o children will be deleted, protect it
         * from attaching to other VMs (this is why Deleting) */

        Assert (m.state == MediaState_Deleting);
        m.state = MediaState_Created;

        return;
    }

    /* delegate the rest to the profi */
    cancelMergeTo (aChain);
}

/**
 * Returns a preferred format for differencing hard disks.
 */
Bstr HardDisk::preferredDiffFormat()
{
    Bstr format;

    AutoCaller autoCaller (this);
    AssertComRCReturn (autoCaller.rc(), format);

    /* mm.format is const, no need to lock */
    format = mm.format;

    /* check that our own format supports diffs */
    if (!(mm.formatObj->capabilities() & HardDiskFormatCapabilities_Differencing))
    {
        /* use the default format if not */
        AutoReadLock propsLock (mVirtualBox->systemProperties());
        format = mVirtualBox->systemProperties()->defaultHardDiskFormat();
    }

    return format;
}

// protected methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Deletes the hard disk storage unit.
 *
 * If @a aProgress is not NULL but the object it points to is @c null then a new
 * progress object will be created and assigned to @a *aProgress on success,
 * otherwise the existing progress object is used. If Progress is NULL, then no
 * progress object is created/used at all.
 *
 * When @a aWait is @c false, this method will create a thread to perform the
 * delete operation asynchronously and will return immediately. Otherwise, it
 * will perform the operation on the calling thread and will not return to the
 * caller until the operation is completed. Note that @a aProgress cannot be
 * NULL when @a aWait is @c false (this method will assert in this case).
 *
 * @param aProgress     Where to find/store a Progress object to track operation
 *                      completion.
 * @param aWait         @c true if this method should block instead of creating
 *                      an asynchronous thread.
 *
 * @note Locks mVirtualBox and this object for writing. Locks treeLock() for
 *       writing.
 */
HRESULT HardDisk::deleteStorage (ComObjPtr <Progress> *aProgress, bool aWait)
{
    AssertReturn (aProgress != NULL || aWait == true, E_FAIL);

    /* unregisterWithVirtualBox() needs a write lock. We want to unregister
     * ourselves atomically after detecting that deletion is possible to make
     * sure that we don't do that after another thread has done
     * VirtualBox::findHardDisk() but before it starts using us (provided that
     * it holds a mVirtualBox lock too of course). */

    AutoWriteLock vboxLock (mVirtualBox);

    AutoWriteLock alock (this);

    if (!(mm.formatObj->capabilities() &
          (HardDiskFormatCapabilities_CreateDynamic |
           HardDiskFormatCapabilities_CreateFixed)))
        return setError (VBOX_E_NOT_SUPPORTED,
            tr ("Hard disk format '%ls' does not support storage deletion"),
            mm.format.raw());

    /* Note that we are fine with Inaccessible state too: a) for symmetry with
     * create calls and b) because it doesn't really harm to try, if it is
     * really inaccessibke, the delete operation will fail anyway. Accepting
     * Inaccessible state is especially important because all registered hard
     * disks are initially Inaccessible upon VBoxSVC startup until
     * COMGETTER(State) is called. */

    switch (m.state)
    {
        case MediaState_Created:
        case MediaState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    if (m.backRefs.size() != 0)
        return setError (VBOX_E_OBJECT_IN_USE,
            tr ("Hard disk '%ls' is attached to %d virtual machines"),
                m.locationFull.raw(), m.backRefs.size());

    HRESULT rc = canClose();
    CheckComRCReturnRC (rc);

    /* go to Deleting state before leaving the lock */
    m.state = MediaState_Deleting;

    /* we need to leave this object's write lock now because of
     * unregisterWithVirtualBox() that locks treeLock() for writing */
    alock.leave();

    /* try to remove from the list of known hard disks before performing actual
     * deletion (we favor the consistency of the media registry in the first
     * place which would have been broken if unregisterWithVirtualBox() failed
     * after we successfully deleted the storage) */

    rc = unregisterWithVirtualBox();

    alock.enter();

    /* restore the state because we may fail below; we will set it later again*/
    m.state = MediaState_Created;

    CheckComRCReturnRC (rc);

    ComObjPtr <Progress> progress;

    if (aProgress != NULL)
    {
        /* use the existing progress object... */
        progress = *aProgress;

        /* ...but create a new one if it is null */
        if (progress.isNull())
        {
            progress.createObject();
            rc = progress->init (mVirtualBox, static_cast<IHardDisk*>(this),
                BstrFmt (tr ("Deleting hard disk storage unit '%ls'"),
                         m.locationFull.raw()),
                FALSE /* aCancelable */);
            CheckComRCReturnRC (rc);
        }
    }

    std::auto_ptr <Task> task (new Task (this, progress, Task::Delete));
    AssertComRCReturnRC (task->autoCaller.rc());

    if (aWait)
    {
        /* go to Deleting state before starting the task */
        m.state = MediaState_Deleting;

        rc = task->runNow();
    }
    else
    {
        rc = task->startThread();
        CheckComRCReturnRC (rc);

        /* go to Deleting state before leaving the lock */
        m.state = MediaState_Deleting;
    }

    /* task is now owned (or already deleted) by taskThread() so release it */
    task.release();

    if (aProgress != NULL)
    {
        /* return progress to the caller */
        *aProgress = progress;
    }

    return rc;
}

/**
 * Creates a new differencing storage unit using the given target hard disk's
 * format and the location. Note that @c aTarget must be NotCreated.
 *
 * As opposed to the CreateDiffStorage() method, this method doesn't try to lock
 * this hard disk for reading assuming that the caller has already done so. This
 * is used when taking an online snaopshot (where all original hard disks are
 * locked for writing and must remain such). Note however that if @a aWait is
 * @c false and this method returns a success then the thread started by
 * this method will unlock the hard disk (unless it is in
 * MediaState_LockedWrite state) so make sure the hard disk is either in
 * MediaState_LockedWrite or call #LockRead() before calling this method! If @a
 * aWait is @c true then this method neither locks nor unlocks the hard disk, so
 * make sure you do it yourself as needed.
 *
 * If @a aProgress is not NULL but the object it points to is @c null then a new
 * progress object will be created and assigned to @a *aProgress on success,
 * otherwise the existing progress object is used. If @a aProgress is NULL, then no
 * progress object is created/used at all.
 *
 * When @a aWait is @c false, this method will create a thread to perform the
 * create operation asynchronously and will return immediately. Otherwise, it
 * will perform the operation on the calling thread and will not return to the
 * caller until the operation is completed. Note that @a aProgress cannot be
 * NULL when @a aWait is @c false (this method will assert in this case).
 *
 * @param aTarget       Target hard disk.
 * @param aProgress     Where to find/store a Progress object to track operation
 *                      completion.
 * @param aWait         @c true if this method should block instead of creating
 *                      an asynchronous thread.
 *
 * @note Locks this object and @a aTarget for writing.
 */
HRESULT HardDisk::createDiffStorage(ComObjPtr<HardDisk> &aTarget,
                                    ComObjPtr<Progress> *aProgress,
                                     bool aWait)
{
    AssertReturn (!aTarget.isNull(), E_FAIL);
    AssertReturn (aProgress != NULL || aWait == true, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoCaller targetCaller (aTarget);
    CheckComRCReturnRC (targetCaller.rc());

    AutoMultiWriteLock2 alock (this, aTarget);

    AssertReturn (mm.type != HardDiskType_Writethrough, E_FAIL);

    /* Note: MediaState_LockedWrite is ok when taking an online snapshot */
    AssertReturn (m.state == MediaState_LockedRead ||
                  m.state == MediaState_LockedWrite, E_FAIL);

    if (aTarget->m.state != MediaState_NotCreated)
        return aTarget->setStateError();

    HRESULT rc = S_OK;

    /* check that the hard disk is not attached to any VM in the current state*/
    for (BackRefList::const_iterator it = m.backRefs.begin();
         it != m.backRefs.end(); ++ it)
    {
        if (it->inCurState)
        {
            /* Note: when a VM snapshot is being taken, all normal hard disks
             * attached to the VM in the current state will be, as an exception,
             * also associated with the snapshot which is about to create  (see
             * SnapshotMachine::init()) before deassociating them from the
             * current state (which takes place only on success in
             * Machine::fixupHardDisks()), so that the size of snapshotIds
             * will be 1 in this case. The given condition is used to filter out
             * this legal situatinon and do not report an error. */

            if (it->snapshotIds.size() == 0)
            {
                return setError (VBOX_E_INVALID_OBJECT_STATE,
                    tr ("Hard disk '%ls' is attached to a virtual machine "
                        "with UUID {%RTuuid}. No differencing hard disks "
                        "based on it may be created until it is detached"),
                    m.locationFull.raw(), it->machineId.raw());
            }

            Assert (it->snapshotIds.size() == 1);
        }
    }

    ComObjPtr <Progress> progress;

    if (aProgress != NULL)
    {
        /* use the existing progress object... */
        progress = *aProgress;

        /* ...but create a new one if it is null */
        if (progress.isNull())
        {
            progress.createObject();
            rc = progress->init (mVirtualBox, static_cast<IHardDisk*> (this),
                BstrFmt (tr ("Creating differencing hard disk storage unit '%ls'"),
                         aTarget->m.locationFull.raw()),
                FALSE /* aCancelable */);
            CheckComRCReturnRC (rc);
        }
    }

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr <Task> task (new Task (this, progress, Task::CreateDiff));
    AssertComRCReturnRC (task->autoCaller.rc());

    task->setData (aTarget);

    /* register a task (it will deregister itself when done) */
    ++ mm.numCreateDiffTasks;
    Assert (mm.numCreateDiffTasks != 0); /* overflow? */

    if (aWait)
    {
        /* go to Creating state before starting the task */
        aTarget->m.state = MediaState_Creating;

        rc = task->runNow();
    }
    else
    {
        rc = task->startThread();
        CheckComRCReturnRC (rc);

        /* go to Creating state before leaving the lock */
        aTarget->m.state = MediaState_Creating;
    }

    /* task is now owned (or already deleted) by taskThread() so release it */
    task.release();

    if (aProgress != NULL)
    {
        /* return progress to the caller */
        *aProgress = progress;
    }

    return rc;
}

/**
 * Prepares this (source) hard disk, target hard disk and all intermediate hard
 * disks for the merge operation.
 *
 * This method is to be called prior to calling the #mergeTo() to perform
 * necessary consistency checks and place involved hard disks to appropriate
 * states. If #mergeTo() is not called or fails, the state modifications
 * performed by this method must be undone by #cancelMergeTo().
 *
 * Note that when @a aIgnoreAttachments is @c true then it's the caller's
 * responsibility to detach the source and all intermediate hard disks before
 * calling #mergeTo() (which will fail otherwise).
 *
 * See #mergeTo() for more information about merging.
 *
 * @param aTarget       Target hard disk.
 * @param aChain        Where to store the created merge chain.
 * @param aIgnoreAttachments    Don't check if the source or any intermediate
 *                              hard disk is attached to any VM.
 *
 * @note Locks treeLock() for reading. Locks this object, aTarget and all
 *       intermediate hard disks for writing.
 */
HRESULT HardDisk::prepareMergeTo(HardDisk *aTarget,
                                 MergeChain * &aChain,
                                 bool aIgnoreAttachments /*= false*/)
{
    AssertReturn (aTarget != NULL, E_FAIL);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    AutoCaller targetCaller (aTarget);
    AssertComRCReturnRC (targetCaller.rc());

    aChain = NULL;

    /* we walk the tree */
    AutoReadLock treeLock (this->treeLock());

    HRESULT rc = S_OK;

    /* detect the merge direction */
    bool forward;
    {
        HardDisk *parent = mParent;
        while (parent != NULL && parent != aTarget)
            parent = parent->mParent;
        if (parent == aTarget)
            forward = false;
        else
        {
            parent = aTarget->mParent;
            while (parent != NULL && parent != this)
                parent = parent->mParent;
            if (parent == this)
                forward = true;
            else
            {
                Bstr tgtLoc;
                {
                    AutoReadLock alock (this);
                    tgtLoc = aTarget->locationFull();
                }

                AutoReadLock alock (this);
                return setError (E_FAIL,
                    tr ("Hard disks '%ls' and '%ls' are unrelated"),
                    m.locationFull.raw(), tgtLoc.raw());
            }
        }
    }

    /* build the chain (will do necessary checks and state changes) */
    std::auto_ptr <MergeChain> chain (new MergeChain (forward,
                                                      aIgnoreAttachments));
    {
        HardDisk *last = forward ? aTarget : this;
        HardDisk *first = forward ? this : aTarget;

        for (;;)
        {
            if (last == aTarget)
                rc = chain->addTarget (last);
            else if (last == this)
                rc = chain->addSource (last);
            else
                rc = chain->addIntermediate (last);
            CheckComRCReturnRC (rc);

            if (last == first)
                break;

            last = last->mParent;
        }
    }

    aChain = chain.release();

    return S_OK;
}

/**
 * Merges this hard disk to the specified hard disk which must be either its
 * direct ancestor or descendant.
 *
 * Given this hard disk is SOURCE and the specified hard disk is TARGET, we will
 * get two varians of the merge operation:
 *
 *                forward merge
 *                ------------------------->
 *  [Extra] <- SOURCE <- Intermediate <- TARGET
 *  Any        Del       Del             LockWr
 *
 *
 *                            backward merge
 *                <-------------------------
 *             TARGET <- Intermediate <- SOURCE <- [Extra]
 *             LockWr    Del             Del       LockWr
 *
 * Each scheme shows the involved hard disks on the hard disk chain where
 * SOURCE and TARGET belong. Under each hard disk there is a state value which
 * the hard disk must have at a time of the mergeTo() call.
 *
 * The hard disks in the square braces may be absent (e.g. when the forward
 * operation takes place and SOURCE is the base hard disk, or when the backward
 * merge operation takes place and TARGET is the last child in the chain) but if
 * they present they are involved too as shown.
 *
 * Nor the source hard disk neither intermediate hard disks may be attached to
 * any VM directly or in the snapshot, otherwise this method will assert.
 *
 * The #prepareMergeTo() method must be called prior to this method to place all
 * involved to necessary states and perform other consistency checks.
 *
 * If @a aWait is @c true then this method will perform the operation on the
 * calling thread and will not return to the caller until the operation is
 * completed. When this method succeeds, all intermediate hard disk objects in
 * the chain will be uninitialized, the state of the target hard disk (and all
 * involved extra hard disks) will be restored and @a aChain will be deleted.
 * Note that this (source) hard disk is not uninitialized because of possible
 * AutoCaller instances held by the caller of this method on the current thread.
 * It's therefore the responsibility of the caller to call HardDisk::uninit()
 * after releasing all callers in this case!
 *
 * If @a aWait is @c false then this method will crea,te a thread to perform the
 * create operation asynchronously and will return immediately. If the operation
 * succeeds, the thread will uninitialize the source hard disk object and all
 * intermediate hard disk objects in the chain, reset the state of the target
 * hard disk (and all involved extra hard disks) and delete @a aChain. If the
 * operation fails, the thread will only reset the states of all involved hard
 * disks and delete @a aChain.
 *
 * When this method fails (regardless of the @a aWait mode), it is a caller's
 * responsiblity to undo state changes and delete @a aChain using
 * #cancelMergeTo().
 *
 * If @a aProgress is not NULL but the object it points to is @c null then a new
 * progress object will be created and assigned to @a *aProgress on success,
 * otherwise the existing progress object is used. If Progress is NULL, then no
 * progress object is created/used at all. Note that @a aProgress cannot be
 * NULL when @a aWait is @c false (this method will assert in this case).
 *
 * @param aChain        Merge chain created by #prepareMergeTo().
 * @param aProgress     Where to find/store a Progress object to track operation
 *                      completion.
 * @param aWait         @c true if this method should block instead of creating
 *                      an asynchronous thread.
 *
 * @note Locks the branch lock for writing. Locks the hard disks from the chain
 *       for writing.
 */
HRESULT HardDisk::mergeTo(MergeChain *aChain,
                          ComObjPtr <Progress> *aProgress,
                          bool aWait)
{
    AssertReturn (aChain != NULL, E_FAIL);
    AssertReturn (aProgress != NULL || aWait == true, E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    HRESULT rc = S_OK;

    ComObjPtr <Progress> progress;

    if (aProgress != NULL)
    {
        /* use the existing progress object... */
        progress = *aProgress;

        /* ...but create a new one if it is null */
        if (progress.isNull())
        {
            AutoReadLock alock (this);

            progress.createObject();
            rc = progress->init (mVirtualBox, static_cast<IHardDisk*>(this),
                BstrFmt (tr ("Merging hard disk '%s' to '%s'"),
                         name().raw(), aChain->target()->name().raw()),
                FALSE /* aCancelable */);
            CheckComRCReturnRC (rc);
        }
    }

    /* setup task object and thread to carry out the operation
     * asynchronously */

    std::auto_ptr <Task> task (new Task (this, progress, Task::Merge));
    AssertComRCReturnRC (task->autoCaller.rc());

    task->setData (aChain);

    /* Note: task owns aChain (will delete it when not needed) in all cases
     * except when @a aWait is @c true and runNow() fails -- in this case
     * aChain will be left away because cancelMergeTo() will be applied by the
     * caller on it as it is required in the documentation above */

    if (aWait)
    {
        rc = task->runNow();
    }
    else
    {
        rc = task->startThread();
        CheckComRCReturnRC (rc);
    }

    /* task is now owned (or already deleted) by taskThread() so release it */
    task.release();

    if (aProgress != NULL)
    {
        /* return progress to the caller */
        *aProgress = progress;
    }

    return rc;
}

/**
 * Undoes what #prepareMergeTo() did. Must be called if #mergeTo() is not called
 * or fails. Frees memory occupied by @a aChain.
 *
 * @param aChain        Merge chain created by #prepareMergeTo().
 *
 * @note Locks the hard disks from the chain for writing.
 */
void HardDisk::cancelMergeTo (MergeChain *aChain)
{
    AutoCaller autoCaller (this);
    AssertComRCReturnVoid (autoCaller.rc());

    AssertReturnVoid (aChain != NULL);

    /* the destructor will do the thing */
    delete aChain;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the value of m.location and calculates the value of m.locationFull.
 *
 * Reimplements MediumBase::setLocation() to specially treat non-FS-path
 * locations and to prepend the default hard disk folder if the given location
 * string does not contain any path information at all.
 *
 * Also, if the specified location is a file path that ends with '/' then the
 * file name part will be generated by this method automatically in the format
 * '{<uuid>}.<ext>' where <uuid> is a fresh UUID that this method will generate
 * and assign to this medium, and <ext> is the default extension for this
 * medium's storage format. Note that this procedure requires the media state to
 * be NotCreated and will return a faiulre otherwise.
 *
 * @param aLocation Location of the storage unit. If the locaiton is a FS-path,
 *                  then it can be relative to the VirtualBox home directory.
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT HardDisk::setLocation (CBSTR aLocation)
{
    /// @todo so far, we assert but later it makes sense to support null
    /// locations for hard disks that are not yet created fail to create a
    /// storage unit instead
    CheckComArgStrNotEmptyOrNull (aLocation);

    AutoCaller autoCaller (this);
    AssertComRCReturnRC (autoCaller.rc());

    /* formatObj may be null only when initializing from an existing path and
     * no format is known yet */
    AssertReturn ((!mm.format.isNull() && !mm.formatObj.isNull()) ||
                  (autoCaller.state() == InInit &&
                   m.state != MediaState_NotCreated && m.id.isEmpty() &&
                   mm.format.isNull() && mm.formatObj.isNull()),
                  E_FAIL);

    /* are we dealing with a new hard disk constructed using the existing
     * location? */
    bool isImport = mm.format.isNull();

    if (isImport ||
        (mm.formatObj->capabilities() & HardDiskFormatCapabilities_File))
    {
        Guid id;

        Utf8Str location (aLocation);

        if (m.state == MediaState_NotCreated)
        {
            /* must be a file (formatObj must be already known) */
            Assert (mm.formatObj->capabilities() & HardDiskFormatCapabilities_File);

            if (RTPathFilename (location) == NULL)
            {
                /* no file name is given (either an empty string or ends with a
                 * slash), generate a new UUID + file name if the state allows
                 * this */

                ComAssertMsgRet (!mm.formatObj->fileExtensions().empty(),
                                 ("Must be at least one extension if it is "
                                  "HardDiskFormatCapabilities_File\n"),
                                 E_FAIL);

                Bstr ext = mm.formatObj->fileExtensions().front();
                ComAssertMsgRet (!ext.isEmpty(),
                                 ("Default extension must not be empty\n"),
                                 E_FAIL);

                id.create();

                location = Utf8StrFmt ("%s{%RTuuid}.%ls",
                                       location.raw(), id.raw(), ext.raw());
            }
        }

        /* append the default folder if no path is given */
        if (!RTPathHavePath (location))
        {
            AutoReadLock propsLock (mVirtualBox->systemProperties());
            location = Utf8StrFmt ("%ls%c%s",
                mVirtualBox->systemProperties()->defaultHardDiskFolder().raw(),
                RTPATH_DELIMITER,
                location.raw());
        }

        /* get the full file name */
        Utf8Str locationFull;
        int vrc = mVirtualBox->calculateFullPath (location, locationFull);
        if (RT_FAILURE (vrc))
            return setError (VBOX_E_FILE_ERROR,
                tr ("Invalid hard disk storage file location '%s' (%Rrc)"),
                location.raw(), vrc);

        /* detect the backend from the storage unit if importing */
        if (isImport)
        {
            char *backendName = NULL;

            /* is it a file? */
            {
                RTFILE file;
                vrc = RTFileOpen (&file, locationFull, RTFILE_O_READ);
                if (RT_SUCCESS (vrc))
                    RTFileClose (file);
            }
            if (RT_SUCCESS (vrc))
            {
                vrc = VDGetFormat (locationFull, &backendName);
            }
            else if (vrc != VERR_FILE_NOT_FOUND && vrc != VERR_PATH_NOT_FOUND)
            {
                /* assume it's not a file, restore the original location */
                location = locationFull = aLocation;
                vrc = VDGetFormat (locationFull, &backendName);
            }

            if (RT_FAILURE (vrc))
                return setError (VBOX_E_IPRT_ERROR,
                    tr ("Could not get the storage format of the hard disk "
                        "'%s' (%Rrc)"), locationFull.raw(), vrc);

            ComAssertRet (backendName != NULL && *backendName != '\0', E_FAIL);

            HRESULT rc = setFormat (Bstr (backendName));
            RTStrFree (backendName);

            /* setFormat() must not fail since we've just used the backend so
             * the format object must be there */
            AssertComRCReturnRC (rc);
        }

        /* is it still a file? */
        if (mm.formatObj->capabilities() & HardDiskFormatCapabilities_File)
        {
            m.location = location;
            m.locationFull = locationFull;

            if (m.state == MediaState_NotCreated)
            {
                /* assign a new UUID (this UUID will be used when calling
                 * VDCreateBase/VDCreateDiff as a wanted UUID). Note that we
                 * also do that if we didn't generate it to make sure it is
                 * either generated by us or reset to null */
                unconst (m.id) = id;
            }
        }
        else
        {
            m.location = locationFull;
            m.locationFull = locationFull;
        }
    }
    else
    {
        m.location = aLocation;
        m.locationFull = aLocation;
    }

    return S_OK;
}

/**
 * Checks that the format ID is valid and sets it on success.
 *
 * Note that this method will caller-reference the format object on success!
 * This reference must be released somewhere to let the HardDiskFormat object be
 * uninitialized.
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT HardDisk::setFormat (CBSTR aFormat)
{
    /* get the format object first */
    {
        AutoReadLock propsLock (mVirtualBox->systemProperties());

        unconst (mm.formatObj)
            = mVirtualBox->systemProperties()->hardDiskFormat (aFormat);
        if (mm.formatObj.isNull())
            return setError (E_INVALIDARG,
                tr ("Invalid hard disk storage format '%ls'"), aFormat);

        /* reference the format permanently to prevent its unexpected
         * uninitialization */
        HRESULT rc = mm.formatObj->addCaller();
        AssertComRCReturnRC (rc);

        /* get properties (preinsert them as keys in the map). Note that the
         * map doesn't grow over the object life time since the set of
         * properties is meant to be constant. */

        Assert (mm.properties.empty());

        for (HardDiskFormat::PropertyList::const_iterator it =
                mm.formatObj->properties().begin();
             it != mm.formatObj->properties().end();
             ++ it)
        {
            mm.properties.insert (std::make_pair (it->name, Bstr::Null));
        }
    }

    unconst (mm.format) = aFormat;

    return S_OK;
}

/**
 * Queries information from the image file.
 *
 * As a result of this call, the accessibility state and data members such as
 * size and description will be updated with the current information.
 *
 * Reimplements MediumBase::queryInfo() to query hard disk information using the
 * VD backend interface.
 *
 * @note This method may block during a system I/O call that checks storage
 *       accessibility.
 *
 * @note Locks treeLock() for reading and writing (for new diff media checked
 *       for the first time). Locks mParent for reading. Locks this object for
 *       writing.
 */
HRESULT HardDisk::queryInfo()
{
    AutoWriteLock alock (this);

    AssertReturn (m.state == MediaState_Created ||
                  m.state == MediaState_Inaccessible ||
                  m.state == MediaState_LockedRead ||
                  m.state == MediaState_LockedWrite,
                  E_FAIL);

    HRESULT rc = S_OK;

    int vrc = VINF_SUCCESS;

    /* check if a blocking queryInfo() call is in progress on some other thread,
     * and wait for it to finish if so instead of querying data ourselves */
    if (m.queryInfoSem != NIL_RTSEMEVENTMULTI)
    {
        Assert (m.state == MediaState_LockedRead);

        ++ m.queryInfoCallers;
        alock.leave();

        vrc = RTSemEventMultiWait (m.queryInfoSem, RT_INDEFINITE_WAIT);

        alock.enter();
        -- m.queryInfoCallers;

        if (m.queryInfoCallers == 0)
        {
            /* last waiting caller deletes the semaphore */
            RTSemEventMultiDestroy (m.queryInfoSem);
            m.queryInfoSem = NIL_RTSEMEVENTMULTI;
        }

        AssertRC (vrc);

        return S_OK;
    }

    /* lazily create a semaphore for possible callers */
    vrc = RTSemEventMultiCreate (&m.queryInfoSem);
    ComAssertRCRet (vrc, E_FAIL);

    bool tempStateSet = false;
    if (m.state != MediaState_LockedRead &&
        m.state != MediaState_LockedWrite)
    {
        /* Cause other methods to prevent any modifications before leaving the
         * lock. Note that clients will never see this temporary state change
         * since any COMGETTER(State) is (or will be) blocked until we finish
         * and restore the actual state. */
        m.state = MediaState_LockedRead;
        tempStateSet = true;
    }

    /* leave the lock before a blocking operation */
    alock.leave();

    bool success = false;
    Utf8Str lastAccessError;

    try
    {
        Utf8Str location (m.locationFull);

        /* are we dealing with a new hard disk constructed using the existing
         * location? */
        bool isImport = m.id.isEmpty();

        PVBOXHDD hdd;
        vrc = VDCreate (mm.vdDiskIfaces, &hdd);
        ComAssertRCThrow (vrc, E_FAIL);

        try
        {
            unsigned flags = VD_OPEN_FLAGS_INFO;

            /* Note that we don't use VD_OPEN_FLAGS_READONLY when opening new
             * hard disks because that would prevent necessary modifications
             * when opening hard disks of some third-party formats for the first
             * time in VirtualBox (such as VMDK for which VDOpen() needs to
             * generate an UUID if it is missing) */
            if (!isImport)
                flags |= VD_OPEN_FLAGS_READONLY;

            vrc = VDOpen (hdd, Utf8Str (mm.format), location, flags,
                          mm.vdDiskIfaces);
            if (RT_FAILURE (vrc))
            {
                lastAccessError = Utf8StrFmt (
                    tr ("Could not open the hard disk '%ls'%s"),
                    m.locationFull.raw(), vdError (vrc).raw());
                throw S_OK;
            }

            if (mm.formatObj->capabilities() & HardDiskFormatCapabilities_Uuid)
            {
                /* check the UUID */
                RTUUID uuid;
                vrc = VDGetUuid (hdd, 0, &uuid);
                ComAssertRCThrow (vrc, E_FAIL);

                if (isImport)
                {
                    unconst (m.id) = uuid;
                }
                else
                {
                    Assert (!m.id.isEmpty());

                    if (m.id != uuid)
                    {
                        lastAccessError = Utf8StrFmt (
                            tr ("UUID {%RTuuid} of the hard disk '%ls' does "
                                "not match the value {%RTuuid} stored in the "
                                "media registry ('%ls')"),
                            &uuid, m.locationFull.raw(), m.id.raw(),
                            mVirtualBox->settingsFileName().raw());
                        throw S_OK;
                    }
                }
            }
            else
            {
                /* the backend does not support storing UUIDs within the
                 * underlying storage so use what we store in XML */

                /* generate an UUID for an imported UUID-less hard disk */
                if (isImport)
                    unconst (m.id).create();
            }

            /* check the type */
            VDIMAGETYPE type;
            vrc = VDGetImageType (hdd, 0, &type);
            ComAssertRCThrow (vrc, E_FAIL);

            if (type == VD_IMAGE_TYPE_DIFF)
            {
                RTUUID parentId;
                vrc = VDGetParentUuid (hdd, 0, &parentId);
                ComAssertRCThrow (vrc, E_FAIL);

                if (isImport)
                {
                    /* the parent must be known to us. Note that we freely
                     * call locking methods of mVirtualBox and parent from the
                     * write lock (breaking the {parent,child} lock order)
                     * because there may be no concurrent access to the just
                     * opened hard disk on ther threads yet (and init() will
                     * fail if this method reporst MediaState_Inaccessible) */

                    Guid id = parentId;
                    ComObjPtr<HardDisk> parent;
                    rc = mVirtualBox->findHardDisk(&id, NULL,
                                                   false /* aSetError */,
                                                   &parent);
                    if (FAILED (rc))
                    {
                        lastAccessError = Utf8StrFmt (
                            tr ("Parent hard disk with UUID {%RTuuid} of the "
                                "hard disk '%ls' is not found in the media "
                                "registry ('%ls')"),
                            &parentId, m.locationFull.raw(),
                            mVirtualBox->settingsFileName().raw());
                        throw S_OK;
                    }

                    /* deassociate from VirtualBox, associate with parent */

                    mVirtualBox->removeDependentChild (this);

                    /* we set mParent & children() */
                    AutoWriteLock treeLock (this->treeLock());

                    Assert (mParent.isNull());
                    mParent = parent;
                    mParent->addDependentChild (this);
                }
                else
                {
                    /* we access mParent */
                    AutoReadLock treeLock (this->treeLock());

                    /* check that parent UUIDs match. Note that there's no need
                     * for the parent's AutoCaller (our lifetime is bound to
                     * it) */

                    if (mParent.isNull())
                    {
                        lastAccessError = Utf8StrFmt (
                            tr ("Hard disk '%ls' is differencing but it is not "
                                "associated with any parent hard disk in the "
                                "media registry ('%ls')"),
                            m.locationFull.raw(),
                            mVirtualBox->settingsFileName().raw());
                        throw S_OK;
                    }

                    AutoReadLock parentLock (mParent);
                    if (mParent->state() != MediaState_Inaccessible &&
                        mParent->id() != parentId)
                    {
                        lastAccessError = Utf8StrFmt (
                            tr ("Parent UUID {%RTuuid} of the hard disk '%ls' "
                                "does not match UUID {%RTuuid} of its parent "
                                "hard disk stored in the media registry ('%ls')"),
                            &parentId, m.locationFull.raw(),
                            mParent->id().raw(),
                            mVirtualBox->settingsFileName().raw());
                        throw S_OK;
                    }

                    /// @todo NEWMEDIA what to do if the parent is not
                    /// accessible while the diff is? Probably, nothing. The
                    /// real code will detect the mismatch anyway.
                }
            }

            m.size = VDGetFileSize (hdd, 0);
            mm.logicalSize = VDGetSize (hdd, 0) / _1M;

            success = true;
        }
        catch (HRESULT aRC)
        {
            rc = aRC;
        }

        VDDestroy (hdd);

    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    alock.enter();

    if (success)
        m.lastAccessError.setNull();
    else
    {
        m.lastAccessError = lastAccessError;
        LogWarningFunc (("'%ls' is not accessible (error='%ls', "
                         "rc=%Rhrc, vrc=%Rrc)\n",
                         m.locationFull.raw(), m.lastAccessError.raw(),
                         rc, vrc));
    }

    /* inform other callers if there are any */
    if (m.queryInfoCallers > 0)
    {
        RTSemEventMultiSignal (m.queryInfoSem);
    }
    else
    {
        /* delete the semaphore ourselves */
        RTSemEventMultiDestroy (m.queryInfoSem);
        m.queryInfoSem = NIL_RTSEMEVENTMULTI;
    }

    if (tempStateSet)
    {
        /* Set the proper state according to the result of the check */
        if (success)
            m.state = MediaState_Created;
        else
            m.state = MediaState_Inaccessible;
    }
    else
    {
        /* we're locked, use a special field to store the result */
        m.accessibleInLock = success;
    }

    return rc;
}

/**
 * @note Called from this object's AutoMayUninitSpan and from under mVirtualBox
 *       write lock.
 *
 * @note Also reused by HardDisk::Reset().
 *
 * @note Locks treeLock() for reading.
 */
HRESULT HardDisk::canClose()
{
    /* we access children */
    AutoReadLock treeLock (this->treeLock());

    if (children().size() != 0)
        return setError (E_FAIL,
            tr ("Hard disk '%ls' has %d child hard disks"),
                children().size());

    return S_OK;
}

/**
 * @note Called from within this object's AutoWriteLock.
 */
HRESULT HardDisk::canAttach(const Guid &aMachineId,
                            const Guid &aSnapshotId)
{
    if (mm.numCreateDiffTasks > 0)
        return setError (E_FAIL,
            tr ("One or more differencing child hard disks are "
                "being created for the hard disk '%ls' (%u)"),
            m.locationFull.raw(), mm.numCreateDiffTasks);

    return S_OK;
}

/**
 * @note Called from within this object's AutoMayUninitSpan (or AutoCaller) and
 *       from under mVirtualBox write lock.
 *
 * @note Locks treeLock() for writing.
 */
HRESULT HardDisk::unregisterWithVirtualBox()
{
    /* Note that we need to de-associate ourselves from the parent to let
     * unregisterHardDisk() properly save the registry */

    /* we modify mParent and access children */
    AutoWriteLock treeLock (this->treeLock());

    const ComObjPtr<HardDisk, ComWeakRef> parent = mParent;

    AssertReturn (children().size() == 0, E_FAIL);

    if (!mParent.isNull())
    {
        /* deassociate from the parent, associate with VirtualBox */
        mVirtualBox->addDependentChild (this);
        mParent->removeDependentChild (this);
        mParent.setNull();
    }

    HRESULT rc = mVirtualBox->unregisterHardDisk(this);

    if (FAILED (rc))
    {
        if (!parent.isNull())
        {
            /* re-associate with the parent as we are still relatives in the
             * registry */
            mParent = parent;
            mParent->addDependentChild (this);
            mVirtualBox->removeDependentChild (this);
        }
    }

    return rc;
}

/**
 * Returns the last error message collected by the vdErrorCall callback and
 * resets it.
 *
 * The error message is returned prepended with a dot and a space, like this:
 * <code>
 *   ". <error_text> (%Rrc)"
 * </code>
 * to make it easily appendable to a more general error message. The @c %Rrc
 * format string is given @a aVRC as an argument.
 *
 * If there is no last error message collected by vdErrorCall or if it is a
 * null or empty string, then this function returns the following text:
 * <code>
 *   " (%Rrc)"
 * </code>
 *
 * @note Doesn't do any object locking; it is assumed that the caller makes sure
 *       the callback isn't called by more than one thread at a time.
 *
 * @param aVRC  VBox error code to use when no error message is provided.
 */
Utf8Str HardDisk::vdError (int aVRC)
{
    Utf8Str error;

    if (mm.vdError.isEmpty())
        error = Utf8StrFmt (" (%Rrc)", aVRC);
    else
        error = Utf8StrFmt (".\n%s", mm.vdError.raw());

    mm.vdError.setNull();

    return error;
}

/**
 * Error message callback.
 *
 * Puts the reported error message to the mm.vdError field.
 *
 * @note Doesn't do any object locking; it is assumed that the caller makes sure
 *       the callback isn't called by more than one thread at a time.
 *
 * @param   pvUser          The opaque data passed on container creation.
 * @param   rc              The VBox error code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   va              Error message arguments.
 */
/*static*/
DECLCALLBACK(void) HardDisk::vdErrorCall(void *pvUser, int rc, RT_SRC_POS_DECL,
                                         const char *pszFormat, va_list va)
{
    HardDisk *that = static_cast<HardDisk*>(pvUser);
    AssertReturnVoid (that != NULL);

    if (that->mm.vdError.isEmpty())
        that->mm.vdError =
            Utf8StrFmt ("%s (%Rrc)", Utf8StrFmtVA (pszFormat, va).raw(), rc);
    else
        that->mm.vdError =
            Utf8StrFmt ("%s.\n%s (%Rrc)", that->mm.vdError.raw(),
                        Utf8StrFmtVA (pszFormat, va).raw(), rc);
}

/**
 * PFNVMPROGRESS callback handler for Task operations.
 *
 * @param uPercent    Completetion precentage (0-100).
 * @param pvUser      Pointer to the Progress instance.
 */
/*static*/
DECLCALLBACK(int) HardDisk::vdProgressCall(PVM /* pVM */, unsigned uPercent,
                                           void *pvUser)
{
    HardDisk *that = static_cast<HardDisk*>(pvUser);
    AssertReturn (that != NULL, VERR_GENERAL_FAILURE);

    if (that->mm.vdProgress != NULL)
    {
        /* update the progress object, capping it at 99% as the final percent
         * is used for additional operations like setting the UUIDs and similar. */
        that->mm.vdProgress->notifyProgress (RT_MIN (uPercent, 99));
    }

    return VINF_SUCCESS;
}

/* static */
DECLCALLBACK(bool) HardDisk::vdConfigAreKeysValid (void *pvUser,
                                                    const char *pszzValid)
{
    HardDisk *that = static_cast<HardDisk*>(pvUser);
    AssertReturn (that != NULL, false);

    /* we always return true since the only keys we have are those found in
     * VDBACKENDINFO */
    return true;
}

/* static */
DECLCALLBACK(int) HardDisk::vdConfigQuerySize(void *pvUser, const char *pszName,
                                              size_t *pcbValue)
{
    AssertReturn (VALID_PTR (pcbValue), VERR_INVALID_POINTER);

    HardDisk *that = static_cast<HardDisk*>(pvUser);
    AssertReturn (that != NULL, VERR_GENERAL_FAILURE);

    Data::PropertyMap::const_iterator it =
        that->mm.properties.find (Bstr (pszName));
    if (it == that->mm.properties.end())
        return VERR_CFGM_VALUE_NOT_FOUND;

    /* we interpret null values as "no value" in HardDisk */
    if (it->second.isNull())
        return VERR_CFGM_VALUE_NOT_FOUND;

    *pcbValue = it->second.length() + 1 /* include terminator */;

    return VINF_SUCCESS;
}

/* static */
DECLCALLBACK(int) HardDisk::vdConfigQuery (void *pvUser, const char *pszName,
                                            char *pszValue, size_t cchValue)
{
    AssertReturn (VALID_PTR (pszValue), VERR_INVALID_POINTER);

    HardDisk *that = static_cast<HardDisk*>(pvUser);
    AssertReturn (that != NULL, VERR_GENERAL_FAILURE);

    Data::PropertyMap::const_iterator it =
        that->mm.properties.find (Bstr (pszName));
    if (it == that->mm.properties.end())
        return VERR_CFGM_VALUE_NOT_FOUND;

    Utf8Str value = it->second;
    if (value.length() >= cchValue)
        return VERR_CFGM_NOT_ENOUGH_SPACE;

    /* we interpret null values as "no value" in HardDisk */
    if (it->second.isNull())
        return VERR_CFGM_VALUE_NOT_FOUND;

    memcpy (pszValue, value, value.length() + 1);

    return VINF_SUCCESS;
}

/**
 * Thread function for time-consuming tasks.
 *
 * The Task structure passed to @a pvUser must be allocated using new and will
 * be freed by this method before it returns.
 *
 * @param pvUser    Pointer to the Task instance.
 */
/* static */
DECLCALLBACK(int) HardDisk::taskThread (RTTHREAD thread, void *pvUser)
{
    std::auto_ptr <Task> task (static_cast <Task *> (pvUser));
    AssertReturn (task.get(), VERR_GENERAL_FAILURE);

    bool isAsync = thread != NIL_RTTHREAD;

    HardDisk *that = task->that;

    /// @todo ugly hack, fix ComAssert... later
    #define setError that->setError

    /* Note: no need in AutoCaller because Task does that */

    LogFlowFuncEnter();
    LogFlowFunc (("{%p}: operation=%d\n", that, task->operation));

    HRESULT rc = S_OK;

    switch (task->operation)
    {
        ////////////////////////////////////////////////////////////////////////

        case Task::CreateDynamic:
        case Task::CreateFixed:
        {
            /* The lock is also used as a signal from the task initiator (which
             * releases it only after RTThreadCreate()) that we can start the job */
            AutoWriteLock thatLock (that);

            /* these parameters we need after creation */
            uint64_t size = 0, logicalSize = 0;

            /* The object may request a specific UUID (through a special form of
             * the setLocation() argumet). Otherwise we have to generate it */
            Guid id = that->m.id;
            bool generateUuid = id.isEmpty();
            if (generateUuid)
            {
                id.create();
                /* VirtualBox::registerHardDisk() will need UUID */
                unconst (that->m.id) = id;
            }

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                Utf8Str format (that->mm.format);
                Utf8Str location (that->m.locationFull);
                uint64_t capabilities = that->mm.formatObj->capabilities();

                /* unlock before the potentially lengthy operation */
                Assert (that->m.state == MediaState_Creating);
                thatLock.leave();

                try
                {
                    /* ensure the directory exists */
                    rc = VirtualBox::ensureFilePathExists (location);
                    CheckComRCThrowRC (rc);

                    PDMMEDIAGEOMETRY geo = { 0 }; /* auto-detect */

                    /* needed for vdProgressCallback */
                    that->mm.vdProgress = task->progress;

                    vrc = VDCreateBase (hdd, format, location,
                                        task->operation == Task::CreateDynamic ?
                                            VD_IMAGE_TYPE_NORMAL :
                                            VD_IMAGE_TYPE_FIXED,
                                        task->d.size * _1M,
                                        VD_IMAGE_FLAGS_NONE,
                                        NULL, &geo, &geo, id.raw(),
                                        VD_OPEN_FLAGS_NORMAL,
                                        NULL, that->mm.vdDiskIfaces);

                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not create the hard disk storage "
                                "unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                    size = VDGetFileSize (hdd, 0);
                    logicalSize = VDGetSize (hdd, 0) / _1M;
                }
                catch (HRESULT aRC) { rc = aRC; }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            if (SUCCEEDED (rc))
            {
                /* register with mVirtualBox as the last step and move to
                 * Created state only on success (leaving an orphan file is
                 * better than breaking media registry consistency) */
                rc = that->mVirtualBox->registerHardDisk(that);
            }

            thatLock.maybeEnter();

            if (SUCCEEDED (rc))
            {
                that->m.state = MediaState_Created;

                that->m.size = size;
                that->mm.logicalSize = logicalSize;
            }
            else
            {
                /* back to NotCreated on failiure */
                that->m.state = MediaState_NotCreated;

                /* reset UUID to prevent it from being reused next time */
                if (generateUuid)
                    unconst (that->m.id).clear();
            }

            break;
        }

        ////////////////////////////////////////////////////////////////////////

        case Task::CreateDiff:
        {
            ComObjPtr<HardDisk> &target = task->d.target;

            /* Lock both in {parent,child} order. The lock is also used as a
             * signal from the task initiator (which releases it only after
             * RTThreadCreate()) that we can start the job*/
            AutoMultiWriteLock2 thatLock (that, target);

            uint64_t size = 0, logicalSize = 0;

            /* The object may request a specific UUID (through a special form of
             * the setLocation() argument). Otherwise we have to generate it */
            Guid targetId = target->m.id;
            bool generateUuid = targetId.isEmpty();
            if (generateUuid)
            {
                targetId.create();
                /* VirtualBox::registerHardDisk() will need UUID */
                unconst (target->m.id) = targetId;
            }

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                Guid id = that->m.id;
                Utf8Str format (that->mm.format);
                Utf8Str location (that->m.locationFull);

                Utf8Str targetFormat (target->mm.format);
                Utf8Str targetLocation (target->m.locationFull);

                Assert (target->m.state == MediaState_Creating);

                /* Note: MediaState_LockedWrite is ok when taking an online
                 * snapshot */
                Assert (that->m.state == MediaState_LockedRead ||
                        that->m.state == MediaState_LockedWrite);

                /* unlock before the potentially lengthy operation */
                thatLock.leave();

                try
                {
                    vrc = VDOpen (hdd, format, location,
                                  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                                  that->mm.vdDiskIfaces);
                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not open the hard disk storage "
                                "unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                    /* ensure the target directory exists */
                    rc = VirtualBox::ensureFilePathExists (targetLocation);
                    CheckComRCThrowRC (rc);

                    /* needed for vdProgressCallback */
                    that->mm.vdProgress = task->progress;

                    vrc = VDCreateDiff (hdd, targetFormat, targetLocation,
                                        VD_IMAGE_FLAGS_NONE,
                                        NULL, targetId.raw(),
                                        id.raw(),
                                        VD_OPEN_FLAGS_NORMAL,
                                        target->mm.vdDiskIfaces,
                                        that->mm.vdDiskIfaces);

                    that->mm.vdProgress = NULL;

                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not create the differencing hard disk "
                                "storage unit '%s'%s"),
                            targetLocation.raw(), that->vdError (vrc).raw());
                    }

                    size = VDGetFileSize (hdd, 1);
                    logicalSize = VDGetSize (hdd, 1) / _1M;
                }
                catch (HRESULT aRC) { rc = aRC; }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            if (SUCCEEDED (rc))
            {
                /* we set mParent & children() (note that thatLock is released
                 * here), but lock VirtualBox first to follow the rule */
                AutoMultiWriteLock2 alock (that->mVirtualBox->lockHandle(),
                                           that->treeLock());

                Assert (target->mParent.isNull());

                /* associate the child with the parent and deassociate from
                 * VirtualBox */
                target->mParent = that;
                that->addDependentChild (target);
                target->mVirtualBox->removeDependentChild (target);

                /* diffs for immutable hard disks are auto-reset by default */
                target->mm.autoReset =
                    that->root()->mm.type == HardDiskType_Immutable ?
                    TRUE : FALSE;

                /* register with mVirtualBox as the last step and move to
                 * Created state only on success (leaving an orphan file is
                 * better than breaking media registry consistency) */
                rc = that->mVirtualBox->registerHardDisk (target);

                if (FAILED (rc))
                {
                    /* break the parent association on failure to register */
                    target->mVirtualBox->addDependentChild (target);
                    that->removeDependentChild (target);
                    target->mParent.setNull();
                }
            }

            thatLock.maybeEnter();

            if (SUCCEEDED (rc))
            {
                target->m.state = MediaState_Created;

                target->m.size = size;
                target->mm.logicalSize = logicalSize;
            }
            else
            {
                /* back to NotCreated on failiure */
                target->m.state = MediaState_NotCreated;

                target->mm.autoReset = FALSE;

                /* reset UUID to prevent it from being reused next time */
                if (generateUuid)
                    unconst (target->m.id).clear();
            }

            if (isAsync)
            {
                /* unlock ourselves when done (unless in MediaState_LockedWrite
                 * state because of taking the online snapshot*/
                if (that->m.state != MediaState_LockedWrite)
                {
                    HRESULT rc2 = that->UnlockRead (NULL);
                    AssertComRC (rc2);
                }
            }

            /* deregister the task registered in createDiffStorage() */
            Assert (that->mm.numCreateDiffTasks != 0);
            -- that->mm.numCreateDiffTasks;

            /* Note that in sync mode, it's the caller's responsibility to
             * unlock the hard disk */

            break;
        }

        ////////////////////////////////////////////////////////////////////////

        case Task::Merge:
        {
            /* The lock is also used as a signal from the task initiator (which
             * releases it only after RTThreadCreate()) that we can start the
             * job. We don't actually need the lock for anything else since the
             * object is protected by MediaState_Deleting and we don't modify
             * its sensitive fields below */
            {
                AutoWriteLock thatLock (that);
            }

            MergeChain *chain = task->d.chain.get();

#if 0
            LogFlow (("*** MERGE forward = %RTbool\n", chain->isForward()));
#endif

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                try
                {
                    /* open all hard disks in the chain (they are in the
                     * {parent,child} order in there. Note that we don't lock
                     * objects in this chain since they must be in states
                     * (Deleting and LockedWrite) that prevent from chaning
                     * their format and location fields from outside. */

                    for (MergeChain::const_iterator it = chain->begin();
                         it != chain->end(); ++ it)
                    {
                        /* complex sanity (sane complexity) */
                        Assert ((chain->isForward() &&
                                 ((*it != chain->back() &&
                                   (*it)->m.state == MediaState_Deleting) ||
                                  (*it == chain->back() &&
                                   (*it)->m.state == MediaState_LockedWrite))) ||
                                (!chain->isForward() &&
                                 ((*it != chain->front() &&
                                   (*it)->m.state == MediaState_Deleting) ||
                                  (*it == chain->front() &&
                                   (*it)->m.state == MediaState_LockedWrite))));

                        Assert (*it == chain->target() ||
                                (*it)->m.backRefs.size() == 0);

                        /* open the first image with VDOPEN_FLAGS_INFO because
                         * it's not necessarily the base one */
                        vrc = VDOpen (hdd, Utf8Str ((*it)->mm.format),
                                      Utf8Str ((*it)->m.locationFull),
                                      it == chain->begin() ?
                                          VD_OPEN_FLAGS_INFO : 0,
                                      (*it)->mm.vdDiskIfaces);
                        if (RT_FAILURE (vrc))
                            throw vrc;
#if 0
                        LogFlow (("*** MERGE disk = %ls\n",
                                  (*it)->m.locationFull.raw()));
#endif
                    }

                    /* needed for vdProgressCallback */
                    that->mm.vdProgress = task->progress;

                    unsigned start = chain->isForward() ?
                        0 : chain->size() - 1;
                    unsigned end = chain->isForward() ?
                        chain->size() - 1 : 0;
#if 0
                    LogFlow (("*** MERGE from %d to %d\n", start, end));
#endif
                    vrc = VDMerge (hdd, start, end, that->mm.vdDiskIfaces);

                    that->mm.vdProgress = NULL;

                    if (RT_FAILURE (vrc))
                        throw vrc;

                    /* update parent UUIDs */
                    /// @todo VDMerge should be taught to do so, including the
                    /// multiple children case
                    if (chain->isForward())
                    {
                        /* target's UUID needs to be updated (note that target
                         * is the only image in the container on success) */
                        vrc = VDSetParentUuid (hdd, 0, chain->parent()->m.id);
                        if (RT_FAILURE (vrc))
                            throw vrc;
                    }
                    else
                    {
                        /* we need to update UUIDs of all source's children
                         * which cannot be part of the container at once so
                         * add each one in there individually */
                        if (chain->children().size() > 0)
                        {
                            for (List::const_iterator it = chain->children().begin();
                                 it != chain->children().end(); ++ it)
                            {
                                /* VD_OPEN_FLAGS_INFO since UUID is wrong yet */
                                vrc = VDOpen (hdd, Utf8Str ((*it)->mm.format),
                                              Utf8Str ((*it)->m.locationFull),
                                              VD_OPEN_FLAGS_INFO,
                                              (*it)->mm.vdDiskIfaces);
                                if (RT_FAILURE (vrc))
                                    throw vrc;

                                vrc = VDSetParentUuid (hdd, 1,
                                                       chain->target()->m.id);
                                if (RT_FAILURE (vrc))
                                    throw vrc;

                                vrc = VDClose (hdd, false /* fDelete */);
                                if (RT_FAILURE (vrc))
                                    throw vrc;
                            }
                        }
                    }
                }
                catch (HRESULT aRC) { rc = aRC; }
                catch (int aVRC)
                {
                    throw setError (E_FAIL,
                        tr ("Could not merge the hard disk '%ls' to '%ls'%s"),
                        chain->source()->m.locationFull.raw(),
                        chain->target()->m.locationFull.raw(),
                        that->vdError (aVRC).raw());
                }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            HRESULT rc2;

            bool saveSettingsFailed = false;

            if (SUCCEEDED (rc))
            {
                /* all hard disks but the target were successfully deleted by
                 * VDMerge; reparent the last one and uninitialize deleted */

                /* we set mParent & children() (note that thatLock is released
                 * here), but lock VirtualBox first to follow the rule */
                AutoMultiWriteLock2 alock (that->mVirtualBox->lockHandle(),
                                           that->treeLock());

                HardDisk *source = chain->source();
                HardDisk *target = chain->target();

                if (chain->isForward())
                {
                    /* first, unregister the target since it may become a base
                     * hard disk which needs re-registration */
                    rc2 = target->mVirtualBox->
                        unregisterHardDisk (target, false /* aSaveSettings */);
                    AssertComRC (rc2);

                    /* then, reparent it and disconnect the deleted branch at
                     * both ends (chain->parent() is source's parent) */
                    target->mParent->removeDependentChild (target);
                    target->mParent = chain->parent();
                    if (!target->mParent.isNull())
                    {
                        target->mParent->addDependentChild (target);
                        target->mParent->removeDependentChild (source);
                        source->mParent.setNull();
                    }
                    else
                    {
                        target->mVirtualBox->addDependentChild (target);
                        target->mVirtualBox->removeDependentChild (source);
                    }

                    /* then, register again */
                    rc2 = target->mVirtualBox->
                        registerHardDisk (target, false /* aSaveSettings */);
                    AssertComRC (rc2);
                }
                else
                {
                    Assert (target->children().size() == 1);
                    HardDisk *targetChild = target->children().front();

                    /* disconnect the deleted branch at the elder end */
                    target->removeDependentChild (targetChild);
                    targetChild->mParent.setNull();

                    const List &children = chain->children();

                    /* reparent source's chidren and disconnect the deleted
                     * branch at the younger end m*/
                    if (children.size() > 0)
                    {
                        /* obey {parent,child} lock order */
                        AutoWriteLock sourceLock (source);

                        for (List::const_iterator it = children.begin();
                             it != children.end(); ++ it)
                        {
                            AutoWriteLock childLock (*it);

                            (*it)->mParent = target;
                            (*it)->mParent->addDependentChild (*it);
                            source->removeDependentChild (*it);
                        }
                    }
                }

                /* try to save the hard disk registry */
                rc = that->mVirtualBox->saveSettings();

                if (SUCCEEDED (rc))
                {
                    /* unregister and uninitialize all hard disks in the chain
                     * but the target */

                    for (MergeChain::iterator it = chain->begin();
                         it != chain->end();)
                    {
                        if (*it == chain->target())
                        {
                            ++ it;
                            continue;
                        }

                        rc2 = (*it)->mVirtualBox->
                            unregisterHardDisk(*it, false /* aSaveSettings */);
                        AssertComRC (rc2);

                        /* now, uninitialize the deleted hard disk (note that
                         * due to the Deleting state, uninit() will not touch
                         * the parent-child relationship so we need to
                         * uninitialize each disk individually) */

                        /* note that the operation initiator hard disk (which is
                         * normally also the source hard disk) is a special case
                         * -- there is one more caller added by Task to it which
                         * we must release. Also, if we are in sync mode, the
                         * caller may still hold an AutoCaller instance for it
                         * and therefore we cannot uninit() it (it's therefore
                         * the caller's responsibility) */
                        if (*it == that)
                            task->autoCaller.release();

                        /* release the caller added by MergeChain before
                         * uninit() */
                        (*it)->releaseCaller();

                        if (isAsync || *it != that)
                            (*it)->uninit();

                        /* delete (to prevent uninitialization in MergeChain
                         * dtor) and advance to the next item */
                        it = chain->erase (it);
                    }

                    /* Note that states of all other hard disks (target, parent,
                     * children) will be restored by the MergeChain dtor */
                }
                else
                {
                    /* too bad if we fail, but we'll need to rollback everything
                     * we did above to at least keep the HD tree in sync with
                     * the current registry on disk */

                    saveSettingsFailed = true;

                    /// @todo NEWMEDIA implement a proper undo

                    AssertFailed();
                }
            }

            if (FAILED (rc))
            {
                /* Here we come if either VDMerge() failed (in which case we
                 * assume that it tried to do everything to make a further
                 * retry possible -- e.g. not deleted intermediate hard disks
                 * and so on) or VirtualBox::saveSettings() failed (where we
                 * should have the original tree but with intermediate storage
                 * units deleted by VDMerge()). We have to only restore states
                 * (through the MergeChain dtor) unless we are run synchronously
                 * in which case it's the responsibility of the caller as stated
                 * in the mergeTo() docs. The latter also implies that we
                 * don't own the merge chain, so release it in this case. */

                if (!isAsync)
                    task->d.chain.release();

                NOREF (saveSettingsFailed);
            }

            break;
        }

        ////////////////////////////////////////////////////////////////////////

        case Task::Clone:
        {
            ComObjPtr<HardDisk> &target = task->d.target;

            /* Lock both in {parent,child} order. The lock is also used as a
             * signal from the task initiator (which releases it only after
             * RTThreadCreate()) that we can start the job*/
            AutoMultiWriteLock2 thatLock (that, target);

            uint64_t size = 0, logicalSize = 0;

            /* The object may request a specific UUID (through a special form of
             * the setLocation() argumet). Otherwise we have to generate it */
            Guid targetId = target->m.id;
            bool generateUuid = targetId.isEmpty();
            if (generateUuid)
            {
                targetId.create();
                /* VirtualBox::registerHardDisk() will need UUID */
                unconst (target->m.id) = targetId;
            }

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                Utf8Str format (that->mm.format);
                Utf8Str location (that->m.locationFull);

                Utf8Str targetFormat (target->mm.format);
                Utf8Str targetLocation (target->m.locationFull);

                Assert (target->m.state == MediaState_Creating);

                Assert (that->m.state == MediaState_LockedRead);

                /* unlock before the potentially lengthy operation */
                thatLock.leave();

                try
                {
                    vrc = VDOpen (hdd, format, location,
                                  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                                  that->mm.vdDiskIfaces);
                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not open the hard disk storage "
                                "unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                    /* ensure the target directory exists */
                    rc = VirtualBox::ensureFilePathExists (targetLocation);
                    CheckComRCThrowRC (rc);

                    /* needed for vdProgressCallback */
                    that->mm.vdProgress = task->progress;

                    PVBOXHDD targetHdd;
                    int vrc = VDCreate (that->mm.vdDiskIfaces, &targetHdd);
                    ComAssertRCThrow (vrc, E_FAIL);

                    vrc = VDCopy (hdd, 0, targetHdd, targetFormat,
                                  targetLocation, false, 0, targetId.raw(),
                                  NULL, target->mm.vdDiskIfaces,
                                  that->mm.vdDiskIfaces);

                    that->mm.vdProgress = NULL;

                    if (RT_FAILURE (vrc))
                    {
                        VDDestroy (targetHdd);

                        throw setError (E_FAIL,
                            tr ("Could not create the clone hard disk "
                                "'%s'%s"),
                            targetLocation.raw(), that->vdError (vrc).raw());
                    }

                    size = VDGetFileSize (targetHdd, 0);
                    logicalSize = VDGetSize (targetHdd, 0) / _1M;

                    VDDestroy (targetHdd);
                }
                catch (HRESULT aRC) { rc = aRC; }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            if (SUCCEEDED (rc))
            {
                /* we set mParent & children() (note that thatLock is released
                 * here), but lock VirtualBox first to follow the rule */
                AutoMultiWriteLock2 alock (that->mVirtualBox->lockHandle(),
                                           that->treeLock());

                Assert (target->mParent.isNull());

                if (!that->mParent.isNull())
                {
                    /* associate the clone with the original's parent and
                     * deassociate from VirtualBox */
                    target->mParent = that->mParent;
                    that->mParent->addDependentChild (target);
                    target->mVirtualBox->removeDependentChild (target);

                    /* register with mVirtualBox as the last step and move to
                     * Created state only on success (leaving an orphan file is
                     * better than breaking media registry consistency) */
                    rc = that->mVirtualBox->registerHardDisk(target);

                    if (FAILED (rc))
                    {
                        /* break the parent association on failure to register */
                        target->mVirtualBox->addDependentChild (target);
                        that->mParent->removeDependentChild (target);
                        target->mParent.setNull();
                    }
                }
                else
                {
                    /* just register  */
                    rc = that->mVirtualBox->registerHardDisk(target);
                }
            }

            thatLock.maybeEnter();

            if (SUCCEEDED (rc))
            {
                target->m.state = MediaState_Created;

                target->m.size = size;
                target->mm.logicalSize = logicalSize;
            }
            else
            {
                /* back to NotCreated on failiure */
                target->m.state = MediaState_NotCreated;

                /* reset UUID to prevent it from being reused next time */
                if (generateUuid)
                    unconst (target->m.id).clear();
            }

            if (isAsync)
            {
                /* unlock ourselves when done (unless in MediaState_LockedWrite
                 * state because of taking the online snapshot*/
                if (that->m.state != MediaState_LockedWrite)
                {
                    HRESULT rc2 = that->UnlockRead (NULL);
                    AssertComRC (rc2);
                }
            }

            /* Note that in sync mode, it's the caller's responsibility to
             * unlock the hard disk */

            break;
        }

        ////////////////////////////////////////////////////////////////////////

        case Task::Delete:
        {
            /* The lock is also used as a signal from the task initiator (which
             * releases it only after RTThreadCreate()) that we can start the job */
            AutoWriteLock thatLock (that);

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                Utf8Str format (that->mm.format);
                Utf8Str location (that->m.locationFull);

                /* unlock before the potentially lengthy operation */
                Assert (that->m.state == MediaState_Deleting);
                thatLock.leave();

                try
                {
                    vrc = VDOpen (hdd, format, location,
                                  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                                  that->mm.vdDiskIfaces);
                    if (RT_SUCCESS (vrc))
                        vrc = VDClose (hdd, true /* fDelete */);

                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not delete the hard disk storage "
                                "unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                }
                catch (HRESULT aRC) { rc = aRC; }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            thatLock.maybeEnter();

            /* go to the NotCreated state even on failure since the storage
             * may have been already partially deleted and cannot be used any
             * more. One will be able to manually re-open the storage if really
             * needed to re-register it. */
            that->m.state = MediaState_NotCreated;

            /* Reset UUID to prevent Create* from reusing it again */
            unconst (that->m.id).clear();

            break;
        }

        case Task::Reset:
        {
            /* The lock is also used as a signal from the task initiator (which
             * releases it only after RTThreadCreate()) that we can start the job */
            AutoWriteLock thatLock (that);

            /// @todo Below we use a pair of delete/create operations to reset
            /// the diff contents but the most efficient way will of course be
            /// to add a VDResetDiff() API call

            uint64_t size = 0, logicalSize = 0;

            try
            {
                PVBOXHDD hdd;
                int vrc = VDCreate (that->mm.vdDiskIfaces, &hdd);
                ComAssertRCThrow (vrc, E_FAIL);

                Guid id = that->m.id;
                Utf8Str format (that->mm.format);
                Utf8Str location (that->m.locationFull);

                Guid parentId = that->mParent->m.id;
                Utf8Str parentFormat (that->mParent->mm.format);
                Utf8Str parentLocation (that->mParent->m.locationFull);

                Assert (that->m.state == MediaState_LockedWrite);

                /* unlock before the potentially lengthy operation */
                thatLock.leave();

                try
                {
                    /* first, delete the storage unit */
                    vrc = VDOpen (hdd, format, location,
                                  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                                  that->mm.vdDiskIfaces);
                    if (RT_SUCCESS (vrc))
                        vrc = VDClose (hdd, true /* fDelete */);

                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not delete the hard disk storage "
                                "unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                    /* next, create it again */
                    vrc = VDOpen (hdd, parentFormat, parentLocation,
                                  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                                  that->mm.vdDiskIfaces);
                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not open the hard disk storage "
                                "unit '%s'%s"),
                            parentLocation.raw(), that->vdError (vrc).raw());
                    }

                    /* needed for vdProgressCallback */
                    that->mm.vdProgress = task->progress;

                    vrc = VDCreateDiff (hdd, format, location,
                                        VD_IMAGE_FLAGS_NONE,
                                        NULL, id.raw(),
                                        parentId.raw(),
                                        VD_OPEN_FLAGS_NORMAL,
                                        that->mm.vdDiskIfaces,
                                        that->mm.vdDiskIfaces);

                    that->mm.vdProgress = NULL;

                    if (RT_FAILURE (vrc))
                    {
                        throw setError (E_FAIL,
                            tr ("Could not create the differencing hard disk "
                                "storage unit '%s'%s"),
                            location.raw(), that->vdError (vrc).raw());
                    }

                    size = VDGetFileSize (hdd, 1);
                    logicalSize = VDGetSize (hdd, 1) / _1M;
                }
                catch (HRESULT aRC) { rc = aRC; }

                VDDestroy (hdd);
            }
            catch (HRESULT aRC) { rc = aRC; }

            thatLock.enter();

            that->m.size = size;
            that->mm.logicalSize = logicalSize;

            if (isAsync)
            {
                /* unlock ourselves when done */
                HRESULT rc2 = that->UnlockWrite (NULL);
                AssertComRC (rc2);
            }

            /* Note that in sync mode, it's the caller's responsibility to
             * unlock the hard disk */

            break;
        }

        default:
            AssertFailedReturn (VERR_GENERAL_FAILURE);
    }

    /* complete the progress if run asynchronously */
    if (isAsync)
    {
        if (!task->progress.isNull())
            task->progress->notifyComplete (rc);
    }
    else
    {
        task->rc = rc;
    }

    LogFlowFunc (("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return VINF_SUCCESS;

    /// @todo ugly hack, fix ComAssert... later
    #undef setError
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
