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
#include "MediumFormatImpl.h"
#include "VirtualBoxImpl.h"
#include "Logging.h"

/* This variable is global and used in the different places so it must be cleared each time before usage to avoid failure */
std::vector< ComObjPtr<Machine> > machineList;

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


void MachineMoveVM::ErrorInfoItem::printItem(bool fLog) const
{
    if (fLog)
    {
        LogRelFunc(("(The error code is %Rrc): %s\n",m_code, m_description.c_str()));
    }
    else
        RTPrintf("(The error code is %Rrc): %s\n",m_code, m_description.c_str());
}

HRESULT MachineMoveVM::init()
{
    HRESULT rc = S_OK;

    errorsList.clear();

    Utf8Str strTargetFolder;
    /* adding a trailing slash if it's needed */
    {
        size_t len = m_targetPath.length() + 2;
        if (len >=RTPATH_MAX)
        {
            Utf8Str errorDesc(" The destination path isn't correct. The length of path exceeded the maximum value.");
            errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));
            throw m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
        }

        char* path = new char [len];
        RTStrCopy(path, len, m_targetPath.c_str());
        RTPathEnsureTrailingSeparator(path, len);
        strTargetFolder = m_targetPath = path;
        delete[] path;
    }

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
        if (FAILED(vrc))
        {
            RTPrintf("strTargetFolder is %s\n", strTargetFolder.c_str());
            Utf8StrFmt errorDesc("Unable to move machine. Can't get the destination storage size (%s)",
                                 strTargetFolder.c_str());
            errorsList.push_back(ErrorInfoItem(E_FAIL, errorDesc.c_str()));
            rc = m_pMachine->setError(E_FAIL, m_pMachine->tr(errorDesc.c_str()));
            throw rc;
        }

        RTDIR hDir;
        Utf8Str strTempFile = strTargetFolder + "test.txt";
        vrc = RTDirOpen(&hDir, strTargetFolder.c_str());
        if (FAILED(vrc))
            throw rc = vrc;
        else
        {
            RTFILE hFile;
            vrc = RTFileOpen(&hFile, strTempFile.c_str(), RTFILE_O_OPEN_CREATE | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE);
            if (FAILED(vrc))
            {
                LogRelFunc(("Can't create a test file %s (The error is %Rrc)\n", strTempFile.c_str(), vrc));
                Utf8StrFmt errorDesc("Can't create a test file test.txt in the %s. Check the access rights of "
                                     "the destination folder.", strTargetFolder.c_str());
                errorsList.push_back(ErrorInfoItem(HRESULT(vrc), errorDesc.c_str()));
                rc = m_pMachine->setError(vrc, m_pMachine->tr(errorDesc.c_str()));

            }

            RTFileClose(hFile);
            RTFileDelete(strTempFile.c_str());
            RTDirClose(hDir);
        }

        long long totalFreeSpace = cbFree;
        long long totalSpace = cbTotal;
        info = Utf8StrFmt("blocks: total %lld, free %u ", cbTotal, cbFree);
        LogRelFunc(("%s \n", info.c_str()));
        LogRelFunc(("total space (Kb) %lld (Mb) %lld (Gb) %lld\n",
               totalSpace/1024, totalSpace/(1024*1024), totalSpace/(1024*1024*1024)));
        LogRelFunc(("total free space (Kb) %lld (Mb) %lld (Gb) %lld\n",
               totalFreeSpace/1024, totalFreeSpace/(1024*1024), totalFreeSpace/(1024*1024*1024)));

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

        LogRelFunc(("%s \n", info.c_str()));

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
        if (   m_type.equals("basic")
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
            if (   m_type.equals("basic")
                && strStateFilePath.contains(strSettingsFilePath))
                vmFolders.insert(std::make_pair(VBox_StateFolder, strStateFilePath));//Utf8Str(bstr_stateFilePath)));
        }

        Utf8Str strSnapshotFolder;
        Bstr bstr_snapshotFolder;
        m_pMachine->COMGETTER(SnapshotFolder)(bstr_snapshotFolder.asOutParam());
        strSnapshotFolder = bstr_snapshotFolder;
        if (   m_type.equals("basic")
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

        /* Global variable (defined at the beginning of file), so clear it before usage */
        machineList.clear();
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

        ULONG uCount       = 1;//looks like it should be initialized by 1. See assertion in the Progress::setNextOperation()
        ULONG uTotalWeight = 1;

        /* The lists llMedias and llSaveStateFiles are filled in the queryMediasForAllStates() */
        queryMediasForAllStates(machineList);

        {
            uint64_t totalMediumsSize = 0;

            for (size_t i = 0; i < llMedias.size(); ++i)
            {
                LONG64  cbSize = 0;
                MEDIUMTASKCHAINMOVE &mtc = llMedias.at(i);
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

                        std::pair<std::map<Utf8Str, MEDIUMTASKMOVE>::iterator,bool> ret;
                        ret = finalMediumsMap.insert(std::make_pair(name, mtc.chain[a - 1]));
                        if (ret.second == true)
                        {
                            /* Calculate progress data */
                            ++uCount;
                            uTotalWeight += mtc.chain[a - 1].uWeight;
                            totalMediumsSize += cbSize;
                            LogRelFunc(("Image %s was added into the moved list\n", name.c_str()));
                        }
                    }
                }
            }

            LogRelFunc(("Total Size of images is %lld bytes\n", totalMediumsSize));
            neededFreeSpace += totalMediumsSize;
        }

        /* Prepare data for moving ".sav" files */
        {
            uint64_t totalStateSize = 0;

            for (size_t i = 0; i < llSaveStateFiles.size(); ++i)
            {
                uint64_t cbFile = 0;
                SAVESTATETASKMOVE &sst = llSaveStateFiles.at(i);

                Utf8Str name = sst.strSaveStateFile;
                /*if a state file is located in the actual VM folder it will be added to the actual list */
                if (name.contains(strSettingsFilePath))
                {
                    vrc = RTFileQuerySize(name.c_str(), &cbFile);
                    if (RT_SUCCESS(vrc))
                    {
                        std::pair<std::map<Utf8Str, SAVESTATETASKMOVE>::iterator,bool> ret;
                        ret = finalSaveStateFilesMap.insert(std::make_pair(name, sst));
                        if (ret.second == true)
                        {
                            totalStateSize += cbFile;
                            ++uCount;
                            uTotalWeight += sst.uWeight;
                            LogRelFunc(("The state file %s was added into the moved list\n", name.c_str()));
                        }
                    }
                    else
                        LogRelFunc(("The state file %s wasn't added into the moved list. Couldn't get the file size.\n",
                                         name.c_str()));
                }
            }

            neededFreeSpace += totalStateSize;
        }

        /* Prepare data for moving the log files */
        {
            Utf8Str strFolder = vmFolders[VBox_LogFolder];

            if (RTPathExists(strFolder.c_str()))
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

                        uint64_t cbFile = 0;
                        vrc = RTFileQuerySize(strFile.c_str(), &cbFile);
                        if (RT_SUCCESS(vrc))
                        {
                            uCount       += 1;
                            uTotalWeight += (ULONG)((cbFile + _1M - 1) / _1M);
                            actualFileList.add(strFile);
                            LogRelFunc(("The log file %s added into the moved list\n", strFile.c_str()));
                        }
                        else
                            LogRelFunc(("The log file %s wasn't added into the moved list. Couldn't get the file size."
                                             "\n", strFile.c_str()));
                        ++it;
                    }
                }
            }
            else
            {
                LogRelFunc(("Information: The original log folder %s doesn't exist\n", strFolder.c_str()));
                rc = S_OK;//it's not error in this case if there isn't an original log folder
            }
        }

        LogRelFunc(("Total space needed is %lld bytes\n", neededFreeSpace));
        /* Check a target location on enough room */
        if (totalFreeSpace - neededFreeSpace <= 1024*1024)
        {
            LogRelFunc(("but free space on destination is %lld\n", totalFreeSpace));
            throw VERR_OUT_OF_RESOURCES;//less than 1Mb free space on the target location
        }

        /* Add step for .vbox machine setting file */
        {
            ++uCount;
            uTotalWeight += 1;
        }

        /* Reserve additional steps in case of failure and rollback all changes */
        {
            uTotalWeight += uCount;//just add 1 for each possible rollback operation
            uCount += uCount;//and increase the steps twice
        }

        /* Init Progress instance */
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
                Utf8StrFmt errorDesc("Couldn't correctly setup the progress object for moving VM operation (%Rrc)", rc);
                errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                throw m_pMachine->setError(VBOX_E_IPRT_ERROR,m_pMachine->tr(errorDesc.c_str()));
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

