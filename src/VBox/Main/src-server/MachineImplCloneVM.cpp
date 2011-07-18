/* $Id$ */
/** @file
 * Implementation of MachineCloneVM
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "MachineImplCloneVM.h"

#include "VirtualBoxImpl.h"
#include "MediumImpl.h"
#include "HostImpl.h"

#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/cpp/utils.h>

#include <VBox/com/list.h>
#include <VBox/com/MultiResult.h>

// typedefs
/////////////////////////////////////////////////////////////////////////////

typedef struct
{
    Utf8Str                 strBaseName;
    ComPtr<IMedium>         pMedium;
    ULONG                   uWeight;
} MEDIUMTASK;

typedef struct
{
    RTCList<MEDIUMTASK>     chain;
    bool                    fCreateDiffs;
    bool                    fAttachLinked;
} MEDIUMTASKCHAIN;

typedef struct
{
    Guid                    snapshotUuid;
    Utf8Str                 strSaveStateFile;
    ULONG                   uWeight;
} SAVESTATETASK;

// The private class
/////////////////////////////////////////////////////////////////////////////

struct MachineCloneVMPrivate
{
    MachineCloneVMPrivate(MachineCloneVM *a_q, ComObjPtr<Machine> &a_pSrcMachine, ComObjPtr<Machine> &a_pTrgMachine, CloneMode_T a_mode, const RTCList<CloneOptions_T> &opts)
      : q_ptr(a_q)
      , p(a_pSrcMachine)
      , pSrcMachine(a_pSrcMachine)
      , pTrgMachine(a_pTrgMachine)
      , mode(a_mode)
      , options(opts)
    {}

    /* Thread management */
    int startWorker()
    {
        return RTThreadCreate(NULL,
                              MachineCloneVMPrivate::workerThread,
                              static_cast<void*>(this),
                              0,
                              RTTHREADTYPE_MAIN_WORKER,
                              0,
                              "MachineClone");
    }

    static int workerThread(RTTHREAD /* Thread */, void *pvUser)
    {
        MachineCloneVMPrivate *pTask = static_cast<MachineCloneVMPrivate*>(pvUser);
        AssertReturn(pTask, VERR_INVALID_POINTER);

        HRESULT rc = pTask->q_ptr->run();

        pTask->pProgress->notifyComplete(rc);

        pTask->q_ptr->destroy();

        return VINF_SUCCESS;
    }

    /* Private helper methods */
    HRESULT createMachineList(const ComPtr<ISnapshot> &pSnapshot, RTCList< ComObjPtr<Machine> > &machineList) const;
    settings::Snapshot findSnapshot(settings::MachineConfigFile *pMCF, const settings::SnapshotsList &snl, const Guid &id) const;
    void updateMACAddresses(settings::NetworkAdaptersList &nwl) const;
    void updateMACAddresses(settings::SnapshotsList &sl) const;
    void updateStorageLists(settings::StorageControllersList &sc, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void updateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId, const Bstr &bstrNewId) const;
    void updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const;
    static int copyStateFileProgress(unsigned uPercentage, void *pvUser);

    /* Private q and parent pointer */
    MachineCloneVM             *q_ptr;
    ComObjPtr<Machine>          p;

    /* Private helper members */
    ComObjPtr<Machine>          pSrcMachine;
    ComObjPtr<Machine>          pTrgMachine;
    ComPtr<IMachine>            pOldMachineState;
    ComObjPtr<Progress>         pProgress;
    Guid                        snapshotId;
    CloneMode_T                 mode;
    RTCList<CloneOptions_T>     options;
    RTCList<MEDIUMTASKCHAIN>    llMedias;
    RTCList<SAVESTATETASK>      llSaveStateFiles; /* Snapshot UUID -> File path */
};

HRESULT MachineCloneVMPrivate::createMachineList(const ComPtr<ISnapshot> &pSnapshot, RTCList< ComObjPtr<Machine> > &machineList) const
{
    HRESULT rc = S_OK;
    Bstr name;
    rc = pSnapshot->COMGETTER(Name)(name.asOutParam());
    if (FAILED(rc)) return rc;

    ComPtr<IMachine> pMachine;
    rc = pSnapshot->COMGETTER(Machine)(pMachine.asOutParam());
    if (FAILED(rc)) return rc;
    machineList.append((Machine*)(IMachine*)pMachine);

    SafeIfaceArray<ISnapshot> sfaChilds;
    rc = pSnapshot->COMGETTER(Children)(ComSafeArrayAsOutParam(sfaChilds));
    if (FAILED(rc)) return rc;
    for (size_t i = 0; i < sfaChilds.size(); ++i)
    {
        rc = createMachineList(sfaChilds[i], machineList);
        if (FAILED(rc)) return rc;
    }

    return rc;
}

settings::Snapshot MachineCloneVMPrivate::findSnapshot(settings::MachineConfigFile *pMCF, const settings::SnapshotsList &snl, const Guid &id) const
{
    settings::SnapshotsList::const_iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
            return *it;
        else if (!it->llChildSnapshots.empty())
            return findSnapshot(pMCF, it->llChildSnapshots, id);
    }
    return settings::Snapshot();
}

