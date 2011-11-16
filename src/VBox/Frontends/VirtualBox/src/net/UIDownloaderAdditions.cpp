/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class implementation
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
#include <QAction>
#include <QDir>
#include <QFile>
#include <QNetworkReply>

/* Local includes: */
#include "UIDownloaderAdditions.h"
#include "QIFileDialog.h"
#include "UIMessageCenter.h"

UIDownloaderAdditions *UIDownloaderAdditions::m_pInstance = 0;

UIDownloaderAdditions *UIDownloaderAdditions::create()
{
    if (!m_pInstance)
        m_pInstance = new UIDownloaderAdditions;
    return m_pInstance;
}

UIDownloaderAdditions *UIDownloaderAdditions::current()
{
    return m_pInstance;
}

void UIDownloaderAdditions::setAction(QAction *pAction)
{
    m_pAction = pAction;
    if (m_pAction)
        m_pAction->setEnabled(false);
}

QAction *UIDownloaderAdditions::action() const
{
    return m_pAction;
}

void UIDownloaderAdditions::start()
{
    /* Call for base-class: */
    UIDownloader::start();
    /* Notify about downloading started: */
    emit sigDownloadingStarted(UIDownloadType_Additions);
}

UIDownloaderAdditions::UIDownloaderAdditions()
    : UIDownloader()
{
}

UIDownloaderAdditions::~UIDownloaderAdditions()
{
    if (m_pAction)
        m_pAction->setEnabled(true);
    if (m_pInstance == this)
        m_pInstance = 0;
}

UIMiniProgressWidget* UIDownloaderAdditions::createProgressWidgetFor(QWidget *pParent) const
{
    return new UIMiniProgressWidgetAdditions(source(), pParent);
}

bool UIDownloaderAdditions::askForDownloadingConfirmation(QNetworkReply *pReply)
{
    return msgCenter().confirmDownloadAdditions(source(), pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
}

void UIDownloaderAdditions::handleDownloadedObject(QNetworkReply *pReply)
{
    /* Read received data: */
    QByteArray receivedData(pReply->readAll());
    /* Serialize the incoming buffer into the .iso image: */
    while (true)
    {
        /* Try to open file to save image: */
        QFile file(target());
        if (file.open(QIODevice::WriteOnly))
        {
            /* Write received data into the file: */
            file.write(receivedData);
            file.close();
            /* Warn user about additions image loaded and saved, propose to mount it: */
            if (msgCenter().confirmMountAdditions(source(), QDir::toNativeSeparators(target())))
                emit sigDownloadFinished(target());
            break;
        }
        else
        {
            /* Warn user about additions image loaded but was not saved: */
            msgCenter().warnAboutAdditionsCantBeSaved(target());
        }

        /* Ask the user about additions image file save location: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(), parentWidget(),
                                                               tr("Select folder to save Guest Additions image to"), true);

        /* Check if user set new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

void UIDownloaderAdditions::warnAboutNetworkError(const QString &strError)
{
    return msgCenter().cannotDownloadGuestAdditions(source(), strError);
}

