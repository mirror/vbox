/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsSelector class implementation.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QAbstractItemModel>
#include <QAccessibleWidget>
#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QItemDelegate>
#include <QLayout>
#include <QPainter>
#include <QTabWidget>
#include <QToolButton>

/* GUI includes: */
#include "QITabWidget.h"
#include "QITreeView.h"
#include "QITreeWidget.h"
#include "UISettingsSelector.h"
#include "UIIconPool.h"
#include "UIImageTools.h"
#include "UISettingsPage.h"
#include "QIToolBar.h"


/** QAccessibleWidget extension used as an accessibility interface for UISettingsSelectorToolBar buttons. */
class UIAccessibilityInterfaceForUISettingsSelectorToolBarButton : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating toolbar button accessibility interface: */
        if (   pObject
            && strClassname == QLatin1String("QToolButton")
            && pObject->property("Belongs to") == "UISettingsSelectorToolBar")
            return new UIAccessibilityInterfaceForUISettingsSelectorToolBarButton(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    UIAccessibilityInterfaceForUISettingsSelectorToolBarButton(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::Button)
    {}

    /** Returns the role. */
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
        /* Make sure button still alive: */
        AssertPtrReturn(button(), QAccessible::NoRole);

        /* Return role for checkable button: */
        if (button()->isCheckable())
            return QAccessible::RadioButton;

        /* Return default role: */
        return QAccessible::Button;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
    {
        /* Prepare the button state: */
        QAccessible::State state;

        /* Make sure button still alive: */
        AssertPtrReturn(button(), state);

        /* Compose the button state: */
        state.checkable = button()->isCheckable();
        state.checked = button()->isChecked();

        /* Return the button state: */
        return state;
    }

private:

    /** Returns corresponding toolbar button. */
    QToolButton *button() const { return qobject_cast<QToolButton*>(widget()); }
};


/** QITreeViewItem subclass used as selector tree-view item. */
class UISelectorTreeViewItem : public QITreeViewItem
{
    Q_OBJECT;

public:

    /** Constructs selector item passing @a pParent to the base-class.
      * This is constructor for root-item only. */
    UISelectorTreeViewItem(QITreeView *pParent);
    /** Constructs selector item passing @a pParentItem to the base-class.
      * This is constructor for rest of items we need.
      * @param  iID      Brings the item ID.
      * @param  icon     Brings the item icon.
      * @param  strLink  Brings the item link. */
    UISelectorTreeViewItem(UISelectorTreeViewItem *pParentItem,
                           int iID,
                           const QIcon &icon,
                           const QString &strLink);

    /** Returns item ID. */
    int id() const { return m_iID; }
    /** Returns item icon. */
    QIcon icon() const { return m_icon; }
    /** Returns item link. */
    QString link() const { return m_strLink; }

    /** Returns item text. */
    virtual QString text() const RT_OVERRIDE { return m_strText; }
    /** Defines item @a strText. */
    virtual void setText(const QString &strText) { m_strText = strText; }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE { return m_children.size(); }
    /** Returns the child item with @a iIndex. */
    virtual UISelectorTreeViewItem *childItem(int iIndex) const RT_OVERRIDE { return m_children.at(iIndex); }

    /** Adds @a pChild to this item. */
    void addChild(UISelectorTreeViewItem *pChild) { m_children << pChild; }
    /** Assigns @a children for this item. */
    void setChildren(const QList<UISelectorTreeViewItem*> &children) { m_children = children; }
    /** Returns position for the passed @a pChild. */
    int posOfChild(UISelectorTreeViewItem *pChild) const { return m_children.indexOf(pChild); }

    /** Searches for the child with @a iID specified. */
    UISelectorTreeViewItem *childItemById(int iID) const;
    /** Searches for the child with @a strLink specified. */
    UISelectorTreeViewItem *childItemByLink(const QString &strLink) const;

private:

    /** Holds the item ID. */
    int      m_iID;
    /** Holds the item icon. */
    QIcon    m_icon;
    /** Holds the item link. */
    QString  m_strLink;
    /** Holds the item text. */
    QString  m_strText;

    /** Holds the list of children. */
    QList<UISelectorTreeViewItem*>  m_children;
};


/** QItemDelegate subclass used as selector tree-view item delegate. */
class UISelectorDelegate : public QItemDelegate
{
    Q_OBJECT;

public:

    /** Constructs selector item delegate passing @a pParent to the base-class. */
    UISelectorDelegate(QObject *pParent);

private:

    /** Paints @a index item with specified @a option using specified @a pPainter. */
    void paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


/** QAbstractItemModel subclass used as selector model. */
class UISelectorModel : public QAbstractItemModel
{
    Q_OBJECT;

public:

    /** Data roles. */
    enum DataRole
    {
        R_Margin = Qt::UserRole + 1,
        R_Spacing,
        R_IconSize,

        R_ItemId,
        R_ItemPixmap,
        R_ItemPixmapRect,
        R_ItemName,
        R_ItemNamePoint,
    };

    /** Constructs selector model passing @a pParent to the base-class. */
    UISelectorModel(QITreeView *pParent);
    /** Destructs selector model. */
    virtual ~UISelectorModel() RT_OVERRIDE;

    /** Returns row count for the passed @a parentIndex. */
    virtual int rowCount(const QModelIndex &parentIndex = QModelIndex()) const RT_OVERRIDE;
    /** Returns column count for the passed @a parentIndex. */
    virtual int columnCount(const QModelIndex &parentIndex = QModelIndex()) const RT_OVERRIDE;

    /** Returns parent item of @a specifiedIndex item. */
    virtual QModelIndex parent(const QModelIndex &specifiedIndex) const RT_OVERRIDE;
    /** Returns item specified by @a iRow, @a iColum and @a parentIndex. */
    virtual QModelIndex index(int iRow, int iColumn, const QModelIndex &parentIndex = QModelIndex()) const RT_OVERRIDE;
    /** Returns root item. */
    QModelIndex root() const;

