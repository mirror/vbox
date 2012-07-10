
/* $Id$ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTFSOBJINFOIMPL
#define ____H_GUESTFSOBJINFOIMPL

#include "VirtualBoxBase.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestFsObjInfo :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestFsObjInfo)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestFsObjInfo, IGuestFsObjInfo)
    DECLARE_NOT_AGGREGATABLE(GuestFsObjInfo)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestFsObjInfo)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestFsObjInfo)
        COM_INTERFACE_ENTRY(IFsObjInfo)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestFsObjInfo)

    HRESULT init(void);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IFsObjInfo interface.
     * @{ */
    STDMETHOD(COMGETTER(AccessTime))(LONG64 *aAccessTime);
    STDMETHOD(COMGETTER(AllocatedSize))(LONG64 *aAllocatedSize);
    STDMETHOD(COMGETTER(BirthTime))(LONG64 *aBirthTime);
    STDMETHOD(COMGETTER(ChangeTime))(LONG64 *aChangeTime);
    STDMETHOD(COMGETTER(DeviceNumber))(ULONG *aDeviceNumber);
    STDMETHOD(COMGETTER(FileAttrs))(BSTR *aFileAttrs);
    STDMETHOD(COMGETTER(GenerationID))(ULONG *aGenerationID);
    STDMETHOD(COMGETTER(GID))(ULONG *aGID);
    STDMETHOD(COMGETTER(GroupName))(BSTR *aGroupName);
    STDMETHOD(COMGETTER(HardLinks))(ULONG *aHardLinks);
    STDMETHOD(COMGETTER(ModificationTime))(LONG64 *aModificationTime);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(NodeID))(LONG64 *aNodeID);
    STDMETHOD(COMGETTER(NodeIDDevice))(ULONG *aNodeIDDevice);
    STDMETHOD(COMGETTER(ObjectSize))(ULONG *aObjectSize);
    STDMETHOD(COMGETTER(Type))(FsObjType *aType);
    STDMETHOD(COMGETTER(UID))(ULONG *aUID);
    STDMETHOD(COMGETTER(UserFlags))(ULONG *aUserFlags);
    STDMETHOD(COMGETTER(UserName))(BSTR *aUserName);
    STDMETHOD(COMGETTER(ACL))(BSTR *aACL);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    struct Data
    {
        LONG64               mAccessTime;
        LONG64               mAllocatedSize;
        LONG64               mBirthTime;
        LONG64               mChangeTime;
        ULONG                mDeviceNumber;
        Utf8Str              mFileAttrs;
        ULONG                mGenerationID;
        ULONG                mGID;
        Utf8Str              mGroupName;
        ULONG                mNumHardLinks;
        LONG64               mModificationTime;
        Utf8Str              mName;
        LONG64               mNodeID;
        ULONG                mNodeIDDevice;
        LONG64               mObjectSize;
        FsObjType            mType;
        ULONG                mUID;
        ULONG                mUserFlags;
        Utf8Str              mUserName;
        Utf8Str              mACL;
    } mData;
};

#endif /* !____H_GUESTFSOBJINFOIMPL */

