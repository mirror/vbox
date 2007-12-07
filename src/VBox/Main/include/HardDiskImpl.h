/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_HARDDISKIMPL
#define ____H_HARDDISKIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#include <VBox/cfgldr.h>
#include <VBox/VBoxHDD-new.h>

#include <iprt/semaphore.h>

#include <list>

class VirtualBox;
class Progress;
class HVirtualDiskImage;

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HardDisk :
    public VirtualBoxSupportErrorInfoImpl <HardDisk, IHardDisk>,
    public VirtualBoxSupportTranslation <HardDisk>,
    public VirtualBoxBaseWithTypedChildren <HardDisk>,
    public IHardDisk
{

public:

    typedef VirtualBoxBaseWithTypedChildren <HardDisk>::DependentChildren
        HardDiskList;

    DECLARE_NOT_AGGREGATABLE(HardDisk)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HardDisk)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDisk)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

protected:

    // protected initializer/uninitializer for internal purposes only
    HRESULT protectedInit (VirtualBox *aVirtualBox, HardDisk *aParent);
    void protectedUninit (AutoLock &alock);

public:

    // IHardDisk properties
    STDMETHOD(COMGETTER(Id)) (GUIDPARAMOUT aId);
    STDMETHOD(COMGETTER(StorageType)) (HardDiskStorageType_T *aStorageType);
    STDMETHOD(COMGETTER(Location)) (BSTR *aLocation);
    STDMETHOD(COMGETTER(Type)) (HardDiskType_T *aType);
    STDMETHOD(COMSETTER(Type)) (HardDiskType_T aType);
    STDMETHOD(COMGETTER(Parent)) (IHardDisk **aParent);
    STDMETHOD(COMGETTER(Children)) (IHardDiskCollection **aChildren);
    STDMETHOD(COMGETTER(Root)) (IHardDisk **aRoot);
    STDMETHOD(COMGETTER(Accessible)) (BOOL *aAccessible);
    STDMETHOD(COMGETTER(AllAccessible)) (BOOL *aAllAccessible);
    STDMETHOD(COMGETTER(LastAccessError)) (BSTR *aLastAccessError);
    STDMETHOD(COMGETTER(MachineId)) (GUIDPARAMOUT aMachineId);
    STDMETHOD(COMGETTER(SnapshotId)) (GUIDPARAMOUT aSnapshotId);

    // IHardDisk methods
    STDMETHOD(CloneToImage) (INPTR BSTR aFilePath, IVirtualDiskImage **aImage,
                             IProgress **aProgress);

    // public methods for internal purposes only

    const Guid &id() const { return mId; }
    HardDiskStorageType_T storageType() const { return mStorageType; }
    HardDiskType_T type() const { return mType; }
    const Guid &machineId() const { return mMachineId; }
    const Guid &snapshotId() const { return mSnapshotId; }

    void setMachineId (const Guid &aId) { mMachineId = aId; }
    void setSnapshotId (const Guid &aId) { mSnapshotId = aId; }

    bool isDifferencing() const
    {
        return mType == HardDiskType_NormalHardDisk &&
               mStorageType == HardDiskStorageType_VirtualDiskImage &&
               !mParent.isNull();
    }
    bool isParentImmutable() const
    {
        AutoLock parentLock (mParent);
        return !mParent.isNull() && mParent->type() == HardDiskType_ImmutableHardDisk;
    }

    inline HVirtualDiskImage *asVDI();

    ComObjPtr <HardDisk> parent() const { return static_cast <HardDisk *> (mParent); }

    /** Shortcut to #dependentChildrenLock() */
    AutoLock::Handle &childrenLock() const { return dependentChildrenLock(); }

    /**
     *  Shortcut to #dependentChildren().
     *  Do |AutoLock alock (childrenLock());| before acceessing the returned list!
     */
    const HardDiskList &children() const { return dependentChildren(); }

    ComObjPtr <HardDisk> root() const;

    HRESULT getBaseAccessible (Bstr &aAccessError, bool aCheckBusy = false,
                               bool aCheckReaders = false);

    // virtual methods that need to be [re]implemented by every subclass

    virtual HRESULT trySetRegistered (BOOL aRegistered);
    virtual HRESULT getAccessible (Bstr &aAccessError) = 0;

    virtual HRESULT saveSettings (CFGNODE aHDNode, CFGNODE aStorageNode) = 0;

    virtual void updatePath (const char *aOldPath, const char *aNewPath) {}

    virtual Bstr toString (bool aShort = false) = 0;
    virtual bool sameAs (HardDisk *that);

    virtual HRESULT cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                                  Progress *aProgress, bool &aDeleteTarget) = 0;
    virtual HRESULT createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                                     Progress *aProgress) = 0;
