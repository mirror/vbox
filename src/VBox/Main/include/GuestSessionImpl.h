
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

#include "GuestCtrlImplPrivate.h"
#include "GuestProcessImpl.h"
#include "GuestDirectoryImpl.h"
#include "GuestFileImpl.h"
#include "GuestFsObjInfoImpl.h"

#include <iprt/isofs.h> /* For UpdateAdditions. */

class Guest;

/**
 * Abstract base class for a lenghtly per-session operation which
 * runs in a Main worker thread.
 */
class GuestSessionTask
{
public:

    GuestSessionTask(GuestSession *pSession);

    virtual ~GuestSessionTask(void);

public:

    virtual int Run(void) = 0;
    virtual int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress) = 0;

    int setProgress(ULONG uPercent);
    int setProgressSuccess(void);
    HRESULT setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg);

protected:

    Utf8Str                 mDesc;
    GuestSession           *mSession;
    /** Progress object for getting updated when running
     *  asynchronously. Optional. */
    ComObjPtr<Progress>     mProgress;
};

/**
 * Task for copying files from host to the guest.
 */
class SessionTaskCopyTo : public GuestSessionTask
{
public:

    SessionTaskCopyTo(GuestSession *pSession,
                      const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags);

    SessionTaskCopyTo(GuestSession *pSession,
                      PRTFILE pSourceFile, size_t cbSourceOffset, uint64_t cbSourceSize,
                      const Utf8Str &strDest, uint32_t uFlags);

    virtual ~SessionTaskCopyTo(void);

public:

    int Run(void);
    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);
    static int taskThread(RTTHREAD Thread, void *pvUser);

protected:

    Utf8Str  mSource;
    PRTFILE  mSourceFile;
    size_t   mSourceOffset;
    uint64_t mSourceSize;
    Utf8Str  mDest;
    uint32_t mCopyFileFlags;
};

/**
 * Task for copying files from guest to the host.
 */
class SessionTaskCopyFrom : public GuestSessionTask
{
public:

    SessionTaskCopyFrom(GuestSession *pSession,
                        const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags);

    virtual ~SessionTaskCopyFrom(void);

public:

    int Run(void);
    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);
    static int taskThread(RTTHREAD Thread, void *pvUser);

protected:

    Utf8Str  mSource;
    Utf8Str  mDest;
    uint32_t mFlags;
};

/**
 * Task for automatically updating the Guest Additions on the guest.
 */
class SessionTaskUpdateAdditions : public GuestSessionTask
{
public:

    SessionTaskUpdateAdditions(GuestSession *pSession,
                               const Utf8Str &strSource, uint32_t uFlags);

    virtual ~SessionTaskUpdateAdditions(void);

public:

    int Run(void);
    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);
    static int taskThread(RTTHREAD Thread, void *pvUser);

protected:

    int copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO,
                        Utf8Str const &strFileSource, const Utf8Str &strFileDest,
                        bool fOptional, uint32_t *pcbSize);
    int runFile(GuestSession *pSession, GuestProcessStartupInfo &procInfo);

    /** The (optionally) specified Guest Additions .ISO on the host
     *  which will be used for the updating process. */
    Utf8Str  mSource;
    /** Update flags. */
    uint32_t mFlags;
};

/**
 * Guest session implementation.
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

    int     init(Guest *aGuest, ULONG aSessionID, Utf8Str aUser, Utf8Str aPassword, Utf8Str aDomain, Utf8Str aName);
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
    STDMETHOD(COMSETTER(Timeout))(ULONG aTimeout);
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(BSTR, aEnvironment));
    STDMETHOD(COMSETTER(Environment))(ComSafeArrayIn(IN_BSTR, aEnvironment));
    STDMETHOD(COMGETTER(Processes))(ComSafeArrayOut(IGuestProcess *, aProcesses));
    STDMETHOD(COMGETTER(Directories))(ComSafeArrayOut(IGuestDirectory *, aDirectories));
    STDMETHOD(COMGETTER(Files))(ComSafeArrayOut(IGuestFile *, aFiles));
    /** @}  */

    /** @name IGuestSession methods.
     * @{ */
    STDMETHOD(Close)(void);
    STDMETHOD(CopyFrom)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress);
    STDMETHOD(CopyTo)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(CopyFileFlag_T, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryCreate)(IN_BSTR aPath, ULONG aMode, ComSafeArrayIn(DirectoryCreateFlag_T, aFlags), IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryCreateTemp)(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aPath, BOOL aSecure, BSTR *aDirectory);
    STDMETHOD(DirectoryExists)(IN_BSTR aPath, BOOL *aExists);
    STDMETHOD(DirectoryOpen)(IN_BSTR aPath, IN_BSTR aFilter, ComSafeArrayIn(DirectoryOpenFlag_T, aFlags), IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryQueryInfo)(IN_BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(DirectoryRemove)(IN_BSTR aPath);
    STDMETHOD(DirectoryRemoveRecursive)(IN_BSTR aPath, ComSafeArrayIn(DirectoryRemoveRecFlag_T, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryRename)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags));
    STDMETHOD(DirectorySetACL)(IN_BSTR aPath, IN_BSTR aACL);
    STDMETHOD(EnvironmentClear)(void);
    STDMETHOD(EnvironmentGet)(IN_BSTR aName, BSTR *aValue);
    STDMETHOD(EnvironmentSet)(IN_BSTR aName, IN_BSTR aValue);
    STDMETHOD(EnvironmentUnset)(IN_BSTR aName);
    STDMETHOD(FileCreateTemp)(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aPath, BOOL aSecure, IGuestFile **aFile);
    STDMETHOD(FileExists)(IN_BSTR aPath, BOOL *aExists);
    STDMETHOD(FileRemove)(IN_BSTR aPath);
    STDMETHOD(FileOpen)(IN_BSTR aPath, IN_BSTR aOpenMode, IN_BSTR aDisposition, ULONG aCreationMode, LONG64 aOffset, IGuestFile **aFile);
    STDMETHOD(FileQueryInfo)(IN_BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(FileQuerySize)(IN_BSTR aPath, LONG64 *aSize);
    STDMETHOD(FileRename)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag_T, aFlags));
    STDMETHOD(FileSetACL)(IN_BSTR aPath, IN_BSTR aACL);
    STDMETHOD(ProcessCreate)(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                             ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS, IGuestProcess **aProcess);
    STDMETHOD(ProcessCreateEx)(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                               ComSafeArrayIn(ProcessCreateFlag_T, aFlags), ULONG aTimeoutMS,
                               ProcessPriority_T aPriority, ComSafeArrayIn(LONG, aAffinity),
                               IGuestProcess **aProcess);
    STDMETHOD(ProcessGet)(ULONG aPID, IGuestProcess **aProcess);
