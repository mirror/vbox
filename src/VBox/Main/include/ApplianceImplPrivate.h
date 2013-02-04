/** @file
 *
 * VirtualBox Appliance private data definitions
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

#ifndef ____H_APPLIANCEIMPLPRIVATE
#define ____H_APPLIANCEIMPLPRIVATE

class VirtualSystemDescription;

#include "ovfreader.h"

////////////////////////////////////////////////////////////////////////////////
//
// Appliance data definition
//
////////////////////////////////////////////////////////////////////////////////

typedef std::pair<Utf8Str, Utf8Str> STRPAIR;

/* Describe a location for the import/export. The location could be a file on a
 * local hard disk or a remote target based on the supported inet protocols. */
struct LocationInfo
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
      : state(ApplianceIdle)
      , fManifest(true)
      , fSha256(false)
      , pReader(NULL)
      , ulWeightForXmlOperation(0)
      , ulWeightForManifestOperation(0)
      , ulTotalDisksMB(0)
      , cDisks(0)
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

    ApplianceState      state;

    LocationInfo        locInfo;        // location info for the currently processed OVF
    bool                fManifest;      // Create a manifest file on export
    bool                fSha256;        // true = SHA256 (OVF 2.0), false = SHA1 (OVF 1.0)
    RTCList<ImportOptions_T> optList;

    ovf::OVFReader      *pReader;

    std::list< ComObjPtr<VirtualSystemDescription> >
                        virtualSystemDescriptions;

    std::list<Utf8Str>  llWarnings;

    ULONG               ulWeightForXmlOperation;
    ULONG               ulWeightForManifestOperation;
    ULONG               ulTotalDisksMB;
    ULONG               cDisks;
    Utf8Str             strOVFSHADigest;

    std::list<Guid>     llGuidsMachinesCreated;
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
    ComPtr<IMachine>    pMachine;
    Bstr                controllerType;
    int32_t             lControllerPort;        // 0-29 for SATA
    int32_t             lDevice;                // IDE: 0 or 1, otherwise 0 always
};

/**
 * Used by Appliance::importMachineGeneric() to store
 * input parameters and rollback information.
 */
struct Appliance::ImportStack
{
    // input pointers
    const LocationInfo              &locInfo;           // ptr to location info from Appliance::importFS()
    Utf8Str                         strSourceDir;       // directory where source files reside
    const ovf::DiskImagesMap        &mapDisks;          // ptr to disks map in OVF
    ComObjPtr<Progress>             &pProgress;         // progress object passed into Appliance::importFS()

    // input parameters from VirtualSystemDescriptions
    Utf8Str                         strNameVBox;        // VM name
    Utf8Str                         strMachineFolder;   // FQ host folder where the VirtualBox machine would be created
    Utf8Str                         strOsTypeVBox;      // VirtualBox guest OS type as string
    Utf8Str                         strDescription;
    uint32_t                        cCPUs;              // CPU count
    bool                            fForceHWVirt;       // if true, we force enabling hardware virtualization
    bool                            fForceIOAPIC;       // if true, we force enabling the IOAPIC
    uint32_t                        ulMemorySizeMB;     // virtual machine RAM in megabytes
#ifdef VBOX_WITH_USB
    bool                            fUSBEnabled;
#endif
    Utf8Str                         strAudioAdapter;    // if not empty, then the guest has audio enabled, and this is the decimal
                                                        // representation of the audio adapter (should always be "0" for AC97 presently)

    // session (not initially created)
    ComPtr<ISession>                pSession;           // session opened in Appliance::importFS() for machine manipulation
    bool                            fSessionOpen;       // true if the pSession is currently open and needs closing

    // a list of images that we created/imported; this is initially empty
    // and will be cleaned up on errors
    std::list<MyHardDiskAttachment> llHardDiskAttachments;      // disks that were attached
    std::list<STRPAIR>              llSrcDisksDigest;           // Digests of the source disks

    ImportStack(const LocationInfo &aLocInfo,
                const ovf::DiskImagesMap &aMapDisks,
                ComObjPtr<Progress> &aProgress)
        : locInfo(aLocInfo),
          mapDisks(aMapDisks),
          pProgress(aProgress),
          cCPUs(1),
          fForceHWVirt(false),
          fForceIOAPIC(false),
          ulMemorySizeMB(0),
          fSessionOpen(false)
    {
        // disk images have to be on the same place as the OVF file. So
        // strip the filename out of the full file path
        strSourceDir = aLocInfo.strPath;
        strSourceDir.stripFilename();
    }
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

Utf8Str convertNetworkAttachmentTypeToString(NetworkAttachmentType_T type);

typedef struct SHASTORAGE
{
    PVDINTERFACE pVDImageIfaces;
    bool         fCreateDigest;
    bool         fSha256;        /* false = SHA1 (OVF 1.x), true = SHA256 (OVF 2.0) */
    Utf8Str      strDigest;
} SHASTORAGE, *PSHASTORAGE;

PVDINTERFACEIO ShaCreateInterface();
PVDINTERFACEIO FileCreateInterface();
PVDINTERFACEIO TarCreateInterface();
int ShaReadBuf(const char *pcszFilename, void **ppvBuf, size_t *pcbSize, PVDINTERFACEIO pIfIo, void *pvUser);
int ShaWriteBuf(const char *pcszFilename, void *pvBuf, size_t cbSize, PVDINTERFACEIO pIfIo, void *pvUser);

#endif // ____H_APPLIANCEIMPLPRIVATE