public:

    void setBusy();
    void clearBusy();
    void addReader();
    void releaseReader();
    void addReaderOnAncestors();
    void releaseReaderOnAncestors();
    bool hasForeignChildren();

    HRESULT setBusyWithChildren();
    void clearBusyWithChildren();
    HRESULT getAccessibleWithChildren (Bstr &aAccessError);

    HRESULT checkConsistency();

    HRESULT createDiffHardDisk (const Bstr &aFolder, const Guid &aMachineId,
                                ComObjPtr <HVirtualDiskImage> &aHardDisk,
                                Progress *aProgress);

    void updatePaths (const char *aOldPath, const char *aNewPath);

    /* the following must be called from under the lock */
    bool isBusy() { isLockedOnCurrentThread(); return mBusy; }
    unsigned readers() { isLockedOnCurrentThread(); return mReaders; }
    const Bstr &lastAccessError() const { return mLastAccessError; }

    static HRESULT openHardDisk (VirtualBox *aVirtualBox, INPTR BSTR aLocation,
                                 ComObjPtr <HardDisk> &hardDisk);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HardDisk"; }

protected:

    HRESULT loadSettings (CFGNODE aHDNode);
    HRESULT saveSettings (CFGNODE aHDNode);

    /** weak VirualBox parent */
    ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    BOOL mRegistered;

    ComObjPtr <HardDisk, ComWeakRef> mParent;

    Guid mId;
    HardDiskStorageType_T mStorageType;
    HardDiskType_T mType;
    Guid mMachineId;
    Guid mSnapshotId;

private:

    Bstr mLastAccessError;

    bool mBusy;
    unsigned mReaders;
};

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HVirtualDiskImage :
    public HardDisk,
    public VirtualBoxSupportTranslation <HVirtualDiskImage>,
    public IVirtualDiskImage
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(HVirtualDiskImage)

    DECLARE_NOT_AGGREGATABLE(HVirtualDiskImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HVirtualDiskImage)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDisk)
        COM_INTERFACE_ENTRY(IVirtualDiskImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  CFGNODE aHDNode, CFGNODE aVDINode);
    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  const BSTR aFilePath, BOOL aRegistered = FALSE);
    void uninit();

    // IHardDisk properties
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (INPTR BSTR aDescription);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *aSize);
    STDMETHOD(COMGETTER(ActualSize)) (ULONG64 *aActualSize);

    // IVirtualDiskImage properties
    STDMETHOD(COMGETTER(FilePath)) (BSTR *aFilePath);
    STDMETHOD(COMSETTER(FilePath)) (INPTR BSTR aFilePath);
    STDMETHOD(COMGETTER(Created)) (BOOL *aCreated);

    // IVirtualDiskImage methods
    STDMETHOD(CreateDynamicImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(CreateFixedImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(DeleteImage)();

    // public methods for internal purposes only

    const Bstr &filePath() const { return mFilePath; }
    const Bstr &filePathFull() const { return mFilePathFull; }

    HRESULT trySetRegistered (BOOL aRegistered);
    HRESULT getAccessible (Bstr &aAccessError);

    HRESULT saveSettings (CFGNODE aHDNode, CFGNODE aStorageNode);

    void updatePath (const char *aOldPath, const char *aNewPath);

    Bstr toString (bool aShort = false);

    HRESULT cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget);
    HRESULT createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress);

    HRESULT cloneDiffImage (const Bstr &aFolder, const Guid &aMachineId,
                            ComObjPtr <HVirtualDiskImage> &aHardDisk,
                            Progress *aProgress);

    HRESULT mergeImageToParent (Progress *aProgress);
    HRESULT mergeImageToChildren (Progress *aProgress);

    HRESULT wipeOutImage();
    HRESULT deleteImage (bool aIgnoreErrors = false);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"VirtualDiskImage"; }

private:

    HRESULT setFilePath (const BSTR aFilePath);
    HRESULT queryInformation (Bstr *aAccessError);
    HRESULT createImage (ULONG64 aSize, BOOL aDynamic, IProgress **aProgress);

    /** VDI asynchronous operation thread function */
    static DECLCALLBACK(int) VDITaskThread (RTTHREAD thread, void *pvUser);

    enum State
    {
        NotCreated,
        Created,
        /* the following must be greater than Created */
        Accessible,
    };

    State mState;
    
    RTSEMEVENTMULTI mStateCheckSem;
    ULONG mStateCheckWaiters;

    Bstr mDescription;

    ULONG64 mSize;
    ULONG64 mActualSize;

    Bstr mFilePath;
    Bstr mFilePathFull;

    friend class HardDisk;
};

