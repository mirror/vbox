/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsDisplay class implementation.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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
# include "UIGlobalSettingsDisplay.h"
# include "UIExtraDataManager.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* Display page constructor: */
UIGlobalSettingsDisplay::UIGlobalSettingsDisplay()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsDisplay::setupUi(this);

    /* Setup widgets: */
    int iMinWidth = 640;
    int iMinHeight = 480;
    int iMaxSize = 16 * _1K;
    m_pResolutionWidthSpin->setMinimum(iMinWidth);
    m_pResolutionHeightSpin->setMinimum(iMinHeight);
    m_pResolutionWidthSpin->setMaximum(iMaxSize);
    m_pResolutionHeightSpin->setMaximum(iMaxSize);

    /* Setup connections: */
    connect(m_pMaxResolutionCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(sltMaxResolutionComboActivated()));

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsDisplay::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    m_cache.m_strMaxGuestResolution = m_settings.maxGuestRes();
    m_cache.m_fActivateHoveredMachineWindow = gEDataManager->activateHoveredMachineWindow();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsDisplay::getFromCache()
{
    /* Fetch from cache: */
    if ((m_cache.m_strMaxGuestResolution.isEmpty()) ||
        (m_cache.m_strMaxGuestResolution == "auto"))
    {
        /* Switch combo-box item: */
        m_pMaxResolutionCombo->setCurrentIndex(m_pMaxResolutionCombo->findData("auto"));
    }
    else if (m_cache.m_strMaxGuestResolution == "any")
    {
        /* Switch combo-box item: */
        m_pMaxResolutionCombo->setCurrentIndex(m_pMaxResolutionCombo->findData("any"));
    }
    else
    {
        /* Switch combo-box item: */
        m_pMaxResolutionCombo->setCurrentIndex(m_pMaxResolutionCombo->findData("fixed"));
        /* Trying to parse text into 2 sections by ',' symbol: */
        int iWidth  = m_cache.m_strMaxGuestResolution.section(',', 0, 0).toInt();
        int iHeight = m_cache.m_strMaxGuestResolution.section(',', 1, 1).toInt();
        /* And set values if they are present: */
        m_pResolutionWidthSpin->setValue(iWidth);
        m_pResolutionHeightSpin->setValue(iHeight);
    }
    /* Set state for corresponding check-box: */
    m_pCheckBoxActivateOnMouseHover->setChecked(m_cache.m_fActivateHoveredMachineWindow);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsDisplay::putToCache()
{
    /* Upload to cache: */
    if (m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex()).toString() == "auto")
    {
        /* If resolution current combo item is "auto" => resolution set to "auto": */
        m_cache.m_strMaxGuestResolution = QString();
    }
    else if (m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex()).toString() == "any" ||
             m_pResolutionWidthSpin->value() == 0 || m_pResolutionHeightSpin->value() == 0)
    {
        /* Else if resolution current combo item is "any"
         * or any of the resolution field attributes is zero => resolution set to "any": */
        m_cache.m_strMaxGuestResolution = "any";
    }
    else if (m_pResolutionWidthSpin->value() != 0 && m_pResolutionHeightSpin->value() != 0)
    {
        /* Else if both field attributes are non-zeroes => resolution set to "fixed": */
        m_cache.m_strMaxGuestResolution = QString("%1,%2").arg(m_pResolutionWidthSpin->value()).arg(m_pResolutionHeightSpin->value());
    }
    /* Acquire state from corresponding check-box: */
    m_cache.m_fActivateHoveredMachineWindow = m_pCheckBoxActivateOnMouseHover->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsDisplay::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    m_settings.setMaxGuestRes(m_cache.m_strMaxGuestResolution);
    gEDataManager->setActivateHoveredMachineWindow(m_cache.m_fActivateHoveredMachineWindow);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsDisplay::setOrderAfter(QWidget*)
{
}

void UIGlobalSettingsDisplay::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsDisplay::retranslateUi(this);

    /* Populate combo-box: */
    populate();
}

void UIGlobalSettingsDisplay::sltMaxResolutionComboActivated()
{
    /* Get current resolution-combo tool-tip data: */
    QString strCurrentComboItemTip = m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex(), Qt::ToolTipRole).toString();
    m_pMaxResolutionCombo->setWhatsThis(strCurrentComboItemTip);

    /* Get current resolution-combo item data: */
    QString strCurrentComboItemData = m_pMaxResolutionCombo->itemData(m_pMaxResolutionCombo->currentIndex()).toString();
    /* Is our combo in state for 'fixed' resolution? */
    bool fCurrentResolutionStateFixed = strCurrentComboItemData == "fixed";
    /* Should be combo-level widgets enabled? */
    bool fComboLevelWidgetsEnabled = fCurrentResolutionStateFixed;
    /* Enable/disable combo-level widgets: */
    m_pResolutionWidthLabel->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionWidthSpin->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionHeightLabel->setEnabled(fComboLevelWidgetsEnabled);
    m_pResolutionHeightSpin->setEnabled(fComboLevelWidgetsEnabled);
}

void UIGlobalSettingsDisplay::populate()
{
    /* Remember current position: */
    int iCurrentPosition = m_pMaxResolutionCombo->currentIndex();
    if (iCurrentPosition == -1)
        iCurrentPosition = 0;

    /* Clear combo-box: */
    m_pMaxResolutionCombo->clear();

    /* Create corresponding items: */
    m_pMaxResolutionCombo->addItem(tr("Automatic", "Maximum Guest Screen Size"), "auto");
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Suggest a reasonable maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);
    m_pMaxResolutionCombo->addItem(tr("None", "Maximum Guest Screen Size"), "any");
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Do not attempt to limit the size of the guest screen."),
                                       Qt::ToolTipRole);
    m_pMaxResolutionCombo->addItem(tr("Hint", "Maximum Guest Screen Size"), "fixed");
    m_pMaxResolutionCombo->setItemData(m_pMaxResolutionCombo->count() - 1,
                                       tr("Suggest a maximum screen size to the guest. "
                                          "The guest will only see this suggestion when guest additions are installed."),
                                       Qt::ToolTipRole);

    /* Choose previous position: */
    m_pMaxResolutionCombo->setCurrentIndex(iCurrentPosition);
    sltMaxResolutionComboActivated();
}

