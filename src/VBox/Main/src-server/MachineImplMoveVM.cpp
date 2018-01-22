/* $Id$ */
/** @file
 * Implementation of MachineMoveVM
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <iprt/fs.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <iprt/stream.h>

#include "MachineImplMoveVM.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

typedef std::multimap<Utf8Str, Utf8Str> list_t;
typedef std::multimap<Utf8Str, Utf8Str>::const_iterator cit_t;
typedef std::multimap<Utf8Str, Utf8Str>::iterator it_t;
typedef std::pair <std::multimap<Utf8Str, Utf8Str>::iterator, std::multimap<Utf8Str, Utf8Str>::iterator> rangeRes_t;

struct fileList_t
{
    HRESULT add(const Utf8Str& folder, const Utf8Str& file)
    {
        HRESULT rc = S_OK;
        m_list.insert(std::make_pair(folder, file));
        return rc;
    }

    HRESULT add(const Utf8Str& fullPath)
    {
        HRESULT rc = S_OK;
        Utf8Str folder = fullPath;
        folder.stripFilename();
        Utf8Str filename = fullPath;
        filename.stripPath();
        m_list.insert(std::make_pair(folder, filename));
        return rc;
    }

    HRESULT removeFileFromList(const Utf8Str& fullPath)
    {
        HRESULT rc = S_OK;
        Utf8Str folder = fullPath;
        folder.stripFilename();
        Utf8Str filename = fullPath;
        filename.stripPath();
        rangeRes_t res = m_list.equal_range(folder);
        for (it_t it=res.first; it!=res.second; ++it)
        {
            if (it->second.equals(filename))
                m_list.erase(it);
        }
        return rc;
    }

    HRESULT removeFileFromList(const Utf8Str& path, const Utf8Str& fileName)
    {
        HRESULT rc = S_OK;
        rangeRes_t res = m_list.equal_range(path);
        for (it_t it=res.first; it!=res.second; ++it)
        {
            if (it->second.equals(fileName))
                m_list.erase(it);
        }
        return rc;
    }

    HRESULT removeFolderFromList(const Utf8Str& path)
    {
        HRESULT rc = S_OK;
        m_list.erase(path);
        return rc;
    }

    rangeRes_t getFilesInRange(const Utf8Str& path)
    {
        rangeRes_t res;
        res = m_list.equal_range(path);
        return res;
    }

    std::list<Utf8Str> getFilesInList(const Utf8Str& path)
    {
        std::list<Utf8Str> list_;
        rangeRes_t res = m_list.equal_range(path);
        for (it_t it=res.first; it!=res.second; ++it)
            list_.push_back(it->second);
        return list_;
    }


    list_t m_list;

};

HRESULT MachineMoveVM::init()
{
    HRESULT rc = S_OK;
    Utf8Str strTargetFolder = m_targetPath;

    /*
     * We have a mode which user is able to request
     * basic mode:
     * - The images which are solely attached to the VM
     *   and located in the original VM folder will be moved.
     *
     * Comment: in the future some other modes can be added.
     */

    try
    {
        Utf8Str info;
        int vrc = 0;

        RTFOFF cbTotal = 0;
        RTFOFF cbFree = 0;
        uint32_t cbBlock = 0;
        uint32_t cbSector = 0;

        vrc = RTFsQuerySizes(strTargetFolder.c_str(), &cbTotal, &cbFree, &cbBlock, &cbSector);
        if (FAILED(vrc)) throw vrc;
        long long totalFreeSpace = cbFree;
        long long totalSpace = cbTotal;
        info = Utf8StrFmt("blocks: total %lld, free %u ", cbTotal, cbFree);
        RTPrintf("%s \n", info.c_str());
        RTPrintf("total space (Kb) %lld (Mb) %lld (Gb) %lld\n",
               totalSpace/1024, totalSpace/(1024*1024), totalSpace/(1024*1024*1024));
        RTPrintf("total free space (Kb) %lld (Mb) %lld (Gb) %lld\n",
               totalFreeSpace/1024, totalFreeSpace/(1024*1024), totalFreeSpace/(1024*1024*1024));

        RTFSPROPERTIES properties;
        vrc = RTFsQueryProperties(strTargetFolder.c_str(), &properties);
        if (FAILED(vrc)) throw vrc;
        info = Utf8StrFmt("disk properties:\n"
                          "remote: %s \n"
                          "read only: %s \n"
                          "compressed: %s \n",
                          properties.fRemote == true ? "true":"false",
                          properties.fReadOnly == true ? "true":"false",
                          properties.fCompressed == true ? "true":"false");

        RTPrintf("%s \n", info.c_str());

        /* Get the original VM path */
        Utf8Str strSettingsFilePath;
        Bstr bstr_settingsFilePath;
        m_pMachine->COMGETTER(SettingsFilePath)(bstr_settingsFilePath.asOutParam());
        strSettingsFilePath = bstr_settingsFilePath;
        strSettingsFilePath.stripFilename();

        vmFolders.insert(std::make_pair(VBox_SettingFolder, strSettingsFilePath));

        /* Collect all files from the VM's folder */
        fileList_t fullFileList;
        rc = getFilesList(strSettingsFilePath, fullFileList);
        if (FAILED(rc)) throw rc;

        /*
         * Collect all known folders used by the VM:
         * - log folder;
         * - state folder;
         * - snapshot folder.
         */
        Utf8Str strLogFolder;
        Bstr bstr_logFolder;
        m_pMachine->COMGETTER(LogFolder)(bstr_logFolder.asOutParam());
        strLogFolder = bstr_logFolder;
        if (   m_type.equals("full")
            && strLogFolder.contains(strSettingsFilePath))
        {
            vmFolders.insert(std::make_pair(VBox_LogFolder, strLogFolder));
        }

        Utf8Str strStateFilePath;
        Bstr bstr_stateFilePath;
        MachineState_T machineState;
        rc = m_pMachine->COMGETTER(State)(&machineState);
        if (FAILED(rc)) throw rc;
        else if (machineState == MachineState_Saved)
        {
            m_pMachine->COMGETTER(StateFilePath)(bstr_stateFilePath.asOutParam());
            strStateFilePath = bstr_stateFilePath;
            strStateFilePath.stripFilename();
            if (   m_type.equals("full")
                && strStateFilePath.contains(strSettingsFilePath))
                vmFolders.insert(std::make_pair(VBox_StateFolder, strStateFilePath));//Utf8Str(bstr_stateFilePath)));
        }

        Utf8Str strSnapshotFolder;
        Bstr bstr_snapshotFolder;
        m_pMachine->COMGETTER(SnapshotFolder)(bstr_snapshotFolder.asOutParam());
        strSnapshotFolder = bstr_snapshotFolder;
        if (   m_type.equals("full")
            && strSnapshotFolder.contains(strSettingsFilePath))
            vmFolders.insert(std::make_pair(VBox_SnapshotFolder, strSnapshotFolder));

        if (m_pMachine->i_isSnapshotMachine())
        {
            Bstr bstrSrcMachineId;
            rc = m_pMachine->COMGETTER(Id)(bstrSrcMachineId.asOutParam());
            if (FAILED(rc)) throw rc;
            ComPtr<IMachine> newSrcMachine;
            rc = m_pMachine->i_getVirtualBox()->FindMachine(bstrSrcMachineId.raw(), newSrcMachine.asOutParam());
            if (FAILED(rc)) throw rc;
        }

        /* Add the current machine and all snapshot machines below this machine
         * in a list for further processing.
         */

        long long neededFreeSpace = 0;

        /* Actual file list */
        fileList_t actualFileList;
        Utf8Str strTargetImageName;
        std::vector< ComObjPtr<Machine> > machineList;

//      if (m_pMachine->i_isSnapshotMachine())
//          Guid snapshotId = m_pMachine->i_getSnapshotId();

        /* Include current state? */
        machineList.push_back(m_pMachine);

        {
            ULONG cSnapshots = 0;
            rc = m_pMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            if (FAILED(rc)) throw rc;
            if (cSnapshots > 0)
            {
                Utf8Str id;
                if (m_pMachine->i_isSnapshotMachine())
                    id = m_pMachine->i_getSnapshotId().toString();
                ComPtr<ISnapshot> pSnapshot;
                rc = m_pMachine->FindSnapshot(Bstr(id).raw(), pSnapshot.asOutParam());
                if (FAILED(rc)) throw rc;
                rc = createMachineList(pSnapshot, machineList);
                if (FAILED(rc)) throw rc;
            }
        }

        ULONG uCount       = 2; /* One init task and the machine creation. */
        ULONG uTotalWeight = 2; /* The init task and the machine creation is worth one. */

        queryMediasForAllStates(machineList, uCount, uTotalWeight);

        {
            uint64_t totalMediumsSize = 0;

            for (size_t i = 0; i < llMedias.size(); ++i)
            {
                LONG64  cbSize = 0;
                MEDIUMTASKCHAIN &mtc = llMedias.at(i);
                for (size_t a = mtc.chain.size(); a > 0; --a)
                {
                    Bstr bstrLocation;
                    Utf8Str strLocation;
                    Utf8Str name = mtc.chain[a - 1].strBaseName;
                    ComPtr<IMedium> plMedium = mtc.chain[a - 1].pMedium;
                    rc = plMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                    if (FAILED(rc)) throw rc;
                        strLocation = bstrLocation;

                    /*if an image is located in the actual VM folder it will be added to the actual list */
                    if (strLocation.contains(strSettingsFilePath))
                    {
                        rc = plMedium->COMGETTER(Size)(&cbSize);
                        if (FAILED(rc)) throw rc;

                        totalMediumsSize += cbSize;

                        std::pair<std::map<Utf8Str, MEDIUMTASK>::iterator,bool> ret;
                        ret = finalMediumsMap.insert(std::make_pair(name, mtc.chain[a - 1]));
                        if (ret.second == true)
                            RTPrintf("Image %s was added into the moved list\n", name.c_str());
                    }
                }
            }
            neededFreeSpace += totalMediumsSize;
        }

        /* Prepare data for moving ".sav" files */
        {
            uint64_t totalStateSize = 0;

            for (size_t i = 0; i < llSaveStateFiles.size(); ++i)
            {
                uint64_t cbFile = 0;
                SAVESTATETASK &sst = llSaveStateFiles.at(i);

                Utf8Str name = sst.strSaveStateFile;
                /*if a state file is located in the actual VM folder it will be added to the actual list */
                if (name.contains(strSettingsFilePath))
                {
                    vrc = RTFileQuerySize(name.c_str(), &cbFile);
                    if (RT_SUCCESS(vrc))
                    {
                        totalStateSize += cbFile;
                    }

                    std::pair<std::map<Utf8Str, SAVESTATETASK>::iterator,bool> ret;
                    ret = finalSaveStateFilesMap.insert(std::make_pair(name, sst));
                    if (ret.second == true)
                    {
                        uCount       += 1;
                        uTotalWeight += 1;//just for now (should be correctly evaluated according its size)
                        RTPrintf("State file %s was added into the moved list\n", name.c_str());
                    }
                }
            }
            neededFreeSpace += totalStateSize;
        }

        /* Prepare data for moving the log files */
        {
            Utf8Str strFolder = vmFolders[VBox_LogFolder];
            if (strFolder.isNotEmpty())
            {
                uint64_t totalLogSize = 0;
                rc = getFolderSize(strFolder, totalLogSize);
                if (SUCCEEDED(rc))
                {
                    neededFreeSpace += totalLogSize;
                    if (totalFreeSpace - neededFreeSpace <= 1024*1024)
                    {
                        throw VERR_OUT_OF_RESOURCES;//less than 1Mb free space on the target location
                    }

                    fileList_t filesList;
                    getFilesList(strFolder, filesList);
                    cit_t it = filesList.m_list.begin();
                    while(it != filesList.m_list.end())
                    {
                        Utf8Str strFile = it->first.c_str();
                        strFile.append(RTPATH_DELIMITER).append(it->second.c_str());
                        RTPrintf("%s\n", strFile.c_str());
                        actualFileList.add(strFile);

                        uCount       += 1;
                        uTotalWeight += 1;//just for now (should be correctly evaluated according its size)

                        RTPrintf("The log file %s added into the moved list\n", strFile.c_str());
                        ++it;
                    }
                }
            }
        }

        /* Check a target location on enough room */
        if (totalFreeSpace - neededFreeSpace <= 1024*1024)
        {
            throw VERR_OUT_OF_RESOURCES;//less than 1Mb free space on the target location
        }

        /* Init both Progress instances */
        {
            rc = m_pProgress->init(m_pMachine->i_getVirtualBox(),
                                 static_cast<IMachine*>(m_pMachine) /* aInitiator */,
                                 Bstr(m_pMachine->tr("Moving Machine")).raw(),
                                 true /* fCancellable */,
                                 uCount,
                                 uTotalWeight,
                                 Bstr(m_pMachine->tr("Initialize Moving")).raw(),
                                 1);
            if (FAILED(rc))
            {
                throw m_pMachine->setError(VBOX_E_IPRT_ERROR,
                                     m_pMachine->tr("Couldn't correctly setup the progress object "
                                                    "for moving VM operation (%Rrc)"),
                                     rc);
            }

            m_pRollBackProgress.createObject();
            rc = m_pRollBackProgress->init(m_pMachine->i_getVirtualBox(),
                                 static_cast<IMachine*>(m_pMachine) /* aInitiator */,
                                 Bstr(m_pMachine->tr("Moving back Machine")).raw(),
                                 true /* fCancellable */,
                                 uCount,
                                 uTotalWeight,
                                 Bstr(m_pMachine->tr("Initialize Moving back")).raw(),
                                 1);
            if (FAILED(rc))
            {
                throw m_pMachine->setError(VBOX_E_IPRT_ERROR,
                                     m_pMachine->tr("Couldn't correctly setup the progress object "
                                                    "for possible rollback operation during moving VM (%Rrc)"),
                                     rc);
            }
        }

        /* save all VM data */
        m_pMachine->i_setModified(Machine::IsModified_MachineData);
        rc = m_pMachine->SaveSettings();
    }
    catch(HRESULT hrc)
    {
        rc = hrc;
    }

    LogFlowFuncLeave();

    return rc;
}

