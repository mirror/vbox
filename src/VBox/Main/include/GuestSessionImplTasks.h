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

class Guest;
class GuestSessionTaskInternalOpen;

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

    HRESULT Init(const Utf8Str &strTaskDesc)
    {
        HRESULT hr = S_OK;
        setTaskDesc(strTaskDesc);
        hr = createAndSetProgressObject();
        return hr;
    }

    const ComObjPtr<Progress>& GetProgressObject() const { return mProgress; }

protected:

    /** @name Directory handling primitives.
     * @{ */
    int directoryCreate(const com::Utf8Str &strPath, DirectoryCreateFlag_T enmDirecotryCreateFlags, uint32_t uMode,
                        bool fFollowSymlinks);
    /** @}  */

    /** @name File handling primitives.
     * @{ */
    int fileCopyFromEx(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags,
                       PRTFILE pFile, uint64_t cbOffset, uint64_t cbSize);
    int fileCopyFrom(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    int fileCopyToEx(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags, PRTFILE pFile,
                     uint64_t cbOffset, uint64_t cbSize); /**< r=bird: 'cbOffset' makes no sense what so ever. It should be 'off', or do you mean sizeof(uint64_t)? */
    int fileCopyTo(const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    /** @}  */

    /** @name Guest property handling primitives.
     * @{ */
    int getGuestProperty(const ComObjPtr<Guest> &pGuest, const Utf8Str &strPath, Utf8Str &strValue);
    /** @}  */

    /** @name Path handling primitives.
     * @{ */
    int pathConstructOnGuest(const Utf8Str &strSourceRoot, const Utf8Str &strSource, const Utf8Str &strDest, Utf8Str &strOut);
    /** @}  */

    int setProgress(ULONG uPercent);
    int setProgressSuccess(void);
    HRESULT setProgressErrorMsg(HRESULT hr, const Utf8Str &strMsg);

    inline void setTaskDesc(const Utf8Str &strTaskDesc) throw()
    {
        mDesc = strTaskDesc;
    }

    HRESULT createAndSetProgressObject();

protected:

    Utf8Str                 mDesc;
    /** The guest session object this task is working on. */
    ComObjPtr<GuestSession> mSession;
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
    int Run(void);

protected:

    /** Session creation flags. */
    uint32_t mFlags;
    /** Session creation timeout (in ms). */
    uint32_t mTimeoutMS;
};

/**
 * Task for copying directories from guest to the host.
 */
class SessionTaskCopyDirFrom : public GuestSessionTask
{
public:

    SessionTaskCopyDirFrom(GuestSession *pSession, const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                           DirectoryCopyFlag_T enmDirCopyFlags);
    virtual ~SessionTaskCopyDirFrom(void);
    int Run(void);

protected:

    int directoryCopyToHost(const Utf8Str &strSource, const Utf8Str &strFilter, const Utf8Str &strDest, bool fRecursive,
                            bool fFollowSymlinks, const Utf8Str &strSubDir /* For recursion. */);
protected:

    Utf8Str             mSource;
    Utf8Str             mDest;
    Utf8Str             mFilter;
    DirectoryCopyFlag_T mDirCopyFlags;
};

/**
 * Task for copying directories from host to the guest.
 */
class SessionTaskCopyDirTo : public GuestSessionTask
{
public:

    SessionTaskCopyDirTo(GuestSession *pSession, const Utf8Str &strSource, const Utf8Str &strDest, const Utf8Str &strFilter,
                         DirectoryCopyFlag_T enmDirCopyFlags);
    virtual ~SessionTaskCopyDirTo(void);
    int Run(void);

protected:

    int directoryCopyToGuest(const Utf8Str &strSource, const Utf8Str &strFilter, const Utf8Str &strDest, bool fRecursive,
                             bool fFollowSymlinks, const Utf8Str &strSubDir /* For recursion. */);
protected:

    Utf8Str             mSource;
    Utf8Str             mDest;
    Utf8Str             mFilter;
    DirectoryCopyFlag_T mDirCopyFlags;
};

/**
 * Task for copying files from host to the guest.
 */
class SessionTaskCopyFileTo : public GuestSessionTask
{
public:

    SessionTaskCopyFileTo(GuestSession *pSession,
                          const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    SessionTaskCopyFileTo(GuestSession *pSession,
                          PRTFILE pSourceFile, size_t cbSourceOffset, uint64_t cbSourceSize,
                          const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    virtual ~SessionTaskCopyFileTo(void);
    int Run(void);

protected:

    Utf8Str        mSource;
    PRTFILE        mSourceFile;
    size_t         mSourceOffset;
    uint64_t       mSourceSize;
    Utf8Str        mDest;
    FileCopyFlag_T mFileCopyFlags;
};

/**
 * Task for copying files from guest to the host.
 */
class SessionTaskCopyFileFrom : public GuestSessionTask
{
public:

    SessionTaskCopyFileFrom(GuestSession *pSession,
                        const Utf8Str &strSource, const Utf8Str &strDest, FileCopyFlag_T enmFileCopyFlags);
    virtual ~SessionTaskCopyFileFrom(void);
    int Run(void);

protected:

    Utf8Str        mSource;
    Utf8Str        mDest;
    FileCopyFlag_T mFileCopyFlags;
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
    int copyFileToGuest(GuestSession *pSession, PRTISOFSFILE pISO, Utf8Str const &strFileSource, const Utf8Str &strFileDest, bool fOptional, uint32_t *pcbSize);
    int runFileOnGuest(GuestSession *pSession, GuestProcessStartupInfo &procInfo);

    /** Files to handle. */
    std::vector<ISOFile> mFiles;
    /** The (optionally) specified Guest Additions .ISO on the host
     *  which will be used for the updating process. */
    Utf8Str                    mSource;
    /** (Optional) installer command line arguments. */
    ProcessArguments           mArguments;
    /** Update flags. */
    uint32_t                   mFlags;
};
#endif /* !____H_GUESTSESSIONIMPL_TASKS */
