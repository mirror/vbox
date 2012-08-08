/** @file
 *
 * Internal helpers/structures for guest control functionality.
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/log.h>

/** Maximum number of guest sessions a VM can have. */
#define VBOX_GUESTCTRL_MAX_SESSIONS     255
/** Maximum of guest processes a guest session can have. */
#define VBOX_GUESTCTRL_MAX_PROCESSES    255
/** Maximum of callback contexts a guest process can have. */
#define VBOX_GUESTCTRL_MAX_CONTEXTS     _64K - 1

/** Builds a context ID out of the session ID, process ID and an
 *  increasing count. */
#define VBOX_GUESTCTRL_CONTEXTID_MAKE(uSession, uProcess, uCount) \
    (  (uint32_t)((uSession) &   0xff) << 24 \
     | (uint32_t)((uProcess) &   0xff) << 16 \
     | (uint32_t)((uCount)   & 0xffff)       \
    )
/** Gets the session ID out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uContextID) \
    ((uContextID) >> 24)
/** Gets the process ID out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_PROCESS(uContextID) \
    (((uContextID) >> 16) & 0xff)
/** Gets the conext count of a process out of a context ID. */
#define VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(uContextID) \
    ((uContextID) & 0xffff)

/** Vector holding a process' CPU affinity. */
typedef std::vector <LONG> ProcessAffinity;
/** Vector holding process startup arguments. */
typedef std::vector <Utf8Str> ProcessArguments;

class GuestProcessStreamBlock;


/**
 * Generic class for a all guest control callbacks/events.
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
 * Class representing a guest control callback.
 */
class GuestCtrlCallback : public GuestCtrlEvent
{
public:
    GuestCtrlCallback(void);

    GuestCtrlCallback(eVBoxGuestCtrlCallbackType enmType);

    virtual ~GuestCtrlCallback(void);

public:

    void Destroy(void);

    int Init(eVBoxGuestCtrlCallbackType enmType);

    eVBoxGuestCtrlCallbackType GetCallbackType(void) { return mType; }

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
    eVBoxGuestCtrlCallbackType  mType;
    /** Callback flags. */
    uint32_t                    uFlags;
    /** Payload which will be available on successful
     *  waiting (optional). */
    void                       *pvPayload;
    /** Size of the payload (optional). */
    size_t                      cbPayload;
};
typedef std::map < uint32_t, GuestCtrlCallback* > GuestCtrlCallbacks;

struct GuestProcessWaitResult
{
    /** The wait result when returning from the wait call. */
    ProcessWaitResult_T         mResult;
    int                         mRC;
};


/*
 * Class representing a guest control process event.
 */
class GuestProcessEvent : public GuestCtrlEvent
{
public:
    GuestProcessEvent(void);

    GuestProcessEvent(uint32_t uWaitFlags);

    virtual ~GuestProcessEvent(void);

public:

    void Destroy(void);

    int Init(uint32_t uWaitFlags);

    uint32_t GetWaitFlags(void) { return ASMAtomicReadU32(&mWaitFlags); }

    GuestProcessWaitResult GetResult(void) { return mWaitResult; }

    int Signal(ProcessWaitResult_T enmResult, int rc = VINF_SUCCESS);

protected:

    /** The waiting flag(s). The specifies what to
     *  wait for. */
    uint32_t                    mWaitFlags;
    /** Structure containing the overall result. */
    GuestProcessWaitResult      mWaitResult;
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
    uint32_t                    mFlags;
    ULONG                       mTimeoutMS;
    ProcessPriority_T           mPriority;
    ProcessAffinity             mAffinity;
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

    bool IsEmpty(void) { return m_mapPairs.empty(); }

    int SetValue(const char *pszKey, const char *pszValue);

protected:

    GuestCtrlStreamPairMap m_mapPairs;
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
#endif // ____H_GUESTIMPLPRIVATE

