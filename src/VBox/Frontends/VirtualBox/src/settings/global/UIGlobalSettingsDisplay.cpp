/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class implementation.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include <QSpacerItem>
#include <QSpinBox>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSettingsDisplay.h"
#include "UIMessageCenter.h"
#include "UIScaleFactorEditor.h"

/** Global settings: Display page data structure. */
struct UIDataSettingsGlobalDisplay
{
    /** Constructs data. */
    UIDataSettingsGlobalDisplay()
        : m_enmMaxGuestResolution(MaxGuestResolutionPolicy_Automatic)
        , m_maxGuestResolution(QSize())
        , m_fActivateHoveredMachineWindow(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalDisplay &other) const
    {
        return true
               && (m_enmMaxGuestResolution == other.m_enmMaxGuestResolution)
               && (m_maxGuestResolution == other.m_maxGuestResolution)
               && (m_fActivateHoveredMachineWindow == other.m_fActivateHoveredMachineWindow)
               && (m_scaleFactors == other.m_scaleFactors)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalDisplay &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalDisplay &other) const { return !equal(other); }

    /** Holds the maximum guest-screen resolution policy. */
    MaxGuestResolutionPolicy m_enmMaxGuestResolution;
    /** Holds the maximum guest-screen resolution. */
    QSize m_maxGuestResolution;
    /** Holds whether we should automatically activate machine window under the mouse cursor. */
    bool m_fActivateHoveredMachineWindow;
    /** Holds the guest screen scale-factor. */
    QList<double> m_scaleFactors;
};


UIGlobalSettingsDisplay::UIGlobalSettingsDisplay()
    : m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsDisplay::~UIGlobalSettingsDisplay()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old display data: */
    UIDataSettingsGlobalDisplay oldDisplayData;

    /* Gather old display data: */
    oldDisplayData.m_enmMaxGuestResolution = gEDataManager->maxGuestResolutionPolicy();
    if (oldDisplayData.m_enmMaxGuestResolution == MaxGuestResolutionPolicy_Fixed)
        oldDisplayData.m_maxGuestResolution = gEDataManager->maxGuestResolutionForPolicyFixed();
    oldDisplayData.m_fActivateHoveredMachineWindow = gEDataManager->activateHoveredMachineWindow();
    oldDisplayData.m_scaleFactors = gEDataManager->scaleFactors(UIExtraDataManager::GlobalID);

    /* Cache old display data: */
    m_pCache->cacheInitialData(oldDisplayData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsDisplay::getFromCache()
{
    /* Get old display data from the cache: */
    const UIDataSettingsGlobalDisplay &oldDisplayData = m_pCache->base();

    /* Load old display data from the cache: */
    m_pMaxResolutionCombo->setCurrentIndex(m_pMaxResolutionCombo->findData((int)oldDisplayData.m_enmMaxGuestResolution));
    if (oldDisplayData.m_enmMaxGuestResolution == MaxGuestResolutionPolicy_Fixed)
    {
        m_pResolutionWidthSpin->setValue(oldDisplayData.m_maxGuestResolution.width());
        m_pResolutionHeightSpin->setValue(oldDisplayData.m_maxGuestResolution.height());
    }
    m_pCheckBoxActivateOnMouseHover->setChecked(oldDisplayData.m_fActivateHoveredMachineWindow);
    m_pScaleFactorEditor->setScaleFactors(oldDisplayData.m_scaleFactors);
    m_pScaleFactorEditor->setMonitorCount(gpDesktop->screenCount());
}

void UIGlobalSettingsDisplay::putToCache()
{
    /* Prepare new display data: */
    UIDataSettingsGlobalDisplay newDisplayData = m_pCache->base();

    /* Gather new display data: */
    newDisplayData.m_enmMaxGuestResolution = (MaxGuestResolutionPolicy)m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex()).toInt();
    if (newDisplayData.m_enmMaxGuestResolution == MaxGuestResolutionPolicy_Fixed)
        newDisplayData.m_maxGuestResolution = QSize(m_pResolutionWidthSpin->value(), m_pResolutionHeightSpin->value());
    newDisplayData.m_fActivateHoveredMachineWindow = m_pCheckBoxActivateOnMouseHover->isChecked();
    newDisplayData.m_scaleFactors = m_pScaleFactorEditor->scaleFactors();

    /* Cache new display data: */
    m_pCache->cacheCurrentData(newDisplayData);
}

void UIGlobalSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update display data and failing state: */
    setFailed(!saveDisplayData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsDisplay::retranslateUi()
{
    m_pMaxResolutionLabel->setText(tr("Maximum Guest Screen &Size:"));
    m_pResolutionWidthLabel->setText(tr("&Width:"));
    m_pResolutionWidthSpin->setWhatsThis(tr("Holds the maximum width which we would like the guest to use."));
    m_pResolutionHeightLabel->setText(tr("&Height:"));
    m_pResolutionHeightSpin->setWhatsThis(tr("Holds the maximum height which we would like the guest to use."));
    m_pLabelGuestScreenScaleFactorEditor->setText(tr("Scale Factor:"));
    m_pScaleFactorEditor->setWhatsThis(tr("Controls the guest screen scale factor."));
    m_pLabelMachineWindow->setText(tr("Machine Windows:"));
    m_pCheckBoxActivateOnMouseHover->setWhatsThis(tr("When checked, machine windows will be raised when the mouse pointer moves over them."));
    m_pCheckBoxActivateOnMouseHover->setText(tr("&Raise Window Under Mouse"));

    /* Reload combo-box: */
    reloadMaximumGuestScreenSizePolicyComboBox();
}

void UIGlobalSettingsDisplay::sltHandleMaximumGuestScreenSizePolicyChange()
{
    /* Get current resolution-combo tool-tip data: */
    const QString strCurrentComboItemTip = m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex(), Qt::ToolTipRole).toString();
    m_pMaxResolutionCombo->setWhatsThis(strCurrentComboItemTip);

    /* Get current resolution-combo item data: */
    const MaxGuestResolutionPolicy enmPolicy = (MaxGuestResolutionPolicy)m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex()).toInt();
    /* Should be combo-level widgets enabled? */
    const bool fComboLevelWidgetsEnabled = enmPolicy == MaxGuestResolutionPolicy_Fixed;
    /* Enable/disable combo-level widgets: */
    m_pResolutionWidthLabel->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionWidthSpin->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionHeightLabel->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionHeightSpin->setEnabled(fComboLevelWidgetsEnabled);
}

void UIGlobalSettingsDisplay::prepare()
{
    prepareWidgets();

    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalDisplay;
    AssertPtrReturnVoid(m_pCache);

    /* Layout/widgets created in the .ui file. */
    AssertPtrReturnVoid(m_pResolutionWidthSpin);
    AssertPtrReturnVoid(m_pResolutionHeightSpin);
    AssertPtrReturnVoid(m_pMaxResolutionCombo);
    {
        /* Configure widgets: */
        const int iMinWidth = 640;
        const int iMinHeight = 480;
        const int iMaxSize = 16 * _1K;
        m_pResolutionWidthSpin->setMinimum(iMinWidth);
        m_pResolutionWidthSpin->setMaximum(iMaxSize);
        m_pResolutionHeightSpin->setMinimum(iMinHeight);
        m_pResolutionHeightSpin->setMaximum(iMaxSize);
        connect(m_pMaxResolutionCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
                this, &UIGlobalSettingsDisplay::sltHandleMaximumGuestScreenSizePolicyChange);
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsDisplay::prepareWidgets()
{
   if (objectName().isEmpty())
       setObjectName(QStringLiteral("UIGlobalSettingsDisplay"));
   QGridLayout *pMainLayout = new QGridLayout(this);
   pMainLayout->setContentsMargins(0, 0, 0, 0);
   pMainLayout->setObjectName(QStringLiteral("pMainLayout"));
   m_pMaxResolutionLabel = new QLabel();
   m_pMaxResolutionLabel->setObjectName(QStringLiteral("m_pMaxResolutionLabel"));
   m_pMaxResolutionLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   pMainLayout->addWidget(m_pMaxResolutionLabel, 0, 0, 1, 1);

   m_pMaxResolutionCombo = new QComboBox();
   m_pMaxResolutionCombo->setObjectName(QStringLiteral("m_pMaxResolutionCombo"));
   QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
   sizePolicy.setHorizontalStretch(1);
   sizePolicy.setVerticalStretch(0);
   sizePolicy.setHeightForWidth(m_pMaxResolutionCombo->sizePolicy().hasHeightForWidth());
   m_pMaxResolutionCombo->setSizePolicy(sizePolicy);
   pMainLayout->addWidget(m_pMaxResolutionCombo, 0, 1, 1, 1);

   m_pResolutionWidthLabel = new QLabel();
   m_pResolutionWidthLabel->setObjectName(QStringLiteral("m_pResolutionWidthLabel"));
   m_pResolutionWidthLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   pMainLayout->addWidget(m_pResolutionWidthLabel, 1, 0, 1, 1);

   m_pResolutionWidthSpin = new QSpinBox();
   m_pResolutionWidthSpin->setObjectName(QStringLiteral("m_pResolutionWidthSpin"));
   QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
   sizePolicy1.setHorizontalStretch(1);
   sizePolicy1.setVerticalStretch(0);
   sizePolicy1.setHeightForWidth(m_pResolutionWidthSpin->sizePolicy().hasHeightForWidth());
   m_pResolutionWidthSpin->setSizePolicy(sizePolicy1);
   pMainLayout->addWidget(m_pResolutionWidthSpin, 1, 1, 1, 1);

   m_pResolutionHeightLabel = new QLabel();
   m_pResolutionHeightLabel->setObjectName(QStringLiteral("m_pResolutionHeightLabel"));
   m_pResolutionHeightLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   pMainLayout->addWidget(m_pResolutionHeightLabel, 2, 0, 1, 1);

   m_pResolutionHeightSpin = new QSpinBox();
   m_pResolutionHeightSpin->setObjectName(QStringLiteral("m_pResolutionHeightSpin"));
   sizePolicy1.setHeightForWidth(m_pResolutionHeightSpin->sizePolicy().hasHeightForWidth());
   m_pResolutionHeightSpin->setSizePolicy(sizePolicy1);
   pMainLayout->addWidget(m_pResolutionHeightSpin, 2, 1, 1, 1);

   m_pLabelGuestScreenScaleFactorEditor = new QLabel();
   m_pLabelGuestScreenScaleFactorEditor->setObjectName(QStringLiteral("m_pLabelGuestScreenScaleFactorEditor"));
   m_pLabelGuestScreenScaleFactorEditor->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   pMainLayout->addWidget(m_pLabelGuestScreenScaleFactorEditor, 3, 0, 1, 1);

   m_pScaleFactorEditor = new UIScaleFactorEditor(this);
   m_pScaleFactorEditor->setObjectName(QStringLiteral("m_pScaleFactorEditor"));
   QSizePolicy sizePolicy2(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
   sizePolicy2.setHorizontalStretch(0);
   sizePolicy2.setVerticalStretch(0);
   sizePolicy2.setHeightForWidth(m_pScaleFactorEditor->sizePolicy().hasHeightForWidth());
   m_pScaleFactorEditor->setSizePolicy(sizePolicy2);
   pMainLayout->addWidget(m_pScaleFactorEditor, 3, 1, 2, 1);

   m_pLabelMachineWindow = new QLabel();
   m_pLabelMachineWindow->setObjectName(QStringLiteral("m_pLabelMachineWindow"));
   m_pLabelMachineWindow->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
   pMainLayout->addWidget(m_pLabelMachineWindow, 5, 0, 1, 1);

   m_pCheckBoxActivateOnMouseHover = new QCheckBox();
   m_pCheckBoxActivateOnMouseHover->setObjectName(QStringLiteral("m_pCheckBoxActivateOnMouseHover"));
   sizePolicy2.setHeightForWidth(m_pCheckBoxActivateOnMouseHover->sizePolicy().hasHeightForWidth());
   m_pCheckBoxActivateOnMouseHover->setSizePolicy(sizePolicy2);
   pMainLayout->addWidget(m_pCheckBoxActivateOnMouseHover, 5, 1, 1, 1);

   QSpacerItem *spacerItem = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
   pMainLayout->addItem(spacerItem, 6, 0, 1, 2);

   m_pMaxResolutionLabel->setBuddy(m_pMaxResolutionCombo);
   m_pResolutionWidthLabel->setBuddy(m_pResolutionWidthSpin);
   m_pResolutionHeightLabel->setBuddy(m_pResolutionHeightSpin);
}

void UIGlobalSettingsDisplay::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

void UIGlobalSettingsDisplay::reloadMaximumGuestScreenSizePolicyComboBox()
{
    /* Remember current position: */
    int iCurrentPosition = m_pMaxResolutionCombo->currentIndex();
    if (iCurrentPosition == -1)
        iCurrentPosition = 0;

    /* Clear combo-box: */
    m_pMaxResolutionCombo->clear();

    /* Create corresponding items: */
    m_pMaxResolutionCombo->addItem(tr("Automatic", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Automatic));
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Suggest a reasonable maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);
    m_pMaxResolutionCombo->addItem(tr("None", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Any));
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Do not attempt to limit the size of the guest screen."),
                                       Qt::ToolTipRole);
    m_pMaxResolutionCombo->addItem(tr("Hint", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Fixed));
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Suggest a maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);

    /* Choose previous position: */
    m_pMaxResolutionCombo->setCurrentIndex(iCurrentPosition);
    sltHandleMaximumGuestScreenSizePolicyChange();
}

