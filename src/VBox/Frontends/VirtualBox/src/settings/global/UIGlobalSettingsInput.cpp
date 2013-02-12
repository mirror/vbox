/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIGlobalSettingsInput class implementation
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

/* Qt includes: */
#include <QShortcut>
#include <QHeaderView>
#include <QAbstractItemDelegate>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>

/* GUI includes: */
#include "UIGlobalSettingsInput.h"
#include "UIShortcutPool.h"
#include "UIHotKeyEditor.h"
#include "UIHostComboEditor.h"
#include "VBoxGlobalSettings.h"

/* Input page constructor: */
UIGlobalSettingsInput::UIGlobalSettingsInput()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsInput::setupUi(this);

    /* Set the new shortcut for the eraze-host-combo button: */
    m_pResetHostCombinationButton->setShortcut(QKeySequence());
    new QShortcut(QKeySequence(Qt::Key_Delete), m_pResetHostCombinationButton, SLOT(animateClick()));
    new QShortcut(QKeySequence(Qt::Key_Backspace), m_pResetHostCombinationButton, SLOT(animateClick()));

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load to cache: */
    m_cache.m_strHostCombo = m_settings.hostCombo();
    m_cache.m_fAutoCapture = m_settings.autoCapture();

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::getFromCache()
{
    /* Fetch from cache: */
    m_pHostKeyEditor->setCombo(m_cache.m_strHostCombo);
    m_pEnableAutoGrabCheckbox->setChecked(m_cache.m_fAutoCapture);
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::putToCache()
{
    /* Upload to cache: */
    m_cache.m_strHostCombo = m_pHostKeyEditor->combo().toString();
    m_cache.m_fAutoCapture = m_pEnableAutoGrabCheckbox->isChecked();
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save from cache: */
    m_settings.setHostCombo(m_cache.m_strHostCombo);
    m_settings.setAutoCapture(m_cache.m_fAutoCapture);

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Navigation stuff: */
void UIGlobalSettingsInput::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pHostKeyEditor);
    setTabOrder(m_pHostKeyEditor, m_pResetHostCombinationButton);
    setTabOrder(m_pResetHostCombinationButton, m_pEnableAutoGrabCheckbox);
}

/* Translation stuff: */
void UIGlobalSettingsInput::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsInput::retranslateUi (this);
}


UIHotKeyTableModel::UIHotKeyTableModel(QObject *pParent, UIActionPoolType type)
    : QAbstractTableModel(pParent)
    , m_type(type)
{
}

void UIHotKeyTableModel::load(const UIShortcutCache &shortcuts)
{
    /* Load shortcuts: */
    foreach (const UIShortcutCacheItem &item, shortcuts)
    {
        /* Filter out unnecessary shortcuts: */
        if ((m_type == UIActionPoolType_Selector && item.key.startsWith(GUI_Input_MachineShortcuts)) ||
            (m_type == UIActionPoolType_Runtime && item.key.startsWith(GUI_Input_SelectorShortcuts)))
            continue;
        /* Load shortcut cache item into model: */
        m_shortcuts << item;
    }
    /* Apply filter: */
    applyFilter();
    /* Notify table: */
    emit sigShortcutsLoaded();
}

void UIHotKeyTableModel::save(UIShortcutCache &shortcuts)
{
    /* Save shortcuts: */
    foreach (const UIShortcutCacheItem &item, m_shortcuts)
    {
        /* Search for corresponding cache item pointer: */
        UIShortcutCache::iterator cacheItemPointer = qFind(shortcuts.begin(), shortcuts.end(), item);
        /* Make sure the pointer is valid: */
        if (cacheItemPointer == shortcuts.end())
            continue;
        /* Save shortcut cache item into cache: */
        *cacheItemPointer = item;
    }
}

void UIHotKeyTableModel::setFilter(const QString &strFilter)
{
    m_strFilter = strFilter;
    applyFilter();
}

