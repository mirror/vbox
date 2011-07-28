/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class implementation
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
#include "UIDownloaderUserManual.h"
#include "QIFileDialog.h"
#include "VBoxProblemReporter.h"

UIDownloaderUserManual *UIDownloaderUserManual::m_pInstance = 0;

UIDownloaderUserManual *UIDownloaderUserManual::create()
{
    if (!m_pInstance)
        m_pInstance = new UIDownloaderUserManual;
    return m_pInstance;
}

UIDownloaderUserManual *UIDownloaderUserManual::current()
{
    return m_pInstance;
}

void UIDownloaderUserManual::setSource(const QString &strSource)
{
    /* Erase the list first: */
    m_sourcesList.clear();
    /* And add there passed value: */
    addSource(strSource);
}

void UIDownloaderUserManual::addSource(const QString &strSource)
{
    /* Append passed value: */
    m_sourcesList << strSource;
}

void UIDownloaderUserManual::start()
{
    /* If at least one source to try left: */
    if (!m_sourcesList.isEmpty())
    {
        /* Set the first of left sources as current one: */
        UIDownloader::setSource(m_sourcesList.takeFirst());
        /* Warn process-bar(s) about source was changed: */
        emit sigSourceChanged(source());
        /* Try to download: */
        startDelayedAcknowledging();
    }
}

UIDownloaderUserManual::UIDownloaderUserManual()
    : UIDownloader()
{
}

UIDownloaderUserManual::~UIDownloaderUserManual()
{
    if (m_pInstance == this)
        m_pInstance = 0;
}

void UIDownloaderUserManual::handleError(QNetworkReply *pReply)
{
    /* Check if other sources present: */
    if (!m_sourcesList.isEmpty())
    {
        /* Restart acknowledging: */
        start();
    }
    else
    {
        /* Call for base-class: */
        UIDownloader::handleError(pReply);
    }
}

UIMiniProgressWidget* UIDownloaderUserManual::createProgressWidgetFor(QWidget *pParent) const
{
    return new UIMiniProcessWidgetUserManual(pParent);
}

bool UIDownloaderUserManual::askForDownloadingConfirmation(QNetworkReply *pReply)
{
    return vboxProblem().confirmUserManualDownload(source(), pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
}

void UIDownloaderUserManual::handleDownloadedObject(QNetworkReply *pReply)
{
    /* Read received data: */
    QByteArray receivedData(pReply->readAll());
    /* Serialize the incoming buffer into the User Manual file: */
    while (true)
    {
        /* Try to open file to save document: */
        QFile file(target());
        if (file.open(QIODevice::WriteOnly))
        {
            /* Write received data into file: */
            file.write(receivedData);
            file.close();
            /* Warn user about User Manual document loaded and saved: */
            vboxProblem().warnAboutUserManualDownloaded(source(), QDir::toNativeSeparators(target()));
            /* Warn listener about User Manual was downloaded: */
            emit sigDownloadFinished(target());
            break;
        }
        else
        {
            /* Warn user about User Manual document loaded but was not saved: */
            vboxProblem().warnAboutUserManualCantBeSaved(source(), QDir::toNativeSeparators(target()));
        }

        /* Ask the user about User Manual file save location: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(), parentWidget(),
                                                               tr("Select folder to save User Manual to"), true);

        /* Check if user set new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

void UIDownloaderUserManual::warnAboutNetworkError(const QString &strError)
{
    return vboxProblem().warnAboutUserManualCantBeDownloaded(source(), strError);
}

