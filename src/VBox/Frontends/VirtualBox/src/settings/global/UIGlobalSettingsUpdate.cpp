/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>

/* GUI includes: */
#include "UIGlobalSettingsUpdate.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UICommon.h"


/** Global settings: Update page data structure. */
struct UIDataSettingsGlobalUpdate
{
    /** Constructs data. */
    UIDataSettingsGlobalUpdate()
        : m_fCheckEnabled(false)
        , m_periodIndex(VBoxUpdateData::PeriodUndefined)
        , m_branchIndex(VBoxUpdateData::BranchStable)
        , m_strDate(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalUpdate &other) const
    {
        return true
               && (m_fCheckEnabled == other.m_fCheckEnabled)
               && (m_periodIndex == other.m_periodIndex)
               && (m_branchIndex == other.m_branchIndex)
               && (m_strDate == other.m_strDate)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalUpdate &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalUpdate &other) const { return !equal(other); }

    /** Holds whether the update check is enabled. */
    bool m_fCheckEnabled;
    /** Holds the update check period. */
    VBoxUpdateData::PeriodType m_periodIndex;
    /** Holds the update branch type. */
    VBoxUpdateData::BranchType m_branchIndex;
    /** Holds the next update date. */
    QString m_strDate;
};


UIGlobalSettingsUpdate::UIGlobalSettingsUpdate()
    : m_pCache(0)
    , m_pCheckBoxUpdate(0)
    , m_pWidgetUpdateSettings(0)
    , m_pLabelUpdatePeriod(0)
    , m_pComboUpdatePeriod(0)
    , m_pLabelUpdateDate(0)
    , m_pFieldUpdateDate(0)
    , m_pLabelUpdateFilter(0)
    , m_pRadioUpdateFilterStable(0)
    , m_pRadioUpdateFilterEvery(0)
    , m_pRadioUpdateFilterBetas(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsUpdate::~UIGlobalSettingsUpdate()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsUpdate::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old update data: */
    UIDataSettingsGlobalUpdate oldUpdateData;

    /* Gather old update data: */
    const VBoxUpdateData updateData(gEDataManager->applicationUpdateData());
    oldUpdateData.m_fCheckEnabled = !updateData.isNoNeedToCheck();
    oldUpdateData.m_periodIndex = updateData.periodIndex();
    oldUpdateData.m_branchIndex = updateData.branchIndex();
    oldUpdateData.m_strDate = updateData.date();

    /* Cache old update data: */
    m_pCache->cacheInitialData(oldUpdateData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::getFromCache()
{
    /* Get old update data from the cache: */
    const UIDataSettingsGlobalUpdate &oldUpdateData = m_pCache->base();

    /* Load old update data from the cache: */
    m_pCheckBoxUpdate->setChecked(oldUpdateData.m_fCheckEnabled);
    if (m_pCheckBoxUpdate->isChecked())
    {
        m_pComboUpdatePeriod->setCurrentIndex(oldUpdateData.m_periodIndex);
        if (oldUpdateData.m_branchIndex == VBoxUpdateData::BranchWithBetas)
            m_pRadioUpdateFilterBetas->setChecked(true);
        else if (oldUpdateData.m_branchIndex == VBoxUpdateData::BranchAllRelease)
            m_pRadioUpdateFilterEvery->setChecked(true);
        else
            m_pRadioUpdateFilterStable->setChecked(true);
    }
    m_pFieldUpdateDate->setText(oldUpdateData.m_strDate);
    sltHandleUpdateToggle(oldUpdateData.m_fCheckEnabled);
}

void UIGlobalSettingsUpdate::putToCache()
{
    /* Prepare new update data: */
    UIDataSettingsGlobalUpdate newUpdateData = m_pCache->base();

    /* Gather new update data: */
    newUpdateData.m_periodIndex = periodType();
    newUpdateData.m_branchIndex = branchType();

    /* Cache new update data: */
    m_pCache->cacheCurrentData(newUpdateData);
}

void UIGlobalSettingsUpdate::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update update data and failing state: */
    setFailed(!saveUpdateData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsUpdate::setOrderAfter(QWidget *pWidget)
{
    /* Configure navigation: */
    setTabOrder(pWidget, m_pCheckBoxUpdate);
    setTabOrder(m_pCheckBoxUpdate, m_pComboUpdatePeriod);
    setTabOrder(m_pComboUpdatePeriod, m_pRadioUpdateFilterStable);
    setTabOrder(m_pRadioUpdateFilterStable, m_pRadioUpdateFilterEvery);
    setTabOrder(m_pRadioUpdateFilterEvery, m_pRadioUpdateFilterBetas);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
    m_pCheckBoxUpdate->setWhatsThis(tr("When checked, the application will "
                                       "periodically connect to the VirtualBox website and check whether a "
                                       "new VirtualBox version is available."));
    m_pCheckBoxUpdate->setText(tr("&Check for Updates"));
    m_pLabelUpdatePeriod->setText(tr("&Once per:"));
    m_pComboUpdatePeriod->setWhatsThis(tr("Selects how often the new version "
                                             "check should be performed. Note that if you want to completely "
                                             "disable this check, just clear the above check box."));
    m_pLabelUpdateDate->setText(tr("Next Check:"));
    m_pLabelUpdateFilter->setText(tr("Check for:"));
    m_pRadioUpdateFilterStable->setWhatsThis(tr("<p>Choose this if you only wish to "
                                                "be notified about stable updates to VirtualBox.</p>"));
    m_pRadioUpdateFilterStable->setText(tr("&Stable Release Versions"));
    m_pRadioUpdateFilterEvery->setWhatsThis(tr("<p>Choose this if you wish to be "
                                               "notified about all new VirtualBox releases.</p>"));
    m_pRadioUpdateFilterEvery->setText(tr("&All New Releases"));
    m_pRadioUpdateFilterBetas->setWhatsThis(tr("<p>Choose this to be notified about "
                                               "all new VirtualBox releases and pre-release versions of VirtualBox.</p>"));
    m_pRadioUpdateFilterBetas->setText(tr("All New Releases and &Pre-Releases"));

    /* Retranslate m_pComboUpdatePeriod combobox: */
    int iCurrenIndex = m_pComboUpdatePeriod->currentIndex();
    m_pComboUpdatePeriod->clear();
    VBoxUpdateData::populate();
    m_pComboUpdatePeriod->insertItems(0, VBoxUpdateData::list());
    m_pComboUpdatePeriod->setCurrentIndex(iCurrenIndex == -1 ? 0 : iCurrenIndex);
}

void UIGlobalSettingsUpdate::sltHandleUpdateToggle(bool fEnabled)
{
    /* Update activity status: */
    m_pWidgetUpdateSettings->setEnabled(fEnabled);

    /* Update time of next check: */
    sltHandleUpdatePeriodChange();

    /* Choose stable branch if nothing chosen: */
    if (   fEnabled
        && !m_pRadioUpdateFilterStable->isChecked()
        && !m_pRadioUpdateFilterEvery->isChecked()
        && !m_pRadioUpdateFilterBetas->isChecked())
        m_pRadioUpdateFilterStable->setChecked(true);
}

void UIGlobalSettingsUpdate::sltHandleUpdatePeriodChange()
{
    const VBoxUpdateData data(periodType(), branchType());
    m_pFieldUpdateDate->setText(data.date());
}

void UIGlobalSettingsUpdate::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalUpdate;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsUpdate::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setRowStretch(2, 1);

        /* Prepare update check-box: */
        m_pCheckBoxUpdate = new QCheckBox(this);
        if (m_pCheckBoxUpdate)
            pLayoutMain->addWidget(m_pCheckBoxUpdate, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayoutMain->addItem(pSpacerItem, 1, 0);

        /* Prepare update settings widget: */
        m_pWidgetUpdateSettings = new QWidget(this);
        if (m_pWidgetUpdateSettings)
        {
            /* Prepare update settings widget layout: */
            QGridLayout *pLayoutUpdateSettings = new QGridLayout(m_pWidgetUpdateSettings);
            if (pLayoutUpdateSettings)
            {
                pLayoutUpdateSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutUpdateSettings->setColumnStretch(2, 1);
                pLayoutUpdateSettings->setRowStretch(5, 1);

                /* Prepare update period label: */
                m_pLabelUpdatePeriod = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdatePeriod)
                {
                    m_pLabelUpdatePeriod->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdatePeriod, 0, 0);
                }
                /* Prepare update period combo: */
                m_pComboUpdatePeriod = new QComboBox(m_pWidgetUpdateSettings);
                if (m_pComboUpdatePeriod)
                {
                    if (m_pLabelUpdatePeriod)
                        m_pLabelUpdatePeriod->setBuddy(m_pComboUpdatePeriod);
                    m_pComboUpdatePeriod->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                    m_pComboUpdatePeriod->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

                    pLayoutUpdateSettings->addWidget(m_pComboUpdatePeriod, 0, 1);
                }

                /* Prepare update date label: */
                m_pLabelUpdateDate = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdateDate)
                {
                    m_pLabelUpdateDate->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdateDate, 1, 0);
                }
                /* Prepare update date field: */
                m_pFieldUpdateDate = new QLabel(m_pWidgetUpdateSettings);
                if (m_pFieldUpdateDate)
                    pLayoutUpdateSettings->addWidget(m_pFieldUpdateDate, 1, 1);

                /* Prepare update date label: */
                m_pLabelUpdateFilter = new QLabel(m_pWidgetUpdateSettings);
                if (m_pLabelUpdateFilter)
                {
                    m_pLabelUpdateFilter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutUpdateSettings->addWidget(m_pLabelUpdateFilter, 2, 0);
                }
                /* Prepare 'update to stable' radio-button: */
                m_pRadioUpdateFilterStable = new QRadioButton(m_pWidgetUpdateSettings);
                if (m_pRadioUpdateFilterStable)
                    pLayoutUpdateSettings->addWidget(m_pRadioUpdateFilterStable, 2, 1);
                /* Prepare 'update to every' radio-button: */
                m_pRadioUpdateFilterEvery = new QRadioButton(m_pWidgetUpdateSettings);
                if (m_pRadioUpdateFilterEvery)
                    pLayoutUpdateSettings->addWidget(m_pRadioUpdateFilterEvery, 3, 1);
                /* Prepare 'update to betas' radio-button: */
                m_pRadioUpdateFilterBetas = new QRadioButton(m_pWidgetUpdateSettings);
                if (m_pRadioUpdateFilterBetas)
                    pLayoutUpdateSettings->addWidget(m_pRadioUpdateFilterBetas, 4, 1);
            }

            pLayoutMain->addWidget(m_pWidgetUpdateSettings, 1, 1);
        }
    }
}

