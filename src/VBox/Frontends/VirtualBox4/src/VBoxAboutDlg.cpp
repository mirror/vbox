/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxAboutDlg class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

VBoxAboutDlg::VBoxAboutDlg (QWidget *aParent, const QString &aVersion)
    : QIWithRetranslateUI<QDialog> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxAboutDlg::setupUi (this);

    setFixedSize (width(), height());

    /* Fill the text background with brush */
    QPalette palette = mTextContainer->palette();
    palette.setBrush (QPalette::Window, QBrush (QPixmap (":/about_tile.png")));
    mTextContainer->setPalette (palette);

    /* Change the text color to white */
    palette = mAboutLabel->palette();
    palette.setColor (QPalette::WindowText, Qt::white);
    mAboutLabel->setPalette (palette);

    /* This is intentionally untranslatable (only Latin-1 here!) */
    const char *Copyright = "&copy; 2004&mdash;2008 Sun Microsystems, Inc.";
    QString tpl = mAboutLabel->text();
    mAboutLabel->setText (tpl.arg (aVersion).arg (Copyright));
}

void VBoxAboutDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxAboutDlg::retranslateUi (this);
}

void VBoxAboutDlg::mouseReleaseEvent (QMouseEvent*)
{
    /* Close the dialog on mouse button release */
    accept();
}