void MachineMoveVM::printStateFile(settings::SnapshotsList &snl)
{
    settings::SnapshotsList::iterator it;
    for (it = snl.begin(); it != snl.end(); ++it)
    {
        if (!it->strStateFile.isEmpty())
        {
            settings::Snapshot snap = (settings::Snapshot)(*it);
            LogRelFunc(("snap.uuid = %s\n", snap.uuid.toStringCurly().c_str()));
            LogRelFunc(("snap.strStateFile = %s\n", snap.strStateFile.c_str()));
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
        machine->COMGETTER(Name)(bstrMachineName.asOutParam());
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
     *   All subfolders related to the original VM are also moved from the original location
     *   (Standard - snapshots and logs folders).
     *
     * canonical mode:
     * - All disks tied with the VM will be moved into a new location if it's possible.
     *   All folders related to the original VM are also moved.
     * This mode is intended to collect all files/images/snapshots related to the VM in the one place.
     *
     */

    /*
     * A way to handle shareable disk:
     * Collect the shareable disks attched to the VM.
     * Get the machines whom the shareable disks attach to.
     * Return an error if the state of any VM doesn't allow to move a shareable disk and
     * this disk is located in the VM's folder (it means the disk is intended for "moving").
     */


    /*
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

        /* Get Machine::Data here because moveAllDisks() change it */
        Machine::Data *machineData = machine->mData.data();
        settings::MachineConfigFile *machineConfFile = machineData->pMachineConfigFile;

        /* Copy all save state files. */
        Utf8Str strTrgSnapshotFolder;
        {
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
                {
                    Utf8StrFmt errorDesc("Could not create snapshots folder '%s' (%Rrc)", strTrgSnapshotFolder.c_str(), vrc);
                    taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                    throw machine->setError(VBOX_E_IPRT_ERROR,machine->tr(errorDesc.c_str()));
                }
            }

            std::map<Utf8Str, SAVESTATETASKMOVE>::iterator itState = taskMoveVM->finalSaveStateFilesMap.begin();
            while (itState != taskMoveVM->finalSaveStateFilesMap.end())
            {
                const SAVESTATETASKMOVE &sst = itState->second;
                const Utf8Str &strTrgSaveState = Utf8StrFmt("%s%c%s", strTrgSnapshotFolder.c_str(), RTPATH_DELIMITER,
                                                            RTPathFilename(sst.strSaveStateFile.c_str()));

                /* Move to next sub-operation. */
                rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Copy the save state file '%s' ..."),
                                                            RTPathFilename(sst.strSaveStateFile.c_str())).raw(), sst.uWeight);
                if (FAILED(rc)) throw rc;

                int vrc = RTFileCopyEx(sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), 0,
                                       MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
                if (RT_FAILURE(vrc))
                {
                    Utf8StrFmt errorDesc("Could not copy state file '%s' to '%s' (%Rrc)",
                                         sst.strSaveStateFile.c_str(), strTrgSaveState.c_str(), vrc);
                    taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                    throw machine->setError(VBOX_E_IPRT_ERROR,machine->tr(errorDesc.c_str()));
                }

                /* save new file in case of restoring */
                newFiles.append(strTrgSaveState);
                /* save original file for deletion in the end */
                originalFiles.append(sst.strSaveStateFile);
                ++itState;
            }
        }

        /*
         * Update state file path
         * very important step!
         * Not obvious how to do it correctly.
         */
        {
            LogRelFunc(("Update state file path\n"));
            rc = taskMoveVM->updatePathsToStateFiles(taskMoveVM->finalSaveStateFilesMap,
                                                     taskMoveVM->vmFolders[VBox_SettingFolder],
                                                     strTargetFolder);
            if (FAILED(rc))
                throw rc;
        }

        /*
         * Moving Machine settings file
         * The settings file are moved after all disks and snapshots because this file should be updated
         * with actual information and only then should be moved.
         */
        {
            LogRelFunc(("Copy Machine settings file \n"));

            rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Copy Machine settings file '%s' ..."),
                                                (*machineConfFile).machineUserData.strName.c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;

            Utf8Str strTargetSettingsFilePath = strTargetFolder;

            /* Check a folder existing and create one if it's not */
            if (!RTDirExists(strTargetSettingsFilePath.c_str()))
            {
                int vrc = RTDirCreateFullPath(strTargetSettingsFilePath.c_str(), 0700);
                if (RT_FAILURE(vrc))
                {
                    Utf8StrFmt errorDesc("Could not create a home machine folder '%s' (%Rrc)",
                                         strTargetSettingsFilePath.c_str(), vrc);
                    taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                    throw machine->setError(VBOX_E_IPRT_ERROR,machine->tr(errorDesc.c_str()));
                }
                LogRelFunc(("Created a home machine folder %s\n", strTargetSettingsFilePath.c_str()));
            }

            /* Create a full path */
            Bstr bstrMachineName;
            machine->COMGETTER(Name)(bstrMachineName.asOutParam());
            strTargetSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName));
            strTargetSettingsFilePath.append(".vbox");

            Utf8Str strSettingsFilePath;
            Bstr bstr_settingsFilePath;
            machine->COMGETTER(SettingsFilePath)(bstr_settingsFilePath.asOutParam());
            strSettingsFilePath = bstr_settingsFilePath;

            int vrc = RTFileCopyEx(strSettingsFilePath.c_str(), strTargetSettingsFilePath.c_str(), 0,
                                   MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
            if (RT_FAILURE(vrc))
            {
                Utf8StrFmt errorDesc("Could not copy the setting file '%s' to '%s' (%Rrc)",
                                     strSettingsFilePath.c_str(), strTargetSettingsFilePath.stripFilename().c_str(), vrc);
                taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                throw machine->setError(VBOX_E_IPRT_ERROR, machine->tr(errorDesc.c_str()));
            }

            LogRelFunc(("The setting file %s has been copied into the folder %s\n", strSettingsFilePath.c_str(),
                        strTargetSettingsFilePath.stripFilename().c_str()));

            /* save new file in case of restoring */
            newFiles.append(strTargetSettingsFilePath);
            /* save original file for deletion in the end */
            originalFiles.append(strSettingsFilePath);
        }

        /* Moving Machine log files */
        {
            LogRelFunc(("Copy machine log files \n"));

            if (taskMoveVM->vmFolders[VBox_LogFolder].isNotEmpty())
            {
                /* Check an original log folder existence */
                if (RTDirExists(taskMoveVM->vmFolders[VBox_LogFolder].c_str()))
                {
                    Utf8Str strTargetLogFolderPath = strTargetFolder;
                    strTargetLogFolderPath.append(RTPATH_DELIMITER).append("Logs");

                    /* Check a destination log folder existence and create one if it's not */
                    if (!RTDirExists(strTargetLogFolderPath.c_str()))
                    {
                        int vrc = RTDirCreateFullPath(strTargetLogFolderPath.c_str(), 0700);
                        if (RT_FAILURE(vrc))
                        {
                            Utf8StrFmt errorDesc("Could not create log folder '%s' (%Rrc)",
                                                 strTargetLogFolderPath.c_str(), vrc);
                            taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                            throw machine->setError(VBOX_E_IPRT_ERROR,machine->tr(errorDesc.c_str()));
                        }
                        LogRelFunc(("Created a log machine folder %s\n", strTargetLogFolderPath.c_str()));
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
                        rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt(machine->tr("Copying the log file '%s' ..."),
                                                                RTPathFilename(strFullSourceFilePath.c_str())).raw(), 1);
                        if (FAILED(rc)) throw rc;

                        int vrc = RTFileCopyEx(strFullSourceFilePath.c_str(), strFullTargetFilePath.c_str(), 0,
                                       MachineMoveVM::copyFileProgress, &taskMoveVM->m_pProgress);
                        if (RT_FAILURE(vrc))
                        {
                            Utf8StrFmt errorDesc("Could not copy the log file '%s' to '%s' (%Rrc)",
                                                 strFullSourceFilePath.c_str(),
                                                 strFullTargetFilePath.stripFilename().c_str(),
                                                 vrc);
                            taskMoveVM->errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                            throw machine->setError(VBOX_E_IPRT_ERROR,machine->tr(errorDesc.c_str()));
                        }

                        LogRelFunc(("The log file %s has been copied into the folder %s\n", strFullSourceFilePath.c_str(),
                                         strFullTargetFilePath.stripFilename().c_str()));

                        /* save new file in case of restoring */
                        newFiles.append(strFullTargetFilePath);
                        /* save original file for deletion in the end */
                        originalFiles.append(strFullSourceFilePath);

                        ++it;
                    }
                }
            }
        }

        /* save all VM data */
        {
            rc = machine->SaveSettings();
        }

        {
            LogRelFunc(("Update path to XML setting file\n"));
            Utf8Str strTargetSettingsFilePath = strTargetFolder;
            Bstr bstrMachineName;
            machine->COMGETTER(Name)(bstrMachineName.asOutParam());
            strTargetSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName)).append(".vbox");
            machineData->m_strConfigFileFull = strTargetSettingsFilePath;
            machine->mParent->i_copyPathRelativeToConfig(strTargetSettingsFilePath, machineData->m_strConfigFile);
        }

        /* Save global settings in the VirtualBox.xml */
        {
            /* Marks the global registry for uuid as modified */
            Guid uuid = machine->mData->mUuid;
            machine->mParent->i_markRegistryModified(uuid);

            /* for saving the global settings we should hold only the VirtualBox lock */
            AutoWriteLock vboxLock(machine->mParent COMMA_LOCKVAL_SRC_POS);

            rc = machine->mParent->i_saveSettings();
        }
    }
    catch(HRESULT hrc)
    {
        LogRelFunc(("Moving machine to a new destination was failed. Check original and destination places.\n"));
        rc = hrc;
        taskMoveVM->result = rc;
    }
    catch (...)
    {
        LogRelFunc(("Moving machine to a new destination was failed. Check original and destination places.\n"));
        rc = VirtualBoxBase::handleUnexpectedExceptions(machine, RT_SRC_POS);
        taskMoveVM->result = rc;
    }

    /* Cleanup on failure */
    if (FAILED(rc))
    {
        Machine::Data *machineData = machine->mData.data();

        /* ! Apparently we should update the Progress object !*/
        ULONG operationCount = 0;
        rc = taskMoveVM->m_pProgress->COMGETTER(OperationCount)(&operationCount);
        ULONG operation = 0;
        rc = taskMoveVM->m_pProgress->COMGETTER(Operation)(&operation);
        Bstr bstrOperationDescription;
        rc = taskMoveVM->m_pProgress->COMGETTER(OperationDescription)(bstrOperationDescription.asOutParam());
        Utf8Str strOperationDescription = bstrOperationDescription;
        ULONG operationPercent = 0;
        rc = taskMoveVM->m_pProgress->COMGETTER(OperationPercent)(&operationPercent);

        Bstr bstrMachineName;
        machine->COMGETTER(Name)(bstrMachineName.asOutParam());
        LogRelFunc(("Moving machine %s was failed on operation %s\n",
                    Utf8Str(bstrMachineName.raw()).c_str(), Utf8Str(bstrOperationDescription.raw()).c_str()));

        /* Restoring the original mediums */
        try
        {
            /*
             * Fix the progress counter
             * In instance, the whole "move vm" operation is failed on 5th step. But total count is 20.
             * Where 20 = 2 * 10 operations, where 10 is the real number of operations. And this value was doubled
             * earlier in the init() exactly for one reason - rollback operation. Because in this case we must do
             * the same operations but in backward direction.
             * Thus now we want to correct the progress counter from 5 to 15. Why?
             * Because we should have evaluated the counter as "20/2 + (20/2 - 5)" = 15 or just "20 - 5" = 15
             * And because the 5th step failed it shouldn't be counted.
             * As result, we need to rollback 4 operations.
             * Thus we start from "operation + 1" and finish when "i < operationCount - operation".
             */
            for (ULONG i = operation + 1; i < operationCount - operation; ++i)
            {
                rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt("Skip the empty operation %d...", i + 1).raw(), 1);
                if (FAILED(rc)) throw rc;
            }

            rc = taskMoveVM->moveAllDisks(taskMoveVM->finalMediumsMap);
            if (FAILED(rc))
                throw rc;
        }
        catch(HRESULT hrc)
        {
            LogRelFunc(("Rollback scenario: restoration the original mediums were failed. Machine can be corrupted.\n"));
            taskMoveVM->result = hrc;
        }
        catch (...)
        {
            LogRelFunc(("Rollback scenario: restoration the original mediums were failed. Machine can be corrupted.\n"));
            rc = VirtualBoxBase::handleUnexpectedExceptions(machine, RT_SRC_POS);
            taskMoveVM->result = rc;
        }

        /* Revert original paths to the state files */
        rc = taskMoveVM->updatePathsToStateFiles(taskMoveVM->finalSaveStateFilesMap,
                                                 strTargetFolder,
                                                 taskMoveVM->vmFolders[VBox_SettingFolder]);
        if (FAILED(rc))
        {
            LogRelFunc(("Rollback scenario: can't restore the original paths to the state files. "
                        "Machine settings %s can be corrupted.\n", machineData->m_strConfigFileFull.c_str()));
        }

        /* Delete all created files. Here we update progress object */
        rc = taskMoveVM->deleteFiles(newFiles);
        if (FAILED(rc))
            LogRelFunc(("Rollback scenario: can't delete new created files. Check the destination folder."));

        /* Delete destination folder */
        RTDirRemove(strTargetFolder.c_str());

        /* save all VM data */
        {
            AutoWriteLock  srcLock(machine COMMA_LOCKVAL_SRC_POS);
            srcLock.release();
            rc = machine->SaveSettings();
            srcLock.acquire();
        }

        /* Restore an original path to XML setting file */
        {
            LogRelFunc(("Rollback scenario: restoration of the original path to XML setting file\n"));
            Utf8Str strOriginalSettingsFilePath = taskMoveVM->vmFolders[VBox_SettingFolder];
            strOriginalSettingsFilePath.append(RTPATH_DELIMITER).append(Utf8Str(bstrMachineName)).append(".vbox");
            machineData->m_strConfigFileFull = strOriginalSettingsFilePath;
            machine->mParent->i_copyPathRelativeToConfig(strOriginalSettingsFilePath, machineData->m_strConfigFile);
        }

        /* Marks the global registry for uuid as modified */
        {
            AutoWriteLock  srcLock(machine COMMA_LOCKVAL_SRC_POS);
            srcLock.release();
            Guid uuid = machine->mData->mUuid;
            machine->mParent->i_markRegistryModified(uuid);
            srcLock.acquire();
        }

        /* save the global settings; for that we should hold only the VirtualBox lock */
        {
            AutoWriteLock vboxLock(machine->mParent COMMA_LOCKVAL_SRC_POS);
            rc = machine->mParent->i_saveSettings();
        }

        /* In case of failure the progress object on the other side (user side) get notification about operation
           completion but the operation percentage may not be set to 100% */
    }
    else /*Operation was successful and now we can delete the original files like the state files, XML setting, log files */
    {
        /*
         * In case of success it's not urgent to update the progress object because we call i_notifyComplete() with
         * the success result. As result, the last number of progress operation can be not equal the number of operations
         * because we doubled the number of operations for rollback case.
         * But if we want to update the progress object corectly it's needed to add all medium moved by standard
         * "move medium" logic (for us it's taskMoveVM->finalMediumsMap) to the current number of operation.
         */

        ULONG operationCount = 0;
        rc = taskMoveVM->m_pProgress->COMGETTER(OperationCount)(&operationCount);
        ULONG operation = 0;
        rc = taskMoveVM->m_pProgress->COMGETTER(Operation)(&operation);

        for (ULONG i = operation; i < operation + taskMoveVM->finalMediumsMap.size() - 1; ++i)
        {
            rc = taskMoveVM->m_pProgress->SetNextOperation(BstrFmt("Skip the empty operation %d...", i).raw(), 1);
            if (FAILED(rc)) throw rc;
        }

        rc = taskMoveVM->deleteFiles(originalFiles);
        if (FAILED(rc))
            LogRelFunc(("Forward scenario: can't delete all original files.\n"));
    }

    if (!taskMoveVM->m_pProgress.isNull())
    {
        /* Set the first error happened */
        if (!taskMoveVM->errorsList.empty())
        {
            ErrorInfoItem ei = taskMoveVM->errorsList.front();
            machine->setError(ei.m_code, machine->tr(ei.m_description.c_str()));
        }

        taskMoveVM->m_pProgress->i_notifyComplete(taskMoveVM->result);
    }

    LogFlowFuncLeave();
}