#if 0
    STDMETHOD(SetTimeout)(ULONG aTimeoutMS);
#endif
    STDMETHOD(SymlinkCreate)(IN_BSTR aSource, IN_BSTR aTarget, SymlinkType_T aType);
    STDMETHOD(SymlinkExists)(IN_BSTR aSymlink, BOOL *aExists);
    STDMETHOD(SymlinkRead)(IN_BSTR aSymlink, ComSafeArrayIn(SymlinkReadFlag_T, aFlags), BSTR *aTarget);
    STDMETHOD(SymlinkRemoveDirectory)(IN_BSTR aPath);
    STDMETHOD(SymlinkRemoveFile)(IN_BSTR aFile);
    /** @}  */

private:

    typedef std::vector <ComObjPtr<GuestDirectory> > SessionDirectories;
    typedef std::vector <ComObjPtr<GuestFile> > SessionFiles;
    /** Map of guest processes. The key specifies the internal process number.
     *  To retrieve the process' guest PID use the Id() method of the IProcess interface. */
    typedef std::map <uint32_t, ComObjPtr<GuestProcess> > SessionProcesses;

public:
    /** @name Public internal methods.
     * @{ */
    int                     directoryClose(ComObjPtr<GuestDirectory> pDirectory);
    int                     directoryCreateInternal(const Utf8Str &strPath, uint32_t uMode, uint32_t uFlags, ComObjPtr<GuestDirectory> &pDirectory);
    int                     objectCreateTempInternal(Utf8Str strTemplate,
                                                     Utf8Str strPath,
                                                     bool fDirectory,
                                                     Utf8Str &strName,
                                                     int *prc);
    int                     directoryOpenInternal(const Utf8Str &strPath, const Utf8Str &strFilter, uint32_t uFlags, ComObjPtr<GuestDirectory> &pDirectory);
    int                     dispatchToProcess(uint32_t uContextID, uint32_t uFunction, void *pvData, size_t cbData);
    int                     fileClose(ComObjPtr<GuestFile> pFile);
    int                     fileRemoveInternal(Utf8Str strPath, int *prc);
    int                     fileOpenInternal(const Utf8Str &strPath, const Utf8Str &strOpenMode, const Utf8Str &strDisposition,
                                             uint32_t uCreationMode, int64_t iOffset, ComObjPtr<GuestFile> &pFile);
    int                     fileQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData);
    int                     fileQuerySizeInternal(const Utf8Str &strPath, int64_t *pllSize);
    const GuestCredentials &getCredentials(void);
    const GuestEnvironment &getEnvironment(void);
    Utf8Str                 getName(void);
    Guest                  *getParent(void) { return mData.mParent; }
    uint32_t                getProtocolVersion(void) { return mData.mProtocolVersion; }
    int                     processClose(ComObjPtr<GuestProcess> pProcess);
    int                     processCreateExInteral(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProgress);
    inline bool             processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess);
    inline int              processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess);
    int                     startTaskAsync(const Utf8Str &strTaskDesc, GuestSessionTask *pTask, ComObjPtr<Progress> &pProgress);
    int                     queryInfo(void);
    /** @}  */

private:

    struct Data
    {
        /** Guest control protocol version to be used.
         *  Guest Additions < VBox 4.2 have version 1,
         *  any newer version will have version 2. */
        uint32_t             mProtocolVersion;
        /** Flag indicating if this is an internal session
         *  or not. Internal session are not accessible by clients. */
        bool                 fInternal;
        /** Pointer to the parent (Guest). */
        Guest               *mParent;
        /** The session credentials. */
        GuestCredentials     mCredentials;
        /** The (optional) session name. */
        Utf8Str              mName;
        /** The session ID. */
        ULONG                mId;
        /** The session timeout. Default is 30s. */
        ULONG                mTimeout;
        /** The session's environment block. Can be
         *  overwritten/extended by ProcessCreate(Ex). */
        GuestEnvironment     mEnvironment;
        /** Directory objects bound to this session. */
        SessionDirectories   mDirectories;
        /** File objects bound to this session. */
        SessionFiles         mFiles;
        /** Process objects bound to this session. */
        SessionProcesses     mProcesses;
    } mData;
};

#endif /* !____H_GUESTSESSIONIMPL */

