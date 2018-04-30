/* $Id$ */
/** @file
 * VirtualBox Main - Guest session tasks header.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTSESSIONIMPL_TASKS
#define ____H_GUESTSESSIONIMPL_TASKS

#include "GuestSessionWrap.h"
#include "EventImpl.h"

#include "GuestCtrlImplPrivate.h"
#include "GuestSessionImpl.h"
#include "ThreadTask.h"

#include <iprt/isofs.h> /* For UpdateAdditions. */
#include <iprt/fs.h> /* For PCRTFSOBJINFO. */

#include <vector>

class Guest;
class GuestSessionTask;
class GuestSessionTaskInternalOpen;


/**
 * Structure for keeping a file system source specification,
 * along with options.
 */
struct GuestSessionFsSourceSpec
{
    GuestSessionFsSourceSpec()
        : enmType(FsObjType_Unknown)
        , enmPathStyle(PathStyle_Unknown)
        , fDryRun(false)
        , fFollowSymlinks(false) { }

    Utf8Str     strSource;
    Utf8Str     strFilter;
    FsObjType_T enmType;
    PathStyle_T enmPathStyle;
    bool        fDryRun;
    bool        fFollowSymlinks;
    union
    {
        /** Directory-specific data. */
        struct
        {
            /** Directory copy flags. */
            DirectoryCopyFlag_T fCopyFlags;
            bool                fRecursive;
        } Dir;
        /** File-specific data. */
        struct
        {
            /** File copy flags. */
            FileCopyFlag_T      fCopyFlags;
            /** Host file handle to use for reading from / writing to.
             *  Optional and can be NULL if not used. */
            PRTFILE             phFile;
            /** Source file offset to start copying from. */
            size_t              offStart;
            /** Source size (in bytes) to copy. */
            uint64_t            cbSize;
        } File;
    } Type;
};

/** A set of GuestSessionFsSourceSpec sources. */
typedef std::vector<GuestSessionFsSourceSpec> GuestSessionFsSourceSet;

/**
 * Structure for keeping a file system entry.
 */
struct FsEntry
{
    /** The entrie's file mode. */
    RTFMODE fMode;
    /** The entrie's path, relative to the list's root path. */
    Utf8Str strPath;
};

/** A vector of FsEntry entries. */
typedef std::vector<FsEntry *> FsEntries;

/**
 * Class for storing and handling file system entries, neeed for doing
 * internal file / directory operations to / from the guest.
 */
class FsList
{
public:

    FsList(const GuestSessionTask &Task);
    virtual ~FsList();

public:

    int Init(const Utf8Str &strSrcRootAbs, const Utf8Str &strDstRootAbs, const GuestSessionFsSourceSpec &SourceSpec);
    void Destroy(void);

    int AddEntryFromGuest(const Utf8Str &strFile, const GuestFsObjData &fsObjData);
    int AddDirFromGuest(const Utf8Str &strPath, const Utf8Str &strSubDir = "");

    int AddEntryFromHost(const Utf8Str &strFile, PCRTFSOBJINFO pcObjInfo);
    int AddDirFromHost(const Utf8Str &strPath, const Utf8Str &strSubDir = "");

public:

    /** The guest session task object this list is working on. */
    const GuestSessionTask  &mTask;
    /** File system filter / options to use for this task. */
    GuestSessionFsSourceSpec mSourceSpec;
    /** The source' root path.
     *  For a single file list this is the full (absolute) path to a file,
     *  for a directory list this is the source root directory. */
    Utf8Str                 mSrcRootAbs;
    /** The destinations's root path.
     *  For a single file list this is the full (absolute) path to a file,
     *  for a directory list this is the destination root directory. */
    Utf8Str                 mDstRootAbs;
    /** Total size (in bytes) of all list entries together. */
    uint64_t                mcbTotalSize;
    /** List of file system entries this list contains. */
    FsEntries               mVecEntries;
};

/** A set of FsList lists. */
typedef std::vector<FsList *> FsLists;

/**
 * Abstract base class for a lenghtly per-session operation which
 * runs in a Main worker thread.
 */
class GuestSessionTask : public ThreadTask
{
public:

    GuestSessionTask(GuestSession *pSession);

    virtual ~GuestSessionTask(void);

public:

    virtual int Run(void) = 0;
    void handler()
    {
        int vrc = Run();
        NOREF(vrc);
        /** @todo
         *
         * r=bird: what was your idea WRT to Run status code and async tasks?
         *
         */
    }

    int RunAsync(const Utf8Str &strDesc, ComObjPtr<Progress> &pProgress);

    virtual HRESULT Init(const Utf8Str &strTaskDesc)
    {
        setTaskDesc(strTaskDesc);
        int rc = createAndSetProgressObject(); /* Single operation by default. */
        if (RT_FAILURE(rc))
            return E_FAIL;

        return S_OK;
    }

    const ComObjPtr<Progress>& GetProgressObject(void) const { return mProgress; }

    const ComObjPtr<GuestSession>& GetSession(void) const { return mSession; }

protected:

    /** @name Directory handling primitives.
     * @{ */
    int directoryCreate(const com::Utf8Str &strPath, DirectoryCreateFlag_T enmDirecotryCreateFlags, uint32_t uMode,
                        bool fFollowSymlinks);
    /** @}  */