    /** Returns model data for @a specifiedIndex and @a iRole. */
    virtual QVariant data(const QModelIndex &specifiedIndex, int iRole) const RT_OVERRIDE;
    /** Defines model data for @a specifiedIndex and @a iRole as @a value. */
    virtual bool setData(const QModelIndex &specifiedIndex, const QVariant &value, int iRole) RT_OVERRIDE;

    /** Adds item with certain @a strId. */
    QModelIndex addItem(int iID, const QIcon &icon, const QString &strLink);
    /** Deletes item with certain @a iID. */
    void delItem(int iID);

    /** Returns item with certain @a iID. */
    QModelIndex findItem(int iID);
    /** Returns item with certain @a strLink. */
    QModelIndex findItem(const QString &strLink);

    /** Sorts the contents of model by @a iColumn and @a enmOrder. */
    virtual void sort(int iColumn = 0, Qt::SortOrder enmOrder = Qt::AscendingOrder) RT_OVERRIDE;

private:

    /** Returns model flags for @a specifiedIndex. */
    virtual Qt::ItemFlags flags(const QModelIndex &specifiedIndex) const RT_OVERRIDE;

    /** Returns a pointer to item containing within @a specifiedIndex. */
    static UISelectorTreeViewItem *indexToItem(const QModelIndex &specifiedIndex);

    /** Holds the parent tree-view reference. */
    QITreeView *m_pParentTree;

    /** Holds the root item instance. */
    UISelectorTreeViewItem *m_pRootItem;
};


/** QITreeView extension used as selector tree-view. */
class UISelectorTreeView : public QITreeView
{
    Q_OBJECT;

public:

    /** Constructs selector tree-view passing @a pParent to the base-class. */
    UISelectorTreeView(QWidget *pParent);

    /** Calculates widget minimum size-hint. */
    virtual QSize minimumSizeHint() const RT_OVERRIDE;
    /** Calculates widget size-hint. */
    virtual QSize sizeHint() const RT_OVERRIDE;

private:

    /** Prepares all. */
    void prepare();
};


/** Tree-widget column sections. */
enum TreeWidgetSection
{
    TreeWidgetSection_Category = 0,
    TreeWidgetSection_Id,
    TreeWidgetSection_Link,
    TreeWidgetSection_Max
};


/** Simple container of all the selector item data. */
class UISelectorItem
{
public:

    /** Constructs selector item.
      * @param  icon       Brings the item icon.
      * @param  iID        Brings the item ID.
      * @param  strLink    Brings the item link.
      * @param  pPage      Brings the item page reference.
      * @param  iParentID  Brings the item parent ID. */
    UISelectorItem(const QIcon &icon, int iID, const QString &strLink, UISettingsPage *pPage, int iParentID)
        : m_icon(icon)
        , m_iID(iID)
        , m_strLink(strLink)
        , m_pPage(pPage)
        , m_iParentID(iParentID)
    {}

    /** Destructs selector item. */
    virtual ~UISelectorItem() {}

    /** Returns the item icon. */
    QIcon icon() const { return m_icon; }
    /** Returns the item text. */
    QString text() const { return m_strText; }
    /** Defines the item @a strText. */
    void setText(const QString &strText) { m_strText = strText; }
    /** Returns the item ID. */
    int id() const { return m_iID; }
    /** Returns the item link. */
    QString link() const { return m_strLink; }
    /** Returns the item page reference. */
    UISettingsPage *page() const { return m_pPage; }
    /** Returns the item parent ID. */
    int parentID() const { return m_iParentID; }

protected:

    /** Holds the item icon. */
    QIcon m_icon;
    /** Holds the item text. */
    QString m_strText;
    /** Holds the item ID. */
    int m_iID;
    /** Holds the item link. */
    QString m_strLink;
    /** Holds the item page reference. */
    UISettingsPage *m_pPage;
    /** Holds the item parent ID. */
    int m_iParentID;
};


/** UISelectorItem subclass used as tab-widget selector item. */
class UISelectorActionItem : public UISelectorItem
{
public:

    /** Constructs selector item.
      * @param  icon       Brings the item icon.
      * @param  iID        Brings the item ID.
      * @param  strLink    Brings the item link.
      * @param  pPage      Brings the item page reference.
      * @param  iParentID  Brings the item parent ID.
      * @param  pParent    Brings the item parent. */
    UISelectorActionItem(const QIcon &icon, int iID, const QString &strLink, UISettingsPage *pPage, int iParentID, QObject *pParent)
        : UISelectorItem(icon, iID, strLink, pPage, iParentID)
        , m_pAction(new QAction(icon, QString(), pParent))
        , m_pTabWidget(0)
    {
        m_pAction->setCheckable(true);
    }

    /** Returns the action instance. */
    QAction *action() const { return m_pAction; }

    /** Defines the @a pTabWidget instance. */
    void setTabWidget(QTabWidget *pTabWidget) { m_pTabWidget = pTabWidget; }
    /** Returns the tab-widget instance. */
    QTabWidget *tabWidget() const { return m_pTabWidget; }

protected:

    /** Holds the action instance. */
    QAction *m_pAction;
    /** Holds the tab-widget instance. */
    QTabWidget *m_pTabWidget;
};


/*********************************************************************************************************************************
*   Class UISelectorTreeViewItem implementation.                                                                                 *
*********************************************************************************************************************************/

UISelectorTreeViewItem::UISelectorTreeViewItem(QITreeView *pParent)
    : QITreeViewItem(pParent)
{
}

UISelectorTreeViewItem::UISelectorTreeViewItem(UISelectorTreeViewItem *pParentItem,
                                               int iID,
                                               const QIcon &icon,
                                               const QString &strLink)
    : QITreeViewItem(pParentItem)
    , m_iID(iID)
    , m_icon(icon)
    , m_strLink(strLink)
{
    if (pParentItem)
        pParentItem->addChild(this);
}

UISelectorTreeViewItem *UISelectorTreeViewItem::childItemById(int iID) const
{
    for (int i = 0; i < childCount(); ++i)
        if (UISelectorTreeViewItem *pItem = childItem(i))
            if (pItem->id() == iID)
                return pItem;
    return 0;
}

UISelectorTreeViewItem *UISelectorTreeViewItem::childItemByLink(const QString &strLink) const
{
    for (int i = 0; i < childCount(); ++i)
        if (UISelectorTreeViewItem *pItem = childItem(i))
            if (pItem->link() == strLink)
                return pItem;
    return 0;
}


/*********************************************************************************************************************************
*   Class UISelectorDelegate implementation.                                                                                     *
*********************************************************************************************************************************/

UISelectorDelegate::UISelectorDelegate(QObject *pParent)
    : QItemDelegate(pParent)
{
}

void UISelectorDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    /* Sanity checks: */
    AssertPtrReturnVoid(pPainter);
    AssertReturnVoid(index.isValid());

