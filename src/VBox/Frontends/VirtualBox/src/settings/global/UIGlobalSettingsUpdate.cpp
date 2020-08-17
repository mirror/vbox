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
    : m_pLastChosenRadio(0)
    , m_pCache(0)
    , m_pUpdateDateText(0)
    , m_pUpdateDateLabel(0)
    , m_pUpdatePeriodLabel(0)
    , m_pUpdateFilterLabel(0)
    , m_pCheckBoxUpdate(0)
    , m_pComboBoxUpdatePeriod(0)
    , m_pRadioUpdateFilterBetas(0)
    , m_pRadioUpdateFilterEvery(0)
    , m_pRadioUpdateFilterStable(0)
    , m_pContainerUpdate(0)
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
        m_pComboBoxUpdatePeriod->setCurrentIndex(oldUpdateData.m_periodIndex);
        if (oldUpdateData.m_branchIndex == VBoxUpdateData::BranchWithBetas)
            m_pRadioUpdateFilterBetas->setChecked(true);
        else if (oldUpdateData.m_branchIndex == VBoxUpdateData::BranchAllRelease)
            m_pRadioUpdateFilterEvery->setChecked(true);
        else
            m_pRadioUpdateFilterStable->setChecked(true);
    }
    m_pUpdateDateText->setText(oldUpdateData.m_strDate);
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
    setTabOrder(m_pCheckBoxUpdate, m_pComboBoxUpdatePeriod);
    setTabOrder(m_pComboBoxUpdatePeriod, m_pRadioUpdateFilterStable);
    setTabOrder(m_pRadioUpdateFilterStable, m_pRadioUpdateFilterEvery);
    setTabOrder(m_pRadioUpdateFilterEvery, m_pRadioUpdateFilterBetas);
}

void UIGlobalSettingsUpdate::retranslateUi()
{
    m_pCheckBoxUpdate->setWhatsThis(tr("When checked, the application will "
                                       "periodically connect to the VirtualBox website and check whether a "
                                       "new VirtualBox version is available."));
    m_pCheckBoxUpdate->setText(tr("&Check for Updates"));
    m_pUpdatePeriodLabel->setText(tr("&Once per:"));
    m_pComboBoxUpdatePeriod->setWhatsThis(tr("Selects how often the new version "
                                             "check should be performed. Note that if you want to completely "
                                             "disable this check, just clear the above check box."));
    m_pUpdateDateLabel->setText(tr("Next Check:"));
    m_pUpdateFilterLabel->setText(tr("Check for:"));
    m_pRadioUpdateFilterStable->setWhatsThis(tr("<p>Choose this if you only wish to "
                                                "be notified about stable updates to VirtualBox.</p>"));
    m_pRadioUpdateFilterStable->setText(tr("&Stable Release Versions"));
    m_pRadioUpdateFilterEvery->setWhatsThis(tr("<p>Choose this if you wish to be "
                                               "notified about all new VirtualBox releases.</p>"));
    m_pRadioUpdateFilterEvery->setText(tr("&All New Releases"));
    m_pRadioUpdateFilterBetas->setWhatsThis(tr("<p>Choose this to be notified about "
                                               "all new VirtualBox releases and pre-release versions of VirtualBox.</p>"));
    m_pRadioUpdateFilterBetas->setText(tr("All New Releases and &Pre-Releases"));

    /* Retranslate m_pComboBoxUpdatePeriod combobox: */
    int iCurrenIndex = m_pComboBoxUpdatePeriod->currentIndex();
    m_pComboBoxUpdatePeriod->clear();
    VBoxUpdateData::populate();
    m_pComboBoxUpdatePeriod->insertItems(0, VBoxUpdateData::list());
    m_pComboBoxUpdatePeriod->setCurrentIndex(iCurrenIndex == -1 ? 0 : iCurrenIndex);
}

