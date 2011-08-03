/* $Id$ */
/** @file
 * VirtualBox Main - interface for guest directory entries, VBoxC.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTDIRENTRYIMPL
#define ____H_GUESTDIRENTRYIMPL

#include "VirtualBoxBase.h"

class Guest;
class GuestProcessStreamBlock;

/**
 * A directory entry.
 */
class ATL_NO_VTABLE GuestDirEntry :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestDirEntry)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestDirEntry, IGuestDirEntry)
    DECLARE_NOT_AGGREGATABLE(GuestDirEntry)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestDirEntry)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestDirEntry)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestDirEntry)

    HRESULT init(Guest *aParent, GuestProcessStreamBlock &streamBlock);
    void    uninit();
    HRESULT FinalConstruct();
    void    FinalRelease();
    /** @}  */

    /** @name IGuestDirEntry properties
     * @{ */
    STDMETHOD(COMGETTER(NodeId))(LONG64 *aNodeId);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Type))(GuestDirEntryType_T *aType);
    /** @}  */

public:
    /** @name Public internal methods
     * @{ */
    static GuestDirEntryType_T fileTypeToEntryType(const char *pszFileType);
    /** @}  */

private:
    struct Data
    {
        /** The entry's node ID. */
        LONG64              mNodeId;
        /** The entry's name. */
        Bstr                mName;
        /** The entry's type. */
        GuestDirEntryType_T mType;
    } mData;
};

#endif /* !____H_GUESTDIRENTRYIMPL */