    /* Acquire model: */
    const UISelectorModel *pModel = qobject_cast<const UISelectorModel*>(index.model());

    /* Acquire item properties: */
    const QPalette palette = QGuiApplication::palette();
    const QRect itemRectangle = option.rect;
    const QStyle::State enmState = option.state;
    const bool fChosen = enmState & QStyle::State_Selected;

    /* Draw background: */
    QColor backColor;
    if (fChosen)
    {
        /* Prepare painter path: */
        QPainterPath painterPath;
        painterPath.lineTo(itemRectangle.width() - itemRectangle.height(), 0);
        painterPath.lineTo(itemRectangle.width(),                          itemRectangle.height());
        painterPath.lineTo(0,                                              itemRectangle.height());
        painterPath.closeSubpath();
        painterPath.translate(itemRectangle.topLeft());

        /* Prepare painting gradient: */
        backColor = palette.color(QPalette::Active, QPalette::Highlight);
        const QColor bcTone1 = backColor.lighter(100);
        const QColor bcTone2 = backColor.lighter(120);
        QLinearGradient grad(itemRectangle.topLeft(), itemRectangle.bottomRight());
        grad.setColorAt(0, bcTone1);
        grad.setColorAt(1, bcTone2);

        /* Paint fancy shape: */
        pPainter->save();
        pPainter->setClipPath(painterPath);
        pPainter->setPen(backColor);
        pPainter->fillRect(itemRectangle, grad);
        pPainter->restore();
    }
    else
    {
        /* Just init painting color: */
        backColor = palette.color(QPalette::Active, QPalette::Window);
    }

    /* Draw icon: */
    const QRect itemPixmapRect = pModel->data(index, UISelectorModel::R_ItemPixmapRect).toRect();
    const QPixmap itemPixmap = pModel->data(index, UISelectorModel::R_ItemPixmap).value<QPixmap>();
    pPainter->save();
    pPainter->translate(itemRectangle.topLeft());
    pPainter->drawPixmap(itemPixmapRect, itemPixmap);
    pPainter->restore();

    /* Draw name: */
    const QColor foreground = suitableForegroundColor(palette, backColor);
    const QFont font = pModel->data(index, Qt::FontRole).value<QFont>();
    const QPoint namePoint = pModel->data(index, UISelectorModel::R_ItemNamePoint).toPoint();
    const QString strName = pModel->data(index, UISelectorModel::R_ItemName).toString();
    pPainter->save();
    pPainter->translate(itemRectangle.topLeft());
    pPainter->setPen(foreground);
    pPainter->setFont(font);
    pPainter->drawText(namePoint, strName);
    pPainter->restore();
}


/*********************************************************************************************************************************
*   Class UISelectorModel implementation.                                                                                        *
*********************************************************************************************************************************/

UISelectorModel::UISelectorModel(QITreeView *pParentTree)
    : QAbstractItemModel(pParentTree)
    , m_pParentTree(pParentTree)
    , m_pRootItem(new UISelectorTreeViewItem(pParentTree))
{
}

UISelectorModel::~UISelectorModel()
{
    delete m_pRootItem;
}

int UISelectorModel::rowCount(const QModelIndex &parentIndex) const
{
    UISelectorTreeViewItem *pParentItem = indexToItem(parentIndex);
    return pParentItem ? pParentItem->childCount() : 1 /* root item */;
}

int UISelectorModel::columnCount(const QModelIndex & /* parentIndex */) const
{
    return 1;
}

QModelIndex UISelectorModel::parent(const QModelIndex &specifiedIndex) const
{
    if (!specifiedIndex.isValid())
        return QModelIndex();

    UISelectorTreeViewItem *pItem = indexToItem(specifiedIndex);
    UISelectorTreeViewItem *pParentOfItem =
        pItem ? qobject_cast<UISelectorTreeViewItem*>(pItem->parentItem()) : 0;
    UISelectorTreeViewItem *pParentOfParent =
        pParentOfItem ? qobject_cast<UISelectorTreeViewItem*>(pParentOfItem->parentItem()) : 0;
    const int iParentPosition = pParentOfParent ? pParentOfParent->posOfChild(pParentOfItem) : 0;

    return pParentOfItem ? createIndex(iParentPosition, 0, pParentOfItem) : QModelIndex();
}

QModelIndex UISelectorModel::index(int iRow, int iColumn, const QModelIndex &parentIndex) const
{
    if (!hasIndex(iRow, iColumn, parentIndex))
        return QModelIndex();

    UISelectorTreeViewItem *pItem = !parentIndex.isValid()
                                  ? m_pRootItem
                                  : indexToItem(parentIndex)->childItem(iRow);

    return pItem ? createIndex(iRow, iColumn, pItem) : QModelIndex();
}

QModelIndex UISelectorModel::root() const
{
    return index(0, 0);
}

