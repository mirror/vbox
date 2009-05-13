/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotDetailsDlg class implementation
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

#include <VBoxSnapshotDetailsDlg.h>
#include <VBoxGlobal.h>
#include <VBoxProblemReporter.h>
#include <VBoxUtils.h>

/* Qt includes */
#include <QPushButton>

VBoxSnapshotDetailsDlg::VBoxSnapshotDetailsDlg (QWidget *aParent)
    : QIWithRetranslateUI<QDialog> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxSnapshotDetailsDlg::setupUi (this);

    /* Setup mTeSummary browser. */
    mTeSummary->viewport()->setAutoFillBackground (false);
    mTeSummary->setFocus();

    /* Setup connections */
    connect (mLeName, SIGNAL (textChanged (const QString&)),
             this, SLOT (onNameChanged (const QString&)));
    connect (mButtonBox, SIGNAL (helpRequested()),
             &vboxProblem(), SLOT (showHelpHelpDialog()));

    resize (450, 450);
}

void VBoxSnapshotDetailsDlg::getFromSnapshot (const CSnapshot &aSnapshot)
{
    mSnapshot = aSnapshot;
    CMachine machine = aSnapshot.GetMachine();

    /* Get properties */
    mLeName->setText (aSnapshot.GetName());
    mTeDescription->setText (aSnapshot.GetDescription());

    retranslateUi();
}

void VBoxSnapshotDetailsDlg::putBackToSnapshot()
{
    AssertReturn (!mSnapshot.isNull(), (void) 0);

    mSnapshot.SetName (mLeName->text());
    mSnapshot.SetDescription (mTeDescription->toPlainText());
}

void VBoxSnapshotDetailsDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSnapshotDetailsDlg::retranslateUi (this);

    if(mSnapshot.isNull())
        return;

    CMachine machine = mSnapshot.GetMachine();

    setWindowTitle (tr ("Details of %1 (%2)")
                    .arg (mSnapshot.GetName()).arg (machine.GetName()));

    /* Compose summary */
    mTeSummary->setText (
        vboxGlobal().detailsReport (machine, false /* withLinks */));
}

void VBoxSnapshotDetailsDlg::onNameChanged (const QString &aText)
{
    mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (!aText.trimmed().isEmpty());
}

