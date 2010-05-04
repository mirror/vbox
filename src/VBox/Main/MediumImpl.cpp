/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "MediumImpl.h"
#include "ProgressImpl.h"
#include "SystemPropertiesImpl.h"
#include "VirtualBoxImpl.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/com/array.h>
#include <VBox/com/SupportErrorInfo.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/file.h>
#include <iprt/tcp.h>

#include <VBox/VBoxHDD.h>

#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
//
// Medium data definition
//
////////////////////////////////////////////////////////////////////////////////

/** Describes how a machine refers to this image. */
struct BackRef
{
    /** Equality predicate for stdc++. */
    struct EqualsTo : public std::unary_function <BackRef, bool>
    {
        explicit EqualsTo(const Guid &aMachineId) : machineId(aMachineId) {}

        bool operator()(const argument_type &aThat) const
        {
            return aThat.machineId == machineId;
        }

        const Guid machineId;
    };

    typedef std::list<Guid> GuidList;

    BackRef(const Guid &aMachineId,
            const Guid &aSnapshotId = Guid::Empty)
        : machineId(aMachineId),
          fInCurState(aSnapshotId.isEmpty())
    {
        if (!aSnapshotId.isEmpty())
            llSnapshotIds.push_back(aSnapshotId);
    }

    Guid machineId;
    bool fInCurState : 1;
    GuidList llSnapshotIds;
};

typedef std::list<BackRef> BackRefList;

struct Medium::Data
{
    Data()
        : pVirtualBox(NULL),
          state(MediumState_NotCreated),
          size(0),
          readers(0),
          preLockState(MediumState_NotCreated),
          queryInfoSem(NIL_RTSEMEVENTMULTI),
          queryInfoRunning(false),
          type(MediumType_Normal),
          devType(DeviceType_HardDisk),
          logicalSize(0),
          hddOpenMode(OpenReadWrite),
          autoReset(false),
          setImageId(false),
          setParentId(false),
          hostDrive(FALSE),
          implicit(false),
          numCreateDiffTasks(0),
          vdDiskIfaces(NULL)
    {}

    /** weak VirtualBox parent */
    VirtualBox * const pVirtualBox;

    const Guid id;
    Utf8Str strDescription;
    MediumState_T state;
    Utf8Str strLocation;
    Utf8Str strLocationFull;
    uint64_t size;
    Utf8Str strLastAccessError;

    // pParent and llChildren are protected by VirtualBox::getMediaTreeLockHandle()
    ComObjPtr<Medium> pParent;
    MediaList llChildren;           // to add a child, just call push_back; to remove a child, call child->deparent() which does a lookup

    BackRefList backRefs;

    size_t readers;
    MediumState_T preLockState;

    RTSEMEVENTMULTI queryInfoSem;
    bool queryInfoRunning : 1;

    const Utf8Str strFormat;
    ComObjPtr<MediumFormat> formatObj;

    MediumType_T type;
    DeviceType_T devType;
    uint64_t logicalSize;   /*< In MBytes. */

    HDDOpenMode hddOpenMode;

    BOOL autoReset : 1;

    /** the following members are invalid after changing UUID on open */
    BOOL setImageId : 1;
    BOOL setParentId : 1;
    const Guid imageId;
    const Guid parentId;

    BOOL hostDrive : 1;

    typedef std::map <Bstr, Bstr> PropertyMap;
    PropertyMap properties;

    bool implicit : 1;

    uint32_t numCreateDiffTasks;

    Utf8Str vdError;        /*< Error remembered by the VD error callback. */

    VDINTERFACE vdIfError;
    VDINTERFACEERROR vdIfCallsError;

    VDINTERFACE vdIfConfig;
    VDINTERFACECONFIG vdIfCallsConfig;

    VDINTERFACE vdIfTcpNet;
    VDINTERFACETCPNET vdIfCallsTcpNet;

    PVDINTERFACE vdDiskIfaces;
};

////////////////////////////////////////////////////////////////////////////////
//
// Globals
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Medium::Task class for asynchronous operations.
 *
 * @note Instances of this class must be created using new() because the
 *       task thread function will delete them when the task is complete.
 *
 * @note The constructor of this class adds a caller on the managed Medium
 *       object which is automatically released upon destruction.
 */
class Medium::Task
{
public:
    Task(Medium *aMedium, Progress *aProgress)
        : mVDOperationIfaces(NULL),
          m_pfNeedsSaveSettings(NULL),
          mMedium(aMedium),
          mMediumCaller(aMedium),
          mThread(NIL_RTTHREAD),
          mProgress(aProgress)
    {
        AssertReturnVoidStmt(aMedium, mRC = E_FAIL);
        mRC = mMediumCaller.rc();
        if (FAILED(mRC))
            return;

        /* Set up a per-operation progress interface, can be used freely (for
         * binary operations you can use it either on the source or target). */
        mVDIfCallsProgress.cbSize = sizeof(VDINTERFACEPROGRESS);
        mVDIfCallsProgress.enmInterface = VDINTERFACETYPE_PROGRESS;
        mVDIfCallsProgress.pfnProgress = vdProgressCall;
        int vrc = VDInterfaceAdd(&mVDIfProgress,
                                "Medium::Task::vdInterfaceProgress",
                                VDINTERFACETYPE_PROGRESS,
                                &mVDIfCallsProgress,
                                mProgress,
                                &mVDOperationIfaces);
        AssertRC(vrc);
        if (RT_FAILURE(vrc))
            mRC = E_FAIL;
    }

    // Make all destructors virtual. Just in case.
    virtual ~Task()
    {}

    HRESULT rc() const { return mRC; }
    bool isOk() const { return SUCCEEDED(rc()); }

    static int fntMediumTask(RTTHREAD aThread, void *pvUser);

    bool isAsync() { return mThread != NIL_RTTHREAD; }

    PVDINTERFACE mVDOperationIfaces;

    // Whether the caller needs to call VirtualBox::saveSettings() after
    // the task function returns. Only used in synchronous (wait) mode;
    // otherwise the task will save the settings itself.
    bool *m_pfNeedsSaveSettings;

    const ComObjPtr<Medium> mMedium;
    AutoCaller mMediumCaller;

    friend HRESULT Medium::runNow(Medium::Task*, bool*);

protected:
    HRESULT mRC;
    RTTHREAD mThread;

private:
    virtual HRESULT handler() = 0;

    const ComObjPtr<Progress> mProgress;

    static DECLCALLBACK(int) vdProgressCall(void *pvUser, unsigned uPercent);

    VDINTERFACE mVDIfProgress;
    VDINTERFACEPROGRESS mVDIfCallsProgress;
};

class Medium::CreateBaseTask : public Medium::Task
{
public:
    CreateBaseTask(Medium *aMedium,
                   Progress *aProgress,
                   uint64_t aSize,
                   MediumVariant_T aVariant)
        : Medium::Task(aMedium, aProgress),
          mSize(aSize),
          mVariant(aVariant)
    {}

    uint64_t mSize;
    MediumVariant_T mVariant;

private:
    virtual HRESULT handler();
};

class Medium::CreateDiffTask : public Medium::Task
{
public:
    CreateDiffTask(Medium *aMedium,
                   Progress *aProgress,
                   Medium *aTarget,
                   MediumVariant_T aVariant,
                   MediumLockList *aMediumLockList,
                   bool fKeepMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mpMediumLockList(aMediumLockList),
          mTarget(aTarget),
          mVariant(aVariant),
          mTargetCaller(aTarget),
          mfKeepMediumLockList(fKeepMediumLockList)
    {
        AssertReturnVoidStmt(aTarget != NULL, mRC = E_FAIL);
        mRC = mTargetCaller.rc();
        if (FAILED(mRC))
            return;
    }

    ~CreateDiffTask()
    {
        if (!mfKeepMediumLockList && mpMediumLockList)
            delete mpMediumLockList;
    }

    MediumLockList *mpMediumLockList;

    const ComObjPtr<Medium> mTarget;
    MediumVariant_T mVariant;

private:
    virtual HRESULT handler();

    AutoCaller mTargetCaller;
    bool mfKeepMediumLockList;
};

class Medium::CloneTask : public Medium::Task
{
public:
    CloneTask(Medium *aMedium,
              Progress *aProgress,
              Medium *aTarget,
              MediumVariant_T aVariant,
              Medium *aParent,
              MediumLockList *aSourceMediumLockList,
              MediumLockList *aTargetMediumLockList,
              bool fKeepSourceMediumLockList = false,
              bool fKeepTargetMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mTarget(aTarget),
          mParent(aParent),
          mpSourceMediumLockList(aSourceMediumLockList),
          mpTargetMediumLockList(aTargetMediumLockList),
          mVariant(aVariant),
          mTargetCaller(aTarget),
          mParentCaller(aParent),
          mfKeepSourceMediumLockList(fKeepSourceMediumLockList),
          mfKeepTargetMediumLockList(fKeepTargetMediumLockList)
    {
        AssertReturnVoidStmt(aTarget != NULL, mRC = E_FAIL);
        mRC = mTargetCaller.rc();
        if (FAILED(mRC))
            return;
        /* aParent may be NULL */
        mRC = mParentCaller.rc();
        if (FAILED(mRC))
            return;
        AssertReturnVoidStmt(aSourceMediumLockList != NULL, mRC = E_FAIL);
        AssertReturnVoidStmt(aTargetMediumLockList != NULL, mRC = E_FAIL);
    }

    ~CloneTask()
    {
        if (!mfKeepSourceMediumLockList && mpSourceMediumLockList)
            delete mpSourceMediumLockList;
        if (!mfKeepTargetMediumLockList && mpTargetMediumLockList)
            delete mpTargetMediumLockList;
    }

    const ComObjPtr<Medium> mTarget;
    const ComObjPtr<Medium> mParent;
    MediumLockList *mpSourceMediumLockList;
    MediumLockList *mpTargetMediumLockList;
    MediumVariant_T mVariant;

private:
    virtual HRESULT handler();

    AutoCaller mTargetCaller;
    AutoCaller mParentCaller;
    bool mfKeepSourceMediumLockList;
    bool mfKeepTargetMediumLockList;
};

class Medium::CompactTask : public Medium::Task
{
public:
    CompactTask(Medium *aMedium,
                Progress *aProgress,
                MediumLockList *aMediumLockList,
                bool fKeepMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mpMediumLockList(aMediumLockList),
          mfKeepMediumLockList(fKeepMediumLockList)
    {
        AssertReturnVoidStmt(aMediumLockList != NULL, mRC = E_FAIL);
    }

    ~CompactTask()
    {
        if (!mfKeepMediumLockList && mpMediumLockList)
            delete mpMediumLockList;
    }

    MediumLockList *mpMediumLockList;

private:
    virtual HRESULT handler();

    bool mfKeepMediumLockList;
};

class Medium::ResetTask : public Medium::Task
{
public:
    ResetTask(Medium *aMedium,
              Progress *aProgress,
              MediumLockList *aMediumLockList,
              bool fKeepMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mpMediumLockList(aMediumLockList),
          mfKeepMediumLockList(fKeepMediumLockList)
    {}

    ~ResetTask()
    {
        if (!mfKeepMediumLockList && mpMediumLockList)
            delete mpMediumLockList;
    }

    MediumLockList *mpMediumLockList;

private:
    virtual HRESULT handler();

    bool mfKeepMediumLockList;
};

class Medium::DeleteTask : public Medium::Task
{
public:
    DeleteTask(Medium *aMedium,
               Progress *aProgress,
               MediumLockList *aMediumLockList,
               bool fKeepMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mpMediumLockList(aMediumLockList),
          mfKeepMediumLockList(fKeepMediumLockList)
    {}

    ~DeleteTask()
    {
        if (!mfKeepMediumLockList && mpMediumLockList)
            delete mpMediumLockList;
    }

    MediumLockList *mpMediumLockList;

private:
    virtual HRESULT handler();

    bool mfKeepMediumLockList;
};

class Medium::MergeTask : public Medium::Task
{
public:
    MergeTask(Medium *aMedium,
              Medium *aTarget,
              bool fMergeForward,
              Medium *aParentForTarget,
              const MediaList &aChildrenToReparent,
              Progress *aProgress,
              MediumLockList *aMediumLockList,
              bool fKeepMediumLockList = false)
        : Medium::Task(aMedium, aProgress),
          mTarget(aTarget),
          mfMergeForward(fMergeForward),
          mParentForTarget(aParentForTarget),
          mChildrenToReparent(aChildrenToReparent),
          mpMediumLockList(aMediumLockList),
          mTargetCaller(aTarget),
          mParentForTargetCaller(aParentForTarget),
          mfChildrenCaller(false),
          mfKeepMediumLockList(fKeepMediumLockList)
    {
        AssertReturnVoidStmt(aMediumLockList != NULL, mRC = E_FAIL);
        for (MediaList::const_iterator it = mChildrenToReparent.begin();
             it != mChildrenToReparent.end();
             ++it)
        {
            HRESULT rc2 = (*it)->addCaller();
            if (FAILED(rc2))
            {
                mRC = E_FAIL;
                for (MediaList::const_iterator it2 = mChildrenToReparent.begin();
                     it2 != it;
                     --it2)
                {
                    (*it2)->releaseCaller();
                }
                return;
            }
        }
        mfChildrenCaller = true;
    }

    ~MergeTask()
    {
        if (!mfKeepMediumLockList && mpMediumLockList)
            delete mpMediumLockList;
        if (mfChildrenCaller)
        {
            for (MediaList::const_iterator it = mChildrenToReparent.begin();
                 it != mChildrenToReparent.end();
                 ++it)
            {
                (*it)->releaseCaller();
            }
        }
    }

    const ComObjPtr<Medium> mTarget;
    bool mfMergeForward;
    /* When mChildrenToReparent is empty then mParentForTarget is non-null.
     * In other words: they are used in different cases. */
    const ComObjPtr<Medium> mParentForTarget;
    MediaList mChildrenToReparent;
    MediumLockList *mpMediumLockList;

private:
    virtual HRESULT handler();

