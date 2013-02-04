/* $Id$ */

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

#ifndef ____H_APPLIANCEIMPL
#define ____H_APPLIANCEIMPL

/* VBox includes */
#include "VirtualBoxBase.h"

/* Todo: This file needs massive cleanup. Split IAppliance in a public and
 * private classes. */
#include <iprt/tar.h>

/* VBox forward declarations */
class Progress;
class VirtualSystemDescription;
struct VirtualSystemDescriptionEntry;
struct LocationInfo;
typedef struct VDINTERFACE   *PVDINTERFACE;
typedef struct VDINTERFACEIO *PVDINTERFACEIO;
typedef struct SHASTORAGE    *PSHASTORAGE;

namespace ovf
{
    struct HardDiskController;
    struct VirtualSystem;
    class OVFReader;
    struct DiskImage;
}

namespace xml
{
    class Document;
    class ElementNode;
}

namespace settings
{
    class MachineConfigFile;
}

class ATL_NO_VTABLE Appliance :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IAppliance)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(Appliance, IAppliance)

    DECLARE_NOT_AGGREGATABLE(Appliance)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(Appliance)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IAppliance)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Appliance)

    enum OVFFormat
    {
        unspecified,
        OVF_0_9,
        OVF_1_0,
        OVF_2_0
    };

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return BaseFinalConstruct(); }
    void FinalRelease() { uninit(); BaseFinalRelease(); }

    HRESULT init(VirtualBox *aVirtualBox);
    void uninit();

    /* IAppliance properties */
    STDMETHOD(COMGETTER(Path))(BSTR *aPath);
    STDMETHOD(COMGETTER(Disks))(ComSafeArrayOut(BSTR, aDisks));
    STDMETHOD(COMGETTER(VirtualSystemDescriptions))(ComSafeArrayOut(IVirtualSystemDescription*, aVirtualSystemDescriptions));
    STDMETHOD(COMGETTER(Machines))(ComSafeArrayOut(BSTR, aMachines));

    /* IAppliance methods */
    /* Import methods */
    STDMETHOD(Read)(IN_BSTR path, IProgress **aProgress);
    STDMETHOD(Interpret)(void);
    STDMETHOD(ImportMachines)(ComSafeArrayIn(ImportOptions_T, options), IProgress **aProgress);
    /* Export methods */
    STDMETHOD(CreateVFSExplorer)(IN_BSTR aURI, IVFSExplorer **aExplorer);
    STDMETHOD(Write)(IN_BSTR format, BOOL fManifest, IN_BSTR path, IProgress **aProgress);

    STDMETHOD(GetWarnings)(ComSafeArrayOut(BSTR, aWarnings));

    /* public methods only for internal purposes */

    static HRESULT setErrorStatic(HRESULT aResultCode,
                                  const Utf8Str &aText)
    {
        return setErrorInternal(aResultCode, getStaticClassIID(), getStaticComponentName(), aText, false, true);
    }

    /* private instance data */
