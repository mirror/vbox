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
    , m_pLabelMaxGuestScreenSize(0)
    , m_pComboMaxGuestScreenSize(0)
    , m_pLabelMaxGuestScreenWidth(0)
    , m_pSpinboxMaxGuestScreenWidth(0)
    , m_pLabelMaxGuestScreenHeight(0)
    , m_pSpinboxMaxGuestScreenHeight(0)
    , m_pLabelScaleFactor(0)
    , m_pEditorScaleFactor(0)
    , m_pLabelMachineWindows(0)
    , m_pCheckBoxActivateOnMouseHover(0)
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
    m_pComboMaxGuestScreenSize->setCurrentIndex(m_pComboMaxGuestScreenSize->findData((int)oldDisplayData.m_enmMaxGuestResolution));
    if (oldDisplayData.m_enmMaxGuestResolution == MaxGuestResolutionPolicy_Fixed)
    {
        m_pSpinboxMaxGuestScreenWidth->setValue(oldDisplayData.m_maxGuestResolution.width());
        m_pSpinboxMaxGuestScreenHeight->setValue(oldDisplayData.m_maxGuestResolution.height());
    }
    m_pCheckBoxActivateOnMouseHover->setChecked(oldDisplayData.m_fActivateHoveredMachineWindow);
    m_pEditorScaleFactor->setScaleFactors(oldDisplayData.m_scaleFactors);
    m_pEditorScaleFactor->setMonitorCount(gpDesktop->screenCount());
}

void UIGlobalSettingsDisplay::putToCache()
{
    /* Prepare new display data: */
    UIDataSettingsGlobalDisplay newDisplayData = m_pCache->base();

    /* Gather new display data: */
    newDisplayData.m_enmMaxGuestResolution = (MaxGuestResolutionPolicy)m_pComboMaxGuestScreenSize->itemData(m_pComboMaxGuestScreenSize->currentIndex()).toInt();
    if (newDisplayData.m_enmMaxGuestResolution == MaxGuestResolutionPolicy_Fixed)
        newDisplayData.m_maxGuestResolution = QSize(m_pSpinboxMaxGuestScreenWidth->value(), m_pSpinboxMaxGuestScreenHeight->value());
    newDisplayData.m_fActivateHoveredMachineWindow = m_pCheckBoxActivateOnMouseHover->isChecked();
    newDisplayData.m_scaleFactors = m_pEditorScaleFactor->scaleFactors();

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
    m_pLabelMaxGuestScreenSize->setText(tr("Maximum Guest Screen &Size:"));
    m_pLabelMaxGuestScreenWidth->setText(tr("&Width:"));
    m_pSpinboxMaxGuestScreenWidth->setWhatsThis(tr("Holds the maximum width which we would like the guest to use."));
    m_pLabelMaxGuestScreenHeight->setText(tr("&Height:"));
    m_pSpinboxMaxGuestScreenHeight->setWhatsThis(tr("Holds the maximum height which we would like the guest to use."));
    m_pLabelScaleFactor->setText(tr("Scale Factor:"));
    m_pEditorScaleFactor->setWhatsThis(tr("Controls the guest screen scale factor."));
    m_pLabelMachineWindows->setText(tr("Machine Windows:"));
    m_pCheckBoxActivateOnMouseHover->setWhatsThis(tr("When checked, machine windows will be raised when the mouse pointer moves over them."));
    m_pCheckBoxActivateOnMouseHover->setText(tr("&Raise Window Under Mouse"));

    /* Reload combo-box: */
    reloadMaximumGuestScreenSizePolicyComboBox();
}

void UIGlobalSettingsDisplay::sltHandleMaximumGuestScreenSizePolicyChange()
{
    /* Get current resolution-combo tool-tip data: */
    const QString strCurrentComboItemTip = m_pComboMaxGuestScreenSize->itemData(m_pComboMaxGuestScreenSize->currentIndex(), Qt::ToolTipRole).toString();
    m_pComboMaxGuestScreenSize->setWhatsThis(strCurrentComboItemTip);

    /* Get current resolution-combo item data: */
    const MaxGuestResolutionPolicy enmPolicy = (MaxGuestResolutionPolicy)m_pComboMaxGuestScreenSize->itemData(m_pComboMaxGuestScreenSize->currentIndex()).toInt();
    /* Should be combo-level widgets enabled? */
    const bool fComboLevelWidgetsEnabled = enmPolicy == MaxGuestResolutionPolicy_Fixed;
    /* Enable/disable combo-level widgets: */
    m_pLabelMaxGuestScreenWidth->setEnabled(fComboLevelWidgetsEnabled);
    m_pSpinboxMaxGuestScreenWidth->setEnabled(fComboLevelWidgetsEnabled);
    m_pLabelMaxGuestScreenHeight->setEnabled(fComboLevelWidgetsEnabled);
    m_pSpinboxMaxGuestScreenHeight->setEnabled(fComboLevelWidgetsEnabled);
}

