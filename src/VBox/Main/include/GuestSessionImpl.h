
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

#ifndef ____H_GUESTSESSIONIMPL
#define ____H_GUESTSESSIONIMPL

#include "VirtualBoxBase.h"

#include "GuestProcessImpl.h"
#include "GuestDirectoryImpl.h"
#include "GuestFileImpl.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestSession :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestSession)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestSession, IGuestSession)
    DECLARE_NOT_AGGREGATABLE(GuestSession)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestSession)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestSession)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestSession)

    HRESULT init(const ComPtr<IGuest> pGuest, Utf8Str aUser, Utf8Str aPassword, Utf8Str aDomain, Utf8Str aName);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IGuestSession properties.
     * @{ */
    STDMETHOD(COMGETTER(User))(BSTR *aName);
    STDMETHOD(COMGETTER(Domain))(BSTR *aDomain);
    STDMETHOD(COMGETTER(Name))(BSTR *aName);
    STDMETHOD(COMGETTER(Id))(ULONG *aId);
    STDMETHOD(COMGETTER(Timeout))(ULONG *aTimeout);
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(BSTR, aEnvironment));
    STDMETHOD(COMGETTER(Processes))(ComSafeArrayOut(IGuestProcess *, aProcesses));
    STDMETHOD(COMGETTER(Directories))(ComSafeArrayOut(IGuestDirectory *, aDirectories));
    STDMETHOD(COMGETTER(Files))(ComSafeArrayOut(IGuestFile *, aFiles));
    /** @}  */

    /** @name IGuestSession methods.
     * @{ */
    STDMETHOD(Close)(void);
    STDMETHOD(CopyFrom)(BSTR aSource, BSTR aDest, ComSafeArrayIn(ULONG, aFlags), IProgress **aProgress);
    STDMETHOD(CopyTo)(BSTR aSource, BSTR aDest, ComSafeArrayIn(ULONG, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryCreate)(BSTR aPath, ULONG aMode, ULONG aFlags, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryCreateTemp)(BSTR aTemplate, ULONG aMode, BSTR aName, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryExists)(BSTR aPath, BOOL *aExists);
    STDMETHOD(DirectoryOpen)(BSTR aPath, BSTR aFilter, BSTR aFlags, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryQueryInfo)(BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(DirectoryRemove)(BSTR aPath);
    STDMETHOD(DirectoryRemoveRecursive)(BSTR aPath, ComSafeArrayIn(DirectoryRemoveRecFlag, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryRename)(BSTR aSource, BSTR aDest, ComSafeArrayIn(DirectoryRenameFlag, aFlags));
    STDMETHOD(DirectorySetACL)(BSTR aPath, BSTR aACL);
    STDMETHOD(EnvironmentClear)(void);
    STDMETHOD(EnvironmentSet)(BSTR aName, BSTR aValue);
    STDMETHOD(EnvironmentSetArray)(ComSafeArrayIn(BSTR, aValues));
    STDMETHOD(EnvironmentUnset)(BSTR aName);
    STDMETHOD(FileCreateTemp)(BSTR aTemplate, ULONG aMode, BSTR aName, IGuestFile **aFile);
    STDMETHOD(FileExists)(BSTR aPath, BOOL *aExists);
    STDMETHOD(FileOpen)(BSTR aPath, BSTR aOpenMode, BSTR aDisposition, ULONG aCreationMode, LONG64 aOffset, IGuestFile **aFile);
    STDMETHOD(FileQueryInfo)(BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(FileQuerySize)(BSTR aPath, LONG64 *aSize);
    STDMETHOD(FileRemove)(BSTR aPath);
    STDMETHOD(FileRename)(BSTR aSource, BSTR aDest, ComSafeArrayIn(DirectoryRenameFlag, aFlags));
    STDMETHOD(FileSetACL)(BSTR aPath, BSTR aACL);
    STDMETHOD(ProcessCreate)(BSTR aCommand, ComSafeArrayIn(BSTR, aArguments), ComSafeArrayIn(BSTR, aEnvironment),
                             ComSafeArrayIn(ProcessCreateFlag, aFlags), ULONG aTimeoutMS, IGuestProcess **IGuestProcess);
    STDMETHOD(ProcessCreateEx)(BSTR aCommand, ComSafeArrayIn(BSTR, aArguments), ComSafeArrayIn(BSTR, aEnvironment),
                               ComSafeArrayIn(ProcessCreateFlag, aFlags), ULONG aTimeoutMS,
                               ProcessPriority aPriority, ComSafeArrayIn(ULONG, aAffinity),
                               IGuestProcess **IGuestProcess);
    STDMETHOD(ProcessGet)(ULONG aPID, IGuestProcess **IGuestProcess);
    STDMETHOD(SetTimeout)(ULONG aTimeoutMS);
    STDMETHOD(SymlinkCreate)(BSTR aSource, BSTR aTarget, SymlinkType aType);
    STDMETHOD(SymlinkExists)(BSTR aSymlink, BOOL *aExists);
    STDMETHOD(SymlinkRead)(BSTR aSymlink, ComSafeArrayIn(SymlinkReadFlag, aFlags), BSTR *aTarget);
    STDMETHOD(SymlinkRemoveDirectory)(BSTR aPath);
    STDMETHOD(SymlinkRemoveFile)(BSTR aFile);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    typedef std::map <Utf8Str, Utf8Str> SessionEnvironment;
    typedef std::list <ComObjPtr<GuestProcess> > SessionProcesses;
    typedef std::list <ComObjPtr<GuestDirectory> > SessionDirectories;
    typedef std::list <ComObjPtr<GuestFile> > SessionFiles;

    struct Data
    {
        ComPtr<IGuest>       mParent;
        Utf8Str              mUser;
        Utf8Str              mDomain;
        Utf8Str              mPassword;
        Utf8Str              mName;
        ULONG                mId;
        ULONG                mTimeout;
        SessionEnvironment   mEnvironment;
        SessionProcesses     mProcesses;
        SessionDirectories   mDirectories;
        SessionFiles         mFiles;
    } mData;
};

#endif /* !____H_GUESTSESSIONIMPL */

