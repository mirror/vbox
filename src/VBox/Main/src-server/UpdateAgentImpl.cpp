/* $Id$ */
/** @file
 * IUpdateAgent COM class implementations.
 */

/*
 * Copyright (C) 2020-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#define LOG_GROUP LOG_GROUP_MAIN_UPDATEAGENT

#include <iprt/cpp/utils.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/http.h>
#include <iprt/system.h>
#include <iprt/message.h>
#include <iprt/pipe.h>
#include <iprt/env.h>
#include <iprt/process.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <VBox/com/defs.h>
#include <VBox/version.h>

#include "HostImpl.h"
#include "UpdateAgentImpl.h"
#include "ProgressImpl.h"
#include "AutoCaller.h"
#include "LoggingNew.h"
#include "VirtualBoxImpl.h"
#include "ThreadTask.h"
#include "SystemPropertiesImpl.h"
#include "VirtualBoxBase.h"


////////////////////////////////////////////////////////////////////////////////
//
// UpdateAgent private data definition
//
////////////////////////////////////////////////////////////////////////////////

class UpdateAgentTask : public ThreadTask
{
public:
    UpdateAgentTask(UpdateAgentBase *aThat, Progress *aProgress)
        : m_pParent(aThat)
        , m_pProgress(aProgress)
    {
        m_strTaskName = "UpdateAgentTask";
    }
    virtual ~UpdateAgentTask(void) { }

private:
    void handler(void);

    /** Weak pointer to parent (update agent). */
    UpdateAgentBase     *m_pParent;
    /** Smart pointer to the progress object for this job. */
    ComObjPtr<Progress>  m_pProgress;

    friend class UpdateAgent;  // allow member functions access to private data
};

void UpdateAgentTask::handler(void)
{
    UpdateAgentBase *pUpdateAgent = this->m_pParent;
    AssertPtr(pUpdateAgent);

    HRESULT rc = pUpdateAgent->i_updateTask(this);

    if (!m_pProgress.isNull())
        m_pProgress->i_notifyComplete(rc);

    LogFlowFunc(("rc=%Rhrc\n", rc)); RT_NOREF(rc);
}


/*********************************************************************************************************************************
*   Update agent base class implementation                                                                                       *
*********************************************************************************************************************************/
UpdateAgent::UpdateAgent()
{
}

UpdateAgent::~UpdateAgent()
{
    delete m;
}

HRESULT UpdateAgent::FinalConstruct()
{
    return BaseFinalConstruct();
}

void UpdateAgent::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

HRESULT UpdateAgent::init(VirtualBox *aVirtualBox)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Weak reference to a VirtualBox object */
    unconst(m_VirtualBox) = aVirtualBox;

    autoInitSpan.setSucceeded();
    return S_OK;
}

void UpdateAgent::uninit()
{
    // Enclose the state transition Ready->InUninit->NotReady.
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

HRESULT UpdateAgent::checkFor(ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aProgress);

    return VBOX_E_NOT_SUPPORTED;
}

HRESULT UpdateAgent::download(ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aProgress);

    return VBOX_E_NOT_SUPPORTED;
}

HRESULT UpdateAgent::install(ComPtr<IProgress> &aProgress)
{
    RT_NOREF(aProgress);

    return VBOX_E_NOT_SUPPORTED;
}

HRESULT UpdateAgent::rollback(void)
{
    return VBOX_E_NOT_SUPPORTED;
}

HRESULT UpdateAgent::getName(com::Utf8Str &aName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aName = mData.m_strName;

    return S_OK;
}

HRESULT UpdateAgent::getOrder(ULONG *aOrder)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aOrder = 0; /* 0 means no order / disabled. */

    return S_OK;
}

HRESULT UpdateAgent::getDependsOn(std::vector<com::Utf8Str> &aDeps)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aDeps.resize(0); /* No dependencies by default. */

    return S_OK;
}

HRESULT UpdateAgent::getVersion(com::Utf8Str &aVer)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aVer = mData.m_lastResult.strVer;

    return S_OK;
}

HRESULT UpdateAgent::getDownloadUrl(com::Utf8Str &aUrl)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aUrl = mData.m_lastResult.strDownloadUrl;

    return S_OK;
}


