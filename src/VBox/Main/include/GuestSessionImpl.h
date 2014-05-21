/* $Id$ */
/** @file
 * VirtualBox Main - Guest session handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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

#include "GuestSessionWrap.h"
#include "EventImpl.h"

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

protected:

    int getGuestProperty(const ComObjPtr<Guest> &pGuest,
                           const Utf8Str &strPath, Utf8Str &strValue);
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
 * Task for opening a guest session.
 */
class SessionTaskOpen : public GuestSessionTask
{
public:

    SessionTaskOpen(GuestSession *pSession,
                    uint32_t uFlags,
                    uint32_t uTimeoutMS);

    virtual ~SessionTaskOpen(void);

public:

    int Run(int *pGuestRc);
    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);
    static int taskThread(RTTHREAD Thread, void *pvUser);

protected:

    /** Session creation flags. */
    uint32_t mFlags;
    /** Session creation timeout (in ms). */
    uint32_t mTimeoutMS;
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
                               const Utf8Str &strSource, const ProcessArguments &aArguments,
                               uint32_t uFlags);

    virtual ~SessionTaskUpdateAdditions(void);

public:

    int Run(void);
    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);
    static int taskThread(RTTHREAD Thread, void *pvUser);

protected:

    /**
     * Suported OS types for automatic updating.
     */
    enum eOSType
    {
        eOSType_Unknown = 0,
        eOSType_Windows = 1,
        eOSType_Linux   = 2,
        eOSType_Solaris = 3
    };

    /**
     * Structure representing a file to
     * get off the .ISO, copied to the guest.
     */
    struct InstallerFile
    {
        InstallerFile(const Utf8Str &aSource,
                      const Utf8Str &aDest,
                      uint32_t       aFlags = 0)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags) { }

        InstallerFile(const Utf8Str          &aSource,
                      const Utf8Str          &aDest,
                      uint32_t                aFlags,
                      GuestProcessStartupInfo startupInfo)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags),
              mProcInfo(startupInfo)
        {
            mProcInfo.mCommand = strDest;
            if (mProcInfo.mName.isEmpty())
                mProcInfo.mName = strDest;
        }

        /** Source file on .ISO. */
        Utf8Str                 strSource;
        /** Destination file on the guest. */
        Utf8Str                 strDest;
        /** File flags. */
        uint32_t                fFlags;
        /** Optional arguments if this file needs to be
         *  executed. */
        GuestProcessStartupInfo mProcInfo;
    };

    int i_addProcessArguments(ProcessArguments &aArgumentsDest,
                              const ProcessArguments &aArgumentsSource);
    int i_copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO,
                          Utf8Str const &strFileSource, const Utf8Str &strFileDest,
                          bool fOptional, uint32_t *pcbSize);
    int i_runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo);

    /** Files to handle. */
    std::vector<InstallerFile> mFiles;
    /** The (optionally) specified Guest Additions .ISO on the host
     *  which will be used for the updating process. */
    Utf8Str                    mSource;
    /** (Optional) installer command line arguments. */
    ProcessArguments           mArguments;
    /** Update flags. */
    uint32_t                   mFlags;
};

/**
 * Guest session implementation.
 */
class ATL_NO_VTABLE GuestSession :
    public GuestSessionWrap,
    public GuestBase
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    DECLARE_EMPTY_CTOR_DTOR(GuestSession)

    int     init(Guest *pGuest, const GuestSessionStartupInfo &ssInfo, const GuestCredentials &guestCreds);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