HRESULT MachineMoveVM::moveAllDisks(const std::map<Utf8Str, MEDIUMTASKMOVE>& listOfDisks,
                                    const Utf8Str* strTargetFolder)
{
    HRESULT rc = S_OK;
    ComObjPtr<Machine> &machine = m_pMachine;
    Utf8Str strLocation;

    AutoWriteLock  machineLock(machine COMMA_LOCKVAL_SRC_POS);

    try{
        std::map<Utf8Str, MEDIUMTASKMOVE>::const_iterator itMedium = listOfDisks.begin();
        while(itMedium != listOfDisks.end())
        {
            const MEDIUMTASKMOVE &mt = itMedium->second;
            ComPtr<IMedium> pMedium = mt.pMedium;
            Utf8Str strTargetImageName;
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
                rc = m_pProgress->SetNextOperation(BstrFmt(machine->tr("Moving medium '%ls' ..."),
                                                       bstrSrcName.raw()).raw(),
                                                       mt.uWeight);
                if (FAILED(rc)) throw rc;
            }
            else
            {
                strTargetImageName = mt.strBaseName;//Should contain full path to the image
                rc = m_pProgress->SetNextOperation(BstrFmt(machine->tr("Moving medium '%ls' back..."),
                                                       bstrSrcName.raw()).raw(),
                                                       mt.uWeight);
                if (FAILED(rc)) throw rc;
            }



            /* consistency: use \ if appropriate on the platform */
            RTPathChangeToDosSlashes(strTargetImageName.mutableRaw(), false);

            bstrLocation = strTargetImageName.c_str();

            MediumType_T mediumType;//immutable, shared, passthrough
            rc = pMedium->COMGETTER(Type)(&mediumType);
            if (FAILED(rc)) throw rc;

            DeviceType_T deviceType;//floppy, hard, DVD
            rc = pMedium->COMGETTER(DeviceType)(&deviceType);
            if (FAILED(rc)) throw rc;

            /* Drop lock early because IMedium::SetLocation needs to get the VirtualBox one. */
            machineLock.release();

            ComPtr<IProgress> moveDiskProgress;
            rc = pMedium->SetLocation(bstrLocation.raw(), moveDiskProgress.asOutParam());
            if (SUCCEEDED(rc))
            {
                /* In case of failure moveDiskProgress would be in the invalid state or not initialized at all
                 * Call WaitForAsyncProgressCompletion only in success
                 */
                /* Wait until the async process has finished. */
                rc = m_pProgress->WaitForAsyncProgressCompletion(moveDiskProgress);
            }

            /*acquire the lock back*/
            machineLock.acquire();

            if (FAILED(rc)) throw rc;

            LogRelFunc(("Moving %s has been finished\n", strTargetImageName.c_str()));

            /* Check the result of the async process. */
            LONG iRc;
            rc = moveDiskProgress->COMGETTER(ResultCode)(&iRc);
            if (FAILED(rc)) throw rc;
            /* If the thread of the progress object has an error, then
             * retrieve the error info from there, or it'll be lost. */
            if (FAILED(iRc))
            {
                ProgressErrorInfo pie(moveDiskProgress);
                Utf8Str errorDesc(pie.getText().raw());
                errorsList.push_back(ErrorInfoItem(pie.getResultCode(), errorDesc.c_str()));

                throw machine->setError(ProgressErrorInfo(moveDiskProgress));
            }

            ++itMedium;
        }

        machineLock.release();
    }
    catch(HRESULT hrc)
    {
        LogRelFunc(("\nException during moving the disk %s\n", strLocation.c_str()));
        rc = hrc;
        machineLock.release();
    }
    catch (...)
    {
        LogRelFunc(("\nException during moving the disk %s\n", strLocation.c_str()));
        rc = VirtualBoxBase::handleUnexpectedExceptions(m_pMachine, RT_SRC_POS);
        machineLock.release();
    }

    if (FAILED(rc))
    {
        Utf8StrFmt errorDesc("Exception during moving the disk %s\n", strLocation.c_str());
        errorsList.push_back(ErrorInfoItem(rc, errorDesc.c_str()));
    }

    return rc;
}