// dependent inline members
////////////////////////////////////////////////////////////////////////////////

inline HVirtualDiskImage *HardDisk::asVDI()
{
    AssertReturn (mStorageType == HardDiskStorageType_VirtualDiskImage, 0);
    return static_cast <HVirtualDiskImage *> (this);
}

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HISCSIHardDisk :
    public HardDisk,
    public VirtualBoxSupportTranslation <HISCSIHardDisk>,
    public IISCSIHardDisk
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(HISCSIHardDisk)

    DECLARE_NOT_AGGREGATABLE(HISCSIHardDisk)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HISCSIHardDisk)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDisk)
        COM_INTERFACE_ENTRY(IISCSIHardDisk)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aVirtualBox,
                  CFGNODE aHDNode, CFGNODE aISCSINode);
    HRESULT init (VirtualBox *aVirtualBox);
    void uninit();

    // IHardDisk properties
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (INPTR BSTR aDescription);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *aSize);
    STDMETHOD(COMGETTER(ActualSize)) (ULONG64 *aActualSize);

    // IISCSIHardDisk properties
    STDMETHOD(COMGETTER(Server)) (BSTR *aServer);
    STDMETHOD(COMSETTER(Server)) (INPTR BSTR aServer);
    STDMETHOD(COMGETTER(Port)) (USHORT *aPort);
    STDMETHOD(COMSETTER(Port)) (USHORT aPort);
    STDMETHOD(COMGETTER(Target)) (BSTR *aTarget);
    STDMETHOD(COMSETTER(Target)) (INPTR BSTR aTarget);
    STDMETHOD(COMGETTER(Lun)) (ULONG64 *aLun);
    STDMETHOD(COMSETTER(Lun)) (ULONG64 aLun);
    STDMETHOD(COMGETTER(UserName)) (BSTR *aUserName);
    STDMETHOD(COMSETTER(UserName)) (INPTR BSTR aUserName);
    STDMETHOD(COMGETTER(Password)) (BSTR *aPassword);
    STDMETHOD(COMSETTER(Password)) (INPTR BSTR aPassword);

    // public methods for internal purposes only

    const Bstr &server() const { return mServer; }
    USHORT port() const { return mPort; }
    const Bstr &target() const { return mTarget; }
    ULONG64 lun() const { return mLun; }
    const Bstr &userName() const { return mUserName; }
    const Bstr &password() const { return mPassword; }

    HRESULT trySetRegistered (BOOL aRegistered);
    HRESULT getAccessible (Bstr &aAccessError);

    HRESULT saveSettings (CFGNODE aHDNode, CFGNODE aStorageNode);

    Bstr toString (bool aShort = false);

    HRESULT cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget);
    HRESULT createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress);

public:

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"ISCSIHardDisk"; }

private:

    HRESULT queryInformation (Bstr &aAccessError);

    Bstr mDescription;

    ULONG64 mSize;
    ULONG64 mActualSize;

    Bstr mServer;
    USHORT mPort;
    Bstr mTarget;
    ULONG64 mLun;
    Bstr mUserName;
    Bstr mPassword;
};

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HVMDKImage :
    public HardDisk,
    public VirtualBoxSupportTranslation <HVMDKImage>,
    public IVMDKImage
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(HVMDKImage)

    DECLARE_NOT_AGGREGATABLE(HVMDKImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HVMDKImage)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDisk)
        COM_INTERFACE_ENTRY(IVMDKImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  CFGNODE aHDNode, CFGNODE aVMDKNode);
    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  INPTR BSTR aFilePath, BOOL aRegistered = FALSE);
    void uninit();

    // IHardDisk properties
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (INPTR BSTR aDescription);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *aSize);
    STDMETHOD(COMGETTER(ActualSize)) (ULONG64 *aActualSize);

    // IVirtualDiskImage properties
    STDMETHOD(COMGETTER(FilePath)) (BSTR *aFilePath);
    STDMETHOD(COMSETTER(FilePath)) (INPTR BSTR aFilePath);
    STDMETHOD(COMGETTER(Created)) (BOOL *aCreated);

    // IVirtualDiskImage methods
    STDMETHOD(CreateDynamicImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(CreateFixedImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(DeleteImage)();

    // public methods for internal purposes only

    const Bstr &filePath() const { return mFilePath; }
    const Bstr &filePathFull() const { return mFilePathFull; }

    HRESULT trySetRegistered (BOOL aRegistered);
    HRESULT getAccessible (Bstr &aAccessError);

    HRESULT saveSettings (CFGNODE aHDNode, CFGNODE aStorageNode);

    void updatePath (const char *aOldPath, const char *aNewPath);

    Bstr toString (bool aShort = false);

    HRESULT cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget);
    HRESULT createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"VMDKImage"; }

