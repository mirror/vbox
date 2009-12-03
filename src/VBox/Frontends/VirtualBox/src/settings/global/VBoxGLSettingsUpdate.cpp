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
#include "VBoxGlobal.h"

VBoxGLSettingsUpdate::VBoxGLSettingsUpdate()
    : mSettingsChanged (false)
    , mLastChosen (0)
{
    /* Apply UI decorations */
    Ui::VBoxGLSettingsUpdate::setupUi (this);

    /* Setup connections */
    connect (mCbCheck, SIGNAL (toggled (bool)), this, SLOT (toggleUpdater (bool)));
    connect (mCbOncePer, SIGNAL (activated (int)), this, SLOT (activatedPeriod (int)));
    connect (mRbStable, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));
    connect (mRbAllRelease, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));
    connect (mRbWithBetas, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));

    /* Applying language settings */
    retranslateUi();
}

void VBoxGLSettingsUpdate::getFrom (const CSystemProperties &, const VBoxGlobalSettings &)
{
    VBoxUpdateData data (vboxGlobal().virtualBox().GetExtraData (VBoxDefs::GUI_UpdateDate));

    mCbCheck->setChecked (!data.isNoNeedToCheck());
    if (mCbCheck->isChecked())
    {
        mCbOncePer->setCurrentIndex (data.periodIndex());
        if (data.branchIndex() == VBoxUpdateData::BranchWithBetas)
            mRbWithBetas->setChecked (true);
        else if (data.branchIndex() == VBoxUpdateData::BranchAllRelease)
            mRbAllRelease->setChecked (true);
        else
            mRbStable->setChecked (true);
    }
    mTxDate->setText (data.date());

    mSettingsChanged = false;
}

void VBoxGLSettingsUpdate::putBackTo (CSystemProperties &, VBoxGlobalSettings &)
{
    if (!mSettingsChanged)
        return;

    VBoxUpdateData newData (periodType(), branchType());
    vboxGlobal().virtualBox().SetExtraData (VBoxDefs::GUI_UpdateDate, newData.data());
}

void VBoxGLSettingsUpdate::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mCbCheck);
    setTabOrder (mCbCheck, mCbOncePer);
    setTabOrder (mCbOncePer, mRbStable);
    setTabOrder (mRbStable, mRbAllRelease);
    setTabOrder (mRbAllRelease, mRbWithBetas);
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
    mLbFilter->setEnabled (aOn);
    mRbStable->setEnabled (aOn);
    mRbAllRelease->setEnabled (aOn);
    mRbWithBetas->setEnabled (aOn);
    mRbStable->setAutoExclusive (aOn);
    mRbAllRelease->setAutoExclusive (aOn);
    mRbWithBetas->setAutoExclusive (aOn);

    /* Update 'check for new version' time */
    if (aOn)
        activatedPeriod (mCbOncePer->currentIndex());
    else
    {
        activatedPeriod (VBoxUpdateData::PeriodNever);
        mLastChosen = mRbWithBetas->isChecked() ? mRbWithBetas :
                      mRbAllRelease->isChecked() ? mRbAllRelease : mRbStable;
    }

    if (mLastChosen)
        mLastChosen->setChecked (aOn);
}

void VBoxGLSettingsUpdate::activatedPeriod (int /*aIndex*/)
{
    VBoxUpdateData data (periodType(), branchType());
    mTxDate->setText (data.date());
    mSettingsChanged = true;
}

void VBoxGLSettingsUpdate::toggledBranch()
{
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

VBoxUpdateData::PeriodType VBoxGLSettingsUpdate::periodType() const
{
    VBoxUpdateData::PeriodType result = mCbCheck->isChecked() ?
        (VBoxUpdateData::PeriodType) mCbOncePer->currentIndex() : VBoxUpdateData::PeriodNever;
    return result == VBoxUpdateData::PeriodUndefined ? VBoxUpdateData::Period1Day : result;
}

VBoxUpdateData::BranchType VBoxGLSettingsUpdate::branchType() const
{
    if (mRbWithBetas->isChecked())
        return VBoxUpdateData::BranchWithBetas;
    else if (mRbAllRelease->isChecked())
        return VBoxUpdateData::BranchAllRelease;
    else
        return VBoxUpdateData::BranchStable;
}

