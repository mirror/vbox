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

#ifndef ____H_HARDDISKIMPL
#define ____H_HARDDISKIMPL

#include "VirtualBoxBase.h"

#include "VirtualBoxImpl.h"
#include "HardDiskFormatImpl.h"
#include "MediumImpl.h"

#include <VBox/com/SupportErrorInfo.h>

#include <VBox/VBoxHDD.h>

#include <map>

class Progress;

////////////////////////////////////////////////////////////////////////////////

/**
 * The HardDisk component class implements the IHardDisk interface.
 */
class ATL_NO_VTABLE HardDisk
    : public com::SupportErrorInfoDerived<MediumBase, HardDisk, IHardDisk>
    , public VirtualBoxBaseWithTypedChildrenNEXT<HardDisk>
    , public VirtualBoxSupportTranslation<HardDisk>
    , public IHardDisk
{
public:

    typedef VirtualBoxBaseWithTypedChildrenNEXT <HardDisk>::DependentChildren
        List;

    class MergeChain;
    class CloneChain;

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE (HardDisk)

    DECLARE_NOT_AGGREGATABLE (HardDisk)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (HardDisk)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY2 (IMedium, MediumBase)
        COM_INTERFACE_ENTRY (IHardDisk)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HardDisk)

    HRESULT FinalConstruct();
    void FinalRelease();

    enum HDDOpenMode  { OpenReadWrite, OpenReadOnly };
                // have to use a special enum for the overloaded init() below;
                // can't use AccessMode_T from XIDL because that's mapped to an int
                // and would be ambiguous

    // public initializer/uninitializer for internal purposes only
    HRESULT init(VirtualBox *aVirtualBox,
                 CBSTR aFormat,
                 CBSTR aLocation);
    HRESULT init(VirtualBox *aVirtualBox,
                 CBSTR aLocation,
                 HDDOpenMode enOpenMode);
    HRESULT init(VirtualBox *aVirtualBox,
                 HardDisk *aParent,
                 const settings::Key &aNode);
    void uninit();

    // IMedium properties & methods
    COM_FORWARD_IMedium_TO_BASE (MediumBase)

    // IHardDisk properties
    STDMETHOD(COMGETTER(Format)) (BSTR *aFormat);
    STDMETHOD(COMGETTER(Type)) (HardDiskType_T *aType);
    STDMETHOD(COMSETTER(Type)) (HardDiskType_T aType);
    STDMETHOD(COMGETTER(Parent)) (IHardDisk **aParent);
    STDMETHOD(COMGETTER(Children)) (ComSafeArrayOut (IHardDisk *, aChildren));
    STDMETHOD(COMGETTER(Root)) (IHardDisk **aRoot);
    STDMETHOD(COMGETTER(ReadOnly)) (BOOL *aReadOnly);
    STDMETHOD(COMGETTER(LogicalSize)) (ULONG64 *aLogicalSize);
    STDMETHOD(COMGETTER(AutoReset)) (BOOL *aAutoReset);
    STDMETHOD(COMSETTER(AutoReset)) (BOOL aAutoReset);

    // IHardDisk methods
    STDMETHOD(GetProperty) (IN_BSTR aName, BSTR *aValue);
    STDMETHOD(SetProperty) (IN_BSTR aName, IN_BSTR aValue);
    STDMETHOD(GetProperties) (IN_BSTR aNames,
                              ComSafeArrayOut (BSTR, aReturnNames),
                              ComSafeArrayOut (BSTR, aReturnValues));
    STDMETHOD(SetProperties) (ComSafeArrayIn (IN_BSTR, aNames),
                              ComSafeArrayIn (IN_BSTR, aValues));
    STDMETHOD(CreateBaseStorage) (ULONG64 aLogicalSize,
                                  HardDiskVariant_T aVariant,
                                  IProgress **aProgress);
    STDMETHOD(DeleteStorage) (IProgress **aProgress);
    STDMETHOD(CreateDiffStorage) (IHardDisk *aTarget,
                                  HardDiskVariant_T aVariant,
                                  IProgress **aProgress);
    STDMETHOD(MergeTo) (IN_GUID aTargetId, IProgress **aProgress);
    STDMETHOD(CloneTo) (IHardDisk *aTarget, HardDiskVariant_T aVariant,
                        IProgress **aProgress);
    STDMETHOD(FlattenTo) (IHardDisk *aTarget, HardDiskVariant_T aVariant,
                          IProgress **aProgress);
    STDMETHOD(Compact) (IProgress **aProgress);
    STDMETHOD(Reset) (IProgress **aProgress);

    // public methods for internal purposes only

    /**
     * Shortcut to VirtualBoxBaseWithTypedChildrenNEXT::dependentChildren().
     */
    const List &children() const { return dependentChildren(); }

    void updatePaths (const char *aOldPath, const char *aNewPath);

    ComObjPtr<HardDisk> root (uint32_t *aLevel = NULL);

    bool isReadOnly();

    HRESULT saveSettings (settings::Key &aParentNode);

    HRESULT compareLocationTo (const char *aLocation, int &aResult);

    /**
     * Shortcut to #deleteStorage() that doesn't wait for operation completion
     * and implies the progress object will be used for waiting.
     */
    HRESULT deleteStorageNoWait (ComObjPtr <Progress> &aProgress)
    { return deleteStorage (&aProgress, false /* aWait */); }

    /**
     * Shortcut to #deleteStorage() that wait for operation completion by
     * blocking the current thread.
     */
    HRESULT deleteStorageAndWait (ComObjPtr <Progress> *aProgress = NULL)
    { return deleteStorage (aProgress, true /* aWait */); }

    /**
     * Shortcut to #createDiffStorage() that doesn't wait for operation
     * completion and implies the progress object will be used for waiting.
     */
    HRESULT createDiffStorageNoWait (ComObjPtr<HardDisk> &aTarget,
                                     HardDiskVariant_T aVariant,
                                     ComObjPtr <Progress> &aProgress)
    { return createDiffStorage (aTarget, aVariant, &aProgress, false /* aWait */); }

    /**
     * Shortcut to #createDiffStorage() that wait for operation completion by
     * blocking the current thread.
     */
    HRESULT createDiffStorageAndWait (ComObjPtr<HardDisk> &aTarget,
                                      HardDiskVariant_T aVariant,
                                      ComObjPtr <Progress> *aProgress = NULL)
    { return createDiffStorage (aTarget, aVariant, aProgress, true /* aWait */); }

    HRESULT prepareMergeTo (HardDisk *aTarget, MergeChain * &aChain,
                            bool aIgnoreAttachments = false);

    /**
     * Shortcut to #mergeTo() that doesn't wait for operation completion and
     * implies the progress object will be used for waiting.
     */
    HRESULT mergeToNoWait (MergeChain *aChain,
                           ComObjPtr <Progress> &aProgress)
    { return mergeTo (aChain, &aProgress, false /* aWait */); }

    /**
     * Shortcut to #mergeTo() that wait for operation completion by
     * blocking the current thread.
     */
    HRESULT mergeToAndWait (MergeChain *aChain,
                            ComObjPtr <Progress> *aProgress = NULL)
    { return mergeTo (aChain, aProgress, true /* aWait */); }

    void cancelMergeTo (MergeChain *aChain);

    Utf8Str name();

    HRESULT prepareDiscard (MergeChain * &aChain);
    HRESULT discard (ComObjPtr <Progress> &aProgress, MergeChain *aChain);
    void cancelDiscard (MergeChain *aChain);

    /** Returns a preferred format for a differencing hard disk. */
    Bstr preferredDiffFormat();

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    ComObjPtr <HardDisk> parent() const { return static_cast <HardDisk *> (mParent); }
    HardDiskType_T type() const { return mm.type; }

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "HardDisk"; }

