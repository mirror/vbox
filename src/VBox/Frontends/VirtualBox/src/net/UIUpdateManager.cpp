/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIUpdateManager class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QEventLoop>
#include <QNetworkReply>
#include <QTimer>
#include <QDir>
#include <QPointer>
#include <VBox/version.h>

/* Local includes: */
#include "UIUpdateDefs.h"
#include "UIUpdateManager.h"
#include "UINetworkManager.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxUtils.h"
#include "UIDownloaderExtensionPack.h"
#include "UIGlobalSettingsExtension.h"
#include "VBoxDefs.h"

/* Using declarations: */
using namespace VBoxGlobalDefs;

/* Interface representing update step: */
class UIUpdateStep : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIUpdateStep()
    {
        /* Connect completion-signal of the step to the destruction-slot of the step: */
        connect(this, SIGNAL(sigStepComplete()), this, SLOT(deleteLater()), Qt::QueuedConnection);
    }

signals:

    /* Completion-signal of the step: */
    void sigStepComplete();

protected slots:

    /* Starting-slot of the step: */
    virtual void sltStartStep() = 0;
};

/* Queue for processing update steps: */
class UIUpdateQueue : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIUpdateQueue(UIUpdateManager *pParent) : QObject(pParent) {}

    /* Enqueue passed-step with previously queued (if any) passed-steps: */
    void add(UIUpdateStep *pStep)
    {
        /* Set 'this' as parent for passed step,
         * that way passed-step could be cleaned-up in case of queue destruction: */
        pStep->setParent(this);

        /* If queue had no passed-steps yet: */
        if (!m_pLastStep)
        {
            /* Connect starting-signal of the queue to starting-slot of the passed-step: */
            connect(this, SIGNAL(sigStartQueue()), pStep, SLOT(sltStartStep()), Qt::QueuedConnection);
        }
        /* If queue already had at least one passed-step: */
        else
        {
            /* Reconnect completion-signal of the last-step from completion-signal of the queue to starting-slot of the passed-step: */
            disconnect(m_pLastStep, SIGNAL(sigStepComplete()), this, SIGNAL(sigQueueFinished()));
            connect(m_pLastStep, SIGNAL(sigStepComplete()), pStep, SLOT(sltStartStep()), Qt::QueuedConnection);
        }

        /* Connect completion-signal of the passed-step to the completion-signal of the queue: */
        connect(pStep, SIGNAL(sigStepComplete()), this, SIGNAL(sigQueueFinished()), Qt::QueuedConnection);

        /* Remember last-step: */
        m_pLastStep = pStep;
    }

    /* Starts a queue: */
    void start()
    {
        /* Emit signal which starts queue: */
        emit sigStartQueue();
    }

signals:

    /* Starting-signal of the queue: */
    void sigStartQueue();

    /* Completion-signal of the queue: */
    void sigQueueFinished();

private:

    /* Guarded pointer to the last passed-step: */
    QPointer<UIUpdateStep> m_pLastStep;
};

/* Update step to check for the new VirtualBox version: */
class UIUpdateStepVirtualBox : public UIUpdateStep
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIUpdateStepVirtualBox(bool fForceCall)
        : m_url("http://update.virtualbox.org/query.php")
        , m_fForceCall(fForceCall)
    {
    }