QVariant UISelectorModel::data(const QModelIndex &specifiedIndex, int iRole) const
{
    if (!specifiedIndex.isValid())
        return QVariant();

    switch (iRole)
    {
        /* Basic Attributes: */
        case Qt::FontRole:
        {
            QFont font = m_pParentTree->font();
            font.setBold(true);
            return font;
        }
        case Qt::SizeHintRole:
        {
            const QFontMetrics fm(data(specifiedIndex, Qt::FontRole).value<QFont>());
            const int iMargin = data(specifiedIndex, R_Margin).toInt();
            const int iSpacing = data(specifiedIndex, R_Spacing).toInt();
            const int iIconSize = data(specifiedIndex, R_IconSize).toInt();
            const QString strName = data(specifiedIndex, R_ItemName).toString();
            const int iMinimumContentWidth = iIconSize /* icon width */
                                           + iSpacing
                                           + fm.horizontalAdvance(strName) /* name width */;
            const int iMinimumContentHeight = qMax(fm.height() /* font height */,
                                                   iIconSize /* icon height */);
            const int iMinimumWidth = iMinimumContentWidth + iMargin * 2;
            const int iMinimumHeight = iMinimumContentHeight + iMargin * 2;
            return QSize(iMinimumWidth, iMinimumHeight);
        }

        /* Advanced Attributes: */
        case R_Margin:
        {
            return QApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 1.5;
        }
        case R_Spacing:
        {
            return QApplication::style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) * 2;
        }
        case R_IconSize:
        {
            return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.5;
        }
        case R_ItemId:
        {
            if (UISelectorTreeViewItem *pItem = indexToItem(specifiedIndex))
                return pItem->id();
            return QUuid();
        }
        case R_ItemPixmap:
        {
            if (UISelectorTreeViewItem *pItem = indexToItem(specifiedIndex))
            {
                const QIcon icon = pItem->icon();
                const int iIconSize = data(specifiedIndex, R_IconSize).toInt();
                return icon.pixmap(m_pParentTree->windowHandle(), QSize(iIconSize, iIconSize));
            }
            return QPixmap();
        }
        case R_ItemPixmapRect:
        {
            const int iMargin = data(specifiedIndex, R_Margin).toInt();
            const int iWidth = data(specifiedIndex, R_IconSize).toInt();
            return QRect(iMargin, iMargin, iWidth, iWidth);
        }
        case R_ItemName:
        {
            if (UISelectorTreeViewItem *pItem = indexToItem(specifiedIndex))
                return pItem->text();
            return QString();
        }
        case R_ItemNamePoint:
        {
            const int iMargin = data(specifiedIndex, R_Margin).toInt();
            const int iSpacing = data(specifiedIndex, R_Spacing).toInt();
            const int iIconSize = data(specifiedIndex, R_IconSize).toInt();
            const QFontMetrics fm(data(specifiedIndex, Qt::FontRole).value<QFont>());
            const QSize sizeHint = data(specifiedIndex, Qt::SizeHintRole).toSize();
            return QPoint(iMargin + iIconSize + iSpacing,
                          sizeHint.height() / 2 + fm.ascent() / 2 - 1 /* base line */);
        }

        default:
            break;
    }

    return QVariant();
}

bool UISelectorModel::setData(const QModelIndex &specifiedIndex, const QVariant &aValue, int iRole)
{
    if (!specifiedIndex.isValid())
        return QAbstractItemModel::setData(specifiedIndex, aValue, iRole);

    switch (iRole)
    {
        /* Advanced Attributes: */
        case R_ItemName:
        {
            if (UISelectorTreeViewItem *pItem = indexToItem(specifiedIndex))
            {
                pItem->setText(aValue.toString());
                emit dataChanged(specifiedIndex, specifiedIndex);
                return true;
            }
            return false;
        }

        default:
            break;
    }

    return false;
}

QModelIndex UISelectorModel::addItem(int iID, const QIcon &icon, const QString &strLink)
{
    beginInsertRows(root(), m_pRootItem->childCount(), m_pRootItem->childCount());
    new UISelectorTreeViewItem(m_pRootItem, iID, icon, strLink);
    endInsertRows();
    return index(m_pRootItem->childCount() - 1, 0, root());
}

void UISelectorModel::delItem(int iID)
{
    if (UISelectorTreeViewItem *pItem = m_pRootItem->childItemById(iID))
    {
        const int iItemPosition = m_pRootItem->posOfChild(pItem);
        beginRemoveRows(root(), iItemPosition, iItemPosition);
        delete pItem;
        endRemoveRows();
    }
}

QModelIndex UISelectorModel::findItem(int iID)
{
    UISelectorTreeViewItem *pItem = m_pRootItem->childItemById(iID);
    if (!pItem)
        return QModelIndex();

    const int iItemPosition = m_pRootItem->posOfChild(pItem);
    return pItem ? createIndex(iItemPosition, 0, pItem) : QModelIndex();
}

QModelIndex UISelectorModel::findItem(const QString &strLink)
{
    UISelectorTreeViewItem *pItem = m_pRootItem->childItemByLink(strLink);
    if (!pItem)
        return QModelIndex();

    const int iItemPosition = m_pRootItem->posOfChild(pItem);
    return pItem ? createIndex(iItemPosition, 0, pItem) : QModelIndex();
}

void UISelectorModel::sort(int /* iColumn */, Qt::SortOrder enmOrder)
{
    /* Prepare empty list for sorted items: */
    QList<UISelectorTreeViewItem*> sortedItems;

    /* For each of items: */
    const int iItemCount = m_pRootItem->childCount();
    for (int iItemPos = 0; iItemPos < iItemCount; ++iItemPos)
    {
        /* Get iterated item/id: */
        UISelectorTreeViewItem *pItem = m_pRootItem->childItem(iItemPos);
        const int iID = pItem->id();

        /* Gather suitable position in the list of sorted items: */
        int iInsertPosition = 0;
        for (; iInsertPosition < sortedItems.size(); ++iInsertPosition)
        {
            /* Get sorted item/id: */
            UISelectorTreeViewItem *pSortedItem = sortedItems.at(iInsertPosition);
            const int iSortedID = pSortedItem->id();

            /* Apply sorting rule: */
            if (   ((enmOrder == Qt::AscendingOrder) && (iID < iSortedID))
                || ((enmOrder == Qt::DescendingOrder) && (iID > iSortedID)))
                break;
        }

        /* Insert iterated item into sorted position: */
        sortedItems.insert(iInsertPosition, pItem);
    }

    /* Update corresponding model-indexes: */
    m_pRootItem->setChildren(sortedItems);
    beginMoveRows(root(), 0, iItemCount - 1, root(), 0);
    endMoveRows();
}