HRESULT MachineMoveVM::updatePathsToStateFiles(const std::map<Utf8Str, SAVESTATETASKMOVE>& listOfFiles,
                                               const Utf8Str& sourcePath, const Utf8Str& targetPath)
{
    HRESULT rc = S_OK;

    std::map<Utf8Str, SAVESTATETASKMOVE>::const_iterator itState = listOfFiles.begin();
    while (itState != listOfFiles.end())
    {
        const SAVESTATETASKMOVE &sst = itState->second;

        if (sst.snapshotUuid != Guid::Empty)
        {
            Utf8Str strGuidMachine = sst.snapshotUuid.toString();
            ComObjPtr<Snapshot> snapshotMachineObj;

            rc = m_pMachine->i_findSnapshotById(sst.snapshotUuid, snapshotMachineObj, true);
            if (SUCCEEDED(rc) && !snapshotMachineObj.isNull())
            {
                snapshotMachineObj->i_updateSavedStatePaths(sourcePath.c_str(),
                                                            targetPath.c_str());
            }
        }
        else
        {
            const Utf8Str &path = m_pMachine->mSSData->strStateFilePath;
            /*
             * This check for the case when a new value is equal to the old one.
             * Maybe the more clever check is needed in the some corner cases.
             */
            if (!path.contains(targetPath))
            {
                m_pMachine->mSSData->strStateFilePath = Utf8StrFmt("%s%s",
                                                                   targetPath.c_str(),
                                                                   path.c_str() + sourcePath.length());
            }
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
        Utf8StrFmt errorDesc("Folder '%s' doesn't exist (%Rrc)", strRootFolder.c_str(), vrc);
        errorsList.push_back(ErrorInfoItem(VERR_FILE_NOT_FOUND, errorDesc.c_str()));

        m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
        rc = vrc;
    }
    else
    {
        Utf8StrFmt errorDesc("Could not open folder '%s' (%Rrc)", strRootFolder.c_str(), vrc);
        errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

        throw m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
    }

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
            LogRelFunc(("Deleting file %s ...\n", listOfFiles.at(i).c_str()));
            rc = m_pProgress->SetNextOperation(BstrFmt("Deleting file %s...", listOfFiles.at(i).c_str()).raw(), 1);
            if (FAILED(rc)) throw rc;

            int vrc = RTFileDelete(listOfFiles.at(i).c_str());
            if (RT_FAILURE(vrc))
            {
                Utf8StrFmt errorDesc("Could not delete file '%s' (%Rrc)", listOfFiles.at(i).c_str(), rc);
                errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                rc = m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
            }
            else
                LogRelFunc(("File %s has been deleted\n", listOfFiles.at(i).c_str()));
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
    HRESULT rc = S_OK;
    int vrc = 0;
    uint64_t totalFolderSize = 0;
    fileList_t filesList;

    bool ex = RTPathExists(strRootFolder.c_str());
    if (ex == true)
    {
        rc = getFilesList(strRootFolder, filesList);
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
                {
                    Utf8StrFmt errorDesc("Could not get the size of file '%s' (%Rrc)", fullPath.c_str(), vrc);
                    errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

                    throw m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
                }
                ++it;
            }

            size = totalFolderSize;
        }
        else
        {
            Utf8StrFmt errorDesc("Could not calculate the size of folder '%s' (%Rrc)", strRootFolder.c_str(), vrc);
            errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

            m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
        }
    }
    else
        size = 0;

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

