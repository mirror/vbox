/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class implementation.
 */

/*
 * Copyright (C) 2012-2021 Oracle Corporation
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
#include <QGridLayout>
#include <QLabel>

/* GUI includes: */
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSettingsDisplay.h"
#include "UIMaximumGuestScreenSizeEditor.h"
#include "UIMessageCenter.h"
#include "UIScaleFactorEditor.h"


/** Global settings: Display page data structure. */
struct UIDataSettingsGlobalDisplay
{
    /** Constructs data. */
    UIDataSettingsGlobalDisplay()
        : m_fActivateHoveredMachineWindow(false)
        , m_fDisableHostScreenSaver(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalDisplay &other) const
    {
        return    true
               && (m_guiMaximumGuestScreenSizeValue == other.m_guiMaximumGuestScreenSizeValue)
               && (m_fActivateHoveredMachineWindow == other.m_fActivateHoveredMachineWindow)
               && (m_fDisableHostScreenSaver == other.m_fDisableHostScreenSaver)
               && (m_scaleFactors == other.m_scaleFactors)
                  ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalDisplay &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalDisplay &other) const { return !equal(other); }

    /** Holds the maximum guest-screen size value. */
    UIMaximumGuestScreenSizeValue  m_guiMaximumGuestScreenSizeValue;
    /** Holds whether we should automatically activate machine window under the mouse cursor. */
    bool                           m_fActivateHoveredMachineWindow;
    /** Holds whether we should disable host sceen saver on a vm is running. */
    bool                           m_fDisableHostScreenSaver;

    /** Holds the guest screen scale-factor. */
    QList<double>                  m_scaleFactors;
};


/*********************************************************************************************************************************
*   Class UIGlobalSettingsDisplay implementation.                                                                                *
*********************************************************************************************************************************/

UIGlobalSettingsDisplay::UIGlobalSettingsDisplay()
    : m_pCache(0)
    , m_pLabelMaximumGuestScreenSizePolicy(0)
    , m_pLabelMaximumGuestScreenWidth(0)
    , m_pLabelMaximumGuestScreenHeight(0)
    , m_pEditorMaximumGuestScreenSize(0)
    , m_pLabelScaleFactor(0)
    , m_pEditorScaleFactor(0)
    , m_pLabelMachineWindows(0)
    , m_pCheckBoxActivateOnMouseHover(0)
    , m_pCheckBoxDisableHostScreenSaver(0)
{
    prepare();
}

UIGlobalSettingsDisplay::~UIGlobalSettingsDisplay()
{
    cleanup();
}

void UIGlobalSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Cache old data: */
    UIDataSettingsGlobalDisplay oldData;
    oldData.m_guiMaximumGuestScreenSizeValue = UIMaximumGuestScreenSizeValue(gEDataManager->maxGuestResolutionPolicy(),
                                                                             gEDataManager->maxGuestResolutionForPolicyFixed());
    oldData.m_fActivateHoveredMachineWindow = gEDataManager->activateHoveredMachineWindow();
    oldData.m_fDisableHostScreenSaver = gEDataManager->disableHostScreenSaver();
    oldData.m_scaleFactors = gEDataManager->scaleFactors(UIExtraDataManager::GlobalID);
    m_pCache->cacheInitialData(oldData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsDisplay::getFromCache()
{
    /* Load old data from cache: */
    const UIDataSettingsGlobalDisplay &oldData = m_pCache->base();
    m_pEditorMaximumGuestScreenSize->setValue(oldData.m_guiMaximumGuestScreenSizeValue);
    m_pCheckBoxActivateOnMouseHover->setChecked(oldData.m_fActivateHoveredMachineWindow);
    m_pCheckBoxDisableHostScreenSaver->setChecked(oldData.m_fDisableHostScreenSaver);
    m_pEditorScaleFactor->setScaleFactors(oldData.m_scaleFactors);
    m_pEditorScaleFactor->setMonitorCount(gpDesktop->screenCount());
}

void UIGlobalSettingsDisplay::putToCache()
{
    /* Prepare new data: */
    UIDataSettingsGlobalDisplay newData = m_pCache->base();

    /* Cache new data: */
    newData.m_guiMaximumGuestScreenSizeValue = m_pEditorMaximumGuestScreenSize->value();
    newData.m_fActivateHoveredMachineWindow = m_pCheckBoxActivateOnMouseHover->isChecked();
    newData.m_fDisableHostScreenSaver = m_pCheckBoxDisableHostScreenSaver->isChecked();
    newData.m_scaleFactors = m_pEditorScaleFactor->scaleFactors();
    m_pCache->cacheCurrentData(newData);
}

void UIGlobalSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update data and failing state: */
    setFailed(!saveData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsDisplay::retranslateUi()
{
    m_pLabelMaximumGuestScreenSizePolicy->setText(tr("Maximum Guest Screen &Size:"));
    m_pLabelMaximumGuestScreenWidth->setText(tr("&Width:"));
    m_pLabelMaximumGuestScreenHeight->setText(tr("&Height:"));
    m_pLabelScaleFactor->setText(tr("Scale &Factor:"));
    m_pEditorScaleFactor->setWhatsThis(tr("Controls the guest screen scale factor."));
    m_pLabelMachineWindows->setText(tr("Machine Windows:"));
    m_pCheckBoxActivateOnMouseHover->setWhatsThis(tr("When checked, machine windows will be raised when the mouse pointer moves over them."));
    m_pCheckBoxActivateOnMouseHover->setText(tr("&Raise Window Under Mouse"));
    m_pCheckBoxDisableHostScreenSaver->setWhatsThis(tr("When checked, screen saver of the host OS is disabled."));
    m_pCheckBoxDisableHostScreenSaver->setText(tr("&Disable Host Screen Saver"));
}

void UIGlobalSettingsDisplay::prepare()
{
    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalDisplay;
    AssertPtrReturnVoid(m_pCache);

    /* Prepare everything: */
    prepareWidgets();

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
        pLayoutMain->setRowStretch(7, 1);

        /* Prepare maximum guest screen size label: */
        m_pLabelMaximumGuestScreenSizePolicy = new QLabel(this);
        if (m_pLabelMaximumGuestScreenSizePolicy)
        {
           m_pLabelMaximumGuestScreenSizePolicy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
           pLayoutMain->addWidget(m_pLabelMaximumGuestScreenSizePolicy, 0, 0);
        }
        /* Prepare maximum guest screen width label: */
        m_pLabelMaximumGuestScreenWidth = new QLabel(this);
        if (m_pLabelMaximumGuestScreenWidth)
        {
            m_pLabelMaximumGuestScreenWidth->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMaximumGuestScreenWidth, 1, 0);
        }
        /* Prepare maximum guest screen height label: */
        m_pLabelMaximumGuestScreenHeight = new QLabel(this);
        if (m_pLabelMaximumGuestScreenHeight)
        {
            m_pLabelMaximumGuestScreenHeight->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            pLayoutMain->addWidget(m_pLabelMaximumGuestScreenHeight, 2, 0);
        }
        /* Prepare maximum guest screen size editor: */
        m_pEditorMaximumGuestScreenSize = new UIMaximumGuestScreenSizeEditor(this);
        if (m_pEditorMaximumGuestScreenSize)
        {
            if (m_pLabelMaximumGuestScreenSizePolicy)
                m_pLabelMaximumGuestScreenSizePolicy->setBuddy(m_pEditorMaximumGuestScreenSize->focusProxy1());
            if (m_pLabelMaximumGuestScreenWidth)
                m_pLabelMaximumGuestScreenWidth->setBuddy(m_pEditorMaximumGuestScreenSize->focusProxy2());
            if (m_pLabelMaximumGuestScreenHeight)
                m_pLabelMaximumGuestScreenHeight->setBuddy(m_pEditorMaximumGuestScreenSize->focusProxy3());
            pLayoutMain->addWidget(m_pEditorMaximumGuestScreenSize, 0, 1, 3, 2);
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
        {
            if (m_pLabelScaleFactor)
                m_pLabelScaleFactor->setBuddy(m_pEditorScaleFactor->focusProxy());
            pLayoutMain->addWidget(m_pEditorScaleFactor, 3, 1, 2, 2);
        }

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

        /* Prepare 'disable host screen saver' check-box: */
        m_pCheckBoxDisableHostScreenSaver = new QCheckBox(this);
        if (m_pCheckBoxDisableHostScreenSaver)
            pLayoutMain->addWidget(m_pCheckBoxDisableHostScreenSaver, 6, 1);
    }
}

void UIGlobalSettingsDisplay::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsDisplay::saveData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save display settings from cache: */
    if (   fSuccess
        && m_pCache->wasChanged())
    {
        /* Get old data from cache: */
        const UIDataSettingsGlobalDisplay &oldData = m_pCache->base();
        /* Get new data from cache: */
        const UIDataSettingsGlobalDisplay &newData = m_pCache->data();

        /* Save maximum guest screen size and policy: */
        if (   fSuccess
            && newData.m_guiMaximumGuestScreenSizeValue != oldData.m_guiMaximumGuestScreenSizeValue)
            /* fSuccess = */ gEDataManager->setMaxGuestScreenResolution(newData.m_guiMaximumGuestScreenSizeValue.m_enmPolicy,
                                                                        newData.m_guiMaximumGuestScreenSizeValue.m_size);
        /* Save whether hovered machine-window should be activated automatically: */
        if (   fSuccess
            && newData.m_fActivateHoveredMachineWindow != oldData.m_fActivateHoveredMachineWindow)
            /* fSuccess = */ gEDataManager->setActivateHoveredMachineWindow(newData.m_fActivateHoveredMachineWindow);
        /* Save whether the host screen saver is to be disable when a vm is running: */
        if (   fSuccess
            && newData.m_fDisableHostScreenSaver != oldData.m_fDisableHostScreenSaver)
            /* fSuccess = */ gEDataManager->setDisableHostScreenSaver(newData.m_fDisableHostScreenSaver);
        /* Save guest-screen scale-factor: */
        if (   fSuccess
            && newData.m_scaleFactors != oldData.m_scaleFactors)
            /* fSuccess = */ gEDataManager->setScaleFactors(newData.m_scaleFactors, UIExtraDataManager::GlobalID);
    }
    /* Return result: */
    return fSuccess;
}