void MachineCloneVMPrivate::updateMACAddresses(settings::NetworkAdaptersList &nwl) const
{
    const bool fNotNAT = options.contains(CloneOptions_KeepNATMACs);
    settings::NetworkAdaptersList::iterator it;
    for (it = nwl.begin(); it != nwl.end(); ++it)
    {
        if (   fNotNAT
            && it->mode == NetworkAttachmentType_NAT)
            continue;
        Host::generateMACAddress(it->strMACAddress);
    }
}

void MachineCloneVMPrivate::updateMACAddresses(settings::SnapshotsList &sl) const
{
    settings::SnapshotsList::iterator it;
    for (it = sl.begin(); it != sl.end(); ++it)
    {
        updateMACAddresses(it->hardware.llNetworkAdapters);
        if (!it->llChildSnapshots.empty())
            updateMACAddresses(it->llChildSnapshots);
    }
}

void MachineCloneVMPrivate::updateStorageLists(settings::StorageControllersList &sc, const Bstr &bstrOldId, const Bstr &bstrNewId) const
{
    settings::StorageControllersList::iterator it3;
    for (it3 = sc.begin();
         it3 != sc.end();
         ++it3)
    {
        settings::AttachedDevicesList &llAttachments = it3->llAttachedDevices;
        settings::AttachedDevicesList::iterator it4;
        for (it4 = llAttachments.begin();
             it4 != llAttachments.end();
             ++it4)
        {
            if (   it4->deviceType == DeviceType_HardDisk
                && it4->uuid == bstrOldId)
            {
                it4->uuid = bstrNewId;
            }
        }
    }
}

void MachineCloneVMPrivate::updateSnapshotStorageLists(settings::SnapshotsList &sl, const Bstr &bstrOldId, const Bstr &bstrNewId) const
{
    settings::SnapshotsList::iterator it;
    for (  it  = sl.begin();
           it != sl.end();
         ++it)
    {
        updateStorageLists(it->storage.llStorageControllers, bstrOldId, bstrNewId);
        if (!it->llChildSnapshots.empty())
            updateSnapshotStorageLists(it->llChildSnapshots, bstrOldId, bstrNewId);
    }
}

void MachineCloneVMPrivate::updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile) const
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
            it->strStateFile = strFile;
        else if (!it->llChildSnapshots.empty())
            updateStateFile(it->llChildSnapshots, id, strFile);
    }
}

/* static */
int MachineCloneVMPrivate::copyStateFileProgress(unsigned uPercentage, void *pvUser)
{
    ComObjPtr<Progress> pProgress = *static_cast< ComObjPtr<Progress>* >(pvUser);

    BOOL fCanceled = false;
    HRESULT rc = pProgress->COMGETTER(Canceled)(&fCanceled);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;
    /* If canceled by the user tell it to the copy operation. */
    if (fCanceled) return VERR_CANCELLED;
    /* Set the new process. */
    rc = pProgress->SetCurrentOperationProgress(uPercentage);
    if (FAILED(rc)) return VERR_GENERAL_FAILURE;

    return VINF_SUCCESS;
}

// The public class
/////////////////////////////////////////////////////////////////////////////

MachineCloneVM::MachineCloneVM(ComObjPtr<Machine> pSrcMachine, ComObjPtr<Machine> pTrgMachine, CloneMode_T mode, const RTCList<CloneOptions_T> &opts)
    : d_ptr(new MachineCloneVMPrivate(this, pSrcMachine, pTrgMachine, mode, opts))
{
}

MachineCloneVM::~MachineCloneVM()
{
    delete d_ptr;
}