HRESULT UpdateAgent::getWebUrl(com::Utf8Str &aUrl)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aUrl = mData.m_lastResult.strWebUrl;

    return S_OK;
}

HRESULT UpdateAgent::getReleaseNotes(com::Utf8Str &aRelNotes)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aRelNotes = mData.m_lastResult.strReleaseNotes;

    return S_OK;
}

HRESULT UpdateAgent::getEnabled(BOOL *aEnabled)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = m->fEnabled;

    return S_OK;
}

HRESULT UpdateAgent::setEnabled(const BOOL aEnabled)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->fEnabled = aEnabled;

    return i_commitSettings(alock);
}


HRESULT UpdateAgent::getHidden(BOOL *aHidden)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aHidden = mData.m_fHidden;

    return S_OK;
}

HRESULT UpdateAgent::getState(UpdateState_T *aState)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aState = mData.m_enmState;

    return S_OK;
}

HRESULT UpdateAgent::getCheckFrequency(ULONG *aFreqSeconds)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aFreqSeconds = m->uCheckFreqSeconds;

    return S_OK;
}

HRESULT UpdateAgent::setCheckFrequency(ULONG aFreqSeconds)
{
    if (aFreqSeconds < RT_SEC_1DAY) /* Don't allow more frequent checks for now. */
        return setError(E_INVALIDARG, tr("Frequency too small; one day is the minimum"));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->uCheckFreqSeconds = aFreqSeconds;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::getChannel(UpdateChannel_T *aChannel)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aChannel = m->enmChannel;

    return S_OK;
}

HRESULT UpdateAgent::setChannel(UpdateChannel_T aChannel)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->enmChannel = aChannel;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::getCheckCount(ULONG *aCount)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCount = m->uCheckCount;

    return S_OK;
}

HRESULT UpdateAgent::getRepositoryURL(com::Utf8Str &aRepo)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aRepo = m->strRepoUrl;

    return S_OK;
}

HRESULT UpdateAgent::setRepositoryURL(const com::Utf8Str &aRepo)
{
    if (!aRepo.startsWith("https://", com::Utf8Str::CaseInsensitive))
        return setError(E_INVALIDARG, tr("Invalid URL scheme specified; only https:// is supported."));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strRepoUrl = aRepo;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::getProxyMode(ProxyMode_T *aMode)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMode = m->enmProxyMode;

    return S_OK;
}

HRESULT UpdateAgent::setProxyMode(ProxyMode_T aMode)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->enmProxyMode = aMode;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::getProxyURL(com::Utf8Str &aAddress)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aAddress = m->strProxyUrl;

    return S_OK;
}

HRESULT UpdateAgent::setProxyURL(const com::Utf8Str &aAddress)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strProxyUrl = aAddress;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::getLastCheckDate(com::Utf8Str &aDate)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aDate = m->strLastCheckDate;

    return S_OK;
}