    AutoCaller mTargetCaller;
    AutoCaller mParentForTargetCaller;
    bool mfChildrenCaller;
    bool mfKeepMediumLockList;
};

/**
 * Thread function for time-consuming medium tasks.
 *
 * @param pvUser    Pointer to the Medium::Task instance.
 */
/* static */
DECLCALLBACK(int) Medium::Task::fntMediumTask(RTTHREAD aThread, void *pvUser)
{
    LogFlowFuncEnter();
    AssertReturn(pvUser, (int)E_INVALIDARG);
    Medium::Task *pTask = static_cast<Medium::Task *>(pvUser);

    pTask->mThread = aThread;

    HRESULT rc = pTask->handler();

    /* complete the progress if run asynchronously */
    if (pTask->isAsync())
    {
        if (!pTask->mProgress.isNull())
            pTask->mProgress->notifyComplete(rc);
    }

    /* pTask is no longer needed, delete it. */
    delete pTask;

    LogFlowFunc(("rc=%Rhrc\n", rc));
    LogFlowFuncLeave();

    return (int)rc;
}

/**
 * PFNVDPROGRESS callback handler for Task operations.
 *
 * @param pvUser      Pointer to the Progress instance.
 * @param uPercent    Completetion precentage (0-100).
 */
/*static*/
DECLCALLBACK(int) Medium::Task::vdProgressCall(void *pvUser, unsigned uPercent)
{
    Progress *that = static_cast<Progress *>(pvUser);

    if (that != NULL)
    {
        /* update the progress object, capping it at 99% as the final percent
         * is used for additional operations like setting the UUIDs and similar. */
        HRESULT rc = that->SetCurrentOperationProgress(uPercent * 99 / 100);
        if (FAILED(rc))
        {
            if (rc == E_FAIL)
                return VERR_CANCELLED;
            else
                return VERR_INVALID_STATE;
        }
    }

    return VINF_SUCCESS;
}

/**
 * Implementation code for the "create base" task.
 */
HRESULT Medium::CreateBaseTask::handler()
{
    return mMedium->taskCreateBaseHandler(*this);
}

/**
 * Implementation code for the "create diff" task.
 */
HRESULT Medium::CreateDiffTask::handler()
{
    return mMedium->taskCreateDiffHandler(*this);
}

/**
 * Implementation code for the "clone" task.
 */
HRESULT Medium::CloneTask::handler()
{
    return mMedium->taskCloneHandler(*this);
}

/**
 * Implementation code for the "compact" task.
 */
HRESULT Medium::CompactTask::handler()
{
    return mMedium->taskCompactHandler(*this);
}

/**
 * Implementation code for the "reset" task.
 */
HRESULT Medium::ResetTask::handler()
{
    return mMedium->taskResetHandler(*this);
}

/**
 * Implementation code for the "delete" task.
 */
HRESULT Medium::DeleteTask::handler()
{
    return mMedium->taskDeleteHandler(*this);
}

/**
 * Implementation code for the "merge" task.
 */
HRESULT Medium::MergeTask::handler()
{
    return mMedium->taskMergeHandler(*this);
}


////////////////////////////////////////////////////////////////////////////////
//
// Medium constructor / destructor
//
////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(Medium)

HRESULT Medium::FinalConstruct()
{
    m = new Data;

    /* Initialize the callbacks of the VD error interface */
    m->vdIfCallsError.cbSize = sizeof(VDINTERFACEERROR);
    m->vdIfCallsError.enmInterface = VDINTERFACETYPE_ERROR;
    m->vdIfCallsError.pfnError = vdErrorCall;
    m->vdIfCallsError.pfnMessage = NULL;

    /* Initialize the callbacks of the VD config interface */
    m->vdIfCallsConfig.cbSize = sizeof(VDINTERFACECONFIG);
    m->vdIfCallsConfig.enmInterface = VDINTERFACETYPE_CONFIG;
    m->vdIfCallsConfig.pfnAreKeysValid = vdConfigAreKeysValid;
    m->vdIfCallsConfig.pfnQuerySize = vdConfigQuerySize;
    m->vdIfCallsConfig.pfnQuery = vdConfigQuery;

    /* Initialize the callbacks of the VD TCP interface (we always use the host
     * IP stack for now) */
    m->vdIfCallsTcpNet.cbSize = sizeof(VDINTERFACETCPNET);
    m->vdIfCallsTcpNet.enmInterface = VDINTERFACETYPE_TCPNET;
    m->vdIfCallsTcpNet.pfnClientConnect = RTTcpClientConnect;
    m->vdIfCallsTcpNet.pfnClientClose = RTTcpClientClose;
    m->vdIfCallsTcpNet.pfnSelectOne = RTTcpSelectOne;
    m->vdIfCallsTcpNet.pfnRead = RTTcpRead;
    m->vdIfCallsTcpNet.pfnWrite = RTTcpWrite;
    m->vdIfCallsTcpNet.pfnFlush = RTTcpFlush;
    m->vdIfCallsTcpNet.pfnGetLocalAddress = RTTcpGetLocalAddress;
    m->vdIfCallsTcpNet.pfnGetPeerAddress = RTTcpGetPeerAddress;

    /* Initialize the per-disk interface chain */
    int vrc;
    vrc = VDInterfaceAdd(&m->vdIfError,
                         "Medium::vdInterfaceError",
                         VDINTERFACETYPE_ERROR,
                         &m->vdIfCallsError, this, &m->vdDiskIfaces);
    AssertRCReturn(vrc, E_FAIL);

    vrc = VDInterfaceAdd(&m->vdIfConfig,
                         "Medium::vdInterfaceConfig",
                         VDINTERFACETYPE_CONFIG,
                         &m->vdIfCallsConfig, this, &m->vdDiskIfaces);
    AssertRCReturn(vrc, E_FAIL);

    vrc = VDInterfaceAdd(&m->vdIfTcpNet,
                         "Medium::vdInterfaceTcpNet",
                         VDINTERFACETYPE_TCPNET,
                         &m->vdIfCallsTcpNet, this, &m->vdDiskIfaces);
    AssertRCReturn(vrc, E_FAIL);

    vrc = RTSemEventMultiCreate(&m->queryInfoSem);
    AssertRCReturn(vrc, E_FAIL);
    vrc = RTSemEventMultiSignal(m->queryInfoSem);
    AssertRCReturn(vrc, E_FAIL);

    return S_OK;
}

void Medium::FinalRelease()
{
    uninit();

    delete m;
}

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
 * @param aLocation     Storage unit location.
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 */
HRESULT Medium::init(VirtualBox *aVirtualBox,
                     CBSTR aFormat,
                     CBSTR aLocation,
                     bool *pfNeedsSaveSettings)
{
    AssertReturn(aVirtualBox != NULL, E_FAIL);
    AssertReturn(aFormat != NULL && *aFormat != '\0', E_FAIL);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(m->pVirtualBox) = aVirtualBox;

    /* no storage yet */
    m->state = MediumState_NotCreated;

    /* cannot be a host drive */
    m->hostDrive = FALSE;

    /* No storage unit is created yet, no need to queryInfo() */

    rc = setFormat(aFormat);
    if (FAILED(rc)) return rc;

    if (m->formatObj->capabilities() & MediumFormatCapabilities_File)
    {
        rc = setLocation(aLocation);
        if (FAILED(rc)) return rc;
    }
    else
    {
        rc = setLocation(aLocation);
        if (FAILED(rc)) return rc;
    }

    if (!(m->formatObj->capabilities() & (   MediumFormatCapabilities_CreateFixed
                                           | MediumFormatCapabilities_CreateDynamic))
       )
    {
        /* storage for hard disks of this format can neither be explicitly
         * created by VirtualBox nor deleted, so we place the hard disk to
         * Created state here and also add it to the registry */
        m->state = MediumState_Created;
        unconst(m->id).create();

        AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        rc = m->pVirtualBox->registerHardDisk(this, pfNeedsSaveSettings);
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the medium object by opening the storage unit at the specified
 * location. The enOpenMode parameter defines whether the image will be opened
 * read/write or read-only.
 *
 * Note that the UUID, format and the parent of this medium will be
 * determined when reading the medium storage unit, unless new values are
 * specified by the parameters. If the detected or set parent is
 * not known to VirtualBox, then this method will fail.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aLocation     Storage unit location.
 * @param enOpenMode    Whether to open the image read/write or read-only.
 * @param aDeviceType   Device type of medium.
 * @param aSetImageId   Whether to set the image UUID or not.
 * @param aImageId      New image UUID if @aSetId is true. Empty string means
 *                      create a new UUID, and a zero UUID is invalid.
 * @param aSetParentId  Whether to set the parent UUID or not.
 * @param aParentId     New parent UUID if @aSetParentId is true. Empty string
 *                      means create a new UUID, and a zero UUID is valid.
 */
HRESULT Medium::init(VirtualBox *aVirtualBox,
                     CBSTR aLocation,
                     HDDOpenMode enOpenMode,
                     DeviceType_T aDeviceType,
                     BOOL aSetImageId,
                     const Guid &aImageId,
                     BOOL aSetParentId,
                     const Guid &aParentId)
{
    AssertReturn(aVirtualBox, E_INVALIDARG);
    AssertReturn(aLocation, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(m->pVirtualBox) = aVirtualBox;

    /* there must be a storage unit */
    m->state = MediumState_Created;

    /* remember device type for correct unregistering later */
    m->devType = aDeviceType;

    /* cannot be a host drive */
    m->hostDrive = FALSE;

    /* remember the open mode (defaults to ReadWrite) */
    m->hddOpenMode = enOpenMode;

    if (aDeviceType == DeviceType_HardDisk)
        rc = setLocation(aLocation);
    else
        rc = setLocation(aLocation, "RAW");
    if (FAILED(rc)) return rc;

    /* save the new uuid values, will be used by queryInfo() */
    m->setImageId = aSetImageId;
    unconst(m->imageId) = aImageId;
    m->setParentId = aSetParentId;
    unconst(m->parentId) = aParentId;

    /* get all the information about the medium from the storage unit */
    rc = queryInfo();

    if (SUCCEEDED(rc))
    {
        /* if the storage unit is not accessible, it's not acceptable for the
         * newly opened media so convert this into an error */
        if (m->state == MediumState_Inaccessible)
        {
            Assert(!m->strLastAccessError.isEmpty());
            rc = setError(E_FAIL, m->strLastAccessError.c_str());
        }
        else
        {
            AssertReturn(!m->id.isEmpty(), E_FAIL);

            /* storage format must be detected by queryInfo() if the medium is accessible */
            AssertReturn(!m->strFormat.isEmpty(), E_FAIL);
        }
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the medium object by loading its data from the given settings
 * node. In this mode, the image will always be opened read/write.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aParent       Parent medium disk or NULL for a root (base) medium.
 * @param aDeviceType   Device type of the medium.
 * @param aNode         Configuration settings.
 *
 * @note Locks VirtualBox for writing, the medium tree for writing.
 */
HRESULT Medium::init(VirtualBox *aVirtualBox,
                     Medium *aParent,
                     DeviceType_T aDeviceType,
                     const settings::Medium &data)
{
    using namespace settings;

    AssertReturn(aVirtualBox, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    HRESULT rc = S_OK;

    /* share VirtualBox and parent weakly */
    unconst(m->pVirtualBox) = aVirtualBox;

    /* register with VirtualBox/parent early, since uninit() will
     * unconditionally unregister on failure */
    if (aParent)
    {
        // differencing image: add to parent
        AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        m->pParent = aParent;
        aParent->m->llChildren.push_back(this);
    }

    /* see below why we don't call queryInfo() (and therefore treat the medium
     * as inaccessible for now */
    m->state = MediumState_Inaccessible;
    m->strLastAccessError = tr("Accessibility check was not yet performed");

    /* required */
    unconst(m->id) = data.uuid;

    /* assume not a host drive */
    m->hostDrive = FALSE;

    /* optional */
    m->strDescription = data.strDescription;

    /* required */
    if (aDeviceType == DeviceType_HardDisk)
    {
        AssertReturn(!data.strFormat.isEmpty(), E_FAIL);
        rc = setFormat(Bstr(data.strFormat));
        if (FAILED(rc)) return rc;
    }
    else
    {
        /// @todo handle host drive settings here as well?
        if (!data.strFormat.isEmpty())
            rc = setFormat(Bstr(data.strFormat));
        else
            rc = setFormat(Bstr("RAW"));
        if (FAILED(rc)) return rc;
    }

    /* optional, only for diffs, default is false;
     * we can only auto-reset diff images, so they
     * must not have a parent */
    if (aParent != NULL)
        m->autoReset = data.fAutoReset;
    else
        m->autoReset = false;

    /* properties (after setting the format as it populates the map). Note that
     * if some properties are not supported but preseint in the settings file,
     * they will still be read and accessible (for possible backward
     * compatibility; we can also clean them up from the XML upon next
     * XML format version change if we wish) */
    for (settings::PropertiesMap::const_iterator it = data.properties.begin();
         it != data.properties.end(); ++it)
    {
        const Utf8Str &name = it->first;
        const Utf8Str &value = it->second;
        m->properties[Bstr(name)] = Bstr(value);
    }

    /* required */
    rc = setLocation(data.strLocation);
    if (FAILED(rc)) return rc;

    if (aDeviceType == DeviceType_HardDisk)
    {
        /* type is only for base hard disks */
        if (m->pParent.isNull())
            m->type = data.hdType;
    }
    else
        m->type = MediumType_Writethrough;

    /* remember device type for correct unregistering later */
    m->devType = aDeviceType;

    LogFlowThisFunc(("m->strLocationFull='%s', m->strFormat=%s, m->id={%RTuuid}\n",
                     m->strLocationFull.raw(), m->strFormat.raw(), m->id.raw()));

    /* Don't call queryInfo() for registered media to prevent the calling
     * thread (i.e. the VirtualBox server startup thread) from an unexpected
     * freeze but mark it as initially inaccessible instead. The vital UUID,
     * location and format properties are read from the registry file above; to
     * get the actual state and the rest of the data, the user will have to call
     * COMGETTER(State). */

    AutoWriteLock treeLock(aVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    /* load all children */
    for (settings::MediaList::const_iterator it = data.llChildren.begin();
         it != data.llChildren.end();
         ++it)
    {
        const settings::Medium &med = *it;

        ComObjPtr<Medium> pHD;
        pHD.createObject();
        rc = pHD->init(aVirtualBox,
                       this,            // parent
                       aDeviceType,
                       med);              // child data
        if (FAILED(rc)) break;

        rc = m->pVirtualBox->registerHardDisk(pHD, NULL /*pfNeedsSaveSettings*/);
        if (FAILED(rc)) break;
    }

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    return rc;
}

/**
 * Initializes the medium object by providing the host drive information.
 * Not used for anything but the host floppy/host DVD case.
 *
 * @todo optimize all callers to avoid reconstructing objects with the same
 * information over and over again - in the typical case each VM referring to
 * a particular host drive has its own instance.
 *
 * @param aVirtualBox   VirtualBox object.
 * @param aDeviceType   Device type of the medium.
 * @param aLocation     Location of the host drive.
 * @param aDescription  Comment for this host drive.
 *
 * @note Locks VirtualBox lock for writing.
 */
HRESULT Medium::init(VirtualBox *aVirtualBox,
                     DeviceType_T aDeviceType,
                     CBSTR aLocation,
                     CBSTR aDescription)
{
    ComAssertRet(aDeviceType == DeviceType_DVD || aDeviceType == DeviceType_Floppy, E_INVALIDARG);
    ComAssertRet(aLocation, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(m->pVirtualBox) = aVirtualBox;

    /* fake up a UUID which is unique, but also reproducible */
    RTUUID uuid;
    RTUuidClear(&uuid);
    if (aDeviceType == DeviceType_DVD)
        memcpy(&uuid.au8[0], "DVD", 3);
    else
        memcpy(&uuid.au8[0], "FD", 2);
    /* use device name, adjusted to the end of uuid, shortened if necessary */
    Utf8Str loc(aLocation);
    size_t cbLocation = strlen(loc.raw());
    if (cbLocation > 12)
        memcpy(&uuid.au8[4], loc.raw() + (cbLocation - 12), 12);
    else
        memcpy(&uuid.au8[4 + 12 - cbLocation], loc.raw(), cbLocation);
    unconst(m->id) = uuid;

    m->type = MediumType_Writethrough;
    m->devType = aDeviceType;
    m->state = MediumState_Created;
    m->hostDrive = true;
    HRESULT rc = setFormat(Bstr("RAW"));
    if (FAILED(rc)) return rc;
    rc = setLocation(aLocation);
    if (FAILED(rc)) return rc;
    m->strDescription = aDescription;

/// @todo generate uuid (similarly to host network interface uuid) from location and device type

    autoInitSpan.setSucceeded();
    return S_OK;
}

/**
 * Uninitializes the instance.
 *
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 *
 * @note All children of this hard disk get uninitialized by calling their
 *       uninit() methods.
 *
 * @note Caller must hold the tree lock of the medium tree this medium is on.
 */
void Medium::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    if (!m->formatObj.isNull())
    {
        /* remove the caller reference we added in setFormat() */
        m->formatObj->releaseCaller();
        m->formatObj.setNull();
    }

    if (m->state == MediumState_Deleting)
    {
        /* we are being uninitialized after've been deleted by merge.
         * Reparenting has already been done so don't touch it here (we are
         * now orphans and removeDependentChild() will assert) */
        Assert(m->pParent.isNull());
    }
    else
    {
        MediaList::iterator it;
        for (it = m->llChildren.begin();
            it != m->llChildren.end();
            ++it)
        {
            Medium *pChild = *it;
            pChild->m->pParent.setNull();
            pChild->uninit();
        }
        m->llChildren.clear();          // this unsets all the ComPtrs and probably calls delete

        if (m->pParent)
        {
            // this is a differencing disk: then remove it from the parent's children list
            deparent();
        }
    }

    RTSemEventMultiSignal(m->queryInfoSem);
    RTSemEventMultiDestroy(m->queryInfoSem);
    m->queryInfoSem = NIL_RTSEMEVENTMULTI;

    unconst(m->pVirtualBox) = NULL;
}

/**
 * Internal helper that removes "this" from the list of children of its
 * parent. Used in uninit() and other places when reparenting is necessary.
 *
 * The caller must hold the hard disk tree lock!
 */
void Medium::deparent()
{
    MediaList &llParent = m->pParent->m->llChildren;
    for (MediaList::iterator it = llParent.begin();
         it != llParent.end();
         ++it)
    {
        Medium *pParentsChild = *it;
        if (this == pParentsChild)
        {
            llParent.erase(it);
            break;
        }
    }
    m->pParent.setNull();
}

/**
 * Internal helper that removes "this" from the list of children of its
 * parent. Used in uninit() and other places when reparenting is necessary.
 *
 * The caller must hold the hard disk tree lock!
 */
void Medium::setParent(const ComObjPtr<Medium> &pParent)
{
    m->pParent = pParent;
    if (pParent)
        pParent->m->llChildren.push_back(this);
}


////////////////////////////////////////////////////////////////////////////////
//
// IMedium public methods
//
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Medium::COMGETTER(Id)(BSTR *aId)
{
    CheckComArgOutPointerValid(aId);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->id.toUtf16().cloneTo(aId);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Description)(BSTR *aDescription)
{
    CheckComArgOutPointerValid(aDescription);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strDescription.cloneTo(aDescription);

    return S_OK;
}

STDMETHODIMP Medium::COMSETTER(Description)(IN_BSTR aDescription)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

//     AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /// @todo update m->description and save the global registry (and local
    /// registries of portable VMs referring to this medium), this will also
    /// require to add the mRegistered flag to data

    NOREF(aDescription);

    ReturnComNotImplemented();
}

STDMETHODIMP Medium::COMGETTER(State)(MediumState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    *aState = m->state;

    return S_OK;
}


STDMETHODIMP Medium::COMGETTER(Location)(BSTR *aLocation)
{
    CheckComArgOutPointerValid(aLocation);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strLocationFull.cloneTo(aLocation);

    return S_OK;
}

STDMETHODIMP Medium::COMSETTER(Location)(IN_BSTR aLocation)
{
    CheckComArgStrNotEmptyOrNull(aLocation);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /// @todo NEWMEDIA for file names, add the default extension if no extension
    /// is present (using the information from the VD backend which also implies
    /// that one more parameter should be passed to setLocation() requesting
    /// that functionality since it is only allwed when called from this method

    /// @todo NEWMEDIA rename the file and set m->location on success, then save
    /// the global registry (and local registries of portable VMs referring to
    /// this medium), this will also require to add the mRegistered flag to data

    ReturnComNotImplemented();
}

STDMETHODIMP Medium::COMGETTER(Name)(BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    getName().cloneTo(aName);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(DeviceType)(DeviceType_T *aDeviceType)
{
    CheckComArgOutPointerValid(aDeviceType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aDeviceType = m->devType;

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(HostDrive)(BOOL *aHostDrive)
{
    CheckComArgOutPointerValid(aHostDrive);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aHostDrive = m->hostDrive;

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Size)(ULONG64 *aSize)
{
    CheckComArgOutPointerValid(aSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSize = m->size;

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Format)(BSTR *aFormat)
{
    CheckComArgOutPointerValid(aFormat);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, m->strFormat is const */
    m->strFormat.cloneTo(aFormat);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(MediumFormat)(IMediumFormat **aMediumFormat)
{
    CheckComArgOutPointerValid(aMediumFormat);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* no need to lock, m->formatObj is const */
    m->formatObj.queryInterfaceTo(aMediumFormat);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Type)(MediumType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aType = m->type;

    return S_OK;
}

STDMETHODIMP Medium::COMSETTER(Type)(MediumType_T aType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // we access mParent and members
    AutoMultiWriteLock2 mlock(&m->pVirtualBox->getMediaTreeLockHandle(), this->lockHandle() COMMA_LOCKVAL_SRC_POS);

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    /** @todo implement this case later */
    CheckComArgExpr(aType, aType != MediumType_Shareable);

    if (m->type == aType)
    {
        /* Nothing to do */
        return S_OK;
    }

    /* cannot change the type of a differencing hard disk */
    if (m->pParent)
        return setError(E_FAIL,
                        tr("Cannot change the type of hard disk '%s' because it is a differencing hard disk"),
                        m->strLocationFull.raw());

    /* cannot change the type of a hard disk being in use by more than one VM */
    if (m->backRefs.size() > 1)
        return setError(E_FAIL,
                        tr("Cannot change the type of hard disk '%s' because it is attached to %d virtual machines"),
                        m->strLocationFull.raw(), m->backRefs.size());

    switch (aType)
    {
        case MediumType_Normal:
        case MediumType_Immutable:
        {
            /* normal can be easily converted to immutable and vice versa even
             * if they have children as long as they are not attached to any
             * machine themselves */
            break;
        }
        case MediumType_Writethrough:
        case MediumType_Shareable:
        {
            /* cannot change to writethrough or shareable if there are children */
            if (getChildren().size() != 0)
                return setError(E_FAIL,
                                tr("Cannot change type for hard disk '%s' since it has %d child hard disk(s)"),
                                m->strLocationFull.raw(), getChildren().size());
            break;
        }
        default:
            AssertFailedReturn(E_FAIL);
    }

    m->type = aType;

    // saveSettings needs vbox lock
    ComObjPtr<VirtualBox> pVirtualBox(m->pVirtualBox);
    mlock.leave();
    AutoWriteLock alock(m->pVirtualBox COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = pVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP Medium::COMGETTER(Parent)(IMedium **aParent)
{
    CheckComArgOutPointerValid(aParent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* we access mParent */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    m->pParent.queryInterfaceTo(aParent);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Children)(ComSafeArrayOut(IMedium *, aChildren))
{
    CheckComArgOutSafeArrayPointerValid(aChildren);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* we access children */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IMedium> children(this->getChildren());
    children.detachTo(ComSafeArrayOutArg(aChildren));

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(Base)(IMedium **aBase)
{
    CheckComArgOutPointerValid(aBase);

    /* base() will do callers/locking */

    getBase().queryInterfaceTo(aBase);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(ReadOnly)(BOOL *aReadOnly)
{
    CheckComArgOutPointerValid(aReadOnly);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* isRadOnly() will do locking */

    *aReadOnly = isReadOnly();

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(LogicalSize)(ULONG64 *aLogicalSize)
{
    CheckComArgOutPointerValid(aLogicalSize);

    {
        AutoCaller autoCaller(this);
        if (FAILED(autoCaller.rc())) return autoCaller.rc();

        /* we access mParent */
        AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (m->pParent.isNull())
        {
            *aLogicalSize = m->logicalSize;

            return S_OK;
        }
    }

    /* We assume that some backend may decide to return a meaningless value in
     * response to VDGetSize() for differencing hard disks and therefore
     * always ask the base hard disk ourselves. */

    /* base() will do callers/locking */

    return getBase()->COMGETTER(LogicalSize)(aLogicalSize);
}

STDMETHODIMP Medium::COMGETTER(AutoReset)(BOOL *aAutoReset)
{
    CheckComArgOutPointerValid(aAutoReset);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->pParent)
        *aAutoReset = FALSE;

    *aAutoReset = m->autoReset;

    return S_OK;
}

STDMETHODIMP Medium::COMSETTER(AutoReset)(BOOL aAutoReset)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock(m->pVirtualBox, this COMMA_LOCKVAL_SRC_POS);

    if (m->pParent.isNull())
        return setError(VBOX_E_NOT_SUPPORTED,
                        tr("Hard disk '%s' is not differencing"),
                        m->strLocationFull.raw());

    if (m->autoReset != aAutoReset)
    {
        m->autoReset = aAutoReset;

        return m->pVirtualBox->saveSettings();
    }

    return S_OK;
}
STDMETHODIMP Medium::COMGETTER(LastAccessError)(BSTR *aLastAccessError)
{
    CheckComArgOutPointerValid(aLastAccessError);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strLastAccessError.cloneTo(aLastAccessError);

    return S_OK;
}

STDMETHODIMP Medium::COMGETTER(MachineIds)(ComSafeArrayOut(BSTR,aMachineIds))
{
    CheckComArgOutSafeArrayPointerValid(aMachineIds);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> machineIds;

    if (m->backRefs.size() != 0)
    {
        machineIds.reset(m->backRefs.size());

        size_t i = 0;
        for (BackRefList::const_iterator it = m->backRefs.begin();
             it != m->backRefs.end(); ++it, ++i)
        {
             it->machineId.toUtf16().detachTo(&machineIds[i]);
        }
    }

    machineIds.detachTo(ComSafeArrayOutArg(aMachineIds));

    return S_OK;
}

STDMETHODIMP Medium::RefreshState(MediumState_T *aState)
{
    CheckComArgOutPointerValid(aState);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* queryInfo() locks this for writing. */
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
        case MediumState_LockedRead:
        {
            rc = queryInfo();
            break;
        }
        default:
            break;
    }

    *aState = m->state;

    return rc;
}

STDMETHODIMP Medium::GetSnapshotIds(IN_BSTR aMachineId,
                                    ComSafeArrayOut(BSTR, aSnapshotIds))
{
    CheckComArgExpr(aMachineId, Guid(aMachineId).isEmpty() == false);
    CheckComArgOutSafeArrayPointerValid(aSnapshotIds);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<BSTR> snapshotIds;

    Guid id(aMachineId);
    for (BackRefList::const_iterator it = m->backRefs.begin();
         it != m->backRefs.end(); ++it)
    {
        if (it->machineId == id)
        {
            size_t size = it->llSnapshotIds.size();

            /* if the medium is attached to the machine in the current state, we
             * return its ID as the first element of the array */
            if (it->fInCurState)
                ++size;

            if (size > 0)
            {
                snapshotIds.reset(size);

                size_t j = 0;
                if (it->fInCurState)
                    it->machineId.toUtf16().detachTo(&snapshotIds[j++]);

                for (BackRef::GuidList::const_iterator jt = it->llSnapshotIds.begin();
                     jt != it->llSnapshotIds.end();
                     ++jt, ++j)
                {
                     (*jt).toUtf16().detachTo(&snapshotIds[j]);
                }
            }

            break;
        }
    }

    snapshotIds.detachTo(ComSafeArrayOutArg(aSnapshotIds));

    return S_OK;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP Medium::LockRead(MediumState_T *aState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Wait for a concurrently running queryInfo() to complete */
    while (m->queryInfoRunning)
    {
        alock.leave();
        RTSemEventMultiWait(m->queryInfoSem, RT_INDEFINITE_WAIT);
        alock.enter();
    }

    /* return the current state before */
    if (aState)
        *aState = m->state;

    HRESULT rc = S_OK;

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
        case MediumState_LockedRead:
        {
            ++m->readers;

            ComAssertMsgBreak(m->readers != 0, ("Counter overflow"), rc = E_FAIL);

            /* Remember pre-lock state */
            if (m->state != MediumState_LockedRead)
                m->preLockState = m->state;

            LogFlowThisFunc(("Okay - prev state=%d readers=%d\n", m->state, m->readers));
            m->state = MediumState_LockedRead;

            break;
        }
        default:
        {
            LogFlowThisFunc(("Failing - state=%d\n", m->state));
            rc = setStateError();
            break;
        }
    }

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP Medium::UnlockRead(MediumState_T *aState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    switch (m->state)
    {
        case MediumState_LockedRead:
        {
            Assert(m->readers != 0);
            --m->readers;

            /* Reset the state after the last reader */
            if (m->readers == 0)
            {
                m->state = m->preLockState;
                /* There are cases where we inject the deleting state into
                 * a medium locked for reading. Make sure #unmarkForDeletion()
                 * gets the right state afterwards. */
                if (m->preLockState == MediumState_Deleting)
                    m->preLockState = MediumState_Created;
            }

            LogFlowThisFunc(("new state=%d\n", m->state));
            break;
        }
        default:
        {
            LogFlowThisFunc(("Failing - state=%d\n", m->state));
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Medium '%s' is not locked for reading"),
                          m->strLocationFull.raw());
            break;
        }
    }

    /* return the current state after */
    if (aState)
        *aState = m->state;

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP Medium::LockWrite(MediumState_T *aState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Wait for a concurrently running queryInfo() to complete */
    while (m->queryInfoRunning)
    {
        alock.leave();
        RTSemEventMultiWait(m->queryInfoSem, RT_INDEFINITE_WAIT);
        alock.enter();
    }

    /* return the current state before */
    if (aState)
        *aState = m->state;

    HRESULT rc = S_OK;

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
        {
            m->preLockState = m->state;

            LogFlowThisFunc(("Okay - prev state=%d locationFull=%s\n", m->state, getLocationFull().c_str()));
            m->state = MediumState_LockedWrite;
            break;
        }
        default:
        {
            LogFlowThisFunc(("Failing - state=%d locationFull=%s\n", m->state, getLocationFull().c_str()));
            rc = setStateError();
            break;
        }
    }

    return rc;
}

/**
 * @note @a aState may be NULL if the state value is not needed (only for
 *       in-process calls).
 */
STDMETHODIMP Medium::UnlockWrite(MediumState_T *aState)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    switch (m->state)
    {
        case MediumState_LockedWrite:
        {
            m->state = m->preLockState;
            /* There are cases where we inject the deleting state into
             * a medium locked for writing. Make sure #unmarkForDeletion()
             * gets the right state afterwards. */
            if (m->preLockState == MediumState_Deleting)
                m->preLockState = MediumState_Created;
            LogFlowThisFunc(("new state=%d locationFull=%s\n", m->state, getLocationFull().c_str()));
            break;
        }
        default:
        {
            LogFlowThisFunc(("Failing - state=%d locationFull=%s\n", m->state, getLocationFull().c_str()));
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Medium '%s' is not locked for writing"),
                          m->strLocationFull.raw());
            break;
        }
    }

    /* return the current state after */
    if (aState)
        *aState = m->state;

    return rc;
}

STDMETHODIMP Medium::Close()
{
    // we're accessing parent/child and backrefs, so lock the tree first, then ourselves
    AutoMultiWriteLock2 multilock(&m->pVirtualBox->getMediaTreeLockHandle(),
                                  this->lockHandle()
                                  COMMA_LOCKVAL_SRC_POS);

    bool wasCreated = true;
    bool fNeedsSaveSettings = false;

    switch (m->state)
    {
        case MediumState_NotCreated:
            wasCreated = false;
            break;
        case MediumState_Created:
        case MediumState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    if (m->backRefs.size() != 0)
        return setError(VBOX_E_OBJECT_IN_USE,
                        tr("Medium '%s' is attached to %d virtual machines"),
                        m->strLocationFull.raw(), m->backRefs.size());

    /* perform extra media-dependent close checks */
    HRESULT rc = canClose();
    if (FAILED(rc)) return rc;

    if (wasCreated)
    {
        /* remove from the list of known media before performing actual
         * uninitialization (to keep the media registry consistent on
         * failure to do so) */
        rc = unregisterWithVirtualBox(&fNeedsSaveSettings);
        if (FAILED(rc)) return rc;
    }

    // make a copy of VirtualBox pointer which gets nulled by uninit()
    ComObjPtr<VirtualBox> pVirtualBox(m->pVirtualBox);

    /* Keep the locks held until after uninit, as otherwise the consistency
     * of the medium tree cannot be guaranteed. */
    uninit();

    multilock.release();

    if (fNeedsSaveSettings)
    {
        AutoWriteLock vboxlock(pVirtualBox COMMA_LOCKVAL_SRC_POS);
        pVirtualBox->saveSettings();
    }

    return S_OK;
}

STDMETHODIMP Medium::GetProperty(IN_BSTR aName, BSTR *aValue)
{
    CheckComArgStrNotEmptyOrNull(aName);
    CheckComArgOutPointerValid(aValue);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Data::PropertyMap::const_iterator it = m->properties.find(Bstr(aName));
    if (it == m->properties.end())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Property '%ls' does not exist"), aName);

    it->second.cloneTo(aValue);

    return S_OK;
}

STDMETHODIMP Medium::SetProperty(IN_BSTR aName, IN_BSTR aValue)
{
    CheckComArgStrNotEmptyOrNull(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock(m->pVirtualBox, this COMMA_LOCKVAL_SRC_POS);

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
            break;
        default:
            return setStateError();
    }

    Data::PropertyMap::iterator it = m->properties.find(Bstr(aName));
    if (it == m->properties.end())
        return setError(VBOX_E_OBJECT_NOT_FOUND,
                        tr("Property '%ls' does not exist"),
                        aName);

    if (aValue && !*aValue)
        it->second = (const char *)NULL;
    else
        it->second = aValue;

    HRESULT rc = m->pVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP Medium::GetProperties(IN_BSTR aNames,
                                   ComSafeArrayOut(BSTR, aReturnNames),
                                   ComSafeArrayOut(BSTR, aReturnValues))
{
    CheckComArgOutSafeArrayPointerValid(aReturnNames);
    CheckComArgOutSafeArrayPointerValid(aReturnValues);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    /// @todo make use of aNames according to the documentation
    NOREF(aNames);

    com::SafeArray<BSTR> names(m->properties.size());
    com::SafeArray<BSTR> values(m->properties.size());
    size_t i = 0;

    for (Data::PropertyMap::const_iterator it = m->properties.begin();
         it != m->properties.end();
         ++it)
    {
        it->first.cloneTo(&names[i]);
        it->second.cloneTo(&values[i]);
        ++i;
    }

    names.detachTo(ComSafeArrayOutArg(aReturnNames));
    values.detachTo(ComSafeArrayOutArg(aReturnValues));

    return S_OK;
}

STDMETHODIMP Medium::SetProperties(ComSafeArrayIn(IN_BSTR, aNames),
                                   ComSafeArrayIn(IN_BSTR, aValues))
{
    CheckComArgSafeArrayNotNull(aNames);
    CheckComArgSafeArrayNotNull(aValues);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock(m->pVirtualBox, this COMMA_LOCKVAL_SRC_POS);

    com::SafeArray<IN_BSTR> names(ComSafeArrayInArg(aNames));
    com::SafeArray<IN_BSTR> values(ComSafeArrayInArg(aValues));

    /* first pass: validate names */
    for (size_t i = 0;
         i < names.size();
         ++i)
    {
        if (m->properties.find(Bstr(names[i])) == m->properties.end())
            return setError(VBOX_E_OBJECT_NOT_FOUND,
                            tr("Property '%ls' does not exist"), names[i]);
    }

    /* second pass: assign */
    for (size_t i = 0;
         i < names.size();
         ++i)
    {
        Data::PropertyMap::iterator it = m->properties.find(Bstr(names[i]));
        AssertReturn(it != m->properties.end(), E_FAIL);

        if (values[i] && !*values[i])
            it->second = (const char *)NULL;
        else
            it->second = values[i];
    }

    HRESULT rc = m->pVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP Medium::CreateBaseStorage(ULONG64 aLogicalSize,
                                       MediumVariant_T aVariant,
                                       IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    ComObjPtr <Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        aVariant = (MediumVariant_T)((unsigned)aVariant & (unsigned)~MediumVariant_Diff);
        if (    !(aVariant & MediumVariant_Fixed)
            &&  !(m->formatObj->capabilities() & MediumFormatCapabilities_CreateDynamic))
            throw setError(VBOX_E_NOT_SUPPORTED,
                           tr("Hard disk format '%s' does not support dynamic storage creation"),
                           m->strFormat.raw());
        if (    (aVariant & MediumVariant_Fixed)
            &&  !(m->formatObj->capabilities() & MediumFormatCapabilities_CreateDynamic))
            throw setError(VBOX_E_NOT_SUPPORTED,
                           tr("Hard disk format '%s' does not support fixed storage creation"),
                           m->strFormat.raw());

        if (m->state != MediumState_NotCreated)
            throw setStateError();

        pProgress.createObject();
        rc = pProgress->init(m->pVirtualBox,
                             static_cast<IMedium*>(this),
                             (aVariant & MediumVariant_Fixed)
                               ? BstrFmt(tr("Creating fixed hard disk storage unit '%s'"), m->strLocationFull.raw())
                               : BstrFmt(tr("Creating dynamic hard disk storage unit '%s'"), m->strLocationFull.raw()),
                             TRUE /* aCancelable */);
        if (FAILED(rc))
            throw rc;

        /* setup task object to carry out the operation asynchronously */
        pTask = new Medium::CreateBaseTask(this, pProgress, aLogicalSize,
                                           aVariant);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;

        m->state = MediumState_Creating;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        rc = startThread(pTask);

        if (SUCCEEDED(rc))
            pProgress.queryInterfaceTo(aProgress);
    }
    else if (pTask != NULL)
        delete pTask;

    return rc;
}

STDMETHODIMP Medium::DeleteStorage(IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr <Progress> pProgress;

    HRESULT rc = deleteStorage(&pProgress, false /* aWait */,
                               NULL /* pfNeedsSaveSettings */);
    if (SUCCEEDED(rc))
        pProgress.queryInterfaceTo(aProgress);

    return rc;
}

STDMETHODIMP Medium::CreateDiffStorage(IMedium *aTarget,
                                       MediumVariant_T aVariant,
                                       IProgress **aProgress)
{
    CheckComArgNotNull(aTarget);
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<Medium> diff = static_cast<Medium*>(aTarget);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->type == MediumType_Writethrough)
        return setError(E_FAIL,
                        tr("Hard disk '%s' is Writethrough"),
                        m->strLocationFull.raw());

    /* Apply the normal locking logic to the entire chain. */
    MediumLockList *pMediumLockList(new MediumLockList());
    HRESULT rc = diff->createMediumLockList(true, this, *pMediumLockList);
    if (FAILED(rc))
    {
        delete pMediumLockList;
        return rc;
    }

    ComObjPtr <Progress> pProgress;

    rc = createDiffStorage(diff, aVariant, pMediumLockList, &pProgress,
                           false /* aWait */, NULL /* pfNeedsSaveSettings*/);
    if (FAILED(rc))
        delete pMediumLockList;
    else
        pProgress.queryInterfaceTo(aProgress);

    return rc;
}

STDMETHODIMP Medium::MergeTo(IMedium *aTarget, IProgress **aProgress)
{
    CheckComArgNotNull(aTarget);
    CheckComArgOutPointerValid(aProgress);
    ComAssertRet(aTarget != this, E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<Medium> pTarget = static_cast<Medium*>(aTarget);

    bool fMergeForward = false;
    ComObjPtr<Medium> pParentForTarget;
    MediaList childrenToReparent;
    MediumLockList *pMediumLockList = NULL;

    HRESULT rc = S_OK;

    rc = prepareMergeTo(pTarget, NULL, NULL, true, fMergeForward,
                        pParentForTarget, childrenToReparent, pMediumLockList);
    if (FAILED(rc)) return rc;

    ComObjPtr <Progress> pProgress;

    rc = mergeTo(pTarget, fMergeForward, pParentForTarget, childrenToReparent,
                 pMediumLockList, &pProgress, false /* aWait */,
                 NULL /* pfNeedsSaveSettings */);
    if (FAILED(rc))
        cancelMergeTo(childrenToReparent, pMediumLockList);
    else
        pProgress.queryInterfaceTo(aProgress);

    return rc;
}

STDMETHODIMP Medium::CloneTo(IMedium *aTarget,
                             MediumVariant_T aVariant,
                             IMedium *aParent,
                             IProgress **aProgress)
{
    CheckComArgNotNull(aTarget);
    CheckComArgOutPointerValid(aProgress);
    ComAssertRet(aTarget != this, E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<Medium> pTarget = static_cast<Medium*>(aTarget);
    ComObjPtr<Medium> pParent;
    if (aParent)
        pParent = static_cast<Medium*>(aParent);

    HRESULT rc = S_OK;
    ComObjPtr<Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        // locking: we need the tree lock first because we access parent pointers
        AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        // and we need to write-lock the images involved
        AutoMultiWriteLock3 alock(this, pTarget, pParent COMMA_LOCKVAL_SRC_POS);

        if (    pTarget->m->state != MediumState_NotCreated
            &&  pTarget->m->state != MediumState_Created)
            throw pTarget->setStateError();

        /* Build the source lock list. */
        MediumLockList *pSourceMediumLockList(new MediumLockList());
        rc = createMediumLockList(false, NULL,
                                  *pSourceMediumLockList);
        if (FAILED(rc))
        {
            delete pSourceMediumLockList;
            throw rc;
        }

        /* Build the target lock list (including the to-be parent chain). */
        MediumLockList *pTargetMediumLockList(new MediumLockList());
        rc = pTarget->createMediumLockList(true, pParent,
                                           *pTargetMediumLockList);
        if (FAILED(rc))
        {
            delete pSourceMediumLockList;
            delete pTargetMediumLockList;
            throw rc;
        }

        rc = pSourceMediumLockList->Lock();
        if (FAILED(rc))
        {
            delete pSourceMediumLockList;
            delete pTargetMediumLockList;
            throw setError(rc,
                           tr("Failed to lock source media '%ls'"),
                           getLocationFull().raw());
        }
        rc = pTargetMediumLockList->Lock();
        if (FAILED(rc))
        {
            delete pSourceMediumLockList;
            delete pTargetMediumLockList;
            throw setError(rc,
                           tr("Failed to lock target media '%ls'"),
                           pTarget->getLocationFull().raw());
        }

        pProgress.createObject();
        rc = pProgress->init(m->pVirtualBox,
                             static_cast <IMedium *>(this),
                             BstrFmt(tr("Creating clone hard disk '%s'"), pTarget->m->strLocationFull.raw()),
                             TRUE /* aCancelable */);
        if (FAILED(rc))
        {
            delete pSourceMediumLockList;
            delete pTargetMediumLockList;
            throw rc;
        }

        /* setup task object to carry out the operation asynchronously */
        pTask = new Medium::CloneTask(this, pProgress, pTarget, aVariant,
                                      pParent, pSourceMediumLockList,
                                      pTargetMediumLockList);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;

        if (pTarget->m->state == MediumState_NotCreated)
            pTarget->m->state = MediumState_Creating;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        rc = startThread(pTask);

        if (SUCCEEDED(rc))
            pProgress.queryInterfaceTo(aProgress);
    }
    else if (pTask != NULL)
        delete pTask;

    return rc;
}

STDMETHODIMP Medium::Compact(IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    ComObjPtr <Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        /* We need to lock both the current object, and the tree lock (would
         * cause a lock order violation otherwise) for createMediumLockList. */
        AutoMultiWriteLock2 multilock(&m->pVirtualBox->getMediaTreeLockHandle(),
                                      this->lockHandle()
                                      COMMA_LOCKVAL_SRC_POS);

        /* Build the medium lock list. */
        MediumLockList *pMediumLockList(new MediumLockList());
        rc = createMediumLockList(true, NULL,
                                  *pMediumLockList);
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw rc;
        }

        rc = pMediumLockList->Lock();
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw setError(rc,
                           tr("Failed to lock media when compacting '%ls'"),
                           getLocationFull().raw());
        }

        pProgress.createObject();
        rc = pProgress->init(m->pVirtualBox,
                             static_cast <IMedium *>(this),
                             BstrFmt(tr("Compacting hard disk '%s'"), m->strLocationFull.raw()),
                             TRUE /* aCancelable */);
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw rc;
        }

        /* setup task object to carry out the operation asynchronously */
        pTask = new Medium::CompactTask(this, pProgress, pMediumLockList);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        rc = startThread(pTask);

        if (SUCCEEDED(rc))
            pProgress.queryInterfaceTo(aProgress);
    }
    else if (pTask != NULL)
        delete pTask;

    return rc;
}

STDMETHODIMP Medium::Resize(ULONG64 aLogicalSize, IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    NOREF(aLogicalSize);
    NOREF(aProgress);
    ReturnComNotImplemented();
}

STDMETHODIMP Medium::Reset(IProgress **aProgress)
{
    CheckComArgOutPointerValid(aProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    ComObjPtr <Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        /* canClose() needs the tree lock */
        AutoMultiWriteLock2 multilock(&m->pVirtualBox->getMediaTreeLockHandle(),
                                      this->lockHandle()
                                      COMMA_LOCKVAL_SRC_POS);

        LogFlowThisFunc(("ENTER for medium %s\n", m->strLocationFull.c_str()));

        if (m->pParent.isNull())
            throw setError(VBOX_E_NOT_SUPPORTED,
                           tr("Hard disk '%s' is not differencing"),
                           m->strLocationFull.raw());

        rc = canClose();
        if (FAILED(rc))
            throw rc;

        /* Build the medium lock list. */
        MediumLockList *pMediumLockList(new MediumLockList());
        rc = createMediumLockList(true, NULL,
                                  *pMediumLockList);
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw rc;
        }

        rc = pMediumLockList->Lock();
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw setError(rc,
                           tr("Failed to lock media when resetting '%ls'"),
                           getLocationFull().raw());
        }

        pProgress.createObject();
        rc = pProgress->init(m->pVirtualBox,
                             static_cast<IMedium*>(this),
                             BstrFmt(tr("Resetting differencing hard disk '%s'"), m->strLocationFull.raw()),
                             FALSE /* aCancelable */);
        if (FAILED(rc))
            throw rc;

        /* setup task object to carry out the operation asynchronously */
        pTask = new Medium::ResetTask(this, pProgress, pMediumLockList);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        rc = startThread(pTask);

        if (SUCCEEDED(rc))
            pProgress.queryInterfaceTo(aProgress);
    }
    else
    {
        /* Note: on success, the task will unlock this */
        {
            AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
            HRESULT rc2 = UnlockWrite(NULL);
            AssertComRC(rc2);
        }
        if (pTask != NULL)
            delete pTask;
    }

    LogFlowThisFunc(("LEAVE, rc=%Rhrc\n", rc));

    return rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Medium internal methods
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Internal method to return the medium's parent medium. Must have caller + locking!
 * @return
 */
const ComObjPtr<Medium>& Medium::getParent() const
{
    return m->pParent;
}

/**
 * Internal method to return the medium's list of child media. Must have caller + locking!
 * @return
 */
const MediaList& Medium::getChildren() const
{
    return m->llChildren;
}

/**
 * Internal method to return the medium's GUID. Must have caller + locking!
 * @return
 */
const Guid& Medium::getId() const
{
    return m->id;
}

/**
 * Internal method to return the medium's GUID. Must have caller + locking!
 * @return
 */
MediumState_T Medium::getState() const
{
    return m->state;
}

/**
 * Internal method to return the medium's location. Must have caller + locking!
 * @return
 */
const Utf8Str& Medium::getLocation() const
{
    return m->strLocation;
}

/**
 * Internal method to return the medium's full location. Must have caller + locking!
 * @return
 */
const Utf8Str& Medium::getLocationFull() const
{
    return m->strLocationFull;
}

/**
 * Internal method to return the medium's format string. Must have caller + locking!
 * @return
 */
const Utf8Str& Medium::getFormat() const
{
    return m->strFormat;
}

/**
 * Internal method to return the medium's format object. Must have caller + locking!
 * @return
 */
const ComObjPtr<MediumFormat> & Medium::getMediumFormat() const
{
    return m->formatObj;
}

/**
 * Internal method to return the medium's size. Must have caller + locking!
 * @return
 */
uint64_t Medium::getSize() const
{
    return m->size;
}

/**
 * Adds the given machine and optionally the snapshot to the list of the objects
 * this image is attached to.
 *
 * @param aMachineId    Machine ID.
 * @param aSnapshotId   Snapshot ID; when non-empty, adds a snapshot attachment.
 */
HRESULT Medium::attachTo(const Guid &aMachineId,
                         const Guid &aSnapshotId /*= Guid::Empty*/)
{
    AssertReturn(!aMachineId.isEmpty(), E_FAIL);

    LogFlowThisFunc(("ENTER, aMachineId: {%RTuuid}, aSnapshotId: {%RTuuid}\n", aMachineId.raw(), aSnapshotId.raw()));

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
        case MediumState_LockedRead:
        case MediumState_LockedWrite:
            break;

        default:
            return setStateError();
    }

    if (m->numCreateDiffTasks > 0)
        return setError(E_FAIL,
                        tr("Cannot attach hard disk '%s' {%RTuuid}: %u differencing child hard disk(s) are being created"),
                        m->strLocationFull.raw(),
                        m->id.raw(),
                        m->numCreateDiffTasks);

    BackRefList::iterator it = std::find_if(m->backRefs.begin(),
                                            m->backRefs.end(),
                                            BackRef::EqualsTo(aMachineId));
    if (it == m->backRefs.end())
    {
        BackRef ref(aMachineId, aSnapshotId);
        m->backRefs.push_back(ref);

        return S_OK;
    }

    // if the caller has not supplied a snapshot ID, then we're attaching
    // to a machine a medium which represents the machine's current state,
    // so set the flag
    if (aSnapshotId.isEmpty())
    {
        /* sanity: no duplicate attachments */
        AssertReturn(!it->fInCurState, E_FAIL);
        it->fInCurState = true;

        return S_OK;
    }

    // otherwise: a snapshot medium is being attached

    /* sanity: no duplicate attachments */
    for (BackRef::GuidList::const_iterator jt = it->llSnapshotIds.begin();
         jt != it->llSnapshotIds.end();
         ++jt)
    {
        const Guid &idOldSnapshot = *jt;

        if (idOldSnapshot == aSnapshotId)
        {
#ifdef DEBUG
            dumpBackRefs();
#endif
            return setError(E_FAIL,
                            tr("Cannot attach medium '%s' {%RTuuid} from snapshot '%RTuuid': medium is already in use by this snapshot!"),
                            m->strLocationFull.raw(),
                            m->id.raw(),
                            aSnapshotId.raw(),
                            idOldSnapshot.raw());
        }
    }

    it->llSnapshotIds.push_back(aSnapshotId);
    it->fInCurState = false;

    LogFlowThisFuncLeave();

    return S_OK;
}

/**
 * Removes the given machine and optionally the snapshot from the list of the
 * objects this image is attached to.
 *
 * @param aMachineId    Machine ID.
 * @param aSnapshotId   Snapshot ID; when non-empty, removes the snapshot
 *                      attachment.
 */
HRESULT Medium::detachFrom(const Guid &aMachineId,
                           const Guid &aSnapshotId /*= Guid::Empty*/)
{
    AssertReturn(!aMachineId.isEmpty(), E_FAIL);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    BackRefList::iterator it =
        std::find_if(m->backRefs.begin(), m->backRefs.end(),
                     BackRef::EqualsTo(aMachineId));
    AssertReturn(it != m->backRefs.end(), E_FAIL);

    if (aSnapshotId.isEmpty())
    {
        /* remove the current state attachment */
        it->fInCurState = false;
    }
    else
    {
        /* remove the snapshot attachment */
        BackRef::GuidList::iterator jt =
            std::find(it->llSnapshotIds.begin(), it->llSnapshotIds.end(), aSnapshotId);

        AssertReturn(jt != it->llSnapshotIds.end(), E_FAIL);
        it->llSnapshotIds.erase(jt);
    }

    /* if the backref becomes empty, remove it */
    if (it->fInCurState == false && it->llSnapshotIds.size() == 0)
        m->backRefs.erase(it);

    return S_OK;
}

/**
 * Internal method to return the medium's list of backrefs. Must have caller + locking!
 * @return
 */
const Guid* Medium::getFirstMachineBackrefId() const
{
    if (!m->backRefs.size())
        return NULL;

    return &m->backRefs.front().machineId;
}

const Guid* Medium::getFirstMachineBackrefSnapshotId() const
{
    if (!m->backRefs.size())
        return NULL;

    const BackRef &ref = m->backRefs.front();
    if (!ref.llSnapshotIds.size())
        return NULL;

    return &ref.llSnapshotIds.front();
}

#ifdef DEBUG
/**
 * Debugging helper that gets called after VirtualBox initialization that writes all
 * machine backreferences to the debug log.
 */
void Medium::dumpBackRefs()
{
    AutoCaller autoCaller(this);
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("Dumping backrefs for medium '%s':\n", m->strLocationFull.raw()));

    for (BackRefList::iterator it2 = m->backRefs.begin();
         it2 != m->backRefs.end();
         ++it2)
    {
        const BackRef &ref = *it2;
        LogFlowThisFunc(("  Backref from machine {%RTuuid} (fInCurState: %d)\n", ref.machineId.raw(), ref.fInCurState));

        for (BackRef::GuidList::const_iterator jt2 = it2->llSnapshotIds.begin();
             jt2 != it2->llSnapshotIds.end();
             ++jt2)
        {
            const Guid &id = *jt2;
            LogFlowThisFunc(("  Backref from snapshot {%RTuuid}\n", id.raw()));
        }
    }
}
#endif

/**
 * Checks if the given change of \a aOldPath to \a aNewPath affects the location
 * of this media and updates it if necessary to reflect the new location.
 *
 * @param aOldPath  Old path (full).
 * @param aNewPath  New path (full).
 *
 * @note Locks this object for writing.
 */
HRESULT Medium::updatePath(const char *aOldPath, const char *aNewPath)
{
    AssertReturn(aOldPath, E_FAIL);
    AssertReturn(aNewPath, E_FAIL);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    LogFlowThisFunc(("locationFull.before='%s'\n", m->strLocationFull.raw()));

    const char *pcszMediumPath = m->strLocationFull.c_str();

    if (RTPathStartsWith(pcszMediumPath, aOldPath))
    {
        Utf8Str newPath = Utf8StrFmt("%s%s",
                                     aNewPath,
                                     pcszMediumPath + strlen(aOldPath));
        Utf8Str path = newPath;
        m->pVirtualBox->calculateRelativePath(path, path);
        unconst(m->strLocationFull) = newPath;
        unconst(m->strLocation) = path;

        LogFlowThisFunc(("locationFull.after='%s'\n", m->strLocationFull.raw()));
    }

    return S_OK;
}

/**
 * Checks if the given change of \a aOldPath to \a aNewPath affects the location
 * of this hard disk or any its child and updates the paths if necessary to
 * reflect the new location.
 *
 * @param aOldPath  Old path (full).
 * @param aNewPath  New path (full).
 *
 * @note Locks the medium tree for reading, this object and all children for writing.
 */
void Medium::updatePaths(const char *aOldPath, const char *aNewPath)
{
    AssertReturnVoid(aOldPath);
    AssertReturnVoid(aNewPath);

    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* we access children() */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    updatePath(aOldPath, aNewPath);

    /* update paths of all children */
    for (MediaList::const_iterator it = getChildren().begin();
         it != getChildren().end();
         ++it)
    {
        (*it)->updatePaths(aOldPath, aNewPath);
    }
}

/**
 * Returns the base hard disk of the hard disk chain this hard disk is part of.
 *
 * The base hard disk is found by walking up the parent-child relationship axis.
 * If the hard disk doesn't have a parent (i.e. it's a base hard disk), it
 * returns itself in response to this method.
 *
 * @param aLevel    Where to store the number of ancestors of this hard disk
 *                  (zero for the base), may be @c NULL.
 *
 * @note Locks medium tree for reading.
 */
ComObjPtr<Medium> Medium::getBase(uint32_t *aLevel /*= NULL*/)
{
    ComObjPtr<Medium> pBase;
    uint32_t level;

    AutoCaller autoCaller(this);
    AssertReturn(autoCaller.isOk(), pBase);

    /* we access mParent */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    pBase = this;
    level = 0;

    if (m->pParent)
    {
        for (;;)
        {
            AutoCaller baseCaller(pBase);
            AssertReturn(baseCaller.isOk(), pBase);

            if (pBase->m->pParent.isNull())
                break;

            pBase = pBase->m->pParent;
            ++level;
        }
    }

    if (aLevel != NULL)
        *aLevel = level;

    return pBase;
}

/**
 * Returns @c true if this hard disk cannot be modified because it has
 * dependants (children) or is part of the snapshot. Related to the hard disk
 * type and posterity, not to the current media state.
 *
 * @note Locks this object and medium tree for reading.
 */
bool Medium::isReadOnly()
{
    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), false);

    /* we access children */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (m->type)
    {
        case MediumType_Normal:
        {
            if (getChildren().size() != 0)
                return true;

            for (BackRefList::const_iterator it = m->backRefs.begin();
                 it != m->backRefs.end(); ++it)
                if (it->llSnapshotIds.size() != 0)
                    return true;

            return false;
        }
        case MediumType_Immutable:
            return true;
        case MediumType_Writethrough:
        case MediumType_Shareable:
            return false;
        default:
            break;
    }

    AssertFailedReturn(false);
}

/**
 * Saves hard disk data by appending a new <HardDisk> child node to the given
 * parent node which can be either <HardDisks> or <HardDisk>.
 *
 * @param data      Settings struct to be updated.
 *
 * @note Locks this object, medium tree and children for reading.
 */
HRESULT Medium::saveSettings(settings::Medium &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* we access mParent */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.uuid = m->id;
    data.strLocation = m->strLocation;
    data.strFormat = m->strFormat;

    /* optional, only for diffs, default is false */
    if (m->pParent)
        data.fAutoReset = !!m->autoReset;
    else
        data.fAutoReset = false;

    /* optional */
    data.strDescription = m->strDescription;

    /* optional properties */
    data.properties.clear();
    for (Data::PropertyMap::const_iterator it = m->properties.begin();
         it != m->properties.end();
         ++it)
    {
        /* only save properties that have non-default values */
        if (!it->second.isEmpty())
        {
            Utf8Str name = it->first;
            Utf8Str value = it->second;
            data.properties[name] = value;
        }
    }

    /* only for base hard disks */
    if (m->pParent.isNull())
        data.hdType = m->type;

    /* save all children */
    for (MediaList::const_iterator it = getChildren().begin();
         it != getChildren().end();
         ++it)
    {
        settings::Medium med;
        HRESULT rc = (*it)->saveSettings(med);
        AssertComRCReturnRC(rc);
        data.llChildren.push_back(med);
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
HRESULT Medium::compareLocationTo(const char *aLocation, int &aResult)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str locationFull(m->strLocationFull);

    /// @todo NEWMEDIA delegate the comparison to the backend?

    if (m->formatObj->capabilities() & MediumFormatCapabilities_File)
    {
        Utf8Str location(aLocation);

        /* For locations represented by files, append the default path if
         * only the name is given, and then get the full path. */
        if (!RTPathHavePath(aLocation))
        {
            location = Utf8StrFmt("%s%c%s",
                                  m->pVirtualBox->getDefaultHardDiskFolder().raw(),
                                  RTPATH_DELIMITER,
                                  aLocation);
        }

        int vrc = m->pVirtualBox->calculateFullPath(location, location);
        if (RT_FAILURE(vrc))
            return setError(E_FAIL,
                            tr("Invalid hard disk storage file location '%s' (%Rrc)"),
                            location.raw(),
                            vrc);

        aResult = RTPathCompare(locationFull.c_str(), location.c_str());
    }
    else
        aResult = locationFull.compare(aLocation);

    return S_OK;
}

/**
 * Constructs a medium lock list for this medium. The lock is not taken.
 *
 * @note Locks the medium tree for reading.
 *
 * @param fMediumWritable   Whether to associate a write lock with this medium.
 * @param pToBeParent       Medium which will become the parent of this medium.
 * @param mediumLockList    Where to store the resulting list.
 */
HRESULT Medium::createMediumLockList(bool fMediumWritable,
                                     Medium *pToBeParent,
                                     MediumLockList &mediumLockList)
{
    HRESULT rc = S_OK;

    /* we access parent medium objects */
    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

    /* paranoid sanity checking if the medium has a to-be parent medium */
    if (pToBeParent)
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        ComAssertRet(getParent().isNull(), E_FAIL);
        ComAssertRet(getChildren().size() == 0, E_FAIL);
    }

    ErrorInfoKeeper eik;
    MultiResult mrc(S_OK);

    ComObjPtr<Medium> pMedium = this;
    while (!pMedium.isNull())
    {
        // need write lock for RefreshState if medium is inaccessible
        AutoWriteLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

        /* Accessibility check must be first, otherwise locking interferes
         * with getting the medium state. Lock lists are not created for
         * fun, and thus getting the image status is no luxury. */
        MediumState_T mediumState = pMedium->getState();
        if (mediumState == MediumState_Inaccessible)
        {
            rc = pMedium->RefreshState(&mediumState);
            if (FAILED(rc)) return rc;

            if (mediumState == MediumState_Inaccessible)
            {
                Bstr error;
                rc = pMedium->COMGETTER(LastAccessError)(error.asOutParam());
                if (FAILED(rc)) return rc;

                Bstr loc;
                rc = pMedium->COMGETTER(Location)(loc.asOutParam());
                if (FAILED(rc)) return rc;

                /* collect multiple errors */
                eik.restore();

                /* be in sync with MediumBase::setStateError() */
                Assert(!error.isEmpty());
                mrc = setError(E_FAIL,
                               tr("Medium '%ls' is not accessible. %ls"),
                               loc.raw(),
                               error.raw());

                eik.fetch();
            }
        }

        if (pMedium == this)
            mediumLockList.Prepend(pMedium, fMediumWritable);
        else
            mediumLockList.Prepend(pMedium, false);

        pMedium = pMedium->getParent();
        if (pMedium.isNull() && pToBeParent)
        {
            pMedium = pToBeParent;
            pToBeParent = NULL;
        }
    }

    return mrc;
}

/**
 * Returns a preferred format for differencing hard disks.
 */
Bstr Medium::preferredDiffFormat()
{
    Utf8Str strFormat;

    AutoCaller autoCaller(this);
    AssertComRCReturn(autoCaller.rc(), strFormat);

    /* m->strFormat is const, no need to lock */
    strFormat = m->strFormat;

    /* check that our own format supports diffs */
    if (!(m->formatObj->capabilities() & MediumFormatCapabilities_Differencing))
    {
        /* use the default format if not */
        AutoReadLock propsLock(m->pVirtualBox->systemProperties() COMMA_LOCKVAL_SRC_POS);
        strFormat = m->pVirtualBox->getDefaultHardDiskFormat();
    }

    return strFormat;
}

/**
 * Returns the medium type. Must have caller + locking!
 * @return
 */
MediumType_T Medium::getType() const
{
    return m->type;
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Returns a short version of the location attribute.
 *
 * @note Must be called from under this object's read or write lock.
 */
Utf8Str Medium::getName()
{
    Utf8Str name = RTPathFilename(m->strLocationFull.c_str());
    return name;
}

/**
 * Sets the value of m->strLocation and calculates the value of m->strLocationFull.
 *
 * Treats non-FS-path locations specially, and prepends the default hard disk
 * folder if the given location string does not contain any path information
 * at all.
 *
 * Also, if the specified location is a file path that ends with '/' then the
 * file name part will be generated by this method automatically in the format
 * '{<uuid>}.<ext>' where <uuid> is a fresh UUID that this method will generate
 * and assign to this medium, and <ext> is the default extension for this
 * medium's storage format. Note that this procedure requires the media state to
 * be NotCreated and will return a failure otherwise.
 *
 * @param aLocation Location of the storage unit. If the location is a FS-path,
 *                  then it can be relative to the VirtualBox home directory.
 * @param aFormat   Optional fallback format if it is an import and the format
 *                  cannot be determined.
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT Medium::setLocation(const Utf8Str &aLocation, const Utf8Str &aFormat)
{
    AssertReturn(!aLocation.isEmpty(), E_FAIL);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    /* formatObj may be null only when initializing from an existing path and
     * no format is known yet */
    AssertReturn(    (!m->strFormat.isEmpty() && !m->formatObj.isNull())
                  || (    autoCaller.state() == InInit
                       && m->state != MediumState_NotCreated
                       && m->id.isEmpty()
                       && m->strFormat.isEmpty()
                       && m->formatObj.isNull()),
                 E_FAIL);

    /* are we dealing with a new medium constructed using the existing
     * location? */
    bool isImport = m->strFormat.isEmpty();

    if (   isImport
        || (   (m->formatObj->capabilities() & MediumFormatCapabilities_File)
            && !m->hostDrive))
    {
        Guid id;

        Utf8Str location(aLocation);

        if (m->state == MediumState_NotCreated)
        {
            /* must be a file (formatObj must be already known) */
            Assert(m->formatObj->capabilities() & MediumFormatCapabilities_File);

            if (RTPathFilename(location.c_str()) == NULL)
            {
                /* no file name is given (either an empty string or ends with a
                 * slash), generate a new UUID + file name if the state allows
                 * this */

                ComAssertMsgRet(!m->formatObj->fileExtensions().empty(),
                                ("Must be at least one extension if it is MediumFormatCapabilities_File\n"),
                                E_FAIL);

                Bstr ext = m->formatObj->fileExtensions().front();
                ComAssertMsgRet(!ext.isEmpty(),
                                ("Default extension must not be empty\n"),
                                E_FAIL);

                id.create();

                location = Utf8StrFmt("%s{%RTuuid}.%ls",
                                      location.raw(), id.raw(), ext.raw());
            }
        }

        /* append the default folder if no path is given */
        if (!RTPathHavePath(location.c_str()))
            location = Utf8StrFmt("%s%c%s",
                                  m->pVirtualBox->getDefaultHardDiskFolder().raw(),
                                  RTPATH_DELIMITER,
                                  location.raw());

        /* get the full file name */
        Utf8Str locationFull;
        int vrc = m->pVirtualBox->calculateFullPath(location, locationFull);
        if (RT_FAILURE(vrc))
            return setError(VBOX_E_FILE_ERROR,
                            tr("Invalid medium storage file location '%s' (%Rrc)"),
                            location.raw(), vrc);

        /* detect the backend from the storage unit if importing */
        if (isImport)
        {
            char *backendName = NULL;

            /* is it a file? */
            {
                RTFILE file;
                vrc = RTFileOpen(&file, locationFull.c_str(), RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
                if (RT_SUCCESS(vrc))
                    RTFileClose(file);
            }
            if (RT_SUCCESS(vrc))
            {
                vrc = VDGetFormat(NULL, locationFull.c_str(), &backendName);
            }
            else if (vrc != VERR_FILE_NOT_FOUND && vrc != VERR_PATH_NOT_FOUND)
            {
                /* assume it's not a file, restore the original location */
                location = locationFull = aLocation;
                vrc = VDGetFormat(NULL, locationFull.c_str(), &backendName);
            }

            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_FILE_NOT_FOUND || vrc == VERR_PATH_NOT_FOUND)
                    return setError(VBOX_E_FILE_ERROR,
                                    tr("Could not find file for the medium '%s' (%Rrc)"),
                                    locationFull.raw(), vrc);
                else if (aFormat.isEmpty())
                    return setError(VBOX_E_IPRT_ERROR,
                                    tr("Could not get the storage format of the medium '%s' (%Rrc)"),
                                    locationFull.raw(), vrc);
                else
                {
                    HRESULT rc = setFormat(Bstr(aFormat));
                    /* setFormat() must not fail since we've just used the backend so
                     * the format object must be there */
                    AssertComRCReturnRC(rc);
                }
            }
            else
            {
                ComAssertRet(backendName != NULL && *backendName != '\0', E_FAIL);

                HRESULT rc = setFormat(Bstr(backendName));
                RTStrFree(backendName);

                /* setFormat() must not fail since we've just used the backend so
                 * the format object must be there */
                AssertComRCReturnRC(rc);
            }
        }

        /* is it still a file? */
        if (m->formatObj->capabilities() & MediumFormatCapabilities_File)
        {
            m->strLocation = location;
            m->strLocationFull = locationFull;

            if (m->state == MediumState_NotCreated)
            {
                /* assign a new UUID (this UUID will be used when calling
                 * VDCreateBase/VDCreateDiff as a wanted UUID). Note that we
                 * also do that if we didn't generate it to make sure it is
                 * either generated by us or reset to null */
                unconst(m->id) = id;
            }
        }
        else
        {
            m->strLocation = locationFull;
            m->strLocationFull = locationFull;
        }
    }
    else
    {
        m->strLocation = aLocation;
        m->strLocationFull = aLocation;
    }

    return S_OK;
}

/**
 * Queries information from the image file.
 *
 * As a result of this call, the accessibility state and data members such as
 * size and description will be updated with the current information.
 *
 * @note This method may block during a system I/O call that checks storage
 *       accessibility.
 *
 * @note Locks medium tree for reading and writing (for new diff media checked
 *       for the first time). Locks mParent for reading. Locks this object for
 *       writing.
 */
HRESULT Medium::queryInfo()
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (   m->state != MediumState_Created
        && m->state != MediumState_Inaccessible
        && m->state != MediumState_LockedRead)
        return E_FAIL;

    HRESULT rc = S_OK;

    int vrc = VINF_SUCCESS;

    /* check if a blocking queryInfo() call is in progress on some other thread,
     * and wait for it to finish if so instead of querying data ourselves */
    if (m->queryInfoRunning)
    {
        Assert(   m->state == MediumState_LockedRead
               || m->state == MediumState_LockedWrite);

        alock.leave();

        vrc = RTSemEventMultiWait(m->queryInfoSem, RT_INDEFINITE_WAIT);

        alock.enter();

        AssertRC(vrc);

        return S_OK;
    }

    bool success = false;
    Utf8Str lastAccessError;

    /* are we dealing with a new medium constructed using the existing
     * location? */
    bool isImport = m->id.isEmpty();
    unsigned flags = VD_OPEN_FLAGS_INFO;

    /* Note that we don't use VD_OPEN_FLAGS_READONLY when opening new
     * media because that would prevent necessary modifications
     * when opening media of some third-party formats for the first
     * time in VirtualBox (such as VMDK for which VDOpen() needs to
     * generate an UUID if it is missing) */
    if (    (m->hddOpenMode == OpenReadOnly)
         || !isImport
       )
        flags |= VD_OPEN_FLAGS_READONLY;

    /* Lock the medium, which makes the behavior much more consistent */
    if (flags & VD_OPEN_FLAGS_READONLY)
        rc = LockRead(NULL);
    else
        rc = LockWrite(NULL);
    if (FAILED(rc)) return rc;

    /* Copies of the input state fields which are not read-only,
     * as we're dropping the lock. CAUTION: be extremely careful what
     * you do with the contents of this medium object, as you will
     * create races if there are concurrent changes. */
    Utf8Str format(m->strFormat);
    Utf8Str location(m->strLocationFull);
    ComObjPtr<MediumFormat> formatObj = m->formatObj;

    /* "Output" values which can't be set because the lock isn't held
     * at the time the values are determined. */
    Guid mediumId = m->id;
    uint64_t mediumSize = 0;
    uint64_t mediumLogicalSize = 0;

    /* leave the lock before a lengthy operation */
    vrc = RTSemEventMultiReset(m->queryInfoSem);
    AssertRCReturn(vrc, E_FAIL);
    m->queryInfoRunning = true;
    alock.leave();

    try
    {
        /* skip accessibility checks for host drives */
        if (m->hostDrive)
        {
            success = true;
            throw S_OK;
        }

        PVBOXHDD hdd;
        vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        try
        {
            /** @todo This kind of opening of images is assuming that diff
             * images can be opened as base images. Should be documented if
             * it must work for all medium format backends. */
            vrc = VDOpen(hdd,
                         format.c_str(),
                         location.c_str(),
                         flags,
                         m->vdDiskIfaces);
            if (RT_FAILURE(vrc))
            {
                lastAccessError = Utf8StrFmt(tr("Could not open the medium '%s'%s"),
                                             location.c_str(), vdError(vrc).c_str());
                throw S_OK;
            }

            if (formatObj->capabilities() & MediumFormatCapabilities_Uuid)
            {
                /* Modify the UUIDs if necessary. The associated fields are
                 * not modified by other code, so no need to copy. */
                if (m->setImageId)
                {
                    vrc = VDSetUuid(hdd, 0, m->imageId);
                    ComAssertRCThrow(vrc, E_FAIL);
                }
                if (m->setParentId)
                {
                    vrc = VDSetParentUuid(hdd, 0, m->parentId);
                    ComAssertRCThrow(vrc, E_FAIL);
                }
                /* zap the information, these are no long-term members */
                m->setImageId = false;
                unconst(m->imageId).clear();
                m->setParentId = false;
                unconst(m->parentId).clear();

                /* check the UUID */
                RTUUID uuid;
                vrc = VDGetUuid(hdd, 0, &uuid);
                ComAssertRCThrow(vrc, E_FAIL);

                if (isImport)
                {
                    mediumId = uuid;

                    if (mediumId.isEmpty() && (m->hddOpenMode == OpenReadOnly))
                        // only when importing a VDMK that has no UUID, create one in memory
                        mediumId.create();
                }
                else
                {
                    Assert(!mediumId.isEmpty());

                    if (mediumId != uuid)
                    {
                        lastAccessError = Utf8StrFmt(
                            tr("UUID {%RTuuid} of the medium '%s' does not match the value {%RTuuid} stored in the media registry ('%s')"),
                            &uuid,
                            location.c_str(),
                            mediumId.raw(),
                            m->pVirtualBox->settingsFilePath().c_str());
                        throw S_OK;
                    }
                }
            }
            else
            {
                /* the backend does not support storing UUIDs within the
                 * underlying storage so use what we store in XML */

                /* generate an UUID for an imported UUID-less medium */
                if (isImport)
                {
                    if (m->setImageId)
                        mediumId = m->imageId;
                    else
                        mediumId.create();
                }
            }

            /* check the type */
            unsigned uImageFlags;
            vrc = VDGetImageFlags(hdd, 0, &uImageFlags);
            ComAssertRCThrow(vrc, E_FAIL);

            if (uImageFlags & VD_IMAGE_FLAGS_DIFF)
            {
                RTUUID parentId;
                vrc = VDGetParentUuid(hdd, 0, &parentId);
                ComAssertRCThrow(vrc, E_FAIL);

                if (isImport)
                {
                    /* the parent must be known to us. Note that we freely
                     * call locking methods of mVirtualBox and parent from the
                     * write lock (breaking the {parent,child} lock order)
                     * because there may be no concurrent access to the just
                     * opened hard disk on ther threads yet (and init() will
                     * fail if this method reporst MediumState_Inaccessible) */

                    Guid id = parentId;
                    ComObjPtr<Medium> pParent;
                    rc = m->pVirtualBox->findHardDisk(&id, NULL,
                                                   false /* aSetError */,
                                                   &pParent);
                    if (FAILED(rc))
                    {
                        lastAccessError = Utf8StrFmt(
                            tr("Parent hard disk with UUID {%RTuuid} of the hard disk '%s' is not found in the media registry ('%s')"),
                            &parentId, location.c_str(),
                            m->pVirtualBox->settingsFilePath().c_str());
                        throw S_OK;
                    }

                    /* we set mParent & children() */
                    AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

                    Assert(m->pParent.isNull());
                    m->pParent = pParent;
                    m->pParent->m->llChildren.push_back(this);
                }
                else
                {
                    /* we access mParent */
                    AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

                    /* check that parent UUIDs match. Note that there's no need
                     * for the parent's AutoCaller (our lifetime is bound to
                     * it) */

                    if (m->pParent.isNull())
                    {
                        lastAccessError = Utf8StrFmt(
                            tr("Hard disk '%s' is differencing but it is not associated with any parent hard disk in the media registry ('%s')"),
                            location.c_str(),
                            m->pVirtualBox->settingsFilePath().c_str());
                        throw S_OK;
                    }

                    AutoReadLock parentLock(m->pParent COMMA_LOCKVAL_SRC_POS);
                    if (    m->pParent->getState() != MediumState_Inaccessible
                         && m->pParent->getId() != parentId)
                    {
                        lastAccessError = Utf8StrFmt(
                            tr("Parent UUID {%RTuuid} of the hard disk '%s' does not match UUID {%RTuuid} of its parent hard disk stored in the media registry ('%s')"),
                            &parentId, location.c_str(),
                            m->pParent->getId().raw(),
                            m->pVirtualBox->settingsFilePath().c_str());
                        throw S_OK;
                    }

                    /// @todo NEWMEDIA what to do if the parent is not
                    /// accessible while the diff is? Probably nothing. The
                    /// real code will detect the mismatch anyway.
                }
            }

            mediumSize = VDGetFileSize(hdd, 0);
            mediumLogicalSize = VDGetSize(hdd, 0) / _1M;

            success = true;
        }
        catch (HRESULT aRC)
        {
            rc = aRC;
        }

        VDDestroy(hdd);

    }
    catch (HRESULT aRC)
    {
        rc = aRC;
    }

    alock.enter();

    if (isImport)
        unconst(m->id) = mediumId;

    if (success)
    {
        m->size = mediumSize;
        m->logicalSize = mediumLogicalSize;
        m->strLastAccessError.setNull();
    }
    else
    {
        m->strLastAccessError = lastAccessError;
        LogWarningFunc(("'%s' is not accessible (error='%s', rc=%Rhrc, vrc=%Rrc)\n",
                         location.c_str(), m->strLastAccessError.c_str(),
                         rc, vrc));
    }

    /* inform other callers if there are any */
    RTSemEventMultiSignal(m->queryInfoSem);
    m->queryInfoRunning = false;

    /* Set the proper state according to the result of the check */
    if (success)
        m->preLockState = MediumState_Created;
    else
        m->preLockState = MediumState_Inaccessible;

    if (flags & VD_OPEN_FLAGS_READONLY)
        rc = UnlockRead(NULL);
    else
        rc = UnlockWrite(NULL);
    if (FAILED(rc)) return rc;

    return rc;
}

/**
 * Sets the extended error info according to the current media state.
 *
 * @note Must be called from under this object's write or read lock.
 */
HRESULT Medium::setStateError()
{
    HRESULT rc = E_FAIL;

    switch (m->state)
    {
        case MediumState_NotCreated:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Storage for the medium '%s' is not created"),
                          m->strLocationFull.raw());
            break;
        }
        case MediumState_Created:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Storage for the medium '%s' is already created"),
                          m->strLocationFull.raw());
            break;
        }
        case MediumState_LockedRead:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Medium '%s' is locked for reading by another task"),
                          m->strLocationFull.raw());
            break;
        }
        case MediumState_LockedWrite:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Medium '%s' is locked for writing by another task"),
                          m->strLocationFull.raw());
            break;
        }
        case MediumState_Inaccessible:
        {
            /* be in sync with Console::powerUpThread() */
            if (!m->strLastAccessError.isEmpty())
                rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                              tr("Medium '%s' is not accessible. %s"),
                              m->strLocationFull.raw(), m->strLastAccessError.c_str());
            else
                rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                              tr("Medium '%s' is not accessible"),
                              m->strLocationFull.raw());
            break;
        }
        case MediumState_Creating:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Storage for the medium '%s' is being created"),
                          m->strLocationFull.raw());
            break;
        }
        case MediumState_Deleting:
        {
            rc = setError(VBOX_E_INVALID_OBJECT_STATE,
                          tr("Storage for the medium '%s' is being deleted"),
                          m->strLocationFull.raw());
            break;
        }
        default:
        {
            AssertFailed();
            break;
        }
    }

    return rc;
}

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
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *                This only works in "wait" mode; otherwise saveSettings gets called automatically by the thread that was created,
 *                and this parameter is ignored.
 *
 * @note Locks mVirtualBox and this object for writing. Locks medium tree for
 *       writing.
 */
