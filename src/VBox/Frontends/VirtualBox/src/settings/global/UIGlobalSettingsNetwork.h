/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsNetwork class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGlobalSettingsNetwork_h___
#define ___UIGlobalSettingsNetwork_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsNetwork.gen.h"

/* Forward declarations: */
class UIItemNetworkNAT;
class UIItemNetworkHost;
struct UIDataSettingsGlobalNetwork;
struct UIDataSettingsGlobalNetworkNAT;
struct UIDataSettingsGlobalNetworkHost;
typedef UISettingsCache<UIDataSettingsGlobalNetwork> UISettingsCacheGlobalNetwork;


/** Global settings: Network page. */
class UIGlobalSettingsNetwork : public UISettingsPageGlobal,
                                public Ui::UIGlobalSettingsNetwork
{
    Q_OBJECT;

public:

    /** Constructs Network settings page. */
    UIGlobalSettingsNetwork();
    /** Destructs Network settings page. */
    ~UIGlobalSettingsNetwork();

protected:

    /** Loads data into the cache from corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void loadToCacheFrom(QVariant &data) /* override */;
    /** Loads data into corresponding widgets from the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void getFromCache() /* override */;

    /** Saves data from corresponding widgets to the cache,
      * this task SHOULD be performed in the GUI thread only. */
    virtual void putToCache() /* override */;
    /** Saves data from the cache to corresponding external object(s),
      * this task COULD be performed in other than the GUI thread. */
    virtual void saveFromCacheTo(QVariant &data) /* overrride */;

    /** Performs validation, updates @a messages list if something is wrong. */
    virtual bool validate(QList<UIValidationMessage> &messages) /* override */;

    /** Defines TAB order for passed @a pWidget. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles command to add NAT network. */
    void sltAddNetworkNAT();
    /** Handles command to edit NAT network. */
    void sltEditNetworkNAT();
    /** Handles command to remove NAT network. */
    void sltRemoveNetworkNAT();
    /** Handles @a pChangedItem change for NAT network tree. */
    void sltHandleItemChangeNetworkNAT(QTreeWidgetItem *pChangedItem);
    /** Handles NAT network tree current item change. */
    void sltHandleCurrentItemChangeNetworkNAT();
    /** Handles context menu request for @a position of NAT network tree. */
    void sltHandleContextMenuRequestNetworkNAT(const QPoint &position);

    /** Handles command to add host network. */
    void sltAddNetworkHost();
    /** Handles command to edit host network. */
    void sltEditNetworkHost();
    /** Handles command to remove host network. */
    void sltRemoveNetworkHost();
    /** Handles host network tree current item change. */
    void sltHandleCurrentItemChangeNetworkHost();
    /** Handles context menu request for @a position of host network tree. */
    void sltHandleContextMenuRequestNetworkHost(const QPoint &position);

private:

    /** Uploads NAT @a network data into passed @a data storage unit. */
    void loadDataNetworkNAT(const CNATNetwork &network, UIDataSettingsGlobalNetworkNAT &data);
    /** Saves @a data to corresponding NAT network. */
    void saveDataNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data);
    /** Creates a new item in the NAT network tree on the basis of passed @a data, @a fChooseItem if requested. */
    void createTreeItemNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data, bool fChooseItem = false);
    /** Removes existing @a pItem from the NAT network tree. */
    void removeTreeItemNetworkNAT(UIItemNetworkNAT *pItem);

    /** Uploads host @a network data into passed @a data storage unit. */
    void loadDataNetworkHost(const CHostNetworkInterface &iface, UIDataSettingsGlobalNetworkHost &data);
    /** Saves @a data to corresponding host network. */
    void saveDataNetworkHost(const UIDataSettingsGlobalNetworkHost &data);
    /** Creates a new item in the host network tree on the basis of passed @a data, @a fChooseItem if requested. */
    void createTreeItemNetworkHost(const UIDataSettingsGlobalNetworkHost &data, bool fChooseItem = false);
    /** Removes existing @a pItem from the host network tree. */
    void removeTreeItemNetworkHost(UIItemNetworkHost *pItem);

    /** Holds the Add NAT network action instance. */
    QAction *m_pActionAddNetworkNAT;
    /** Holds the Edit NAT network action instance. */
    QAction *m_pActionEditNetworkNAT;
    /** Holds the Remove NAT network action instance. */
    QAction *m_pActionRemoveNetworkNAT;

    /** Holds the Add host network action instance. */
    QAction *m_pActionAddNetworkHost;
    /** Holds the Edit host network action instance. */
    QAction *m_pActionEditNetworkHost;
    /** Holds the Remove host network action instance. */
    QAction *m_pActionRemoveNetworkHost;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalNetwork *m_pCache;
};

#endif /* !___UIGlobalSettingsNetwork_h___ */