bool UIHotKeyTableModel::isAllShortcutsUnique()
{
    /* Enumerate all the sequences: */
    QMap<QString, QString> usedSequences;
    foreach (const UIShortcutCacheItem &item, m_shortcuts)
        if (!item.currentSequence.isEmpty())
            usedSequences.insertMulti(item.currentSequence, item.key);
    /* Enumerate all the duplicated sequences: */
    QSet<QString> duplicatedSequences;
    foreach (const QString &strKey, usedSequences.keys())
        if (usedSequences.count(strKey) > 1)
            duplicatedSequences.unite(usedSequences.values(strKey).toSet());
    /* Is there something changed? */
    if (m_duplicatedSequences != duplicatedSequences)
    {
        m_duplicatedSequences = duplicatedSequences;
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
    /* Are there duplicated shortcuts? */
    if (!m_duplicatedSequences.isEmpty())
        return false;
    /* True by default: */
    return true;
}

int UIHotKeyTableModel::rowCount(const QModelIndex& /*parent = QModelIndex()*/) const
{
    return m_filteredShortcuts.size();
}

int UIHotKeyTableModel::columnCount(const QModelIndex& /*parent = QModelIndex()*/) const
{
    return 2;
}

Qt::ItemFlags UIHotKeyTableModel::flags(const QModelIndex &index) const
{
    /* No flags for invalid index: */
    if (!index.isValid()) return Qt::NoItemFlags;
    /* Switch for different columns: */
    switch (index.column())
    {
        case UIHotKeyTableSection_Name: return Qt::ItemIsEnabled;
        case UIHotKeyTableSection_Value: return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        default: break;
    }
    /* No flags by default: */
    return Qt::NoItemFlags;
}

QVariant UIHotKeyTableModel::headerData(int iSection, Qt::Orientation orientation, int iRole /*= Qt::DisplayRole*/) const
{
    /* Switch for different roles: */
    switch (iRole)
    {
        case Qt::DisplayRole:
        {
            /* Invalid for vertical header: */
            if (orientation == Qt::Vertical) return QString();
            /* Switch for different columns: */
            switch (iSection)
            {
                case UIHotKeyTableSection_Name: return tr("Name");
                case UIHotKeyTableSection_Value: return tr("Shortcut");
                default: break;
            }
            /* Invalid for other cases: */
            return QString();
        }
        default: break;
    }
    /* Invalid by default: */
    return QVariant();
}

QVariant UIHotKeyTableModel::data(const QModelIndex &index, int iRole /*= Qt::DisplayRole*/) const
{
    /* No data for invalid index: */
    if (!index.isValid()) return QVariant();
    int iIndex = index.row();
    /* Switch for different roles: */
    switch (iRole)
    {
        case Qt::DisplayRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIHotKeyTableSection_Name: return m_filteredShortcuts[iIndex].description;
                case UIHotKeyTableSection_Value: return m_filteredShortcuts[iIndex].key == UIHostCombo::hostComboCacheKey() ?
                                                        UIHostCombo::toReadableString(m_filteredShortcuts[iIndex].currentSequence) :
                                                        m_filteredShortcuts[iIndex].currentSequence;
                default: break;
            }
            /* Invalid for other cases: */
            return QString();
        }
        case Qt::EditRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIHotKeyTableSection_Value: return m_filteredShortcuts[iIndex].key == UIHostCombo::hostComboCacheKey() ?
                                                        QVariant::fromValue(UIHostComboWrapper(m_filteredShortcuts[iIndex].currentSequence)) :
                                                        QVariant::fromValue(UIHotKey(m_filteredShortcuts[iIndex].currentSequence,
                                                                                     m_filteredShortcuts[iIndex].defaultSequence));
                default: break;
            }
            /* Invalid for other cases: */
            return QString();
        }
        case Qt::FontRole:
        {
            /* Do we have a default font? */
            QFont font(QApplication::font());
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIHotKeyTableSection_Value:
                {
                    if (m_filteredShortcuts[iIndex].key != UIHostCombo::hostComboCacheKey() &&
                        m_filteredShortcuts[iIndex].currentSequence != m_filteredShortcuts[iIndex].defaultSequence)
                        font.setBold(true);
                    break;
                }
                default: break;
            }
            /* Return resulting font: */
            return font;
        }
        case Qt::ForegroundRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIHotKeyTableSection_Value:
                {
                    if (m_duplicatedSequences.contains(m_filteredShortcuts[iIndex].key))
                        return QBrush(Qt::red);
                    break;
                }
                default: break;
            }
            /* Default for other cases: */
            return QString();
        }
        default: break;
    }
    /* Invalid by default: */
    return QVariant();
}