HRESULT Medium::deleteStorage(ComObjPtr<Progress> *aProgress,
                              bool aWait,
                              bool *pfNeedsSaveSettings)
{
    AssertReturn(aProgress != NULL || aWait == true, E_FAIL);

    HRESULT rc = S_OK;
    ComObjPtr<Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        /* we're accessing the media tree, and canClose() needs it too */
        AutoMultiWriteLock2 multilock(&m->pVirtualBox->getMediaTreeLockHandle(),
                                      this->lockHandle()
                                      COMMA_LOCKVAL_SRC_POS);
        LogFlowThisFunc(("aWait=%RTbool locationFull=%s\n", aWait, getLocationFull().c_str() ));

        if (    !(m->formatObj->capabilities() & (   MediumFormatCapabilities_CreateDynamic
                                                   | MediumFormatCapabilities_CreateFixed)))
            throw setError(VBOX_E_NOT_SUPPORTED,
                           tr("Hard disk format '%s' does not support storage deletion"),
                           m->strFormat.raw());

        /* Note that we are fine with Inaccessible state too: a) for symmetry
         * with create calls and b) because it doesn't really harm to try, if
         * it is really inaccessible, the delete operation will fail anyway.
         * Accepting Inaccessible state is especially important because all
         * registered hard disks are initially Inaccessible upon VBoxSVC
         * startup until COMGETTER(RefreshState) is called. Accept Deleting
         * state because some callers need to put the image in this state early
         * to prevent races. */
        switch (m->state)
        {
            case MediumState_Created:
            case MediumState_Deleting:
            case MediumState_Inaccessible:
                break;
            default:
                throw setStateError();
        }

        if (m->backRefs.size() != 0)
        {
            Utf8Str strMachines;
            for (BackRefList::const_iterator it = m->backRefs.begin();
                it != m->backRefs.end();
                ++it)
            {
                const BackRef &b = *it;
                if (strMachines.length())
                    strMachines.append(", ");
                strMachines.append(b.machineId.toString().c_str());
            }
#ifdef DEBUG
            dumpBackRefs();
#endif
            throw setError(VBOX_E_OBJECT_IN_USE,
                           tr("Cannot delete storage: hard disk '%s' is still attached to the following %d virtual machine(s): %s"),
                           m->strLocationFull.c_str(),
                           m->backRefs.size(),
                           strMachines.c_str());
        }

        rc = canClose();
        if (FAILED(rc))
            throw rc;

        /* go to Deleting state, so that the medium is not actually locked */
        if (m->state != MediumState_Deleting)
        {
            rc = markForDeletion();
            if (FAILED(rc))
                throw rc;
        }

        /* Build the medium lock list. */
        MediumLockList *pMediumLockList(new MediumLockList());
        rc = createMediumLockList(true, NULL,
                                  *pMediumLockList);
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw rc;
        }

        rc = pMediumLockList->Lock();
        if (FAILED(rc))
        {
            delete pMediumLockList;
            throw setError(rc,
                           tr("Failed to lock media when deleting '%ls'"),
                           getLocationFull().raw());
        }

        /* try to remove from the list of known hard disks before performing
         * actual deletion (we favor the consistency of the media registry in
         * the first place which would have been broken if
         * unregisterWithVirtualBox() failed after we successfully deleted the
         * storage) */
        rc = unregisterWithVirtualBox(pfNeedsSaveSettings);
        if (FAILED(rc))
            throw rc;

        if (aProgress != NULL)
        {
            /* use the existing progress object... */
            pProgress = *aProgress;

            /* ...but create a new one if it is null */
            if (pProgress.isNull())
            {
                pProgress.createObject();
                rc = pProgress->init(m->pVirtualBox,
                                     static_cast<IMedium*>(this),
                                     BstrFmt(tr("Deleting hard disk storage unit '%s'"), m->strLocationFull.raw()),
                                     FALSE /* aCancelable */);
                if (FAILED(rc))
                    throw rc;
            }
        }

        /* setup task object to carry out the operation sync/async */
        pTask = new Medium::DeleteTask(this, pProgress, pMediumLockList);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        if (aWait)
            rc = runNow(pTask, NULL /* pfNeedsSaveSettings*/);
        else
            rc = startThread(pTask);

        if (SUCCEEDED(rc) && aProgress != NULL)
            *aProgress = pProgress;

    }
    else
    {
        if (pTask)
            delete pTask;

        /* Undo deleting state if necessary. */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        unmarkForDeletion();
    }

    return rc;
}