private:

    /** Wrapped @name IGuestSession properties.
     * @{ */
    HRESULT getUser(com::Utf8Str &aUser);
    HRESULT getDomain(com::Utf8Str &aDomain);
    HRESULT getName(com::Utf8Str &aName);
    HRESULT getId(ULONG *aId);
    HRESULT getTimeout(ULONG *aTimeout);
    HRESULT setTimeout(ULONG aTimeout);
    HRESULT getProtocolVersion(ULONG *aProtocolVersion);
    HRESULT getStatus(GuestSessionStatus_T *aStatus);
    HRESULT getEnvironment(std::vector<com::Utf8Str> &aEnvironment);
    HRESULT setEnvironment(const std::vector<com::Utf8Str> &aEnvironment);
    HRESULT getProcesses(std::vector<ComPtr<IGuestProcess> > &aProcesses);
    HRESULT getDirectories(std::vector<ComPtr<IGuestDirectory> > &aDirectories);
    HRESULT getFiles(std::vector<ComPtr<IGuestFile> > &aFiles);
    HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);
    /** @}  */

    /** Wrapped @name IGuestSession methods.
     * @{ */
    HRESULT close();
    HRESULT copyFrom(const com::Utf8Str &aSource,
                     const com::Utf8Str &aDest,
                     const std::vector<CopyFileFlag_T> &aFlags,
                     ComPtr<IProgress> &aProgress);
    HRESULT copyTo(const com::Utf8Str &aSource,
                   const com::Utf8Str &aDest,
                   const std::vector<CopyFileFlag_T> &aFlags,
                   ComPtr<IProgress> &aProgress);
    HRESULT directoryCreate(const com::Utf8Str &aPath,
                            ULONG aMode,
                            const std::vector<DirectoryCreateFlag_T> &aFlags);
    HRESULT directoryCreateTemp(const com::Utf8Str &aTemplateName,
                                ULONG aMode,
                                const com::Utf8Str &aPath,
                                BOOL aSecure,
                                com::Utf8Str &aDirectory);
    HRESULT directoryExists(const com::Utf8Str &aPath,
                            BOOL *aExists);
    HRESULT directoryOpen(const com::Utf8Str &aPath,
                          const com::Utf8Str &aFilter,
                          const std::vector<DirectoryOpenFlag_T> &aFlags,
                          ComPtr<IGuestDirectory> &aDirectory);
    HRESULT directoryQueryInfo(const com::Utf8Str &aPath,
                               ComPtr<IGuestFsObjInfo> &aInfo);
    HRESULT directoryRemove(const com::Utf8Str &aPath);
    HRESULT directoryRemoveRecursive(const com::Utf8Str &aPath,
                                     const std::vector<DirectoryRemoveRecFlag_T> &aFlags,
                                     ComPtr<IProgress> &aProgress);
    HRESULT directoryRename(const com::Utf8Str &aSource,
                            const com::Utf8Str &aDest,
                            const std::vector<PathRenameFlag_T> &aFlags);
    HRESULT directorySetACL(const com::Utf8Str &aPath,
                             const com::Utf8Str &aAcl);
    HRESULT environmentClear();
    HRESULT environmentGet(const com::Utf8Str &aName,
                           com::Utf8Str &aValue);
    HRESULT environmentSet(const com::Utf8Str &aName,
                           const com::Utf8Str &aValue);
    HRESULT environmentUnset(const com::Utf8Str &aName);
    HRESULT fileCreateTemp(const com::Utf8Str &aTemplateName,
                           ULONG aMode,
                           const com::Utf8Str &aPath,
                           BOOL aSecure,
                           ComPtr<IGuestFile> &aFile);
    HRESULT fileExists(const com::Utf8Str &aPath,
                       BOOL *aExists);
    HRESULT fileRemove(const com::Utf8Str &aPath);
    HRESULT fileOpen(const com::Utf8Str &aPath,
                     const com::Utf8Str &aOpenMode,
                     const com::Utf8Str &aDisposition,
                     ULONG aCreationMode,
                     ComPtr<IGuestFile> &aFile);
    HRESULT fileOpenEx(const com::Utf8Str &aPath,
                       const com::Utf8Str &aOpenMode,
                       const com::Utf8Str &aDisposition,
                       const com::Utf8Str &aSharingMode,
                       ULONG aCreationMode,
                       LONG64 aOffset,
                       ComPtr<IGuestFile> &aFile);
    HRESULT fileQueryInfo(const com::Utf8Str &aPath,
                          ComPtr<IGuestFsObjInfo> &aInfo);
    HRESULT fileQuerySize(const com::Utf8Str &aPath,
                          LONG64 *aSize);
    HRESULT fileRename(const com::Utf8Str &aSource,
                       const com::Utf8Str &aDest,
                       const std::vector<PathRenameFlag_T> &aFlags);
    HRESULT fileSetACL(const com::Utf8Str &aFile,
                       const com::Utf8Str &aAcl);
    HRESULT processCreate(const com::Utf8Str &aCommand,
                          const std::vector<com::Utf8Str> &aArguments,
                          const std::vector<com::Utf8Str> &aEnvironment,
                          const std::vector<ProcessCreateFlag_T> &aFlags,
                          ULONG aTimeoutMS,
                          ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT processCreateEx(const com::Utf8Str &aCommand,
                            const std::vector<com::Utf8Str> &aArguments,
                            const std::vector<com::Utf8Str> &aEnvironment,
                            const std::vector<ProcessCreateFlag_T> &aFlags,
                            ULONG aTimeoutMS,
                            ProcessPriority_T aPriority,
                            const std::vector<LONG> &aAffinity,
                            ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT processGet(ULONG aPid,
                       ComPtr<IGuestProcess> &aGuestProcess);
    HRESULT symlinkCreate(const com::Utf8Str &aSource,
                          const com::Utf8Str &aTarget,
                          SymlinkType_T aType);
    HRESULT symlinkExists(const com::Utf8Str &aSymlink,
                          BOOL *aExists);
    HRESULT symlinkRead(const com::Utf8Str &aSymlink,
                        const std::vector<SymlinkReadFlag_T> &aFlags,
                        com::Utf8Str &aTarget);
    HRESULT symlinkRemoveDirectory(const com::Utf8Str &aPath);
    HRESULT symlinkRemoveFile(const com::Utf8Str &aFile);
    HRESULT waitFor(ULONG aWaitFor,
                    ULONG aTimeoutMS,
                    GuestSessionWaitResult_T *aReason);
    HRESULT waitForArray(const std::vector<GuestSessionWaitForFlag_T> &aWaitFor,
                         ULONG aTimeoutMS,
                         GuestSessionWaitResult_T *aReason);
    /** @}  */

    /** Map of guest directories. The key specifies the internal directory ID. */
    typedef std::map <uint32_t, ComObjPtr<GuestDirectory> > SessionDirectories;
    /** Map of guest files. The key specifies the internal file ID. */
    typedef std::map <uint32_t, ComObjPtr<GuestFile> > SessionFiles;
    /** Map of guest processes. The key specifies the internal process number.
     *  To retrieve the process' guest PID use the Id() method of the IProcess interface. */
    typedef std::map <uint32_t, ComObjPtr<GuestProcess> > SessionProcesses;

public:
    /** @name Public internal methods.
     * @{ */
    int                     i_closeSession(uint32_t uFlags, uint32_t uTimeoutMS, int *pGuestRc);
    inline bool             i_directoryExists(uint32_t uDirID, ComObjPtr<GuestDirectory> *pDir);
    int                     i_directoryRemoveFromList(GuestDirectory *pDirectory);
    int                     i_directoryRemoveInternal(const Utf8Str &strPath, uint32_t uFlags, int *pGuestRc);
    int                     i_directoryCreateInternal(const Utf8Str &strPath, uint32_t uMode, uint32_t uFlags, int *pGuestRc);
    int                     i_objectCreateTempInternal(const Utf8Str &strTemplate, const Utf8Str &strPath, bool fDirectory,
                                                       Utf8Str &strName, int *pGuestRc);
    int                     i_directoryOpenInternal(const GuestDirectoryOpenInfo &openInfo,
                                                    ComObjPtr<GuestDirectory> &pDirectory, int *pGuestRc);
    int                     i_directoryQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc);
    int                     i_dispatchToDirectory(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int                     i_dispatchToFile(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int                     i_dispatchToObject(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int                     i_dispatchToProcess(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int                     i_dispatchToThis(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    inline bool             i_fileExists(uint32_t uFileID, ComObjPtr<GuestFile> *pFile);
    int                     i_fileRemoveFromList(GuestFile *pFile);
    int                     i_fileRemoveInternal(const Utf8Str &strPath, int *pGuestRc);
    int                     i_fileOpenInternal(const GuestFileOpenInfo &openInfo, ComObjPtr<GuestFile> &pFile, int *pGuestRc);
    int                     i_fileQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc);
    int                     i_fileQuerySizeInternal(const Utf8Str &strPath, int64_t *pllSize, int *pGuestRc);
    int                     i_fsQueryInfoInternal(const Utf8Str &strPath, GuestFsObjData &objData, int *pGuestRc);
    const GuestCredentials  &i_getCredentials(void);
    const GuestEnvironment  &i_getEnvironment(void);
    EventSource            *i_getEventSource(void) { return mEventSource; }
    Utf8Str                 i_getName(void);
    ULONG                   i_getId(void) { return mData.mSession.mID; }
    static Utf8Str          i_guestErrorToString(int guestRc);
    HRESULT                 i_isReadyExternal(void);
    int                     i_onRemove(void);
    int                     i_onSessionStatusChange(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCbData);
    int                     i_startSessionInternal(int *pGuestRc);
    int                     i_startSessionAsync(void);
    static DECLCALLBACK(int)
                            i_startSessionThread(RTTHREAD Thread, void *pvUser);
    Guest                  *i_getParent(void) { return mParent; }
    uint32_t                i_getProtocolVersion(void) { return mData.mProtocolVersion; }
    int                     i_pathRenameInternal(const Utf8Str &strSource, const Utf8Str &strDest, uint32_t uFlags,
                                                 int *pGuestRc);
    int                     i_processRemoveFromList(GuestProcess *pProcess);
    int                     i_processCreateExInteral(GuestProcessStartupInfo &procInfo, ComObjPtr<GuestProcess> &pProgress);
    inline bool             i_processExists(uint32_t uProcessID, ComObjPtr<GuestProcess> *pProcess);
    inline int              i_processGetByPID(ULONG uPID, ComObjPtr<GuestProcess> *pProcess);
    int                     i_sendCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms);
    static HRESULT          i_setErrorExternal(VirtualBoxBase *pInterface, int guestRc);
    int                     i_setSessionStatus(GuestSessionStatus_T sessionStatus, int sessionRc);
    int                     i_signalWaiters(GuestSessionWaitResult_T enmWaitResult, int rc /*= VINF_SUCCESS */);
    int                     i_startTaskAsync(const Utf8Str &strTaskDesc, GuestSessionTask *pTask,
                                             ComObjPtr<Progress> &pProgress);
    int                     i_queryInfo(void);
    int                     i_waitFor(uint32_t fWaitFlags, ULONG uTimeoutMS, GuestSessionWaitResult_T &waitResult, int *pGuestRc);
    int                     i_waitForStatusChange(GuestWaitEvent *pEvent, uint32_t fWaitFlags, uint32_t uTimeoutMS,
                                                  GuestSessionStatus_T *pSessionStatus, int *pGuestRc);
    /** @}  */

private:

    /** Pointer to the parent (Guest). */
    Guest                          *mParent;
    /**
     * The session's event source. This source is used for
     * serving the internal listener as well as all other
     * external listeners that may register to it.
     *
     * Note: This can safely be used without holding any locks.
     * An AutoCaller suffices to prevent it being destroy while in use and
     * internally there is a lock providing the necessary serialization.
     */
    const ComObjPtr<EventSource>    mEventSource;

    struct Data
    {
        /** The session credentials. */
        GuestCredentials            mCredentials;
        /** The session's startup info. */
        GuestSessionStartupInfo     mSession;
        /** The session's current status. */
        GuestSessionStatus_T        mStatus;
        /** The session's environment block. Can be
         *  overwritten/extended by ProcessCreate(Ex). */
        GuestEnvironment            mEnvironment;
        /** Directory objects bound to this session. */
        SessionDirectories          mDirectories;
        /** File objects bound to this session. */
        SessionFiles                mFiles;
        /** Process objects bound to this session. */
        SessionProcesses            mProcesses;
        /** Guest control protocol version to be used.
         *  Guest Additions < VBox 4.3 have version 1,
         *  any newer version will have version 2. */
        uint32_t                    mProtocolVersion;
        /** Session timeout (in ms). */
        uint32_t                    mTimeout;
        /** Total number of session objects (processes,
         *  files, ...). */
        uint32_t                    mNumObjects;
        /** The last returned session status
         *  returned from the guest side. */
        int                         mRC;
    } mData;
};

#endif /* !____H_GUESTSESSIONIMPL */

