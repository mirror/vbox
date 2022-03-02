/* $Id$ */
/** @file
 * VBox Qt GUI - Qt GUI - Utility Classes and Functions specific to X11..
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox includes */
#include "UIDesktopServices.h"

/* Qt includes */
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>


bool UIDesktopServices::createMachineShortcut(const QString & /* strSrcFile */, const QString &strDstPath, const QString &strName, const QUuid &uUuid)
{
    QFile link(strDstPath + QDir::separator() + strName + ".desktop");
    if (link.open(QFile::WriteOnly | QFile::Truncate))
    {
        const QString strVBox = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" + VBOX_GUI_VMRUNNER_IMAGE);
        QTextStream out(&link);
#ifndef VBOX_IS_QT6_OR_LATER /* defaults to UTF-8 in qt6 */
        out.setCodec("UTF-8");
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
# define QT_ENDL Qt::endl
#else
# define QT_ENDL endl
#endif
        /* Create a link which starts VirtualBox with the machine uuid. */
        out << "[Desktop Entry]" << QT_ENDL
            << "Encoding=UTF-8" << QT_ENDL
            << "Version=1.0" << QT_ENDL
            << "Name=" << strName << QT_ENDL
            << "Comment=Starts the VirtualBox machine " << strName << QT_ENDL
            << "Type=Application" << QT_ENDL
            << "Exec=" << strVBox << " --comment \"" << strName << "\" --startvm \"" << uUuid.toString() << "\"" << QT_ENDL
            << "Icon=virtualbox-vbox.png" << QT_ENDL;
        /* This would be a real file link entry, but then we could also simply
         * use a soft link (on most UNIX fs):
        out << "[Desktop Entry]" << QT_ENDL
            << "Encoding=UTF-8" << QT_ENDL
            << "Version=1.0" << QT_ENDL
            << "Name=" << strName << QT_ENDL
            << "Type=Link" << QT_ENDL
            << "Icon=virtualbox-vbox.png" << QT_ENDL
        */
        link.setPermissions(link.permissions() | QFile::ExeOwner);
        /** @todo r=bird: check status here perhaps, might've run out of disk space or
         *        some such thing... */
        return true;
    }
    return false;
}

bool UIDesktopServices::openInFileManager(const QString &strFile)
{
    QFileInfo fi(strFile);
    return QDesktopServices::openUrl(QUrl("file://" + QDir::toNativeSeparators(fi.absolutePath()), QUrl::TolerantMode));
}