private slots:

    /* Startup slot: */
    void sltStartStep()
    {
        /* Calculate the count of checks left: */
        int cCount = 1;
        QString strCount = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateCheckCount);
        if (!strCount.isEmpty())
        {
            bool ok = false;
            int c = strCount.toLongLong(&ok);
            if (ok) cCount = c;
        }

        /* Compose query: */
        QUrl url(m_url);
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
        url.addQueryItem("count", QString::number(cCount));
        url.addQueryItem("branch", VBoxUpdateData(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate)).branchName());
        QString strUserAgent(QString("VirtualBox %1 <%2>").arg(vboxGlobal().virtualBox().GetVersion()).arg(vboxGlobal().platformInfo()));

        /* Setup GET request: */
        QNetworkRequest request;
        request.setUrl(url);
        request.setRawHeader("User-Agent", strUserAgent.toAscii());
        QNetworkReply *pReply = gNetworkManager->get(request);
        connect(pReply, SIGNAL(sslErrors(QList<QSslError>)), pReply, SLOT(ignoreSslErrors()));
        connect(pReply, SIGNAL(finished()), this, SLOT(sltHandleCheckReply()));
    }

    /* Finishing slot: */
    void sltHandleCheckReply()
    {
        /* Get corresponding network reply object: */
        QNetworkReply *pReply = qobject_cast<QNetworkReply*>(sender());
        /* And ask it for suicide: */
        pReply->deleteLater();

        /* Handle normal result: */
        if (pReply->error() == QNetworkReply::NoError)
        {
            /* Deserialize incoming data: */
            QString strResponseData(pReply->readAll());

            /* Newer version of necessary package found: */
            if (strResponseData.indexOf(QRegExp("^\\d+\\.\\d+\\.\\d+ \\S+$")) == 0)
            {
                QStringList response = strResponseData.split(" ", QString::SkipEmptyParts);
                msgCenter().showUpdateSuccess(vboxGlobal().mainWindow(), response[0], response[1]);
            }
            /* No newer version of necessary package found: */
            else
            {
                if (m_fForceCall)
                    msgCenter().showUpdateNotFound(vboxGlobal().mainWindow());
            }

            /* Save left count of checks: */
            int cCount = 1;
            QString strCount = vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateCheckCount);
            if (!strCount.isEmpty())
            {
                bool ok = false;
                int c = strCount.toLongLong(&ok);
                if (ok) cCount = c;
            }
            vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateCheckCount, QString("%1").arg((qulonglong)cCount + 1));
        }
        /* Handle errors: */
        else
        {
            if (m_fForceCall)
                msgCenter().showUpdateFailure(vboxGlobal().mainWindow(), pReply->errorString());
        }

        /* Notify about step completion: */
        emit sigStepComplete();
    }

private:

    /* Variables: */
    QUrl m_url;
    bool m_fForceCall;
};

/* Update step to check for the new VirtualBox Extension Pack version: */
class UIUpdateStepVirtualBoxExtensionPack : public UIUpdateStep
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIUpdateStepVirtualBoxExtensionPack() {}

private slots:

    /* Startup slot: */
    void sltStartStep()
    {
        /* Return if already downloading: */
        if (UIDownloaderExtensionPack::current())
        {
            emit sigStepComplete();
            return;
        }

        /* Get extension pack: */
        CExtPack extPack = vboxGlobal().virtualBox().GetExtensionPackManager().Find(UI_ExtPackName);
        /* Return if extension pack is NOT installed: */
        if (extPack.isNull())
        {
            emit sigStepComplete();
            return;
        }

        /* Get VirtualBox version: */
        QString strVBoxVersion(vboxGlobal().vboxVersionStringNormalized());
        VBoxVersion vboxVersion(strVBoxVersion);
        /* Get extension pack version: */
        QString strExtPackVersion(extPack.GetVersion().remove(VBOX_BUILD_PUBLISHER));
        QStringList strExtPackVersionParts = strExtPackVersion.split(QRegExp("[-_]"), QString::SkipEmptyParts);
        VBoxVersion extPackVersion(strExtPackVersionParts[0]);
        /* Check if extension pack version less than required: */
        if ((vboxVersion.z() % 2 != 0) /* Skip unstable VBox version */ ||
            !(extPackVersion < vboxVersion) /* Ext Pack version more or equal to VBox version */)
        {
            emit sigStepComplete();
            return;
        }

        if (strExtPackVersion.contains("ENTERPRISE"))
        {
            /* Inform the user that he should update the extension pack: */
            msgCenter().requestUserDownloadExtensionPack(UI_ExtPackName, strExtPackVersion, strVBoxVersion);
            /* Never try to download for ENTERPRISE version: */
            emit sigStepComplete();
            return;
        }
        else
        {
            /* Ask the user about extension pack downloading: */
            if (!msgCenter().proposeDownloadExtensionPack(UI_ExtPackName, strExtPackVersion))
            {
                emit sigStepComplete();
                return;
            }
        }

        /* Create and configure the Extension Pack downloader: */
        UIDownloaderExtensionPack *pDl = UIDownloaderExtensionPack::create();
        pDl->setParentWidget(msgCenter().mainWindowShown());
        /* After downloading finished => propose to install the Extension Pack: */
        connect(pDl, SIGNAL(sigNotifyAboutExtensionPackDownloaded(const QString &, const QString &)),
                this, SLOT(sltHandleDownloadedExtensionPack(const QString &, const QString &)));
        /* Also, destroyed downloader is a signal to finish the step: */
        connect(pDl, SIGNAL(destroyed(QObject*)), this, SIGNAL(sigStepComplete()));
        /* Start downloading: */
        pDl->start();
    }

    /* Finishing slot: */
    void sltHandleDownloadedExtensionPack(const QString &strSource, const QString &strTarget)
    {
        /* Warn the user about extension pack was downloaded and saved, propose to install it: */
        if (msgCenter().proposeInstallExtentionPack(UI_ExtPackName, strSource, QDir::toNativeSeparators(strTarget)))
            UIGlobalSettingsExtension::doInstallation(strTarget, msgCenter().mainWindowShown(), NULL);
    }
};

