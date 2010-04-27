/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QAction>
#include <QDir>
#include <QFile>

/* Local includes */
#include "UIDownloaderUserManual.h"
#include "QIFileDialog.h"
#include "QIHttp.h"
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

void UIDownloaderUserManual::destroy()
{
    if (m_pInstance)
        delete m_pInstance;
    m_pInstance = 0;
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

void UIDownloaderUserManual::setParentWidget(QWidget *pParent)
{
    m_pParent = pParent;
}

QWidget* UIDownloaderUserManual::parentWidget() const
{
    return m_pParent;
}

UIMiniProcessWidgetUserManual* UIDownloaderUserManual::processWidget(QWidget *pParent /* = 0 */) const
{
    UIMiniProcessWidgetUserManual *pWidget = new UIMiniProcessWidgetUserManual(pParent);

    /* Connect the cancel signal: */
    connect(pWidget, SIGNAL(sigCancel()), this, SLOT(cancelDownloading()));
    /* Connect the signal to notify about source changed: */
    connect(this, SIGNAL(sigSourceChanged(const QString&)), pWidget, SLOT(sltSetSource(const QString&)));
    /* Connect the signal to notify about the download process: */
    connect(this, SIGNAL(sigDownloadProcess(int, int)), pWidget, SLOT(sltProcess(int, int)));
    /* Make sure the widget is destroyed when this class is deleted: */
    connect(this, SIGNAL(destroyed(QObject*)), pWidget, SLOT(close()));

    return pWidget;
}

void UIDownloaderUserManual::startDownload()
{
    /* If at least one source to try left: */
    if (!m_sourcesList.isEmpty())
    {
        /* Set the first of left sources as current one: */
        UIDownloader::setSource(m_sourcesList.takeFirst());
        /* Warn process-bar(s) about source was changed: */
        emit sigSourceChanged(source());
        /* Try to download: */
        acknowledgeStart();
    }
}

void UIDownloaderUserManual::acknowledgeFinished(bool fError)
{
    /* If current source was wrong but other is present
     * we will try other source else we should finish: */
    if (m_pHttp->errorCode() != QIHttp::Aborted && m_pHttp->errorCode() != QIHttp::NoError && !m_sourcesList.isEmpty())
        startDownload();
    else
        UIDownloader::acknowledgeFinished(fError);
}

void UIDownloaderUserManual::downloadFinished(bool fError)
{
    if (fError)
        UIDownloader::downloadFinished(fError);
    else
    {
        /* Read all received data: */
        QByteArray receivedData(m_pHttp->readAll());

        /* Serialize the incoming buffer into the User Manual file: */
        while (true)
        {
            /* Try to open file to save document: */
            QFile file(m_strTarget);
            if (file.open(QIODevice::WriteOnly))
            {
                /* Write received data into file: */
                file.write(receivedData);
                file.close();
                /* Warn user about User Manual document loaded and saved: */
                vboxProblem().warnAboutUserManualDownloaded(m_source.toString(), QDir::toNativeSeparators(m_strTarget));
                /* Warn listener about User Manual was downloaded: */
                emit sigDownloadFinished(m_strTarget);
                /* Close the downloader: */
                QTimer::singleShot(0, this, SLOT(suicide()));
                break;
            }
            else
            {
                /* Warn user about User Manual document loaded but was not saved: */
                vboxProblem().warnAboutUserManualCantBeSaved(m_source.toString(), QDir::toNativeSeparators(m_strTarget));
            }

            /* Ask the user about User Manual file save location: */
            QString target = QIFileDialog::getExistingDirectory(QFileInfo(m_strTarget).absolutePath(), m_pParent,
                                                                tr("Select folder to save User Manual to"), true);
            /* If user reject to set save point: */
            if (target.isNull())
                /* Just close the downloder: */
                QTimer::singleShot(0, this, SLOT(suicide()));
            /* If user set correct save point: */
            else
                /* Store it and try to save User Manual document there: */
                m_strTarget = QDir(target).absoluteFilePath(QFileInfo(m_strTarget).fileName());
        }
    }
}

void UIDownloaderUserManual::suicide()
{
    UIDownloaderUserManual::destroy();
}

UIDownloaderUserManual::UIDownloaderUserManual()
    : UIDownloader()
    , m_pParent(0)
{
}

bool UIDownloaderUserManual::confirmDownload()
{
    return vboxProblem().confirmUserManualDownload(m_source.toString(), m_pHttp->lastResponse().contentLength());
}

void UIDownloaderUserManual::warnAboutError(const QString &strError)
{
    return vboxProblem().warnAboutUserManualCantBeDownloaded(m_source.toString(), strError);
}

