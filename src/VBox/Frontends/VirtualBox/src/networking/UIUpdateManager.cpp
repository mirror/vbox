/* $Id$ */
/** @file
 * VBox Qt GUI - UIUpdateManager class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDir>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

/* GUI includes: */
#include "QIProcess.h"
#include "UICommon.h"
#include "VBoxUtils.h"
#include "UIExecutionQueue.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UINetworkRequestManager.h"
#include "UINetworkCustomer.h"
#include "UINetworkRequest.h"
#include "UINotificationCenter.h"
#include "UIUpdateDefs.h"
#include "UIUpdateManager.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"
#include "CSystemProperties.h"

/* Other VBox includes: */
#include <iprt/path.h>
#include <iprt/system.h>
#include <VBox/version.h>

/* enable to test the version update check */
//#define VBOX_NEW_VERSION_TEST "5.1.12_0 http://unknown.unknown.org/0.0.0/VirtualBox-0.0.0-0-unknown.pkg"


/** UINetworkCustomer extension for new version check. */
class UINewVersionChecker : public UINetworkCustomer
{
    Q_OBJECT;

signals:

    /** Notifies about new version check complete. */
    void sigNewVersionChecked();

public:

    /** Constructs new version checker.
      * @param  fForceCall  Brings whether this customer has forced privelegies. */
    UINewVersionChecker(bool fForceCall);

    /** Starts new version check. */
    void start();

protected:

    /** Handles network reply progress for @a iReceived amount of bytes among @a iTotal. */
    virtual void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal);
    /** Handles network reply failed with specified @a strError. */
    virtual void processNetworkReplyFailed(const QString &strError);
    /** Handles network reply canceling for a passed @a pReply. */
    virtual void processNetworkReplyCanceled(UINetworkReply *pReply);
    /** Handles network reply finishing for a passed @a pReply. */
    virtual void processNetworkReplyFinished(UINetworkReply *pReply);

private:

    /** Returns whether this customer has forced privelegies. */
    bool isItForceCall() const { return m_fForceCall; }

    /** Generates platform information. */
    static QString platformInfo();

    /** Holds whether this customer has forced privelegies. */
    bool  m_fForceCall;
    /** Holds the new version checker URL. */
    QUrl  m_url;
};


/** UIExecutionStep extension to check for the new VirtualBox version. */
class UIUpdateStepVirtualBox : public UIExecutionStep
{
    Q_OBJECT;

public:

    /** Constructs extension step. */
    UIUpdateStepVirtualBox(bool fForceCall);
    /** Destructs extension step. */
    virtual ~UIUpdateStepVirtualBox() /* override final */;

    /** Executes the step. */
    virtual void exec() /* override */;

private:

    /** Holds the new version checker instance. */
    UINewVersionChecker *m_pNewVersionChecker;
};


/** UIExecutionStep extension to check for the new VirtualBox Extension Pack version. */
class UIUpdateStepVirtualBoxExtensionPack : public UIExecutionStep
{
    Q_OBJECT;

public:

    /** Constructs extension step. */
    UIUpdateStepVirtualBoxExtensionPack();

    /** Executes the step. */
    virtual void exec() /* override */;

private slots:

    /** Handles downloaded Extension Pack.
      * @param  strSource  Brings the EP source.
      * @param  strTarget  Brings the EP target.
      * @param  strDigest  Brings the EP digest. */
    void sltHandleDownloadedExtensionPack(const QString &strSource,
                                          const QString &strTarget,
                                          const QString &strDigest);
};


/*********************************************************************************************************************************
*   Class UINewVersionChecker implementation.                                                                                    *
*********************************************************************************************************************************/

UINewVersionChecker::UINewVersionChecker(bool fForceCall)
    : m_fForceCall(fForceCall)
    , m_url("https://update.virtualbox.org/query.php")
{
}

