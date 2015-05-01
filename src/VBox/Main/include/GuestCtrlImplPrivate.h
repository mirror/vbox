/* $Id$ */
/** @file
 *
 * Internal helpers/structures for guest control functionality.
 */

/*
 * Copyright (C) 2011-2015 Oracle Corporation
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
#include "Global.h"

#include <iprt/asm.h>
#include <iprt/env.h>
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



/**
 * Wrapper around the RTEnv API, unusable base class.
 *
 * @remarks Feel free to elevate this class to iprt/cpp/env.h as RTCEnv.
 */
class GuestEnvironmentBase
{
public:
    /**
     * Default constructor.
     *
     * The user must invoke one of the init methods before using the object.
     */
    GuestEnvironmentBase(void)
        : m_hEnv(NIL_RTENV)
    { }

    /**
     * Destructor.
     */
    virtual ~GuestEnvironmentBase(void)
    {
        int rc = RTEnvDestroy(m_hEnv); AssertRC(rc);
        m_hEnv = NIL_RTENV;
    }

    /**
     * Initialize this as a normal environment block.
     * @returns IPRT status code.
     */
    int initNormal(void)
    {
        AssertReturn(m_hEnv == NIL_RTENV, VERR_WRONG_ORDER);
        return RTEnvCreate(&m_hEnv);
    }

    /**
     * Returns the variable count.
     * @return Number of variables.
     * @sa      RTEnvCountEx
     */
    uint32_t count(void) const
    {
        return RTEnvCountEx(m_hEnv);
    }

    /**
     * Deletes the environment change record entirely.
     *
     * The count() method will return zero after this call.
     *
     * @sa      RTEnvReset
     */
    void reset(void)
    {
        int rc = RTEnvReset(m_hEnv);
        AssertRC(rc);
    }

    /**
     * Exports the environment change block as an array of putenv style strings.
     *
     *
     * @returns VINF_SUCCESS or VERR_NO_MEMORY.
     * @param   pArray              The output array.
     */
    int queryPutEnvArray(std::vector<com::Utf8Str> *pArray) const
    {
        uint32_t cVars = RTEnvCountEx(m_hEnv);
        try
        {
            pArray->resize(cVars);
            for (uint32_t iVar = 0; iVar < cVars; iVar++)
            {
                const char *psz = RTEnvGetByIndexRawEx(m_hEnv, iVar);
                AssertReturn(psz, VERR_INTERNAL_ERROR_3); /* someone is racing us! */
                (*pArray)[iVar] = psz;
            }
            return VINF_SUCCESS;
        }
        catch (std::bad_alloc &)
        {
            return VERR_NO_MEMORY;
        }
    }

    /**
     * Applies an array of putenv style strings.
     *
     * @returns IPRT status code.
     * @param   rArray              The array with the putenv style strings.
     * @sa      RTEnvPutEnvEx
     */
    int applyPutEnvArray(const std::vector<com::Utf8Str> &rArray)
    {
        size_t cArray = rArray.size();
        for (size_t i = 0; i < cArray; i++)
        {
            int rc = RTEnvPutEx(m_hEnv, rArray[i].c_str());
            if (RT_FAILURE(rc))
                return rc;
        }
        return VINF_SUCCESS;
    }

    /**
     * See RTEnvQueryUtf8Block for details.
     * @returns IPRT status code.
     * @param   ppszzBlock      Where to return the block pointer.
     * @param   pcbBlock        Where to optionally return the block size.
     * @sa      RTEnvQueryUtf8Block
     */
    int queryUtf8Block(char **ppszzBlock, size_t *pcbBlock)
    {
        return RTEnvQueryUtf8Block(m_hEnv, true /*fSorted*/, ppszzBlock, pcbBlock);
    }

    /**
     * Frees what queryUtf8Block returned, NULL ignored.
     * @sa      RTEnvFreeUtf8Block
     */
    static void freeUtf8Block(char *pszzBlock)
    {
        return RTEnvFreeUtf8Block(pszzBlock);
    }

    /**
     * Get an environment variable.
     *
     * @returns IPRT status code.
     * @param   rName               The variable name.
     * @param   pValue              Where to return the value.
     * @sa      RTEnvGetEx
     */
    int getVariable(const com::Utf8Str &rName, com::Utf8Str *pValue) const
    {
        size_t cchNeeded;
        int rc = RTEnvGetEx(m_hEnv, rName.c_str(), NULL, 0, &cchNeeded);
        if (   RT_SUCCESS(rc)
            || rc == VERR_BUFFER_OVERFLOW)
        {
            try
            {
                pValue->reserve(cchNeeded + 1);
                rc = RTEnvGetEx(m_hEnv, rName.c_str(), pValue->mutableRaw(), pValue->capacity(), NULL);
                pValue->jolt();
            }
            catch (std::bad_alloc &)
            {
                rc = VERR_NO_STR_MEMORY;
            }
        }
        return rc;
    }

    /**
     * Set an environment variable.
     *
     * @returns IPRT status code.
     * @param   rName               The variable name.
     * @param   rValue              The value of the variable.
     * @sa      RTEnvSetEx
     */
    int setVariable(const com::Utf8Str &rName, const com::Utf8Str &rValue)
    {
        return RTEnvSetEx(m_hEnv, rName.c_str(), rValue.c_str());
    }