void MachineMoveVM::updateStateFile(settings::MachineConfigFile *snl, const Guid &id, const Utf8Str &strFile)
{
    settings::SnapshotsList::iterator it;
    for (it = (*snl).llFirstSnapshot.begin(); it != (*snl).llFirstSnapshot.end(); ++it)
    {
        settings::Snapshot snap = (settings::Snapshot)(*it);
        RTPrintf("it->uuid = %s , id = %s\n", snap.uuid.toStringCurly().c_str(), id.toStringCurly().c_str());
        RTPrintf("it->strStateFile = %s , strFile = %s\n", it->strStateFile.c_str(), strFile.c_str());
        if (it->uuid == id)
        {
            it->strStateFile = strFile;
            RTPrintf("Modified it->strStateFile = %s\n", it->strStateFile.c_str());
        }
        else if (!it->llChildSnapshots.empty())
            updateStateFile(it->llChildSnapshots, id, strFile);
    }
}

void MachineMoveVM::updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile)
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (it->uuid == id)
        {
            settings::Snapshot snap = (settings::Snapshot)(*it);
            RTPrintf("it->uuid = %s , id = %s\n", snap.uuid.toStringCurly().c_str(), id.toStringCurly().c_str());
            RTPrintf("it->strStateFile = %s , strFile = %s\n", snap.strStateFile.c_str(), strFile.c_str());
            it->strStateFile = strFile;

            RTPrintf("Modified it->strStateFile = %s\n", it->strStateFile.c_str());
            m_pMachine->mSSData->strStateFilePath = strFile;
            RTPrintf("Modified m_pMachine->mSSData->strStateFilePath = %s\n",
                   m_pMachine->mSSData->strStateFilePath.c_str());
        }
        else if (!it->llChildSnapshots.empty())
            updateStateFile(it->llChildSnapshots, id, strFile);
    }
}