/* static */
Utf8Str UpdateAgentBase::i_getPlatformInfo(void)
{
    /* Prepare platform report: */
    Utf8Str strPlatform;

# if defined (RT_OS_WINDOWS)
    strPlatform = "win";
# elif defined (RT_OS_LINUX)
    strPlatform = "linux";
# elif defined (RT_OS_DARWIN)
    strPlatform = "macosx";
# elif defined (RT_OS_OS2)
    strPlatform = "os2";
# elif defined (RT_OS_FREEBSD)
    strPlatform = "freebsd";
# elif defined (RT_OS_SOLARIS)
    strPlatform = "solaris";
# else
    strPlatform = "unknown";
# endif

    /* The format is <system>.<bitness>: */
    strPlatform.appendPrintf(".%lu", ARCH_BITS);

    /* Add more system information: */
    int vrc;
# ifdef RT_OS_LINUX
    // WORKAROUND:
    // On Linux we try to generate information using script first of all..

    /* Get script path: */
    char szAppPrivPath[RTPATH_MAX];
    vrc = RTPathAppPrivateNoArch(szAppPrivPath, sizeof(szAppPrivPath));
    AssertRC(vrc);
    if (RT_SUCCESS(vrc))
        vrc = RTPathAppend(szAppPrivPath, sizeof(szAppPrivPath), "/VBoxSysInfo.sh");
    AssertRC(vrc);
    if (RT_SUCCESS(vrc))
    {
        RTPIPE hPipeR;
        RTHANDLE hStdOutPipe;
        hStdOutPipe.enmType = RTHANDLETYPE_PIPE;
        vrc = RTPipeCreate(&hPipeR, &hStdOutPipe.u.hPipe, RTPIPE_C_INHERIT_WRITE);
        AssertLogRelRC(vrc);

        char const *szAppPrivArgs[2];
        szAppPrivArgs[0] = szAppPrivPath;
        szAppPrivArgs[1] = NULL;
        RTPROCESS hProc = NIL_RTPROCESS;

        /* Run script: */
        vrc = RTProcCreateEx(szAppPrivPath, szAppPrivArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL /*phStdin*/, &hStdOutPipe,
                             NULL /*phStderr*/, NULL /*pszAsUser*/, NULL /*pszPassword*/, NULL /*pvExtraData*/, &hProc);

        (void) RTPipeClose(hStdOutPipe.u.hPipe);
        hStdOutPipe.u.hPipe = NIL_RTPIPE;

        if (RT_SUCCESS(vrc))
        {
            RTPROCSTATUS  ProcStatus;
            size_t        cbStdOutBuf  = 0;
            size_t        offStdOutBuf = 0;
            char          *pszStdOutBuf = NULL;
            do
            {
                if (hPipeR != NIL_RTPIPE)
                {
                    char    achBuf[1024];
                    size_t  cbRead;
                    vrc = RTPipeReadBlocking(hPipeR, achBuf, sizeof(achBuf), &cbRead);
                    if (RT_SUCCESS(vrc))
                    {
                        /* grow the buffer? */
                        size_t cbBufReq = offStdOutBuf + cbRead + 1;
                        if (   cbBufReq > cbStdOutBuf
                            && cbBufReq < _256K)
                        {
                            size_t cbNew = RT_ALIGN_Z(cbBufReq, 16); // 1024
                            void  *pvNew = RTMemRealloc(pszStdOutBuf, cbNew);
                            if (pvNew)
                            {
                                pszStdOutBuf = (char *)pvNew;
                                cbStdOutBuf  = cbNew;
                            }
                        }

                        /* append if we've got room. */
                        if (cbBufReq <= cbStdOutBuf)
                        {
                            (void) memcpy(&pszStdOutBuf[offStdOutBuf], achBuf, cbRead);
                            offStdOutBuf = offStdOutBuf + cbRead;
                            pszStdOutBuf[offStdOutBuf] = '\0';
                        }
                    }
                    else
                    {
                        AssertLogRelMsg(vrc == VERR_BROKEN_PIPE, ("%Rrc\n", vrc));
                        RTPipeClose(hPipeR);
                        hPipeR = NIL_RTPIPE;
                    }
                }

                /*
                 * Service the process.  Block if we have no pipe.
                 */
                if (hProc != NIL_RTPROCESS)
                {
                    vrc = RTProcWait(hProc,
                                     hPipeR == NIL_RTPIPE ? RTPROCWAIT_FLAGS_BLOCK : RTPROCWAIT_FLAGS_NOBLOCK,
                                     &ProcStatus);
                    if (RT_SUCCESS(vrc))
                        hProc = NIL_RTPROCESS;
                    else
                        AssertLogRelMsgStmt(vrc == VERR_PROCESS_RUNNING, ("%Rrc\n", vrc), hProc = NIL_RTPROCESS);
                }
            } while (   hPipeR != NIL_RTPIPE
                     || hProc != NIL_RTPROCESS);

            if (   ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
                && ProcStatus.iStatus == 0) {
                pszStdOutBuf[offStdOutBuf-1] = '\0';  // remove trailing newline
                Utf8Str pszStdOutBufUTF8(pszStdOutBuf);
                strPlatform.appendPrintf(" [%s]", pszStdOutBufUTF8.strip().c_str());
                // For testing, here is some sample output:
                //strPlatform.appendPrintf(" [Distribution: Redhat | Version: 7.6.1810 | Kernel: Linux version 3.10.0-952.27.2.el7.x86_64 (gcc version 4.8.5 20150623 (Red Hat 4.8.5-36) (GCC) ) #1 SMP Mon Jul 29 17:46:05 UTC 2019]");
            }
        }
        else
            vrc = VERR_TRY_AGAIN; /* (take the fallback path) */
    }

    LogRelFunc(("strPlatform (Linux) = %s\n", strPlatform.c_str()));

    if (RT_FAILURE(vrc))
# endif /* RT_OS_LINUX */
    {
        /* Use RTSystemQueryOSInfo: */
        char szTmp[256];

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            strPlatform.appendPrintf(" [Product: %s", szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            strPlatform.appendPrintf(" %sRelease: %s", strlen(szTmp) == 0 ? "[" : "| ", szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            strPlatform.appendPrintf(" %sVersion: %s", strlen(szTmp) == 0 ? "[" : "| ", szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            strPlatform.appendPrintf(" %sSP: %s]", strlen(szTmp) == 0 ? "[" : "| ", szTmp);

        if (!strPlatform.endsWith("]"))
            strPlatform.append("]");

        LogRelFunc(("strPlatform = %s\n", strPlatform.c_str()));
    }

    return strPlatform;
}

HRESULT UpdateAgent::i_loadSettings(const settings::UpdateAgent &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->fEnabled          = data.fEnabled;
    m->enmChannel        = data.enmChannel;
    m->uCheckFreqSeconds = data.uCheckFreqSeconds;
    m->strRepoUrl        = data.strRepoUrl;
    m->enmProxyMode      = data.enmProxyMode;
    m->strProxyUrl       = data.strProxyUrl;
    m->strLastCheckDate  = data.strLastCheckDate;
    m->uCheckCount       = data.uCheckCount;

    return S_OK;
}

HRESULT UpdateAgent::i_saveSettings(settings::UpdateAgent &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data = *m;

    return S_OK;
}

HRESULT UpdateAgent::i_setCheckCount(ULONG aCount)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->uCheckCount = aCount;

    return i_commitSettings(alock);
}

HRESULT UpdateAgent::i_setLastCheckDate(const com::Utf8Str &aDate)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strLastCheckDate = aDate;

    return i_commitSettings(alock);
}


/*********************************************************************************************************************************
*   Internal helper methods                                                                                                      *
*********************************************************************************************************************************/

/**
 * Internal helper function to commit modified settings.
 *
 * @returns HRESULT
 * @param   aLock               Write lock to release before committing settings.
 */
HRESULT UpdateAgent::i_commitSettings(AutoWriteLock &aLock)
{
    aLock.release();

    AutoWriteLock vboxLock(m_VirtualBox COMMA_LOCKVAL_SRC_POS);
    return m_VirtualBox->i_saveSettings();
}

#if 0
HRESULT UpdateAgent::getUpdateCheckNeeded(BOOL *aUpdateCheckNeeded)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc;
    ComPtr<ISystemProperties> pSystemProperties;
    rc = m_VirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    if (FAILED(rc))
        return rc;

    /*
     * Is update checking enabled?
     */
    BOOL fVBoxUpdateEnabled;
    rc = pSystemProperties->COMGETTER(VBoxUpdateEnabled)(&fVBoxUpdateEnabled);
    if (FAILED(rc))
        return rc;

    if (!fVBoxUpdateEnabled)
    {
        *aUpdateCheckNeeded = false;
        return S_OK;
    }

    /*
     * When was the last update?
     */
    Bstr strVBoxUpdateLastCheckDate;
    rc = pSystemProperties->COMGETTER(VBoxUpdateLastCheckDate)(strVBoxUpdateLastCheckDate.asOutParam());
    if (FAILED(rc))
        return rc;

    // No prior update check performed so do so now
    if (strVBoxUpdateLastCheckDate.isEmpty())
    {
        *aUpdateCheckNeeded = true;
        return S_OK;
    }

    // convert stored timestamp to time spec
    RTTIMESPEC LastCheckTime;
    if (!RTTimeSpecFromString(&LastCheckTime, Utf8Str(strVBoxUpdateLastCheckDate).c_str()))
    {
        *aUpdateCheckNeeded = true;
        return S_OK;
    }

    /*
     * Compare last update with how often we are supposed to check for updates.
     */
    ULONG uVBoxUpdateFrequency = 0;  // value in days
    rc = pSystemProperties->COMGETTER(VBoxUpdateFrequency)(&uVBoxUpdateFrequency);
    if (FAILED(rc))
        return rc;

    if (!uVBoxUpdateFrequency)
    {
        /* Consider config (enable, 0 day interval) as checking once but never again.
           We've already check since we've got a date. */
        *aUpdateCheckNeeded = false;
        return S_OK;
    }
    uint64_t const cSecsInXDays = uVBoxUpdateFrequency * RT_SEC_1DAY_64;

    RTTIMESPEC TimeDiff;
    RTTimeSpecSub(RTTimeNow(&TimeDiff), &LastCheckTime);

    LogRelFunc(("Checking if seconds since last check (%lld) >= Number of seconds in %lu day%s (%lld)\n",
                RTTimeSpecGetSeconds(&TimeDiff), uVBoxUpdateFrequency, uVBoxUpdateFrequency > 1 ? "s" : "", cSecsInXDays));

    if (RTTimeSpecGetSeconds(&TimeDiff) >= (int64_t)cSecsInXDays)
        *aUpdateCheckNeeded = true;

    return S_OK;
}
#endif


