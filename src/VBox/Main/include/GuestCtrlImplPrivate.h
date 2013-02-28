/** @file
 *
 * Internal helpers/structures for guest control functionality.
 */

/*
 * Copyright (C) 2011-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTIMPLPRIVATE
#define ____H_GUESTIMPLPRIVATE

#include "ConsoleImpl.h"

#include <iprt/asm.h>
#include <iprt/semaphore.h>

#include <VBox/com/com.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>

#include <map>
#include <vector>

using namespace com;

#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/HostServices/GuestControlSvc.h>
using namespace guestControl;
#endif

/** Vector holding a process' CPU affinity. */
typedef std::vector <LONG> ProcessAffinity;
/** Vector holding process startup arguments. */
typedef std::vector <Utf8Str> ProcessArguments;

class GuestProcessStreamBlock;
class GuestSession;


/**
 * Base class for a all guest control callbacks/events.
 */
class GuestCtrlEvent
{
public:

    GuestCtrlEvent(void);

    virtual ~GuestCtrlEvent(void);

    /** @todo Copy/comparison operator? */

public:

    int Cancel(void);

    bool Canceled(void);

    virtual void Destroy(void);

    int Init(void);

    virtual int Signal(int rc = VINF_SUCCESS);

    int GetResultCode(void) { return mRC; }

    int Wait(ULONG uTimeoutMS);

protected:

    /** Was the callback canceled? */
    bool                        fCanceled;
    /** Did the callback complete? */
    bool                        fCompleted;
    /** The event semaphore for triggering
     *  the actual event. */
    RTSEMEVENT                  hEventSem;
    /** The waiting mutex. */
    RTSEMMUTEX                  hEventMutex;
    /** Overall result code. */
    int                         mRC;
};


/*
 * Enumeration holding the host callback types.
 */
enum CALLBACKTYPE
{
    CALLBACKTYPE_UNKNOWN = 0,
    /** Guest session status. */
    CALLBACKTYPE_SESSION_NOTIFY = 10,
    /** Guest process status. */
    CALLBACKTYPE_PROC_STATUS = 100,
    /** Guest process output notification. */
    CALLBACKTYPE_PROC_OUTPUT = 105,
    /** Guest process input notification. */
    CALLBACKTYPE_PROC_INPUT = 106,
    /** @todo Docs! */
    CALLBACKTYPE_FILE_OPEN = 210,
    CALLBACKTYPE_FILE_CLOSE = 215,
    CALLBACKTYPE_FILE_READ = 230,
    CALLBACKTYPE_FILE_WRITE = 240,
    CALLBACKTYPE_FILE_SEEK = 250,
    CALLBACKTYPE_FILE_TELL = 260
};


/*
 * Class representing a guest control callback.
 */
class GuestCtrlCallback : public GuestCtrlEvent
{

public:

    GuestCtrlCallback(void);

    GuestCtrlCallback(CALLBACKTYPE enmType);

    virtual ~GuestCtrlCallback(void);

public:

    void Destroy(void);

    int Init(CALLBACKTYPE enmType);

    CALLBACKTYPE GetCallbackType(void) { return mType; }

    const void* GetDataRaw(void) const { return pvData; }

    size_t GetDataSize(void) { return cbData; }

    const void* GetPayloadRaw(void) const { return pvPayload; }

    size_t GetPayloadSize(void) { return cbPayload; }

    int SetData(const void *pvCallback, size_t cbCallback);

    int SetPayload(const void *pvToWrite, size_t cbToWrite);

protected:

    /** Pointer to actual callback data. */
    void                       *pvData;
    /** Size of user-supplied data. */
    size_t                      cbData;
    /** The callback type. */
    CALLBACKTYPE                mType;
    /** Callback flags. */
    uint32_t                    uFlags;
    /** Payload which will be available on successful
     *  waiting (optional). */
    void                       *pvPayload;
    /** Size of the payload (optional). */
    size_t                      cbPayload;
};
typedef std::map < uint32_t, GuestCtrlCallback* > GuestCtrlCallbacks;


