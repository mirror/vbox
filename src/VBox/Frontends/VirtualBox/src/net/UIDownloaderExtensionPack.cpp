/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloader for extension pack
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <QDir>
#include <QFile>
#include <QNetworkReply>

/* Local includes: */
#include "UIDownloaderExtensionPack.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QIFileDialog.h"

/* UIMiniProgressWidget stuff: */
UIMiniProgressWidgetExtension::UIMiniProgressWidgetExtension(const QString &strSource, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
{
    sltSetSource(strSource);
    retranslateUi();
}

void UIMiniProgressWidgetExtension::retranslateUi()
{
    setCancelButtonToolTip(tr("Cancel the VirtualBox Extension Pack download"));
    setProgressBarToolTip(tr("Downloading the VirtualBox Extension Pack from <nobr><b>%1</b>...</nobr>")
                            .arg(source()));
}

/* UIDownloaderExtensionPack stuff: */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::m_pInstance = 0;

/* static */
void UIDownloaderExtensionPack::download(QObject *pListener)
{
    /* Create downloader instance: */
    UIDownloaderExtensionPack *pDownloader = new UIDownloaderExtensionPack;
    pDownloader->setParentWidget(msgCenter().mainWindowShown());

    /* Configure connections for the passed listener: */
    connect(pDownloader, SIGNAL(sigToStartAcknowledging()),
            pListener, SIGNAL(sigDownloaderCreatedForExtensionPack()));
    connect(pDownloader, SIGNAL(sigNotifyAboutExtensionPackDownloaded(const QString &, const QString &)),
            pListener, SLOT(sltHandleDownloadedExtensionPack(const QString &, const QString &)));
}

UIDownloaderExtensionPack::UIDownloaderExtensionPack()
{
    /* Prepare instance: */
    if (!m_pInstance)
        m_pInstance = this;

    /* Prepare source/target: */
    QString strTemplateSourcePath("http://download.virtualbox.org/virtualbox/%1/");
    QString strTemplateSourceName("Oracle_VM_VirtualBox_Extension_Pack-%1.vbox-extpack");
    QString strSourcePath(strTemplateSourcePath.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSourceName(strTemplateSourceName.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSource(strSourcePath + strSourceName);
    QString strTargetPath(vboxGlobal().virtualBox().GetHomeFolder());
    QString strTargetName(strSourceName);
    QString strTarget(QDir(strTargetPath).absoluteFilePath(strTargetName));

    /* Set source/target: */
    setSource(strSource);
    setTarget(strTarget);

    /* Start downloading: */
    start();
}

UIDownloaderExtensionPack::~UIDownloaderExtensionPack()
{
    /* Cleanup instance: */
    if (m_pInstance == this)
        m_pInstance = 0;
}

UIMiniProgressWidget* UIDownloaderExtensionPack::createProgressWidgetFor(QWidget *pParent) const
{
    return new UIMiniProgressWidgetExtension(source(), pParent);
}

bool UIDownloaderExtensionPack::askForDownloadingConfirmation(QNetworkReply *pReply)
{
    return msgCenter().confirmDownloadExtensionPack(source(), pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
}

void UIDownloaderExtensionPack::handleDownloadedObject(QNetworkReply *pReply)
{
    /* Read received data into buffer: */
    QByteArray receivedData(pReply->readAll());
    /* Serialize the incoming buffer into the file: */
    while (true)
    {
        /* Try to open file for writing: */
        QFile file(target());
        if (file.open(QIODevice::WriteOnly))
        {
            /* Write incoming buffer into the file: */
            file.write(receivedData);
            file.close();
            /* Notify listener about extension pack was downloaded: */
            emit sigNotifyAboutExtensionPackDownloaded(source(), target());
            break;
        }
        else
        {
            /* Warn the user about extension pack was downloaded but was NOT saved, explain it: */
            msgCenter().warnAboutExtentionPackCantBeSaved(source(), QDir::toNativeSeparators(target()));
        }

        /* Ask the user for another location for the extension pack file: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(), parentWidget(),
                                                               tr("Select folder to save VirtualBox Extension Pack to"), true);

        /* Check if user had really set a new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

void UIDownloaderExtensionPack::warnAboutNetworkError(const QString &strError)
{
    return msgCenter().cannotDownloadExtensionPack(source(), strError);
}

