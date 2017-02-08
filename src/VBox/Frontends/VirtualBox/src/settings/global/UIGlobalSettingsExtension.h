/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsExtension class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGlobalSettingsExtension_h___
#define ___UIGlobalSettingsExtension_h___

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsExtension.gen.h"


/** Global settings: Extension page item cache structure. */
struct UISettingsCacheGlobalExtensionItem
{
    /** Holds the extension item name. */
    QString m_strName;
    /** Holds the extension item description. */
    QString m_strDescription;
    /** Holds the extension item version. */
    QString m_strVersion;
    /** Holds the extension item revision. */
    ULONG m_uRevision;
    /** Holds whether the extension item usable. */
    bool m_fIsUsable;
    /** Holds why the extension item is unusable. */
    QString m_strWhyUnusable;
};


/** Global settings: Extension page cache structure. */
struct UISettingsCacheGlobalExtension
{
    /** Holds the extension items. */
    QList<UISettingsCacheGlobalExtensionItem> m_items;
};


/** Global settings: Extension page. */
class UIGlobalSettingsExtension : public UISettingsPageGlobal, public Ui::UIGlobalSettingsExtension
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsExtension();

    static void doInstallation(QString const &strFilePath, QString const &strDigest, QWidget *pParent, QString *pstrExtPackName);

protected:

    /* Load data to cache from corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void loadToCacheFrom(QVariant &data);
    /* Load data to corresponding widgets from cache,
     * this task SHOULD be performed in GUI thread only: */
    void getFromCache();

    /* Save data from corresponding widgets to cache,
     * this task SHOULD be performed in GUI thread only: */
    void putToCache();
    /* Save data from cache to corresponding external object(s),
     * this task COULD be performed in other than GUI thread: */
    void saveFromCacheTo(QVariant &data);

    /* Helper: Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Helper: Translation stuff: */
    void retranslateUi();

private slots:

    /* Handlers: Tree-widget stuff: */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);
    void sltShowContextMenu(const QPoint &position);

    /* Handlers: Package stuff: */
    void sltInstallPackage();
    void sltRemovePackage();

private:

    /* Prepare UISettingsCacheGlobalExtensionItem basing on CExtPack: */
    UISettingsCacheGlobalExtensionItem fetchData(const CExtPack &package) const;

    /* Variables: Actions: */
    QAction *m_pActionAdd;
    QAction *m_pActionRemove;

    /* Cache: */
    UISettingsCacheGlobalExtension m_cache;
};

#endif /* !___UIGlobalSettingsExtension_h___ */

