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
#include "VBoxDefs.h"

/* Using declarations: */
using namespace VBoxGlobalDefs;

/* UIMiniProgressWidget stuff: */
UIMiniProgressWidgetExtension::UIMiniProgressWidgetExtension(const QString &strSource, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<UIMiniProgressWidget>(pParent)
{
    sltSetSource(strSource);
    retranslateUi();
}

void UIMiniProgressWidgetExtension::retranslateUi()
{
    setCancelButtonToolTip(tr("Cancel the <nobr><b>%1</b></nobr> download").arg(UI_ExtPackName));
    setProgressBarToolTip(tr("Downloading the <nobr><b>%1</b></nobr> from <nobr><b>%2</b>...</nobr>")
                             .arg(UI_ExtPackName, source()));
}

/* UIDownloaderExtensionPack stuff: */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::m_pInstance = 0;

/* static */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::create()
{
    if (!m_pInstance)
        m_pInstance = new UIDownloaderExtensionPack;
    return m_pInstance;
}

/* static */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::current()
{
    return m_pInstance;
}

void UIDownloaderExtensionPack::start()
{
    /* Call for base-class: */
    UIDownloader::start();
    /* Notify about downloading started: */
    emit sigDownloadingStarted(UIDownloadType_ExtensionPack);
}

UIDownloaderExtensionPack::UIDownloaderExtensionPack()
{
    /* Prepare instance: */
    if (!m_pInstance)
        m_pInstance = this;

    /* Prepare source/target: */
    QString strExtPackUnderscoredName(QString(UI_ExtPackName).replace(' ', '_'));
    QString strTemplateSourcePath("http://download.virtualbox.org/virtualbox/%1/");
    QString strTemplateSourceName(QString("%1-%2.vbox-extpack").arg(strExtPackUnderscoredName));
    QString strSourcePath(strTemplateSourcePath.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSourceName(strTemplateSourceName.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSource(strSourcePath + strSourceName);
    QString strTargetPath(vboxGlobal().virtualBox().GetHomeFolder());
    QString strTargetName(strSourceName);
    QString strTarget(QDir(strTargetPath).absoluteFilePath(strTargetName));

    /* Set source/target: */
    setSource(strSource);
    setTarget(strTarget);
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
    return msgCenter().confirmDownloadExtensionPack(UI_ExtPackName, source(), pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
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
            msgCenter().warnAboutExtentionPackCantBeSaved(UI_ExtPackName, source(), QDir::toNativeSeparators(target()));
        }

        /* Ask the user for another location for the extension pack file: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(), parentWidget(),
                                                               tr("Select folder to save %1 to").arg(UI_ExtPackName), true);

        /* Check if user had really set a new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

void UIDownloaderExtensionPack::warnAboutNetworkError(const QString &strError)
{
    return msgCenter().cannotDownloadExtensionPack(UI_ExtPackName, source(), strError);
}

