/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * About Dialog (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void VBoxAboutDlg::setup (const QString &aVersion)
{
    /* This is intentionally untranslatable (oly Latin-1 here!) */
    const char *Copyright = "&copy; 2004&mdash;2008 innotek GmbH";

    QString tpl = mAboutLabel->text();
    mAboutLabel->setText (tpl
                          .arg (aVersion)
                          .arg (Copyright));
}

void VBoxAboutDlg::mouseReleaseEvent (QMouseEvent *aE )
{
    Q_UNUSED (aE);

    /* close the dialog on mouse button release */
    accept();
}