HRESULT MachineCloneVM::start(IProgress **pProgress)
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    HRESULT rc;
    try
    {
        /* Handle the special case that someone is requesting a _full_ clone
         * with all snapshots (and the current state), but uses a snapshot
         * machine (and not the current one) as source machine. In this case we
         * just replace the source (snapshot) machine with the current machine. */
        if (   d->mode == CloneMode_AllStates
            && d->pSrcMachine->isSnapshotMachine())
        {
            Bstr bstrSrcMachineId;
            rc = d->pSrcMachine->COMGETTER(Id)(bstrSrcMachineId.asOutParam());
            if (FAILED(rc)) throw rc;
            ComPtr<IMachine> newSrcMachine;
            rc = d->pSrcMachine->getVirtualBox()->FindMachine(bstrSrcMachineId.raw(), newSrcMachine.asOutParam());
            if (FAILED(rc)) throw rc;
            d->pSrcMachine = (Machine*)(IMachine*)newSrcMachine;
        }

        /* Lock the target machine early (so nobody mess around with it in the meantime). */
        AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

        if (d->pSrcMachine->isSnapshotMachine())
            d->snapshotId = d->pSrcMachine->getSnapshotId();

        /* Add the current machine and all snapshot machines below this machine
         * in a list for further processing. */
        RTCList< ComObjPtr<Machine> > machineList;

        /* Include current state? */
        if (   d->mode == CloneMode_MachineState
            || d->mode == CloneMode_AllStates)
            machineList.append(d->pSrcMachine);
        /* Should be done a depth copy with all child snapshots? */
        if (   d->mode == CloneMode_MachineAndChildStates
            || d->mode == CloneMode_AllStates)
        {
            ULONG cSnapshots = 0;
            rc = d->pSrcMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            if (FAILED(rc)) throw rc;
            if (cSnapshots > 0)
            {
                Utf8Str id;
                if (    d->mode == CloneMode_MachineAndChildStates
                    && !d->snapshotId.isEmpty())
                    id = d->snapshotId.toString();
                ComPtr<ISnapshot> pSnapshot;
                rc = d->pSrcMachine->FindSnapshot(Bstr(id).raw(), pSnapshot.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = d->createMachineList(pSnapshot, machineList);
                if (FAILED(rc)) throw rc;
                if (d->mode == CloneMode_MachineAndChildStates)
                {
                    rc = pSnapshot->COMGETTER(Machine)(d->pOldMachineState.asOutParam());
                    if (FAILED(rc)) throw rc;
                }
            }
        }

        /* Go over every machine and walk over every attachment this machine has. */
        ULONG uCount       = 2; /* One init task and the machine creation. */
        ULONG uTotalWeight = 2; /* The init task and the machine creation is worth one. */
        for (size_t i = 0; i < machineList.size(); ++i)
        {
            ComObjPtr<Machine> machine = machineList.at(i);
            /* If this is the Snapshot Machine we want to clone, we need to
             * create a new diff file for the new "current state". */
            bool fCreateDiffs = false;
            if (machine == d->pOldMachineState)
                fCreateDiffs = true;
            /* If we want to create a linked clone just attach the medium
             * associated with the snapshot. The rest is taken care of by
             * attach already, so no need to duplicate this. */
            bool fAttachLinked = false;
            if (d->options.contains(CloneOptions_Link))
                fAttachLinked = true;
            SafeIfaceArray<IMediumAttachment> sfaAttachments;
            rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
            if (FAILED(rc)) throw rc;
            /* Add all attachments (and their parents) of the different
             * machines to a worker list. */
            for (size_t a = 0; a < sfaAttachments.size(); ++a)
            {
                const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
                DeviceType_T type;
                rc = pAtt->COMGETTER(Type)(&type);
                if (FAILED(rc)) throw rc;

                /* Only harddisk's are of interest. */
                if (type != DeviceType_HardDisk)
                    continue;

                /* Valid medium attached? */
                ComPtr<IMedium> pSrcMedium;
                rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
                if (FAILED(rc)) throw rc;
                if (pSrcMedium.isNull())
                    continue;

                /* Build up a child->parent list of this attachment. (Note: we are
                 * not interested of any child's not attached to this VM. So this
                 * will not create a full copy of the base/child relationship.) */
                MEDIUMTASKCHAIN mtc;
                mtc.fCreateDiffs = fCreateDiffs;
                mtc.fAttachLinked = fAttachLinked;

                /* If the current state without any snapshots is cloned, we
                 * don't need any diff images in the new clone. Add the last
                 * medium to the list of medias to create only (the clone
                 * operation of IMedium will create a merged copy
                 * automatically). */
                if (d->mode == CloneMode_MachineState)
                {
                    /* Refresh the state so that the file size get read. */
                    MediumState_T e;
                    rc = pSrcMedium->RefreshState(&e);
                    if (FAILED(rc)) throw rc;
                    LONG64 lSize;
                    rc = pSrcMedium->COMGETTER(Size)(&lSize);
                    if (FAILED(rc)) throw rc;

                    ComPtr<IMedium> pBaseMedium;
                    rc = pSrcMedium->COMGETTER(Base)(pBaseMedium.asOutParam());
                    if (FAILED(rc)) throw rc;

                    MEDIUMTASK mt;

                    Bstr bstrBaseName;
                    rc = pBaseMedium->COMGETTER(Name)(bstrBaseName.asOutParam());
                    if (FAILED(rc)) throw rc;

                    /* Save the base name. */
                    mt.strBaseName = bstrBaseName;

                    /* Save the current medium, for later cloning. */
                    mt.pMedium = pSrcMedium;
                    if (fAttachLinked)
                        mt.uWeight = 0; /* dummy */
                    else
                        mt.uWeight = (lSize + _1M - 1) / _1M;
                    mtc.chain.append(mt);
                }
                else
                {
                    /** @todo r=klaus this puts way too many images in the list
                     * when cloning a snapshot (sub)tree, which means that more
                     * images are cloned than necessary. It is just the easiest
                     * way to get a working VM, as getting the image
                     * parent/child relationships right for only the bare
                     * minimum cloning is rather tricky. */
                    while (!pSrcMedium.isNull())
                    {
                        /* Refresh the state so that the file size get read. */
                        MediumState_T e;
                        rc = pSrcMedium->RefreshState(&e);
                        if (FAILED(rc)) throw rc;
                        LONG64 lSize;
                        rc = pSrcMedium->COMGETTER(Size)(&lSize);
                        if (FAILED(rc)) throw rc;

                        /* Save the current medium, for later cloning. */
                        MEDIUMTASK mt;
                        mt.pMedium = pSrcMedium;
                        mt.uWeight = (lSize + _1M - 1) / _1M;
                        mtc.chain.append(mt);

                        /* Query next parent. */
                        rc = pSrcMedium->COMGETTER(Parent)(pSrcMedium.asOutParam());
                        if (FAILED(rc)) throw rc;
                    }
                }

                if (fAttachLinked)
                {
                    /* Implicit diff creation as part of attach is a pretty cheap
                     * operation, and does only need one operation per attachment. */
                    ++uCount;
                    uTotalWeight += 1;  /* 1MB per attachment */
                }
                else
                {
                    /* Currently the copying of diff images involves reading at least
                     * the biggest parent in the previous chain. So even if the new
                     * diff image is small in size, it could need some time to create
                     * it. Adding the biggest size in the chain should balance this a
                     * little bit more, i.e. the weight is the sum of the data which
                     * needs to be read and written. */
                    uint64_t uMaxSize = 0;
                    for (size_t e = mtc.chain.size(); e > 0; --e)
                    {
                        MEDIUMTASK &mt = mtc.chain.at(e - 1);
                        mt.uWeight += uMaxSize;

                        /* Calculate progress data */
                        ++uCount;
                        uTotalWeight += mt.uWeight;

                        /* Save the max size for better weighting of diff image
                         * creation. */
                        uMaxSize = RT_MAX(uMaxSize, mt.uWeight);
                    }
                }
                d->llMedias.append(mtc);
            }
            Bstr bstrSrcSaveStatePath;
            rc = machine->COMGETTER(StateFilePath)(bstrSrcSaveStatePath.asOutParam());
            if (FAILED(rc)) throw rc;
            if (!bstrSrcSaveStatePath.isEmpty())
            {
                SAVESTATETASK sst;
                sst.snapshotUuid     = machine->getSnapshotId();
                sst.strSaveStateFile = bstrSrcSaveStatePath;
                uint64_t cbSize;
                int vrc = RTFileQuerySize(sst.strSaveStateFile.c_str(), &cbSize);
                if (RT_FAILURE(vrc))
                    throw p->setError(VBOX_E_IPRT_ERROR, p->tr("Could not query file size of '%s' (%Rrc)"), sst.strSaveStateFile.c_str(), vrc);
                /* same rule as above: count both the data which needs to
                 * be read and written */
                sst.uWeight = 2 * (cbSize + _1M - 1) / _1M;
                d->llSaveStateFiles.append(sst);
                ++uCount;
                uTotalWeight += sst.uWeight;
            }
        }

        rc = d->pProgress.createObject();
        if (FAILED(rc)) throw rc;
        rc = d->pProgress->init(p->getVirtualBox(),
                                static_cast<IMachine*>(d->pSrcMachine) /* aInitiator */,
                                Bstr(p->tr("Cloning Machine")).raw(),
                                true /* fCancellable */,
                                uCount,
                                uTotalWeight,
                                Bstr(p->tr("Initialize Cloning")).raw(),
                                1);
        if (FAILED(rc)) throw rc;

        int vrc = d->startWorker();

        if (RT_FAILURE(vrc))
            p->setError(VBOX_E_IPRT_ERROR, "Could not create machine clone thread (%Rrc)", vrc);
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }

    if (SUCCEEDED(rc))
        d->pProgress.queryInterfaceTo(pProgress);

    return rc;
}

