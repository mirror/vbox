//Added by qt3to4:
#include <q3mimefactory.h>
/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Snapshot details" dialog UI include (Qt Designer)
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
** If you wish to add, delete or rename functions or slots use
** Qt Designer which will update this file, preserving your code. Create an
** init() function in place of a constructor, and a destroy() function in
** place of a destructor.
*****************************************************************************/


void VBoxSnapshotDetailsDlg::init()
{
    setIcon (QPixmap (":/settings_16px.png"));

    txeSummary->setPaper (this->backgroundBrush());
    txeSummary->setLinkUnderline (false);

    // filter out Enter keys in order to direct them to the default dlg button
    QIKeyFilter *ef = new QIKeyFilter (this, Qt::Key_Enter);
    ef->watchOn (txeSummary);

    txeSummary->setFocus();
}

void VBoxSnapshotDetailsDlg::getFromSnapshot (const CSnapshot &s)
{
    csnapshot = s;
    CMachine machine = s.GetMachine();

    // get properties
    leName->setText (s.GetName());
    txeDescription->setText (s.GetDescription());

    // compose summary
    txeSummary->setText (
        vboxGlobal().detailsReport (machine, false /* isNewVM */,
                                             false /* withLinks */));
    setCaption (tr ("Details of %1 (%2)")
                .arg (csnapshot.GetName()).arg (machine.GetName()));
}

void VBoxSnapshotDetailsDlg::putBackToSnapshot()
{
    AssertReturn (!csnapshot.isNull(), (void) 0);

    csnapshot.SetName (leName->text());
    csnapshot.SetDescription (txeDescription->text());
}

void VBoxSnapshotDetailsDlg::leName_textChanged (const QString &aText)
{
    buttonOk->setEnabled (!aText.stripWhiteSpace().isEmpty());
}

