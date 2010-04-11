/** @file
 *
 * VirtualBox Appliance private data definitions
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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

#ifndef ____H_APPLIANCEIMPLPRIVATE
#define ____H_APPLIANCEIMPLPRIVATE

class VirtualSystemDescription;

#include "ovfreader.h"

////////////////////////////////////////////////////////////////////////////////
//
// Appliance data definition
//
////////////////////////////////////////////////////////////////////////////////

/* Describe a location for the import/export. The location could be a file on a
 * local hard disk or a remote target based on the supported inet protocols. */
struct Appliance::LocationInfo
{
    LocationInfo()
      : storageType(VFSType_File) {}
    VFSType_T storageType; /* Which type of storage should be handled */
    Utf8Str strPath;       /* File path for the import/export */
    Utf8Str strHostname;   /* Hostname on remote storage locations (could be empty) */
    Utf8Str strUsername;   /* Username on remote storage locations (could be empty) */
    Utf8Str strPassword;   /* Password on remote storage locations (could be empty) */
};

// opaque private instance data of Appliance class
struct Appliance::Data
{
    enum ApplianceState { ApplianceIdle, ApplianceImporting, ApplianceExporting };

    Data()
      : state(ApplianceIdle),
        pReader(NULL)
    {
    }

    ~Data()
    {
        if (pReader)
        {
            delete pReader;
            pReader = NULL;
        }
    }

    ApplianceState  state;

    LocationInfo    locInfo;       // location info for the currently processed OVF

    ovf::OVFReader  *pReader;

    bool            fBusyWriting;          // state protection; while this is true nobody else can call methods

    std::list< ComObjPtr<VirtualSystemDescription> >
                    virtualSystemDescriptions;

    std::list<Utf8Str>   llWarnings;

    ULONG           ulWeightPerOperation;
    ULONG           ulTotalDisksMB;
    ULONG           cDisks;
    Utf8Str         strOVFSHA1Digest;
};

struct Appliance::XMLStack
{
    std::map<Utf8Str, const VirtualSystemDescriptionEntry*> mapDisks;
    std::map<Utf8Str, bool> mapNetworks;
};

struct Appliance::TaskOVF
{
    enum TaskType
    {
        Read,
        Import,
        Write
    };

    TaskOVF(Appliance *aThat,
            TaskType aType,
            LocationInfo aLocInfo,
            ComObjPtr<Progress> &aProgress)
      : pAppliance(aThat),
        taskType(aType),
        locInfo(aLocInfo),
        pProgress(aProgress),
        enFormat(unspecified),
        rc(S_OK)
    {}

    static int updateProgress(unsigned uPercent, void *pvUser);

    int startThread();

    Appliance *pAppliance;
    TaskType taskType;
    const LocationInfo locInfo;
    ComObjPtr<Progress> pProgress;

    OVFFormat enFormat;

    HRESULT rc;
};

struct MyHardDiskAttachment
{
    Bstr                bstrUuid;
    ComPtr<IMachine>    pMachine;
    Bstr                controllerType;
    int32_t             lChannel;
    int32_t             lDevice;
};

////////////////////////////////////////////////////////////////////////////////
//
// VirtualSystemDescription data definition
//
////////////////////////////////////////////////////////////////////////////////

struct VirtualSystemDescription::Data
{
    std::list<VirtualSystemDescriptionEntry>
                            llDescriptions;     // item descriptions

    ComPtr<Machine>         pMachine;           // VirtualBox machine this description was exported from (export only)

    settings::MachineConfigFile
                            *pConfig;           // machine config created from <vbox:Machine> element if found (import only)
};

////////////////////////////////////////////////////////////////////////////////
//
// Internal helpers
//
////////////////////////////////////////////////////////////////////////////////

void convertCIMOSType2VBoxOSType(Utf8Str &strType, ovf::CIMOSType_T c, const Utf8Str &cStr);

ovf::CIMOSType_T convertVBoxOSType2CIMOSType(const char *pcszVbox);

#endif // ____H_APPLIANCEIMPLPRIVATE
