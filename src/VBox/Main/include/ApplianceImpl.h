/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
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

/* VBox forward declarations */
class Progress;
class VirtualSystemDescription;
struct VirtualSystemDescriptionEntry;

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
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IAppliance)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (Appliance)

    enum OVFFormat
    {
        unspecified,
        OVF_0_9,
        OVF_1_0
    };

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

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
    STDMETHOD(ImportMachines)(IProgress **aProgress);
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

    struct Data;            // opaque, defined in ApplianceImpl.cpp
    Data *m;

    bool isApplianceIdle();

    HRESULT searchUniqueVMName(Utf8Str& aName) const;
    HRESULT searchUniqueDiskImageFilePath(Utf8Str& aName) const;
    HRESULT getDefaultHardDiskFolder(Utf8Str &str) const;
    void waitForAsyncProgress(ComObjPtr<Progress> &pProgressThis, ComPtr<IProgress> &pProgressAsync);
    void addWarning(const char* aWarning, ...);

    void disksWeight();
    struct LocationInfo;
    enum SetUpProgressMode { ImportFileWithManifest, ImportFileNoManifest, ImportS3, WriteFile, WriteS3 };
    HRESULT setUpProgress(const LocationInfo &locInfo,
                          ComObjPtr<Progress> &pProgress,
                          const Bstr &bstrDescription,
                          SetUpProgressMode mode);

    void parseURI(Utf8Str strUri, LocationInfo &locInfo) const;
    void parseBucket(Utf8Str &aPath, Utf8Str &aBucket);
    Utf8Str manifestFileName(const Utf8Str& aPath) const;

    HRESULT readImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    struct TaskOVF;
    static DECLCALLBACK(int) taskThreadImportOrExport(RTTHREAD aThread, void *pvUser);

    HRESULT readFS(const LocationInfo &locInfo, ComObjPtr<Progress> &pProgress);
    HRESULT readFSOVF(const LocationInfo &locInfo, ComObjPtr<Progress> &pProgress);
    HRESULT readFSOVA(const LocationInfo &locInfo, ComObjPtr<Progress> &pProgress);
    HRESULT readS3(TaskOVF *pTask);

    void convertDiskAttachmentValues(const ovf::HardDiskController &hdc,
                                     uint32_t ulAddressOnParent,
                                     Bstr &controllerType,
                                     int32_t &lControllerPort,
                                     int32_t &lDevice);

    HRESULT importImpl(const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);
    HRESULT manifestVerify(const LocationInfo &locInfo, const ovf::OVFReader &reader, ComObjPtr<Progress> &pProgress);

    HRESULT importFS(TaskOVF *pTask);
    HRESULT importFSOVF(TaskOVF *pTask);
    HRESULT importFSOVA(TaskOVF *pTask);

    struct ImportStack;
    void importOneDiskImage(const ovf::DiskImage &di,
                            const Utf8Str &strTargetPath,
                            ComPtr<IMedium> &pTargetHD,
                            ImportStack &stack);
    void importMachineGeneric(const ovf::VirtualSystem &vsysThis,
                              ComObjPtr<VirtualSystemDescription> &vsdescThis,
                              ComPtr<IMachine> &pNewMachine,
                              ImportStack &stack);
    void importVBoxMachine(ComObjPtr<VirtualSystemDescription> &vsdescThis,
                           ComPtr<IMachine> &pNewMachine,
                           ImportStack &stack);

    HRESULT importS3(TaskOVF *pTask);

    HRESULT writeImpl(OVFFormat aFormat, const LocationInfo &aLocInfo, ComObjPtr<Progress> &aProgress);

    struct XMLStack;
    void buildXML(AutoWriteLockBase& writeLock, xml::Document &doc, XMLStack &stack, const Utf8Str &strPath, OVFFormat enFormat);
    void buildXMLForOneVirtualSystem(AutoWriteLockBase& writeLock,
                                     xml::ElementNode &elmToAddVirtualSystemsTo,
                                     std::list<xml::ElementNode*> *pllElementsWithUuidAttributes,
                                     ComObjPtr<VirtualSystemDescription> &vsdescThis,
                                     OVFFormat enFormat,
                                     XMLStack &stack);

    HRESULT writeFS(TaskOVF *pTask);
    HRESULT writeFSOVF(TaskOVF *pTask);
    HRESULT writeFSOVA(TaskOVF *pTask);
    HRESULT writeS3(TaskOVF *pTask);

    friend class Machine;
};

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
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualSystemDescription)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (VirtualSystemDescription)

    // public initializer/uninitializer for internal purposes only
    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

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