private:
    /** weak VirtualBox parent */
    VirtualBox* const   mVirtualBox;

    struct ImportStack;
    struct TaskOVF;
    struct Data;            // opaque, defined in ApplianceImpl.cpp
    Data *m;

    enum SetUpProgressMode { ImportFile, ImportS3, WriteFile, WriteS3 };

    /*******************************************************************************
     * General stuff
     ******************************************************************************/

    bool isApplianceIdle();
    HRESULT searchUniqueVMName(Utf8Str& aName) const;
    HRESULT searchUniqueDiskImageFilePath(Utf8Str& aName) const;
    HRESULT setUpProgress(ComObjPtr<Progress> &pProgress,
                          const Bstr &bstrDescription,
                          SetUpProgressMode mode);
    void waitForAsyncProgress(ComObjPtr<Progress> &pProgressThis, ComPtr<IProgress> &pProgressAsync);
    void addWarning(const char* aWarning, ...);
    void disksWeight();
    void parseBucket(Utf8Str &aPath, Utf8Str &aBucket);

    static DECLCALLBACK(int) taskThreadImportOrExport(RTTHREAD aThread, void *pvUser);

    /*******************************************************************************
     * Read stuff
     ******************************************************************************/

    HRESULT readImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    HRESULT readFS(TaskOVF *pTask);
    HRESULT readFSOVF(TaskOVF *pTask);
    HRESULT readFSOVA(TaskOVF *pTask);
    HRESULT readFSImpl(TaskOVF *pTask, const RTCString &strFilename, PVDINTERFACEIO pCallbacks, PSHASTORAGE pStorage);
    HRESULT readS3(TaskOVF *pTask);

    /*******************************************************************************
     * Import stuff
     ******************************************************************************/

    HRESULT importImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    HRESULT importFS(TaskOVF *pTask);
    HRESULT importFSOVF(TaskOVF *pTask, AutoWriteLockBase& writeLock);
    HRESULT importFSOVA(TaskOVF *pTask, AutoWriteLockBase& writeLock);
    HRESULT importS3(TaskOVF *pTask);

    HRESULT readManifestFile(const Utf8Str &strFile, void **ppvBuf, size_t *pcbSize, PVDINTERFACEIO pCallbacks, PSHASTORAGE pStorage);
    HRESULT readTarManifestFile(RTTAR tar, const Utf8Str &strFile, void **ppvBuf, size_t *pcbSize, PVDINTERFACEIO pCallbacks, PSHASTORAGE pStorage);
    HRESULT verifyManifestFile(const Utf8Str &strFile, ImportStack &stack, void *pvBuf, size_t cbSize);

    void convertDiskAttachmentValues(const ovf::HardDiskController &hdc,
                                     uint32_t ulAddressOnParent,
                                     Bstr &controllerType,
                                     int32_t &lControllerPort,
                                     int32_t &lDevice);

    void importOneDiskImage(const ovf::DiskImage &di,
                            const Utf8Str &strTargetPath,
                            ComObjPtr<Medium> &pTargetHD,
                            ImportStack &stack,
                            PVDINTERFACEIO pCallbacks,
                            PSHASTORAGE pStorage);
    void importMachineGeneric(const ovf::VirtualSystem &vsysThis,
                              ComObjPtr<VirtualSystemDescription> &vsdescThis,
                              ComPtr<IMachine> &pNewMachine,
                              ImportStack &stack,
                              PVDINTERFACEIO pCallbacks,
                              PSHASTORAGE pStorage);
    void importVBoxMachine(ComObjPtr<VirtualSystemDescription> &vsdescThis,
                           ComPtr<IMachine> &pNewMachine,
                           ImportStack &stack,
                           PVDINTERFACEIO pCallbacks,
                           PSHASTORAGE pStorage);
    void importMachines(ImportStack &stack,
                        PVDINTERFACEIO pCallbacks,
                        PSHASTORAGE pStorage);

    /*******************************************************************************
     * Write stuff
     ******************************************************************************/

    HRESULT writeImpl(OVFFormat aFormat, const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    HRESULT writeFS(TaskOVF *pTask);
    HRESULT writeFSOVF(TaskOVF *pTask, AutoWriteLockBase& writeLock);
    HRESULT writeFSOVA(TaskOVF *pTask, AutoWriteLockBase& writeLock);
    HRESULT writeFSImpl(TaskOVF *pTask, AutoWriteLockBase& writeLock, PVDINTERFACEIO pCallbacks, PSHASTORAGE pStorage);
    HRESULT writeS3(TaskOVF *pTask);

    struct XMLStack;
    void buildXML(AutoWriteLockBase& writeLock, xml::Document &doc, XMLStack &stack, const Utf8Str &strPath, OVFFormat enFormat);
    void buildXMLForOneVirtualSystem(AutoWriteLockBase& writeLock,
                                     xml::ElementNode &elmToAddVirtualSystemsTo,
                                     std::list<xml::ElementNode*> *pllElementsWithUuidAttributes,
                                     ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                     OVFFormat enFormat,
                                     XMLStack &stack);


    friend class Machine;
};

