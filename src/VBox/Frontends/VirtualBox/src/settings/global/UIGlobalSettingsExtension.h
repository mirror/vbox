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


/** Global settings: Extension page item data structure. */
struct UIDataSettingsGlobalExtensionItem
{
    /** Constructs data. */
    UIDataSettingsGlobalExtensionItem()
        : m_strName(QString())
        , m_strDescription(QString())
        , m_strVersion(QString())
        , m_uRevision(0)
        , m_fIsUsable(false)
        , m_strWhyUnusable(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalExtensionItem &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_strDescription == other.m_strDescription)
               && (m_strVersion == other.m_strVersion)
               && (m_uRevision == other.m_uRevision)
               && (m_fIsUsable == other.m_fIsUsable)
               && (m_strWhyUnusable == other.m_strWhyUnusable)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalExtensionItem &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalExtensionItem &other) const { return !equal(other); }

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


/** Global settings: Extension page data structure. */
struct UIDataSettingsGlobalExtension
{
    /** Constructs data. */
    UIDataSettingsGlobalExtension()
        : m_items(QList<UIDataSettingsGlobalExtensionItem>())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalExtension &other) const
    {
        return true
               && (m_items == other.m_items)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalExtension &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalExtension &other) const { return !equal(other); }

    /** Holds the extension items. */
    QList<UIDataSettingsGlobalExtensionItem> m_items;
};
typedef UISettingsCache<UIDataSettingsGlobalExtension> UISettingsCacheGlobalExtension;


/** Global settings: Extension page. */
class UIGlobalSettingsExtension : public UISettingsPageGlobal, public Ui::UIGlobalSettingsExtension
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsExtension();

    static void doInstallation(QString const &strFilePath, QString const &strDigest, QWidget *pParent, QString *pstrExtPackName);

protected:

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

private slots:

    /* Handlers: Tree-widget stuff: */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);
    void sltShowContextMenu(const QPoint &position);

    /* Handlers: Package stuff: */
    void sltInstallPackage();
    void sltRemovePackage();

private:

    /* Prepare UIDataSettingsGlobalExtensionItem basing on CExtPack: */
    UIDataSettingsGlobalExtensionItem fetchData(const CExtPack &package) const;

    /* Variables: Actions: */
    QAction *m_pActionAdd;
    QAction *m_pActionRemove;

    /* Cache: */
    UISettingsCacheGlobalExtension m_cache;
};

#endif /* !___UIGlobalSettingsExtension_h___ */

