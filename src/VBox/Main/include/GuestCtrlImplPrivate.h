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
 * Structure for keeping all the relevant guest directory
 * information around.
 */
struct GuestDirectoryOpenInfo
{
    /** The directory path. */
    Utf8Str                 mPath;
    /** Then open filter. */
    Utf8Str                 mFilter;
    /** Opening flags. */
    uint32_t                mFlags;
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
    /** The file's sharing mode.
     **@todo Not implemented yet.*/
    Utf8Str                 mSharingMode;
    /** Octal creation mode. */
    uint32_t                mCreationMode;
    /** The initial offset on open. */
    uint64_t                mInitialOffset;
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
    int FromMkTemp(const GuestProcessStreamBlock &strmBlk);
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
 * Structure for keeping all the relevant guest session
 * startup parameters around.
 */
class GuestSessionStartupInfo
{
public:

    GuestSessionStartupInfo(void)
        : mIsInternal(false /* Non-internal session */),
          mOpenTimeoutMS(30 * 1000 /* 30s opening timeout */),
          mOpenFlags(0 /* No opening flags set */) { }

    /** The session's friendly name. Optional. */
    Utf8Str                     mName;
    /** The session's unique ID. Used to encode
     *  a context ID. */
    uint32_t                    mID;
    /** Flag indicating if this is an internal session
     *  or not. Internal session are not accessible by
     *  public API clients. */
    bool                        mIsInternal;
    /** Timeout (in ms) used for opening the session. */
    uint32_t                    mOpenTimeoutMS;
    /** Session opening flags. */
    uint32_t                    mOpenFlags;
};


/**
 * Structure for keeping all the relevant guest process
 * startup parameters around.
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
    /** Process affinity. At the moment we
     *  only support 64 VCPUs. API and
     *  guest can do more already!  */
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

    int GetRc(void) const;

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

    uint32_t GetOffset() { return m_cbOffset; }

    size_t GetSize() { return m_cbSize; }

    int ParseBlock(GuestProcessStreamBlock &streamBlock);

protected:

    /** Currently allocated size of internal stream buffer. */
    uint32_t m_cbAllocated;
    /** Currently used size of allocated internal stream buffer. */
    size_t m_cbSize;
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

class GuestWaitEventPayload
{

public:

    GuestWaitEventPayload(void)
        : uType(0),
          cbData(0),
          pvData(NULL) { }

    GuestWaitEventPayload(uint32_t uTypePayload,
                          const void *pvPayload, uint32_t cbPayload)
    {
        if (cbPayload)
        {
            pvData = RTMemAlloc(cbPayload);
            if (pvData)
            {
                uType = uTypePayload;

                memcpy(pvData, pvPayload, cbPayload);
                cbData = cbPayload;
            }
            else /* Throw IPRT error. */
                throw VERR_NO_MEMORY;
        }
        else
        {
            uType = uTypePayload;

            pvData = NULL;
            cbData = 0;
        }
    }

    virtual ~GuestWaitEventPayload(void)
    {
        Clear();
    }

    GuestWaitEventPayload& operator=(const GuestWaitEventPayload &that)
    {
        CopyFromDeep(that);
        return *this;
    }

public:

    void Clear(void)
    {
        if (pvData)
        {
            RTMemFree(pvData);
            cbData = 0;
        }
        uType = 0;
    }

    int CopyFromDeep(const GuestWaitEventPayload &payload)
    {
        Clear();

        int rc = VINF_SUCCESS;
        if (payload.cbData)
        {
            Assert(payload.cbData);
            pvData = RTMemAlloc(payload.cbData);
            if (pvData)
            {
                memcpy(pvData, payload.pvData, payload.cbData);
                cbData = payload.cbData;
                uType = payload.uType;
            }
            else
                rc = VERR_NO_MEMORY;
        }

        return rc;
    }

    const void* Raw(void) const { return pvData; }

    size_t Size(void) const { return cbData; }

    uint32_t Type(void) const { return uType; }

    void* MutableRaw(void) { return pvData; }

protected:

    /** Type of payload. */
    uint32_t uType;
    /** Size (in bytes) of payload. */
    uint32_t cbData;
    /** Pointer to actual payload data. */
    void *pvData;
};

class GuestWaitEventBase
{

protected:

    GuestWaitEventBase(void);
    virtual ~GuestWaitEventBase(void);

public:

    uint32_t                        ContextID(void) { return mCID; };
    int                             GuestResult(void) { return mGuestRc; }
    int                             Result(void) { return mRc; }
    GuestWaitEventPayload &         Payload(void) { return mPayload; }
    int                             SignalInternal(int rc, int guestRc, const GuestWaitEventPayload *pPayload);
    int                             Wait(RTMSINTERVAL uTimeoutMS);

protected:

    int             Init(uint32_t uCID);

protected:

    /* Shutdown indicator. */
    bool                       mfAborted;
    /* Associated context ID (CID). */
    uint32_t                   mCID;
    /** The event semaphore for triggering
     *  the actual event. */
    RTSEMEVENT                 mEventSem;
    /** The event's overall result. If
     *  set to VERR_GSTCTL_GUEST_ERROR,
     *  mGuestRc will contain the actual
     *  error code from the guest side. */
    int                        mRc;
    /** The event'S overall result from the
     *  guest side. If used, mRc must be
     *  set to VERR_GSTCTL_GUEST_ERROR. */
    int                        mGuestRc;
    /** The event's payload data. Optional. */
    GuestWaitEventPayload      mPayload;
};

/** List of public guest event types. */
typedef std::list < VBoxEventType_T > GuestEventTypes;

class GuestWaitEvent : public GuestWaitEventBase
{

public:

    GuestWaitEvent(uint32_t uCID);
    GuestWaitEvent(uint32_t uCID, const GuestEventTypes &lstEvents);
    virtual ~GuestWaitEvent(void);

public:

    int                              Cancel(void);
    const ComPtr<IEvent>             Event(void) { return mEvent; }
    int                              SignalExternal(IEvent *pEvent);
    const GuestEventTypes            Types(void) { return mEventTypes; }
    size_t                           TypeCount(void) { return mEventTypes.size(); }

protected:

    int                              Init(uint32_t uCID);

protected:

    /** List of public event types this event should
     *  be signalled on. Optional. */
    GuestEventTypes            mEventTypes;
    /** Pointer to the actual public event, if any. */
    ComPtr<IEvent>             mEvent;
};
/** Map of pointers to guest events. The primary key
 *  contains the context ID. */
typedef std::map < uint32_t, GuestWaitEvent* > GuestWaitEvents;
/** Map of wait events per public guest event. Nice for
 *  faster lookups when signalling a whole event group. */
typedef std::map < VBoxEventType_T, GuestWaitEvents > GuestEventGroup;

class GuestBase
{

public:

    GuestBase(void);
    virtual ~GuestBase(void);

public:

    /** Signals a wait event using a public guest event; also used for
     *  for external event listeners. */
    int signalWaitEvent(VBoxEventType_T aType, IEvent *aEvent);
    /** Signals a wait event using a guest rc. */
    int signalWaitEventInternal(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, int guestRc, const GuestWaitEventPayload *pPayload);
    /** Signals a wait event without letting public guest events know,
     *  extended director's cut version. */
    int signalWaitEventInternalEx(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, int rc, int guestRc, const GuestWaitEventPayload *pPayload);
public:

    int baseInit(void);
    void baseUninit(void);
    int cancelWaitEvents(void);
    int dispatchGeneric(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb);
    int generateContextID(uint32_t uSessionID, uint32_t uObjectID, uint32_t *puContextID);
    int registerWaitEvent(uint32_t uSessionID, uint32_t uObjectID, GuestWaitEvent **ppEvent);
    int registerWaitEvent(uint32_t uSessionID, uint32_t uObjectID, const GuestEventTypes &lstEvents, GuestWaitEvent **ppEvent);
    void unregisterWaitEvent(GuestWaitEvent *pEvent);
    int waitForEvent(GuestWaitEvent *pEvent, uint32_t uTimeoutMS, VBoxEventType_T *pType, IEvent **ppEvent);

protected:

    /** Pointer to the console object. Needed
     *  for HGCM (VMMDev) communication. */
    Console                 *mConsole;
    /** The next upcoming context ID for this object. */
    uint32_t                 mNextContextID;
    /** Local listener for handling the waiting events
     *  internally. */
    ComPtr<IEventListener>   mLocalListener;
    /** Critical section for wait events access. */
    RTCRITSECT               mWaitEventCritSect;
    /** Map of registered wait events per event group. */
    GuestEventGroup          mWaitEventGroups;
    /** Map of registered wait events. */
    GuestWaitEvents          mWaitEvents;
};

/**
 * Virtual class (interface) for guest objects (processes, files, ...) --
 * contains all per-object callback management.
 */
class GuestObject : public GuestBase
{

public:

    GuestObject(void);
    virtual ~GuestObject(void);

public:

    ULONG getObjectID(void) { return mObjectID; }

protected:

    virtual int onRemove(void) = 0;

    /** Callback dispatcher -- must be implemented by the actual object. */
    virtual int callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb) = 0;

protected:

    int bindToSession(Console *pConsole, GuestSession *pSession, uint32_t uObjectID);
    int registerWaitEvent(const GuestEventTypes &lstEvents, GuestWaitEvent **ppEvent);
    int sendCommand(uint32_t uFunction, uint32_t uParms, PVBOXHGCMSVCPARM paParms);

protected:

    /**
     * Commom parameters for all derived objects, when then have
     * an own mData structure to keep their specific data around.
     */

    /** Pointer to parent session. Per definition
     *  this objects *always* lives shorter than the
     *  parent. */
    GuestSession            *mSession;
    /** The object ID -- must be unique for each guest
     *  object and is encoded into the context ID. Must
     *  be set manually when initializing the object.
     *
     *  For guest processes this is the internal PID,
     *  for guest files this is the internal file ID. */
    uint32_t                 mObjectID;
};
#endif // ____H_GUESTIMPLPRIVATE