void parseURI(Utf8Str strUri, LocationInfo &locInfo);

struct VirtualSystemDescriptionEntry
{
    uint32_t ulIndex;                       // zero-based index of this entry within array
    VirtualSystemDescriptionType_T type;    // type of this entry
    Utf8Str strRef;                         // reference number (hard disk controllers only)
    Utf8Str strOvf;                         // original OVF value (type-dependent)
    Utf8Str strVboxSuggested;               // configuration value (type-dependent); original value suggested by interpret()
    Utf8Str strVboxCurrent;                 // configuration value (type-dependent); current value, either from interpret() or setFinalValue()
    Utf8Str strExtraConfigSuggested;        // extra configuration key=value strings (type-dependent); original value suggested by interpret()
    Utf8Str strExtraConfigCurrent;          // extra configuration key=value strings (type-dependent); current value, either from interpret() or setFinalValue()

    uint32_t ulSizeMB;                      // hard disk images only: a copy of ovf::DiskImage::ulSuggestedSizeMB
};

class ATL_NO_VTABLE VirtualSystemDescription :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IVirtualSystemDescription)
{
    friend class Appliance;

public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VirtualSystemDescription, IVirtualSystemDescription)

    DECLARE_NOT_AGGREGATABLE(VirtualSystemDescription)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualSystemDescription)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IVirtualSystemDescription)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (VirtualSystemDescription)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return BaseFinalConstruct(); }
    void FinalRelease() { uninit(); BaseFinalRelease(); }

    HRESULT init();
    void uninit();

    /* IVirtualSystemDescription properties */
    STDMETHOD(COMGETTER(Count))(ULONG *aCount);

    /* IVirtualSystemDescription methods */
    STDMETHOD(GetDescription)(ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                              ComSafeArrayOut(BSTR, aRefs),
                              ComSafeArrayOut(BSTR, aOvfValues),
                              ComSafeArrayOut(BSTR, aVboxValues),
                              ComSafeArrayOut(BSTR, aExtraConfigValues));

    STDMETHOD(GetDescriptionByType)(VirtualSystemDescriptionType_T aType,
                                    ComSafeArrayOut(VirtualSystemDescriptionType_T, aTypes),
                                    ComSafeArrayOut(BSTR, aRefs),
                                    ComSafeArrayOut(BSTR, aOvfValues),
                                    ComSafeArrayOut(BSTR, aVboxValues),
                                    ComSafeArrayOut(BSTR, aExtraConfigValues));

    STDMETHOD(GetValuesByType)(VirtualSystemDescriptionType_T aType,
                               VirtualSystemDescriptionValueType_T aWhich,
                               ComSafeArrayOut(BSTR, aValues));

    STDMETHOD(SetFinalValues)(ComSafeArrayIn(BOOL, aEnabled),
                              ComSafeArrayIn(IN_BSTR, aVboxValues),
                              ComSafeArrayIn(IN_BSTR, aExtraConfigValues));

    STDMETHOD(AddDescription)(VirtualSystemDescriptionType_T aType,
                              IN_BSTR aVboxValue,
                              IN_BSTR aExtraConfigValue);

    /* public methods only for internal purposes */

    void addEntry(VirtualSystemDescriptionType_T aType,
                  const Utf8Str &strRef,
                  const Utf8Str &aOvfValue,
                  const Utf8Str &aVboxValue,
                  uint32_t ulSizeMB = 0,
                  const Utf8Str &strExtraConfig = "");

    std::list<VirtualSystemDescriptionEntry*> findByType(VirtualSystemDescriptionType_T aType);
    const VirtualSystemDescriptionEntry* findControllerFromID(uint32_t id);

    void importVboxMachineXML(const xml::ElementNode &elmMachine);
    const settings::MachineConfigFile* getMachineConfig() const;

    /* private instance data */
private:
    struct Data;
    Data *m;

    friend class Machine;
};

#endif // ____H_APPLIANCEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