HRESULT MachineMoveVM::queryMediasForAllStates(const std::vector<ComObjPtr<Machine> > &aMachineList)
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
            DeviceType_T deviceType;//floppy, hard, DVD
            rc = pAtt->COMGETTER(Type)(&deviceType);
            if (FAILED(rc)) return rc;

            /* Valid medium attached? */
            ComPtr<IMedium> pMedium;
            rc = pAtt->COMGETTER(Medium)(pMedium.asOutParam());
            if (FAILED(rc)) return rc;

            if (pMedium.isNull())
                continue;

            Bstr bstrLocation;
            rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
            if (FAILED(rc)) throw rc;

            /* Cast to ComObjPtr<Medium> */
            ComObjPtr<Medium> pObjMedium = (Medium *)(IMedium *)pMedium;

            /*Check for "read-only" medium in terms that VBox can't create this one */
            bool fPass = isMediumTypeSupportedForMoving(pMedium);
            if(!fPass)
            {
                LogRelFunc(("Skipping file %s because of this medium type hasn't been supported for moving.\n",
                         Utf8Str(bstrLocation.raw()).c_str()));
                continue;
            }

            MEDIUMTASKCHAINMOVE mtc;
            mtc.devType = deviceType;

            while (!pMedium.isNull())
            {
                /* Refresh the state so that the file size get read. */
                MediumState_T e;
                rc = pMedium->RefreshState(&e);
                if (FAILED(rc)) return rc;

                LONG64 lSize;
                rc = pMedium->COMGETTER(Size)(&lSize);
                if (FAILED(rc)) return rc;

                MediumType_T mediumType;//immutable, shared, passthrough
                rc = pMedium->COMGETTER(Type)(&mediumType);
                if (FAILED(rc)) throw rc;

                rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
                if (FAILED(rc)) throw rc;

                MEDIUMTASKMOVE mt;// = {false, "basename", NULL, 0, 0};
                mt.strBaseName = bstrLocation;
                Utf8Str strFolder = vmFolders[VBox_SnapshotFolder];
                if (strFolder.isNotEmpty() && mt.strBaseName.contains(strFolder))
                {
                    mt.fSnapshot = true;
                }
                else
                    mt.fSnapshot = false;

                mt.uIdx    = UINT32_MAX;
                mt.pMedium = pMedium;
                mt.uWeight = (ULONG)((lSize + _1M - 1) / _1M);
                mtc.chain.append(mt);

                /* Query next parent. */
                rc = pMedium->COMGETTER(Parent)(pMedium.asOutParam());
                if (FAILED(rc)) return rc;
            }

            llMedias.append(mtc);
        }
        /* Add the save state files of this machine if there is one. */
        rc = addSaveState(machine);
        if (FAILED(rc)) return rc;

    }
    /* Build up the index list of the image chain. Unfortunately we can't do
     * that in the previous loop, cause there we go from child -> parent and
     * didn't know how many are between. */
    for (size_t i = 0; i < llMedias.size(); ++i)
    {
        uint32_t uIdx = 0;
        MEDIUMTASKCHAINMOVE &mtc = llMedias.at(i);
        for (size_t a = mtc.chain.size(); a > 0; --a)
            mtc.chain[a - 1].uIdx = uIdx++;
    }

    return rc;
}

