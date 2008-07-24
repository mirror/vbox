/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxGLSettingsUpdate class implementation
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

#include "VBoxGLSettingsUpdate.h"
#include "VBoxUpdateDlg.h"
#include "VBoxGlobal.h"

VBoxGLSettingsUpdate::VBoxGLSettingsUpdate()
    : mSettingsChanged (false)
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsUpdate::setupUi (this);

    /* Setup connections */
    connect (mCbCheck, SIGNAL (toggled (bool)),
             this, SLOT (toggleUpdater (bool)));
    connect (mCbOncePer, SIGNAL (activated (int)),
             this, SLOT (activatedPeriod (int)));

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsUpdate::getFrom (const CSystemProperties &,
                                    const VBoxGlobalSettings &)
{
    VBoxUpdateData data (vboxGlobal().virtualBox().
                         GetExtraData (VBoxDefs::GUI_UpdateDate));

    mCbCheck->setChecked (!data.isNeverCheck());
    if (mCbCheck->isChecked())
        mCbOncePer->setCurrentIndex (data.index());
    mTxDate->setText (data.date());

    mSettingsChanged = false;
}

void VBoxGLSettingsUpdate::putBackTo (CSystemProperties &,
                                      VBoxGlobalSettings &)
{
    if (!mSettingsChanged)
        return;

    int index = mCbCheck->isChecked() ? mCbOncePer->currentIndex() :
                                        VBoxUpdateData::NeverCheck;
    Assert (index != -1);

    VBoxUpdateData newData (index);
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
                                            newData.data());
}

void VBoxGLSettingsUpdate::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mCbCheck);
    setTabOrder (mCbCheck, mCbOncePer);
}

void VBoxGLSettingsUpdate::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxGLSettingsUpdate::retranslateUi (this);

    /* Retranslate mCbOncePer combobox */
    int ci = mCbOncePer->currentIndex();
    mCbOncePer->clear();
    VBoxUpdateData::populate();
    mCbOncePer->insertItems (0, VBoxUpdateData::list());
    mCbOncePer->setCurrentIndex (ci == -1 ? 0 : ci);
}

void VBoxGLSettingsUpdate::toggleUpdater (bool aOn)
{
    /* Enable/disable the sub widget */
    mLbOncePer->setEnabled (aOn);
    mCbOncePer->setEnabled (aOn);
    mLbDate->setEnabled (aOn);
    mTxDate->setEnabled (aOn);

    /* Update 'check for new version' time */
    if (aOn)
        activatedPeriod (mCbOncePer->currentIndex());
    else
        activatedPeriod (VBoxUpdateData::NeverCheck);
}

void VBoxGLSettingsUpdate::activatedPeriod (int aIndex)
{
    VBoxUpdateData data (aIndex);
    mTxDate->setText (data.date());
    mSettingsChanged = true;
}

void VBoxGLSettingsUpdate::showEvent (QShowEvent *aEvent)
{
    VBoxSettingsPage::showEvent (aEvent);

    /* That little hack allows avoid one of qt4 children focusing bug */
    QWidget *current = QApplication::focusWidget();
    mCbOncePer->setFocus (Qt::TabFocusReason);
    if (current)
        current->setFocus (Qt::TabFocusReason);
}

