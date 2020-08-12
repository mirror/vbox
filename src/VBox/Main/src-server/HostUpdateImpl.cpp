/* $Id$ */
/** @file
 * IHostUpdate  COM class implementations.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#define LOG_GROUP LOG_GROUP_MAIN_HOSTUPDATE

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
#include "HostUpdateImpl.h"
#include "ProgressImpl.h"
#include "AutoCaller.h"
#include "LoggingNew.h"
#include "VirtualBoxImpl.h"
#include "ThreadTask.h"
#include "SystemPropertiesImpl.h"
#include "VirtualBoxBase.h"


////////////////////////////////////////////////////////////////////////////////
//
// HostUpdate private data definition
//
////////////////////////////////////////////////////////////////////////////////


class HostUpdate::UpdateCheckTask : public ThreadTask
{
public:
    UpdateCheckTask(UpdateCheckType_T aCheckType, HostUpdate *aThat, Progress *aProgress)
        : m_checkType(aCheckType)
        , m_pHostUpdate(aThat)
        , m_ptrProgress(aProgress)
        , m_rc(S_OK)
    {
        m_strTaskName = "UpdateCheckTask";
    }
    ~UpdateCheckTask() { }

private:
    void handler();

    UpdateCheckType_T m_checkType;
    HostUpdate *m_pHostUpdate;

    /** Smart pointer to the progress object for this job. */
    ComObjPtr<Progress> m_ptrProgress;
    HRESULT m_rc; /**< Not really used for anything, at the moment. */

    friend class HostUpdate;  // allow member functions access to private data
};

void HostUpdate::UpdateCheckTask::handler()
{
    HostUpdate *pHostUpdate = this->m_pHostUpdate;

    LogFlowFuncEnter();
    LogFlowFunc(("HostUpdate %p\n", pHostUpdate));

    HRESULT rc = pHostUpdate->i_updateCheckTask(this);

    LogFlowFunc(("rc=%Rhrc\n", rc)); NOREF(rc);
    LogFlowFuncLeave();
}

