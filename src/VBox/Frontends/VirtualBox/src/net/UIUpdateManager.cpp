/* $Id$ */
/** @file
 * VBox Qt GUI - UIUpdateManager class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QPointer>
# include <QTimer>
# include <QUrl>
# include <QUrlQuery>

/* GUI includes: */
# include "QIProcess.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"
# include "UIDownloaderExtensionPack.h"
# include "UIExtraDataManager.h"
# include "UIMessageCenter.h"
# include "UIModalWindowManager.h"
# include "UINetworkManager.h"
# include "UINetworkCustomer.h"
# include "UINetworkRequest.h"
# include "UIUpdateDefs.h"
# include "UIUpdateManager.h"

/* COM includes: */
# include "CExtPack.h"
# include "CExtPackManager.h"
# include "CSystemProperties.h"

/* Other VBox includes: */
# include <iprt/path.h>
# include <iprt/system.h>
# include <VBox/version.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* enable to test the version update check */
//#define VBOX_NEW_VERSION_TEST "5.1.12_0 http://unknown.unknown.org/0.0.0/VirtualBox-0.0.0-0-unknown.pkg"

/* Forward declarations: */
class UIUpdateStep;


/** QObject subclass providing GUI with
  * an object to manage queue of update-steps. */
class UIUpdateQueue : public QObject
{
    Q_OBJECT;

signals:

    /** Starts the queue. */
    void sigStartQueue();

    /** Notifies about queue is finished. */
    void sigQueueFinished();

public:

    /** Constructs update queue passing @a pParent to the base-class. */
    UIUpdateQueue(UIUpdateManager *pParent) : QObject(pParent) {}

    /** Starts the queue. */
    void start() { emit sigStartQueue(); }

private:

    /** Returns whether queue is empty. */
    bool isEmpty() const { return m_pLastStep.isNull(); }

    /** Defines the last queued @a pStep. */
    void setLastStep(UIUpdateStep *pStep) { m_pLastStep = pStep; }
    /** Returns the last queued step. */
    UIUpdateStep *lastStep() const { return m_pLastStep; }

    /** Holds the last queued step reference. */
    QPointer<UIUpdateStep> m_pLastStep;

    /** Allows step to manage queue. */
    friend class UIUpdateStep;
};


/** Update step interface.
  * UINetworkCustomer extension which allows background network updates. */
class UIUpdateStep : public UINetworkCustomer
{
    Q_OBJECT;

signals:

    /** Notifies about step is finished. */
    void sigStepComplete();

public:

    /** Constructs update step passing @a pQueue and @a fForceCall to the base-class. */
    UIUpdateStep(UIUpdateQueue *pQueue, bool fForceCall);

protected:

    /** Handles network reply progress for @a iReceived amount of bytes among @a iTotal. */
    virtual void processNetworkReplyProgress(qint64 iReceived, qint64 iTotal) /* override */;
    /** Handles network reply canceling for a passed @a pReply. */
    virtual void processNetworkReplyCanceled(UINetworkReply *pReply) /* override */;
    /** Handles network reply finishing for a passed @a pReply. */
    virtual void processNetworkReplyFinished(UINetworkReply *pReply) /* override */;

protected slots:

    /** Starts the step. */
    virtual void sltStartStep() = 0;
};


/** Update step subclass to check for the new VirtualBox version. */
class UIUpdateStepVirtualBox : public UIUpdateStep
{
    Q_OBJECT;

public:

    /** Constructs update step passing @a pQueue and @a fForceCall to the base-class. */
    UIUpdateStepVirtualBox(UIUpdateQueue *pQueue, bool fForceCall)
        : UIUpdateStep(pQueue, fForceCall)
        , m_url("https://update.virtualbox.org/query.php")
    {}

protected:

    /** Handles network reply canceling for a passed @a pReply. */
    virtual void processNetworkReplyCanceled(UINetworkReply *pReply) /* override */;
    /** Handles network reply finishing for a passed @a pReply. */
    virtual void processNetworkReplyFinished(UINetworkReply *pReply) /* override */;

    /** Returns description of the current network operation. */
    virtual const QString description() const /* override */;

protected slots:

    /** Starts the step. */
    virtual void sltStartStep() /* override */;

private:

    /** Generates platform information. */
    static QString platformInfo();

    /** Holds the update step URL. */
    QUrl m_url;
};


/** Update step subclass to check for the new VirtualBox Extension Pack version. */
class UIUpdateStepVirtualBoxExtensionPack : public UIUpdateStep
{
    Q_OBJECT;

public:

    /** Constructs update step passing @a pQueue and @a fForceCall to the base-class. */
    UIUpdateStepVirtualBoxExtensionPack(UIUpdateQueue *pQueue, bool fForceCall)
        : UIUpdateStep(pQueue, fForceCall)
    {}

protected slots:

    /** Starts the step. */
    virtual void sltStartStep() /* override */;

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
*   Class UIUpdateStep implementation.                                                                                           *
*********************************************************************************************************************************/

UIUpdateStep::UIUpdateStep(UIUpdateQueue *pQueue, bool fForceCall)
    : UINetworkCustomer(pQueue, fForceCall)
{
    /* If queue has no steps yet: */
    if (pQueue->isEmpty())
    {
        /* Connect starting-signal of the queue to starting-slot of this step: */
        connect(pQueue, &UIUpdateQueue::sigStartQueue, this, &UIUpdateStep::sltStartStep, Qt::QueuedConnection);
    }
    /* If queue has at least one step already: */
    else
    {
        /* Reconnect completion-signal of the last-step from completion-signal of the queue to starting-slot of this step: */
        disconnect(pQueue->lastStep(), &UIUpdateStep::sigStepComplete, pQueue, &UIUpdateQueue::sigQueueFinished);
        connect(pQueue->lastStep(), &UIUpdateStep::sigStepComplete, this, &UIUpdateStep::sltStartStep, Qt::QueuedConnection);
    }

    /* Connect completion-signal of this step to the completion-signal of the queue: */
    connect(this, &UIUpdateStep::sigStepComplete, pQueue, &UIUpdateQueue::sigQueueFinished, Qt::QueuedConnection);
    /* Connect completion-signal of this step to the destruction-slot of this step: */
    connect(this, &UIUpdateStep::sigStepComplete, this, &UIUpdateStep::deleteLater, Qt::QueuedConnection);

    /* Remember this step as the last one: */
    pQueue->setLastStep(this);
}

void UIUpdateStep::processNetworkReplyProgress(qint64, qint64)
{
}

void UIUpdateStep::processNetworkReplyCanceled(UINetworkReply *)
{
}

void UIUpdateStep::processNetworkReplyFinished(UINetworkReply *)
{
}


/*********************************************************************************************************************************
*   Class UIUpdateStepVirtualBox implementation.                                                                                 *
*********************************************************************************************************************************/

void UIUpdateStepVirtualBox::processNetworkReplyCanceled(UINetworkReply *pReply)
{
    Q_UNUSED(pReply);

    /* Notify about step completion: */
    emit sigStepComplete();
}

void UIUpdateStepVirtualBox::processNetworkReplyFinished(UINetworkReply *pReply)
{
    /* Deserialize incoming data: */
    QString strResponseData(pReply->readAll());

#ifdef VBOX_NEW_VERSION_TEST
    strResponseData = VBOX_NEW_VERSION_TEST;
#endif
    /* Newer version of necessary package found: */
    if (strResponseData.indexOf(QRegExp("^\\d+\\.\\d+\\.\\d+(_[0-9A-Z]+)? \\S+$")) == 0)
    {
        QStringList response = strResponseData.split(" ", QString::SkipEmptyParts);
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

    /* Notify about step completion: */
    emit sigStepComplete();
}

const QString UIUpdateStepVirtualBox::description() const
{
    return tr("Checking for a new VirtualBox version...");
}

void UIUpdateStepVirtualBox::sltStartStep()
{
    /* Compose query: */
    QUrlQuery url;
    url.addQueryItem("platform", vboxGlobal().virtualBox().GetPackageType());
    /* Check if branding is active: */
    if (vboxGlobal().brandingIsActive())
    {
        /* Branding: Check whether we have a local branding file which tells us our version suffix "FOO"
                     (e.g. 3.06.54321_FOO) to identify this installation: */
        url.addQueryItem("version", QString("%1_%2_%3").arg(vboxGlobal().virtualBox().GetVersion())
                                                       .arg(vboxGlobal().virtualBox().GetRevision())
                                                       .arg(vboxGlobal().brandingGetKey("VerSuffix")));
    }
    else
    {
        /* Use hard coded version set by VBOX_VERSION_STRING: */
        url.addQueryItem("version", QString("%1_%2").arg(vboxGlobal().virtualBox().GetVersion())
                                                    .arg(vboxGlobal().virtualBox().GetRevision()));
    }
    url.addQueryItem("count", QString::number(gEDataManager->applicationUpdateCheckCounter()));
    url.addQueryItem("branch", VBoxUpdateData(gEDataManager->applicationUpdateData()).branchName());
    QString strUserAgent(QString("VirtualBox %1 <%2>").arg(vboxGlobal().virtualBox().GetVersion()).arg(platformInfo()));

    /* Send GET request: */
    UserDictionary headers;
    headers["User-Agent"] = strUserAgent;
    QUrl fullUrl(m_url);
    fullUrl.setQuery(url);
    createNetworkRequest(UINetworkRequestType_GET, QList<QUrl>() << fullUrl, headers);
}

/* static */
QString UIUpdateStepVirtualBox::platformInfo()
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
*   Class UIUpdateStepVirtualBoxExtensionPack implementation.                                                                    *
*********************************************************************************************************************************/

void UIUpdateStepVirtualBoxExtensionPack::sltStartStep()
{
    /* Return if Selector UI issued a direct request to install EP: */
    if (gUpdateManager->isEPInstallationRequested())
    {
        emit sigStepComplete();
        return;
    }

    /* Return if already downloading: */
    if (UIDownloaderExtensionPack::current())
    {
        emit sigStepComplete();
        return;
    }

    /* Get extension pack: */
    CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(GUI_ExtPackName);
    /* Return if extension pack is NOT installed: */
    if (extPack.isNull())
    {
        emit sigStepComplete();
        return;
    }

    /* Get VirtualBox version: */
    UIVersion vboxVersion(vboxGlobal().vboxVersionStringNormalized());
    /* Get extension pack version: */
    QString strExtPackVersion(extPack.GetVersion());
    QByteArray abExtPackVersion = strExtPackVersion.toUtf8();

    /* If this version being developed: */
    if (vboxVersion.z() % 2 == 1)
    {
        /* If this version being developed on release branch (we use released one): */
        if (vboxVersion.z() < 97)
            vboxVersion.setZ(vboxVersion.z() - 1);
        /* If this version being developed on trunk (we skip check at all): */
        else
        {
            emit sigStepComplete();
            return;
        }
    }

    /* Get updated VirtualBox version: */
    const QString strVBoxVersion = vboxVersion.toString();

    /* Skip the check if the extension pack is equal to or newer than VBox.
     * Note! Use RTStrVersionCompare for the comparison here as it takes the
     *       beta/alpha/preview/whatever tags into consideration when comparing versions. */
    if (RTStrVersionCompare(abExtPackVersion.constData(), strVBoxVersion.toUtf8().constData()) >= 0)
    {
        emit sigStepComplete();
        return;
    }

    QString strExtPackEdition(extPack.GetEdition());
    if (strExtPackEdition.contains("ENTERPRISE"))
    {
        /* Inform the user that he should update the extension pack: */
        msgCenter().askUserToDownloadExtensionPack(GUI_ExtPackName, strExtPackVersion, strVBoxVersion);
        /* Never try to download for ENTERPRISE version: */
        emit sigStepComplete();
        return;
    }

    /* Ask the user about extension pack downloading: */
    if (!msgCenter().warAboutOutdatedExtensionPack(GUI_ExtPackName, strExtPackVersion))
    {
        emit sigStepComplete();
        return;
    }

    /* Create and configure the Extension Pack downloader: */
    UIDownloaderExtensionPack *pDl = UIDownloaderExtensionPack::create();
    /* After downloading finished => propose to install the Extension Pack: */
    connect(pDl, &UIDownloaderExtensionPack::sigDownloadFinished,
            this, &UIUpdateStepVirtualBoxExtensionPack::sltHandleDownloadedExtensionPack);
    /* Also, destroyed downloader is a signal to finish the step: */
    connect(pDl, &UIDownloaderExtensionPack::destroyed,
            this, &UIUpdateStepVirtualBoxExtensionPack::sigStepComplete);
    /* Start downloading: */
    pDl->start();
}

void UIUpdateStepVirtualBoxExtensionPack::sltHandleDownloadedExtensionPack(const QString &strSource,
                                                                           const QString &strTarget,
                                                                           const QString &strDigest)
{
    /* Warn the user about extension pack was downloaded and saved, propose to install it: */
    if (msgCenter().proposeInstallExtentionPack(GUI_ExtPackName, strSource, QDir::toNativeSeparators(strTarget)))
        vboxGlobal().doExtPackInstallation(strTarget, strDigest, windowManager().networkManagerOrMainWindowShown(), NULL);
    /* Propose to delete the downloaded extension pack: */
    if (msgCenter().proposeDeleteExtentionPack(QDir::toNativeSeparators(strTarget)))
    {
        /* Delete the downloaded extension pack: */
        QFile::remove(QDir::toNativeSeparators(strTarget));
        /* Get the list of old extension pack files in VirtualBox homefolder: */
        const QStringList oldExtPackFiles = QDir(vboxGlobal().homeFolder()).entryList(QStringList("*.vbox-extpack"),
                                                                                      QDir::Files);
        /* Propose to delete old extension pack files if there are any: */
        if (oldExtPackFiles.size())
        {
            if (msgCenter().proposeDeleteOldExtentionPacks(oldExtPackFiles))
            {
                foreach (const QString &strExtPackFile, oldExtPackFiles)
                {
                    /* Delete the old extension pack file: */
                    QFile::remove(QDir::toNativeSeparators(QDir(vboxGlobal().homeFolder()).filePath(strExtPackFile)));
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
    : m_pQueue(new UIUpdateQueue(this))
    , m_fIsRunning(false)
    , m_uTime(1 /* day */ * 24 /* hours */ * 60 /* minutes */ * 60 /* seconds */ * 1000 /* ms */)
    , m_fEPInstallationRequested(false)
{
    /* Prepare instance: */
    if (s_pInstance != this)
        s_pInstance = this;

    /* Configure queue: */
    connect(m_pQueue, &UIUpdateQueue::sigQueueFinished, this, &UIUpdateManager::sltHandleUpdateFinishing);

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Ask updater to check for the first time: */
    if (gEDataManager->applicationUpdateEnabled() && !vboxGlobal().isVMConsoleProcess())
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
            /* Just show Network Access Manager: */
            gNetworkManager->show();
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
        new UIUpdateStepVirtualBox(m_pQueue, fForceCall);
        new UIUpdateStepVirtualBoxExtensionPack(m_pQueue, fForceCall);
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

