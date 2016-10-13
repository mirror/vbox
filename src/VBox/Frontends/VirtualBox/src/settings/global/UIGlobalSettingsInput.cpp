/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsInput class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHeaderView>
# include <QAbstractItemDelegate>
# include <QItemEditorFactory>
# include <QTabWidget>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "QIStyledItemDelegate.h"
# include "QITableView.h"
# include "UIGlobalSettingsInput.h"
# include "UIShortcutPool.h"
# include "UIHotKeyEditor.h"
# include "UIHostComboEditor.h"
# include "VBoxGlobalSettings.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <QShortcut>

/* Namespaces: */
using namespace UIExtraDataDefs;


/** Column index enumerator. */
enum UIHotKeyColumnIndex
{
    UIHotKeyColumnIndex_Description,
    UIHotKeyColumnIndex_Sequence,
    UIHotKeyColumnIndex_Max
};


/** Global settings / Input page / Shortcut table cell. */
class UIShortcutCacheCell : public QITableViewCell
{
    Q_OBJECT;

public:

    /** Constructs table cell.
      * @param  pParent  Brings the row this cell belongs too.
      * @param  strText  Brings the text describing this cell. */
    UIShortcutCacheCell(QITableViewRow *pParent, const QString &strText)
        : QITableViewCell(pParent)
        , m_strText(strText)
    {}

    /** Returns the cell text. */
    virtual QString text() const /* override */ { return m_strText; }

private:

    /** Holds the cell text. */
    QString m_strText;
};


/** Global settings / Input page / Shortcut table row. */
class UIShortcutCacheRow : public QITableViewRow
{
    Q_OBJECT;

public:

    /** Constructs table row on the basis of passed arguments.
      * @param  pParent             Brings the row this cell belongs too.
      * @param  strKey              Brings the unique key inentifying held sequence.
      * @param  strDescription      Brings the deescription for the held sequence.
      * @param  strCurrentSequence  Brings the current held sequence.
      * @param  strDefaultSequence  Brings the default held sequence. */
    UIShortcutCacheRow(QITableView *pParent,
                       const QString &strKey,
                       const QString &strDescription,
                       const QString &strCurrentSequence,
                       const QString &strDefaultSequence)
        : QITableViewRow(pParent)
        , m_strKey(strKey)
        , m_strDescription(strDescription)
        , m_strCurrentSequence(strCurrentSequence)
        , m_strDefaultSequence(strDefaultSequence)
    {
        /* Create cells: */
        createCells();
    }

    /** Constructs table row on the basis of @a other one. */
    UIShortcutCacheRow(const UIShortcutCacheRow &other)
        : QITableViewRow(other.table())
        , m_strKey(other.key())
        , m_strDescription(other.description())
        , m_strCurrentSequence(other.currentSequence())
        , m_strDefaultSequence(other.defaultSequence())
    {
        /* Create cells: */
        createCells();
    }

    /** Destructs table row. */
    ~UIShortcutCacheRow()
    {
        /* Destroy cells: */
        destroyCells();
    }

    /** Copies a table row from @a other one. */
    UIShortcutCacheRow &operator=(const UIShortcutCacheRow &other)
    {
        /* Reassign variables: */
        setTable(other.table());
        m_strKey = other.key();
        m_strDescription = other.description();
        m_strCurrentSequence = other.currentSequence();
        m_strDefaultSequence = other.defaultSequence();

        /* Recreate cells: */
        destroyCells();
        createCells();

        /* Return this: */
        return *this;
    }

    /** Returns whether this row equals to @a other. */
    bool operator==(const UIShortcutCacheRow &other) const
    {
        /* Compare by the key only: */
        return m_strKey == other.key();
    }

    /** Returns the key. */
    QString key() const { return m_strKey; }
    /** Returns the description. */
    QString description() const { return m_strDescription; }
    /** Returns the current sequence. */
    QString currentSequence() const { return m_strCurrentSequence; }
    /** Returns the default sequence. */
    QString defaultSequence() const { return m_strDefaultSequence; }