/*********************************************************************************************************************************
*   Host update implementation                                                                                                   *
*********************************************************************************************************************************/

HostUpdateAgent::HostUpdateAgent(void)
{
}

HostUpdateAgent::~HostUpdateAgent(void)
{
}


HRESULT HostUpdateAgent::FinalConstruct(void)
{
    return BaseFinalConstruct();
}

void HostUpdateAgent::FinalRelease(void)
{
    uninit();

    BaseFinalRelease();
}

HRESULT HostUpdateAgent::init(VirtualBox *aVirtualBox)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Weak reference to a VirtualBox object */
    unconst(m_VirtualBox) = aVirtualBox;

    /* Initialize the bare minimum to get things going.
     ** @todo Add more stuff later here. */
    mData.m_strName = "VirtualBox";
    mData.m_fHidden = false;

    /* Set default repository. */
    m->strRepoUrl = "https://update.virtualbox.org";

    autoInitSpan.setSucceeded();
    return S_OK;
}

void HostUpdateAgent::uninit()
{
    // Enclose the state transition Ready->InUninit->NotReady.
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

HRESULT HostUpdateAgent::checkFor(ComPtr<IProgress> &aProgress)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComObjPtr<Progress> pProgress;
    HRESULT rc = pProgress.createObject();
    if (FAILED(rc))
        return rc;

    rc = pProgress->init(m_VirtualBox,
                         static_cast<IUpdateAgent*>(this),
                         tr("Checking for update for %s ...", this->mData.m_strName.c_str()),
                         TRUE /* aCancelable */);
    if (FAILED(rc))
        return rc;

    /* initialize the worker task */
    UpdateAgentTask *pTask = new UpdateAgentTask(this, pProgress);
    rc = pTask->createThread();
    pTask = NULL;
    if (FAILED(rc))
        return rc;

    return pProgress.queryInterfaceTo(aProgress.asOutParam());
}