protected:

    HRESULT deleteStorage (ComObjPtr <Progress> *aProgress, bool aWait);

    HRESULT createDiffStorage (ComObjPtr <HardDisk> &aTarget,
                               HardDiskVariant_T aVariant,
                               ComObjPtr <Progress> *aProgress,
                               bool aWait);

    HRESULT mergeTo (MergeChain *aChain,
                     ComObjPtr <Progress> *aProgress,
                     bool aWait);

    /**
     * Returns VirtualBox::hardDiskTreeHandle(), for convenience. Don't forget
     * to follow these locking rules:
     *
     * 1. The write lock on this handle must be either held alone on the thread
     *    or requested *after* the VirtualBox object lock. Mixing with other
     *    locks is prohibited.
     *
     * 2. The read lock on this handle may be intermixed with any other lock
     *    with the exception that it must be requested *after* the VirtualBox
     *    object lock.
     */
    RWLockHandle *treeLock() { return mVirtualBox->hardDiskTreeLockHandle(); }

    /** Reimplements VirtualBoxWithTypedChildren::childrenLock() to return
     *  treeLock(). */
    RWLockHandle *childrenLock() { return treeLock(); }

private:

    HRESULT setLocation (CBSTR aLocation);
    HRESULT setFormat (CBSTR aFormat);

    virtual HRESULT queryInfo(bool fWrite);

    HRESULT canClose();
    HRESULT canAttach (const Guid &aMachineId,
                       const Guid &aSnapshotId);

    HRESULT unregisterWithVirtualBox();

    Utf8Str vdError (int aVRC);

    static DECLCALLBACK(void) vdErrorCall (void *pvUser, int rc, RT_SRC_POS_DECL,
                                           const char *pszFormat, va_list va);

    static DECLCALLBACK(int) vdProgressCall (PVM /* pVM */, unsigned uPercent,
                                             void *pvUser);

    static DECLCALLBACK(bool) vdConfigAreKeysValid (void *pvUser,
                                                    const char *pszzValid);
    static DECLCALLBACK(int) vdConfigQuerySize (void *pvUser, const char *pszName,
                                                size_t *pcbValue);
    static DECLCALLBACK(int) vdConfigQuery (void *pvUser, const char *pszName,
                                            char *pszValue, size_t cchValue);

    static DECLCALLBACK(int) taskThread (RTTHREAD thread, void *pvUser);

    /** weak parent */
    ComObjPtr <HardDisk, ComWeakRef> mParent;

    struct Task;
    friend struct Task;

    struct Data
    {
        Data() : type (HardDiskType_Normal), logicalSize (0), autoReset (false)
               , implicit (false), numCreateDiffTasks (0)
               , vdProgress (NULL) , vdDiskIfaces (NULL) {}

        const Bstr format;
        ComObjPtr <HardDiskFormat> formatObj;

        HardDiskType_T type;
        uint64_t logicalSize;   /*< In MBytes. */

        BOOL autoReset : 1;

        typedef std::map <Bstr, Bstr> PropertyMap;
        PropertyMap properties;

        bool implicit : 1;

        uint32_t numCreateDiffTasks;

        Utf8Str vdError;        /*< Error remembered by the VD error callback. */
        Progress *vdProgress;  /*< Progress for the VD progress callback. */

        VDINTERFACE vdIfError;
        VDINTERFACEERROR vdIfCallsError;

        VDINTERFACE vdIfProgress;
        VDINTERFACEPROGRESS vdIfCallsProgress;

        VDINTERFACE vdIfConfig;
        VDINTERFACECONFIG vdIfCallsConfig;

        VDINTERFACE vdIfTcpNet;
        VDINTERFACETCPNET vdIfCallsTcpNet;

        PVDINTERFACE vdDiskIfaces;
    };

    Data mm;
};

#endif /* ____H_HARDDISKIMPL */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