private:

    HRESULT setFilePath (const BSTR aFilePath);
    HRESULT queryInformation (Bstr *aAccessError);
    HRESULT createImage (ULONG64 aSize, BOOL aDynamic, IProgress **aProgress);

    /** VDI asynchronous operation thread function */
    static DECLCALLBACK(int) VDITaskThread (RTTHREAD thread, void *pvUser);

    static DECLCALLBACK(void) VDError (void *pvUser, int rc, RT_SRC_POS_DECL,
                                       const char *pszFormat, va_list va);

    enum State
    {
        NotCreated,
        Created,
        /* the following must be greater than Created */
        Accessible,
    };

    State mState;
    
    RTSEMEVENTMULTI mStateCheckSem;
    ULONG mStateCheckWaiters;

    Bstr mDescription;

    ULONG64 mSize;
    ULONG64 mActualSize;

    Bstr mFilePath;
    Bstr mFilePathFull;

    PVBOXHDD mContainer;

    Utf8Str mLastVDError;

    friend class HardDisk;
};

////////////////////////////////////////////////////////////////////////////////

class ATL_NO_VTABLE HCustomHardDisk :
    public HardDisk,
    public VirtualBoxSupportTranslation <HCustomHardDisk>,
    public ICustomHardDisk
{

public:

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE(HCustomHardDisk)

    DECLARE_NOT_AGGREGATABLE(HCustomHardDisk)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HCustomHardDisk)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IHardDisk)
        COM_INTERFACE_ENTRY(ICustomHardDisk)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  CFGNODE aHDNode, CFGNODE aCustomNode);
    HRESULT init (VirtualBox *aVirtualBox, HardDisk *aParent,
                  INPTR BSTR aLocation, BOOL aRegistered = FALSE);
    void uninit();

    // IHardDisk properties
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (INPTR BSTR aDescription);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *aSize);
    STDMETHOD(COMGETTER(ActualSize)) (ULONG64 *aActualSize);

    // IVirtualDiskImage properties
    STDMETHOD(COMGETTER(Location)) (BSTR *aLocation);
    STDMETHOD(COMSETTER(Location)) (INPTR BSTR aLocation);
    STDMETHOD(COMGETTER(Format))   (BSTR *aFormat);
    STDMETHOD(COMGETTER(Created))  (BOOL *aCreated);

    // IVirtualDiskImage methods
    STDMETHOD(CreateDynamicImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(CreateFixedImage) (ULONG64 aSize, IProgress **aProgress);
    STDMETHOD(DeleteImage)();

    // public methods for internal purposes only

    const Bstr &Location() const { return mLocation; }

    HRESULT trySetRegistered (BOOL aRegistered);
    HRESULT getAccessible (Bstr &aAccessError);

    HRESULT saveSettings (CFGNODE aHDNode, CFGNODE aStorageNode);

    Bstr toString (bool aShort = false);

    HRESULT cloneToImage (const Guid &aId, const Utf8Str &aTargetPath,
                          Progress *aProgress, bool &aDeleteTarget);
    HRESULT createDiffImage (const Guid &aId, const Utf8Str &aTargetPath,
                             Progress *aProgress);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"CustomHardDisk"; }

private:

    HRESULT setLocation (const BSTR aLocation);
    HRESULT queryInformation (Bstr *aAccessError);
    HRESULT createImage (ULONG64 aSize, BOOL aDynamic, IProgress **aProgress);

    /** VDI asynchronous operation thread function */
    static DECLCALLBACK(int) VDITaskThread (RTTHREAD thread, void *pvUser);

    static DECLCALLBACK(void) VDError (void *pvUser, int rc, RT_SRC_POS_DECL,
                                       const char *pszFormat, va_list va);

    enum State
    {
        NotCreated,
        Created,
        /* the following must be greater than Created */
        Accessible,
    };

    State mState;
    
    RTSEMEVENTMULTI mStateCheckSem;
    ULONG mStateCheckWaiters;

    Bstr mDescription;

    ULONG64 mSize;
    ULONG64 mActualSize;

    Bstr mLocation;
    Bstr mLocationFull;
    Bstr mFormat;

    PVBOXHDD mContainer;

    Utf8Str mLastVDError;

    friend class HardDisk;
};


COM_DECL_READONLY_ENUM_AND_COLLECTION (HardDisk)

#endif // ____H_HARDDISKIMPL