    /**
     * Unset an environment variable.
     *
     * @returns IPRT status code.
     * @param   rName               The variable name.
     * @sa      RTEnvUnsetEx
     */
    int unsetVariable(const com::Utf8Str &rName)
    {
        return RTEnvUnsetEx(m_hEnv, rName.c_str());
    }

#if 0
private:
    /* No copy operator. */
    GuestEnvironmentBase(const GuestEnvironmentBase &) { throw E_FAIL; }
#else
    /**
     * Copy constructor.
     * @throws HRESULT
     */
    GuestEnvironmentBase(const GuestEnvironmentBase &rThat, bool fChangeRecord)
        : m_hEnv(NIL_RTENV)
    {
        int rc = cloneCommon(rThat, fChangeRecord);
        if (RT_FAILURE(rc))
            throw (Global::vboxStatusCodeToCOM(rc));
    }
#endif

protected:
    /**
     * Common clone/copy method with type conversion abilities.
     *
     * @returns IPRT status code.
     * @param   rThat           The object to clone.
     * @param   fChangeRecord   Whether the this instance is a change record (true)
     *                          or normal (false) environment.
     */
    int cloneCommon(const GuestEnvironmentBase &rThat, bool fChangeRecord)
    {
        int   rc = VINF_SUCCESS;
        RTENV hNewEnv = NIL_RTENV;
        if (rThat.m_hEnv != NIL_RTENV)
        {
            if (RTEnvIsChangeRecord(rThat.m_hEnv) == fChangeRecord)
                rc = RTEnvClone(&hNewEnv, rThat.m_hEnv);
            else
            {
                /* Need to type convert it. */
                if (fChangeRecord)
                    rc = RTEnvCreateChangeRecord(&hNewEnv);
                else
                    rc = RTEnvCreate(&hNewEnv);
                if (RT_SUCCESS(rc))
                {
                    rc = RTEnvApplyChanges(hNewEnv, rThat.m_hEnv);
                    if (RT_FAILURE(rc))
                        RTEnvDestroy(hNewEnv);
                }
            }

        }
        if (RT_SUCCESS(rc))
        {
            RTEnvDestroy(m_hEnv);
            m_hEnv = hNewEnv;
        }
        return rc;
    }


    /** The environment change record. */
    RTENV       m_hEnv;
};


#if 0 /* Not currently used. */
/**
 * Wrapper around the RTEnv API for a normal environment.
 */
class GuestEnvironment : public GuestEnvironmentBase
{
public:
    /**
     * Default constructor.
     *
     * The user must invoke one of the init methods before using the object.
     */
    GuestEnvironment(void)
        : GuestEnvironmentBase()
    { }

    /**
     * Copy operator.
     * @param   rThat       The object to copy.
     * @throws HRESULT
     */
    GuestEnvironment(const GuestEnvironment &rThat)
        : GuestEnvironmentBase(rThat, false /*fChangeRecord*/)
    { }

    /**
     * Copy operator.
     * @param   rThat       The object to copy.
     * @throws HRESULT
     */
    GuestEnvironment(const GuestEnvironmentBase &rThat)
        : GuestEnvironmentBase(rThat, false /*fChangeRecord*/)
    { }

    /**
     * Initialize this as a normal environment block.
     * @returns IPRT status code.
     */
    int initNormal(void)
    {
        AssertReturn(m_hEnv == NIL_RTENV, VERR_WRONG_ORDER);
        return RTEnvCreate(&m_hEnv);
    }

    /**
     * Replaces this environemnt with that in @a rThat.
     *
     * @returns IPRT status code
     * @param   rThat       The environment to copy. If it's a different type
     *                      we'll convert the data to a normal environment block.
     */
    int copy(const GuestEnvironmentBase &rThat)
    {
        return cloneCommon(rThat, false /*fChangeRecord*/);
    }
};
#endif /* unused */


/**
 * Wrapper around the RTEnv API for a environment change record.
 *
 * This class is used as a record of changes to be applied to a different
 * environment block (in VBoxService before launching a new process).
 */
class GuestEnvironmentChanges : public GuestEnvironmentBase
{
public:
    /**
     * Default constructor.
     *
     * The user must invoke one of the init methods before using the object.
     */
    GuestEnvironmentChanges(void)
        : GuestEnvironmentBase()
    { }

    /**
     * Copy operator.
     * @param   rThat       The object to copy.
     * @throws HRESULT
     */
    GuestEnvironmentChanges(const GuestEnvironmentChanges &rThat)
        : GuestEnvironmentBase(rThat, true /*fChangeRecord*/)
    { }

    /**
     * Copy operator.
     * @param   rThat       The object to copy.
     * @throws HRESULT
     */
    GuestEnvironmentChanges(const GuestEnvironmentBase &rThat)
        : GuestEnvironmentBase(rThat, true /*fChangeRecord*/)
    { }

    /**
     * Initialize this as a environment change record.
     * @returns IPRT status code.
     */
    int initChangeRecord(void)
    {
        AssertReturn(m_hEnv == NIL_RTENV, VERR_WRONG_ORDER);
        return RTEnvCreateChangeRecord(&m_hEnv);
    }

    /**
     * Replaces this environemnt with that in @a rThat.
     *
     * @returns IPRT status code
     * @param   rThat       The environment to copy. If it's a different type
     *                      we'll convert the data to a set of changes.
     */
    int copy(const GuestEnvironmentBase &rThat)
    {
        return cloneCommon(rThat, true /*fChangeRecord*/);
    }
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
    /** The executable. */
    Utf8Str                     mExecutable;
    /** Arguments vector (starting with argument \#0). */
    ProcessArguments            mArguments;
    /** The process environment change record.  */
    GuestEnvironmentChanges     mEnvironment;
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

    virtual int i_onRemove(void) = 0;

    /** Callback dispatcher -- must be implemented by the actual object. */
    virtual int i_callbackDispatcher(PVBOXGUESTCTRLHOSTCBCTX pCbCtx, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb) = 0;

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

