/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsExtension class declaration
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsExtension_h__
#define __UIGlobalSettingsExtension_h__

/* Local includes */
#include "UISettingsPage.h"
#include "UIGlobalSettingsExtension.gen.h"

/* Global settings / Extension page / Cache Item: */
struct UISettingsCacheGlobalExtensionItem
{
    QString m_strName;
    QString m_strDescription;
    QString m_strVersion;
    ULONG m_strRevision;
    bool m_fIsUsable;
    QString m_strWhyUnusable;
};

/* Global settings / Extension page / Cache: */
struct UISettingsCacheGlobalExtension
{
    QList<UISettingsCacheGlobalExtensionItem> m_items;
};

/* Global settings / Extension page: */
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

    /* Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Translation stuff: */
    void retranslateUi();

private slots:

    /* Tree-widget slots: */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);
    void sltShowContextMenu(const QPoint &position);

    /* Package add/remove procedures: */
    void sltInstallPackage();
    void sltRemovePackage();

private:

    /* Prepare UISettingsCacheGlobalExtensionItem basing on CExtPack: */
    UISettingsCacheGlobalExtensionItem fetchData(const CExtPack &package) const;

    /* Actions: */
    QAction *m_pActionAdd;
    QAction *m_pActionRemove;

    /* Cache: */
    UISettingsCacheGlobalExtension m_cache;
};

#endif // __UIGlobalSettingsExtension_h__