Qt::ItemFlags UISelectorModel::flags(const QModelIndex &specifiedIndex) const
{
    return !specifiedIndex.isValid() ? QAbstractItemModel::flags(specifiedIndex)
                                     : Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

/* static */
UISelectorTreeViewItem *UISelectorModel::indexToItem(const QModelIndex &specifiedIndex)
{
    if (!specifiedIndex.isValid())
        return 0;
    return static_cast<UISelectorTreeViewItem*>(specifiedIndex.internalPointer());
}


/*********************************************************************************************************************************
*   Class UISelectorTreeView implementation.                                                                                     *
*********************************************************************************************************************************/

UISelectorTreeView::UISelectorTreeView(QWidget *pParent)
    : QITreeView(pParent)
{
    prepare();
}

QSize UISelectorTreeView::minimumSizeHint() const
{
    /* Sanity check: */
    UISelectorModel *pModel = qobject_cast<UISelectorModel*>(model());
    AssertPtrReturn(pModel, QITreeView::minimumSizeHint());

    /* Calculate largest column width: */
    int iMaximumColumnWidth = 0;
    int iCumulativeColumnHeight = 0;
    for (int i = 0; i < pModel->rowCount(pModel->root()); ++i)
    {
        const QModelIndex iteratedIndex = pModel->index(i, 0, pModel->root());
        const QSize sizeHint = pModel->data(iteratedIndex, Qt::SizeHintRole).toSize();
        const int iHeightHint = sizeHint.height();
        iMaximumColumnWidth = qMax(iMaximumColumnWidth, sizeHint.width() + iHeightHint /* to get the fancy shape */);
        iCumulativeColumnHeight += iHeightHint;
    }

    /* Return selector size-hint: */
    return QSize(iMaximumColumnWidth, iCumulativeColumnHeight);
}

QSize UISelectorTreeView::sizeHint() const
{
    // WORKAROUND:
    // By default QTreeView uses own size-hint
    // which we don't like and want to ignore:
    return minimumSizeHint();
}

void UISelectorTreeView::prepare()
{
    /* Configure tree-view: */
    setFrameShape(QFrame::NoFrame);
    viewport()->setAutoFillBackground(false);
    setContextMenuPolicy(Qt::PreventContextMenu);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

    /* Prepare selector delegate: */
    UISelectorDelegate *pDelegate = new UISelectorDelegate(this);
    if (pDelegate)
        setItemDelegate(pDelegate);
}


/*********************************************************************************************************************************
*   Class UISettingsSelector implementation.                                                                                     *
*********************************************************************************************************************************/

UISettingsSelector::UISettingsSelector(QWidget *pParent /* = 0 */)
    : QObject(pParent)
{
}

UISettingsSelector::~UISettingsSelector()
{
    qDeleteAll(m_list);
    m_list.clear();
}

void UISettingsSelector::setItemText(int iID, const QString &strText)
{
    if (UISelectorItem *pTtem = findItem(iID))
        pTtem->setText(strText);
}

QString UISettingsSelector::itemTextByPage(UISettingsPage *pPage) const
{
    QString strText;
    if (UISelectorItem *pItem = findItemByPage(pPage))
        strText = pItem->text();
    return strText;
}

QWidget *UISettingsSelector::idToPage(int iID) const
{
    UISettingsPage *pPage = 0;
    if (UISelectorItem *pItem = findItem(iID))
        pPage = pItem->page();
    return pPage;
}

QList<UISettingsPage*> UISettingsSelector::settingPages() const
{
    QList<UISettingsPage*> list;
    foreach (UISelectorItem *pItem, m_list)
        if (pItem->page())
            list << pItem->page();
    return list;
}

UISelectorItem *UISettingsSelector::findItem(int iID) const
{
    UISelectorItem *pResult = 0;
    foreach (UISelectorItem *pItem, m_list)
        if (pItem->id() == iID)
        {
            pResult = pItem;
            break;
        }
    return pResult;
}

UISelectorItem *UISettingsSelector::findItemByLink(const QString &strLink) const
{
    UISelectorItem *pResult = 0;
    foreach (UISelectorItem *pItem, m_list)
        if (pItem->link() == strLink)
        {
            pResult = pItem;
            break;
        }
    return pResult;
}

UISelectorItem *UISettingsSelector::findItemByPage(UISettingsPage *pPage) const
{
    UISelectorItem *pResult = 0;
    foreach (UISelectorItem *pItem, m_list)
        if (pItem->page() == pPage)
        {
            pResult = pItem;
            break;
        }
    return pResult;
}


/*********************************************************************************************************************************
*   Class UISettingsSelectorTreeView implementation.                                                                             *
*********************************************************************************************************************************/

UISettingsSelectorTreeView::UISettingsSelectorTreeView(QWidget *pParent /* = 0 */)
    : UISettingsSelector(pParent)
    , m_pTreeView(0)
    , m_pModel(0)
{
    prepare();
}

UISettingsSelectorTreeView::~UISettingsSelectorTreeView()
{
    cleanup();
}

QWidget *UISettingsSelectorTreeView::widget() const
{
    return m_pTreeView;
}

QWidget *UISettingsSelectorTreeView::addItem(const QString & /* strBigIcon */,
                                             const QString &strMediumIcon ,
                                             const QString & /* strSmallIcon */,
                                             int iID,
                                             const QString &strLink,
                                             UISettingsPage *pPage /* = 0 */,
                                             int iParentID /* = -1 */)
{
    QWidget *pResult = 0;
    if (pPage)
    {
        /* Adjust page a bit: */
        pPage->setContentsMargins(0, 0, 0, 0);
        if (pPage->layout())
            pPage->layout()->setContentsMargins(0, 0, 0, 0);
        pResult = pPage;

        /* Add selector-item object: */
        const QIcon icon = UIIconPool::iconSet(strMediumIcon);
        UISelectorItem *pItem = new UISelectorItem(icon, iID, strLink, pPage, iParentID);
        if (pItem)
            m_list.append(pItem);

        /* Add model-item: */
        m_pModel->addItem(iID, pItem->icon(), strLink);
    }
    return pResult;
}

void UISettingsSelectorTreeView::setItemText(int iID, const QString &strText)
{
    /* Call to base-class: */
    UISettingsSelector::setItemText(iID, strText);

    /* Look for the tree-view item to assign the text: */
    QModelIndex specifiedIndex = m_pModel->findItem(iID);
    if (specifiedIndex.isValid())
        m_pModel->setData(specifiedIndex, strText, UISelectorModel::R_ItemName);
}

QString UISettingsSelectorTreeView::itemText(int iID) const
{
    QString strResult;
    if (UISelectorItem *pItem = findItem(iID))
        strResult = pItem->text();
    return strResult;
}

int UISettingsSelectorTreeView::currentId() const
{
    int iID = -1;
    const QModelIndex currentIndex = m_pTreeView->currentIndex();
    if (currentIndex.isValid())
        iID = m_pModel->data(currentIndex, UISelectorModel::R_ItemId).toString().toInt();
    return iID;
}

int UISettingsSelectorTreeView::linkToId(const QString &strLink) const
{
    int iID = -1;
    const QModelIndex specifiedIndex = m_pModel->findItem(strLink);
    if (specifiedIndex.isValid())
        iID = m_pModel->data(specifiedIndex, UISelectorModel::R_ItemId).toString().toInt();
    return iID;
}

void UISettingsSelectorTreeView::selectById(int iID)
{
    const QModelIndex specifiedIndex = m_pModel->findItem(iID);
    if (specifiedIndex.isValid())
        m_pTreeView->setCurrentIndex(specifiedIndex);
}

void UISettingsSelectorTreeView::sltHandleCurrentChanged(const QModelIndex &currentIndex,
                                                         const QModelIndex & /* previousIndex */)
{
    if (currentIndex.isValid())
    {
        const int iID = m_pModel->data(currentIndex, UISelectorModel::R_ItemId).toString().toInt();
        Assert(iID >= 0);
        emit sigCategoryChanged(iID);
    }
}

void UISettingsSelectorTreeView::prepare()
{
    /* Prepare the tree-widget: */
    m_pTreeView = new UISelectorTreeView(qobject_cast<QWidget*>(parent()));
    if (m_pTreeView)
    {
        /* Prepare the model: */
        m_pModel = new UISelectorModel(m_pTreeView);
        if (m_pModel)
        {
            m_pTreeView->setModel(m_pModel);
            m_pTreeView->setRootIndex(m_pModel->root());
        }

        /* Setup connections: */
        connect(m_pTreeView, &QITreeView::currentItemChanged,
                this, &UISettingsSelectorTreeView::sltHandleCurrentChanged);
    }
}

void UISettingsSelectorTreeView::cleanup()
{
    /* Cleanup the model: */
    delete m_pModel;
    m_pModel = 0;
    /* Cleanup the tree-view: */
    delete m_pTreeView;
    m_pTreeView = 0;
}


/*********************************************************************************************************************************
*   Class UISettingsSelectorTreeWidget implementation.                                                                           *
*********************************************************************************************************************************/

UISettingsSelectorTreeWidget::UISettingsSelectorTreeWidget(QWidget *pParent /* = 0 */)
    : UISettingsSelector(pParent)
    , m_pTreeWidget(0)
{
    prepare();
}

UISettingsSelectorTreeWidget::~UISettingsSelectorTreeWidget()
{
    /* Cleanup the tree-widget: */
    delete m_pTreeWidget;
    m_pTreeWidget = 0;
}

QWidget *UISettingsSelectorTreeWidget::widget() const
{
    return m_pTreeWidget;
}

QWidget *UISettingsSelectorTreeWidget::addItem(const QString & /* strBigIcon */,
                                               const QString &strMediumIcon ,
                                               const QString & /* strSmallIcon */,
                                               int iID,
                                               const QString &strLink,
                                               UISettingsPage *pPage /* = 0 */,
                                               int iParentID /* = -1 */)
{
    QWidget *pResult = 0;
    if (pPage)
    {
        /* Adjust page a bit: */
        pPage->setContentsMargins(0, 0, 0, 0);
        if (pPage->layout())
            pPage->layout()->setContentsMargins(0, 0, 0, 0);
        pResult = pPage;

        /* Add selector-item object: */
        const QIcon icon = UIIconPool::iconSet(strMediumIcon);
        UISelectorItem *pItem = new UISelectorItem(icon, iID, strLink, pPage, iParentID);
        if (pItem)
            m_list.append(pItem);

        /* Add tree-widget item: */
        QITreeWidgetItem *pTwItem = new QITreeWidgetItem(m_pTreeWidget, QStringList() << QString("")
                                                                                      << idToString(iID)
                                                                                      << strLink);
        if (pTwItem)
            pTwItem->setIcon(TreeWidgetSection_Category, pItem->icon());
    }
    return pResult;
}

void UISettingsSelectorTreeWidget::setItemText(int iID, const QString &strText)
{
    /* Call to base-class: */
    UISettingsSelector::setItemText(iID, strText);

    /* Look for the tree-widget item to assign the text: */
    QTreeWidgetItem *pItem = findItem(m_pTreeWidget, idToString(iID), TreeWidgetSection_Id);
    if (pItem)
        pItem->setText(TreeWidgetSection_Category, QString(" %1 ").arg(strText));
}

QString UISettingsSelectorTreeWidget::itemText(int iID) const
{
    return pagePath(idToString(iID));
}

int UISettingsSelectorTreeWidget::currentId() const
{
    int iID = -1;
    const QTreeWidgetItem *pItem = m_pTreeWidget->currentItem();
    if (pItem)
        iID = pItem->text(TreeWidgetSection_Id).toInt();
    return iID;
}

int UISettingsSelectorTreeWidget::linkToId(const QString &strLink) const
{
    int iID = -1;
    const QTreeWidgetItem *pItem = findItem(m_pTreeWidget, strLink, TreeWidgetSection_Link);
    if (pItem)
        iID = pItem->text(TreeWidgetSection_Id).toInt();
    return iID;
}

void UISettingsSelectorTreeWidget::selectById(int iID)
{
    QTreeWidgetItem *pItem = findItem(m_pTreeWidget, idToString(iID), TreeWidgetSection_Id);
    if (pItem)
        m_pTreeWidget->setCurrentItem(pItem);
}

void UISettingsSelectorTreeWidget::polish()
{
    /* Get recommended size hint: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    const int iItemWidth = static_cast<QAbstractItemView*>(m_pTreeWidget)->sizeHintForColumn(TreeWidgetSection_Category);
    const int iItemHeight = qMax((int)(iIconMetric * 1.5) /* icon height */,
                                 m_pTreeWidget->fontMetrics().height() /* text height */)
                          + 4 /* margin itself */ * 2 /* margin count */;

    /* Set final size hint for items: */
    m_pTreeWidget->setSizeHintForItems(QSize(iItemWidth , iItemHeight));

    /* Adjust selector width/height: */
    m_pTreeWidget->setFixedWidth(iItemWidth + 2 * m_pTreeWidget->frameWidth());
    m_pTreeWidget->setMinimumHeight(  m_pTreeWidget->topLevelItemCount() * iItemHeight
                                    + 1 /* margin itself */ * 2 /* margin count */);

    /* Sort selector by the id column: */
    m_pTreeWidget->sortItems(TreeWidgetSection_Id, Qt::AscendingOrder);

    /* Resize column(s) to content: */
    m_pTreeWidget->resizeColumnToContents(TreeWidgetSection_Category);
}