/*********************************************************************************************************************************
*   Host update internal functions                                                                                               *
*********************************************************************************************************************************/

DECLCALLBACK(HRESULT) HostUpdateAgent::i_updateTask(UpdateAgentTask *pTask)
{
    RT_NOREF(pTask);

    // Following the sequence of steps in UIUpdateStepVirtualBox::sltStartStep()
    // Build up our query URL starting with the configured repository.
    Utf8Str strUrl;
    strUrl.appendPrintf("%s/query.php/?", m->strRepoUrl.c_str());

    // Add platform ID.
    Bstr platform;
    HRESULT rc = m_VirtualBox->COMGETTER(PackageType)(platform.asOutParam());
    AssertComRCReturn(rc, rc);
    strUrl.appendPrintf("platform=%ls", platform.raw()); // e.g. SOLARIS_64BITS_GENERIC

    // Get the complete current version string for the query URL
    Bstr versionNormalized;
    rc = m_VirtualBox->COMGETTER(VersionNormalized)(versionNormalized.asOutParam());
    AssertComRCReturn(rc, rc);
    strUrl.appendPrintf("&version=%ls", versionNormalized.raw()); // e.g. 6.1.1
#ifdef DEBUG // Comment out previous line and uncomment this one for testing.
//  strUrl.appendPrintf("&version=6.0.12");
#endif

    ULONG revision = 0;
    rc = m_VirtualBox->COMGETTER(Revision)(&revision);
    AssertComRCReturn(rc, rc);
    strUrl.appendPrintf("_%u", revision); // e.g. 135618

    // Update the last update check timestamp.
    RTTIME Time;
    RTTIMESPEC TimeNow;
    char szTimeStr[RTTIME_STR_LEN];
    RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&TimeNow)), szTimeStr, sizeof(szTimeStr));
    LogRel2(("Update agent (%s): Setting last update check timestamp to '%s'\n", mData.m_strName.c_str(), szTimeStr));

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->strLastCheckDate = szTimeStr;
    m->uCheckCount++;

    rc = i_commitSettings(alock);
    AssertComRCReturn(rc, rc);

    strUrl.appendPrintf("&count=%RU32", m->uCheckCount);

    // Update the query URL (if necessary) with the 'channel' information.
    switch (m->enmChannel)
    {
        case UpdateChannel_All:
            strUrl.appendPrintf("&branch=allrelease"); // query.php expects 'allrelease' and not 'allreleases'
            break;
        case UpdateChannel_WithBetas:
            strUrl.appendPrintf("&branch=withbetas");
            break;
        /** @todo Handle UpdateChannel_WithTesting once implemented on the backend. */
        case UpdateChannel_Stable:
            RT_FALL_THROUGH();
        default:
            strUrl.appendPrintf("&branch=stable");
            break;
    }

    LogRel2(("Update agent (%s): Using URL '%s'\n", mData.m_strName.c_str(), strUrl.c_str()));

    /*
     * Compose the User-Agent header for the GET request.
     */
    Bstr version;
    rc = m_VirtualBox->COMGETTER(Version)(version.asOutParam()); // e.g. 6.1.0_RC1
    AssertComRCReturn(rc, rc);

    Utf8StrFmt const strUserAgent("VirtualBox %ls <%s>", version.raw(), UpdateAgent::i_getPlatformInfo().c_str());
    LogRel2(("Update agent (%s): Using user agent '%s'\n",  mData.m_strName.c_str(), strUserAgent.c_str()));

    /*
     * Create the HTTP client instance and pass it to a inner worker method to
     * ensure proper cleanup.
     */
    RTHTTP hHttp = NIL_RTHTTP;
    int vrc = RTHttpCreate(&hHttp);
    if (RT_SUCCESS(vrc))
    {
        try
        {
            rc = i_checkForUpdateInner(hHttp, strUrl, strUserAgent);
        }
        catch (...)
        {
            AssertFailed();
            rc = E_UNEXPECTED;
        }
        RTHttpDestroy(hHttp);
    }
    else
        rc = setErrorVrc(vrc, tr("Update agent (%s): RTHttpCreate() failed: %Rrc"), mData.m_strName.c_str(), vrc);

    return rc;
}

