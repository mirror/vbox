/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetwork class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsNetwork_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsNetwork_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIPortForwardingTable.h"

/* Forward declarations: */
class QILabelSeparator;
class QITreeWidget;
class QTreeWidgetItem;
class UIItemNetworkNAT;
class UIToolBar;
struct UIDataSettingsGlobalNetwork;
struct UIDataSettingsGlobalNetworkNAT;
typedef UISettingsCache<UIDataPortForwardingRule> UISettingsCachePortForwardingRule;
typedef UISettingsCachePoolOfTwo<UIDataSettingsGlobalNetworkNAT, UISettingsCachePortForwardingRule, UISettingsCachePortForwardingRule> UISettingsCacheGlobalNetworkNAT;
typedef UISettingsCachePool<UIDataSettingsGlobalNetwork, UISettingsCacheGlobalNetworkNAT> UISettingsCacheGlobalNetwork;

/** Global settings: Network page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsNetwork : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs Network settings page. */
    UIGlobalSettingsNetwork();
    /** Destructs Network settings page. */
    ~UIGlobalSettingsNetwork();

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

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles command to add NAT network. */
    void sltAddNATNetwork();
    /** Handles command to remove NAT network. */
    void sltRemoveNATNetwork();
    /** Handles command to edit NAT network. */
    void sltEditNATNetwork();

    /** Handles @a pChangedItem change for NAT network tree. */
    void sltHandleItemChangeNATNetwork(QTreeWidgetItem *pChangedItem);
    /** Handles NAT network tree current item change. */
    void sltHandleCurrentItemChangeNATNetwork();
    /** Handles context menu request for @a position of NAT network tree. */
    void sltHandleContextMenuRequestNATNetwork(const QPoint &position);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares NAT network tree-widget. */
    void prepareNATNetworkTreeWidget();
    /** Prepares NAT network toolbar. */
    void prepareNATNetworkToolbar();
    /** Prepares connections. */
    void prepareConnections();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing network data from the cache. */
    bool saveNetworkData();

    /** Uploads NAT @a network data into passed @a cache storage unit. */
    void loadToCacheFromNATNetwork(const CNATNetwork &network, UISettingsCacheGlobalNetworkNAT &cache);
    /** Removes corresponding NAT network on the basis of @a cache. */
    bool removeNATNetwork(const UISettingsCacheGlobalNetworkNAT &cache);
    /** Creates corresponding NAT network on the basis of @a cache. */
    bool createNATNetwork(const UISettingsCacheGlobalNetworkNAT &cache);
    /** Updates @a cache of corresponding NAT network. */
    bool updateNATNetwork(const UISettingsCacheGlobalNetworkNAT &cache);
    /** Creates a new item in the NAT network tree on the basis of passed @a cache. */
    void createTreeWidgetItemForNATNetwork(const UISettingsCacheGlobalNetworkNAT &cache);
    /** Creates a new item in the NAT network tree on the basis of passed
      * @a data, @a ipv4rules, @a ipv6rules, @a fChooseItem if requested. */
    void createTreeWidgetItemForNATNetwork(const UIDataSettingsGlobalNetworkNAT &data,
                                           const UIPortForwardingDataList &ipv4rules,
                                           const UIPortForwardingDataList &ipv6rules,
                                           bool fChooseItem = false);
    /** Removes existing @a pItem from the NAT network tree. */
    void removeTreeWidgetItemOfNATNetwork(UIItemNetworkNAT *pItem);
    /** Returns whether the NAT network described by the @a cache could be updated or recreated otherwise. */
    bool isNetworkCouldBeUpdated(const UISettingsCacheGlobalNetworkNAT &cache) const;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalNetwork *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the separator label instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the NAT networt layout instance. */
        QHBoxLayout      *m_pLayoutNATNetwork;
        /** Holds the NAT networt tree-widget instance. */
        QITreeWidget     *m_pTreeWidgetNATNetwork;
        /** Holds the NAT networt toolbar instance. */
        UIToolBar        *m_pToolbarNATNetwork;
        /** Holds the 'add NAT network' action instance. */
        QAction          *m_pActionAdd;
        /** Holds the 'remove NAT network' action instance. */
        QAction          *m_pActionRemove;
        /** Holds the 'edit NAT network' action instance. */
        QAction          *m_pActionEdit;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsNetwork_h */