/**
 * Mark a medium for deletion.
 *
 * @note Caller must hold the write lock on this medium!
 */
HRESULT Medium::markForDeletion()
{
    ComAssertRet(this->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);
    switch (m->state)
    {
        case MediumState_Created:
        case MediumState_Inaccessible:
            m->preLockState = m->state;
            m->state = MediumState_Deleting;
            return S_OK;
        default:
            return setStateError();
    }
}

/**
 * Removes the "mark for deletion".
 *
 * @note Caller must hold the write lock on this medium!
 */
HRESULT Medium::unmarkForDeletion()
{
    ComAssertRet(this->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);
    switch (m->state)
    {
        case MediumState_Deleting:
            m->state = m->preLockState;
            return S_OK;
        default:
            return setStateError();
    }
}

/**
 * Mark a medium for deletion which is in locked state.
 *
 * @note Caller must hold the write lock on this medium!
 */
HRESULT Medium::markLockedForDeletion()
{
    ComAssertRet(this->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);
    if (   (   m->state == MediumState_LockedRead
            || m->state == MediumState_LockedWrite)
        && m->preLockState == MediumState_Created)
    {
        m->preLockState = MediumState_Deleting;
        return S_OK;
    }
    else
        return setStateError();
}

/**
 * Removes the "mark for deletion" for a medium in locked state.
 *
 * @note Caller must hold the write lock on this medium!
 */