/*
 * Class representing a guest control process waiting
 * event.
 */
class GuestProcessWaitEvent : public GuestCtrlEvent
{
public:

    GuestProcessWaitEvent(void);

    GuestProcessWaitEvent(uint32_t uWaitFlags);

    virtual ~GuestProcessWaitEvent(void);

public:

    void Destroy(void);

    int Init(uint32_t uWaitFlags);

    uint32_t GetWaitFlags(void) { return ASMAtomicReadU32(&mFlags); }

    ProcessWaitResult_T GetWaitResult(void) { return mResult; }

    int GetWaitRc(void) { return mRC; }

    int Signal(ProcessWaitResult_T enmResult, int rc = VINF_SUCCESS);

protected:

    /** The waiting flag(s). The specifies what to
     *  wait for. See ProcessWaitFlag_T. */
    uint32_t                    mFlags;
    /** Structure containing the overall result. */
    ProcessWaitResult_T         mResult;
};


/**
 * Simple structure mantaining guest credentials.
 */
struct GuestCredentials
{
    Utf8Str                     mUser;
    Utf8Str                     mPassword;
    Utf8Str                     mDomain;
};


typedef std::vector <Utf8Str> GuestEnvironmentArray;
class GuestEnvironment
{
public:

    int BuildEnvironmentBlock(void **ppvEnv, size_t *pcbEnv, uint32_t *pcEnvVars);

    void Clear(void);

    int CopyFrom(const GuestEnvironmentArray &environment);

    int CopyTo(GuestEnvironmentArray &environment);

    static void FreeEnvironmentBlock(void *pvEnv);

    Utf8Str Get(const Utf8Str &strKey);

    Utf8Str Get(size_t nPos);

    bool Has(const Utf8Str &strKey);

    int Set(const Utf8Str &strKey, const Utf8Str &strValue);

    int Set(const Utf8Str &strPair);

    size_t Size(void);

    int Unset(const Utf8Str &strKey);

public:

    GuestEnvironment& operator=(const GuestEnvironmentArray &that);

    GuestEnvironment& operator=(const GuestEnvironment &that);

protected:

    int appendToEnvBlock(const char *pszEnv, void **ppvList, size_t *pcbList, uint32_t *pcEnvVars);

protected:

    std::map <Utf8Str, Utf8Str> mEnvironment;
};


/**
 * Structure for keeping all the relevant guest file
 * information around.
 */
struct GuestFileOpenInfo
{
    /** The filename. */
    Utf8Str                 mFileName;
    /** Then file's opening mode. */
    Utf8Str                 mOpenMode;
    /** The file's disposition mode. */
    Utf8Str                 mDisposition;
    /** Octal creation mode. */
    uint32_t                mCreationMode;
    /** The initial offset on open. */
    int64_t                 mInitialOffset;
};


/**
 * Structure representing information of a
 * file system object.
 */
struct GuestFsObjData
{
    /** Helper function to extract the data from
     *  a certin VBoxService tool's guest stream block. */
    int FromLs(const GuestProcessStreamBlock &strmBlk);
    int FromStat(const GuestProcessStreamBlock &strmBlk);

    int64_t              mAccessTime;
    int64_t              mAllocatedSize;
    int64_t              mBirthTime;
    int64_t              mChangeTime;
    uint32_t             mDeviceNumber;
    Utf8Str              mFileAttrs;
    uint32_t             mGenerationID;
    uint32_t             mGID;
    Utf8Str              mGroupName;
    uint32_t             mNumHardLinks;
    int64_t              mModificationTime;
    Utf8Str              mName;
    int64_t              mNodeID;
    uint32_t             mNodeIDDevice;
    int64_t              mObjectSize;
    FsObjType_T          mType;
    uint32_t             mUID;
    uint32_t             mUserFlags;
    Utf8Str              mUserName;
    Utf8Str              mACL;
};


/**
 * Structure for keeping all the relevant process
 * starting parameters around.
 */
class GuestProcessStartupInfo
{
public:

    GuestProcessStartupInfo(void)
        : mFlags(ProcessCreateFlag_None),
          mTimeoutMS(30 * 1000 /* 30s timeout by default */),
          mPriority(ProcessPriority_Default) { }

    /** The process' friendly name. */
    Utf8Str                     mName;
    /** The actual command to execute. */
    Utf8Str                     mCommand;
    ProcessArguments            mArguments;
    GuestEnvironment            mEnvironment;
    /** Process creation flags. */
    uint32_t                    mFlags;
    ULONG                       mTimeoutMS;
    /** Process priority. */
    ProcessPriority_T           mPriority;
    /** Process affinity. */
    uint64_t                    mAffinity;
};


/**
 * Class representing the "value" side of a "key=value" pair.
 */
class GuestProcessStreamValue
{
public:

    GuestProcessStreamValue(void) { }
    GuestProcessStreamValue(const char *pszValue)
        : mValue(pszValue) {}

    GuestProcessStreamValue(const GuestProcessStreamValue& aThat)
           : mValue(aThat.mValue) { }

    Utf8Str mValue;
};

/** Map containing "key=value" pairs of a guest process stream. */
typedef std::pair< Utf8Str, GuestProcessStreamValue > GuestCtrlStreamPair;
typedef std::map < Utf8Str, GuestProcessStreamValue > GuestCtrlStreamPairMap;
typedef std::map < Utf8Str, GuestProcessStreamValue >::iterator GuestCtrlStreamPairMapIter;
typedef std::map < Utf8Str, GuestProcessStreamValue >::const_iterator GuestCtrlStreamPairMapIterConst;

/**
 * Class representing a block of stream pairs (key=value). Each block in a raw guest
 * output stream is separated by "\0\0", each pair is separated by "\0". The overall
 * end of a guest stream is marked by "\0\0\0\0".
 */
class GuestProcessStreamBlock
{
public:

    GuestProcessStreamBlock(void);

    virtual ~GuestProcessStreamBlock(void);

public:

    void Clear(void);

#ifdef DEBUG
    void DumpToLog(void) const;
#endif

    int GetInt64Ex(const char *pszKey, int64_t *piVal) const;

    int64_t GetInt64(const char *pszKey) const;

    size_t GetCount(void) const;

    const char* GetString(const char *pszKey) const;

    int GetUInt32Ex(const char *pszKey, uint32_t *puVal) const;

    uint32_t GetUInt32(const char *pszKey) const;

    bool IsEmpty(void) { return mPairs.empty(); }

    int SetValue(const char *pszKey, const char *pszValue);

protected:

    GuestCtrlStreamPairMap mPairs;
};

/** Vector containing multiple allocated stream pair objects. */
typedef std::vector< GuestProcessStreamBlock > GuestCtrlStreamObjects;
typedef std::vector< GuestProcessStreamBlock >::iterator GuestCtrlStreamObjectsIter;
typedef std::vector< GuestProcessStreamBlock >::const_iterator GuestCtrlStreamObjectsIterConst;

/**
 * Class for parsing machine-readable guest process output by VBoxService'
 * toolbox commands ("vbox_ls", "vbox_stat" etc), aka "guest stream".
 */
class GuestProcessStream
{

public:

    GuestProcessStream();

    virtual ~GuestProcessStream();

public:

    int AddData(const BYTE *pbData, size_t cbData);

    void Destroy();

#ifdef DEBUG
    void Dump(const char *pszFile);
#endif

    uint32_t GetOffset();

    uint32_t GetSize();

    int ParseBlock(GuestProcessStreamBlock &streamBlock);

protected:

    /** Currently allocated size of internal stream buffer. */
    uint32_t m_cbAllocated;
    /** Currently used size of allocated internal stream buffer. */
    uint32_t m_cbSize;
    /** Current offset within the internal stream buffer. */
    uint32_t m_cbOffset;
    /** Internal stream buffer. */
    BYTE *m_pbBuffer;
};

class Guest;
class Progress;

class GuestTask
{

public:

    enum TaskType
    {
        /** Copies a file from host to the guest. */
        TaskType_CopyFileToGuest   = 50,
        /** Copies a file from guest to the host. */
        TaskType_CopyFileFromGuest = 55,
        /** Update Guest Additions by directly copying the required installer
         *  off the .ISO file, transfer it to the guest and execute the installer
         *  with system privileges. */
        TaskType_UpdateGuestAdditions = 100
    };

    GuestTask(TaskType aTaskType, Guest *aThat, Progress *aProgress);

    virtual ~GuestTask();

    int startThread();

    static int taskThread(RTTHREAD aThread, void *pvUser);
    static int uploadProgress(unsigned uPercent, void *pvUser);
    static HRESULT setProgressSuccess(ComObjPtr<Progress> pProgress);
    static HRESULT setProgressErrorMsg(HRESULT hr,
                                       ComObjPtr<Progress> pProgress, const char * pszText, ...);
    static HRESULT setProgressErrorParent(HRESULT hr,
                                          ComObjPtr<Progress> pProgress, ComObjPtr<Guest> pGuest);

    TaskType taskType;
    ComObjPtr<Guest> pGuest;
    ComObjPtr<Progress> pProgress;
    HRESULT rc;

    /* Task data. */
    Utf8Str strSource;
    Utf8Str strDest;
    Utf8Str strUserName;
    Utf8Str strPassword;
    ULONG   uFlags;
};

/**
 * Pure virtual class (interface) for guest objects (processes, files, ...) --
 * contains all per-object callback management.
 */
class GuestObject
{

public:

    ULONG getObjectID(void) { return mObject.mObjectID; }

protected:

    /** Callback dispatcher -- must be implemented by the actual object. */
    virtual int callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb) = 0;

protected:

    int bindToSession(Console *pConsole, GuestSession *pSession, uint32_t uObjectID);
    int callbackAdd(GuestCtrlCallback *pCallback, uint32_t *puContextID);
    bool callbackExists(uint32_t uContextID);
    int callbackRemove(uint32_t uContextID);
    int callbackRemoveAll(void);
    int sendCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms);

protected:

    /**
     * Commom structure for all derived objects, when then have
     * an own mData structure to keep their specific data around.
     */
    struct Object
    {
        /** Pointer to parent session. Per definition
         *  this objects *always* lives shorter than the
         *  parent. */
        GuestSession            *mSession;
        /** Pointer to the console object. Needed
         *  for HGCM (VMMDev) communication. */
        Console                 *mConsole;
        /** All related callbacks to this object. */
        GuestCtrlCallbacks       mCallbacks;
        /** The next upcoming context ID for this object. */
        ULONG                    mNextContextID;
        /** The object ID -- must be unique for each guest
         *  session and is encoded into the context ID. Must
         *  be set manually when initializing the object.
         *
         *  For guest processes this is the internal PID,
         *  for guest files this is the internal file ID. */
        uint32_t                 mObjectID;
    } mObject;
};

#if 0
/*
 * Guest (HGCM) callbacks. All classes will throw
 * an exception on misuse.
 */

/** Callback class for guest process status. */
class GuestCbProcessStatus : public GuestCtrlCallback
{

public:

    int Init(uint32_t uProtocol, uint32_t uFunction,
             PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
    {
        AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

        int rc = GuestCtrlCallback::Init();
        if (RT_FAILURE(rc))
            return rc;

        if (   uFunction != GUEST_EXEC_SEND_STATUS
            || pSvcCb->mParms < 5)
            return VERR_INVALID_PARAMETER;

        /* pSvcCb->mpaParms[0] always contains the context ID. */
        pSvcCb->mpaParms[1].getUInt32(&mPID);
        pSvcCb->mpaParms[2].getUInt32(&mStatus);
        pSvcCb->mpaParms[3].getUInt32(&mFlags); /* Can contain an IPRT error, which is a signed int. */
        pSvcCb->mpaParms[4].getPointer(&mData, &mcbData);

        return VINF_SUCCESS;
    }

    void Destroy(void) { }

    uint32_t  mPID;
    uint32_t  mStatus;
    uint32_t  mFlags;
    void     *mData;
    uint32_t  mcbData;
};

/** Callback class for guest process input. */
class GuestCbProcessInput : public GuestCtrlCallback
{

public:

    int Init(uint32_t uProtocol, uint32_t uFunction,
             PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
    {
        AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

        int rc = GuestCtrlCallback::Init();
        if (RT_FAILURE(rc))
            return rc;

        if (   uFunction != GUEST_EXEC_SEND_INPUT_STATUS
            || pSvcCb->mParms < 5)
            return VERR_INVALID_PARAMETER;

        /* pSvcCb->mpaParms[0] always contains the context ID. */
        pSvcCb->mpaParms[1].getUInt32(&mPID);
        /* Associated file handle. */
        pSvcCb->mpaParms[2].getUInt32(&mStatus);
        pSvcCb->mpaParms[3].getUInt32(&mFlags);
        pSvcCb->mpaParms[4].getUInt32(&mProcessed);

        return VINF_SUCCESS;
    }

    void Destroy(void) { }

    GuestCbProcessInput& operator=(const GuestCbProcessInput &that)
    {
        mPID = that.mPID;
        mStatus = that.mStatus;
        mFlags = that.mFlags;
        mProcessed = that.mProcessed;
        return *this;
    }

    uint32_t  mPID;
    uint32_t  mStatus;
    uint32_t  mFlags;
    uint32_t  mProcessed;
};

/** Callback class for guest process output. */
class GuestCbProcessOutput : public GuestCtrlCallback
{

public:

    int Init(uint32_t uProtocol, uint32_t uFunction,
             PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
    {
        AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

        int rc = GuestCtrlCallback::Init();
        if (RT_FAILURE(rc))
            return rc;

        if (   uFunction != GUEST_EXEC_SEND_OUTPUT
            || pSvcCb->mParms < 5)
            return VERR_INVALID_PARAMETER;

        /* pSvcCb->mpaParms[0] always contains the context ID. */
        pSvcCb->mpaParms[1].getUInt32(&mPID);
        /* Associated file handle. */
        pSvcCb->mpaParms[2].getUInt32(&mHandle);
        pSvcCb->mpaParms[3].getUInt32(&mFlags);

        void *pbData; uint32_t cbData;
        rc = pSvcCb->mpaParms[4].getPointer(&pbData, &cbData);
        if (RT_SUCCESS(rc))
        {
            Assert(cbData);
            mData = RTMemAlloc(cbData);
            AssertPtrReturn(mData, VERR_NO_MEMORY);
            memcpy(mData, pbData, cbData);
            mcbData = cbData;
        }

        return rc;
    }

    void Destroy(void)
    {
        if (mData)
        {
            RTMemFree(mData);
            mData = NULL;
            mcbData = 0;
        }
    }

    GuestCbProcessOutput& operator=(const GuestCbProcessOutput &that)
    {
        mPID = that.mPID;
        mHandle = that.mHandle;
        mFlags = that.mFlags;

        Destroy();
        if (that.mcbData)
        {
            void *pvData = RTMemAlloc(that.mcbData);
            if (pvData)
            {
                AssertPtr(pvData);
                memcpy(pvData, that.mData, that.mcbData);
                mData = pvData;
                mcbData = that.mcbData;
            }
        }

        return *this;
    }

    uint32_t  mPID;
    uint32_t  mHandle;
    uint32_t  mFlags;
    void     *mData;
    size_t    mcbData;
};

/** Callback class for guest process IO notifications. */
class GuestCbProcessIO : public GuestCtrlCallback
{

public:

    int Init(uint32_t uProtocol, uint32_t uFunction,
             PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
    {
        AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

        int rc = GuestCtrlCallback::Init();
        if (RT_FAILURE(rc))
            return rc;

        return VERR_NOT_IMPLEMENTED;
    }

    void Destroy(void) { GuestCtrlCallback::Destroy(); }

    GuestCbProcessIO& operator=(const GuestCbProcessIO &that)
    {
        return *this;
    }
};
#endif
#endif // ____H_GUESTIMPLPRIVATE

