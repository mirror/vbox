/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class declaration.
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

#ifndef ___UIMachineSettingsInterface_h___
#define ___UIMachineSettingsInterface_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIMachineSettingsInterface.gen.h"

/* Forward declarations: */
class UIActionPool;


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
typedef UISettingsCache<UIDataSettingsMachineInterface> UISettingsCacheMachineInterface;


/** Machine settings: User Interface page. */
class UIMachineSettingsInterface : public UISettingsPageMachine,
                                   public Ui::UIMachineSettingsInterface
{
    Q_OBJECT;

public:

    /** Constructor, early takes @a strMachineID into account for size-hint calculation. */
    UIMachineSettingsInterface(const QString strMachineID);
    /** Destructor. */
    ~UIMachineSettingsInterface();

protected:

    /** Returns whether the page content was changed. */
    bool changed() const { return m_cache.wasChanged(); }

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void loadToCacheFrom(QVariant &data);
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void getFromCache();

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    void putToCache();
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    void saveFromCacheTo(QVariant &data);

    /** Defines TAB order. */
    void setOrderAfter(QWidget *pWidget);

    /** Handles translation event. */
    void retranslateUi();

    /** Performs final page polishing. */
    void polishPage();

private:

    /** Prepare routine. */
    void prepare();

    /** Cleanup routine. */
    void cleanup();

    /* Cache: */
    UISettingsCacheMachineInterface m_cache;

    /** Holds the machine ID copy. */
    const QString m_strMachineID;
    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;
};

#endif /* !___UIMachineSettingsInterface_h___ */

