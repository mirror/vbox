/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIDownloaderAdditions class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <QNetworkReply>
#include <QDir>
#include <QFile>

/* Local includes: */
#include "UIDownloaderAdditions.h"
#include "QIFileDialog.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* static */
UIDownloaderAdditions* UIDownloaderAdditions::m_spInstance = 0;

/* static */
UIDownloaderAdditions* UIDownloaderAdditions::create()
{
    if (!m_spInstance)
        m_spInstance = new UIDownloaderAdditions;
    return m_spInstance;
}

/* static */
UIDownloaderAdditions* UIDownloaderAdditions::current()
{
    return m_spInstance;
}

UIDownloaderAdditions::UIDownloaderAdditions()
{
    /* Prepare instance: */
    if (!m_spInstance)
        m_spInstance = this;

    /* Set description: */
    setDescription(tr("VirtualBox Guest Additions"));

    /* Prepare source/target: */
    const QString &strName = QString("VBoxGuestAdditions_%1.iso").arg(vboxGlobal().vboxVersionStringNormalized());
    const QString &strSource = QString("http://download.virtualbox.org/virtualbox/%1/").arg(vboxGlobal().vboxVersionStringNormalized()) + strName;
    const QString &strTarget = QDir(vboxGlobal().virtualBox().GetHomeFolder()).absoluteFilePath(strName);

    /* Set source/target: */
    setSource(strSource);
    setTarget(strTarget);
}

UIDownloaderAdditions::~UIDownloaderAdditions()
{
    /* Cleanup instance: */
    if (m_spInstance == this)
        m_spInstance = 0;
}

bool UIDownloaderAdditions::askForDownloadingConfirmation(QNetworkReply *pReply)
{
    return msgCenter().confirmDownloadAdditions(source().toString(), pReply->header(QNetworkRequest::ContentLengthHeader).toInt());
}

void UIDownloaderAdditions::handleDownloadedObject(QNetworkReply *pReply)
{
    /* Read received data into the buffer: */
    QByteArray receivedData(pReply->readAll());
    /* Serialize that buffer into the file: */
    while (true)
    {
        /* Try to open file for writing: */
        QFile file(target());
        if (file.open(QIODevice::WriteOnly))
        {
            /* Write buffer into the file: */
            file.write(receivedData);
            file.close();

            /* Warn the user about additions-image loaded and saved, propose to mount it: */
            if (msgCenter().confirmMountAdditions(source().toString(), QDir::toNativeSeparators(target())))
                emit sigDownloadFinished(target());
            break;
        }

        /* Warn the user about additions-image was downloaded but was NOT saved: */
        msgCenter().warnAboutAdditionsCantBeSaved(target());

        /* Ask the user for another location for the additions-image file: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(),
                                                               msgCenter().networkManagerOrMainMachineWindowShown(),
                                                               tr("Select folder to save Guest Additions image to"), true);

        /* Check if user had really set a new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

