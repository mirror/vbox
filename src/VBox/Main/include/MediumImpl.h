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

#ifndef ____H_MEDIUMIMPL
#define ____H_MEDIUMIMPL

#include "VirtualBoxBase.h"

#include <VBox/com/SupportErrorInfo.h>

#include <list>
#include <algorithm>

class VirtualBox;

////////////////////////////////////////////////////////////////////////////////

/**
 * Base component class for all media types.
 *
 * Provides the basic implementation of the IMedium interface.
 *
 * @note Subclasses must initialize the mVirtualBox data member in their init()
 *       implementations with the valid VirtualBox instance because some
 *       MediaBase methods call its methods.
 */
class ATL_NO_VTABLE MediumBase :
    virtual public VirtualBoxBaseProto,
    public com::SupportErrorInfoBase,
    public VirtualBoxSupportTranslation <MediumBase>,
    VBOX_SCRIPTABLE_IMPL(IMedium)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (MediumBase)

    DECLARE_EMPTY_CTOR_DTOR (MediumBase)

    /** Describes how a machine refers to this image. */
    struct BackRef
    {
        /** Equality predicate for stdc++. */
        struct EqualsTo : public std::unary_function <BackRef, bool>
        {
            explicit EqualsTo (const Guid &aMachineId) : machineId (aMachineId) {}

            bool operator() (const argument_type &aThat) const
            {
                return aThat.machineId == machineId;
            }

            const Guid machineId;
        };

        typedef std::list <Guid> GuidList;

        BackRef() : inCurState (false) {}

        BackRef (const Guid &aMachineId, const Guid &aSnapshotId = Guid::Empty)
            : machineId (aMachineId)
            , inCurState (aSnapshotId.isEmpty())
        {
            if (!aSnapshotId.isEmpty())
                snapshotIds.push_back (aSnapshotId);
        }

        Guid machineId;
        bool inCurState : 1;
        GuidList snapshotIds;
    };

    typedef std::list <BackRef> BackRefList;

    // IMedium properties
    STDMETHOD(COMGETTER(Id)) (BSTR *aId);
    STDMETHOD(COMGETTER(Description)) (BSTR *aDescription);
    STDMETHOD(COMSETTER(Description)) (IN_BSTR aDescription);
    STDMETHOD(COMGETTER(State)) (MediaState_T *aState);
    STDMETHOD(COMGETTER(Location)) (BSTR *aLocation);
    STDMETHOD(COMSETTER(Location)) (IN_BSTR aLocation);
    STDMETHOD(COMGETTER(Name)) (BSTR *aName);
    STDMETHOD(COMGETTER(Size)) (ULONG64 *aSize);
    STDMETHOD(COMGETTER(LastAccessError)) (BSTR *aLastAccessError);
    STDMETHOD(COMGETTER(MachineIds)) (ComSafeArrayOut (BSTR, aMachineIds));

    // IMedium methods
    STDMETHOD(GetSnapshotIds) (IN_BSTR aMachineId,
                               ComSafeArrayOut (BSTR, aSnapshotIds));
    STDMETHOD(LockRead) (MediaState_T *aState);
    STDMETHOD(UnlockRead) (MediaState_T *aState);
    STDMETHOD(LockWrite) (MediaState_T *aState);
    STDMETHOD(UnlockWrite) (MediaState_T *aState);
    STDMETHOD(Close)();

    // public methods for internal purposes only

    HRESULT updatePath (const char *aOldPath, const char *aNewPath);

    HRESULT attachTo (const Guid &aMachineId,
                      const Guid &aSnapshotId = Guid::Empty);
    HRESULT detachFrom (const Guid &aMachineId,
                        const Guid &aSnapshotId = Guid::Empty);

    // unsafe inline public methods for internal purposes only (ensure there is
    // a caller and a read lock before calling them!)

    const Guid &id() const { return m.id; }
    MediaState_T state() const { return m.state; }
    const Bstr &location() const { return m.location; }
    const Bstr &locationFull() const { return m.locationFull; }
    const BackRefList &backRefs() const { return m.backRefs; }

    bool isAttachedTo (const Guid &aMachineId)
    {
        BackRefList::iterator it =
            std::find_if (m.backRefs.begin(), m.backRefs.end(),
                          BackRef::EqualsTo (aMachineId));
        return it != m.backRefs.end() && it->inCurState;
    }

