/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class implementation
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

/* Local includes */
#include "UIDownloaderAdditions.h"
#include "QIFileDialog.h"
#include "QIHttp.h"
#include "VBoxProblemReporter.h"

/* Global includes */
#include <QAction>
#include <QDir>
#include <QFile>
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

void UIDownloaderAdditions::destroy()
{
    if (m_pInstance)
        delete m_pInstance;
    m_pInstance = 0;
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

void UIDownloaderAdditions::setParentWidget(QWidget *pParent)
{
    m_pParent = pParent;
}

QWidget *UIDownloaderAdditions::parentWidget() const
{
    return m_pParent;
}

UIMiniProcessWidgetAdditions* UIDownloaderAdditions::processWidget(QWidget *pParent /* = 0 */) const
{
    UIMiniProcessWidgetAdditions *pWidget = new UIMiniProcessWidgetAdditions(m_source.toString(), pParent);

    /* Connect the cancel signal. */
    connect(pWidget, SIGNAL(sigCancel()),
            this, SLOT(cancelDownloading()));
    /* Connect the signal to notify about the download process. */
    connect(this, SIGNAL(sigDownloadProcess(int, int)),
            pWidget, SLOT(sltProcess(int, int)));
    /* Make sure the widget is destroyed when this class is deleted. */
    connect(this, SIGNAL(destroyed(QObject*)),
            pWidget, SLOT(close()));

    return pWidget;
}

void UIDownloaderAdditions::startDownload()
{
    acknowledgeStart();
}

void UIDownloaderAdditions::downloadFinished(bool fError)
{
    if (fError)
        UIDownloader::downloadFinished(fError);
    else
    {
        QByteArray receivedData(m_pHttp->readAll());
        /* Serialize the incoming buffer into the .iso image. */
        while (true)
        {
            QFile file(m_strTarget);
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(receivedData);
                file.close();
                if (vboxProblem().confirmMountAdditions(m_source.toString(),
                                                        QDir::toNativeSeparators(m_strTarget)))
                    emit downloadFinished(m_strTarget);
                QTimer::singleShot(0, this, SLOT(suicide()));
                break;
            }
            else
            {
                vboxProblem().message(m_pParent, VBoxProblemReporter::Error,
                                      tr("<p>Failed to save the downloaded file as "
                                         "<nobr><b>%1</b>.</nobr></p>")
                                      .arg(QDir::toNativeSeparators(m_strTarget)));
            }

            QString target = QIFileDialog::getExistingDirectory(QFileInfo(m_strTarget).absolutePath(), m_pParent,
                                                                tr("Select folder to save Guest Additions image to"), true);
            if (target.isNull())
                QTimer::singleShot(0, this, SLOT(suicide()));
            else
                m_strTarget = QDir(target).absoluteFilePath(QFileInfo(m_strTarget).fileName());
        }
    }
}

void UIDownloaderAdditions::suicide()
{
    if (m_pAction)
        m_pAction->setEnabled(true);
    UIDownloaderAdditions::destroy();
}

UIDownloaderAdditions::UIDownloaderAdditions()
    : UIDownloader()
    , m_pAction(0)
    , m_pParent(0)
{
}

bool UIDownloaderAdditions::confirmDownload()
{
    return vboxProblem().confirmDownloadAdditions(m_source.toString(),
                                                  m_pHttp->lastResponse().contentLength());
}

void UIDownloaderAdditions::warnAboutError(const QString &strError)
{
    return vboxProblem().cannotDownloadGuestAdditions(m_source.toString(), strError);
}