Utf8Str HostUpdate::i_platformInfo()
{
    /* Prepare platform report: */
    Utf8Str strPlatform;

#if defined (RT_OS_WINDOWS)
    strPlatform = "win";
#elif defined (RT_OS_LINUX)
    strPlatform = "linux";
#elif defined (RT_OS_DARWIN)
    strPlatform = "macosx";
#elif defined (RT_OS_OS2)
    strPlatform = "os2";
#elif defined (RT_OS_FREEBSD)
    strPlatform = "freebsd";
#elif defined (RT_OS_SOLARIS)
    strPlatform = "solaris";
#else
    strPlatform = "unknown";
#endif

    /* The format is <system>.<bitness>: */
    strPlatform.appendPrintf(".%lu", ARCH_BITS);

    /* Add more system information: */
    int vrc;
#ifdef RT_OS_LINUX
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
#endif /* RT_OS_LINUX */
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

HRESULT HostUpdate::i_checkForVBoxUpdate()
{
    HRESULT rc;

    // Default to no update required
    m_updateNeeded = FALSE;

    // Following the sequence of steps in UIUpdateStepVirtualBox::sltStartStep()
    // Build up our query URL starting with the URL basename
    Bstr url("https://update.virtualbox.org/query.php/?");
    Bstr platform;
    rc = mVirtualBox->COMGETTER(PackageType)(platform.asOutParam());
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::packageType() failed: %Rrc"), __FUNCTION__, rc);
    url.appendPrintf("platform=%ls", platform.raw()); // e.g. SOLARIS_64BITS_GENERIC

    // Get the complete current version string for the query URL
    Bstr versionNormalized;
    rc = mVirtualBox->COMGETTER(VersionNormalized)(versionNormalized.asOutParam());
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::versionNormalized() failed: %Rrc"), __FUNCTION__, rc);
    url.appendPrintf("&version=%ls", versionNormalized.raw()); // e.g. 6.1.1
    // url.appendPrintf("&version=6.0.12"); // comment out previous line and uncomment this one for testing

    ULONG revision;
    rc = mVirtualBox->COMGETTER(Revision)(&revision);
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::revision() failed: %Rrc"), __FUNCTION__, rc);
    url.appendPrintf("_%ld", revision); // e.g. 135618

    // acquire the System Properties interface
    ComPtr<ISystemProperties> pSystemProperties;
    rc = mVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::systemProperties() failed: %Rrc"), __FUNCTION__, rc);

    // Update the VBoxUpdate setting 'VBoxUpdateLastCheckDate'
    RTTIME Time;
    RTTIMESPEC TimeNow;
    char szTimeStr[RTTIME_STR_LEN];

    RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&TimeNow)), szTimeStr, sizeof(szTimeStr));
    LogRelFunc(("VBox updating UpdateDate with TimeString = %s\n", szTimeStr));
    rc = pSystemProperties->COMSETTER(VBoxUpdateLastCheckDate)(Bstr(szTimeStr).raw());
    if (FAILED(rc))
        return rc; // ISystemProperties::setLastCheckDate calls setError() on failure

    // Update the queryURL and the VBoxUpdate setting 'VBoxUpdateCount'
    ULONG cVBoxUpdateCount = 0;
    rc = pSystemProperties->COMGETTER(VBoxUpdateCount)(&cVBoxUpdateCount);
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: retrieving ISystemProperties::VBoxUpdateCount failed: %Rrc"), __FUNCTION__, rc);

    cVBoxUpdateCount++;

    rc = pSystemProperties->COMSETTER(VBoxUpdateCount)(cVBoxUpdateCount);
    if (FAILED(rc))
        return rc; // ISystemProperties::setVBoxUpdateCount calls setError() on failure
    url.appendPrintf("&count=%lu", cVBoxUpdateCount);

    // Update the query URL and the VBoxUpdate settings (if necessary) with the 'Target' information.
    VBoxUpdateTarget_T enmTarget = VBoxUpdateTarget_Stable; // default branch is 'stable'
    rc = pSystemProperties->COMGETTER(VBoxUpdateTarget)(&enmTarget);
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: retrieving ISystemProperties::Target failed: %Rrc"), __FUNCTION__, rc);

    switch (enmTarget)
    {
        case VBoxUpdateTarget_AllReleases:
            url.appendPrintf("&branch=allrelease"); // query.php expects 'allrelease' and not 'allreleases'
            break;
        case VBoxUpdateTarget_WithBetas:
            url.appendPrintf("&branch=withbetas");
            break;
        case VBoxUpdateTarget_Stable:
        default:
            url.appendPrintf("&branch=stable");
            break;
    }

    rc = pSystemProperties->COMSETTER(VBoxUpdateTarget)(enmTarget);
    if (FAILED(rc))
        return rc; // ISystemProperties::setTarget calls setError() on failure

    LogRelFunc(("VBox update URL = %s\n", Utf8Str(url).c_str()));

    // Setup the User-Agent headers for the GET request
    Bstr version;
    rc = mVirtualBox->COMGETTER(Version)(version.asOutParam()); // e.g. 6.1.0_RC1
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::version() failed: %Rrc"), __FUNCTION__, rc);

    Utf8StrFmt const strUserAgent("VirtualBox %ls <%s>", version.raw(), HostUpdate::i_platformInfo().c_str());
    LogRelFunc(("userAgent = %s\n", strUserAgent.c_str()));

    RTHTTP hHttp = NIL_RTHTTP;
    int vrc = RTHttpCreate(&hHttp);
    if (RT_FAILURE(vrc))
        return setErrorVrc(vrc, tr("%s: RTHttpCreate() failed: %Rrc"), __FUNCTION__, vrc);

    /// @todo Are there any other headers needed to be added first via RTHttpSetHeaders()?
    vrc = RTHttpAddHeader(hHttp, "User-Agent", strUserAgent.c_str(), strUserAgent.length(), RTHTTPADDHDR_F_BACK);
    if (RT_FAILURE(vrc))
        return setErrorVrc(vrc, tr("%s: RTHttpAddHeader() failed: %Rrc (on User-Agent)"), __FUNCTION__, vrc);

    ProxyMode_T enmProxyMode;
    rc = pSystemProperties->COMGETTER(ProxyMode)(&enmProxyMode);
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: ISystemProperties::proxyMode() failed: %Rrc"), __FUNCTION__, rc);

    if (enmProxyMode == ProxyMode_Manual)
    {
        Bstr strProxyURL;

        rc = pSystemProperties->COMGETTER(ProxyURL)(strProxyURL.asOutParam());
        if (FAILED(rc))
            return setErrorVrc(rc, tr("%s: ISystemProperties::proxyURL() failed: %Rrc"), __FUNCTION__, rc);
        vrc = RTHttpSetProxyByUrl(hHttp, Utf8Str(strProxyURL).c_str());
        if (RT_FAILURE(vrc))
            return setErrorVrc(vrc, tr("%s: RTHttpSetProxyByUrl() failed: %Rrc"), __FUNCTION__, vrc);
    }
    else if (enmProxyMode == ProxyMode_System)
    {
        vrc = RTHttpUseSystemProxySettings(hHttp);
        if (RT_FAILURE(vrc))
            return setErrorVrc(vrc, tr("%s: RTHttpUseSystemProxySettings() failed: %Rrc"), __FUNCTION__, vrc);
    }

    void *pvResponse = 0;
    size_t cbResponse = 0;
    vrc = RTHttpGetBinary(hHttp, Utf8Str(url).c_str(), &pvResponse, &cbResponse);
    if (RT_FAILURE(vrc))
        return setErrorVrc(vrc, tr("%s: RTHttpGetBinary() failed: %Rrc"), __FUNCTION__, vrc);

    RTCList<RTCString> lstHttpReply = RTCString((char *)pvResponse, (size_t)cbResponse).split(" ", RTCString::RemoveEmptyParts);
    RTHttpFreeResponse(pvResponse);

    // If url is platform=DARWIN_64BITS_GENERIC&version=6.0.12&branch=stable for example, the reply is:
    // reply[0] = 6.0.14
    // reply[1] = https://download.virtualbox.org/virtualbox/6.0.14/VirtualBox-6.0.14-133895-OSX.dmg
    // If no update required, 'UPTODATE' is returned.
    if (strcmp(lstHttpReply.at(0).c_str(), "UPTODATE") == 0)
    {
        m_updateNeeded = FALSE;
    }
    else
    {
        m_updateNeeded = TRUE;
        m_updateVersion = lstHttpReply.at(0).c_str();
        m_updateURL = lstHttpReply.at(1).c_str();
        LogRelFunc(("HTTP server reply = %s %s\n", lstHttpReply.at(0).c_str(), lstHttpReply.at(1).c_str()));
    }

    // clean-up HTTP request paperwork
    if (hHttp != NIL_RTHTTP)
        RTHttpDestroy(hHttp);

    return S_OK;
}

