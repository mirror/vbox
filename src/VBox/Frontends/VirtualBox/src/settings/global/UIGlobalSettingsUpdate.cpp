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

/* Local includes */
#include "UIGlobalSettingsUpdate.h"
#include "VBoxGlobal.h"

/* Update page constructor: */
UIGlobalSettingsUpdate::UIGlobalSettingsUpdate()
    : m_pLastChosenRadio(0)
    , m_fChanged(false)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsUpdate::setupUi(this);

    /* Setup connections: */
    connect(m_pEnableUpdateCheckbox, SIGNAL(toggled(bool)), this, SLOT(sltUpdaterToggled(bool)));
    connect(m_pUpdatePeriodCombo, SIGNAL(activated(int)), this, SLOT(sltPeriodActivated()));
    connect(m_pUpdateFilterStableRadio, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));
    connect(m_pUpdateFilterEveryRadio, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));
    connect(m_pUpdateFilterBetasRadio, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsUpdate::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Fill internal variables with corresponding values: */
    VBoxUpdateData updateData(vboxGlobal().virtualBox().GetExtraData(GUI_UpdateDate));
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
    m_pEnableUpdateCheckbox->setChecked(m_cache.m_fCheckEnabled);
    if (m_pEnableUpdateCheckbox->isChecked())
    {
        m_pUpdatePeriodCombo->setCurrentIndex(m_cache.m_periodIndex);
        if (m_cache.m_branchIndex == VBoxUpdateData::BranchWithBetas)
            m_pUpdateFilterBetasRadio->setChecked(true);
        else if (m_cache.m_branchIndex == VBoxUpdateData::BranchAllRelease)
            m_pUpdateFilterEveryRadio->setChecked(true);
        else
            m_pUpdateFilterStableRadio->setChecked(true);
    }
    m_pUpdateDateText->setText(m_cache.m_strDate);

    /* Reset settings altering flag: */
    m_fChanged = false;
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
    if (!m_fChanged)
        return;

    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Gather corresponding values from internal variables: */
    VBoxUpdateData newData(m_cache.m_periodIndex, m_cache.m_branchIndex);
    vboxGlobal().virtualBox().SetExtraData(GUI_UpdateDate, newData.data());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pEnableUpdateCheckbox);
    setTabOrder(m_pEnableUpdateCheckbox, m_pUpdatePeriodCombo);
    setTabOrder(m_pUpdatePeriodCombo, m_pUpdateFilterStableRadio);
    setTabOrder(m_pUpdateFilterStableRadio, m_pUpdateFilterEveryRadio);
    setTabOrder(m_pUpdateFilterEveryRadio, m_pUpdateFilterBetasRadio);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsUpdate::retranslateUi(this);

    /* Retranslate m_pUpdatePeriodCombo combobox: */
    int iCurrenIndex = m_pUpdatePeriodCombo->currentIndex();
    m_pUpdatePeriodCombo->clear();
    VBoxUpdateData::populate();
    m_pUpdatePeriodCombo->insertItems (0, VBoxUpdateData::list());
    m_pUpdatePeriodCombo->setCurrentIndex(iCurrenIndex == -1 ? 0 : iCurrenIndex);
}

void UIGlobalSettingsUpdate::sltUpdaterToggled(bool fEnabled)
{
    /* Enable/disable the sub-widgets depending on activity status: */
    m_pUpdatePeriodLabel->setEnabled(fEnabled);
    m_pUpdatePeriodCombo->setEnabled(fEnabled);
    m_pUpdateDateLabel->setEnabled(fEnabled);
    m_pUpdateDateText->setEnabled(fEnabled);
    m_pUpdateFilterLabel->setEnabled(fEnabled);
    m_pUpdateFilterStableRadio->setEnabled(fEnabled);
    m_pUpdateFilterEveryRadio->setEnabled(fEnabled);
    m_pUpdateFilterBetasRadio->setEnabled(fEnabled);
    m_pUpdateFilterStableRadio->setAutoExclusive(fEnabled);
    m_pUpdateFilterEveryRadio->setAutoExclusive(fEnabled);
    m_pUpdateFilterBetasRadio->setAutoExclusive(fEnabled);

    /* Update time of next check: */
    sltPeriodActivated();

    /* Temporary remember branch type if was switched off: */
    if (!fEnabled)
    {
        m_pLastChosenRadio = m_pUpdateFilterBetasRadio->isChecked() ? m_pUpdateFilterBetasRadio :
                             m_pUpdateFilterEveryRadio->isChecked() ? m_pUpdateFilterEveryRadio : m_pUpdateFilterStableRadio;
    }

    /* Check/uncheck last selected radio depending on activity status: */
    if (m_pLastChosenRadio)
        m_pLastChosenRadio->setChecked(fEnabled);
}

void UIGlobalSettingsUpdate::sltPeriodActivated()
{
    VBoxUpdateData data(periodType(), branchType());
    m_pUpdateDateText->setText(data.date());
    m_fChanged = true;
}

void UIGlobalSettingsUpdate::sltBranchToggled()
{
    m_fChanged = true;
}

VBoxUpdateData::PeriodType UIGlobalSettingsUpdate::periodType() const
{
    VBoxUpdateData::PeriodType result = m_pEnableUpdateCheckbox->isChecked() ?
        (VBoxUpdateData::PeriodType)m_pUpdatePeriodCombo->currentIndex() : VBoxUpdateData::PeriodNever;
    return result == VBoxUpdateData::PeriodUndefined ? VBoxUpdateData::Period1Day : result;
}

VBoxUpdateData::BranchType UIGlobalSettingsUpdate::branchType() const
{
    if (m_pUpdateFilterBetasRadio->isChecked())
        return VBoxUpdateData::BranchWithBetas;
    else if (m_pUpdateFilterEveryRadio->isChecked())
        return VBoxUpdateData::BranchAllRelease;
    else
        return VBoxUpdateData::BranchStable;
}