bool UIHotKeyTableModel::setData(const QModelIndex &index, const QVariant &value, int iRole /*= Qt::EditRole*/)
{
    /* Nothing to set for invalid index: */
    if (!index.isValid()) return false;
    /* Switch for different roles: */
    switch (iRole)
    {
        case Qt::EditRole:
        {
            /* Switch for different columns: */
            switch (index.column())
            {
                case UIHotKeyTableSection_Value:
                {
                    /* Get index: */
                    int iIndex = index.row();
                    /* Set sequence to shortcut: */
                    UIShortcutCacheItem &filteredShortcut = m_filteredShortcuts[iIndex];
                    UIShortcutCache::iterator shortcutPointer = qFind(m_shortcuts.begin(), m_shortcuts.end(), filteredShortcut);
                    if (shortcutPointer != m_shortcuts.end())
                    {
                        filteredShortcut.currentSequence = filteredShortcut.key == UIHostCombo::hostComboCacheKey() ?
                                                           value.value<UIHostComboWrapper>().toString() :
                                                           value.value<UIHotKey>().sequence();
                        *shortcutPointer = filteredShortcut;
                        emit sigRevalidationRequired();
                        return true;
                    }
                    break;
                }
                default: break;
            }
            break;
        }
        default: break;
    }
    /* Nothing to set by default: */
    return false;
}

void UIHotKeyTableModel::sort(int iColumn, Qt::SortOrder order /*= Qt::AscendingOrder*/)
{
    qStableSort(m_shortcuts.begin(), m_shortcuts.end(), UIShortcutCacheItemFunctor(iColumn, order));
    applyFilter();
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void UIHotKeyTableModel::applyFilter()
{
    /* Erase items first: */
    beginRemoveRows(QModelIndex(), 0, m_filteredShortcuts.size() - 1);
    m_filteredShortcuts.clear();
    endRemoveRows();

    /* If filter is empty: */
    if (m_strFilter.isEmpty())
    {
        /* Just add all the items: */
        m_filteredShortcuts = m_shortcuts;
    }
    else
    {
        /* Check if the description matches the filter: */
        foreach (const UIShortcutCacheItem &item, m_shortcuts)
        {
            /* If neither description nor sequence matches the filter, skip item: */
            if (!item.description.contains(m_strFilter, Qt::CaseInsensitive) &&
                !item.currentSequence.contains(m_strFilter, Qt::CaseInsensitive))
                continue;
            /* Add that item: */
            m_filteredShortcuts << item;
        }
    }
    beginInsertRows(QModelIndex(), 0, m_filteredShortcuts.size() - 1);
    endInsertRows();
}


UIHotKeyTable::UIHotKeyTable(UIHotKeyTableModel *pModel)
{
    /* Connect model: */
    setModel(pModel);
    connect(pModel, SIGNAL(sigShortcutsLoaded()), this, SLOT(sltHandleShortcutsLoaded()));

    /* Configure self: */
    setTabKeyNavigation(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked);

    /* Configure headers: */
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize((int)(verticalHeader()->minimumSectionSize() * 1.33));
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setResizeMode(UIHotKeyTableSection_Name, QHeaderView::Interactive);
    horizontalHeader()->setResizeMode(UIHotKeyTableSection_Value, QHeaderView::Stretch);

    /* Register delegate editor: */
    if (QAbstractItemDelegate *pAbstractItemDelegate = itemDelegate())
    {
        if (QStyledItemDelegate *pStyledItemDelegate = qobject_cast<QStyledItemDelegate*>(pAbstractItemDelegate))
        {
            /* Create new item editor factory: */
            QItemEditorFactory *pNewItemEditorFactory = new QItemEditorFactory;

            /* Register UIHotKeyEditor as the UIHotKey editor: */
            int iHotKeyTypeId = qRegisterMetaType<UIHotKey>();
            QStandardItemEditorCreator<UIHotKeyEditor> *pHotKeyItemEditorCreator = new QStandardItemEditorCreator<UIHotKeyEditor>();
            pNewItemEditorFactory->registerEditor((QVariant::Type)iHotKeyTypeId, pHotKeyItemEditorCreator);

            /* Register UIHostComboEditor as the UIHostComboWrapper: */
            int iHostComboTypeId = qRegisterMetaType<UIHostComboWrapper>();
            QStandardItemEditorCreator<UIHostComboEditor> *pHostComboItemEditorCreator = new QStandardItemEditorCreator<UIHostComboEditor>();
            pNewItemEditorFactory->registerEditor((QVariant::Type)iHostComboTypeId, pHostComboItemEditorCreator);

            /* Set configured item editor factory for table delegate: */
            pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);
        }
    }
}

void UIHotKeyTable::sltHandleShortcutsLoaded()
{
    /* Resize columns to feat contents: */
    resizeColumnsToContents();

    /* Configure sorting: */
    sortByColumn(UIHotKeyTableSection_Name, Qt::AscendingOrder);
    setSortingEnabled(true);
}

