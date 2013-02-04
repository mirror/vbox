/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsLanguage class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsLanguage_h__
#define __UIGlobalSettingsLanguage_h__

/* Local includes */
#include "UISettingsPage.h"
#include "UIGlobalSettingsLanguage.gen.h"

/* Global settings / Language page / Cache: */
struct UISettingsCacheGlobalLanguage
{
    QString m_strLanguageId;
};

/* Global settings / Language page: */
class UIGlobalSettingsLanguage : public UISettingsPageGlobal, public Ui::UIGlobalSettingsLanguage
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsLanguage();

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

    /* Reload language tree: */
    void reload(const QString &strLangId);

private slots:

    /* Routine to paint language items: */
    void sltLanguageItemPainted(QTreeWidgetItem *pItem, QPainter *pPainter);

    /* Slot to handle current language change signal: */
    void sltCurrentLanguageChanged(QTreeWidgetItem *pItem);

private:

    /* Edited flag: */
    bool m_fIsLanguageChanged;

    /* Cache: */
    UISettingsCacheGlobalLanguage m_cache;
};

#endif // __UIGlobalSettingsLanguage_h__

