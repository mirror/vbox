/* $Id$ */
/** @file
 * Definition of MachineMoveVM
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

#ifndef ____H_MACHINEIMPLMOVEVM
#define ____H_MACHINEIMPLMOVEVM

#include "MachineImpl.h"
#include "ProgressImpl.h"
#include "ThreadTask.h"

/////////////////////////////////////////////////////////////////////////////

enum VBoxFolder_t
{
    VBox_UnknownFolderType = 0,
    VBox_OutsideVMfolder,
    VBox_SettingFolder,
    VBox_LogFolder,
    VBox_StateFolder,
    VBox_SnapshotFolder
};

typedef struct
{
    Utf8Str                 strBaseName;
    ComPtr<IMedium>         pMedium;
    uint32_t                uIdx;
    ULONG                   uWeight;
    bool                    fSnapshot;
} MEDIUMTASK;

typedef struct
{
    RTCList<MEDIUMTASK>     chain;
    DeviceType_T            devType;
    bool                    fCreateDiffs;
    bool                    fAttachLinked;
} MEDIUMTASKCHAIN;

typedef struct
{
    Guid                    snapshotUuid;
    Utf8Str                 strSaveStateFile;
    ULONG                   uWeight;
} SAVESTATETASK;

struct fileList_t;

class MachineMoveVM : public ThreadTask
{
public:

    MachineMoveVM(ComObjPtr<Machine> aMachine,
                  const com::Utf8Str &aTargetPath,
                  const com::Utf8Str &aType,
                  ComObjPtr<Progress> &aProgress)
      : ThreadTask("TaskMoveVM"),
        m_pMachine(aMachine),
        m_pProgress(aProgress),
        m_targetPath(aTargetPath),
        m_type(aType),
        result(S_OK)
    {
    }

    virtual ~MachineMoveVM()
    {
    }

    HRESULT init();
    static DECLCALLBACK(int) updateProgress(unsigned uPercent, void *pvUser);
    static DECLCALLBACK(int) copyFileProgress(unsigned uPercentage, void *pvUser);
    static void i_MoveVMThreadTask(MachineMoveVM* task);

    RTCList<MEDIUMTASKCHAIN>    llMedias;
    RTCList<SAVESTATETASK>      llSaveStateFiles;
    std::map<Utf8Str, MEDIUMTASK>     finalMediumsMap;
    std::map<Utf8Str, SAVESTATETASK>  finalSaveStateFilesMap;
    std::map<VBoxFolder_t, Utf8Str> vmFolders;

    ComObjPtr<Machine>  m_pMachine;
    ComObjPtr<Progress> m_pProgress;
    ComPtr<ISession>    m_pSession;
    ComPtr<IMachine>    m_pSessionMachine;
    Utf8Str             m_targetPath;
    Utf8Str             m_type;
    HRESULT             result;

    void handler()
    {
        i_MoveVMThreadTask(this);
    }

    /* MachineCloneVM::start helper: */
    HRESULT createMachineList(const ComPtr<ISnapshot> &pSnapshot, std::vector< ComObjPtr<Machine> > &aMachineList) const;
    inline HRESULT queryBaseName(const ComPtr<IMedium> &pMedium, Utf8Str &strBaseName) const;
    HRESULT queryMediasForAllStates(const std::vector<ComObjPtr<Machine> > &aMachineList, ULONG &uCount, ULONG &uTotalWeight);
    void updateProgressStats(MEDIUMTASKCHAIN &mtc, ULONG &uCount, ULONG &uTotalWeight) const;
    HRESULT addSaveState(const ComObjPtr<Machine> &machine, ULONG &uCount, ULONG &uTotalWeight);
    void updateStateFile(settings::MachineConfigFile *snl, const Guid &id, const Utf8Str &strFile);
    void updateStateFile(settings::SnapshotsList &snl, const Guid &id, const Utf8Str &strFile);
    void printStateFile(settings::SnapshotsList &snl);
    HRESULT getFilesList(const Utf8Str& strRootFolder, fileList_t &filesList);
    HRESULT getFolderSize(const Utf8Str& strRootFolder, uint64_t& size);
    HRESULT deleteFiles(const RTCList<Utf8Str>& listOfFiles);
    HRESULT updateStateFilesXMLSettings(const std::map<Utf8Str, SAVESTATETASK>& listOfFiles, const Utf8Str& targetPath);
    HRESULT moveAllDisks(const std::map<Utf8Str, MEDIUMTASK>& listOfDisks, const Utf8Str* strTargetFolder = NULL);
    HRESULT restoreAllDisks(const std::map<Utf8Str, MEDIUMTASK>& listOfDisks);
};

#endif // ____H_MACHINEIMPLMOVEVM
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