bool UIGlobalSettingsDisplay::saveDisplayData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save display settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old display data from the cache: */
        const UIDataSettingsGlobalDisplay &oldDisplayData = m_pCache->base();
        /* Get new display data from the cache: */
        const UIDataSettingsGlobalDisplay &newDisplayData = m_pCache->data();

        /* Save maximum guest resolution policy and/or value: */
        if (   fSuccess
            && (   newDisplayData.m_enmMaxGuestResolution != oldDisplayData.m_enmMaxGuestResolution
                || newDisplayData.m_maxGuestResolution != oldDisplayData.m_maxGuestResolution))
            gEDataManager->setMaxGuestScreenResolution(newDisplayData.m_enmMaxGuestResolution, newDisplayData.m_maxGuestResolution);
        /* Save whether hovered machine-window should be activated automatically: */
        if (fSuccess && newDisplayData.m_fActivateHoveredMachineWindow != oldDisplayData.m_fActivateHoveredMachineWindow)
            gEDataManager->setActivateHoveredMachineWindow(newDisplayData.m_fActivateHoveredMachineWindow);
        /* Save guest-screen scale-factor: */
        if (fSuccess && newDisplayData.m_scaleFactors != oldDisplayData.m_scaleFactors)
            gEDataManager->setScaleFactors(newDisplayData.m_scaleFactors, UIExtraDataManager::GlobalID);
    }
    /* Return result: */
    return fSuccess;
}
