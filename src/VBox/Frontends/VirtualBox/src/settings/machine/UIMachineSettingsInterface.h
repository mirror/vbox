/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsInterface class declaration.
 */

/*
 * Copyright (C) 2008-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsInterface_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsInterface_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QCheckBox;
class QLabel;
class UIActionPool;
class UIMenuBarEditorWidget;
class UIStatusBarEditorWidget;
class UIVisualStateEditor;
struct UIDataSettingsMachineInterface;
typedef UISettingsCache<UIDataSettingsMachineInterface> UISettingsCacheMachineInterface;

/** Machine settings: User Interface page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsInterface : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    /** Constructs User Interface settings page. */
    UIMachineSettingsInterface(const QUuid &uMachineId);
    /** Destructs User Interface settings page. */
    ~UIMachineSettingsInterface();

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const /* override */;

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() /* override */;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs final page polishing. */
    virtual void polishPage() /* override */;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing interface data from the cache. */
    bool saveInterfaceData();
    /** Saves existing 'Menu-bar' data from the cache. */
    bool saveMenuBarData();
    /** Saves existing 'Status-bar' data from the cache. */
    bool saveStatusBarData();
    /** Saves existing 'Mini-toolbar' data from the cache. */
    bool saveMiniToolbarData();
    /** Saves existing 'Visual State' data from the cache. */
    bool saveVisualStateData();

    /** Holds the machine ID copy. */
    const QUuid    m_uMachineId;
    /** Holds the action-pool instance. */
    UIActionPool  *m_pActionPool;

    /** Holds the page data cache instance. */
    UISettingsCacheMachineInterface *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the menu-bar editor instance. */
        UIMenuBarEditorWidget   *m_pEditorMenuBar;
        /** Holds the visual state label instance. */
        QLabel                  *m_pLabelVisualState;
        /** Holds the visual state label instance. */
        UIVisualStateEditor     *m_pEditorVisualState;
        /** Holds the mini-toolbar label instance. */
        QLabel                  *m_pLabelMiniToolBar;
        /** Holds the 'show mini-toolbar' check-box instance. */
        QCheckBox               *m_pCheckBoxShowMiniToolBar;
        /** Holds the 'mini-toolbar alignment' check-box instance. */
        QCheckBox               *m_pCheckBoxMiniToolBarAlignment;
        /** Holds the status-bar editor instance. */
        UIStatusBarEditorWidget *m_pEditorStatusBar;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsInterface_h */
