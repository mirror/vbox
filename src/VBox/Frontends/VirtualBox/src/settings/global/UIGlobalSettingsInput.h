/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsInput class declaration
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIGlobalSettingsInput_h__
#define __UIGlobalSettingsInput_h__

/* Qt includes: */
#include <QAbstractTableModel>
#include <QTableView>

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIGlobalSettingsInput.gen.h"
#include "UIActionPool.h"

/* Forward declartions: */
class QTabWidget;
class QLineEdit;
class UIHotKeyTableModel;
class UIHotKeyTable;

/* Global settings / Input page / Cache / Shortcut cache item: */
struct UIShortcutCacheItem
{
    UIShortcutCacheItem(const QString &strKey,
                        const QString &strDescription,
                        const QString &strCurrentSequence,
                        const QString &strDefaultSequence)
        : key(strKey)
        , description(strDescription)
        , currentSequence(strCurrentSequence)
        , defaultSequence(strDefaultSequence)
    {}

    UIShortcutCacheItem(const UIShortcutCacheItem &other)
        : key(other.key)
        , description(other.description)
        , currentSequence(other.currentSequence)
        , defaultSequence(other.defaultSequence)
    {}

    UIShortcutCacheItem& operator=(const UIShortcutCacheItem &other)
    {
        key = other.key;
        description = other.description;
        currentSequence = other.currentSequence;
        defaultSequence = other.defaultSequence;
        return *this;
    }

    bool operator==(const UIShortcutCacheItem &other) const
    {
        return key == other.key;
    }

    QString key;
    QString description;
    QString currentSequence;
    QString defaultSequence;
};

/* Global settings / Input page / Cache / Shortcut cache item sort functor: */
class UIShortcutCacheItemFunctor
{
public:

    UIShortcutCacheItemFunctor(int iColumn, Qt::SortOrder order)
        : m_iColumn(iColumn)
        , m_order(order)
    {
    }

    bool operator()(const UIShortcutCacheItem &item1, const UIShortcutCacheItem &item2)
    {
        switch (m_iColumn)
        {
            case 0: return m_order == Qt::AscendingOrder ? item1.description < item2.description : item1.description > item2.description;
            case 1: return m_order == Qt::AscendingOrder ? item1.currentSequence < item2.currentSequence : item1.currentSequence > item2.currentSequence;
            default: break;
        }
        return m_order == Qt::AscendingOrder ? item1.key < item2.key : item1.key > item2.key;
    }

private:

    int m_iColumn;
    Qt::SortOrder m_order;
};

/* Global settings / Input page / Cache / Shortcut cache: */
typedef QList<UIShortcutCacheItem> UIShortcutCache;

/* Global settings / Input page / Cache: */
struct UISettingsCacheGlobalInput
{
    UIShortcutCache m_shortcuts;
    bool m_fAutoCapture;
};

/* Global settings / Input page: */
class UIGlobalSettingsInput : public UISettingsPageGlobal, public Ui::UIGlobalSettingsInput
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIGlobalSettingsInput();

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

    /* Validation stuff: */
    void setValidator(QIWidgetValidator *pValidator);
    bool revalidate(QString &strWarning, QString &strTitle);

    /* Navigation stuff: */
    void setOrderAfter(QWidget *pWidget);

    /* Translation stuff: */
    void retranslateUi();

private:

    /* Cache: */
    QIWidgetValidator *m_pValidator;
    UISettingsCacheGlobalInput m_cache;
    QTabWidget *m_pTabWidget;
    UIHotKeyTableModel *m_pSelectorModel;
    UIHotKeyTable *m_pSelectorTable;
    QLineEdit *m_pSelectorFilterEditor;
    UIHotKeyTableModel *m_pMachineModel;
    UIHotKeyTable *m_pMachineTable;
    QLineEdit *m_pMachineFilterEditor;
};

/* Hot-key table indexes: */
enum UIHotKeyTableIndex
{
    UIHotKeyTableIndex_Selector = 0,
    UIHotKeyTableIndex_Machine = 1
};

/* Hot-key table field indexes: */
enum UIHotKeyTableSection
{
    UIHotKeyTableSection_Name = 0,
    UIHotKeyTableSection_Value = 1
};

/* A model representing hot-key combination table: */
class UIHotKeyTableModel : public QAbstractTableModel
{
    Q_OBJECT;

signals:

    /* Notifier: Readiness stuff: */
    void sigShortcutsLoaded();

    /* Notifier: Validation stuff: */
    void sigRevalidationRequired();

public:

    /* Constructor: */
    UIHotKeyTableModel(QObject *pParent, UIActionPoolType type);

    /* API: Loading/saving stuff: */
    void load(const UIShortcutCache &shortcuts);
    void save(UIShortcutCache &shortcuts);

    /* API: Validation stuff: */
    bool isAllShortcutsUnique();

private slots:

    /* Handler: Filtering stuff: */
    void sltHandleFilterTextChange(const QString &strText);

private:

    /* Internal API: Size stuff: */
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    /* Internal API: Data stuff: */
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int iSection, Qt::Orientation orientation, int iRole = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int iRole = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int iRole = Qt::EditRole);

    /* Internal API: Sorting stuff: */
    void sort(int iColumn, Qt::SortOrder order = Qt::AscendingOrder);

    /* Helper: Filtering stuff: */
    void applyFilter();

    /* Variables: */
    UIActionPoolType m_type;
    QString m_strFilter;
    UIShortcutCache m_shortcuts;
    UIShortcutCache m_filteredShortcuts;
    QSet<QString> m_duplicatedSequences;
};

/* A table reflecting hot-key combinations: */
class UIHotKeyTable : public QTableView
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIHotKeyTable(QWidget *pParent, UIHotKeyTableModel *pModel);

private slots:

    /* Handler: Readiness stuff: */
    void sltHandleShortcutsLoaded();
};

#endif // __UIGlobalSettingsInput_h__