void MachineMoveVM::printStateFile(settings::SnapshotsList &snl)
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (!it->strStateFile.isEmpty())
        {
            settings::Snapshot snap = (settings::Snapshot)(*it);
            RTPrintf("snap.uuid = %s\n", snap.uuid.toStringCurly().c_str());
            RTPrintf("snap.strStateFile = %s\n", snap.strStateFile.c_str());
        }

        if (!it->llChildSnapshots.empty())
            printStateFile(it->llChildSnapshots);
    }
}

/* static */
DECLCALLBACK(int) MachineMoveVM::updateProgress(unsigned uPercent, void *pvUser)
{
    MachineMoveVM* pTask = *(MachineMoveVM**)pvUser;

    if (    pTask
         && !pTask->m_pProgress.isNull())
    {
        BOOL fCanceled;
        pTask->m_pProgress->COMGETTER(Canceled)(&fCanceled);
        if (fCanceled)
            return -1;
        pTask->m_pProgress->SetCurrentOperationProgress(uPercent);
    }
    return VINF_SUCCESS;
}

/* static */
DECLCALLBACK(int) MachineMoveVM::copyFileProgress(unsigned uPercentage, void *pvUser)
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


/* static */
void MachineMoveVM::i_MoveVMThreadTask(MachineMoveVM* task)
{
    LogFlowFuncEnter();
    HRESULT rc = S_OK;

    MachineMoveVM* taskMoveVM = task;
    ComObjPtr<Machine> &machine = taskMoveVM->m_pMachine;

    AutoCaller autoCaller(machine);
//  if (FAILED(autoCaller.rc())) return;//Should we return something here?

    Utf8Str strTargetFolder = taskMoveVM->m_targetPath;
    {
        Bstr bstrMachineName;
        taskMoveVM->m_pMachine->COMGETTER(Name)(bstrMachineName.asOutParam());
        strTargetFolder.append(Utf8Str(bstrMachineName));
    }

    RTCList<Utf8Str> newFiles;       /* All extra created files (save states, ...) */
    RTCList<Utf8Str> originalFiles;  /* All original files except images */
    typedef std::map<Utf8Str, ComObjPtr<Medium> > MediumMap;
    MediumMap mapOriginalMedium;

    /*
     * We have the couple modes which user is able to request
     * basic mode:
     * - The images which are solely attached to the VM
     *   and located in the original VM folder will be moved.
     *
     * full mode:
     * - All disks which are directly attached to the VM
     *   will be moved.
     *
     * Apart from that all other files located in the original VM
     * folder will be moved.
     */
     /* Collect the shareable disks.
     * Get the machines whom the shareable disks attach to.
     * Return an error if the state of any VM doesn't allow to move a shareable disk.
     * Check new destination whether enough room for the VM or not. if "not" return an error.
     * Make a copy of VM settings and a list with all files which are moved. Save the list on the disk.
     * Start "move" operation.
     * Check the result of operation.
     * if the operation was successful:
     * - delete all files in the original VM folder;
     * - update VM disks info with new location;
     * - update all other VM if it's needed;
     * - update global settings
     */

    try
    {
        /* Move all disks */
        rc = taskMoveVM->moveAllDisks(taskMoveVM->finalMediumsMap, &strTargetFolder);
        if (FAILED(rc))
            throw rc;

        {
            RTPrintf("0 Print all state files\n");
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Copy all save state files. */
        Utf8Str strTrgSnapshotFolder;
        {
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();
            settings::MachineConfigFile *machineConfFile = machineData->pMachineConfigFile;

            /* When the current snapshot folder is absolute we reset it to the
             * default relative folder. */
            if (RTPathStartsWithRoot((*machineConfFile).machineUserData.strSnapshotFolder.c_str()))
                (*machineConfFile).machineUserData.strSnapshotFolder = "Snapshots";
            (*machineConfFile).strStateFile = "";

            /* The absolute name of the snapshot folder. */
            strTrgSnapshotFolder = Utf8StrFmt("%s%c%s", strTargetFolder.c_str(), RTPATH_DELIMITER,
                                              (*machineConfFile).machineUserData.strSnapshotFolder.c_str());

            /* Check if a snapshot folder is necessary and if so doesn't already
             * exists. */
            if (   taskMoveVM->finalSaveStateFilesMap.size() != 0
                && !RTDirExists(strTrgSnapshotFolder.c_str()))
            {
                int vrc = RTDirCreateFullPath(strTrgSnapshotFolder.c_str(), 0700);
                if (RT_FAILURE(vrc))
                    throw machine->setError(VBOX_E_IPRT_ERROR,
                                      machine->tr("Could not create snapshots folder '%s' (%Rrc)"),
                                            strTrgSnapshotFolder.c_str(), vrc);
            }

            std::map<Utf8Str, SAVESTATETASK>::iterator itState = taskMoveVM->finalSaveStateFilesMap.begin();
            while (itState != taskMoveVM->finalSaveStateFilesMap.end())
            {
                const SAVESTATETASK &sst = itState->second;
                const Utf8Str &strTrgSaveState = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER,
                                                            RTPathFilename(sst.strSaveStateFile.c_str()));

                /* Move to next sub-operation. */
                rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Move save state file '%s' ..."),
                                                            RTPathFilename(sst.strSaveStateFile.c_str())).raw(), sst.uWeight);
                if (FAILED(rc)) throw rc;

                int vrc = RTFileCopyEx(sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), 0,
                                       MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
                if (RT_FAILURE(vrc))
                    throw machine->setError(VBOX_E_IPRT_ERROR,
                                      machine->tr("Could not move state file '%s' to '%s' (%Rrc)"),
                                            sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), vrc);

                /* save new file in case of restoring */
                newFiles.append(strTrgSaveState);
                /* save original file for deletion in the end */
                originalFiles.append(sst.strSaveStateFile);
                ++itState;
            }
        }

        /* Update XML settings. */
        rc = taskMoveVM->updateStateFilesXMLSettings(taskMoveVM->finalSaveStateFilesMap, strTrgSnapshotFolder);
        if (FAILED(rc))
            throw rc;

        {
            RTPrintf("\n1 Print all state files\n");
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Moving Machine settings file */
        {
            RTPrintf("\nMoving Machine settings file \n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();
            settings::MachineConfigFile *machineConfFile = machineData->pMachineConfigFile;

            rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Moving Machine settings '%s' ..."),
                                                (*machineConfFile).machineUserData.strName.c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;

            Utf8Str strTargetSettingsFilePath = strTargetFolder;
            Bstr bstrMachineName;
            taskMoveVM->m_pMachine->COMGETTER(Name)(bstrMachineName.asOutParam());
            strTargetSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName)).append(".vbox");

            Utf8Str strSettingsFilePath;
            Bstr bstr_settingsFilePath;
            taskMoveVM->m_pMachine->COMGETTER(SettingsFilePath)(bstr_settingsFilePath.asOutParam());
            strSettingsFilePath = bstr_settingsFilePath;

            int vrc = RTFileCopyEx(strSettingsFilePath.c_str(), strTargetSettingsFilePath.c_str(), 0,
                                   MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
            if (RT_FAILURE(vrc))
                throw machine->setError(VBOX_E_IPRT_ERROR,
                                        machine->tr("Could not move setting file '%s' to '%s' (%Rrc)"),
                                        strSettingsFilePath.c_str(), strTargetSettingsFilePath.c_str(), vrc);

            /* save new file in case of restoring */
            newFiles.append(strTargetSettingsFilePath);
            /* save original file for deletion in the end */
            originalFiles.append(strSettingsFilePath);
        }

        {
            RTPrintf("\n2 Print all state files\n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();
            RTPrintf("\nmachineData->m_strConfigFileFull = %s\n", machineData->m_strConfigFileFull.c_str());
            RTPrintf("\nmachineData->m_strConfigFile = %s\n", machineData->m_strConfigFile.c_str());
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Moving Machine log files */
        {
            RTPrintf("\nMoving Machine log files \n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();
            settings::MachineConfigFile *machineConfFile = machineData->pMachineConfigFile;

            Utf8Str strTargetLogFolderPath = strTargetFolder;

            if (taskMoveVM->vmFolders[VBox_LogFolder].isNotEmpty())
            {
                Bstr bstrMachineName;
                taskMoveVM->m_pMachine->COMGETTER(Name)(bstrMachineName.asOutParam());
                strTargetLogFolderPath.append(RTPATH_DELIMITER).append("Logs");

                /* Check a log folder existing and create one if it's not */
                if (!RTDirExists(strTargetLogFolderPath.c_str()))
                {
                    int vrc = RTDirCreateFullPath(strTargetLogFolderPath.c_str(), 0700);
                    if (RT_FAILURE(vrc))
                        throw machine->setError(VBOX_E_IPRT_ERROR,
                                          machine->tr("Could not create log folder '%s' (%Rrc)"),
                                                strTargetLogFolderPath.c_str(), vrc);
                }

                fileList_t filesList;
                taskMoveVM->getFilesList(taskMoveVM->vmFolders[VBox_LogFolder], filesList);
                cit_t it = filesList.m_list.begin();
                while(it != filesList.m_list.end())
                {
                    Utf8Str strFullSourceFilePath = it->first.c_str();
                    strFullSourceFilePath.append(RTPATH_DELIMITER).append(it->second.c_str());

                    Utf8Str strFullTargetFilePath = strTargetLogFolderPath;
                    strFullTargetFilePath.append(RTPATH_DELIMITER).append(it->second.c_str());

                    /* Move to next sub-operation. */
                    rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Moving Machine log file '%s' ..."),
                                                            RTPathFilename(strFullSourceFilePath.c_str())).raw(), 1);
                    if (FAILED(rc)) throw rc;

                    int vrc = RTFileCopyEx(strFullSourceFilePath.c_str(), strFullTargetFilePath.c_str(), 0,
                                   MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
                    if (RT_FAILURE(vrc))
                        throw machine->setError(VBOX_E_IPRT_ERROR,
                                        machine->tr("Could not move log file '%s' to '%s' (%Rrc)"),
                                        strFullSourceFilePath.c_str(), strFullTargetFilePath.c_str(), vrc);

                    RTPrintf("The log file %s moved into the folder %s\n", strFullSourceFilePath.c_str(),
                             strFullTargetFilePath.c_str());

                    /* save new file in case of restoring */
                    newFiles.append(strFullTargetFilePath);
                    /* save original file for deletion in the end */
                    originalFiles.append(strFullSourceFilePath);

                    ++it;
                }
            }
        }

        /* save all VM data */
        {
            RTPrintf("\nCall Machine::SaveSettings()\n");
            rc = taskMoveVM->m_pMachine->SaveSettings();
        }

        {
            RTPrintf("\n3 Print all state files\n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();

            Utf8Str strTargetSettingsFilePath = strTargetFolder;
            Bstr bstrMachineName;
            taskMoveVM->m_pMachine->COMGETTER(Name)(bstrMachineName.asOutParam());
            strTargetSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName)).append(".vbox");
            machineData->m_strConfigFileFull = strTargetSettingsFilePath;
            taskMoveVM->m_pMachine->mParent->i_copyPathRelativeToConfig(strTargetSettingsFilePath, machineData->m_strConfigFile);
//          machineData->m_strConfigFile = strTargetSettingsFilePath;
            RTPrintf("\nmachineData->m_strConfigFileFull = %s\n", machineData->m_strConfigFileFull.c_str());
            RTPrintf("\nmachineData->m_strConfigFile = %s\n", machineData->m_strConfigFile.c_str());
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Marks the global registry for uuid as modified */
        {
            Guid uuid = taskMoveVM->m_pMachine->mData->mUuid;
            taskMoveVM->m_pMachine->mParent->i_markRegistryModified(uuid);

            // save the global settings; for that we should hold only the VirtualBox lock
            AutoWriteLock vboxLock(taskMoveVM->m_pMachine->mParent COMMA_LOCKVAL_SRC_POS);
            RTPrintf("\nCall global VirtualBox i_saveSettings()\n");
            rc = taskMoveVM->m_pMachine->mParent->i_saveSettings();
        }

        {
            RTPrintf("\n4 Print all state files\n");
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

    }
    catch(HRESULT hrc)
    {
        RTPrintf("\nHRESULT exception\n");
        rc = hrc;
        taskMoveVM->result = rc;
    }
    catch (...)
    {
        RTPrintf("\nUnknown exception\n");
        rc = VirtualBoxBase::handleUnexpectedExceptions(taskMoveVM->m_pMachine, RT_SRC_POS);
        taskMoveVM->result = rc;
    }

    /* Cleanup on failure */
    if (FAILED(rc))
    {
        /* ! Apparently we should update the Progress object !*/

        /* Restore the original XML settings. */
        rc = taskMoveVM->updateStateFilesXMLSettings(taskMoveVM->finalSaveStateFilesMap,
                                                     taskMoveVM->vmFolders[VBox_StateFolder]);
        if (FAILED(rc))
            throw rc;

        {
            RTPrintf("\nFailed: Print all state files\n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();
            RTPrintf("\nmachineData->m_strConfigFileFull = %s\n", machineData->m_strConfigFileFull.c_str());
            RTPrintf("\nmachineData->m_strConfigFile = %s\n", machineData->m_strConfigFile.c_str());
            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Delete all created files. */
        rc = taskMoveVM->deleteFiles(newFiles);
        if (FAILED(rc))
            RTPrintf("Can't delete all new files.");

        RTDirRemove(strTargetFolder.c_str());

        /* Restoring the original mediums */
        try
        {
            rc = taskMoveVM->moveAllDisks(taskMoveVM->finalMediumsMap);
            if (FAILED(rc))
                throw rc;
        }
        catch(HRESULT hrc)
        {
            RTPrintf("\nFailed: HRESULT exception\n");
            taskMoveVM->result = hrc;
        }
        catch (...)
        {
            RTPrintf("\nFailed: Unknown exception\n");
            rc = VirtualBoxBase::handleUnexpectedExceptions(taskMoveVM->m_pMachine, RT_SRC_POS);
            taskMoveVM->result = rc;
        }

        /* save all VM data */
        {
            AutoWriteLock  srcLock(machine COMMA_LOCKVAL_SRC_POS);
            srcLock.release();
            RTPrintf("\nFailed: Call SaveSettings()\n");
            rc = taskMoveVM->m_pMachine->SaveSettings();
            srcLock.acquire();
        }

        /* Restore an original path to XML setting file */
        {
            RTPrintf("\nFailed: Print all state files\n");
            Machine::Data *machineData = taskMoveVM->m_pMachine->mData.data();

            Utf8Str strOriginalSettingsFilePath = taskMoveVM->vmFolders[VBox_SettingFolder];
            Bstr bstrMachineName;
            taskMoveVM->m_pMachine->COMGETTER(Name)(bstrMachineName.asOutParam());
            strOriginalSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName)).append(".vbox");

            machineData->m_strConfigFileFull = strOriginalSettingsFilePath;

            taskMoveVM->m_pMachine->mParent->i_copyPathRelativeToConfig(strOriginalSettingsFilePath,
                                                                        machineData->m_strConfigFile);

            RTPrintf("\nmachineData->m_strConfigFileFull = %s\n", machineData->m_strConfigFileFull.c_str());
            RTPrintf("\nmachineData->m_strConfigFile = %s\n", machineData->m_strConfigFile.c_str());

            taskMoveVM->printStateFile(taskMoveVM->m_pMachine->mData->pMachineConfigFile->llFirstSnapshot);
        }

        /* Marks the global registry for uuid as modified */
        {
            AutoWriteLock  srcLock(machine COMMA_LOCKVAL_SRC_POS);
            srcLock.release();
            Guid uuid = taskMoveVM->m_pMachine->mData->mUuid;
            taskMoveVM->m_pMachine->mParent->i_markRegistryModified(uuid);
            srcLock.acquire();

            // save the global settings; for that we should hold only the VirtualBox lock
            AutoWriteLock vboxLock(taskMoveVM->m_pMachine->mParent COMMA_LOCKVAL_SRC_POS);
            RTPrintf("\nFailed: Call global VirtualBox i_saveSettings()\n");
            rc = taskMoveVM->m_pMachine->mParent->i_saveSettings();
        }
    }
    else /*Operation was successful and now we can delete the original files like the state files, XML setting, log files */
    {
        rc = taskMoveVM->deleteFiles(originalFiles);
        if (FAILED(rc))
            RTPrintf("Can't delete all original files.");
    }

    if (!taskMoveVM->m_pProgress.isNull())
        taskMoveVM->m_pProgress->i_notifyComplete(taskMoveVM->result);

    LogFlowFuncLeave();
}

HRESULT MachineMoveVM::moveAllDisks(const std::map<Utf8Str, MEDIUMTASK>& listOfDisks,
                                    const Utf8Str* strTargetFolder)
{
    HRESULT rc = S_OK;
    ComObjPtr<Machine> &machine = m_pMachine;

    AutoWriteLock  machineLock(machine COMMA_LOCKVAL_SRC_POS);

    try{
        std::map<Utf8Str, MEDIUMTASK>::const_iterator itMedium = listOfDisks.begin();
        while(itMedium != listOfDisks.end())
        {
            const MEDIUMTASK &mt = itMedium->second;
            ComPtr<IMedium> pMedium = mt.pMedium;
            Utf8Str strTargetImageName;
            Utf8Str strLocation;
            Bstr bstrLocation;
            Bstr bstrSrcName;

            rc = pMedium->COMGETTER(Name)(bstrSrcName.asOutParam());
            if (FAILED(rc)) throw rc;

            if (strTargetFolder != NULL && !strTargetFolder->isEmpty())
            {
                strTargetImageName = *strTargetFolder;
                rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;
                strLocation = bstrLocation;

                if (mt.fSnapshot == true)
                {
                    strLocation.stripFilename().stripPath().append(RTPATH_DELIMITER).append(Utf8Str(bstrSrcName));
                }
                else
                {
                    strLocation.stripPath();
                }

                strTargetImageName.append(RTPATH_DELIMITER).append(strLocation);
                rc = m_pProgress->SetNextOperation(BstrFmt(machine->tr("Moving Disk '%ls' ..."),
                                                       bstrSrcName.raw()).raw(),
                                                       mt.uWeight);
                if (FAILED(rc)) throw rc;
            }
            else
            {
                strTargetImageName = mt.strBaseName;//Should contain full path to the image
                rc = m_pRollBackProgress->SetNextOperation(BstrFmt(machine->tr("Moving back Disk '%ls' ..."),
                                                       bstrSrcName.raw()).raw(),
                                                       mt.uWeight);
                if (FAILED(rc)) throw rc;
            }



            /* consistency: use \ if appropriate on the platform */
            RTPathChangeToDosSlashes(strTargetImageName.mutableRaw(), false);
            RTPrintf("\nTarget disk %s \n", strTargetImageName.c_str());

            ComPtr<IProgress> moveDiskProgress;
            bstrLocation = strTargetImageName.c_str();
            rc = pMedium->SetLocation(bstrLocation.raw(), moveDiskProgress.asOutParam());

            /* Wait until the async process has finished. */
            machineLock.release();
            if (strTargetFolder != NULL && !strTargetFolder->isEmpty())
            {
                rc = m_pProgress->WaitForAsyncProgressCompletion(moveDiskProgress);
            }
            else
            {
                rc = m_pRollBackProgress->WaitForAsyncProgressCompletion(moveDiskProgress);
            }

            machineLock.acquire();
            if (FAILED(rc)) throw rc;

            RTPrintf("\nMoving %s has finished\n", strTargetImageName.c_str());

            /* Check the result of the async process. */
            LONG iRc;
            rc = moveDiskProgress->COMGETTER(ResultCode)(&iRc);
            if (FAILED(rc)) throw rc;
            /* If the thread of the progress object has an error, then
             * retrieve the error info from there, or it'll be lost. */
            if (FAILED(iRc))
                throw machine->setError(ProgressErrorInfo(moveDiskProgress));

            ++itMedium;
        }

        machineLock.release();
    }
    catch(HRESULT hrc)
    {
        RTPrintf("\nHRESULT exception\n");
        rc = hrc;
        machineLock.release();
    }
    catch (...)
    {
        RTPrintf("\nUnknown exception\n");
        rc = VirtualBoxBase::handleUnexpectedExceptions(m_pMachine, RT_SRC_POS);
        machineLock.release();
    }

    return rc;
}

HRESULT MachineMoveVM::updateStateFilesXMLSettings(const std::map<Utf8Str, SAVESTATETASK>& listOfFiles,
                                                    const Utf8Str& targetPath)
{
    HRESULT rc = S_OK;
    Machine::Data *machineData = m_pMachine->mData.data();
    settings::MachineConfigFile *machineConfFile = machineData->pMachineConfigFile;

    std::map<Utf8Str, SAVESTATETASK>::const_iterator itState = listOfFiles.begin();
    while (itState != listOfFiles.end())
    {
        const SAVESTATETASK &sst = itState->second;
        const Utf8Str &strTargetSaveStateFilePath = Utf8StrFmt("%s%c%s", targetPath.c_str(),
                                                           RTPATH_DELIMITER,
                                                           RTPathFilename(sst.strSaveStateFile.c_str()));

        /* Update the path in the configuration either for the current
         * machine state or the snapshots. */
        if (!sst.snapshotUuid.isValid() || sst.snapshotUuid.isZero())
        {
            (*machineConfFile).strStateFile = strTargetSaveStateFilePath;
        }
        else
        {
            updateStateFile(m_pMachine->mData->pMachineConfigFile->llFirstSnapshot,
                            sst.snapshotUuid, strTargetSaveStateFilePath);
        }

        ++itState;
    }

    return rc;
}

HRESULT MachineMoveVM::getFilesList(const Utf8Str& strRootFolder, fileList_t &filesList)
{
    RTDIR hDir;
    HRESULT rc = S_OK;
    int vrc = RTDirOpen(&hDir, strRootFolder.c_str());
    if (RT_SUCCESS(vrc))
    {
        RTDIRENTRY DirEntry;
        while (RT_SUCCESS(RTDirRead(hDir, &DirEntry, NULL)))
        {
            if (RTDirEntryIsStdDotLink(&DirEntry))
                continue;

            if (DirEntry.enmType == RTDIRENTRYTYPE_FILE)
            {
                Utf8Str fullPath(strRootFolder);
                filesList.add(strRootFolder, DirEntry.szName);
            }
            else if (DirEntry.enmType == RTDIRENTRYTYPE_DIRECTORY)
            {
                Utf8Str strNextFolder(strRootFolder);
                strNextFolder.append(RTPATH_DELIMITER).append(DirEntry.szName);
                rc = getFilesList(strNextFolder, filesList);
                if (FAILED(rc))
                    break;
            }
        }

        vrc = RTDirClose(hDir);
    }
    else if (vrc == VERR_FILE_NOT_FOUND)
    {
        m_pMachine->setError(VBOX_E_IPRT_ERROR,
                           m_pMachine->tr("Folder '%s' doesn't exist (%Rrc)"),
                           strRootFolder.c_str(), vrc);
        rc = vrc;
    }
    else
        throw m_pMachine->setError(VBOX_E_IPRT_ERROR,
                                 m_pMachine->tr("Could not open folder '%s' (%Rrc)"),
                                 strRootFolder.c_str(), vrc);
    return rc;
}

HRESULT MachineMoveVM::deleteFiles(const RTCList<Utf8Str>& listOfFiles)
{
    HRESULT rc = S_OK;
    /* Delete all created files. */
    try
    {
        for (size_t i = 0; i < listOfFiles.size(); ++i)
        {
            int vrc = RTFileDelete(listOfFiles.at(i).c_str());
            if (RT_FAILURE(vrc))
                rc = m_pMachine->setError(VBOX_E_IPRT_ERROR,
                                                      m_pMachine->tr("Could not delete file '%s' (%Rrc)"),
                                                      listOfFiles.at(i).c_str(), rc);
            else
                RTPrintf("\nFile %s has been deleted\n", listOfFiles.at(i).c_str());
        }
    }
    catch(HRESULT hrc)
    {
        rc = hrc;
    }
    catch (...)
    {
        rc = VirtualBoxBase::handleUnexpectedExceptions(m_pMachine, RT_SRC_POS);
    }

    return rc;
}

HRESULT MachineMoveVM::getFolderSize(const Utf8Str& strRootFolder, uint64_t& size)
{
    int vrc = 0;
    uint64_t totalFolderSize = 0;
    fileList_t filesList;

    HRESULT rc = getFilesList(strRootFolder, filesList);
    if (SUCCEEDED(rc))
    {
        cit_t it = filesList.m_list.begin();
        while(it != filesList.m_list.end())
        {
            uint64_t cbFile = 0;
            Utf8Str fullPath =  it->first;
            fullPath.append(RTPATH_DELIMITER).append(it->second);
            vrc = RTFileQuerySize(fullPath.c_str(), &cbFile);
            if (RT_SUCCESS(vrc))
            {
                totalFolderSize += cbFile;
            }
            else
                throw m_pMachine->setError(VBOX_E_IPRT_ERROR,
                                 m_pMachine->tr("Could not get the size of file '%s' (%Rrc)"),
                                 fullPath.c_str(), vrc);
            ++it;
        }
//      RTPrintf("Total folder %s size %llu\n", strRootFolder.c_str(), totalFolderSize);
        size = totalFolderSize;
    }
    else
        m_pMachine->setError(VBOX_E_IPRT_ERROR,
                           m_pMachine->tr("Could not calculate the size of folder '%s' (%Rrc)"),
                           strRootFolder.c_str(), vrc);
    return rc;
}

HRESULT MachineMoveVM::queryBaseName(const ComPtr<IMedium> &pMedium, Utf8Str &strBaseName) const
{
    ComPtr<IMedium> pBaseMedium;
    HRESULT rc = pMedium->COMGETTER(Base)(pBaseMedium.asOutParam());
    if (FAILED(rc)) return rc;
    Bstr bstrBaseName;
    rc = pBaseMedium->COMGETTER(Name)(bstrBaseName.asOutParam());
    if (FAILED(rc)) return rc;
    strBaseName = bstrBaseName;
    return rc;
}

HRESULT MachineMoveVM::createMachineList(const ComPtr<ISnapshot> &pSnapshot,
                                         std::vector< ComObjPtr<Machine> > &aMachineList) const
{
    HRESULT rc = S_OK;
    Bstr name;
    rc = pSnapshot->COMGETTER(Name)(name.asOutParam());
    if (FAILED(rc)) return rc;

    ComPtr<IMachine> l_pMachine;
    rc = pSnapshot->COMGETTER(Machine)(l_pMachine.asOutParam());
    if (FAILED(rc)) return rc;
    aMachineList.push_back((Machine*)(IMachine*)l_pMachine);

    SafeIfaceArray<ISnapshot> sfaChilds;
    rc = pSnapshot->COMGETTER(Children)(ComSafeArrayAsOutParam(sfaChilds));
    if (FAILED(rc)) return rc;
    for (size_t i = 0; i < sfaChilds.size(); ++i)
    {
        rc = createMachineList(sfaChilds[i], aMachineList);
        if (FAILED(rc)) return rc;
    }

    return rc;
}

HRESULT MachineMoveVM::queryMediasForAllStates(const std::vector<ComObjPtr<Machine> > &aMachineList,
                                                       ULONG &uCount, ULONG &uTotalWeight)
{
    /* In this case we create a exact copy of the original VM. This means just
     * adding all directly and indirectly attached disk images to the worker
     * list. */
    HRESULT rc = S_OK;
    for (size_t i = 0; i < aMachineList.size(); ++i)
    {
        const ComObjPtr<Machine> &machine = aMachineList.at(i);

        /* Add all attachments (and their parents) of the different
         * machines to a worker list. */
        SafeIfaceArray<IMediumAttachment> sfaAttachments;
        rc = machine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(sfaAttachments));
        if (FAILED(rc)) return rc;
        for (size_t a = 0; a < sfaAttachments.size(); ++a)
        {
            const ComPtr<IMediumAttachment> &pAtt = sfaAttachments[a];
            DeviceType_T type;
            rc = pAtt->COMGETTER(Type)(&type);
            if (FAILED(rc)) return rc;

            /* Only harddisks and floppies are of interest. */
            if (   type != DeviceType_HardDisk
                && type != DeviceType_Floppy)
                continue;

            /* Valid medium attached? */
            ComPtr<IMedium> pSrcMedium;
            rc = pAtt->COMGETTER(Medium)(pSrcMedium.asOutParam());
            if (FAILED(rc)) return rc;

            if (pSrcMedium.isNull())
                continue;

            MEDIUMTASKCHAIN mtc;
            mtc.devType       = type;

            while (!pSrcMedium.isNull())
            {
                /* Refresh the state so that the file size get read. */
                MediumState_T e;
                rc = pSrcMedium->RefreshState(&e);
                if (FAILED(rc)) return rc;
                LONG64 lSize;
                rc = pSrcMedium->COMGETTER(Size)(&lSize);
                if (FAILED(rc)) return rc;

                Bstr bstrLocation;
                rc = pSrcMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;

                /* Save the current medium, for later cloning. */
                MEDIUMTASK mt;
                mt.strBaseName = bstrLocation;
                Utf8Str strFolder = vmFolders[VBox_SnapshotFolder];
                if (strFolder.isNotEmpty() && mt.strBaseName.contains(strFolder))
                {
                    mt.fSnapshot = true;
                }
                else
                    mt.fSnapshot = false;

                mt.uIdx    = UINT32_MAX;
                mt.pMedium = pSrcMedium;
                mt.uWeight = (ULONG)((lSize + _1M - 1) / _1M);
                mtc.chain.append(mt);

                /* Query next parent. */
                rc = pSrcMedium->COMGETTER(Parent)(pSrcMedium.asOutParam());
                if (FAILED(rc)) return rc;
            }
            /* Update the progress info. */
            updateProgressStats(mtc, uCount, uTotalWeight);
            /* Append the list of images which have  to be moved. */
            llMedias.append(mtc);
        }
        /* Add the save state files of this machine if there is one. */
        rc = addSaveState(machine, uCount, uTotalWeight);
        if (FAILED(rc)) return rc;

    }
    /* Build up the index list of the image chain. Unfortunately we can't do
     * that in the previous loop, cause there we go from child -> parent and
     * didn't know how many are between. */
    for (size_t i = 0; i < llMedias.size(); ++i)
    {
        uint32_t uIdx = 0;
        MEDIUMTASKCHAIN &mtc = llMedias.at(i);
        for (size_t a = mtc.chain.size(); a > 0; --a)
            mtc.chain[a - 1].uIdx = uIdx++;
    }

    return rc;
}

HRESULT MachineMoveVM::addSaveState(const ComObjPtr<Machine> &machine, ULONG &uCount, ULONG &uTotalWeight)
{
    Bstr bstrSrcSaveStatePath;
    HRESULT rc = machine->COMGETTER(StateFilePath)(bstrSrcSaveStatePath.asOutParam());
    if (FAILED(rc)) return rc;
    if (!bstrSrcSaveStatePath.isEmpty())
    {
        SAVESTATETASK sst;

        sst.snapshotUuid = machine->i_getSnapshotId();
        sst.strSaveStateFile = bstrSrcSaveStatePath;
        uint64_t cbSize;
        int vrc = RTFileQuerySize(sst.strSaveStateFile.c_str(), &cbSize);
        if (RT_FAILURE(vrc))
            return m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr("Could not query file size of '%s' (%Rrc)"),
                                      sst.strSaveStateFile.c_str(), vrc);
        /* same rule as above: count both the data which needs to
         * be read and written */
        sst.uWeight = (ULONG)(2 * (cbSize + _1M - 1) / _1M);
        llSaveStateFiles.append(sst);
        RTPrintf("Added state file %s into the llSaveStateFiles.\n", sst.strSaveStateFile.c_str());
        ++uCount;
        uTotalWeight += sst.uWeight;
    }
    return S_OK;
}

void MachineMoveVM::updateProgressStats(MEDIUMTASKCHAIN &mtc, ULONG &uCount, ULONG &uTotalWeight) const
{

    /* Currently the copying of diff images involves reading at least
     * the biggest parent in the previous chain. So even if the new
     * diff image is small in size, it could need some time to create
     * it. Adding the biggest size in the chain should balance this a
     * little bit more, i.e. the weight is the sum of the data which
     * needs to be read and written. */
    ULONG uMaxWeight = 0;
    for (size_t e = mtc.chain.size(); e > 0; --e)
    {
        MEDIUMTASK &mt = mtc.chain.at(e - 1);
        mt.uWeight += uMaxWeight;

        /* Calculate progress data */
        ++uCount;
        uTotalWeight += mt.uWeight;

        /* Save the max size for better weighting of diff image
         * creation. */
        uMaxWeight = RT_MAX(uMaxWeight, mt.uWeight);
    }
}