HRESULT Medium::unmarkLockedForDeletion()
{
    ComAssertRet(this->lockHandle()->isWriteLockOnCurrentThread(), E_FAIL);
    if (   (   m->state == MediumState_LockedRead
            || m->state == MediumState_LockedWrite)
        && m->preLockState == MediumState_Deleting)
    {
        m->preLockState = MediumState_Created;
        return S_OK;
    }
    else
        return setStateError();
}

/**
 * Creates a new differencing storage unit using the given target hard disk's
 * format and the location. Note that @c aTarget must be NotCreated.
 *
 * The @a aMediumLockList parameter contains the associated medium lock list,
 * which must be in locked state. If @a aWait is @c true then the caller is
 * responsible for unlocking.
 *
 * If @a aProgress is not NULL but the object it points to is @c null then a
 * new progress object will be created and assigned to @a *aProgress on
 * success, otherwise the existing progress object is used. If @a aProgress is
 * NULL, then no progress object is created/used at all.
 *
 * When @a aWait is @c false, this method will create a thread to perform the
 * create operation asynchronously and will return immediately. Otherwise, it
 * will perform the operation on the calling thread and will not return to the
 * caller until the operation is completed. Note that @a aProgress cannot be
 * NULL when @a aWait is @c false (this method will assert in this case).
 *
 * @param aTarget           Target hard disk.
 * @param aVariant          Precise image variant to create.
 * @param aMediumLockList   List of media which should be locked.
 * @param aProgress         Where to find/store a Progress object to track
 *                          operation completion.
 * @param aWait             @c true if this method should block instead of
 *                          creating an asynchronous thread.
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been
 *                          initialized to false and that will be set to true
 *                          by this function if the caller should invoke
 *                          VirtualBox::saveSettings() because the global
 *                          settings have changed. This only works in "wait"
 *                          mode; otherwise saveSettings is called
 *                          automatically by the thread that was created,
 *                          and this parameter is ignored.
 *
 * @note Locks this object and @a aTarget for writing.
 */
