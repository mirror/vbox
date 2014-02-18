
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
#include "GuestCtrlImplPrivate.h"

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

    int     init(const GuestFsObjData &objData);
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
    STDMETHOD(COMGETTER(FileAttributes))(BSTR *aFileAttrs);
    STDMETHOD(COMGETTER(GenerationId))(ULONG *aGenerationId);
    STDMETHOD(COMGETTER(GID))(ULONG *aGID);
    STDMETHOD(COMGETTER(GroupName))(BSTR *aGroupName);
    STDMETHOD(COMGETTER(HardLinks))(ULONG *aHardLinks);
    STDMETHOD(COMGETTER(ModificationTime))(LONG64 *aModificationTime);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(NodeId))(LONG64 *aNodeId);
    STDMETHOD(COMGETTER(NodeIdDevice))(ULONG *aNodeIdDevice);
    STDMETHOD(COMGETTER(ObjectSize))(LONG64 *aObjectSize);
    STDMETHOD(COMGETTER(Type))(FsObjType_T *aType);
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

    GuestFsObjData mData;
};

#endif /* !____H_GUESTFSOBJINFOIMPL */

