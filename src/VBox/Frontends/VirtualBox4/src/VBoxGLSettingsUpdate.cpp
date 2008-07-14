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
    : mLastSelected (0)
    , mSettingsChanged (false)
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsUpdate::setupUi (this);

    /* Setup connections */
    connect (mGbCheck, SIGNAL (toggled (bool)),
             this, SLOT (toggleUpdater (bool)));
    connect (mRbAuto, SIGNAL (clicked()), this, SLOT (toggleType()));
    connect (mRbOncePer, SIGNAL (clicked()), this, SLOT (toggleType()));
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

    if (data.index() == VBoxUpdateData::NeverCheck)
    {
        mGbCheck->setChecked (false);
    }
    else
    {
        mGbCheck->setChecked (true);

        if (data.isAutomatic())
        {
            mRbAuto->setChecked (true);
        }
        else
        {
            mRbOncePer->setChecked (true);
            Assert (data.index() >= 0);
            mCbOncePer->setCurrentIndex (data.index());
        }
    }
    mTxDate->setText (data.date());

    mSettingsChanged = false;
}

void VBoxGLSettingsUpdate::putBackTo (CSystemProperties &,
                                      VBoxGlobalSettings &)
{
    if (!mSettingsChanged)
        return;

    int index = -1;

    if (!mGbCheck->isChecked())
    {
        index = VBoxUpdateData::NeverCheck;
    }
    else
    {
        if (mRbAuto->isChecked())
            index = VBoxUpdateData::AutoCheck;
        else
            index = mCbOncePer->currentIndex();
    }
    Assert (index != -1);

    VBoxUpdateData newData (index);

    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate,
                                            newData.data());
}

void VBoxGLSettingsUpdate::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mGbCheck);
    setTabOrder (mGbCheck, mRbAuto);
    setTabOrder (mRbAuto, mRbOncePer);
    setTabOrder (mRbOncePer, mCbOncePer);
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
    /* Toggle auto-exclusiveness on/off to let the buttons be
     * unchecked both in case of group-box is not checked and
     * exclusively checked in case of group-box is checked. */
    mRbAuto->setAutoExclusive (aOn);
    mRbOncePer->setAutoExclusive (aOn);

    /* Enable/disable the sub widget */
    mGbCheckContent->setEnabled (aOn);

    /* Toggle both buttons off when the group box unchecked. */
    if (!aOn)
    {
        mLastSelected = mRbOncePer->isChecked() ? mRbOncePer : mRbAuto;
        mRbAuto->blockSignals (true);
        mRbOncePer->blockSignals (true);
        mRbAuto->setChecked (false);
        mRbOncePer->setChecked (false);
        mRbAuto->blockSignals (false);
        mRbOncePer->blockSignals (false);
        activatedPeriod (VBoxUpdateData::NeverCheck);
    }
    /* Toggle last checked button on when the group box checked. */
    else if (!mRbAuto->isChecked() && !mRbOncePer->isChecked())
    {
        mLastSelected->blockSignals (true);
        mLastSelected->setChecked (true);
        mLastSelected->blockSignals (false);
        toggleType();
    }
}

void VBoxGLSettingsUpdate::toggleType()
{
    mCbOncePer->setEnabled (mRbOncePer->isChecked());
    activatedPeriod (mRbAuto->isChecked() ?
        VBoxUpdateData::AutoCheck : mCbOncePer->currentIndex());
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