    /** @name File handling primitives.
     * @{ */
    int fileCopyFromGuestInner(ComObjPtr<GuestFile> &srcFile, PRTFILE phDstFile, FileCopyFlag_T fFileCopyFlags,
                               uint64_t offCopy, uint64_t cbSize);
    int fileCopyFromGuest(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags);
    int fileCopyToGuestInner(PRTFILE phSrcFile, ComObjPtr<GuestFile> &dstFile, FileCopyFlag_T fFileCopyFlags,
                             uint64_t offCopy, uint64_t cbSize);

    int fileCopyToGuest(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T fFileCopyFlags);
    /** @}  */

    /** @name Guest property handling primitives.
     * @{ */
    int getGuestProperty(const ComObjPtr<Guest> &pGuest, const Utf8Str &strPath, Utf8Str &strValue);
    /** @}  */

    int setProgress(ULONG uPercent);
    int setProgressSuccess(void);
    HRESULT setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg);

    inline void setTaskDesc(const Utf8Str &strTaskDesc) throw()
    {
        mDesc = strTaskDesc;
    }

    int createAndSetProgressObject(ULONG cOperations  = 1);

protected:

    Utf8Str                 mDesc;
    /** The guest session object this task is working on. */
    ComObjPtr<GuestSession> mSession;
    /** Progress object for getting updated when running
     *  asynchronously. Optional. */
    ComObjPtr<Progress>     mProgress;
    /** The guest's path style (depending on the guest OS type set). */
    uint32_t                mfPathStyle;
    /** The guest's path style as string representation (depending on the guest OS type set). */
    Utf8Str                 mPathStyle;
};

/**
 * Task for opening a guest session.
 */
class GuestSessionTaskOpen : public GuestSessionTask
{
public:

    GuestSessionTaskOpen(GuestSession *pSession,
                    uint32_t uFlags,
                    uint32_t uTimeoutMS);
    virtual ~GuestSessionTaskOpen(void);
    int Run(void);

protected:

    /** Session creation flags. */
    uint32_t mFlags;
    /** Session creation timeout (in ms). */
    uint32_t mTimeoutMS;
};

class GuestSessionCopyTask : public GuestSessionTask
{
public:

    GuestSessionCopyTask(GuestSession *pSession);
    virtual ~GuestSessionCopyTask();

protected:

    /** Source set. */
    GuestSessionFsSourceSet mSources;
    /** Destination to copy to. */
    Utf8Str                 mDest;
    /** Vector of file system lists to handle.
     *  This either can be from the guest or the host side. */
    FsLists                 mVecLists;
};

/**
 * Guest session task for copying files / directories from guest to the host.
 */
class GuestSessionTaskCopyFrom : public GuestSessionCopyTask
{
public:

    GuestSessionTaskCopyFrom(GuestSession *pSession, GuestSessionFsSourceSet vecSrc, const Utf8Str &strDest);
    virtual ~GuestSessionTaskCopyFrom(void);

    HRESULT Init(const Utf8Str &strTaskDesc);
    int Run(void);
};

/**
 * Task for copying directories from host to the guest.
 */
class GuestSessionTaskCopyTo : public GuestSessionCopyTask
{
public:

    GuestSessionTaskCopyTo(GuestSession *pSession, GuestSessionFsSourceSet vecSrc, const Utf8Str &strDest);
    virtual ~GuestSessionTaskCopyTo(void);

    HRESULT Init(const Utf8Str &strTaskDesc);
    int Run(void);
};

/**
 * Guest session task for automatically updating the Guest Additions on the guest.
 */
class GuestSessionTaskUpdateAdditions : public GuestSessionTask
{
public:

    GuestSessionTaskUpdateAdditions(GuestSession *pSession,
                               const Utf8Str &strSource, const ProcessArguments &aArguments,
                               uint32_t fFlags);
    virtual ~GuestSessionTaskUpdateAdditions(void);
    int Run(void);

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
    struct ISOFile
    {
        ISOFile(const Utf8Str &aSource,
                const Utf8Str &aDest,
                uint32_t       aFlags = 0)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags) { }

        ISOFile(const Utf8Str                 &aSource,
                const Utf8Str                 &aDest,
                uint32_t                       aFlags,
                const GuestProcessStartupInfo &aStartupInfo)
            : strSource(aSource),
              strDest(aDest),
              fFlags(aFlags),
              mProcInfo(aStartupInfo)
        {
            mProcInfo.mExecutable = strDest;
            if (mProcInfo.mName.isEmpty())
                mProcInfo.mName = strDest;
        }

        /** Source file on .ISO. */
        Utf8Str                 strSource;
        /** Destination file on the guest. */
        Utf8Str                 strDest;
        /** ISO file flags (see ISOFILE_FLAG_ defines). */
        uint32_t                fFlags;
        /** Optional arguments if this file needs to be
         *  executed. */
        GuestProcessStartupInfo mProcInfo;
    };

    int addProcessArguments(ProcessArguments &aArgumentsDest, const ProcessArguments &aArgumentsSource);
    int copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO, Utf8Str const &strFileSource, const Utf8Str &strFileDest, bool fOptional);
    int runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo);

    /** Files to handle. */
    std::vector<ISOFile>        mFiles;
    /** The (optionally) specified Guest Additions .ISO on the host
     *  which will be used for the updating process. */
    Utf8Str                     mSource;
    /** (Optional) installer command line arguments. */
    ProcessArguments            mArguments;
    /** Update flags. */
    uint32_t                    mFlags;
};
#endif /* !____H_GUESTSESSIONIMPL_TASKS */