void UIGlobalSettingsUpdate::prepareConnections()
{
    connect(m_pCheckBoxUpdate, &QCheckBox::toggled, this, &UIGlobalSettingsUpdate::sltHandleUpdateToggle);
    connect(m_pComboUpdatePeriod, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &UIGlobalSettingsUpdate::sltHandleUpdatePeriodChange);
}

void UIGlobalSettingsUpdate::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

VBoxUpdateData::PeriodType UIGlobalSettingsUpdate::periodType() const
{
    const VBoxUpdateData::PeriodType result = m_pCheckBoxUpdate->isChecked() ?
        (VBoxUpdateData::PeriodType)m_pComboUpdatePeriod->currentIndex() : VBoxUpdateData::PeriodNever;
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

bool UIGlobalSettingsUpdate::saveUpdateData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save update settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old update data from the cache: */
        //const UIDataSettingsGlobalUpdate &oldUpdateData = m_pCache->base();
        /* Get new update data from the cache: */
        const UIDataSettingsGlobalUpdate &newUpdateData = m_pCache->data();

        /* Save new update data from the cache: */
        const VBoxUpdateData newData(newUpdateData.m_periodIndex, newUpdateData.m_branchIndex);
        gEDataManager->setApplicationUpdateData(newData.data());
    }
    /* Return result: */
    return fSuccess;
}