void UISettingsSelectorTreeWidget::sltSettingsGroupChanged(QTreeWidgetItem *pItem, QTreeWidgetItem * /* pPrevItem */)
{
    if (pItem)
    {
        const int iID = pItem->text(TreeWidgetSection_Id).toInt();
        Assert(iID >= 0);
        emit sigCategoryChanged(iID);
    }
}

void UISettingsSelectorTreeWidget::prepare()
{
    /* Prepare the tree-widget: */
    m_pTreeWidget = new QITreeWidget(qobject_cast<QWidget*>(parent()));
    if (m_pTreeWidget)
    {
        /* Configure tree-widget: */
        m_pTreeWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pTreeWidget->setRootIsDecorated(false);
        m_pTreeWidget->setUniformRowHeights(true);
        /* Adjust size-policy: */
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        m_pTreeWidget->setSizePolicy(sizePolicy);
        /* Adjust icon size: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pTreeWidget->setIconSize(QSize((int)(1.5 * iIconMetric), (int)(1.5 * iIconMetric)));
        /* Add the columns, hide unnecessary columns and header: */
        m_pTreeWidget->setColumnCount(TreeWidgetSection_Max);
        m_pTreeWidget->header()->hide();
        m_pTreeWidget->hideColumn(TreeWidgetSection_Id);
        m_pTreeWidget->hideColumn(TreeWidgetSection_Link);
        /* Setup connections: */
        connect(m_pTreeWidget, &QITreeWidget::currentItemChanged,
                this, &UISettingsSelectorTreeWidget::sltSettingsGroupChanged);
    }
}

