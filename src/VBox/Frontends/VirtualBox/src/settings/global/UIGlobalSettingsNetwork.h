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

    /** Defines TAB order. */
    virtual void setOrderAfter(QWidget *pWidget) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /* Handlers: NAT network stuff: */
    void sltAddNetworkNAT();
    void sltEditNetworkNAT();
    void sltRemoveNetworkNAT();
    void sltHandleItemChangeNetworkNAT(QTreeWidgetItem *pChangedItem);
    void sltHandleCurrentItemChangeNetworkNAT();
    void sltHandleContextMenuRequestNetworkNAT(const QPoint &position);

    /* Handlers: Host network stuff: */
    void sltAddNetworkHost();
    void sltEditNetworkHost();
    void sltRemoveNetworkHost();
    void sltHandleCurrentItemChangeNetworkHost();
    void sltHandleContextMenuRequestNetworkHost(const QPoint &position);

private:

    /* Helpers: NAT network cache stuff: */
    void loadDataNetworkNAT(const CNATNetwork &network, UIDataSettingsGlobalNetworkNAT &data);
    void saveDataNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data);

    /* Helpers: NAT network tree stuff: */
    void createTreeItemNetworkNAT(const UIDataSettingsGlobalNetworkNAT &data, bool fChooseItem = false);
    void removeTreeItemNetworkNAT(UIItemNetworkNAT *pItem);

    /* Helpers: Host network cache stuff: */
    void loadDataNetworkHost(const CHostNetworkInterface &iface, UIDataSettingsGlobalNetworkHost &data);
    void saveDataNetworkHost(const UIDataSettingsGlobalNetworkHost &data);

    /* Helpers: Host network tree stuff: */
    void createTreeItemNetworkHost(const UIDataSettingsGlobalNetworkHost &data, bool fChooseItem = false);
    void removeTreeItemNetworkHost(UIItemNetworkHost *pItem);

    /* Variables: NAT network actions: */
    QAction *m_pActionAddNetworkNAT;
    QAction *m_pActionEditNetworkNAT;
    QAction *m_pActionRemoveNetworkNAT;

    /* Variables: Host network actions: */
    QAction *m_pActionAddNetworkHost;
    QAction *m_pActionEditNetworkHost;
    QAction *m_pActionRemoveNetworkHost;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalNetwork *m_pCache;
};

#endif /* !___UIGlobalSettingsNetwork_h___ */

