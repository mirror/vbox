/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderUserManual class implementation
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
    UIMiniProcessWidgetUserManual *pWidget = new UIMiniProcessWidgetUserManual(m_source.toString(), pParent);

    /* Connect the cancel signal: */
    connect(pWidget, SIGNAL(sigCancel()), this, SLOT(cancelDownloading()));
    /* Connect the signal to notify about the download process: */
    connect(this, SIGNAL(sigDownloadProcess(int, int)), pWidget, SLOT(sltProcess(int, int)));
    /* Make sure the widget is destroyed when this class is deleted: */
    connect(this, SIGNAL(destroyed(QObject*)), pWidget, SLOT(close()));

    return pWidget;
}

void UIDownloaderUserManual::startDownload()
{
    acknowledgeStart();
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
                emit downloadFinished(m_strTarget);
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

