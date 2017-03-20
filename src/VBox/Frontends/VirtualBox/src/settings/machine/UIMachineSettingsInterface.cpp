/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
# include "UIMachineSettingsInterface.h"
# include "UIExtraDataManager.h"
# include "UIActionPool.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIMachineSettingsInterface::UIMachineSettingsInterface(const QString strMachineID)
    : m_strMachineID(strMachineID)
    , m_pActionPool(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsInterface::~UIMachineSettingsInterface()
{
    /* Cleanup: */
    cleanup();
}

void UIMachineSettingsInterface::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Prepare interface data: */
    UIDataSettingsMachineInterface interfaceData;

    /* Cache interface data: */
    interfaceData.m_fStatusBarEnabled = gEDataManager->statusBarEnabled(m_machine.GetId());
    interfaceData.m_statusBarRestrictions = gEDataManager->restrictedStatusBarIndicators(m_machine.GetId());
    interfaceData.m_statusBarOrder = gEDataManager->statusBarIndicatorOrder(m_machine.GetId());
#ifndef VBOX_WS_MAC
    interfaceData.m_fMenuBarEnabled = gEDataManager->menuBarEnabled(m_machine.GetId());
#endif /* !VBOX_WS_MAC */
    interfaceData.m_restrictionsOfMenuBar = gEDataManager->restrictedRuntimeMenuTypes(m_machine.GetId());
    interfaceData.m_restrictionsOfMenuApplication = gEDataManager->restrictedRuntimeMenuApplicationActionTypes(m_machine.GetId());
    interfaceData.m_restrictionsOfMenuMachine = gEDataManager->restrictedRuntimeMenuMachineActionTypes(m_machine.GetId());
    interfaceData.m_restrictionsOfMenuView = gEDataManager->restrictedRuntimeMenuViewActionTypes(m_machine.GetId());
    interfaceData.m_restrictionsOfMenuInput = gEDataManager->restrictedRuntimeMenuInputActionTypes(m_machine.GetId());
    interfaceData.m_restrictionsOfMenuDevices = gEDataManager->restrictedRuntimeMenuDevicesActionTypes(m_machine.GetId());
#ifdef VBOX_WITH_DEBUGGER_GUI
    interfaceData.m_restrictionsOfMenuDebug = gEDataManager->restrictedRuntimeMenuDebuggerActionTypes(m_machine.GetId());
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
    interfaceData.m_restrictionsOfMenuWindow = gEDataManager->restrictedRuntimeMenuWindowActionTypes(m_machine.GetId());
#endif /* VBOX_WS_MAC */
    interfaceData.m_restrictionsOfMenuHelp = gEDataManager->restrictedRuntimeMenuHelpActionTypes(m_machine.GetId());
#ifndef VBOX_WS_MAC
    interfaceData.m_fShowMiniToolBar = gEDataManager->miniToolbarEnabled(m_machine.GetId());
    interfaceData.m_fMiniToolBarAtTop = gEDataManager->miniToolbarAlignment(m_machine.GetId()) == Qt::AlignTop;
#endif /* !VBOX_WS_MAC */

    /* Cache interface data: */
    m_cache.cacheInitialData(interfaceData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsInterface::getFromCache()
{
    /* Get interface data from cache: */
    const UIDataSettingsMachineInterface &interfaceData = m_cache.base();

    /* Prepare interface data: */
    m_pStatusBarEditor->setStatusBarEnabled(interfaceData.m_fStatusBarEnabled);
    m_pStatusBarEditor->setStatusBarConfiguration(interfaceData.m_statusBarRestrictions,
                                                  interfaceData.m_statusBarOrder);
#ifndef VBOX_WS_MAC
    m_pMenuBarEditor->setMenuBarEnabled(interfaceData.m_fMenuBarEnabled);
#endif /* !VBOX_WS_MAC */
    m_pMenuBarEditor->setRestrictionsOfMenuBar(interfaceData.m_restrictionsOfMenuBar);
    m_pMenuBarEditor->setRestrictionsOfMenuApplication(interfaceData.m_restrictionsOfMenuApplication);
    m_pMenuBarEditor->setRestrictionsOfMenuMachine(interfaceData.m_restrictionsOfMenuMachine);
    m_pMenuBarEditor->setRestrictionsOfMenuView(interfaceData.m_restrictionsOfMenuView);
    m_pMenuBarEditor->setRestrictionsOfMenuInput(interfaceData.m_restrictionsOfMenuInput);
    m_pMenuBarEditor->setRestrictionsOfMenuDevices(interfaceData.m_restrictionsOfMenuDevices);
#ifdef VBOX_WITH_DEBUGGER_GUI
    m_pMenuBarEditor->setRestrictionsOfMenuDebug(interfaceData.m_restrictionsOfMenuDebug);
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
    m_pMenuBarEditor->setRestrictionsOfMenuWindow(interfaceData.m_restrictionsOfMenuWindow);
#endif /* VBOX_WS_MAC */
    m_pMenuBarEditor->setRestrictionsOfMenuHelp(interfaceData.m_restrictionsOfMenuHelp);
#ifndef VBOX_WS_MAC
    m_pCheckBoxShowMiniToolBar->setChecked(interfaceData.m_fShowMiniToolBar);
    m_pComboToolBarAlignment->setChecked(interfaceData.m_fMiniToolBarAtTop);
#endif /* !VBOX_WS_MAC */

    /* Polish page finally: */
    polishPage();

    /* Revalidate: */
    revalidate();
}

void UIMachineSettingsInterface::putToCache()
{
    /* Prepare interface data: */
    UIDataSettingsMachineInterface interfaceData = m_cache.base();

    /* Gather interface data from page: */
    interfaceData.m_fStatusBarEnabled = m_pStatusBarEditor->isStatusBarEnabled();
    interfaceData.m_statusBarRestrictions = m_pStatusBarEditor->statusBarIndicatorRestrictions();
    interfaceData.m_statusBarOrder = m_pStatusBarEditor->statusBarIndicatorOrder();
#ifndef VBOX_WS_MAC
    interfaceData.m_fMenuBarEnabled = m_pMenuBarEditor->isMenuBarEnabled();
#endif /* !VBOX_WS_MAC */
    interfaceData.m_restrictionsOfMenuBar = m_pMenuBarEditor->restrictionsOfMenuBar();
    interfaceData.m_restrictionsOfMenuApplication = m_pMenuBarEditor->restrictionsOfMenuApplication();
    interfaceData.m_restrictionsOfMenuMachine = m_pMenuBarEditor->restrictionsOfMenuMachine();
    interfaceData.m_restrictionsOfMenuView = m_pMenuBarEditor->restrictionsOfMenuView();
    interfaceData.m_restrictionsOfMenuInput = m_pMenuBarEditor->restrictionsOfMenuInput();
    interfaceData.m_restrictionsOfMenuDevices = m_pMenuBarEditor->restrictionsOfMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    interfaceData.m_restrictionsOfMenuDebug = m_pMenuBarEditor->restrictionsOfMenuDebug();
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
    interfaceData.m_restrictionsOfMenuWindow = m_pMenuBarEditor->restrictionsOfMenuWindow();
#endif /* VBOX_WS_MAC */
    interfaceData.m_restrictionsOfMenuHelp = m_pMenuBarEditor->restrictionsOfMenuHelp();
#ifndef VBOX_WS_MAC
    interfaceData.m_fShowMiniToolBar = m_pCheckBoxShowMiniToolBar->isChecked();
    interfaceData.m_fMiniToolBarAtTop = m_pComboToolBarAlignment->isChecked();
#endif /* !VBOX_WS_MAC */

    /* Cache interface data: */
    m_cache.cacheCurrentData(interfaceData);
}

void UIMachineSettingsInterface::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in valid mode & interface data was changed: */
    if (isMachineInValidMode() && m_cache.wasChanged())
    {
        /* Get interface data from cache: */
        const UIDataSettingsMachineInterface &interfaceData = m_cache.data();

        /* Store interface data: */
        if (isMachineInValidMode())
        {
            gEDataManager->setStatusBarEnabled(interfaceData.m_fStatusBarEnabled, m_machine.GetId());
            gEDataManager->setRestrictedStatusBarIndicators(interfaceData.m_statusBarRestrictions, m_machine.GetId());
            gEDataManager->setStatusBarIndicatorOrder(interfaceData.m_statusBarOrder, m_machine.GetId());
#ifndef VBOX_WS_MAC
            gEDataManager->setMenuBarEnabled(interfaceData.m_fMenuBarEnabled, m_machine.GetId());
#endif /* !VBOX_WS_MAC */
            gEDataManager->setRestrictedRuntimeMenuTypes(interfaceData.m_restrictionsOfMenuBar, m_machine.GetId());
            gEDataManager->setRestrictedRuntimeMenuApplicationActionTypes(interfaceData.m_restrictionsOfMenuApplication, m_machine.GetId());
            gEDataManager->setRestrictedRuntimeMenuMachineActionTypes(interfaceData.m_restrictionsOfMenuMachine, m_machine.GetId());
            gEDataManager->setRestrictedRuntimeMenuViewActionTypes(interfaceData.m_restrictionsOfMenuView, m_machine.GetId());
            gEDataManager->setRestrictedRuntimeMenuInputActionTypes(interfaceData.m_restrictionsOfMenuInput, m_machine.GetId());
            gEDataManager->setRestrictedRuntimeMenuDevicesActionTypes(interfaceData.m_restrictionsOfMenuDevices, m_machine.GetId());
#ifdef VBOX_WITH_DEBUGGER_GUI
            gEDataManager->setRestrictedRuntimeMenuDebuggerActionTypes(interfaceData.m_restrictionsOfMenuDebug, m_machine.GetId());
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
            gEDataManager->setRestrictedRuntimeMenuWindowActionTypes(interfaceData.m_restrictionsOfMenuWindow, m_machine.GetId());
#endif /* VBOX_WS_MAC */
            gEDataManager->setRestrictedRuntimeMenuHelpActionTypes(interfaceData.m_restrictionsOfMenuHelp, m_machine.GetId());
#ifndef VBOX_WS_MAC
            gEDataManager->setMiniToolbarEnabled(interfaceData.m_fShowMiniToolBar, m_machine.GetId());
            gEDataManager->setMiniToolbarAlignment(interfaceData.m_fMiniToolBarAtTop ? Qt::AlignTop : Qt::AlignBottom, m_machine.GetId());
#endif /* !VBOX_WS_MAC */
        }
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsInterface::setOrderAfter(QWidget *pWidget)
{
    /* Tab-order: */
    setTabOrder(pWidget, m_pCheckBoxShowMiniToolBar);
    setTabOrder(m_pCheckBoxShowMiniToolBar, m_pComboToolBarAlignment);
}

void UIMachineSettingsInterface::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsInterface::retranslateUi(this);
}

void UIMachineSettingsInterface::polishPage()
{
    /* Polish interface availability: */
    m_pMenuBarEditor->setEnabled(isMachineInValidMode());
#ifdef VBOX_WS_MAC
    m_pLabelMiniToolBar->hide();
    m_pCheckBoxShowMiniToolBar->hide();
    m_pComboToolBarAlignment->hide();
#else /* !VBOX_WS_MAC */
    m_pLabelMiniToolBar->setEnabled(isMachineInValidMode());
    m_pCheckBoxShowMiniToolBar->setEnabled(isMachineInValidMode());
    m_pComboToolBarAlignment->setEnabled(isMachineInValidMode() && m_pCheckBoxShowMiniToolBar->isChecked());
#endif /* !VBOX_WS_MAC */
    m_pStatusBarEditor->setEnabled(isMachineInValidMode());
}

void UIMachineSettingsInterface::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsInterface::setupUi(this);

    /* Create personal action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Runtime);
    m_pMenuBarEditor->setActionPool(m_pActionPool);

    /* Assign corresponding machine ID: */
    m_pMenuBarEditor->setMachineID(m_strMachineID);
    m_pStatusBarEditor->setMachineID(m_strMachineID);

    /* Translate finally: */
    retranslateUi();
}

void UIMachineSettingsInterface::cleanup()
{
    /* Destroy personal action-pool: */
    UIActionPool::destroy(m_pActionPool);
}

