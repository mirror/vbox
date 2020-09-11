/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsLanguage class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsLanguage_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsLanguage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"

/* Forward declartions: */
class QILabelSeparator;
class QIRichTextLabel;
class QITreeWidget;
class QTreeWidgetItem;
struct UIDataSettingsGlobalLanguage;
typedef UISettingsCache<UIDataSettingsGlobalLanguage> UISettingsCacheGlobalLanguage;

/** Global settings: Language page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsLanguage : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs Language settings page. */
    UIGlobalSettingsLanguage();
    /** Destructs Language settings page. */
    ~UIGlobalSettingsLanguage();

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

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Performs final page polishing. */
    virtual void polishEvent(QShowEvent *pEvent) /* override */;

private slots:

    /** Handles @a pItem painting with passed @a pPainter. */
    void sltHandleItemPainting(QTreeWidgetItem *pItem, QPainter *pPainter);

    /** Handles @a pCurrentItem change. */
    void sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnection();
    /** Cleanups all. */
    void cleanup();

    /** Reloads language list, choosing item with @a strLanguageId as current. */
    void reloadLanguageTree(const QString &strLanguageId);

    /** Saves existing language data from the cache. */
    bool saveLanguageData();

    /** Holds whether the page is polished. */
    bool  m_fPolished;

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalLanguage *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the tree-widget instance. */
        QITreeWidget     *m_pTreeWidget;
        /** Holds the separator label instance. */
        QILabelSeparator *m_pLabelSeparator;
        /** Holds the info label instance. */
        QIRichTextLabel  *m_pLabelInfo;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsLanguage_h */