HRESULT Medium::createDiffStorage(ComObjPtr<Medium> &aTarget,
                                  MediumVariant_T aVariant,
                                  MediumLockList *aMediumLockList,
                                  ComObjPtr<Progress> *aProgress,
                                  bool aWait,
                                  bool *pfNeedsSaveSettings)
{
    AssertReturn(!aTarget.isNull(), E_FAIL);
    AssertReturn(aMediumLockList, E_FAIL);
    AssertReturn(aProgress != NULL || aWait == true, E_FAIL);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoCaller targetCaller(aTarget);
    if (FAILED(targetCaller.rc())) return targetCaller.rc();

    HRESULT rc = S_OK;
    ComObjPtr<Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        AutoMultiWriteLock2 alock(this, aTarget COMMA_LOCKVAL_SRC_POS);

        ComAssertThrow(m->type != MediumType_Writethrough, E_FAIL);
        ComAssertThrow(m->state == MediumState_LockedRead, E_FAIL);

        if (aTarget->m->state != MediumState_NotCreated)
            throw aTarget->setStateError();

        /* Check that the hard disk is not attached to the current state of
         * any VM referring to it. */
        for (BackRefList::const_iterator it = m->backRefs.begin();
             it != m->backRefs.end();
             ++it)
        {
            if (it->fInCurState)
            {
                /* Note: when a VM snapshot is being taken, all normal hard
                 * disks attached to the VM in the current state will be, as an
                 * exception, also associated with the snapshot which is about
                 * to create (see SnapshotMachine::init()) before deassociating
                 * them from the current state (which takes place only on
                 * success in Machine::fixupHardDisks()), so that the size of
                 * snapshotIds will be 1 in this case. The extra condition is
                 * used to filter out this legal situation. */
                if (it->llSnapshotIds.size() == 0)
                    throw setError(VBOX_E_INVALID_OBJECT_STATE,
                                   tr("Hard disk '%s' is attached to a virtual machine with UUID {%RTuuid}. No differencing hard disks based on it may be created until it is detached"),
                                   m->strLocationFull.raw(), it->machineId.raw());

                Assert(it->llSnapshotIds.size() == 1);
            }
        }

        if (aProgress != NULL)
        {
            /* use the existing progress object... */
            pProgress = *aProgress;

            /* ...but create a new one if it is null */
            if (pProgress.isNull())
            {
                pProgress.createObject();
                rc = pProgress->init(m->pVirtualBox,
                                     static_cast<IMedium*>(this),
                                     BstrFmt(tr("Creating differencing hard disk storage unit '%s'"), aTarget->m->strLocationFull.raw()),
                                     TRUE /* aCancelable */);
                if (FAILED(rc))
                    throw rc;
            }
        }

        /* setup task object to carry out the operation sync/async */
        pTask = new Medium::CreateDiffTask(this, pProgress, aTarget, aVariant,
                                           aMediumLockList,
                                           aWait /* fKeepMediumLockList */);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
             throw rc;

        /* register a task (it will deregister itself when done) */
        ++m->numCreateDiffTasks;
        Assert(m->numCreateDiffTasks != 0); /* overflow? */

        aTarget->m->state = MediumState_Creating;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        if (aWait)
            rc = runNow(pTask, pfNeedsSaveSettings);
        else
            rc = startThread(pTask);

        if (SUCCEEDED(rc) && aProgress != NULL)
            *aProgress = pProgress;
    }
    else if (pTask != NULL)
        delete pTask;

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
 * See #mergeTo() for more information about merging.
 *
 * @param pTarget       Target hard disk.
 * @param aMachineId    Allowed machine attachment. NULL means do not check.
 * @param aSnapshotId   Allowed snapshot attachment. NULL or empty UUID means
 *                      do not check.
 * @param fLockMedia    Flag whether to lock the medium lock list or not.
 *                      If set to false and the medium lock list locking fails
 *                      later you must call #cancelMergeTo().
 * @param fMergeForward Resulting merge direction (out).
 * @param pParentForTarget New parent for target medium after merge (out).
 * @param aChildrenToReparent List of children of the source which will have
 *                      to be reparented to the target after merge (out).
 * @param aMediumLockList Medium locking information (out).
 *
 * @note Locks medium tree for reading. Locks this object, aTarget and all
 *       intermediate hard disks for writing.
 */
HRESULT Medium::prepareMergeTo(const ComObjPtr<Medium> &pTarget,
                               const Guid *aMachineId,
                               const Guid *aSnapshotId,
                               bool fLockMedia,
                               bool &fMergeForward,
                               ComObjPtr<Medium> &pParentForTarget,
                               MediaList &aChildrenToReparent,
                               MediumLockList * &aMediumLockList)
{
    AssertReturn(pTarget != NULL, E_FAIL);
    AssertReturn(pTarget != this, E_FAIL);

    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoCaller targetCaller(pTarget);
    AssertComRCReturnRC(targetCaller.rc());

    HRESULT rc = S_OK;
    fMergeForward = false;
    pParentForTarget.setNull();
    aChildrenToReparent.clear();
    Assert(aMediumLockList == NULL);
    aMediumLockList = NULL;

    try
    {
        // locking: we need the tree lock first because we access parent pointers
        AutoReadLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        /* more sanity checking and figuring out the merge direction */
        ComObjPtr<Medium> pMedium = getParent();
        while (!pMedium.isNull() && pMedium != pTarget)
            pMedium = pMedium->getParent();
        if (pMedium == pTarget)
            fMergeForward = false;
        else
        {
            pMedium = pTarget->getParent();
            while (!pMedium.isNull() && pMedium != this)
                pMedium = pMedium->getParent();
            if (pMedium == this)
                fMergeForward = true;
            else
            {
                Utf8Str tgtLoc;
                {
                    AutoReadLock alock(pTarget COMMA_LOCKVAL_SRC_POS);
                    tgtLoc = pTarget->getLocationFull();
                }

                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
                throw setError(E_FAIL,
                               tr("Hard disks '%s' and '%s' are unrelated"),
                               m->strLocationFull.raw(), tgtLoc.raw());
            }
        }

        /* Build the lock list. */
        aMediumLockList = new MediumLockList();
        if (fMergeForward)
            rc = pTarget->createMediumLockList(true, NULL, *aMediumLockList);
        else
            rc = createMediumLockList(false, NULL, *aMediumLockList);
        if (FAILED(rc))
            throw rc;

        /* Sanity checking, must be after lock list creation as it depends on
         * valid medium states. The medium objects must be accessible. Only
         * do this if immediate locking is requested, otherwise it fails when
         * we construct a medium lock list for an already running VM. Snapshot
         * deletion uses this to simplify its life. */
        if (fLockMedia)
        {
            {
                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
                if (m->state != MediumState_Created)
                    throw setStateError();
            }
            {
                AutoReadLock alock(pTarget COMMA_LOCKVAL_SRC_POS);
                if (pTarget->m->state != MediumState_Created)
                    throw pTarget->setStateError();
            }
        }

        /* check medium attachment and other sanity conditions */
        if (fMergeForward)
        {
            AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
            if (getChildren().size() > 1)
            {
                throw setError(E_FAIL,
                               tr("Medium '%s' involved in the merge operation has more than one child medium (%d)"),
                               m->strLocationFull.raw(), getChildren().size());
            }
            /* One backreference is only allowed if the machine ID is not empty
             * and it matches the machine the image is attached to (including
             * the snapshot ID if not empty). */
            if (   m->backRefs.size() != 0
                && (   !aMachineId
                    || m->backRefs.size() != 1
                    || aMachineId->isEmpty()
                    || *getFirstMachineBackrefId() != *aMachineId
                    || (   (!aSnapshotId || !aSnapshotId->isEmpty())
                        && *getFirstMachineBackrefSnapshotId() != *aSnapshotId)))
                throw setError(E_FAIL,
                               tr("Medium '%s' is attached to %d virtual machines"),
                               m->strLocationFull.raw(), m->backRefs.size());
            if (m->type == MediumType_Immutable)
                throw setError(E_FAIL,
                               tr("Medium '%s' is immutable"),
                               m->strLocationFull.raw());
        }
        else
        {
            AutoReadLock alock(pTarget COMMA_LOCKVAL_SRC_POS);
            if (pTarget->getChildren().size() > 1)
            {
                throw setError(E_FAIL,
                               tr("Medium '%s' involved in the merge operation has more than one child medium (%d)"),
                               pTarget->m->strLocationFull.raw(),
                               pTarget->getChildren().size());
            }
            if (pTarget->m->type == MediumType_Immutable)
                throw setError(E_FAIL,
                               tr("Medium '%s' is immutable"),
                               pTarget->m->strLocationFull.raw());
        }
        ComObjPtr<Medium> pLast(fMergeForward ? (Medium *)pTarget : this);
        ComObjPtr<Medium> pLastIntermediate = pLast->getParent();
        for (pLast = pLastIntermediate;
             !pLast.isNull() && pLast != pTarget && pLast != this;
             pLast = pLast->getParent())
        {
            AutoReadLock alock(pLast COMMA_LOCKVAL_SRC_POS);
            if (pLast->getChildren().size() > 1)
            {
                throw setError(E_FAIL,
                               tr("Medium '%s' involved in the merge operation has more than one child medium (%d)"),
                               pLast->m->strLocationFull.raw(),
                               pLast->getChildren().size());
            }
            if (pLast->m->backRefs.size() != 0)
                throw setError(E_FAIL,
                               tr("Medium '%s' is attached to %d virtual machines"),
                               pLast->m->strLocationFull.raw(),
                               pLast->m->backRefs.size());

        }

        /* Update medium states appropriately */
        if (m->state == MediumState_Created)
        {
            rc = markForDeletion();
            if (FAILED(rc))
                throw rc;
        }
        else
        {
            if (fLockMedia)
                throw setStateError();
            else if (   m->state == MediumState_LockedWrite
                     || m->state == MediumState_LockedRead)
            {
                /* Either mark it for deletiion in locked state or allow
                 * others to have done so. */
                if (m->preLockState == MediumState_Created)
                    markLockedForDeletion();
                else if (m->preLockState != MediumState_Deleting)
                    throw setStateError();
            }
            else
                throw setStateError();
        }

        if (fMergeForward)
        {
            /* we will need parent to reparent target */
            pParentForTarget = m->pParent;
        }
        else
        {
            /* we will need to reparent children of the source */
            for (MediaList::const_iterator it = getChildren().begin();
                 it != getChildren().end();
                 ++it)
            {
                pMedium = *it;
                if (fLockMedia)
                {
                    rc = pMedium->LockWrite(NULL);
                    if (FAILED(rc))
                        throw rc;
                }

                aChildrenToReparent.push_back(pMedium);
            }
        }
        for (pLast = pLastIntermediate;
             !pLast.isNull() && pLast != pTarget && pLast != this;
             pLast = pLast->getParent())
        {
            AutoWriteLock alock(pLast COMMA_LOCKVAL_SRC_POS);
            if (pLast->m->state == MediumState_Created)
            {
                rc = pLast->markForDeletion();
                if (FAILED(rc))
                    throw rc;
            }
            else
                throw pLast->setStateError();
        }

        /* Tweak the lock list in the backward merge case, as the target
         * isn't marked to be locked for writing yet. */
        if (!fMergeForward)
        {
            MediumLockList::Base::iterator lockListBegin =
                aMediumLockList->GetBegin();
            MediumLockList::Base::iterator lockListEnd =
                aMediumLockList->GetEnd();
            lockListEnd--;
            for (MediumLockList::Base::iterator it = lockListBegin;
                 it != lockListEnd;
                 ++it)
            {
                MediumLock &mediumLock = *it;
                if (mediumLock.GetMedium() == pTarget)
                {
                    HRESULT rc2 = mediumLock.UpdateLock(true);
                    AssertComRC(rc2);
                    break;
                }
            }
        }

        if (fLockMedia)
        {
            rc = aMediumLockList->Lock();
            if (FAILED(rc))
            {
                AutoReadLock alock(pTarget COMMA_LOCKVAL_SRC_POS);
                throw setError(rc,
                               tr("Failed to lock media when merging to '%ls'"),
                               pTarget->getLocationFull().raw());
            }
        }
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (FAILED(rc))
    {
        delete aMediumLockList;
        aMediumLockList = NULL;
    }

    return rc;
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
 * Each diagram shows the involved hard disks on the hard disk chain where
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
 * involved extra hard disks) will be restored. @a aMediumLockList will not be
 * deleted, whether the operation is successful or not. The caller has to do
 * this if appropriate. Note that this (source) hard disk is not uninitialized
 * because of possible AutoCaller instances held by the caller of this method
 * on the current thread. It's therefore the responsibility of the caller to
 * call Medium::uninit() after releasing all callers.
 *
 * If @a aWait is @c false then this method will create a thread to perform the
 * operation asynchronously and will return immediately. If the operation
 * succeeds, the thread will uninitialize the source hard disk object and all
 * intermediate hard disk objects in the chain, reset the state of the target
 * hard disk (and all involved extra hard disks) and delete @a aMediumLockList.
 * If the operation fails, the thread will only reset the states of all
 * involved hard disks and delete @a aMediumLockList.
 *
 * When this method fails (regardless of the @a aWait mode), it is a caller's
 * responsiblity to undo state changes and delete @a aMediumLockList using
 * #cancelMergeTo().
 *
 * If @a aProgress is not NULL but the object it points to is @c null then a new
 * progress object will be created and assigned to @a *aProgress on success,
 * otherwise the existing progress object is used. If Progress is NULL, then no
 * progress object is created/used at all. Note that @a aProgress cannot be
 * NULL when @a aWait is @c false (this method will assert in this case).
 *
 * @param pTarget       Target hard disk.
 * @param fMergeForward Merge direction.
 * @param pParentForTarget New parent for target medium after merge.
 * @param aChildrenToReparent List of children of the source which will have
 *                      to be reparented to the target after merge.
 * @param aMediumLockList Medium locking information.
 * @param aProgress     Where to find/store a Progress object to track operation
 *                      completion.
 * @param aWait         @c true if this method should block instead of creating
 *                      an asynchronous thread.
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *                This only works in "wait" mode; otherwise saveSettings gets called automatically by the thread that was created,
 *                and this parameter is ignored.
 *
 * @note Locks the tree lock for writing. Locks the hard disks from the chain
 *       for writing.
 */
HRESULT Medium::mergeTo(const ComObjPtr<Medium> &pTarget,
                        bool fMergeForward,
                        const ComObjPtr<Medium> &pParentForTarget,
                        const MediaList &aChildrenToReparent,
                        MediumLockList *aMediumLockList,
                        ComObjPtr <Progress> *aProgress,
                        bool aWait,
                        bool *pfNeedsSaveSettings)
{
    AssertReturn(pTarget != NULL, E_FAIL);
    AssertReturn(pTarget != this, E_FAIL);
    AssertReturn(aMediumLockList != NULL, E_FAIL);
    AssertReturn(aProgress != NULL || aWait == true, E_FAIL);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    ComObjPtr <Progress> pProgress;
    Medium::Task *pTask = NULL;

    try
    {
        if (aProgress != NULL)
        {
            /* use the existing progress object... */
            pProgress = *aProgress;

            /* ...but create a new one if it is null */
            if (pProgress.isNull())
            {
                Utf8Str tgtName;
                {
                    AutoReadLock alock(pTarget COMMA_LOCKVAL_SRC_POS);
                    tgtName = pTarget->getName();
                }

                AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

                pProgress.createObject();
                rc = pProgress->init(m->pVirtualBox,
                                     static_cast<IMedium*>(this),
                                     BstrFmt(tr("Merging hard disk '%s' to '%s'"),
                                             getName().raw(),
                                             tgtName.raw()),
                                     TRUE /* aCancelable */);
                if (FAILED(rc))
                    throw rc;
            }
        }

        /* setup task object to carry out the operation sync/async */
        pTask = new Medium::MergeTask(this, pTarget, fMergeForward,
                                      pParentForTarget, aChildrenToReparent,
                                      pProgress, aMediumLockList,
                                      aWait /* fKeepMediumLockList */);
        rc = pTask->rc();
        AssertComRC(rc);
        if (FAILED(rc))
            throw rc;
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        if (aWait)
            rc = runNow(pTask, pfNeedsSaveSettings);
        else
            rc = startThread(pTask);

        if (SUCCEEDED(rc) && aProgress != NULL)
            *aProgress = pProgress;
    }
    else if (pTask != NULL)
        delete pTask;

    return rc;
}

/**
 * Undoes what #prepareMergeTo() did. Must be called if #mergeTo() is not
 * called or fails. Frees memory occupied by @a aMediumLockList and unlocks
 * the medium objects in @a aChildrenToReparent.
 *
 * @param aChildrenToReparent List of children of the source which will have
 *                      to be reparented to the target after merge.
 * @param aMediumLockList Medium locking information.
 *
 * @note Locks the hard disks from the chain for writing.
 */
void Medium::cancelMergeTo(const MediaList &aChildrenToReparent,
                           MediumLockList *aMediumLockList)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AssertReturnVoid(aMediumLockList != NULL);

    /* Revert media marked for deletion to previous state. */
    HRESULT rc;
    MediumLockList::Base::const_iterator mediumListBegin =
        aMediumLockList->GetBegin();
    MediumLockList::Base::const_iterator mediumListEnd =
        aMediumLockList->GetEnd();
    for (MediumLockList::Base::const_iterator it = mediumListBegin;
         it != mediumListEnd;
         ++it)
    {
        const MediumLock &mediumLock = *it;
        const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();
        AutoWriteLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

        if (pMedium->m->state == MediumState_Deleting)
        {
            rc = pMedium->unmarkForDeletion();
            AssertComRC(rc);
        }
    }

    /* the destructor will do the work */
    delete aMediumLockList;

    /* unlock the children which had to be reparented */
    for (MediaList::const_iterator it = aChildrenToReparent.begin();
         it != aChildrenToReparent.end();
         ++it)
    {
        const ComObjPtr<Medium> &pMedium = *it;

        AutoWriteLock alock(pMedium COMMA_LOCKVAL_SRC_POS);
        pMedium->UnlockWrite(NULL);
    }
}

/**
 * Checks that the format ID is valid and sets it on success.
 *
 * Note that this method will caller-reference the format object on success!
 * This reference must be released somewhere to let the MediumFormat object be
 * uninitialized.
 *
 * @note Must be called from under this object's write lock.
 */
HRESULT Medium::setFormat(CBSTR aFormat)
{
    /* get the format object first */
    {
        AutoReadLock propsLock(m->pVirtualBox->systemProperties() COMMA_LOCKVAL_SRC_POS);

        unconst(m->formatObj)
            = m->pVirtualBox->systemProperties()->mediumFormat(aFormat);
        if (m->formatObj.isNull())
            return setError(E_INVALIDARG,
                            tr("Invalid hard disk storage format '%ls'"),
                            aFormat);

        /* reference the format permanently to prevent its unexpected
         * uninitialization */
        HRESULT rc = m->formatObj->addCaller();
        AssertComRCReturnRC(rc);

        /* get properties (preinsert them as keys in the map). Note that the
         * map doesn't grow over the object life time since the set of
         * properties is meant to be constant. */

        Assert(m->properties.empty());

        for (MediumFormat::PropertyList::const_iterator it =
                m->formatObj->properties().begin();
             it != m->formatObj->properties().end();
             ++it)
        {
            m->properties.insert(std::make_pair(it->name, Bstr::Null));
        }
    }

    unconst(m->strFormat) = aFormat;

    return S_OK;
}

/**
 * @note Also reused by Medium::Reset().
 *
 * @note Caller must hold the media tree write lock!
 */