QString UISettingsSelectorTreeWidget::pagePath(const QString &strMatch) const
{
    const QTreeWidgetItem *pTreeItem = findItem(m_pTreeWidget, strMatch, TreeWidgetSection_Id);
    return path(pTreeItem);
}

QTreeWidgetItem *UISettingsSelectorTreeWidget::findItem(QTreeWidget *pView, const QString &strMatch, int iColumn) const
{
    const QList<QTreeWidgetItem*> list = pView->findItems(strMatch, Qt::MatchExactly, iColumn);
    return list.count() ? list.first() : 0;
}

QString UISettingsSelectorTreeWidget::idToString(int iID) const
{
    return QString("%1").arg(iID, 2, 10, QLatin1Char('0'));
}

/* static */
QString UISettingsSelectorTreeWidget::path(const QTreeWidgetItem *pItem)
{
    static QString strSep = ": ";
    QString strPath;
    const QTreeWidgetItem *pCurrentItem = pItem;
    while (pCurrentItem)
    {
        if (!strPath.isNull())
            strPath = strSep + strPath;
        strPath = pCurrentItem->text(TreeWidgetSection_Category).simplified() + strPath;
        pCurrentItem = pCurrentItem->parent();
    }
    return strPath;
}


/*********************************************************************************************************************************
*   Class UISettingsSelectorToolBar implementation.                                                                              *
*********************************************************************************************************************************/

UISettingsSelectorToolBar::UISettingsSelectorToolBar(QWidget *pParent /* = 0 */)
    : UISettingsSelector(pParent)
    , m_pToolBar(0)
    , m_pActionGroup(0)
{
    prepare();
}

UISettingsSelectorToolBar::~UISettingsSelectorToolBar()
{
    /* Cleanup the action group: */
    delete m_pActionGroup;
    m_pActionGroup = 0;

    /* Cleanup the toolbar: */
    delete m_pToolBar;
    m_pToolBar = 0;
}

QWidget *UISettingsSelectorToolBar::widget() const
{
    return m_pToolBar;
}

QWidget *UISettingsSelectorToolBar::addItem(const QString &strBigIcon,
                                            const QString & /* strMediumIcon */,
                                            const QString &strSmallIcon,
                                            int iID,
                                            const QString &strLink,
                                            UISettingsPage *pPage /* = 0 */,
                                            int iParentID /* = -1 */)
{
    const QIcon icon = UIIconPool::iconSet(strBigIcon);

    QWidget *pResult = 0;
    UISelectorActionItem *pItem = new UISelectorActionItem(icon, iID, strLink, pPage, iParentID, this);
    m_list.append(pItem);

    if (iParentID == -1 &&
        pPage != 0)
    {
        m_pActionGroup->addAction(pItem->action());
        m_pToolBar->addAction(pItem->action());
        m_pToolBar->widgetForAction(pItem->action())
            ->setProperty("Belongs to", "UISettingsSelectorToolBar");
        pPage->setContentsMargins(0, 0, 0, 0);
        pPage->layout()->setContentsMargins(0, 0, 0, 0);
        pResult = pPage;
    }
    else if (iParentID == -1 &&
             pPage == 0)
    {
        m_pActionGroup->addAction(pItem->action());
        m_pToolBar->addAction(pItem->action());
        m_pToolBar->widgetForAction(pItem->action())
            ->setProperty("Belongs to", "UISettingsSelectorToolBar");
        QITabWidget *pTabWidget = new QITabWidget();
        pTabWidget->setIconSize(QSize(16, 16));
        pTabWidget->setContentsMargins(0, 0, 0, 0);
        pItem->setTabWidget(pTabWidget);
        pResult = pTabWidget;
    }
    else
    {
        UISelectorActionItem *pParent = findActionItem(iParentID);
        if (pParent)
        {
            QTabWidget *pTabWidget = pParent->tabWidget();
            pPage->setContentsMargins(9, 5, 9, 9);
            pPage->layout()->setContentsMargins(0, 0, 0, 0);
            const QIcon icon1 = UIIconPool::iconSet(strSmallIcon);
            if (pTabWidget)
                pTabWidget->addTab(pPage, icon1, "");
        }
    }
    return pResult;
}