HRESULT HostUpdateAgent::i_checkForUpdateInner(RTHTTP hHttp, Utf8Str const &strUrl, Utf8Str const &strUserAgent)
{
    /** @todo Are there any other headers needed to be added first via RTHttpSetHeaders()? */
    int vrc = RTHttpAddHeader(hHttp, "User-Agent", strUserAgent.c_str(), strUserAgent.length(), RTHTTPADDHDR_F_BACK);
    if (RT_FAILURE(vrc))
        return setErrorVrc(vrc, tr("Update agent (%s): RTHttpAddHeader() failed: %Rrc (on User-Agent)"),
                                   mData.m_strName.c_str(), vrc);

    /*
     * Configure proxying.
     */
    if (m->enmProxyMode == ProxyMode_Manual)
    {
        vrc = RTHttpSetProxyByUrl(hHttp, m->strProxyUrl.c_str());
        if (RT_FAILURE(vrc))
            return setErrorVrc(vrc, tr("Update agent (%s): RTHttpSetProxyByUrl() failed: %Rrc"), mData.m_strName.c_str(), vrc);
    }
    else if (m->enmProxyMode == ProxyMode_System)
    {
        vrc = RTHttpUseSystemProxySettings(hHttp);
        if (RT_FAILURE(vrc))
            return setErrorVrc(vrc, tr("Update agent (%s): RTHttpUseSystemProxySettings() failed: %Rrc"),
                                       mData.m_strName.c_str(), vrc);
    }
    else
        Assert(m->enmProxyMode == ProxyMode_NoProxy);

    /*
     * Perform the GET request, returning raw binary stuff.
     */
    void *pvResponse = NULL;
    size_t cbResponse = 0;
    vrc = RTHttpGetBinary(hHttp, strUrl.c_str(), &pvResponse, &cbResponse);
    if (RT_FAILURE(vrc))
        return setErrorVrc(vrc, tr("Update agent (%s): RTHttpGetBinary() failed: %Rrc"), mData.m_strName.c_str(), vrc);

    /* Note! We can do nothing that might throw exceptions till we call RTHttpFreeResponse! */

    /*
     * If url is platform=DARWIN_64BITS_GENERIC&version=6.0.12&branch=stable for example, the reply is:
     *      6.0.14<SPACE>https://download.virtualbox.org/virtualbox/6.0.14/VirtualBox-6.0.14-133895-OSX.dmg
     * If no update required, 'UPTODATE' is returned.
     */
    /* Parse out the two first words of the response, ignoring whatever follows: */
    const char *pchResponse = (const char *)pvResponse;
    while (cbResponse > 0 && *pchResponse == ' ')
        cbResponse--, pchResponse++;

    char ch;
    const char *pchWord0 = pchResponse;
    while (cbResponse > 0 && (ch = *pchResponse) != ' ' && ch != '\0')
        cbResponse--, pchResponse++;
    size_t const cchWord0 = (size_t)(pchResponse - pchWord0);

    while (cbResponse > 0 && *pchResponse == ' ')
        cbResponse--, pchResponse++;
    const char *pchWord1 = pchResponse;
    while (cbResponse > 0 && (ch = *pchResponse) != ' ' && ch != '\0')
        cbResponse--, pchResponse++;
    size_t const cchWord1 = (size_t)(pchResponse - pchWord1);

    HRESULT rc;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Decode the two word: */
    static char const s_szUpToDate[] = "UPTODATE";
    if (   cchWord0 == sizeof(s_szUpToDate) - 1
        && memcmp(pchWord0, s_szUpToDate, sizeof(s_szUpToDate) - 1) == 0)
    {
        mData.m_enmState = UpdateState_NotAvailable;
        rc = S_OK;
    }
    else
    {
        mData.m_enmState = UpdateState_Error; /* Play safe by default. */

        vrc = RTStrValidateEncodingEx(pchWord0, cchWord0, 0 /*fFlags*/);
        if (RT_SUCCESS(vrc))
            vrc = RTStrValidateEncodingEx(pchWord1, cchWord1, 0 /*fFlags*/);
        if (RT_SUCCESS(vrc))
        {
            /** @todo Any additional sanity checks we could perform here? */
            rc = mData.m_lastResult.strVer.assignEx(pchWord0, cchWord0);
            if (SUCCEEDED(rc))
                rc = mData.m_lastResult.strDownloadUrl.assignEx(pchWord1, cchWord1);

            if (RT_SUCCESS(vrc))
            {
                /** @todo Implement this on the backend first.
                 *        We also could do some guessing based on the installed version vs. reported update version? */
                mData.m_lastResult.enmSeverity = UpdateSeverity_Invalid;
                mData.m_enmState               = UpdateState_Available;
            }

            LogRel(("Update agent (%s): HTTP server replied: %.*s %.*s\n",
                    mData.m_strName.c_str(), cchWord0, pchWord0, cchWord1, pchWord1));
        }
        else
            rc = setErrorVrc(vrc, tr("Update agent (%s): Invalid server response: %Rrc (%.*Rhxs -- %.*Rhxs)"),
                             mData.m_strName.c_str(), vrc, cchWord0, pchWord0, cchWord1, pchWord1);
    }

    RTHttpFreeResponse(pvResponse);

    return rc;
}