HRESULT MachineMoveVM::addSaveState(const ComObjPtr<Machine> &machine)
{
    Bstr bstrSrcSaveStatePath;
    HRESULT rc = machine->COMGETTER(StateFilePath)(bstrSrcSaveStatePath.asOutParam());
    if (FAILED(rc)) return rc;
    if (!bstrSrcSaveStatePath.isEmpty())
    {
        SAVESTATETASKMOVE sst;

        sst.snapshotUuid = machine->i_getSnapshotId();
        sst.strSaveStateFile = bstrSrcSaveStatePath;
        uint64_t cbSize;

        int vrc = RTFileQuerySize(sst.strSaveStateFile.c_str(), &cbSize);
        if (RT_FAILURE(vrc))
        {
            Utf8StrFmt errorDesc("Could not get file size of '%s' (%Rrc)", sst.strSaveStateFile.c_str(), vrc);
            errorsList.push_back(ErrorInfoItem(VBOX_E_IPRT_ERROR, errorDesc.c_str()));

            return m_pMachine->setError(VBOX_E_IPRT_ERROR, m_pMachine->tr(errorDesc.c_str()));
        }
        /* same rule as above: count both the data which needs to
         * be read and written */
        sst.uWeight = (ULONG)(2 * (cbSize + _1M - 1) / _1M);
        llSaveStateFiles.append(sst);
    }
    return S_OK;
}

