/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxAboutDlg class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "VBoxAboutDlg.h"
#include "VBoxGlobal.h"

#include <iprt/path.h>

/* Qt includes */
#include <QDir>
#include <QEvent>
#include <QPainter>

VBoxAboutDlg::VBoxAboutDlg (QWidget* aParent, const QString &aVersion)
    : QIWithRetranslateUI2 <QIDialog> (aParent, Qt::CustomizeWindowHint |
                                       Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    mVersion (aVersion)
{
    retranslateUi();

    QString sPath (":/about.png");
    /* Branding: Use a custom about splash picture if set */
    QString sSplash = vboxGlobal().brandingGetKey ("UI/AboutSplash");
    if (vboxGlobal().brandingIsActive() && !sSplash.isEmpty())
    {
        char szExecPath[PATH_MAX];
        RTPathExecDir (szExecPath, PATH_MAX);
        QString tmpPath = QString ("%1/%2").arg (szExecPath).arg (sSplash);
        if (QFile::exists (tmpPath))
            sPath = tmpPath;
    }

    mBgImage.load (sPath);
}

bool VBoxAboutDlg::event (QEvent *aEvent)
{
    if (aEvent->type() == QEvent::Polish)
        setFixedSize (mBgImage.size());
    return QIDialog::event (aEvent);
}

void VBoxAboutDlg::retranslateUi()
{
    setWindowTitle (tr ("VirtualBox - About"));
    QString aboutText =  tr ("VirtualBox Graphical User Interface");
#ifdef VBOX_BLEEDING_EDGE
    QString versionText = "EXPERIMENTAL build %1 - " + QString(VBOX_BLEEDING_EDGE);
#else
    QString versionText = tr ("Version %1");
#endif
#if VBOX_OSE
    mAboutText = aboutText + " " + versionText.arg (mVersion) + "\n" +
                 QString ("%1 2004-2009 Sun Microsystems, Inc.").arg (QChar (0xa9));
#else /* VBOX_OSE */
    mAboutText = aboutText + "\n" +
                 versionText.arg (mVersion);
#endif /* VBOX_OSE */
}

void VBoxAboutDlg::paintEvent (QPaintEvent * /* aEvent */)
{
    QPainter painter (this);
    painter.drawPixmap (0, 0, mBgImage);
    painter.setFont (font());

    /* Branding: Set a different text color (because splash also could be white),
                 otherwise use white as default color */
    QString sColor = vboxGlobal().brandingGetKey("UI/AboutTextColor");
    if (!sColor.isEmpty())
        painter.setPen (QColor(sColor).name());
    else
        painter.setPen (Qt::white);
#if VBOX_OSE
    painter.drawText (QRect (0, 400, 600, 32),
                      Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                      mAboutText);
#else /* VBOX_OSE */
    painter.drawText (QRect (313, 370, 300, 72),
                      Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
                      mAboutText);
#endif /* VBOX_OSE */
}

void VBoxAboutDlg::mouseReleaseEvent (QMouseEvent * /* aEvent */)
{
    /* close the dialog on mouse button release */
    accept();
}