HRESULT HostUpdate::i_updateCheckTask(UpdateCheckTask *pTask)
{
    LogFlowFuncEnter();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (pTask->m_checkType == UpdateCheckType_VirtualBox)
        pTask->m_rc = i_checkForVBoxUpdate();

#if 0
    else if (pTask->m_checkType == UpdateCheckType_ExtensionPack)
        ;
    else if (pTask->m_checkType == UpdateCheckType_GuestAdditions)
        ;
    else
        assert();
#endif

    if (!pTask->m_ptrProgress.isNull())
        pTask->m_ptrProgress->i_notifyComplete(pTask->m_rc);

    LogFlowFunc(("rc=%Rhrc\n", pTask->m_rc));
    LogFlowFuncLeave();

    return pTask->m_rc;
}

////////////////////////////////////////////////////////////////////////////////
//
// HostUpdate constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////
HostUpdate::HostUpdate()
    : mVirtualBox(NULL)
{
}

HostUpdate::~HostUpdate()
{
}


HRESULT HostUpdate::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostUpdate::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

HRESULT HostUpdate::init(VirtualBox *aVirtualBox)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Weak reference to a VirtualBox object */
    unconst(mVirtualBox) = aVirtualBox;

    autoInitSpan.setSucceeded();
    return S_OK;
}

void HostUpdate::uninit()
{
    // Enclose the state transition Ready->InUninit->NotReady.
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

HRESULT HostUpdate::getUpdate(ComPtr<IHostUpdate> &aUpdate)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    aUpdate = m_pHostUpdate;
    return S_OK;
}