    /** Defines @a strCurrentSequence. */
    void setCurrentSequence(const QString &strCurrentSequence) { m_strCurrentSequence = strCurrentSequence; }

protected:

    /** Returns the number of children. */
    virtual int childCount() const /* override */
    {
        return UIHotKeyColumnIndex_Max;
    }

    /** Returns the child item with @a iIndex. */
    virtual QITableViewCell *childItem(int iIndex) const /* override */
    {
        switch (iIndex)
        {
            case UIHotKeyColumnIndex_Description: return m_cells.first;
            case UIHotKeyColumnIndex_Sequence: return m_cells.second;
            default: break;
        }
        return 0;
    }

private:

    /** Creates cells. */
    void createCells()
    {
        /* Create cells on the basis of description and current sequence: */
        m_cells = qMakePair(new UIShortcutCacheCell(this, m_strDescription),
                            new UIShortcutCacheCell(this, m_strCurrentSequence));
    }

    /** Destroys cells. */
    void destroyCells()
    {
        /* Destroy cells: */
        delete m_cells.first;
        delete m_cells.second;
        m_cells.first = 0;
        m_cells.second = 0;
    }

    /** Holds the key. */
    QString m_strKey;
    /** Holds the description. */
    QString m_strDescription;
    /** Holds the current sequence. */
    QString m_strCurrentSequence;
    /** Holds the default sequence. */
    QString m_strDefaultSequence;

    /** Holds the cell instances. */
    QPair<UIShortcutCacheCell*, UIShortcutCacheCell*> m_cells;
};


/** Global settings / Input page / Cache / Shortcut cache. */
typedef QList<UIShortcutCacheRow> UIShortcutCache;


/** Global settings / Input page / Cache. */
class UISettingsCacheGlobalInput : public QObject
{
    Q_OBJECT;

public:

    /** Constructs cache passing @a pParent to the base-class. */
    UISettingsCacheGlobalInput(QObject *pParent)
        : QObject(pParent)
        , m_fAutoCapture(false)
    {}

    /** Returns the shortcuts cache [full access]. */
    UIShortcutCache &shortcuts() { return m_shortcuts; }

    /** Defines whether the keyboard auto-capture is @a fEnabled. */
    void setAutoCapture(bool fEnabled) { m_fAutoCapture = fEnabled; }
    /** Returns whether the keyboard auto-capture is enabled. */
    bool autoCapture() const { return m_fAutoCapture; }

private:

    /** Holds the shortcut cache. */
    UIShortcutCache m_shortcuts;

    /** Holds whether the keyboard auto-capture is enabled. */
    bool m_fAutoCapture;
};


/** Global settings / Input page / Cache / Shortcut cache item sort functor. */
class UIShortcutCacheItemFunctor
{
public:

    /** Constructs cache sorting functor.
      * @param  iColumn  Brings the column sorting should be done according to.
      * @param  m_order  Brings the sorting order to be applied. */
    UIShortcutCacheItemFunctor(int iColumn, Qt::SortOrder order)
        : m_iColumn(iColumn)
        , m_order(order)
    {}

    /** Returns whether the @a item1 is more/less than the @a item2.
      * @note  Order depends on the one set through constructor, stored in m_order. */
    bool operator()(const UIShortcutCacheRow &item1, const UIShortcutCacheRow &item2)
    {
        switch (m_iColumn)
        {
            case UIHotKeyColumnIndex_Description:
                return m_order == Qt::AscendingOrder ? item1.description() < item2.description() : item1.description() > item2.description();
            case UIHotKeyColumnIndex_Sequence:
                return m_order == Qt::AscendingOrder ? item1.currentSequence() < item2.currentSequence() : item1.currentSequence() > item2.currentSequence();
            default: break;
        }
        return m_order == Qt::AscendingOrder ? item1.key() < item2.key() : item1.key() > item2.key();
    }

private:

    /** Holds the column sorting should be done according to. */
    int m_iColumn;
    /** Holds the sorting order to be applied. */
    Qt::SortOrder m_order;
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
class UIHotKeyTable : public QITableView
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIHotKeyTable(QWidget *pParent, UIHotKeyTableModel *pModel, const QString &strObjectName);

private slots:

    /* Handler: Readiness stuff: */
    void sltHandleShortcutsLoaded();

private:

    /** Prepares all. */
    void prepare();
};


/*********************************************************************************************************************************
*   Class UIHotKeyTableModel implementation.                                                                                     *
*********************************************************************************************************************************/

UIHotKeyTableModel::UIHotKeyTableModel(QObject *pParent, UIActionPoolType type)
    : QAbstractTableModel(pParent)
    , m_type(type)
{
}

void UIHotKeyTableModel::load(const UIShortcutCache &shortcuts)
{
    /* Load shortcuts: */
    foreach (const UIShortcutCacheRow &item, shortcuts)
    {
        /* Filter out unnecessary shortcuts: */
        if ((m_type == UIActionPoolType_Selector && item.key().startsWith(GUI_Input_MachineShortcuts)) ||
            (m_type == UIActionPoolType_Runtime && item.key().startsWith(GUI_Input_SelectorShortcuts)))
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
    /* Save model items: */
    foreach (const UIShortcutCacheRow &item, m_shortcuts)
    {
        /* Search for corresponding cache item index: */
        int iIndexOfCacheItem = shortcuts.indexOf(item);
        /* Make sure index is valid: */
        if (iIndexOfCacheItem == -1)
            continue;
        /* Save model item into the cache: */
        shortcuts[iIndexOfCacheItem] = item;
    }
}

bool UIHotKeyTableModel::isAllShortcutsUnique()
{
    /* Enumerate all the sequences: */
    QMap<QString, QString> usedSequences;
    foreach (const UIShortcutCacheRow &item, m_shortcuts)
        if (!item.currentSequence().isEmpty())
            usedSequences.insertMulti(item.currentSequence(), item.key());
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

void UIHotKeyTableModel::sltHandleFilterTextChange(const QString &strText)
{
    m_strFilter = strText;
    applyFilter();
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
        case UIHotKeyColumnIndex_Description: return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        case UIHotKeyColumnIndex_Sequence: return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
        default: break;
    }
    /* No flags by default: */
    return Qt::NoItemFlags;
}

QVariant UIHotKeyTableModel::headerData(int iSection, Qt::Orientation orientation, int iRole /* = Qt::DisplayRole*/) const
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
                case UIHotKeyColumnIndex_Description: return tr("Name");
                case UIHotKeyColumnIndex_Sequence: return tr("Shortcut");
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

QVariant UIHotKeyTableModel::data(const QModelIndex &index, int iRole /* = Qt::DisplayRole*/) const
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
                case UIHotKeyColumnIndex_Description:
                {
                    /* Return shortcut description: */
                    return m_filteredShortcuts[iIndex].description();
                }
                case UIHotKeyColumnIndex_Sequence:
                {
                    /* If that is host-combo cell: */
                    if (m_filteredShortcuts[iIndex].key() == UIHostCombo::hostComboCacheKey())
                        /* We should return host-combo: */
                        return UIHostCombo::toReadableString(m_filteredShortcuts[iIndex].currentSequence());
                    /* In other cases we should return hot-combo: */
                    QString strHotCombo = m_filteredShortcuts[iIndex].currentSequence();
                    /* But if that is machine table and hot-combo is not empty: */
                    if (m_type == UIActionPoolType_Runtime && !strHotCombo.isEmpty())
                        /* We should prepend it with Host+ prefix: */
                        strHotCombo.prepend(UIHostCombo::hostComboModifierName());
                    /* Return what we've got: */
                    return strHotCombo;
                }
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
                case UIHotKeyColumnIndex_Sequence: return m_filteredShortcuts[iIndex].key() == UIHostCombo::hostComboCacheKey() ?
                                                          QVariant::fromValue(UIHostComboWrapper(m_filteredShortcuts[iIndex].currentSequence())) :
                                                          QVariant::fromValue(UIHotKey(m_type == UIActionPoolType_Runtime ?
                                                                                       UIHotKeyType_Simple : UIHotKeyType_WithModifiers,
                                                                                       m_filteredShortcuts[iIndex].currentSequence(),
                                                                                       m_filteredShortcuts[iIndex].defaultSequence()));
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
                case UIHotKeyColumnIndex_Sequence:
                {
                    if (m_filteredShortcuts[iIndex].key() != UIHostCombo::hostComboCacheKey() &&
                        m_filteredShortcuts[iIndex].currentSequence() != m_filteredShortcuts[iIndex].defaultSequence())
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
                case UIHotKeyColumnIndex_Sequence:
                {
                    if (m_duplicatedSequences.contains(m_filteredShortcuts[iIndex].key()))
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

bool UIHotKeyTableModel::setData(const QModelIndex &index, const QVariant &value, int iRole /* = Qt::EditRole*/)
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
                case UIHotKeyColumnIndex_Sequence:
                {
                    /* Get index: */
                    int iIndex = index.row();
                    /* Set sequence to shortcut: */
                    UIShortcutCacheRow &filteredShortcut = m_filteredShortcuts[iIndex];
                    int iShortcutIndex = m_shortcuts.indexOf(filteredShortcut);
                    if (iShortcutIndex != -1)
                    {
                        filteredShortcut.setCurrentSequence(filteredShortcut.key() == UIHostCombo::hostComboCacheKey() ?
                                                            value.value<UIHostComboWrapper>().toString() :
                                                            value.value<UIHotKey>().sequence());
                        m_shortcuts[iShortcutIndex] = filteredShortcut;
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

void UIHotKeyTableModel::sort(int iColumn, Qt::SortOrder order /* = Qt::AscendingOrder*/)
{
    /* Sort whole the list: */
    qStableSort(m_shortcuts.begin(), m_shortcuts.end(), UIShortcutCacheItemFunctor(iColumn, order));
    /* Make sure host-combo item is always the first one: */
    UIShortcutCacheRow fakeHostComboItem(0, UIHostCombo::hostComboCacheKey(), QString(), QString(), QString());
    int iIndexOfHostComboItem = m_shortcuts.indexOf(fakeHostComboItem);
    if (iIndexOfHostComboItem != -1)
    {
        UIShortcutCacheRow hostComboItem = m_shortcuts.takeAt(iIndexOfHostComboItem);
        m_shortcuts.prepend(hostComboItem);
    }
    /* Apply the filter: */
    applyFilter();
    /* Notify the model: */
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void UIHotKeyTableModel::applyFilter()
{
    /* Erase items first if necessary: */
    if (!m_filteredShortcuts.isEmpty())
    {
        beginRemoveRows(QModelIndex(), 0, m_filteredShortcuts.size() - 1);
        m_filteredShortcuts.clear();
        endRemoveRows();
    }

    /* If filter is empty: */
    if (m_strFilter.isEmpty())
    {
        /* Just add all the items: */
        m_filteredShortcuts = m_shortcuts;
    }
    else
    {
        /* Check if the description matches the filter: */
        foreach (const UIShortcutCacheRow &item, m_shortcuts)
        {
            /* If neither description nor sequence matches the filter, skip item: */
            if (!item.description().contains(m_strFilter, Qt::CaseInsensitive) &&
                !item.currentSequence().contains(m_strFilter, Qt::CaseInsensitive))
                continue;
            /* Add that item: */
            m_filteredShortcuts << item;
        }
    }

    /* Add items finally if necessary: */
    if (!m_filteredShortcuts.isEmpty())
    {
        beginInsertRows(QModelIndex(), 0, m_filteredShortcuts.size() - 1);
        endInsertRows();
    }
}


/*********************************************************************************************************************************
*   Class UIHotKeyTable implementation.                                                                                          *
*********************************************************************************************************************************/

UIHotKeyTable::UIHotKeyTable(QWidget *pParent, UIHotKeyTableModel *pModel, const QString &strObjectName)
    : QITableView(pParent)
{
    /* Set object name: */
    setObjectName(strObjectName);
    /* Set model: */
    setModel(pModel);

    /* Prepare all: */
    prepare();
}

void UIHotKeyTable::sltHandleShortcutsLoaded()
{
    /* Resize columns to feat contents: */
    resizeColumnsToContents();

    /* Configure sorting: */
    sortByColumn(UIHotKeyColumnIndex_Description, Qt::AscendingOrder);
    setSortingEnabled(true);
}

void UIHotKeyTable::prepare()
{
    /* Configure self: */
    setTabKeyNavigation(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked);

    /* Configure headers: */
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize((int)(verticalHeader()->minimumSectionSize() * 1.33));
    horizontalHeader()->setStretchLastSection(false);
#if QT_VERSION >= 0x050000
    horizontalHeader()->setSectionResizeMode(UIHotKeyColumnIndex_Description, QHeaderView::Interactive);
    horizontalHeader()->setSectionResizeMode(UIHotKeyColumnIndex_Sequence, QHeaderView::Stretch);
#else /* QT_VERSION < 0x050000 */
    horizontalHeader()->setResizeMode(UIHotKeyColumnIndex_Description, QHeaderView::Interactive);
    horizontalHeader()->setResizeMode(UIHotKeyColumnIndex_Sequence, QHeaderView::Stretch);
#endif /* QT_VERSION < 0x050000 */

    /* Connect model: */
    connect(model(), SIGNAL(sigShortcutsLoaded()), this, SLOT(sltHandleShortcutsLoaded()));

    /* Check if we do have proper item delegate: */
    QIStyledItemDelegate *pStyledItemDelegate = qobject_cast<QIStyledItemDelegate*>(itemDelegate());
    AssertPtrReturnVoid(pStyledItemDelegate);
    {
        /* Configure item delegate: */
        pStyledItemDelegate->setWatchForEditorDataCommits(true);

        /* Create new item editor factory: */
        QItemEditorFactory *pNewItemEditorFactory = new QItemEditorFactory;
        AssertPtrReturnVoid(pNewItemEditorFactory);
        {
            /* Register UIHotKeyEditor as the UIHotKey editor: */
            int iHotKeyTypeId = qRegisterMetaType<UIHotKey>();
            QStandardItemEditorCreator<UIHotKeyEditor> *pHotKeyItemEditorCreator = new QStandardItemEditorCreator<UIHotKeyEditor>();
            pNewItemEditorFactory->registerEditor((QVariant::Type)iHotKeyTypeId, pHotKeyItemEditorCreator);

            /* Register UIHostComboEditor as the UIHostComboWrapper editor: */
            int iHostComboTypeId = qRegisterMetaType<UIHostComboWrapper>();
            QStandardItemEditorCreator<UIHostComboEditor> *pHostComboItemEditorCreator = new QStandardItemEditorCreator<UIHostComboEditor>();
            pNewItemEditorFactory->registerEditor((QVariant::Type)iHostComboTypeId, pHostComboItemEditorCreator);

            /* Assign configured item editor factory to item delegate: */
            pStyledItemDelegate->setItemEditorFactory(pNewItemEditorFactory);
        }
    }
}


/*********************************************************************************************************************************
*   Class UIGlobalSettingsInput implementation.                                                                                  *
*********************************************************************************************************************************/

/* Input page constructor: */
UIGlobalSettingsInput::UIGlobalSettingsInput()
    : m_pTabWidget(0)
    , m_pSelectorFilterEditor(0), m_pSelectorModel(0), m_pSelectorTable(0)
    , m_pMachineFilterEditor(0), m_pMachineModel(0), m_pMachineTable(0)
    , m_pCache(new UISettingsCacheGlobalInput(this))
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsInput::setupUi(this);

    /* Create tab widget: */
    m_pTabWidget = new QTabWidget(this);
    m_pTabWidget->setMinimumWidth(400);
    m_pMainLayout->addWidget(m_pTabWidget, 0, 0, 1, 2);

    /* Create selector tab: */
    QWidget *pSelectorTab = new QWidget;
    m_pTabWidget->insertTab(UIHotKeyTableIndex_Selector, pSelectorTab, QString());
    m_pSelectorFilterEditor = new QLineEdit(pSelectorTab);
    m_pSelectorModel = new UIHotKeyTableModel(this, UIActionPoolType_Selector);
    m_pSelectorTable = new UIHotKeyTable(pSelectorTab, m_pSelectorModel, "m_pSelectorTable");
    connect(m_pSelectorFilterEditor, SIGNAL(textChanged(const QString &)),
            m_pSelectorModel, SLOT(sltHandleFilterTextChange(const QString &)));
    QVBoxLayout *pSelectorLayout = new QVBoxLayout(pSelectorTab);
#ifndef VBOX_WS_WIN
    /* On Windows host that looks ugly, but
     * On Mac OS X and X11 that deserves it's place. */
    pSelectorLayout->setContentsMargins(0, 0, 0, 0);
#endif /* !VBOX_WS_WIN */
    pSelectorLayout->setSpacing(1);
    pSelectorLayout->addWidget(m_pSelectorFilterEditor);
    pSelectorLayout->addWidget(m_pSelectorTable);
    setTabOrder(m_pTabWidget, m_pSelectorFilterEditor);
    setTabOrder(m_pSelectorFilterEditor, m_pSelectorTable);

    /* Create machine tab: */
    QWidget *pMachineTab = new QWidget;
    m_pTabWidget->insertTab(UIHotKeyTableIndex_Machine, pMachineTab, QString());
    m_pMachineFilterEditor = new QLineEdit(pMachineTab);
    m_pMachineModel = new UIHotKeyTableModel(this, UIActionPoolType_Runtime);
    m_pMachineTable = new UIHotKeyTable(pMachineTab, m_pMachineModel, "m_pMachineTable");
    connect(m_pMachineFilterEditor, SIGNAL(textChanged(const QString &)),
            m_pMachineModel, SLOT(sltHandleFilterTextChange(const QString &)));
    QVBoxLayout *pMachineLayout = new QVBoxLayout(pMachineTab);
#ifndef VBOX_WS_WIN
    /* On Windows host that looks ugly, but
     * On Mac OS X and X11 that deserves it's place. */
    pMachineLayout->setContentsMargins(0, 0, 0, 0);
#endif /* !VBOX_WS_WIN */
    pMachineLayout->setSpacing(1);
    pMachineLayout->addWidget(m_pMachineFilterEditor);
    pMachineLayout->addWidget(m_pMachineTable);
    setTabOrder(m_pSelectorTable, m_pMachineFilterEditor);
    setTabOrder(m_pMachineFilterEditor, m_pMachineTable);

    /* In the VM process we start by displaying the machine tab: */
    if (VBoxGlobal::instance()->isVMConsoleProcess())
        m_pTabWidget->setCurrentWidget(pMachineTab);

    /* Prepare validation: */
    prepareValidation();

    /* Apply language settings: */
    retranslateUi();
}

/* Load data to cache from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Load host-combo shortcut to cache: */
    m_pCache->shortcuts() << UIShortcutCacheRow(m_pMachineTable, UIHostCombo::hostComboCacheKey(), tr("Host Key Combination"),  m_settings.hostCombo(), QString());
    /* Load all other shortcuts to cache: */
    const QMap<QString, UIShortcut>& shortcuts = gShortcutPool->shortcuts();
    const QList<QString> shortcutKeys = shortcuts.keys();
    foreach (const QString &strShortcutKey, shortcutKeys)
    {
        const UIShortcut &shortcut = shortcuts[strShortcutKey];
        QITableView *pParent = strShortcutKey.startsWith(GUI_Input_MachineShortcuts) ? m_pMachineTable :
                               strShortcutKey.startsWith(GUI_Input_SelectorShortcuts) ? m_pSelectorTable : 0;
        AssertPtr(pParent);
        m_pCache->shortcuts() << UIShortcutCacheRow(pParent, strShortcutKey, VBoxGlobal::removeAccelMark(shortcut.description()),
                                                    shortcut.sequence().toString(QKeySequence::NativeText),
                                                    shortcut.defaultSequence().toString(QKeySequence::NativeText));
    }
    /* Load other things to cache: */
    m_pCache->setAutoCapture(m_settings.autoCapture());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::getFromCache()
{
    /* Fetch from cache: */
    m_pSelectorModel->load(m_pCache->shortcuts());
    m_pMachineModel->load(m_pCache->shortcuts());
    m_pEnableAutoGrabCheckbox->setChecked(m_pCache->autoCapture());

    /* Revalidate: */
    revalidate();
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIGlobalSettingsInput::putToCache()
{
    /* Upload to cache: */
    m_pSelectorModel->save(m_pCache->shortcuts());
    m_pMachineModel->save(m_pCache->shortcuts());
    m_pCache->setAutoCapture(m_pEnableAutoGrabCheckbox->isChecked());
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIGlobalSettingsInput::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties & settings: */
    UISettingsPageGlobal::fetchData(data);

    /* Save host-combo shortcut from cache: */
    UIShortcutCacheRow fakeHostComboItem(0, UIHostCombo::hostComboCacheKey(), QString(), QString(), QString());
    int iIndexOfHostComboItem = m_pCache->shortcuts().indexOf(fakeHostComboItem);
    if (iIndexOfHostComboItem != -1)
        m_settings.setHostCombo(m_pCache->shortcuts()[iIndexOfHostComboItem].currentSequence());
    /* Iterate over cached shortcuts: */
    QMap<QString, QString> sequences;
    foreach (const UIShortcutCacheRow &item, m_pCache->shortcuts())
        sequences.insert(item.key(), item.currentSequence());
    /* Save shortcut sequences from cache: */
    gShortcutPool->setOverrides(sequences);
    /* Save other things from cache: */
    m_settings.setAutoCapture(m_pCache->autoCapture());

    /* Upload properties & settings to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsInput::validate(QList<UIValidationMessage> &messages)
{
    /* Pass by default: */
    bool fPass = true;

    /* Check VirtualBox Manager page for unique shortcuts: */
    if (!m_pSelectorModel->isAllShortcutsUnique())
    {
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(UIHotKeyTableIndex_Selector));
        message.second << tr("Some items have the same shortcuts assigned.");
        messages << message;
        fPass = false;
    }

    /* Check Virtual Machine page for unique shortcuts: */
    if (!m_pMachineModel->isAllShortcutsUnique())
    {
        UIValidationMessage message;
        message.first = VBoxGlobal::removeAccelMark(m_pTabWidget->tabText(UIHotKeyTableIndex_Machine));
        message.second << tr("Some items have the same shortcuts assigned.");
        messages << message;
        fPass = false;
    }

    /* Return result: */
    return fPass;
}

/* Navigation stuff: */
void UIGlobalSettingsInput::setOrderAfter(QWidget *pWidget)
{
    setTabOrder(pWidget, m_pTabWidget);
    setTabOrder(m_pMachineTable, m_pEnableAutoGrabCheckbox);
}

/* Translation stuff: */
void UIGlobalSettingsInput::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsInput::retranslateUi(this);

    /* Translate tab-widget labels: */
    m_pTabWidget->setTabText(UIHotKeyTableIndex_Selector, tr("&VirtualBox Manager"));
    m_pTabWidget->setTabText(UIHotKeyTableIndex_Machine, tr("Virtual &Machine"));
    m_pSelectorTable->setWhatsThis(tr("Lists all available shortcuts which can be configured."));
    m_pMachineTable->setWhatsThis(tr("Lists all available shortcuts which can be configured."));
    m_pSelectorFilterEditor->setWhatsThis(tr("Holds a sequence to filter the shortcut list."));
    m_pMachineFilterEditor->setWhatsThis(tr("Holds a sequence to filter the shortcut list."));
}

void UIGlobalSettingsInput::prepareValidation()
{
    /* Prepare validation: */
    connect(m_pSelectorModel, SIGNAL(sigRevalidationRequired()), this, SLOT(revalidate()));
    connect(m_pMachineModel, SIGNAL(sigRevalidationRequired()), this, SLOT(revalidate()));
}

# include "UIGlobalSettingsInput.moc"