void UINewVersionChecker::start()
{
    /* Compose query: */
    QUrlQuery url;
    url.addQueryItem("platform", uiCommon().virtualBox().GetPackageType());
    /* Check if branding is active: */
    if (uiCommon().brandingIsActive())
    {
        /* Branding: Check whether we have a local branding file which tells us our version suffix "FOO"
                     (e.g. 3.06.54321_FOO) to identify this installation: */
        url.addQueryItem("version", QString("%1_%2_%3").arg(uiCommon().virtualBox().GetVersion())
                                                       .arg(uiCommon().virtualBox().GetRevision())
                                                       .arg(uiCommon().brandingGetKey("VerSuffix")));
    }
    else
    {
        /* Use hard coded version set by VBOX_VERSION_STRING: */
        url.addQueryItem("version", QString("%1_%2").arg(uiCommon().virtualBox().GetVersion())
                                                    .arg(uiCommon().virtualBox().GetRevision()));
    }
    url.addQueryItem("count", QString::number(gEDataManager->applicationUpdateCheckCounter()));
    url.addQueryItem("branch", VBoxUpdateData(gEDataManager->applicationUpdateData()).branchName());
    const QString strUserAgent(QString("VirtualBox %1 <%2>").arg(uiCommon().virtualBox().GetVersion()).arg(platformInfo()));

    /* Send GET request: */
    UserDictionary headers;
    headers["User-Agent"] = strUserAgent;
    QUrl fullUrl(m_url);
    fullUrl.setQuery(url);
    createNetworkRequest(UINetworkRequestType_GET, QList<QUrl>() << fullUrl, QString(), headers);
}

void UINewVersionChecker::processNetworkReplyProgress(qint64, qint64)
{
}

void UINewVersionChecker::processNetworkReplyFailed(const QString &)
{
    emit sigNewVersionChecked();
}

void UINewVersionChecker::processNetworkReplyCanceled(UINetworkReply *)
{
    emit sigNewVersionChecked();
}

void UINewVersionChecker::processNetworkReplyFinished(UINetworkReply *pReply)
{
    /* Deserialize incoming data: */
    const QString strResponseData(pReply->readAll());

#ifdef VBOX_NEW_VERSION_TEST
    strResponseData = VBOX_NEW_VERSION_TEST;
#endif
    /* Newer version of necessary package found: */
    if (strResponseData.indexOf(QRegExp("^\\d+\\.\\d+\\.\\d+(_[0-9A-Z]+)? \\S+$")) == 0)
    {
        const QStringList response = strResponseData.split(" ", QString::SkipEmptyParts);
        msgCenter().showUpdateSuccess(response[0], response[1]);
    }
    /* No newer version of necessary package found: */
    else
    {
        if (isItForceCall())
            msgCenter().showUpdateNotFound();
    }

    /* Increment update check counter: */
    gEDataManager->incrementApplicationUpdateCheckCounter();

    /* Notify about completion: */
    emit sigNewVersionChecked();
}