HRESULT Medium::canClose()
{
    Assert(m->pVirtualBox->getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    if (getChildren().size() != 0)
        return setError(E_FAIL,
                        tr("Cannot close medium '%s' because it has %d child hard disk(s)"),
                        m->strLocationFull.raw(), getChildren().size());

    return S_OK;
}

/**
 * Calls either VirtualBox::unregisterImage or VirtualBox::unregisterHardDisk depending
 * on the device type of this medium.
 *
 * @param pfNeedsSaveSettings Optional pointer to a bool that must have been initialized to false and that will be set to true
 *                by this function if the caller should invoke VirtualBox::saveSettings() because the global settings have changed.
 *
 * @note Caller must have locked the media tree lock for writing!
 */
HRESULT Medium::unregisterWithVirtualBox(bool *pfNeedsSaveSettings)
{
    /* Note that we need to de-associate ourselves from the parent to let
     * unregisterHardDisk() properly save the registry */

    /* we modify mParent and access children */
    Assert(m->pVirtualBox->getMediaTreeLockHandle().isWriteLockOnCurrentThread());

    Medium *pParentBackup = m->pParent;
    AssertReturn(getChildren().size() == 0, E_FAIL);
    if (m->pParent)
        deparent();

    HRESULT rc = E_FAIL;
    switch (m->devType)
    {
        case DeviceType_DVD:
            rc = m->pVirtualBox->unregisterImage(this, DeviceType_DVD, pfNeedsSaveSettings);
        break;

        case DeviceType_Floppy:
            rc = m->pVirtualBox->unregisterImage(this, DeviceType_Floppy, pfNeedsSaveSettings);
        break;

        case DeviceType_HardDisk:
            rc = m->pVirtualBox->unregisterHardDisk(this, pfNeedsSaveSettings);
        break;

        default:
        break;
    }

    if (FAILED(rc))
    {
        if (pParentBackup)
        {
            /* re-associate with the parent as we are still relatives in the
             * registry */
            m->pParent = pParentBackup;
            m->pParent->m->llChildren.push_back(this);
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
Utf8Str Medium::vdError(int aVRC)
{
    Utf8Str error;

    if (m->vdError.isEmpty())
        error = Utf8StrFmt(" (%Rrc)", aVRC);
    else
        error = Utf8StrFmt(".\n%s", m->vdError.raw());

    m->vdError.setNull();

    return error;
}

/**
 * Error message callback.
 *
 * Puts the reported error message to the m->vdError field.
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
DECLCALLBACK(void) Medium::vdErrorCall(void *pvUser, int rc, RT_SRC_POS_DECL,
                                       const char *pszFormat, va_list va)
{
    NOREF(pszFile); NOREF(iLine); NOREF(pszFunction); /* RT_SRC_POS_DECL */

    Medium *that = static_cast<Medium*>(pvUser);
    AssertReturnVoid(that != NULL);

    if (that->m->vdError.isEmpty())
        that->m->vdError =
            Utf8StrFmt("%s (%Rrc)", Utf8StrFmtVA(pszFormat, va).raw(), rc);
    else
        that->m->vdError =
            Utf8StrFmt("%s.\n%s (%Rrc)", that->m->vdError.raw(),
                       Utf8StrFmtVA(pszFormat, va).raw(), rc);
}

/* static */
DECLCALLBACK(bool) Medium::vdConfigAreKeysValid(void *pvUser,
                                                const char * /* pszzValid */)
{
    Medium *that = static_cast<Medium*>(pvUser);
    AssertReturn(that != NULL, false);

    /* we always return true since the only keys we have are those found in
     * VDBACKENDINFO */
    return true;
}

/* static */
DECLCALLBACK(int) Medium::vdConfigQuerySize(void *pvUser, const char *pszName,
                                            size_t *pcbValue)
{
    AssertReturn(VALID_PTR(pcbValue), VERR_INVALID_POINTER);

    Medium *that = static_cast<Medium*>(pvUser);
    AssertReturn(that != NULL, VERR_GENERAL_FAILURE);

    Data::PropertyMap::const_iterator it =
        that->m->properties.find(Bstr(pszName));
    if (it == that->m->properties.end())
        return VERR_CFGM_VALUE_NOT_FOUND;

    /* we interpret null values as "no value" in Medium */
    if (it->second.isEmpty())
        return VERR_CFGM_VALUE_NOT_FOUND;

    *pcbValue = it->second.length() + 1 /* include terminator */;

    return VINF_SUCCESS;
}

/* static */
DECLCALLBACK(int) Medium::vdConfigQuery(void *pvUser, const char *pszName,
                                        char *pszValue, size_t cchValue)
{
    AssertReturn(VALID_PTR(pszValue), VERR_INVALID_POINTER);

    Medium *that = static_cast<Medium*>(pvUser);
    AssertReturn(that != NULL, VERR_GENERAL_FAILURE);

    Data::PropertyMap::const_iterator it =
        that->m->properties.find(Bstr(pszName));
    if (it == that->m->properties.end())
        return VERR_CFGM_VALUE_NOT_FOUND;

    Utf8Str value = it->second;
    if (value.length() >= cchValue)
        return VERR_CFGM_NOT_ENOUGH_SPACE;

    /* we interpret null values as "no value" in Medium */
    if (it->second.isEmpty())
        return VERR_CFGM_VALUE_NOT_FOUND;

    memcpy(pszValue, value.c_str(), value.length() + 1);

    return VINF_SUCCESS;
}

/**
 * Starts a new thread driven by the appropriate Medium::Task::handler() method.
 *
 * @note When the task is executed by this method, IProgress::notifyComplete()
 *       is automatically called for the progress object associated with this
 *       task when the task is finished to signal the operation completion for
 *       other threads asynchronously waiting for it.
 */
HRESULT Medium::startThread(Medium::Task *pTask)
{
#ifdef VBOX_WITH_MAIN_LOCK_VALIDATION
    /* Extreme paranoia: The calling thread should not hold the medium
     * tree lock or any medium lock. Since there is no separate lock class
     * for medium objects be even more strict: no other object locks. */
    Assert(!AutoLockHoldsLocksInClass(LOCKCLASS_LISTOFMEDIA));
    Assert(!AutoLockHoldsLocksInClass(getLockingClass()));
#endif

    /// @todo use a more descriptive task name
    int vrc = RTThreadCreate(NULL, Medium::Task::fntMediumTask, pTask,
                             0, RTTHREADTYPE_MAIN_HEAVY_WORKER, 0,
                             "Medium::Task");
    if (RT_FAILURE(vrc))
    {
        delete pTask;
        ComAssertMsgRCRet(vrc,
                          ("Could not create Medium::Task thread (%Rrc)\n",
                           vrc),
                          E_FAIL);
    }

    return S_OK;
}

/**
 * Fix the parent UUID of all children to point to this medium as their
 * parent.
 */
HRESULT Medium::fixParentUuidOfChildren(const MediaList &childrenToReparent)
{
    MediumLockList mediumLockList;
    HRESULT rc = createMediumLockList(false, this, mediumLockList);
    AssertComRCReturnRC(rc);

    try
    {
        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        try
        {
            MediumLockList::Base::iterator lockListBegin =
                mediumLockList.GetBegin();
            MediumLockList::Base::iterator lockListEnd =
                mediumLockList.GetEnd();
            for (MediumLockList::Base::iterator it = lockListBegin;
                 it != lockListEnd;
                 ++it)
            {
                MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();
                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                // open the image
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             VD_OPEN_FLAGS_READONLY,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw vrc;
            }

            for (MediaList::const_iterator it = childrenToReparent.begin();
                 it != childrenToReparent.end();
                 ++it)
            {
                /* VD_OPEN_FLAGS_INFO since UUID is wrong yet */
                vrc = VDOpen(hdd,
                             (*it)->m->strFormat.c_str(),
                             (*it)->m->strLocationFull.c_str(),
                             VD_OPEN_FLAGS_INFO,
                             (*it)->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw vrc;

                vrc = VDSetParentUuid(hdd, VD_LAST_IMAGE, m->id);
                if (RT_FAILURE(vrc))
                    throw vrc;

                vrc = VDClose(hdd, false /* fDelete */);
                if (RT_FAILURE(vrc))
                    throw vrc;

                (*it)->UnlockWrite(NULL);
            }
        }
        catch (HRESULT aRC) { rc = aRC; }
        catch (int aVRC)
        {
            throw setError(E_FAIL,
                            tr("Could not update medium UUID references to parent '%s' (%s)"),
                            m->strLocationFull.raw(),
                            vdError(aVRC).raw());
        }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    return rc;
}

/**
 * Runs Medium::Task::handler() on the current thread instead of creating
 * a new one.
 *
 * This call implies that it is made on another temporary thread created for
 * some asynchronous task. Avoid calling it from a normal thread since the task
 * operations are potentially lengthy and will block the calling thread in this
 * case.
 *
 * @note When the task is executed by this method, IProgress::notifyComplete()
 *       is not called for the progress object associated with this task when
 *       the task is finished. Instead, the result of the operation is returned
 *       by this method directly and it's the caller's responsibility to
 *       complete the progress object in this case.
 */
HRESULT Medium::runNow(Medium::Task *pTask,
                       bool *pfNeedsSaveSettings)
{
#ifdef VBOX_WITH_MAIN_LOCK_VALIDATION
    /* Extreme paranoia: The calling thread should not hold the medium
     * tree lock or any medium lock. Since there is no separate lock class
     * for medium objects be even more strict: no other object locks. */
    Assert(!AutoLockHoldsLocksInClass(LOCKCLASS_LISTOFMEDIA));
    Assert(!AutoLockHoldsLocksInClass(getLockingClass()));
#endif

    pTask->m_pfNeedsSaveSettings = pfNeedsSaveSettings;

    /* NIL_RTTHREAD indicates synchronous call. */
    return (HRESULT)Medium::Task::fntMediumTask(NIL_RTTHREAD, pTask);
}

/**
 * Implementation code for the "create base" task.
 *
 * This only gets started from Medium::CreateBaseStorage() and always runs
 * asynchronously. As a result, we always save the VirtualBox.xml file when
 * we're done here.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskCreateBaseHandler(Medium::CreateBaseTask &task)
{
    HRESULT rc = S_OK;

    /* these parameters we need after creation */
    uint64_t size = 0, logicalSize = 0;
    bool fGenerateUuid = false;

    try
    {
        AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

        /* The object may request a specific UUID (through a special form of
        * the setLocation() argument). Otherwise we have to generate it */
        Guid id = m->id;
        fGenerateUuid = id.isEmpty();
        if (fGenerateUuid)
        {
            id.create();
            /* VirtualBox::registerHardDisk() will need UUID */
            unconst(m->id) = id;
        }

        Utf8Str format(m->strFormat);
        Utf8Str location(m->strLocationFull);
        uint64_t capabilities = m->formatObj->capabilities();
        ComAssertThrow(capabilities & (  VD_CAP_CREATE_FIXED
                                       | VD_CAP_CREATE_DYNAMIC), E_FAIL);
        Assert(m->state == MediumState_Creating);

        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        /* unlock before the potentially lengthy operation */
        thisLock.leave();

        try
        {
            /* ensure the directory exists */
            rc = VirtualBox::ensureFilePathExists(location);
            if (FAILED(rc))
                throw rc;

            PDMMEDIAGEOMETRY geo = { 0, 0, 0 }; /* auto-detect */

            vrc = VDCreateBase(hdd,
                               format.c_str(),
                               location.c_str(),
                               task.mSize * _1M,
                               task.mVariant,
                               NULL,
                               &geo,
                               &geo,
                               id.raw(),
                               VD_OPEN_FLAGS_NORMAL,
                               NULL,
                               task.mVDOperationIfaces);
            if (RT_FAILURE(vrc))
            {
                throw setError(E_FAIL,
                            tr("Could not create the hard disk storage unit '%s'%s"),
                            location.raw(), vdError(vrc).raw());
            }

            size = VDGetFileSize(hdd, 0);
            logicalSize = VDGetSize(hdd, 0) / _1M;
        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        /* register with mVirtualBox as the last step and move to
         * Created state only on success (leaving an orphan file is
         * better than breaking media registry consistency) */
        bool fNeedsSaveSettings = false;
        AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
        rc = m->pVirtualBox->registerHardDisk(this, &fNeedsSaveSettings);
        treeLock.release();

        if (fNeedsSaveSettings)
        {
            AutoWriteLock vboxlock(m->pVirtualBox COMMA_LOCKVAL_SRC_POS);
            m->pVirtualBox->saveSettings();
        }
    }

    // reenter the lock before changing state
    AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

    if (SUCCEEDED(rc))
    {
        m->state = MediumState_Created;

        m->size = size;
        m->logicalSize = logicalSize;
    }
    else
    {
        /* back to NotCreated on failure */
        m->state = MediumState_NotCreated;

        /* reset UUID to prevent it from being reused next time */
        if (fGenerateUuid)
            unconst(m->id).clear();
    }

    return rc;
}

/**
 * Implementation code for the "create diff" task.
 *
 * This task always gets started from Medium::createDiffStorage() and can run
 * synchronously or asynchronously depending on the "wait" parameter passed to
 * that function. If we run synchronously, the caller expects the bool
 * *pfNeedsSaveSettings to be set before returning; otherwise (in asynchronous
 * mode), we save the settings ourselves.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskCreateDiffHandler(Medium::CreateDiffTask &task)
{
    HRESULT rc = S_OK;

    bool fNeedsSaveSettings = false;

    const ComObjPtr<Medium> &pTarget = task.mTarget;

    uint64_t size = 0, logicalSize = 0;
    bool fGenerateUuid = false;

    try
    {
        /* Lock both in {parent,child} order. */
        AutoMultiWriteLock2 mediaLock(this, pTarget COMMA_LOCKVAL_SRC_POS);

        /* The object may request a specific UUID (through a special form of
         * the setLocation() argument). Otherwise we have to generate it */
        Guid targetId = pTarget->m->id;
        fGenerateUuid = targetId.isEmpty();
        if (fGenerateUuid)
        {
            targetId.create();
            /* VirtualBox::registerHardDisk() will need UUID */
            unconst(pTarget->m->id) = targetId;
        }

        Guid id = m->id;

        Utf8Str targetFormat(pTarget->m->strFormat);
        Utf8Str targetLocation(pTarget->m->strLocationFull);
        uint64_t capabilities = m->formatObj->capabilities();
        ComAssertThrow(capabilities & VD_CAP_CREATE_DYNAMIC, E_FAIL);

        Assert(pTarget->m->state == MediumState_Creating);
        Assert(m->state == MediumState_LockedRead);

        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        /* the two media are now protected by their non-default states;
         * unlock the media before the potentially lengthy operation */
        mediaLock.leave();

        try
        {
            /* Open all hard disk images in the target chain but the last. */
            MediumLockList::Base::const_iterator targetListBegin =
                task.mpMediumLockList->GetBegin();
            MediumLockList::Base::const_iterator targetListEnd =
                task.mpMediumLockList->GetEnd();
            for (MediumLockList::Base::const_iterator it = targetListBegin;
                 it != targetListEnd;
                 ++it)
            {
                const MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();

                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                /* Skip over the target diff image */
                if (pMedium->m->state == MediumState_Creating)
                    continue;

                /* sanity check */
                Assert(pMedium->m->state == MediumState_LockedRead);

                /* Open all images in appropriate mode. */
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             VD_OPEN_FLAGS_READONLY,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL,
                                   tr("Could not open the hard disk storage unit '%s'%s"),
                                   pMedium->m->strLocationFull.raw(),
                                   vdError(vrc).raw());
            }

            /* ensure the target directory exists */
            rc = VirtualBox::ensureFilePathExists(targetLocation);
            if (FAILED(rc))
                throw rc;

            vrc = VDCreateDiff(hdd,
                               targetFormat.c_str(),
                               targetLocation.c_str(),
                               task.mVariant | VD_IMAGE_FLAGS_DIFF,
                               NULL,
                               targetId.raw(),
                               id.raw(),
                               VD_OPEN_FLAGS_NORMAL,
                               pTarget->m->vdDiskIfaces,
                               task.mVDOperationIfaces);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                                tr("Could not create the differencing hard disk storage unit '%s'%s"),
                                targetLocation.raw(), vdError(vrc).raw());

            size = VDGetFileSize(hdd, VD_LAST_IMAGE);
            logicalSize = VDGetSize(hdd, VD_LAST_IMAGE) / _1M;
        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    if (SUCCEEDED(rc))
    {
        AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        Assert(pTarget->m->pParent.isNull());

        /* associate the child with the parent */
        pTarget->m->pParent = this;
        m->llChildren.push_back(pTarget);

        /** @todo r=klaus neither target nor base() are locked,
            * potential race! */
        /* diffs for immutable hard disks are auto-reset by default */
        pTarget->m->autoReset = (getBase()->m->type == MediumType_Immutable);

        /* register with mVirtualBox as the last step and move to
         * Created state only on success (leaving an orphan file is
         * better than breaking media registry consistency) */
        rc = m->pVirtualBox->registerHardDisk(pTarget, &fNeedsSaveSettings);

        if (FAILED(rc))
            /* break the parent association on failure to register */
            deparent();
    }

    AutoMultiWriteLock2 mediaLock(this, pTarget COMMA_LOCKVAL_SRC_POS);

    if (SUCCEEDED(rc))
    {
        pTarget->m->state = MediumState_Created;

        pTarget->m->size = size;
        pTarget->m->logicalSize = logicalSize;
    }
    else
    {
        /* back to NotCreated on failure */
        pTarget->m->state = MediumState_NotCreated;

        pTarget->m->autoReset = FALSE;

        /* reset UUID to prevent it from being reused next time */
        if (fGenerateUuid)
            unconst(pTarget->m->id).clear();
    }

    if (task.isAsync())
    {
        if (fNeedsSaveSettings)
        {
            mediaLock.leave();
            AutoWriteLock vboxlock(m->pVirtualBox COMMA_LOCKVAL_SRC_POS);
            m->pVirtualBox->saveSettings();
        }
    }
    else
        // synchronous mode: report save settings result to caller
        if (task.m_pfNeedsSaveSettings)
            *task.m_pfNeedsSaveSettings = fNeedsSaveSettings;

    /* deregister the task registered in createDiffStorage() */
    Assert(m->numCreateDiffTasks != 0);
    --m->numCreateDiffTasks;

    /* Note that in sync mode, it's the caller's responsibility to
     * unlock the hard disk */

    return rc;
}

/**
 * Implementation code for the "merge" task.
 *
 * This task always gets started from Medium::mergeTo() and can run
 * synchronously or asynchrously depending on the "wait" parameter passed to
 * that function. If we run synchronously, the caller expects the bool
 * *pfNeedsSaveSettings to be set before returning; otherwise (in asynchronous
 * mode), we save the settings ourselves.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskMergeHandler(Medium::MergeTask &task)
{
    HRESULT rc = S_OK;

    const ComObjPtr<Medium> &pTarget = task.mTarget;

    try
    {
        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        try
        {
            // Similar code appears in SessionMachine::onlineMergeMedium, so
            // if you make any changes below check whether they are applicable
            // in that context as well.

            unsigned uTargetIdx = VD_LAST_IMAGE;
            unsigned uSourceIdx = VD_LAST_IMAGE;
            /* Open all hard disks in the chain. */
            MediumLockList::Base::iterator lockListBegin =
                task.mpMediumLockList->GetBegin();
            MediumLockList::Base::iterator lockListEnd =
                task.mpMediumLockList->GetEnd();
            unsigned i = 0;
            for (MediumLockList::Base::iterator it = lockListBegin;
                 it != lockListEnd;
                 ++it)
            {
                MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();

                if (pMedium == this)
                    uSourceIdx = i;
                else if (pMedium == pTarget)
                    uTargetIdx = i;

                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                /*
                 * complex sanity (sane complexity)
                 *
                 * The current image must be in the Deleting (image is merged)
                 * or LockedRead (parent image) state if it is not the target.
                 * If it is the target it must be in the LockedWrite state.
                 */
                Assert(   (   pMedium != pTarget
                           && (   pMedium->m->state == MediumState_Deleting
                               || pMedium->m->state == MediumState_LockedRead))
                       || (   pMedium == pTarget
                           && pMedium->m->state == MediumState_LockedWrite));

                /*
                 * Image must be the target, in the LockedRead state
                 * or Deleting state where it is not allowed to be attached
                 * to a virtual machine.
                 */
                Assert(   pMedium == pTarget
                       || pMedium->m->state == MediumState_LockedRead
                       || (   pMedium->m->backRefs.size() == 0
                           && pMedium->m->state == MediumState_Deleting));
                /* The source medium must be in Deleting state. */
                Assert(  pMedium != this
                       || pMedium->m->state == MediumState_Deleting);

                unsigned uOpenFlags = 0;

                if (   pMedium->m->state == MediumState_LockedRead
                    || pMedium->m->state == MediumState_Deleting)
                    uOpenFlags = VD_OPEN_FLAGS_READONLY;

                /* Open the image */
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             uOpenFlags,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw vrc;

                i++;
            }

            ComAssertThrow(   uSourceIdx != VD_LAST_IMAGE
                           && uTargetIdx != VD_LAST_IMAGE, E_FAIL);

            vrc = VDMerge(hdd, uSourceIdx, uTargetIdx,
                          task.mVDOperationIfaces);
            if (RT_FAILURE(vrc))
                throw vrc;

            /* update parent UUIDs */
            if (!task.mfMergeForward)
            {
                /* we need to update UUIDs of all source's children
                 * which cannot be part of the container at once so
                 * add each one in there individually */
                if (task.mChildrenToReparent.size() > 0)
                {
                    for (MediaList::const_iterator it = task.mChildrenToReparent.begin();
                         it != task.mChildrenToReparent.end();
                         ++it)
                    {
                        /* VD_OPEN_FLAGS_INFO since UUID is wrong yet */
                        vrc = VDOpen(hdd,
                                     (*it)->m->strFormat.c_str(),
                                     (*it)->m->strLocationFull.c_str(),
                                     VD_OPEN_FLAGS_INFO,
                                     (*it)->m->vdDiskIfaces);
                        if (RT_FAILURE(vrc))
                            throw vrc;

                        vrc = VDSetParentUuid(hdd, VD_LAST_IMAGE,
                                              pTarget->m->id);
                        if (RT_FAILURE(vrc))
                            throw vrc;

                        vrc = VDClose(hdd, false /* fDelete */);
                        if (RT_FAILURE(vrc))
                            throw vrc;

                        (*it)->UnlockWrite(NULL);
                    }
                }
            }
        }
        catch (HRESULT aRC) { rc = aRC; }
        catch (int aVRC)
        {
            throw setError(E_FAIL,
                            tr("Could not merge the hard disk '%s' to '%s'%s"),
                            m->strLocationFull.raw(),
                            pTarget->m->strLocationFull.raw(),
                            vdError(aVRC).raw());
        }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    HRESULT rc2;

    if (SUCCEEDED(rc))
    {
        /* all hard disks but the target were successfully deleted by
         * VDMerge; reparent the last one and uninitialize deleted media. */

        AutoWriteLock treeLock(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        if (task.mfMergeForward)
        {
            /* first, unregister the target since it may become a base
             * hard disk which needs re-registration */
            rc2 = m->pVirtualBox->unregisterHardDisk(pTarget, NULL /*&fNeedsSaveSettings*/);
            AssertComRC(rc2);

            /* then, reparent it and disconnect the deleted branch at
             * both ends (chain->parent() is source's parent) */
            pTarget->deparent();
            pTarget->m->pParent = task.mParentForTarget;
            if (pTarget->m->pParent)
            {
                pTarget->m->pParent->m->llChildren.push_back(pTarget);
                deparent();
            }

            /* then, register again */
            rc2 = m->pVirtualBox->registerHardDisk(pTarget, NULL /*&fNeedsSaveSettings*/);
            AssertComRC(rc2);
        }
        else
        {
            Assert(pTarget->getChildren().size() == 1);
            Medium *targetChild = pTarget->getChildren().front();

            /* disconnect the deleted branch at the elder end */
            targetChild->deparent();

            /* reparent source's children and disconnect the deleted
             * branch at the younger end */
            if (task.mChildrenToReparent.size() > 0)
            {
                /* obey {parent,child} lock order */
                AutoWriteLock sourceLock(this COMMA_LOCKVAL_SRC_POS);

                for (MediaList::const_iterator it = task.mChildrenToReparent.begin();
                     it != task.mChildrenToReparent.end();
                     it++)
                {
                    Medium *pMedium = *it;
                    AutoWriteLock childLock(pMedium COMMA_LOCKVAL_SRC_POS);

                    pMedium->deparent();  // removes pMedium from source
                    pMedium->setParent(pTarget);
                }
            }
        }

        /* unregister and uninitialize all hard disks removed by the merge */
        MediumLockList::Base::iterator lockListBegin =
            task.mpMediumLockList->GetBegin();
        MediumLockList::Base::iterator lockListEnd =
            task.mpMediumLockList->GetEnd();
        for (MediumLockList::Base::iterator it = lockListBegin;
             it != lockListEnd;
             )
        {
            MediumLock &mediumLock = *it;
            /* Create a real copy of the medium pointer, as the medium
             * lock deletion below would invalidate the referenced object. */
            const ComObjPtr<Medium> pMedium = mediumLock.GetMedium();

            /* The target and all images not merged (readonly) are skipped */
            if (   pMedium == pTarget
                || pMedium->m->state == MediumState_LockedRead)
            {
                ++it;
                continue;
            }

            rc2 = pMedium->m->pVirtualBox->unregisterHardDisk(pMedium,
                                                              NULL /*pfNeedsSaveSettings*/);
            AssertComRC(rc2);

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
            if (pMedium == this)
            {
                Assert(getChildren().size() == 0);
                Assert(m->backRefs.size() == 0);
                task.mMediumCaller.release();
            }

            /* Delete the medium lock list entry, which also releases the
             * caller added by MergeChain before uninit() and updates the
             * iterator to point to the right place. */
            rc2 = task.mpMediumLockList->RemoveByIterator(it);
            AssertComRC(rc2);

            if (task.isAsync() || pMedium != this)
                pMedium->uninit();
        }
    }

    if (task.isAsync())
    {
        // in asynchronous mode, save settings now
        AutoWriteLock vboxlock(m->pVirtualBox COMMA_LOCKVAL_SRC_POS);
        m->pVirtualBox->saveSettings();
    }
    else
        // synchronous mode: report save settings result to caller
        if (task.m_pfNeedsSaveSettings)
            *task.m_pfNeedsSaveSettings = true;

    if (FAILED(rc))
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
        if (task.isAsync())
        {
            Assert(task.mChildrenToReparent.size() == 0);
            cancelMergeTo(task.mChildrenToReparent, task.mpMediumLockList);
        }
    }

    return rc;
}

/**
 * Implementation code for the "clone" task.
 *
 * This only gets started from Medium::CloneTo() and always runs asynchronously.
 * As a result, we always save the VirtualBox.xml file when we're done here.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskCloneHandler(Medium::CloneTask &task)
{
    HRESULT rc = S_OK;

    const ComObjPtr<Medium> &pTarget = task.mTarget;
    const ComObjPtr<Medium> &pParent = task.mParent;

    bool fCreatingTarget = false;

    uint64_t size = 0, logicalSize = 0;
    bool fGenerateUuid = false;

    try
    {
        /* Lock all in {parent,child} order. The lock is also used as a
         * signal from the task initiator (which releases it only after
         * RTThreadCreate()) that we can start the job. */
        AutoMultiWriteLock3 thisLock(this, pTarget, pParent COMMA_LOCKVAL_SRC_POS);

        fCreatingTarget = pTarget->m->state == MediumState_Creating;

        /* The object may request a specific UUID (through a special form of
         * the setLocation() argument). Otherwise we have to generate it */
        Guid targetId = pTarget->m->id;
        fGenerateUuid = targetId.isEmpty();
        if (fGenerateUuid)
        {
            targetId.create();
            /* VirtualBox::registerHardDisk() will need UUID */
            unconst(pTarget->m->id) = targetId;
        }

        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        try
        {
            /* Open all hard disk images in the source chain. */
            MediumLockList::Base::const_iterator sourceListBegin =
                task.mpSourceMediumLockList->GetBegin();
            MediumLockList::Base::const_iterator sourceListEnd =
                task.mpSourceMediumLockList->GetEnd();
            for (MediumLockList::Base::const_iterator it = sourceListBegin;
                 it != sourceListEnd;
                 ++it)
            {
                const MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();
                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                /* sanity check */
                Assert(pMedium->m->state == MediumState_LockedRead);

                /** Open all images in read-only mode. */
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             VD_OPEN_FLAGS_READONLY,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL,
                                    tr("Could not open the hard disk storage unit '%s'%s"),
                                    pMedium->m->strLocationFull.raw(),
                                    vdError(vrc).raw());
            }

            Utf8Str targetFormat(pTarget->m->strFormat);
            Utf8Str targetLocation(pTarget->m->strLocationFull);

            Assert(    pTarget->m->state == MediumState_Creating
                    || pTarget->m->state == MediumState_LockedWrite);
            Assert(m->state == MediumState_LockedRead);
            Assert(pParent.isNull() || pParent->m->state == MediumState_LockedRead);

            /* unlock before the potentially lengthy operation */
            thisLock.leave();

            /* ensure the target directory exists */
            rc = VirtualBox::ensureFilePathExists(targetLocation);
            if (FAILED(rc))
                throw rc;

            PVBOXHDD targetHdd;
            vrc = VDCreate(m->vdDiskIfaces, &targetHdd);
            ComAssertRCThrow(vrc, E_FAIL);

            try
            {
                /* Open all hard disk images in the target chain. */
                MediumLockList::Base::const_iterator targetListBegin =
                    task.mpTargetMediumLockList->GetBegin();
                MediumLockList::Base::const_iterator targetListEnd =
                    task.mpTargetMediumLockList->GetEnd();
                for (MediumLockList::Base::const_iterator it = targetListBegin;
                     it != targetListEnd;
                     ++it)
                {
                    const MediumLock &mediumLock = *it;
                    const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();

                    /* If the target medium is not created yet there's no
                     * reason to open it. */
                    if (pMedium == pTarget && fCreatingTarget)
                        continue;

                    AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                    /* sanity check */
                    Assert(    pMedium->m->state == MediumState_LockedRead
                            || pMedium->m->state == MediumState_LockedWrite);

                    /* Open all images in appropriate mode. */
                    vrc = VDOpen(targetHdd,
                                 pMedium->m->strFormat.c_str(),
                                 pMedium->m->strLocationFull.c_str(),
                                 (pMedium->m->state == MediumState_LockedWrite) ? VD_OPEN_FLAGS_NORMAL : VD_OPEN_FLAGS_READONLY,
                                 pMedium->m->vdDiskIfaces);
                    if (RT_FAILURE(vrc))
                        throw setError(E_FAIL,
                                       tr("Could not open the hard disk storage unit '%s'%s"),
                                       pMedium->m->strLocationFull.raw(),
                                       vdError(vrc).raw());
                }

                /** @todo r=klaus target isn't locked, race getting the state */
                vrc = VDCopy(hdd,
                             VD_LAST_IMAGE,
                             targetHdd,
                             targetFormat.c_str(),
                             (fCreatingTarget) ? targetLocation.raw() : (char *)NULL,
                             false,
                             0,
                             task.mVariant,
                             targetId.raw(),
                             NULL,
                             pTarget->m->vdDiskIfaces,
                             task.mVDOperationIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL,
                                    tr("Could not create the clone hard disk '%s'%s"),
                                    targetLocation.raw(), vdError(vrc).raw());

                size = VDGetFileSize(targetHdd, VD_LAST_IMAGE);
                logicalSize = VDGetSize(targetHdd, VD_LAST_IMAGE) / _1M;
            }
            catch (HRESULT aRC) { rc = aRC; }

            VDDestroy(targetHdd);
        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    /* Only do the parent changes for newly created images. */
    if (SUCCEEDED(rc) && fCreatingTarget)
    {
        /* we set mParent & children() */
        AutoWriteLock alock2(m->pVirtualBox->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);

        Assert(pTarget->m->pParent.isNull());

        if (pParent)
        {
            /* associate the clone with the parent and deassociate
             * from VirtualBox */
            pTarget->m->pParent = pParent;
            pParent->m->llChildren.push_back(pTarget);

            /* register with mVirtualBox as the last step and move to
             * Created state only on success (leaving an orphan file is
             * better than breaking media registry consistency) */
            rc = pParent->m->pVirtualBox->registerHardDisk(pTarget, NULL /* pfNeedsSaveSettings */);

            if (FAILED(rc))
                /* break parent association on failure to register */
                pTarget->deparent();     // removes target from parent
        }
        else
        {
            /* just register  */
            rc = m->pVirtualBox->registerHardDisk(pTarget, NULL /* pfNeedsSaveSettings */);
        }
    }

    if (fCreatingTarget)
    {
        AutoWriteLock mLock(pTarget COMMA_LOCKVAL_SRC_POS);

        if (SUCCEEDED(rc))
        {
            pTarget->m->state = MediumState_Created;

            pTarget->m->size = size;
            pTarget->m->logicalSize = logicalSize;
        }
        else
        {
            /* back to NotCreated on failure */
            pTarget->m->state = MediumState_NotCreated;

            /* reset UUID to prevent it from being reused next time */
            if (fGenerateUuid)
                unconst(pTarget->m->id).clear();
        }
    }

    // now, at the end of this task (always asynchronous), save the settings
    {
        AutoWriteLock vboxlock(m->pVirtualBox COMMA_LOCKVAL_SRC_POS);
        m->pVirtualBox->saveSettings();
    }

    /* Everything is explicitly unlocked when the task exits,
     * as the task destruction also destroys the source chain. */

    /* Make sure the source chain is released early. It could happen
     * that we get a deadlock in Appliance::Import when Medium::Close
     * is called & the source chain is released at the same time. */
    task.mpSourceMediumLockList->Clear();

    return rc;
}

