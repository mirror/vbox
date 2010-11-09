/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsUpdate class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "UIGlobalSettingsUpdate.h"
#include "VBoxGlobal.h"

UIGlobalSettingsUpdate::UIGlobalSettingsUpdate()
    : mSettingsChanged(false)
    , mLastChosen(0)
{
    /* Apply UI decorations */
    Ui::UIGlobalSettingsUpdate::setupUi (this);

    /* Setup connections */
    connect (mCbCheck, SIGNAL (toggled (bool)), this, SLOT (toggleUpdater (bool)));
    connect (mCbOncePer, SIGNAL (activated (int)), this, SLOT (activatedPeriod (int)));
    connect (mRbStable, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));
    connect (mRbAllRelease, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));
    connect (mRbWithBetas, SIGNAL (toggled (bool)), this, SLOT (toggledBranch()));

    /* Applying language settings */
    retranslateUi();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsUpdate::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Fill internal variables with corresponding values: */
    VBoxUpdateData updateData(vboxGlobal().virtualBox().GetExtraData(VBoxDefs::GUI_UpdateDate));
    m_cache.m_fCheckEnabled = !updateData.isNoNeedToCheck();
    m_cache.m_periodIndex = updateData.periodIndex();
    m_cache.m_branchIndex = updateData.branchIndex();
    m_cache.m_strDate = updateData.date();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsUpdate::getFromCache()
{
    /* Apply internal variables data to QWidget(s): */
    mCbCheck->setChecked(m_cache.m_fCheckEnabled);
    if (mCbCheck->isChecked())
    {
        mCbOncePer->setCurrentIndex(m_cache.m_periodIndex);
        if (m_cache.m_branchIndex == VBoxUpdateData::BranchWithBetas)
            mRbWithBetas->setChecked(true);
        else if (m_cache.m_branchIndex == VBoxUpdateData::BranchAllRelease)
            mRbAllRelease->setChecked(true);
        else
            mRbStable->setChecked(true);
    }
    mTxDate->setText(m_cache.m_strDate);

    /* Reset settings altering flag: */
    mSettingsChanged = false;
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsUpdate::putToCache()
{
    /* Gather internal variables data from QWidget(s): */
    m_cache.m_periodIndex = periodType();
    m_cache.m_branchIndex = branchType();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsUpdate::saveFromCacheTo(QVariant &data)
{
    /* Test settings altering flag: */
    if (!mSettingsChanged)
        return;

    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Gather corresponding values from internal variables: */
    VBoxUpdateData newData(m_cache.m_periodIndex, m_cache.m_branchIndex);
    vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_UpdateDate, newData.data());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mCbCheck);
    setTabOrder (mCbCheck, mCbOncePer);
    setTabOrder (mCbOncePer, mRbStable);
    setTabOrder (mRbStable, mRbAllRelease);
    setTabOrder (mRbAllRelease, mRbWithBetas);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIGlobalSettingsUpdate::retranslateUi (this);

    /* Retranslate mCbOncePer combobox */
    int ci = mCbOncePer->currentIndex();
    mCbOncePer->clear();
    VBoxUpdateData::populate();
    mCbOncePer->insertItems (0, VBoxUpdateData::list());
    mCbOncePer->setCurrentIndex (ci == -1 ? 0 : ci);
}

void UIGlobalSettingsUpdate::toggleUpdater (bool aOn)
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

void UIGlobalSettingsUpdate::activatedPeriod (int /*aIndex*/)
{
    VBoxUpdateData data (periodType(), branchType());
    mTxDate->setText (data.date());
    mSettingsChanged = true;
}

void UIGlobalSettingsUpdate::toggledBranch()
{
    mSettingsChanged = true;
}

void UIGlobalSettingsUpdate::showEvent (QShowEvent *aEvent)
{
    UISettingsPage::showEvent (aEvent);

    /* That little hack allows avoid one of qt4 children focusing bug */
    QWidget *current = QApplication::focusWidget();
    mCbOncePer->setFocus (Qt::TabFocusReason);
    if (current)
        current->setFocus (Qt::TabFocusReason);
}

VBoxUpdateData::PeriodType UIGlobalSettingsUpdate::periodType() const
{
    VBoxUpdateData::PeriodType result = mCbCheck->isChecked() ?
        (VBoxUpdateData::PeriodType) mCbOncePer->currentIndex() : VBoxUpdateData::PeriodNever;
    return result == VBoxUpdateData::PeriodUndefined ? VBoxUpdateData::Period1Day : result;
}

VBoxUpdateData::BranchType UIGlobalSettingsUpdate::branchType() const
{
    if (mRbWithBetas->isChecked())
        return VBoxUpdateData::BranchWithBetas;
    else if (mRbAllRelease->isChecked())
        return VBoxUpdateData::BranchAllRelease;
    else
        return VBoxUpdateData::BranchStable;
}