HRESULT HostUpdate::updateCheck(UpdateCheckType_T aCheckType,
                                ComPtr<IProgress> &aProgress)
{
    HRESULT rc;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    // Check whether VirtualBox updates have been disabled before spawning the task thread.
    ComPtr<ISystemProperties> pSystemProperties;
    rc = mVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: IVirtualBox::systemProperties() failed: %Rrc"), __FUNCTION__, rc);

    BOOL fVBoxUpdateEnabled = true;
    rc = pSystemProperties->COMGETTER(VBoxUpdateEnabled)(&fVBoxUpdateEnabled);
    if (FAILED(rc))
        return setErrorVrc(rc, tr("%s: retrieving ISystemProperties::VBoxUpdateEnabled failed: %Rrc"), __FUNCTION__, rc);

    if (!fVBoxUpdateEnabled)
        return E_NOTIMPL;

    ComObjPtr<Progress> pProgress;
    rc = pProgress.createObject();
    if (FAILED(rc))
    {
        return rc;
    }

    rc = pProgress->init(mVirtualBox,
                         static_cast<IHostUpdate*>(this),
                         tr("Checking for software update..."),
                         TRUE /* aCancelable */);
    if (FAILED(rc))
    {
        return rc;
    }

    /* initialize the worker task */
    UpdateCheckTask *pTask = new UpdateCheckTask(aCheckType, this, pProgress);
    rc = pTask->createThread();
    pTask = NULL;
    if (FAILED(rc))
    {
        return rc;
    }

    rc = pProgress.queryInterfaceTo(aProgress.asOutParam());

    return rc;
}

HRESULT HostUpdate::getUpdateVersion(com::Utf8Str &aUpdateVersion)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m_updateVersion.isNotEmpty())
    {
        aUpdateVersion = m_updateVersion;
    }

    return S_OK;
}

HRESULT HostUpdate::getUpdateURL(com::Utf8Str &aUpdateURL)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m_updateURL.isNotEmpty())
    {
        aUpdateURL = m_updateURL;
    }

    return S_OK;
}

HRESULT HostUpdate::getUpdateResponse(BOOL *aUpdateNeeded)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aUpdateNeeded = m_updateNeeded;

    return S_OK;
}

HRESULT HostUpdate::getUpdateCheckNeeded(BOOL *aUpdateCheckNeeded)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc;
    ComPtr<ISystemProperties> pSystemProperties;
    rc = mVirtualBox->COMGETTER(SystemProperties)(pSystemProperties.asOutParam());
    if (FAILED(rc))
        return rc;

    BOOL fVBoxUpdateEnabled;
    rc = pSystemProperties->COMGETTER(VBoxUpdateEnabled)(&fVBoxUpdateEnabled);
    if (FAILED(rc))
        return rc;

    if (!fVBoxUpdateEnabled)
    {
        *aUpdateCheckNeeded = false;
        return S_OK;
    }

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

    ULONG uVBoxUpdateFrequency = 0;  // value in days
    rc = pSystemProperties->COMGETTER(VBoxUpdateFrequency)(&uVBoxUpdateFrequency);
    if (FAILED(rc))
        return rc;

    if (!uVBoxUpdateFrequency)
    {
        *aUpdateCheckNeeded = false;
        return S_OK;
    }

    ULONG ulSecondsInXDays = uVBoxUpdateFrequency /* in days */ * 24 /* hours */ * 60 /* minutes */ * 60 /* seconds */;
    RTTIMESPEC TimeNow;
    (void) RTTimeNow(&TimeNow);
    PRTTIMESPEC TimeDiff = RTTimeSpecSub(&TimeNow, &LastCheckTime);

    LogRelFunc(("Checking if seconds since last check (%ld) >= Number of seconds in %lu day%s (%ld)\n",
        RTTimeSpecGetSeconds(TimeDiff), uVBoxUpdateFrequency, uVBoxUpdateFrequency > 1 ? "s" : "", ulSecondsInXDays));

    if (RTTimeSpecGetSeconds(TimeDiff) >= ulSecondsInXDays)
        *aUpdateCheckNeeded = true;

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