void UIGlobalSettingsDisplay::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalDisplay;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsDisplay::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayoutMain = new QGridLayout(this);
    if (pLayoutMain)
    {
        pLayoutMain->setColumnStretch(1, 1);
        pLayoutMain->setRowStretch(6, 1);

        const int iMinWidth = 640;
        const int iMinHeight = 480;
        const int iMaxSize = 16 * _1K;

        /* Prepare max guest screen size label: */
        m_pLabelMaxGuestScreenSize = new QLabel(this);
        if (m_pLabelMaxGuestScreenSize)
        {
           m_pLabelMaxGuestScreenSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
           pLayoutMain->addWidget(m_pLabelMaxGuestScreenSize, 0, 0);
        }
        /* Prepare max guest screen size combo: */
        m_pComboMaxGuestScreenSize = new QComboBox(this);
        if (m_pComboMaxGuestScreenSize)
        {
            if (m_pLabelMaxGuestScreenSize)
                m_pLabelMaxGuestScreenSize->setBuddy(m_pComboMaxGuestScreenSize);
            pLayoutMain->addWidget(m_pComboMaxGuestScreenSize, 0, 1, 1, 2);
        }

        /* Prepare max guest screen width label: */
        m_pLabelMaxGuestScreenWidth = new QLabel(this);
        if (m_pLabelMaxGuestScreenWidth)
        {
            m_pLabelMaxGuestScreenWidth->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMaxGuestScreenWidth, 1, 0);
        }
        /* Prepare max guest screen width spinbox: */
        m_pSpinboxMaxGuestScreenWidth = new QSpinBox(this);
        if (m_pSpinboxMaxGuestScreenWidth)
        {
            if (m_pLabelMaxGuestScreenWidth)
                m_pLabelMaxGuestScreenWidth->setBuddy(m_pSpinboxMaxGuestScreenWidth);
            m_pSpinboxMaxGuestScreenWidth->setMinimum(iMinWidth);
            m_pSpinboxMaxGuestScreenWidth->setMaximum(iMaxSize);

            pLayoutMain->addWidget(m_pSpinboxMaxGuestScreenWidth, 1, 1, 1, 2);
        }

        /* Prepare max guest screen height label: */
        m_pLabelMaxGuestScreenHeight = new QLabel(this);
        if (m_pLabelMaxGuestScreenHeight)
        {
            m_pLabelMaxGuestScreenHeight->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMaxGuestScreenHeight, 2, 0);
        }
        /* Prepare max guest screen width spinbox: */
        m_pSpinboxMaxGuestScreenHeight = new QSpinBox(this);
        if (m_pSpinboxMaxGuestScreenHeight)
        {
            if (m_pLabelMaxGuestScreenHeight)
                m_pLabelMaxGuestScreenHeight->setBuddy(m_pSpinboxMaxGuestScreenHeight);
            m_pSpinboxMaxGuestScreenHeight->setMinimum(iMinHeight);
            m_pSpinboxMaxGuestScreenHeight->setMaximum(iMaxSize);

            pLayoutMain->addWidget(m_pSpinboxMaxGuestScreenHeight, 2, 1, 1, 2);
        }

        /* Prepare scale-factor label: */
        m_pLabelScaleFactor = new QLabel(this);
        if (m_pLabelScaleFactor)
        {
            m_pLabelScaleFactor->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelScaleFactor, 3, 0);
        }
        /* Prepare scale-factor editor: */
        m_pEditorScaleFactor = new UIScaleFactorEditor(this);
        if (m_pEditorScaleFactor)
            pLayoutMain->addWidget(m_pEditorScaleFactor, 3, 1, 2, 2);

        /* Prepare 'machine-windows' label: */
        m_pLabelMachineWindows = new QLabel(this);
        if (m_pLabelMachineWindows)
        {
            m_pLabelMachineWindows->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMachineWindows, 5, 0);
        }
        /* Prepare 'activate on mouse hover' check-box: */
        m_pCheckBoxActivateOnMouseHover = new QCheckBox(this);
        if (m_pCheckBoxActivateOnMouseHover)
            pLayoutMain->addWidget(m_pCheckBoxActivateOnMouseHover, 5, 1);
    }
}

void UIGlobalSettingsDisplay::prepareConnections()
{
    connect(m_pComboMaxGuestScreenSize, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            this, &UIGlobalSettingsDisplay::sltHandleMaximumGuestScreenSizePolicyChange);
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
    int iCurrentPosition = m_pComboMaxGuestScreenSize->currentIndex();
    if (iCurrentPosition == -1)
        iCurrentPosition = 0;

    /* Clear combo-box: */
    m_pComboMaxGuestScreenSize->clear();

    /* Create corresponding items: */
    m_pComboMaxGuestScreenSize->addItem(tr("Automatic", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Automatic));
    m_pComboMaxGuestScreenSize->setItemData(m_pComboMaxGuestScreenSize->count() - 1,
                                       tr("Suggest a reasonable maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);
    m_pComboMaxGuestScreenSize->addItem(tr("None", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Any));
    m_pComboMaxGuestScreenSize->setItemData(m_pComboMaxGuestScreenSize->count() - 1,
                                       tr("Do not attempt to limit the size of the guest screen."),
                                       Qt::ToolTipRole);
    m_pComboMaxGuestScreenSize->addItem(tr("Hint", "Maximum Guest Screen Size"),
                                   QVariant((int)MaxGuestResolutionPolicy_Fixed));
    m_pComboMaxGuestScreenSize->setItemData(m_pComboMaxGuestScreenSize->count() - 1,
                                       tr("Suggest a maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);

    /* Choose previous position: */
    m_pComboMaxGuestScreenSize->setCurrentIndex(iCurrentPosition);
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