/* static */
QString UINewVersionChecker::platformInfo()
{
    /* Prepare platform report: */
    QString strPlatform;

#if defined (Q_OS_WIN)
    strPlatform = "win";
#elif defined (Q_OS_LINUX)
    strPlatform = "linux";
#elif defined (Q_OS_MACX)
    strPlatform = "macosx";
#elif defined (Q_OS_OS2)
    strPlatform = "os2";
#elif defined (Q_OS_FREEBSD)
    strPlatform = "freebsd";
#elif defined (Q_OS_SOLARIS)
    strPlatform = "solaris";
#else
    strPlatform = "unknown";
#endif

    /* The format is <system>.<bitness>: */
    strPlatform += QString(".%1").arg(ARCH_BITS);

    /* Add more system information: */
    int vrc;
#ifdef Q_OS_LINUX
    // WORKAROUND:
    // On Linux we try to generate information using script first of all..

    /* Get script path: */
    char szAppPrivPath[RTPATH_MAX];
    vrc = RTPathAppPrivateNoArch(szAppPrivPath, sizeof(szAppPrivPath));
    AssertRC(vrc);
    if (RT_SUCCESS(vrc))
    {
        /* Run script: */
        QByteArray result = QIProcess::singleShot(QString(szAppPrivPath) + "/VBoxSysInfo.sh");
        if (!result.isNull())
            strPlatform += QString(" [%1]").arg(QString(result).trimmed());
        else
            vrc = VERR_TRY_AGAIN; /* (take the fallback path) */
    }
    if (RT_FAILURE(vrc))
#endif /* Q_OS_LINUX */
    {
        /* Use RTSystemQueryOSInfo: */
        char szTmp[256];
        QStringList components;

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            components << QString("Product: %1").arg(szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            components << QString("Release: %1").arg(szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            components << QString("Version: %1").arg(szTmp);

        vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
        if ((RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW) && szTmp[0] != '\0')
            components << QString("SP: %1").arg(szTmp);

        if (!components.isEmpty())
            strPlatform += QString(" [%1]").arg(components.join(" | "));
    }

    return strPlatform;
}


/*********************************************************************************************************************************
*   Class UIUpdateStepVirtualBox implementation.                                                                                 *
*********************************************************************************************************************************/

UIUpdateStepVirtualBox::UIUpdateStepVirtualBox(bool fForceCall)
    : m_pNewVersionChecker(0)
{
    m_pNewVersionChecker = new UINewVersionChecker(fForceCall);
    if (m_pNewVersionChecker)
        connect(m_pNewVersionChecker, &UINewVersionChecker::sigNewVersionChecked,
                this, &UIUpdateStepVirtualBox::sigStepFinished);
}

UIUpdateStepVirtualBox::~UIUpdateStepVirtualBox()
{
    delete m_pNewVersionChecker;
    m_pNewVersionChecker = 0;
}

void UIUpdateStepVirtualBox::exec()
{
    m_pNewVersionChecker->start();
}


/*********************************************************************************************************************************
*   Class UIUpdateStepVirtualBoxExtensionPack implementation.                                                                    *
*********************************************************************************************************************************/

UIUpdateStepVirtualBoxExtensionPack::UIUpdateStepVirtualBoxExtensionPack()
{
}

void UIUpdateStepVirtualBoxExtensionPack::exec()
{
    /* Return if VirtualBox Manager issued a direct request to install EP: */
    if (gUpdateManager->isEPInstallationRequested())
    {
        emit sigStepFinished();
        return;
    }

    /* Return if already downloading: */
    if (UINotificationDownloaderExtensionPack::exists())
    {
        emit sigStepFinished();
        return;
    }

    /* Get extension pack manager: */
    CExtPackManager extPackManager = uiCommon().virtualBox().GetExtensionPackManager();
    /* Return if extension pack manager is NOT available: */
    if (extPackManager.isNull())
    {
        emit sigStepFinished();
        return;
    }

    /* Get extension pack: */
    CExtPack extPack = extPackManager.Find(GUI_ExtPackName);
    /* Return if extension pack is NOT installed: */
    if (extPack.isNull())
    {
        emit sigStepFinished();
        return;
    }

    /* Get VirtualBox version: */
    UIVersion vboxVersion(uiCommon().vboxVersionStringNormalized());
    /* Get extension pack version: */
    QString strExtPackVersion(extPack.GetVersion());

    /* If this version being developed: */
    if (vboxVersion.z() % 2 == 1)
    {
        /* If this version being developed on release branch (we use released one): */
        if (vboxVersion.z() < 97)
            vboxVersion.setZ(vboxVersion.z() - 1);
        /* If this version being developed on trunk (we skip check at all): */
        else
        {
            emit sigStepFinished();
            return;
        }
    }

    /* Get updated VirtualBox version: */
    const QString strVBoxVersion = vboxVersion.toString();

    /* Skip the check if the extension pack is equal to or newer than VBox. */
    if (UIVersion(strExtPackVersion) >= vboxVersion)
    {
        emit sigStepFinished();
        return;
    }

    QString strExtPackEdition(extPack.GetEdition());
    if (strExtPackEdition.contains("ENTERPRISE"))
    {
        /* Inform the user that he should update the extension pack: */
        msgCenter().askUserToDownloadExtensionPack(GUI_ExtPackName, strExtPackVersion, strVBoxVersion);
        /* Never try to download for ENTERPRISE version: */
        emit sigStepFinished();
        return;
    }

    /* Ask the user about extension pack downloading: */
    if (!msgCenter().warnAboutOutdatedExtensionPack(GUI_ExtPackName, strExtPackVersion))
    {
        emit sigStepFinished();
        return;
    }

    /* Download extension pack: */
    UINotificationDownloaderExtensionPack *pNotification = UINotificationDownloaderExtensionPack::instance(GUI_ExtPackName);
    /* After downloading finished => propose to install the Extension Pack: */
    connect(pNotification, &UINotificationDownloaderExtensionPack::sigExtensionPackDownloaded,
            this, &UIUpdateStepVirtualBoxExtensionPack::sltHandleDownloadedExtensionPack);
    /* Also, destroyed downloader is a signal to finish the step: */
    connect(pNotification, &UINotificationDownloaderExtensionPack::sigDownloaderDestroyed,
            this, &UIUpdateStepVirtualBoxExtensionPack::sigStepFinished);
    /* Append and start notification: */
    gpNotificationCenter->append(pNotification);
}

void UIUpdateStepVirtualBoxExtensionPack::sltHandleDownloadedExtensionPack(const QString &strSource,
                                                                           const QString &strTarget,
                                                                           const QString &strDigest)
{
    /* Warn the user about extension pack was downloaded and saved, propose to install it: */
    if (msgCenter().proposeInstallExtentionPack(GUI_ExtPackName, strSource, QDir::toNativeSeparators(strTarget)))
        uiCommon().doExtPackInstallation(strTarget, strDigest, windowManager().mainWindowShown(), NULL);
    /* Propose to delete the downloaded extension pack: */
    if (msgCenter().proposeDeleteExtentionPack(QDir::toNativeSeparators(strTarget)))
    {
        /* Delete the downloaded extension pack: */
        QFile::remove(QDir::toNativeSeparators(strTarget));
        /* Get the list of old extension pack files in VirtualBox homefolder: */
        const QStringList oldExtPackFiles = QDir(uiCommon().homeFolder()).entryList(QStringList("*.vbox-extpack"),
                                                                                    QDir::Files);
        /* Propose to delete old extension pack files if there are any: */
        if (oldExtPackFiles.size())
        {
            if (msgCenter().proposeDeleteOldExtentionPacks(oldExtPackFiles))
            {
                foreach (const QString &strExtPackFile, oldExtPackFiles)
                {
                    /* Delete the old extension pack file: */
                    QFile::remove(QDir::toNativeSeparators(QDir(uiCommon().homeFolder()).filePath(strExtPackFile)));
                }
            }
        }
    }
}


/*********************************************************************************************************************************
*   Class UIUpdateManager implementation.                                                                                        *
*********************************************************************************************************************************/

/* static */
UIUpdateManager* UIUpdateManager::s_pInstance = 0;

UIUpdateManager::UIUpdateManager()
    : m_pQueue(new UIExecutionQueue(this))
    , m_fIsRunning(false)
    , m_uTime(1 /* day */ * 24 /* hours */ * 60 /* minutes */ * 60 /* seconds */ * 1000 /* ms */)
    , m_fEPInstallationRequested(false)
{
    /* Prepare instance: */
    if (s_pInstance != this)
        s_pInstance = this;

    /* Configure queue: */
    connect(m_pQueue, &UIExecutionQueue::sigQueueFinished, this, &UIUpdateManager::sltHandleUpdateFinishing);

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Ask updater to check for the first time, for Selector UI only: */
    if (gEDataManager->applicationUpdateEnabled() && uiCommon().uiType() == UICommon::UIType_SelectorUI)
        QTimer::singleShot(0, this, SLOT(sltCheckIfUpdateIsNecessary()));
#endif /* VBOX_WITH_UPDATE_REQUEST */
}

UIUpdateManager::~UIUpdateManager()
{
    /* Cleanup instance: */
    if (s_pInstance == this)
        s_pInstance = 0;
}

/* static */
void UIUpdateManager::schedule()
{
    /* Ensure instance is NOT created: */
    if (s_pInstance)
        return;

    /* Create instance: */
    new UIUpdateManager;
}

/* static */
void UIUpdateManager::shutdown()
{
    /* Ensure instance is created: */
    if (!s_pInstance)
        return;

    /* Delete instance: */
    delete s_pInstance;
}

void UIUpdateManager::sltForceCheck()
{
    /* Force call for new version check: */
    sltCheckIfUpdateIsNecessary(true /* force call */);
}

void UIUpdateManager::sltCheckIfUpdateIsNecessary(bool fForceCall /* = false */)
{
    /* If already running: */
    if (m_fIsRunning)
    {
        /* And we have a force-call: */
        if (fForceCall)
        {
            /// @todo show notification-center
        }
        return;
    }

    /* Set as running: */
    m_fIsRunning = true;

    /* Load/decode curent update data: */
    VBoxUpdateData currentData(gEDataManager->applicationUpdateData());

    /* If update is really necessary: */
    if (
#ifdef VBOX_NEW_VERSION_TEST
        true ||
#endif
        fForceCall || currentData.isNeedToCheck())
    {
        /* Prepare update queue: */
        m_pQueue->enqueue(new UIUpdateStepVirtualBox(fForceCall));
        m_pQueue->enqueue(new UIUpdateStepVirtualBoxExtensionPack);
        /* Start update queue: */
        m_pQueue->start();
    }
    else
        sltHandleUpdateFinishing();
}

void UIUpdateManager::sltHandleUpdateFinishing()
{
    /* Load/decode curent update data: */
    VBoxUpdateData currentData(gEDataManager->applicationUpdateData());
    /* Encode/save new update data: */
    VBoxUpdateData newData(currentData.periodIndex(), currentData.branchIndex());
    gEDataManager->setApplicationUpdateData(newData.data());

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Ask updater to check for the next time: */
    QTimer::singleShot(m_uTime, this, SLOT(sltCheckIfUpdateIsNecessary()));
#endif /* VBOX_WITH_UPDATE_REQUEST */

    /* Set as not running: */
    m_fIsRunning = false;
}

#include "UIUpdateManager.moc"