void UISettingsSelectorToolBar::setItemText(int iID, const QString &strText)
{
    if (UISelectorActionItem *pItem = findActionItem(iID))
    {
        pItem->setText(strText);
        if (pItem->action())
            pItem->action()->setText(strText);
        if (pItem->parentID() &&
            pItem->page())
        {
            const UISelectorActionItem *pParent = findActionItem(pItem->parentID());
            if (pParent &&
                pParent->tabWidget())
                pParent->tabWidget()->setTabText(
                    pParent->tabWidget()->indexOf(pItem->page()), strText);
        }
    }
}

QString UISettingsSelectorToolBar::itemText(int iID) const
{
    QString strResult;
    if (UISelectorItem *pItem = findItem(iID))
        strResult = pItem->text();
    return strResult;
}

int UISettingsSelectorToolBar::currentId() const
{
    const UISelectorActionItem *pAction = findActionItemByAction(m_pActionGroup->checkedAction());
    int iID = -1;
    if (pAction)
        iID = pAction->id();
    return iID;
}

int UISettingsSelectorToolBar::linkToId(const QString &strLink) const
{
    int iID = -1;
    const UISelectorItem *pItem = UISettingsSelector::findItemByLink(strLink);
    if (pItem)
        iID = pItem->id();
    return iID;
}

QWidget *UISettingsSelectorToolBar::idToPage(int iID) const
{
    QWidget *pPage = 0;
    if (const UISelectorActionItem *pItem = findActionItem(iID))
    {
        pPage = pItem->page();
        if (!pPage)
            pPage = pItem->tabWidget();
    }
    return pPage;
}

void UISettingsSelectorToolBar::selectById(int iID)
{
    if (const UISelectorActionItem *pItem = findActionItem(iID))
    {
        if (pItem->parentID() != -1)
        {
            const UISelectorActionItem *pParent = findActionItem(pItem->parentID());
            if (pParent &&
                pParent->tabWidget())
            {
                pParent->action()->trigger();
                pParent->tabWidget()->setCurrentIndex(
                    pParent->tabWidget()->indexOf(pItem->page()));
            }
        }
        else
            pItem->action()->trigger();
    }
}

int UISettingsSelectorToolBar::minWidth() const
{
    return m_pToolBar->sizeHint().width() + 2 * 10;
}

void UISettingsSelectorToolBar::sltSettingsGroupChanged(QAction *pAction)
{
    const UISelectorActionItem *pItem = findActionItemByAction(pAction);
    if (pItem)
        emit sigCategoryChanged(pItem->id());
}

void UISettingsSelectorToolBar::sltSettingsGroupChanged(int iIndex)
{
    const UISelectorActionItem *pItem = findActionItemByTabWidget(qobject_cast<QTabWidget*>(sender()), iIndex);
    if (pItem)
    {
        if (pItem->page() &&
            !pItem->tabWidget())
            emit sigCategoryChanged(pItem->id());
        else
        {
            const UISelectorActionItem *pChild = static_cast<UISelectorActionItem*>(
                findItemByPage(static_cast<UISettingsPage*>(pItem->tabWidget()->currentWidget())));
            if (pChild)
                emit sigCategoryChanged(pChild->id());
        }
    }
}

void UISettingsSelectorToolBar::prepare()
{
    /* Install tool-bar button accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUISettingsSelectorToolBarButton::pFactory);

    /* Prepare the toolbar: */
    m_pToolBar = new QIToolBar(qobject_cast<QWidget*>(parent()));
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        m_pToolBar->setUseTextLabels(true);
        m_pToolBar->setIconSize(QSize(32, 32));
#ifdef VBOX_WS_MAC
        m_pToolBar->setShowToolBarButton(false);
#endif

        /* Prepare the action group: */
        m_pActionGroup = new QActionGroup(this);
        if (m_pActionGroup)
        {
            m_pActionGroup->setExclusive(true);
            connect(m_pActionGroup, &QActionGroup::triggered,
                    this, static_cast<void(UISettingsSelectorToolBar::*)(QAction*)>(&UISettingsSelectorToolBar::sltSettingsGroupChanged));
        }
    }
}

UISelectorActionItem *UISettingsSelectorToolBar::findActionItem(int iID) const
{
    return static_cast<UISelectorActionItem*>(UISettingsSelector::findItem(iID));
}

UISelectorActionItem *UISettingsSelectorToolBar::findActionItemByAction(QAction *pAction) const
{
    UISelectorActionItem *pResult = 0;
    foreach (UISelectorItem *pItem, m_list)
        if (static_cast<UISelectorActionItem*>(pItem)->action() == pAction)
        {
            pResult = static_cast<UISelectorActionItem*>(pItem);
            break;
        }

    return pResult;
}

UISelectorActionItem *UISettingsSelectorToolBar::findActionItemByTabWidget(QTabWidget *pTabWidget, int iIndex) const
{
    UISelectorActionItem *pResult = 0;
    foreach (UISelectorItem *pItem, m_list)
        if (static_cast<UISelectorActionItem*>(pItem)->tabWidget() == pTabWidget)
        {
            QTabWidget *pTabWidget2 = static_cast<UISelectorActionItem*>(pItem)->tabWidget(); /// @todo r=bird: same as pTabWidget?
            pResult = static_cast<UISelectorActionItem*>(
                findItemByPage(static_cast<UISettingsPage*>(pTabWidget2->widget(iIndex))));
            break;
        }

    return pResult;

}


#include "UISettingsSelector.moc"