/* UIUpdateManager stuff: */
UIUpdateManager* UIUpdateManager::m_pInstance = 0;

/* static */
void UIUpdateManager::schedule()
{
    /* Ensure instance is NOT created: */
    if (m_pInstance)
        return;

    /* Create instance: */
    new UIUpdateManager;
}

/* static */
void UIUpdateManager::shutdown()
{
    /* Ensure instance is created: */
    if (!m_pInstance)
        return;

    /* Delete instance: */
    delete m_pInstance;
}

void UIUpdateManager::sltForceCheck()
{
    /* Force call for new version check: */
    sltCheckIfUpdateIsNecessary(true /* force call */);
}

UIUpdateManager::UIUpdateManager()
    : m_pQueue(new UIUpdateQueue(this))
    , m_uTime(1 /* day */ * 24 /* hours */ * 60 /* minutes */ * 60 /* seconds */ * 1000 /* ms */)
{
    /* Prepare instance: */
    if (m_pInstance != this)
        m_pInstance = this;

    /* Configure queue: */
    connect(m_pQueue, SIGNAL(sigQueueFinished()), this, SLOT(sltHandleUpdateFinishing()));

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Ask updater to check for the first time: */
    if (!vboxGlobal().isVMConsoleProcess())
        QTimer::singleShot(0, this, SLOT(sltCheckIfUpdateIsNecessary()));
#endif /* VBOX_WITH_UPDATE_REQUEST */
}

UIUpdateManager::~UIUpdateManager()
{
    /* Cleanup instance: */
    if (m_pInstance == this)
        m_pInstance = 0;
}

void UIUpdateManager::sltCheckIfUpdateIsNecessary(bool fForceCall /* = false */)
{
    /* Load/decode curent update data: */
    VBoxUpdateData currentData(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate));

    /* If update is really necessary: */
    if (fForceCall || currentData.isNeedToCheck())
    {
        /* Prepare update queue: */
        m_pQueue->add(new UIUpdateStepVirtualBox(fForceCall));
        m_pQueue->add(new UIUpdateStepVirtualBoxExtensionPack);
        /* Start update queue: */
        m_pQueue->start();
    }
    else
        sltHandleUpdateFinishing();
}

void UIUpdateManager::sltHandleUpdateFinishing()
{
    /* Load/decode curent update data: */
    VBoxUpdateData currentData(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate));
    /* Encode/save new update data: */
    VBoxUpdateData newData(currentData.periodIndex(), currentData.branchIndex());
    vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateDate, newData.data());

#ifdef VBOX_WITH_UPDATE_REQUEST
    /* Ask updater to check for the next time: */
    QTimer::singleShot(m_uTime, this, SLOT(sltCheckIfUpdateIsNecessary()));
#endif /* VBOX_WITH_UPDATE_REQUEST */
}

#include "UIUpdateManager.moc"