/**
 * Implementation code for the "delete" task.
 *
 * This task always gets started from Medium::deleteStorage() and can run
 * synchronously or asynchrously depending on the "wait" parameter passed to
 * that function.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskDeleteHandler(Medium::DeleteTask &task)
{
    NOREF(task);
    HRESULT rc = S_OK;

    try
    {
        /* The lock is also used as a signal from the task initiator (which
         * releases it only after RTThreadCreate()) that we can start the job */
        AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        Utf8Str format(m->strFormat);
        Utf8Str location(m->strLocationFull);

        /* unlock before the potentially lengthy operation */
        Assert(m->state == MediumState_Deleting);
        thisLock.release();

        try
        {
            vrc = VDOpen(hdd,
                         format.c_str(),
                         location.c_str(),
                         VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                         m->vdDiskIfaces);
            if (RT_SUCCESS(vrc))
                vrc = VDClose(hdd, true /* fDelete */);

            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                                tr("Could not delete the hard disk storage unit '%s'%s"),
                                location.raw(), vdError(vrc).raw());

        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

    /* go to the NotCreated state even on failure since the storage
     * may have been already partially deleted and cannot be used any
     * more. One will be able to manually re-open the storage if really
     * needed to re-register it. */
    m->state = MediumState_NotCreated;

    /* Reset UUID to prevent Create* from reusing it again */
    unconst(m->id).clear();

    return rc;
}

/**
 * Implementation code for the "reset" task.
 *
 * This always gets started asynchronously from Medium::Reset().
 *
 * @param task
 * @return
 */
HRESULT Medium::taskResetHandler(Medium::ResetTask &task)
{
    HRESULT rc = S_OK;

    uint64_t size = 0, logicalSize = 0;

    try
    {
        /* The lock is also used as a signal from the task initiator (which
         * releases it only after RTThreadCreate()) that we can start the job */
        AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

        /// @todo Below we use a pair of delete/create operations to reset
        /// the diff contents but the most efficient way will of course be
        /// to add a VDResetDiff() API call

        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        Guid id = m->id;
        Utf8Str format(m->strFormat);
        Utf8Str location(m->strLocationFull);

        Medium *pParent = m->pParent;
        Guid parentId = pParent->m->id;
        Utf8Str parentFormat(pParent->m->strFormat);
        Utf8Str parentLocation(pParent->m->strLocationFull);

        Assert(m->state == MediumState_LockedWrite);

        /* unlock before the potentially lengthy operation */
        thisLock.release();

        try
        {
            /* Open all hard disk images in the target chain but the last. */
            MediumLockList::Base::const_iterator targetListBegin =
                task.mpMediumLockList->GetBegin();
            MediumLockList::Base::const_iterator targetListEnd =
                task.mpMediumLockList->GetEnd();
            for (MediumLockList::Base::const_iterator it = targetListBegin;
                 it != targetListEnd;
                 ++it)
            {
                const MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();

                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                /* sanity check */
                Assert(pMedium->m->state == MediumState_LockedRead);

                /* Open all images in appropriate mode. */
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             VD_OPEN_FLAGS_READONLY,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL,
                                   tr("Could not open the hard disk storage unit '%s'%s"),
                                   pMedium->m->strLocationFull.raw(),
                                   vdError(vrc).raw());

                /* Done when we hit the image which should be reset */
                if (pMedium == this)
                    break;
            }

            /* first, delete the storage unit */
            vrc = VDClose(hdd, true /* fDelete */);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                               tr("Could not delete the hard disk storage unit '%s'%s"),
                               location.raw(), vdError(vrc).raw());

            /* next, create it again */
            vrc = VDOpen(hdd,
                         parentFormat.c_str(),
                         parentLocation.c_str(),
                         VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO,
                         m->vdDiskIfaces);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                                tr("Could not open the hard disk storage unit '%s'%s"),
                                parentLocation.raw(), vdError(vrc).raw());

            vrc = VDCreateDiff(hdd,
                               format.c_str(),
                               location.c_str(),
                               /// @todo use the same image variant as before
                               VD_IMAGE_FLAGS_NONE,
                               NULL,
                               id.raw(),
                               parentId.raw(),
                               VD_OPEN_FLAGS_NORMAL,
                               m->vdDiskIfaces,
                               task.mVDOperationIfaces);
            if (RT_FAILURE(vrc))
                throw setError(E_FAIL,
                                tr("Could not create the differencing hard disk storage unit '%s'%s"),
                                location.raw(), vdError(vrc).raw());

            size = VDGetFileSize(hdd, VD_LAST_IMAGE);
            logicalSize = VDGetSize(hdd, VD_LAST_IMAGE) / _1M;
        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

    m->size = size;
    m->logicalSize = logicalSize;

    if (task.isAsync())
    {
        /* unlock ourselves when done */
        HRESULT rc2 = UnlockWrite(NULL);
        AssertComRC(rc2);
    }

    /* Note that in sync mode, it's the caller's responsibility to
     * unlock the hard disk */

    return rc;
}

/**
 * Implementation code for the "compact" task.
 *
 * @param task
 * @return
 */
HRESULT Medium::taskCompactHandler(Medium::CompactTask &task)
{
    HRESULT rc = S_OK;

    /* Lock all in {parent,child} order. The lock is also used as a
     * signal from the task initiator (which releases it only after
     * RTThreadCreate()) that we can start the job. */
    AutoWriteLock thisLock(this COMMA_LOCKVAL_SRC_POS);

    try
    {
        PVBOXHDD hdd;
        int vrc = VDCreate(m->vdDiskIfaces, &hdd);
        ComAssertRCThrow(vrc, E_FAIL);

        try
        {
            /* Open all hard disk images in the chain. */
            MediumLockList::Base::const_iterator mediumListBegin =
                task.mpMediumLockList->GetBegin();
            MediumLockList::Base::const_iterator mediumListEnd =
                task.mpMediumLockList->GetEnd();
            MediumLockList::Base::const_iterator mediumListLast =
                mediumListEnd;
            mediumListLast--;
            for (MediumLockList::Base::const_iterator it = mediumListBegin;
                 it != mediumListEnd;
                 ++it)
            {
                const MediumLock &mediumLock = *it;
                const ComObjPtr<Medium> &pMedium = mediumLock.GetMedium();
                AutoReadLock alock(pMedium COMMA_LOCKVAL_SRC_POS);

                /* sanity check */
                if (it == mediumListLast)
                    Assert(pMedium->m->state == MediumState_LockedWrite);
                else
                    Assert(pMedium->m->state == MediumState_LockedRead);

                /** Open all images but last in read-only mode. */
                vrc = VDOpen(hdd,
                             pMedium->m->strFormat.c_str(),
                             pMedium->m->strLocationFull.c_str(),
                             (it == mediumListLast) ? VD_OPEN_FLAGS_NORMAL : VD_OPEN_FLAGS_READONLY,
                             pMedium->m->vdDiskIfaces);
                if (RT_FAILURE(vrc))
                    throw setError(E_FAIL,
                                   tr("Could not open the hard disk storage unit '%s'%s"),
                                   pMedium->m->strLocationFull.raw(),
                                   vdError(vrc).raw());
            }

            Assert(m->state == MediumState_LockedWrite);

            Utf8Str location(m->strLocationFull);

            /* unlock before the potentially lengthy operation */
            thisLock.leave();

            vrc = VDCompact(hdd, VD_LAST_IMAGE, task.mVDOperationIfaces);
            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_NOT_SUPPORTED)
                    throw setError(VBOX_E_NOT_SUPPORTED,
                                   tr("Compacting is not yet supported for hard disk '%s'"),
                                   location.raw());
                else if (vrc == VERR_NOT_IMPLEMENTED)
                    throw setError(E_NOTIMPL,
                                   tr("Compacting is not implemented, hard disk '%s'"),
                                   location.raw());
                else
                    throw setError(E_FAIL,
                                   tr("Could not compact hard disk '%s'%s"),
                                   location.raw(),
                                   vdError(vrc).raw());
            }
        }
        catch (HRESULT aRC) { rc = aRC; }

        VDDestroy(hdd);
    }
    catch (HRESULT aRC) { rc = aRC; }

    /* Everything is explicitly unlocked when the task exits,
     * as the task destruction also destroys the image chain. */

    return rc;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
