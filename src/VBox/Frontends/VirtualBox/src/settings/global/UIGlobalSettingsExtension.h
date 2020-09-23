/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsExtension class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsExtension_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsExtension_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declarations: */
class QHBoxLayout;
class QITreeWidget;
class QTreeWidgetItem;
class QILabelSeparator;
class QIToolBar;
struct UIDataSettingsGlobalExtension;
struct UIDataSettingsGlobalExtensionItem;
typedef UISettingsCache<UIDataSettingsGlobalExtension> UISettingsCacheGlobalExtension;

/** Global settings: Extension page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsExtension : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs Extension settings page. */
    UIGlobalSettingsExtension();
    /** Destructs Extension settings page. */
    ~UIGlobalSettingsExtension();

protected:

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

private slots:

    /** Handles @a pCurrentItem change. */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);
    /** Handles context menu request for @a position. */
    void sltHandleContextMenuRequest(const QPoint &position);

    /** Handles command to add extension pack. */
    void sltAddPackage();
    /** Handles command to remove extension pack. */
    void sltRemovePackage();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares tree-widget. */
    void prepareTreeWidget();
    /** Prepares toolbar. */
    void prepareToolbar();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Uploads @a package data into passed @a item. */
    void loadData(const CExtPack &package, UIDataSettingsGlobalExtensionItem &item) const;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalExtension *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the separator label instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the packages layout instance. */
        QHBoxLayout      *m_pLayoutPackages;
        /** Holds the tree-widget instance. */
        QITreeWidget     *m_pTreeWidget;
        /** Holds the toolbar instance. */
        QIToolBar        *m_pToolbar;
        /** Holds the 'add package' action instance. */
        QAction          *m_pActionAdd;
        /** Holds the 'remove package' action instance. */
        QAction          *m_pActionRemove;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsExtension_h */