HRESULT MachineCloneVM::run()
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;

    AutoCaller autoCaller(p);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock  srcLock(p COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock trgLock(d->pTrgMachine COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    /*
     * Todo:
     * - What about log files?
     */

    /* Where should all the media go? */
    Utf8Str strTrgSnapshotFolder;
    Utf8Str strTrgMachineFolder = d->pTrgMachine->getSettingsFileFull();
    strTrgMachineFolder.stripFilename();

    RTCList<ComObjPtr<Medium> > newMedia;   /* All created images */
    RTCList<Utf8Str> newFiles;              /* All extra created files (save states, ...) */
    try
    {
        /* Copy all the configuration from this machine to an empty
         * configuration dataset. */
        settings::MachineConfigFile trgMCF = *d->pSrcMachine->mData->pMachineConfigFile;

        /* Reset media registry. */
        trgMCF.mediaRegistry.llHardDisks.clear();
        /* If we got a valid snapshot id, replace the hardware/storage section
         * with the stuff from the snapshot. */
        settings::Snapshot sn;
        if (!d->snapshotId.isEmpty())
            sn = d->findSnapshot(&trgMCF, trgMCF.llFirstSnapshot, d->snapshotId);

        if (d->mode == CloneMode_MachineState)
        {
            if (!sn.uuid.isEmpty())
            {
                trgMCF.hardwareMachine = sn.hardware;
                trgMCF.storageMachine  = sn.storage;
            }

            /* Remove any hint on snapshots. */
            trgMCF.llFirstSnapshot.clear();
            trgMCF.uuidCurrentSnapshot.clear();
        }
        else if (   d->mode == CloneMode_MachineAndChildStates
                 && !sn.uuid.isEmpty())
        {
            /* Copy the snapshot data to the current machine. */
            trgMCF.hardwareMachine = sn.hardware;
            trgMCF.storageMachine  = sn.storage;

            /* The snapshot will be the root one. */
            trgMCF.uuidCurrentSnapshot = sn.uuid;
            trgMCF.llFirstSnapshot.clear();
            trgMCF.llFirstSnapshot.push_back(sn);
        }

        /* Generate new MAC addresses for all machines when not forbidden. */
        if (!d->options.contains(CloneOptions_KeepAllMACs))
        {
            d->updateMACAddresses(trgMCF.hardwareMachine.llNetworkAdapters);
            d->updateMACAddresses(trgMCF.llFirstSnapshot);
        }

        /* When the current snapshot folder is absolute we reset it to the
         * default relative folder. */
        if (RTPathStartsWithRoot(trgMCF.machineUserData.strSnapshotFolder.c_str()))
            trgMCF.machineUserData.strSnapshotFolder = "Snapshots";
        trgMCF.strStateFile = "";
        /* Force writing of setting file. */
        trgMCF.fCurrentStateModified = true;
        /* Set the new name. */
        const Utf8Str strOldVMName = trgMCF.machineUserData.strName;
        trgMCF.machineUserData.strName = d->pTrgMachine->mUserData->s.strName;
        trgMCF.uuid = d->pTrgMachine->mData->mUuid;

        Bstr bstrSrcSnapshotFolder;
        rc = d->pSrcMachine->COMGETTER(SnapshotFolder)(bstrSrcSnapshotFolder.asOutParam());
        if (FAILED(rc)) throw rc;
        /* The absolute name of the snapshot folder. */
        strTrgSnapshotFolder = Utf8StrFmt("%s%c%s", strTrgMachineFolder.c_str(), RTPATH_DELIMITER, trgMCF.machineUserData.strSnapshotFolder.c_str());

        /* Should we rename the disk names. */
        bool fKeepDiskNames = d->options.contains(CloneOptions_KeepDiskNames);

        /* We need to create a map with the already created medias. This is
         * necessary, cause different snapshots could have the same
         * parents/parent chain. If a medium is in this map already, it isn't
         * cloned a second time, but simply used. */
        typedef std::map<Utf8Str, ComObjPtr<Medium> > TStrMediumMap;
        typedef std::pair<Utf8Str, ComObjPtr<Medium> > TStrMediumPair;
        TStrMediumMap map;
        GuidList llRegistriesThatNeedSaving;
        size_t cDisks = 0;
        for (size_t i = 0; i < d->llMedias.size(); ++i)
        {
            const MEDIUMTASKCHAIN &mtc = d->llMedias.at(i);
            ComObjPtr<Medium> pNewParent;
            for (size_t a = mtc.chain.size(); a > 0; --a)
            {
                const MEDIUMTASK &mt = mtc.chain.at(a - 1);
                ComPtr<IMedium> pMedium = mt.pMedium;

                Bstr bstrSrcName;
                rc = pMedium->COMGETTER(Name)(bstrSrcName.asOutParam());
                if (FAILED(rc)) throw rc;

                rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Cloning Disk '%ls' ..."), bstrSrcName.raw()).raw(), mt.uWeight);
                if (FAILED(rc)) throw rc;

                Bstr bstrSrcId;
                rc = pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
                if (FAILED(rc)) throw rc;

                if (mtc.fAttachLinked)
                {
                    IMedium *pTmp = pMedium;
                    ComObjPtr<Medium> pLMedium = static_cast<Medium*>(pTmp);
                    if (pLMedium.isNull())
                        throw E_POINTER;
                    ComObjPtr<Medium> pBase = pLMedium->getBase();
                    if (pBase->isReadOnly())
                    {
                        ComObjPtr<Medium> pDiff;
                        /* create the diff under the snapshot medium */
                        rc = createDiffHelper(pLMedium, strTrgSnapshotFolder,
                                              &newMedia, &pDiff);
                        if (FAILED(rc)) throw rc;
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pDiff));
                        /* diff image has to be used... */
                        pNewParent = pDiff;
                    }
                    else
                    {
                        /* Attach the medium directly, as its type is not
                         * subject to diff creation. */
                        newMedia.append(pLMedium);
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pLMedium));
                        pNewParent = pLMedium;
                    }
                }
                else
                {
                    /* Is a clone already there? */
                    TStrMediumMap::iterator it = map.find(Utf8Str(bstrSrcId));
                    if (it != map.end())
                        pNewParent = it->second;
                    else
                    {
                        ComPtr<IMediumFormat> pSrcFormat;
                        rc = pMedium->COMGETTER(MediumFormat)(pSrcFormat.asOutParam());
                        ULONG uSrcCaps = 0;
                        rc = pSrcFormat->COMGETTER(Capabilities)(&uSrcCaps);
                        if (FAILED(rc)) throw rc;

                        /* Default format? */
                        Utf8Str strDefaultFormat;
                        p->mParent->getDefaultHardDiskFormat(strDefaultFormat);
                        Bstr bstrSrcFormat(strDefaultFormat);
                        ULONG srcVar = MediumVariant_Standard;
                        /* Is the source file based? */
                        if ((uSrcCaps & MediumFormatCapabilities_File) == MediumFormatCapabilities_File)
                        {
                            /* Yes, just use the source format. Otherwise the defaults
                             * will be used. */
                            rc = pMedium->COMGETTER(Format)(bstrSrcFormat.asOutParam());
                            if (FAILED(rc)) throw rc;
                            rc = pMedium->COMGETTER(Variant)(&srcVar);
                            if (FAILED(rc)) throw rc;
                        }

                        Guid newId;
                        newId.create();
                        Utf8Str strNewName(bstrSrcName);
                        if (!fKeepDiskNames)
                        {
                            Utf8Str strSrcTest = bstrSrcName;
                            /* Check if we have to use another name. */
                            if (!mt.strBaseName.isEmpty())
                                strSrcTest = mt.strBaseName;
                            strSrcTest.stripExt();
                            /* If the old disk name was in {uuid} format we also
                             * want the new name in this format, but with the
                             * updated id of course. If the old disk was called
                             * like the VM name, we change it to the new VM name.
                             * For all other disks we rename them with this
                             * template: "new name-disk1.vdi". */
                            if (strSrcTest == strOldVMName)
                                strNewName = Utf8StrFmt("%s%s", trgMCF.machineUserData.strName.c_str(), RTPathExt(Utf8Str(bstrSrcName).c_str()));
                            else if (   strSrcTest.startsWith("{")
                                     && strSrcTest.endsWith("}"))
                            {
                                strSrcTest = strSrcTest.substr(1, strSrcTest.length() - 2);
                                if (isValidGuid(strSrcTest))
                                    strNewName = Utf8StrFmt("%s%s", newId.toStringCurly().c_str(), RTPathExt(strNewName.c_str()));
                            }
                            else
                                strNewName = Utf8StrFmt("%s-disk%d%s", trgMCF.machineUserData.strName.c_str(), ++cDisks, RTPathExt(Utf8Str(bstrSrcName).c_str()));
                        }

                        /* Check if this medium comes from the snapshot folder, if
                         * so, put it there in the cloned machine as well.
                         * Otherwise it goes to the machine folder. */
                        Bstr bstrSrcPath;
                        Utf8Str strFile = Utf8StrFmt("%s%c%s", strTrgMachineFolder.c_str(), RTPATH_DELIMITER, strNewName.c_str());
                        rc = pMedium->COMGETTER(Location)(bstrSrcPath.asOutParam());
                        if (FAILED(rc)) throw rc;
                        if (   !bstrSrcPath.isEmpty()
                            &&  RTPathStartsWith(Utf8Str(bstrSrcPath).c_str(), Utf8Str(bstrSrcSnapshotFolder).c_str())
                            && !(fKeepDiskNames || mt.strBaseName.isEmpty()))
                            strFile = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER, strNewName.c_str());

                        /* Start creating the clone. */
                        ComObjPtr<Medium> pTarget;
                        rc = pTarget.createObject();
                        if (FAILED(rc)) throw rc;

                        rc = pTarget->init(p->mParent,
                                           Utf8Str(bstrSrcFormat),
                                           strFile,
                                           Guid::Empty,  /* empty media registry */
                                           NULL          /* llRegistriesThatNeedSaving */);
                        if (FAILED(rc)) throw rc;

                        /* Update the new uuid. */
                        pTarget->updateId(newId);

                        srcLock.release();
                        /* Do the disk cloning. */
                        ComPtr<IProgress> progress2;
                        rc = pMedium->CloneTo(pTarget,
                                              srcVar,
                                              pNewParent,
                                              progress2.asOutParam());
                        if (FAILED(rc)) throw rc;

                        /* Wait until the async process has finished. */
                        rc = d->pProgress->WaitForAsyncProgressCompletion(progress2);
                        srcLock.acquire();
                        if (FAILED(rc)) throw rc;

                        /* Check the result of the async process. */
                        LONG iRc;
                        rc = progress2->COMGETTER(ResultCode)(&iRc);
                        if (FAILED(rc)) throw rc;
                        if (FAILED(iRc))
                        {
                            /* If the thread of the progress object has an error, then
                             * retrieve the error info from there, or it'll be lost. */
                            ProgressErrorInfo info(progress2);
                            throw p->setError(iRc, Utf8Str(info.getText()).c_str());
                        }
                        /* Remember created medium. */
                        newMedia.append(pTarget);
                        /* Get the medium type from the source and set it to the
                         * new medium. */
                        MediumType_T type;
                        rc = pMedium->COMGETTER(Type)(&type);
                        if (FAILED(rc)) throw rc;
                        rc = pTarget->COMSETTER(Type)(type);
                        if (FAILED(rc)) throw rc;
                        map.insert(TStrMediumPair(Utf8Str(bstrSrcId), pTarget));
                        /* register the new harddisk */
                        {
                            AutoWriteLock tlock(p->mParent->getMediaTreeLockHandle() COMMA_LOCKVAL_SRC_POS);
                            rc = p->mParent->registerHardDisk(pTarget, NULL /* pllRegistriesThatNeedSaving */);
                            if (FAILED(rc)) throw rc;
                        }
                        /* This medium becomes the parent of the next medium in the
                         * chain. */
                        pNewParent = pTarget;
                    }
                }
            }

            /* Create diffs for the last image chain. */
            if (mtc.fCreateDiffs)
            {
                const MEDIUMTASK &mt = mtc.chain.first();
                ComPtr<IMedium> pMedium = mt.pMedium;
                IMedium *pTmp = pMedium;
                ComObjPtr<Medium> pLMedium = static_cast<Medium*>(pTmp);
                if (pLMedium.isNull())
                    throw E_POINTER;
                ComObjPtr<Medium> pBase = pLMedium->getBase();
                if (pBase->isReadOnly())
                {
                    ComObjPtr<Medium> pDiff;
                    rc = createDiffHelper(pNewParent, strTrgSnapshotFolder,
                                          &newMedia, &pDiff);
                    if (FAILED(rc)) throw rc;
                    /* diff image has to be used... */
                    pNewParent = pDiff;
                }
                else
                {
                    /* Attach the medium directly, as its type is not
                     * subject to diff creation. */
                    newMedia.append(pNewParent);
                }
            }
            Bstr bstrSrcId;
            rc = mtc.chain.first().pMedium->COMGETTER(Id)(bstrSrcId.asOutParam());
            if (FAILED(rc)) throw rc;
            Bstr bstrTrgId;
            rc = pNewParent->COMGETTER(Id)(bstrTrgId.asOutParam());
            if (FAILED(rc)) throw rc;
            /* We have to patch the configuration, so it contains the new
             * medium uuid instead of the old one. */
            d->updateStorageLists(trgMCF.storageMachine.llStorageControllers, bstrSrcId, bstrTrgId);
            d->updateSnapshotStorageLists(trgMCF.llFirstSnapshot, bstrSrcId, bstrTrgId);
        }
        /* Make sure all disks know of the new machine uuid. We do this last to
         * be able to change the medium type above. */
        for (size_t i = newMedia.size(); i > 0; --i)
        {
            const ComObjPtr<Medium> &pMedium = newMedia.at(i - 1);
            AutoCaller mac(pMedium);
            if (FAILED(mac.rc())) throw mac.rc();
            AutoWriteLock mlock(pMedium COMMA_LOCKVAL_SRC_POS);
            Guid uuid = d->pTrgMachine->mData->mUuid;
            if (d->options.contains(CloneOptions_Link))
            {
                ComObjPtr<Medium> pParent = pMedium->getParent();
                mlock.release();
                if (!pParent.isNull())
                {
                    AutoCaller mac2(pParent);
                    if (FAILED(mac2.rc())) throw mac2.rc();
                    AutoReadLock mlock2(pParent COMMA_LOCKVAL_SRC_POS);
                    if (pParent->getFirstRegistryMachineId(uuid))
                        VirtualBox::addGuidToListUniquely(llRegistriesThatNeedSaving, uuid);
                }
                mlock.acquire();
            }
            pMedium->addRegistry(uuid, false /* fRecurse */);
        }
        /* Check if a snapshot folder is necessary and if so doesn't already
         * exists. */
        if (   !d->llSaveStateFiles.isEmpty()
            && !RTDirExists(strTrgSnapshotFolder.c_str()))
        {
            int vrc = RTDirCreateFullPath(strTrgSnapshotFolder.c_str(), 0777);
            if (RT_FAILURE(vrc))
                throw p->setError(VBOX_E_IPRT_ERROR,
                                  p->tr("Could not create snapshots folder '%s' (%Rrc)"), strTrgSnapshotFolder.c_str(), vrc);
        }
        /* Clone all save state files. */
        for (size_t i = 0; i < d->llSaveStateFiles.size(); ++i)
        {
            SAVESTATETASK sst = d->llSaveStateFiles.at(i);
            const Utf8Str &strTrgSaveState = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER, RTPathFilename(sst.strSaveStateFile.c_str()));

            /* Move to next sub-operation. */
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Copy save state file '%s' ..."), RTPathFilename(sst.strSaveStateFile.c_str())).raw(), sst.uWeight);
            if (FAILED(rc)) throw rc;
            /* Copy the file only if it was not copied already. */
            if (!newFiles.contains(strTrgSaveState.c_str()))
            {
                int vrc = RTFileCopyEx(sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), 0, MachineCloneVMPrivate::copyStateFileProgress, &d->pProgress);
                if (RT_FAILURE(vrc))
                    throw p->setError(VBOX_E_IPRT_ERROR,
                                      p->tr("Could not copy state file '%s' to '%s' (%Rrc)"), sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), vrc);
                newFiles.append(strTrgSaveState);
            }
            /* Update the path in the configuration either for the current
             * machine state or the snapshots. */
            if (sst.snapshotUuid.isEmpty())
                trgMCF.strStateFile = strTrgSaveState;
            else
                d->updateStateFile(trgMCF.llFirstSnapshot, sst.snapshotUuid, strTrgSaveState);
        }

        {
            rc = d->pProgress->SetNextOperation(BstrFmt(p->tr("Create Machine Clone '%s' ..."), trgMCF.machineUserData.strName.c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;
            /* After modifying the new machine config, we can copy the stuff
             * over to the new machine. The machine have to be mutable for
             * this. */
            rc = d->pTrgMachine->checkStateDependency(p->MutableStateDep);
            if (FAILED(rc)) throw rc;
            rc = d->pTrgMachine->loadMachineDataFromSettings(trgMCF,
                                                             &d->pTrgMachine->mData->mUuid);
            if (FAILED(rc)) throw rc;
        }

        /* Now save the new configuration to disk. */
        rc = d->pTrgMachine->SaveSettings();
        if (FAILED(rc)) throw rc;
        trgLock.release();
        if (!llRegistriesThatNeedSaving.empty())
        {
            srcLock.release();
            rc = p->mParent->saveRegistries(llRegistriesThatNeedSaving);
            if (FAILED(rc)) throw rc;
        }
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    MultiResult mrc(rc);
    /* Cleanup on failure (CANCEL also) */
    if (FAILED(rc))
    {
        int vrc = VINF_SUCCESS;
        /* Delete all created files. */
        for (size_t i = 0; i < newFiles.size(); ++i)
        {
            vrc = RTFileDelete(newFiles.at(i).c_str());
            if (RT_FAILURE(vrc))
                mrc = p->setError(VBOX_E_IPRT_ERROR, p->tr("Could not delete file '%s' (%Rrc)"), newFiles.at(i).c_str(), vrc);
        }
        /* Delete all already created medias. (Reverse, cause there could be
         * parent->child relations.) */
        for (size_t i = newMedia.size(); i > 0; --i)
        {
            const ComObjPtr<Medium> &pMedium = newMedia.at(i - 1);
            mrc = pMedium->deleteStorage(NULL /* aProgress */,
                                         true /* aWait */,
                                         NULL /* llRegistriesThatNeedSaving */);
            pMedium->Close();
        }
        /* Delete the snapshot folder when not empty. */
        if (!strTrgSnapshotFolder.isEmpty())
            RTDirRemove(strTrgSnapshotFolder.c_str());
        /* Delete the machine folder when not empty. */
        RTDirRemove(strTrgMachineFolder.c_str());
    }

    return mrc;
}

HRESULT MachineCloneVM::createDiffHelper(const ComObjPtr<Medium> &pParent,
                                         const Utf8Str &strSnapshotFolder,
                                         RTCList< ComObjPtr<Medium> > *pNewMedia,
                                         ComObjPtr<Medium> *ppDiff)
{
    DPTR(MachineCloneVM);
    ComObjPtr<Machine> &p = d->p;
    HRESULT rc = S_OK;

    try
    {
        Bstr bstrSrcId;
        rc = pParent->COMGETTER(Id)(bstrSrcId.asOutParam());
        if (FAILED(rc)) throw rc;
        ComObjPtr<Medium> diff;
        diff.createObject();
        rc = diff->init(p->mParent,
                        pParent->getPreferredDiffFormat(),
                        Utf8StrFmt("%s%c", strSnapshotFolder.c_str(), RTPATH_DELIMITER),
                        Guid::Empty, /* empty media registry */
                        NULL);       /* pllRegistriesThatNeedSaving */
        if (FAILED(rc)) throw rc;
        MediumLockList *pMediumLockList(new MediumLockList());
        rc = diff->createMediumLockList(true /* fFailIfInaccessible */,
                                        true /* fMediumLockWrite */,
                                        pParent,
                                        *pMediumLockList);
        if (FAILED(rc)) throw rc;
        rc = pMediumLockList->Lock();
        if (FAILED(rc)) throw rc;
        /* this already registers the new diff image */
        rc = pParent->createDiffStorage(diff, MediumVariant_Standard,
                                        pMediumLockList,
                                        NULL /* aProgress */,
                                        true /* aWait */,
                                        NULL); // pllRegistriesThatNeedSaving
        delete pMediumLockList;
        if (FAILED(rc)) throw rc;
        /* Remember created medium. */
        pNewMedia->append(diff);
        *ppDiff = diff;
    }
    catch (HRESULT rc2)
    {
        rc = rc2;
    }
    catch (...)
    {
        rc = VirtualBox::handleUnexpectedExceptions(RT_SRC_POS);
    }

    return rc;
}

void MachineCloneVM::destroy()
{
    delete this;
}

