/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIGlobalSettingsUpdate.h"
# include "UIExtraDataManager.h"
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGlobalSettingsUpdate::UIGlobalSettingsUpdate()
    : m_pLastChosenRadio(0)
    , m_fChanged(false)
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsUpdate::setupUi(this);

    /* Setup connections: */
    connect(m_pCheckBoxUpdate, SIGNAL(toggled(bool)), this, SLOT(sltUpdaterToggled(bool)));
    connect(m_pComboBoxUpdatePeriod, SIGNAL(activated(int)), this, SLOT(sltPeriodActivated()));
    connect(m_pRadioUpdateFilterStable, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));
    connect(m_pRadioUpdateFilterEvery, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));
    connect(m_pRadioUpdateFilterBetas, SIGNAL(toggled(bool)), this, SLOT(sltBranchToggled()));

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
    VBoxUpdateData updateData(gEDataManager->applicationUpdateData());
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
    m_pCheckBoxUpdate->setChecked(m_cache.m_fCheckEnabled);
    if (m_pCheckBoxUpdate->isChecked())
    {
        m_pComboBoxUpdatePeriod->setCurrentIndex(m_cache.m_periodIndex);
        if (m_cache.m_branchIndex == VBoxUpdateData::BranchWithBetas)
            m_pRadioUpdateFilterBetas->setChecked(true);
        else if (m_cache.m_branchIndex == VBoxUpdateData::BranchAllRelease)
            m_pRadioUpdateFilterEvery->setChecked(true);
        else
            m_pRadioUpdateFilterStable->setChecked(true);
    }
    m_pUpdateDateText->setText(m_cache.m_strDate);
    sltUpdaterToggled(m_cache.m_fCheckEnabled);

    /* Mark page as *not changed*: */
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
    gEDataManager->setApplicationUpdateData(newData.data());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation: */
    setTabOrder(pWidget, m_pCheckBoxUpdate);
    setTabOrder(m_pCheckBoxUpdate, m_pComboBoxUpdatePeriod);
    setTabOrder(m_pComboBoxUpdatePeriod, m_pRadioUpdateFilterStable);
    setTabOrder(m_pRadioUpdateFilterStable, m_pRadioUpdateFilterEvery);
    setTabOrder(m_pRadioUpdateFilterEvery, m_pRadioUpdateFilterBetas);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsUpdate::retranslateUi(this);

    /* Retranslate m_pComboBoxUpdatePeriod combobox: */
    int iCurrenIndex = m_pComboBoxUpdatePeriod->currentIndex();
    m_pComboBoxUpdatePeriod->clear();
    VBoxUpdateData::populate();
    m_pComboBoxUpdatePeriod->insertItems(0, VBoxUpdateData::list());
    m_pComboBoxUpdatePeriod->setCurrentIndex(iCurrenIndex == -1 ? 0 : iCurrenIndex);
}

void UIGlobalSettingsUpdate::sltUpdaterToggled(bool fEnabled)
{
    /* Update activity status: */
    m_pContainerUpdate->setEnabled(fEnabled);

    /* Update time of next check: */
    sltPeriodActivated();

    /* Temporary remember branch type if was switched off: */
    if (!fEnabled)
    {
        m_pLastChosenRadio = m_pRadioUpdateFilterBetas->isChecked() ? m_pRadioUpdateFilterBetas :
                             m_pRadioUpdateFilterEvery->isChecked() ? m_pRadioUpdateFilterEvery : m_pRadioUpdateFilterStable;
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
    VBoxUpdateData::PeriodType result = m_pCheckBoxUpdate->isChecked() ?
        (VBoxUpdateData::PeriodType)m_pComboBoxUpdatePeriod->currentIndex() : VBoxUpdateData::PeriodNever;
    return result == VBoxUpdateData::PeriodUndefined ? VBoxUpdateData::Period1Day : result;
}

VBoxUpdateData::BranchType UIGlobalSettingsUpdate::branchType() const
{
    if (m_pRadioUpdateFilterBetas->isChecked())
        return VBoxUpdateData::BranchWithBetas;
    else if (m_pRadioUpdateFilterEvery->isChecked())
        return VBoxUpdateData::BranchAllRelease;
    else
        return VBoxUpdateData::BranchStable;
}

