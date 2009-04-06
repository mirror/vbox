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

/* Qt includes */
#include <QPainter>
#include <QEvent>

VBoxAboutDlg::VBoxAboutDlg (QWidget* aParent, const QString &aVersion)
    : QIWithRetranslateUI2 <QIDialog> (aParent, Qt::CustomizeWindowHint |
                                       Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    mVersion (aVersion),
    mBgImage (":/about.png")
{
    retranslateUi();
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
    QString versionText = tr ("Version %1");
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