protected:

    virtual Utf8Str name();

    virtual HRESULT setLocation (CBSTR aLocation);
    virtual HRESULT queryInfo();

    /**
     * Performs extra checks if the medium can be closed and returns S_OK in
     * this case. Otherwise, returns a respective error message. Called by
     * Close() from within this object's AutoMayUninitSpan and from under
     * mVirtualBox write lock.
     */
    virtual HRESULT canClose() { return S_OK; }

    /**
     * Performs extra checks if the medium can be attached to the specified
     * VM and shapshot at the given time and returns S_OK in this case.
     * Otherwise, returns a respective error message. Called by attachTo() from
     * within this object's AutoWriteLock.
     */
    virtual HRESULT canAttach (const Guid & /* aMachineId */,
                               const Guid & /* aSnapshotId */)
    { return S_OK; }

    /**
     * Unregisters this medium with mVirtualBox. Called by Close() from within
     * this object's AutoMayUninitSpan and from under mVirtualBox write lock.
     */
    virtual HRESULT unregisterWithVirtualBox() = 0;

    HRESULT setStateError();

    /** weak VirtualBox parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

    struct Data
    {
        Data() : state (MediaState_NotCreated), size (0), readers (0)
               , queryInfoSem (NIL_RTSEMEVENTMULTI)
               , queryInfoCallers (0), accessibleInLock (false) {}

        const Guid id;
        Bstr description;
        MediaState_T state;
        Bstr location;
        Bstr locationFull;
        uint64_t size;
        Bstr lastAccessError;

        BackRefList backRefs;

        size_t readers;

        RTSEMEVENTMULTI queryInfoSem;
        size_t queryInfoCallers;

        bool accessibleInLock : 1;
    };

    Data m;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Base component class for simple image file based media such as CD/DVD ISO
 * images or Floppy images.
 *
 * Adds specific protectedInit() and saveSettings() methods that can load image
 * data from the settings files.
 */
class ATL_NO_VTABLE ImageMediumBase
    : public MediumBase
    , public VirtualBoxBaseNEXT
{
public:

    HRESULT FinalConstruct() { return S_OK; }
    void FinalRelease() { uninit(); }

protected:

    // protected initializer/uninitializer for internal purposes only
    HRESULT protectedInit (VirtualBox *aVirtualBox, CBSTR aLocation,
                           const Guid &aId);
    HRESULT protectedInit (VirtualBox *aVirtualBox, const settings::Key &aImageNode);
    void protectedUninit();

public:

    // public methods for internal purposes only
    HRESULT saveSettings (settings::Key &aImagesNode);
};

////////////////////////////////////////////////////////////////////////////////

/**
 * The DVDImage component class implements the IDVDImage interface.
 */
class ATL_NO_VTABLE DVDImage
    : public com::SupportErrorInfoDerived<ImageMediumBase, DVDImage, IDVDImage>
    , public VirtualBoxSupportTranslation<DVDImage>
    , VBOX_SCRIPTABLE_IMPL(IDVDImage)
{
public:

    COM_FORWARD_IMedium_TO_BASE (ImageMediumBase)

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE (DVDImage)

    DECLARE_NOT_AGGREGATABLE (DVDImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (DVDImage)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY2 (IMedium, ImageMediumBase)
        COM_INTERFACE_ENTRY  (IDVDImage)
        COM_INTERFACE_ENTRY2 (IDispatch, IDVDImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (DVDImage)

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aParent, CBSTR aFilePath,
                  const Guid &aId)
    {
        return protectedInit (aParent, aFilePath, aId);
    }

    HRESULT init (VirtualBox *aParent, const settings::Key &aImageNode)
    {
        return protectedInit (aParent, aImageNode);
    }

    void uninit() { protectedUninit(); }

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "DVDImage"; }

private:

    HRESULT unregisterWithVirtualBox();
};

////////////////////////////////////////////////////////////////////////////////

/**
 * The FloppyImage component class implements the IFloppyImage interface.
 */
class ATL_NO_VTABLE FloppyImage
    : public com::SupportErrorInfoDerived <ImageMediumBase, FloppyImage, IFloppyImage>
    , public VirtualBoxSupportTranslation <FloppyImage>
    , VBOX_SCRIPTABLE_IMPL(IFloppyImage)
{
public:

    COM_FORWARD_IMedium_TO_BASE (ImageMediumBase)

    VIRTUALBOXSUPPORTTRANSLATION_OVERRIDE (FloppyImage)

    DECLARE_NOT_AGGREGATABLE (FloppyImage)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (FloppyImage)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY2 (IMedium, ImageMediumBase)
        COM_INTERFACE_ENTRY  (IFloppyImage)
        COM_INTERFACE_ENTRY2 (IDispatch, IFloppyImage)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (FloppyImage)

    // public initializer/uninitializer for internal purposes only

    HRESULT init (VirtualBox *aParent, CBSTR aFilePath,
                  const Guid &aId)
    {
        return protectedInit (aParent, aFilePath, aId);
    }

    HRESULT init (VirtualBox *aParent, const settings::Key &aImageNode)
    {
        return protectedInit (aParent, aImageNode);
    }

    void uninit() { protectedUninit(); }

    /** For com::SupportErrorInfoImpl. */
    static const char *ComponentName() { return "FloppyImage"; }

private:

    HRESULT unregisterWithVirtualBox();
};

#endif /* ____H_MEDIUMIMPL */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
