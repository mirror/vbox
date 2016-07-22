/* $Id$ */
/** @file
 * VBox Qt GUI - UIDownloaderExtensionPack class implementation.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
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

/* Global includes: */
# include <QDir>
# include <QFile>

/* Local includes: */
# include "UIDownloaderExtensionPack.h"
# include "UINetworkReply.h"
# include "QIFileDialog.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIModalWindowManager.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <iprt/sha.h>


/* static */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::m_spInstance = 0;

/* static */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::create()
{
    if (!m_spInstance)
        m_spInstance = new UIDownloaderExtensionPack;
    return m_spInstance;
}

/* static */
UIDownloaderExtensionPack* UIDownloaderExtensionPack::current()
{
    return m_spInstance;
}

UIDownloaderExtensionPack::UIDownloaderExtensionPack()
{
    /* Prepare instance: */
    if (!m_spInstance)
        m_spInstance = this;

    /* Prepare source/target: */
    QString strExtPackUnderscoredName(QString(GUI_ExtPackName).replace(' ', '_'));
    QString strTemplateSourcePath("http://download.virtualbox.org/virtualbox/%1/");
    QString strTemplateSourceName(QString("%1-%2.vbox-extpack").arg(strExtPackUnderscoredName));
    QString strSourcePath(strTemplateSourcePath.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSourceName(strTemplateSourceName.arg(vboxGlobal().vboxVersionStringNormalized()));
    QString strSource(strSourcePath + strSourceName);
    QString strTargetPath(vboxGlobal().homeFolder());
    QString strTargetName(strSourceName);
    QString strTarget(QDir(strTargetPath).absoluteFilePath(strTargetName));

    /* Set source/target: */
    setSource(strSource);
    setTarget(strTarget);
}

UIDownloaderExtensionPack::~UIDownloaderExtensionPack()
{
    /* Cleanup instance: */
    if (m_spInstance == this)
        m_spInstance = 0;
}

/* virtual override */
const QString UIDownloaderExtensionPack::description() const
{
    return UIDownloader::description().arg(tr("VirtualBox Extension Pack"));
}

bool UIDownloaderExtensionPack::askForDownloadingConfirmation(UINetworkReply *pReply)
{
    return msgCenter().confirmDownloadExtensionPack(GUI_ExtPackName, source().toString(), pReply->header(UINetworkReply::ContentLengthHeader).toInt());
}

void UIDownloaderExtensionPack::handleDownloadedObject(UINetworkReply *pReply)
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

            /* Calc the SHA-256 on the bytes, creating a string: */
            uint8_t abHash[RTSHA256_HASH_SIZE];
            RTSha256(receivedData.constData(), receivedData.length(), abHash);
            char szDigest[RTSHA256_DIGEST_LEN + 1];
            int rc = RTSha256ToString(abHash, szDigest, sizeof(szDigest));
            if (RT_FAILURE(rc))
            {
                AssertRC(rc);
                szDigest[0] = '\0';
            }

            /* Warn the listener about extension-pack was downloaded: */
            emit sigDownloadFinished(source().toString(), target(), &szDigest[0]);
            break;
        }

        /* Warn the user about extension-pack was downloaded but was NOT saved: */
        msgCenter().cannotSaveExtensionPack(GUI_ExtPackName, source().toString(), QDir::toNativeSeparators(target()));

        /* Ask the user for another location for the extension-pack file: */
        QString strTarget = QIFileDialog::getExistingDirectory(QFileInfo(target()).absolutePath(),
                                                               windowManager().networkManagerOrMainWindowShown(),
                                                               tr("Select folder to save %1 to").arg(GUI_ExtPackName), true);

        /* Check if user had really set a new target: */
        if (!strTarget.isNull())
            setTarget(QDir(strTarget).absoluteFilePath(QFileInfo(target()).fileName()));
        else
            break;
    }
}