void MachineMoveVM::updateProgressStats(MEDIUMTASKCHAINMOVE &mtc, ULONG &uCount, ULONG &uTotalWeight) const
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
        MEDIUMTASKMOVE &mt = mtc.chain.at(e - 1);
        mt.uWeight += uMaxWeight;

        /* Calculate progress data */
        ++uCount;
        uTotalWeight += mt.uWeight;

        /* Save the max size for better weighting of diff image
         * creation. */
        uMaxWeight = RT_MAX(uMaxWeight, mt.uWeight);
    }
}

bool MachineMoveVM::isMediumTypeSupportedForMoving(const ComPtr<IMedium> &pMedium)
{
    HRESULT rc = S_OK;
    bool fSupported = true;
    Bstr bstrLocation;
    rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
    if (FAILED(rc))
    {
        fSupported = false;
        throw rc;
    }

    DeviceType_T deviceType;
    rc = pMedium->COMGETTER(DeviceType)(&deviceType);
    if (FAILED(rc))
    {
        fSupported = false;
        throw rc;
    }

    ComPtr<IMediumFormat> mediumFormat;
    rc = pMedium->COMGETTER(MediumFormat)(mediumFormat.asOutParam());
    if (FAILED(rc))
    {
        fSupported = false;
        throw rc;
    }

    /*Check whether VBox is able to create this medium format or not, i.e. medium can be "read-only" */
    Bstr bstrFormatName;
    rc = mediumFormat->COMGETTER(Name)(bstrFormatName.asOutParam());
    if (FAILED(rc))
    {
        fSupported = false;
        throw rc;
    }

    Utf8Str formatName = Utf8Str(bstrFormatName);
    if (formatName.compare("VHDX", Utf8Str::CaseInsensitive) == 0)
    {
        LogRelFunc(("Skipping medium %s. VHDX format is supported in \"read-only\" mode only. \n",
                    Utf8Str(bstrLocation.raw()).c_str()));
        fSupported = false;
    }

    /* Check whether medium is represented by file on the disk  or not */
    if (fSupported)
    {
        ComObjPtr<Medium> pObjMedium = (Medium *)(IMedium *)pMedium;
        fSupported = pObjMedium->i_isMediumFormatFile();
        if (!fSupported)
        {
            LogRelFunc(("Skipping medium %s because it's not a real file on the disk.\n",
                        Utf8Str(bstrLocation.raw()).c_str()));
        }
    }

    /* some special checks for DVD */
    if (fSupported && deviceType == DeviceType_DVD)
    {
        Utf8Str ext = bstrLocation;
        ext.assignEx(RTPathSuffix(ext.c_str()));//returns extension with dot (".iso")

        //only ISO image is moved. Otherwise adding some information into log file
        int equality = ext.compare(".iso", Utf8Str::CaseInsensitive);
        if (equality != false)
        {
            LogRelFunc(("Skipping file %s. Only ISO images are supported for now.\n",
                         Utf8Str(bstrLocation.raw()).c_str()));
            fSupported = false;
        }
    }

    return fSupported;
}
