/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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
# include "UIActionPool.h"
# include "UIExtraDataManager.h"
# include "UIMachineSettingsInterface.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: User Interface page data structure. */
struct UIDataSettingsMachineInterface
{
    /** Constructs data. */
    UIDataSettingsMachineInterface()
        : m_fStatusBarEnabled(false)
#ifndef VBOX_WS_MAC
        , m_fMenuBarEnabled(false)
#endif /* !VBOX_WS_MAC */
        , m_restrictionsOfMenuBar(UIExtraDataMetaDefs::MenuType_Invalid)
        , m_restrictionsOfMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType_Invalid)
        , m_restrictionsOfMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType_Invalid)
        , m_restrictionsOfMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType_Invalid)
        , m_restrictionsOfMenuInput(UIExtraDataMetaDefs::RuntimeMenuInputActionType_Invalid)
        , m_restrictionsOfMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType_Invalid)
#ifdef VBOX_WITH_DEBUGGER_GUI
        , m_restrictionsOfMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType_Invalid)
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
        , m_restrictionsOfMenuWindow(UIExtraDataMetaDefs::MenuWindowActionType_Invalid)
#endif /* VBOX_WS_MAC */
        , m_restrictionsOfMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType_Invalid)
#ifndef VBOX_WS_MAC
        , m_fShowMiniToolBar(false)
        , m_fMiniToolBarAtTop(false)
#endif /* !VBOX_WS_MAC */
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsMachineInterface &other) const
    {
        return true
               && (m_fStatusBarEnabled == other.m_fStatusBarEnabled)
               && (m_statusBarRestrictions == other.m_statusBarRestrictions)
               && (m_statusBarOrder == other.m_statusBarOrder)
#ifndef VBOX_WS_MAC
               && (m_fMenuBarEnabled == other.m_fMenuBarEnabled)
#endif /* !VBOX_WS_MAC */
               && (m_restrictionsOfMenuBar == other.m_restrictionsOfMenuBar)
               && (m_restrictionsOfMenuApplication == other.m_restrictionsOfMenuApplication)
               && (m_restrictionsOfMenuMachine == other.m_restrictionsOfMenuMachine)
               && (m_restrictionsOfMenuView == other.m_restrictionsOfMenuView)
               && (m_restrictionsOfMenuInput == other.m_restrictionsOfMenuInput)
               && (m_restrictionsOfMenuDevices == other.m_restrictionsOfMenuDevices)
#ifdef VBOX_WITH_DEBUGGER_GUI
               && (m_restrictionsOfMenuDebug == other.m_restrictionsOfMenuDebug)
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
               && (m_restrictionsOfMenuWindow == other.m_restrictionsOfMenuWindow)
#endif /* VBOX_WS_MAC */
               && (m_restrictionsOfMenuHelp == other.m_restrictionsOfMenuHelp)
#ifndef VBOX_WS_MAC
               && (m_fShowMiniToolBar == other.m_fShowMiniToolBar)
               && (m_fMiniToolBarAtTop == other.m_fMiniToolBarAtTop)
#endif /* !VBOX_WS_MAC */
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsMachineInterface &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsMachineInterface &other) const { return !equal(other); }

    /** Holds whether the status-bar is enabled. */
    bool                  m_fStatusBarEnabled;
    /** Holds the status-bar indicator restrictions. */
    QList<IndicatorType>  m_statusBarRestrictions;
    /** Holds the status-bar indicator order. */
    QList<IndicatorType>  m_statusBarOrder;

#ifndef VBOX_WS_MAC
    /** Holds whether the menu-bar is enabled. */
    bool                                                m_fMenuBarEnabled;
#endif /* !VBOX_WS_MAC */
    /** Holds the menu-bar menu restrictions. */
    UIExtraDataMetaDefs::MenuType                       m_restrictionsOfMenuBar;
    /** Holds the Application menu restrictions. */
    UIExtraDataMetaDefs::MenuApplicationActionType      m_restrictionsOfMenuApplication;
    /** Holds the Machine menu restrictions. */
    UIExtraDataMetaDefs::RuntimeMenuMachineActionType   m_restrictionsOfMenuMachine;
    /** Holds the View menu restrictions. */
    UIExtraDataMetaDefs::RuntimeMenuViewActionType      m_restrictionsOfMenuView;
    /** Holds the Input menu restrictions. */
    UIExtraDataMetaDefs::RuntimeMenuInputActionType     m_restrictionsOfMenuInput;
    /** Holds the Devices menu restrictions. */
    UIExtraDataMetaDefs::RuntimeMenuDevicesActionType   m_restrictionsOfMenuDevices;
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Holds the Debug menu restrictions. */
    UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType  m_restrictionsOfMenuDebug;
#endif /* VBOX_WITH_DEBUGGER_GUI */
#ifdef VBOX_WS_MAC
    /** Holds the Window menu restrictions. */
    UIExtraDataMetaDefs::MenuWindowActionType           m_restrictionsOfMenuWindow;
#endif /* VBOX_WS_MAC */
    /** Holds the Help menu restrictions. */
    UIExtraDataMetaDefs::MenuHelpActionType             m_restrictionsOfMenuHelp;

#ifndef VBOX_WS_MAC
    /** Holds whether the mini-toolbar is enabled. */
    bool  m_fShowMiniToolBar;
    /** Holds whether the mini-toolbar should be aligned at top of screen. */
    bool  m_fMiniToolBarAtTop;
#endif /* !VBOX_WS_MAC */
};


UIMachineSettingsInterface::UIMachineSettingsInterface(const QString strMachineID)
    : m_strMachineID(strMachineID)
    , m_pActionPool(0)
    , m_pCache(new UISettingsCacheMachineInterface)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsInterface::~UIMachineSettingsInterface()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsInterface::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsInterface::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

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
    m_pCache->cacheInitialData(interfaceData);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsInterface::getFromCache()
{
    /* Get interface data from cache: */
    const UIDataSettingsMachineInterface &interfaceData = m_pCache->base();

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
    UIDataSettingsMachineInterface interfaceData = m_pCache->base();

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
    m_pCache->cacheCurrentData(interfaceData);
}

void UIMachineSettingsInterface::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Make sure machine is in valid mode & interface data was changed: */
    if (isMachineInValidMode() && m_pCache->wasChanged())
    {
        /* Get interface data from cache: */
        const UIDataSettingsMachineInterface &interfaceData = m_pCache->data();

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

    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