void UIGlobalSettingsUpdate::sltHandleUpdateToggle(bool fEnabled)
{
    /* Update activity status: */
    m_pContainerUpdate->setEnabled(fEnabled);

    /* Update time of next check: */
    sltHandleUpdatePeriodChange();

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

void UIGlobalSettingsUpdate::sltHandleUpdatePeriodChange()
{
    const VBoxUpdateData data(periodType(), branchType());
    m_pUpdateDateText->setText(data.date());
}

void UIGlobalSettingsUpdate::prepare()
{
    prepareWidgets();

    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalUpdate;
    AssertPtrReturnVoid(m_pCache);

    /* Layout/widgets created in the .ui file. */
    AssertPtrReturnVoid(m_pCheckBoxUpdate);
    AssertPtrReturnVoid(m_pComboBoxUpdatePeriod);
    {
        /* Configure widgets: */
        connect(m_pCheckBoxUpdate, &QCheckBox::toggled, this, &UIGlobalSettingsUpdate::sltHandleUpdateToggle);
        connect(m_pComboBoxUpdatePeriod, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
                this, &UIGlobalSettingsUpdate::sltHandleUpdatePeriodChange);
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsUpdate::prepareWidgets()
{
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("UIGlobalSettingsUpdate"));
    QGridLayout *pMainLayout = new QGridLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);
    pMainLayout->setObjectName(QStringLiteral("gridLayout"));
    m_pCheckBoxUpdate = new QCheckBox();
    m_pCheckBoxUpdate->setObjectName(QStringLiteral("m_pCheckBoxUpdate"));
    pMainLayout->addWidget(m_pCheckBoxUpdate, 0, 0, 1, 2);

    QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    pMainLayout->addItem(pSpacerItem, 1, 0, 1, 1);

    m_pContainerUpdate = new QWidget();
    m_pContainerUpdate->setObjectName(QStringLiteral("m_pContainerUpdate"));
    QGridLayout *pContainerLayoutUpdate = new QGridLayout(m_pContainerUpdate);
    pContainerLayoutUpdate->setContentsMargins(0, 0, 0, 0);
    pContainerLayoutUpdate->setObjectName(QStringLiteral("pContainerLayoutUpdate"));
    pContainerLayoutUpdate->setContentsMargins(0, 0, 0, 0);
    m_pUpdatePeriodLabel = new QLabel(m_pContainerUpdate);
    m_pUpdatePeriodLabel->setObjectName(QStringLiteral("m_pUpdatePeriodLabel"));
    m_pUpdatePeriodLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutUpdate->addWidget(m_pUpdatePeriodLabel, 0, 0, 1, 1);

    QHBoxLayout *pHBoxLayout = new QHBoxLayout();
    pHBoxLayout->setObjectName(QStringLiteral("pHBoxLayout"));
    m_pComboBoxUpdatePeriod = new QComboBox(m_pContainerUpdate);
    m_pComboBoxUpdatePeriod->setObjectName(QStringLiteral("m_pComboBoxUpdatePeriod"));
    QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_pComboBoxUpdatePeriod->sizePolicy().hasHeightForWidth());
    m_pComboBoxUpdatePeriod->setSizePolicy(sizePolicy);
    pHBoxLayout->addWidget(m_pComboBoxUpdatePeriod);

    QSpacerItem *pSpacerItem1 = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    pHBoxLayout->addItem(pSpacerItem1);

    pContainerLayoutUpdate->addLayout(pHBoxLayout, 0, 1, 1, 1);

    m_pUpdateDateLabel = new QLabel(m_pContainerUpdate);
    m_pUpdateDateLabel->setObjectName(QStringLiteral("m_pUpdateDateLabel"));
    m_pUpdateDateLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutUpdate->addWidget(m_pUpdateDateLabel, 1, 0, 1, 1);

    m_pUpdateDateText = new QLabel(m_pContainerUpdate);
    m_pUpdateDateText->setObjectName(QStringLiteral("m_pUpdateDateText"));
    pContainerLayoutUpdate->addWidget(m_pUpdateDateText, 1, 1, 1, 1);

    m_pUpdateFilterLabel = new QLabel(m_pContainerUpdate);
    m_pUpdateFilterLabel->setObjectName(QStringLiteral("m_pUpdateFilterLabel"));
    m_pUpdateFilterLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
    pContainerLayoutUpdate->addWidget(m_pUpdateFilterLabel, 2, 0, 1, 1);

    m_pRadioUpdateFilterStable = new QRadioButton(m_pContainerUpdate);
    m_pRadioUpdateFilterStable->setObjectName(QStringLiteral("m_pRadioUpdateFilterStable"));
    pContainerLayoutUpdate->addWidget(m_pRadioUpdateFilterStable, 2, 1, 1, 1);

    m_pRadioUpdateFilterEvery = new QRadioButton(m_pContainerUpdate);
    m_pRadioUpdateFilterEvery->setObjectName(QStringLiteral("m_pRadioUpdateFilterEvery"));
    pContainerLayoutUpdate->addWidget(m_pRadioUpdateFilterEvery, 3, 1, 1, 1);

    m_pRadioUpdateFilterBetas = new QRadioButton(m_pContainerUpdate);
    m_pRadioUpdateFilterBetas->setObjectName(QStringLiteral("m_pRadioUpdateFilterBetas"));
    pContainerLayoutUpdate->addWidget(m_pRadioUpdateFilterBetas, 4, 1, 1, 1);

    pMainLayout->addWidget(m_pContainerUpdate, 1, 1, 1, 1);

    QSpacerItem *pSpacerItem2 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    pMainLayout->addItem(pSpacerItem2, 2, 0, 1, 2);

    m_pUpdatePeriodLabel->setBuddy(m_pComboBoxUpdatePeriod);
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
