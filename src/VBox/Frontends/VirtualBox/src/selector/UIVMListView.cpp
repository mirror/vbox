/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMItemModel, UIVMListView, UIVMItemPainter class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Local includes */
#include "UIVMListView.h"
#include "UIMessageCenter.h"
#include "VBoxSelectorWnd.h"

/* Global includes */
#include <QScrollBar>
#include <QPainter>
#include <QLinearGradient>
#include <QPixmapCache>
#include <QDropEvent>
#include <QUrl>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* UIVMItemModel class */

void UIVMItemModel::addItem(const CMachine &machine)
{
    addItem(new UIVMItem(machine));
}

void UIVMItemModel::addItem(UIVMItem *aItem)
{
    Assert(aItem);
    insertItem(aItem, m_VMItemList.count());
}

void UIVMItemModel::insertItem(UIVMItem *pItem, int row)
{
    Assert(pItem);
    beginInsertRows(QModelIndex(), row, row);
    m_VMItemList.insert(row, pItem);
    endInsertRows();
    refreshItem(pItem);
}

void UIVMItemModel::removeItem(UIVMItem *aItem)
{
    Assert(aItem);
    int row = m_VMItemList.indexOf(aItem);
    removeRows(row, 1);
}

bool UIVMItemModel::removeRows(int aRow, int aCount, const QModelIndex &aParent /* = QModelIndex() */)
{
    beginRemoveRows(aParent, aRow, aRow + aCount - 1);
    for (int i = aRow + aCount - 1; i >= aRow; --i)
        delete m_VMItemList.takeAt(i);
    endRemoveRows();
    return true;
}

/**
 *  Refreshes the item corresponding to the given UUID.
 */
void UIVMItemModel::refreshItem(UIVMItem *aItem)
{
    Assert(aItem);
    aItem->recache();
    itemChanged(aItem);
}

void UIVMItemModel::itemChanged(UIVMItem *aItem)
{
    Assert(aItem);
    int row = m_VMItemList.indexOf(aItem);
    /* Emit an layout change signal for the case some dimensions of the item
     * has changed also. */
    emit layoutChanged();
    /* Emit an data changed signal. */
    emit dataChanged(index(row), index(row));
}

/**
 *  Clear the item model list. Please note that the items itself are also
 *  deleted.
 */
void UIVMItemModel::clear()
{
    qDeleteAll(m_VMItemList);
}

/**
 *  Returns the list item with the given UUID.
 */
UIVMItem *UIVMItemModel::itemById(const QString &aId) const
{
    foreach(UIVMItem *item, m_VMItemList)
        if (item->id() == aId)
            return item;
    return NULL;
}

UIVMItem *UIVMItemModel::itemByRow(int aRow) const
{
    return m_VMItemList.at(aRow);
}

QModelIndex UIVMItemModel::indexById(const QString &aId) const
{
    int row = rowById(aId);
    if (row >= 0)
        return index(row);
    else
        return QModelIndex();
}

int UIVMItemModel::rowById(const QString &aId) const
{
    for (int i=0; i < m_VMItemList.count(); ++i)
    {
        UIVMItem *item = m_VMItemList.at(i);
        if (item->id() == aId)
            return i;
    }
    return -1;
}

QStringList UIVMItemModel::idList() const
{
    QStringList list;
    foreach(UIVMItem *item, m_VMItemList)
        list << item->id();
    return list;
}

void UIVMItemModel::sortByIdList(const QStringList &list)
{
    emit layoutAboutToBeChanged();
    QList<UIVMItem*> tmpVMItemList(m_VMItemList);
    m_VMItemList.clear();
    /* Iterate over all ids and move the corresponding items ordered in the new
       list. */
    foreach(QString id, list)
    {
        QMutableListIterator<UIVMItem*> it(tmpVMItemList);
        while (it.hasNext())
        {
            UIVMItem *pItem = it.next();
            if (pItem->id() == id)
            {
                m_VMItemList << pItem;
                it.remove();
                break;
            }
        }
    }
    /* If there are items which didn't have been in the id list, we sort them
       by name and appending them to the new list afterward. This make sure the
       old behavior of VBox is respected. */
    if (tmpVMItemList.count() > 0)
    {
        qSort(tmpVMItemList.begin(), tmpVMItemList.end(), UIVMItemNameCompareLessThan);
        QListIterator<UIVMItem*> it(tmpVMItemList);
        while (it.hasNext())
            m_VMItemList << it.next();
    }
    emit layoutChanged();
}

int UIVMItemModel::rowCount(const QModelIndex & /* aParent = QModelIndex() */) const
{
    return m_VMItemList.count();
}

QVariant UIVMItemModel::data(const QModelIndex &aIndex, int aRole) const
{
    if (!aIndex.isValid())
        return QVariant();

    if (aIndex.row() >= m_VMItemList.size())
        return QVariant();

    QVariant v;
    switch (aRole)
    {
        case Qt::DisplayRole:
        {
            v = m_VMItemList.at(aIndex.row())->name();
            break;
        }
        case Qt::DecorationRole:
        {
            v = m_VMItemList.at(aIndex.row())->osIcon();
            break;
        }
        case Qt::ToolTipRole:
        {
            v = m_VMItemList.at(aIndex.row())->toolTipText();
            break;
        }
        case Qt::FontRole:
        {
            QFont f = qApp->font();
            f.setPointSize(f.pointSize() + 1);
            f.setWeight(QFont::Bold);
            v = f;
            break;
        }
        case Qt::AccessibleTextRole:
        {
            UIVMItem *item = m_VMItemList.at(aIndex.row());
            v = QString("%1 (%2)\n%3")
                         .arg(item->name())
                         .arg(item->snapshotName())
                         .arg(item->machineStateName());
            break;
        }
        case SnapShotDisplayRole:
        {
            v = m_VMItemList.at(aIndex.row())->snapshotName();
            break;
        }
        case SnapShotFontRole:
        {
            QFont f = qApp->font();
            v = f;
            break;
        }
        case MachineStateDisplayRole:
        {
            v = m_VMItemList.at(aIndex.row())->machineStateName();
            break;
        }
        case MachineStateDecorationRole:
        {
            v = m_VMItemList.at(aIndex.row())->machineStateIcon();
            break;
        }
        case MachineStateFontRole:
        {
            QFont f = qApp->font();
            f.setPointSize(f.pointSize());
            if (m_VMItemList.at(aIndex.row())->sessionState() != KSessionState_Unlocked)
                f.setItalic(true);
            v = f;
            break;
        }
        case SessionStateDisplayRole:
        {
            v = m_VMItemList.at(aIndex.row())->sessionStateName();
            break;
        }
        case OSTypeIdRole:
        {
            v = m_VMItemList.at(aIndex.row())->osTypeId();
            break;
        }
        case UIVMItemPtrRole:
        {
            v = qVariantFromValue(m_VMItemList.at(aIndex.row()));
            break;
        }
    }
    return v;
}

QVariant UIVMItemModel::headerData(int /* aSection */, Qt::Orientation /* aOrientation */,
                                   int /* aRole = Qt::DisplayRole */) const
{
    return QVariant();
}

bool UIVMItemModel::UIVMItemNameCompareLessThan(UIVMItem* aItem1, UIVMItem* aItem2)
{
    Assert(aItem1);
    Assert(aItem2);
    return aItem1->name().toLower() < aItem2->name().toLower();
}

Qt::ItemFlags UIVMItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

    if (index.isValid())
        return Qt::ItemIsDragEnabled | defaultFlags;
    else
        return Qt::ItemIsDropEnabled | defaultFlags;
}

Qt::DropActions UIVMItemModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions UIVMItemModel::supportedDropActions() const
{
    return Qt::MoveAction; //| Qt::CopyAction | Qt::LinkAction;
}

QStringList UIVMItemModel::mimeTypes() const
{
    QStringList types;
    types << UIVMItemMimeData::type();
    return types;
}

QMimeData *UIVMItemModel::mimeData(const QModelIndexList &indexes) const
{
    UIVMItemMimeData *pMimeData = new UIVMItemMimeData(m_VMItemList.at(indexes.at(0).row()));
    return pMimeData;
}

bool UIVMItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (column > 0)
        return false;

    int beginRow;
    if (row != -1)
        beginRow = row;
    else if (parent.isValid())
        beginRow = parent.row();
    else
        beginRow = rowCount(QModelIndex());

    if (data->hasFormat(UIVMItemMimeData::type()))
    {
        /* Only move allowed. */
        if (action != Qt::MoveAction)
            return false;
        /* Get our own mime data type. */
        const UIVMItemMimeData *pMimeData = qobject_cast<const UIVMItemMimeData*>(data);
        if (pMimeData)
        {
            insertItem(new UIVMItem(pMimeData->item()->machine()), beginRow);
            return true;
        }
    }
    return false;
}

/* UIVMListView class */

UIVMListView::UIVMListView(QAbstractListModel *pModel, QWidget *aParent /* = 0 */)
    : QIListView(aParent)
    , m_fItemInMove(false)
{
    /* For queued events Q_DECLARE_METATYPE isn't sufficient. */
    qRegisterMetaType< QList<QUrl> >("QList<QUrl>");
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    /* Create & set our delegation class */
    UIVMItemPainter *delegate = new UIVMItemPainter(this);
    setItemDelegate(delegate);
    setModel(pModel);
    /* Default icon size */
    setIconSize(QSize(32, 32));
    /* Publish the activation of items */
    connect(this, SIGNAL(activated(const QModelIndex &)),
            this, SIGNAL(activated()));
    /* Use the correct policy for the context menu */
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(pModel, SIGNAL(rowsAboutToBeInserted(const QModelIndex &, int, int)),
            this, SLOT(sltRowsAboutToBeInserted(const QModelIndex &, int, int)));
}

void UIVMListView::selectItemByRow(int row)
{
    setCurrentIndex(model()->index(row, 0));
}

void UIVMListView::selectItemById(const QString &aID)
{
    if (UIVMItemModel *m = qobject_cast <UIVMItemModel*>(model()))
    {
        QModelIndex i = m->indexById(aID);
        if (i.isValid())
            setCurrentIndex(i);
    }
}

void UIVMListView::ensureSomeRowSelected(int aRowHint)
{
    UIVMItem *item = selectedItem();
    if (!item)
    {
        aRowHint = qBound(0, aRowHint, model()->rowCount() - 1);
        selectItemByRow(aRowHint);
        item = selectedItem();
        if (!item)
            selectItemByRow(0);
    }
}

UIVMItem *UIVMListView::selectedItem() const
{
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return NULL;
    return model()->data(indexes.first(), UIVMItemModel::UIVMItemPtrRole).value<UIVMItem*>();
}

void UIVMListView::ensureCurrentVisible()
{
    scrollTo(currentIndex(), QAbstractItemView::EnsureVisible);
}

void UIVMListView::selectionChanged(const QItemSelection &aSelected, const QItemSelection &aDeselected)
{
    QListView::selectionChanged(aSelected, aDeselected);
    selectCurrent();
    ensureCurrentVisible();
    emit currentChanged();
}

void UIVMListView::currentChanged(const QModelIndex &aCurrent, const QModelIndex &aPrevious)
{
    QListView::currentChanged(aCurrent, aPrevious);
    selectCurrent();
    ensureCurrentVisible();
    emit currentChanged();
}

void UIVMListView::dataChanged(const QModelIndex &aTopLeft, const QModelIndex &aBottomRight)
{
    QListView::dataChanged(aTopLeft, aBottomRight);
    selectCurrent();
//    ensureCurrentVisible();
    emit currentChanged();
}

void UIVMListView::sltRowsAboutToBeInserted(const QModelIndex & /* parent */, int /* start */, int /* end */)
{
    /* On D&D the items in the view jumps like hell, cause after inserting the
       new item, but before deleting the old item an update is triggered. We
       disable the updates now. */
    if (m_fItemInMove)
        setUpdatesEnabled(false);
}

void UIVMListView::rowsInserted(const QModelIndex & /* parent */, int start, int /* end */)
{
    /* Select the new item, after a D&D operation. Note: don't call the base
       class, cause this will trigger an update. */
    if (m_fItemInMove)
        selectItemByRow(start);
}

bool UIVMListView::selectCurrent()
{
    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (indexes.isEmpty() ||
        indexes.first() != currentIndex())
    {
        /* Make sure that the current is always selected */
        selectionModel()->select(currentIndex(), QItemSelectionModel::Current | QItemSelectionModel::ClearAndSelect);
        return true;
    }
    return false;
}

void UIVMListView::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if (qApp->activeModalWidget() != 0)
        pEvent->ignore();
    else
    {
        QListView::dragEnterEvent(pEvent);
        checkDragEvent(pEvent);
    }
}

void UIVMListView::dragMoveEvent(QDragMoveEvent *pEvent)
{
    QListView::dragMoveEvent(pEvent);
    checkDragEvent(pEvent);
}

void UIVMListView::checkDragEvent(QDragMoveEvent *pEvent)
{
    if (pEvent->mimeData()->hasUrls())
    {
        QList<QUrl> list = pEvent->mimeData()->urls();
        QString file = list.at(0).toLocalFile();
        if (VBoxGlobal::hasAllowedExtension(file, VBoxDefs::VBoxFileExts))
        {
            Qt::DropAction action = Qt::IgnoreAction;
            if (pEvent->possibleActions().testFlag(Qt::LinkAction))
                action = Qt::LinkAction;
            else if (pEvent->possibleActions().testFlag(Qt::CopyAction))
                action = Qt::CopyAction;
            if (action != Qt::IgnoreAction)
            {
                pEvent->setDropAction(action);
                pEvent->accept();
            }
        }else if (   VBoxGlobal::hasAllowedExtension(file, VBoxDefs::OVFFileExts)
                  && pEvent->possibleActions().testFlag(Qt::CopyAction))
        {
            pEvent->setDropAction(Qt::CopyAction);
            pEvent->accept();
        }else if (   VBoxGlobal::hasAllowedExtension(file, VBoxDefs::VBoxExtPackFileExts)
                  && pEvent->possibleActions().testFlag(Qt::CopyAction))
        {
            pEvent->setDropAction(Qt::CopyAction);
            pEvent->accept();
        }
    }
}

void UIVMListView::dropEvent(QDropEvent *pEvent)
{
    if (pEvent->mimeData()->hasFormat(UIVMItemMimeData::type()))
        QListView::dropEvent(pEvent);
    else if (pEvent->mimeData()->hasUrls())
    {
        QList<QUrl> list = pEvent->mimeData()->urls();
        pEvent->acceptProposedAction();
        emit sigUrlsDropped(list);
    }
}

void UIVMListView::startDrag(Qt::DropActions supportedActions)
{
    /* Select all indexes which are currently selected and dragable. */
    QModelIndexList indexes = selectedIndexes();
    for(int i = indexes.count() - 1 ; i >= 0; --i)
    {
        if (!(model()->flags(indexes.at(i)) & Qt::ItemIsDragEnabled))
            indexes.removeAt(i);
    }
    if (indexes.count() > 0)
    {
        m_fItemInMove = true;
        QMimeData *data = model()->mimeData(indexes);
        if (!data)
            return;
        /* We only support one at the time. Also we need a persistent index,
           cause the position of the old index may change. */
        QPersistentModelIndex oldIdx(indexes.at(0));
        QRect rect;
        QPixmap pixmap = dragPixmap(oldIdx);
        rect.adjust(horizontalOffset(), verticalOffset(), 0, 0);
        QDrag *drag = new QDrag(this);
        drag->setPixmap(pixmap);
        drag->setMimeData(data);
        drag->setHotSpot(QPoint(5, 5));
        Qt::DropAction defaultDropAction = Qt::MoveAction;
        /* Now start the drag. */
        if (drag->exec(supportedActions, defaultDropAction) == Qt::MoveAction)
            /* Remove the old item */
            model()->removeRows(oldIdx.row(), 1, QModelIndex());
        m_fItemInMove = false;
        setUpdatesEnabled(true);
    }
}

QPixmap UIVMListView::dragPixmap(const QModelIndex &index) const
{
    QString name = index.data(Qt::DisplayRole).toString();
    if (name.length() > 100)
        name = name.remove(97, name.length() - 97) + "...";
    QSize nameSize = fontMetrics().boundingRect(name).size();
    nameSize.setWidth(nameSize.width() + 30);
    QPixmap osType = index.data(Qt::DecorationRole).value<QIcon>().pixmap(32, 32);
    QSize osTypeSize = osType.size();
    int space = 5, margin = 15;
    QSize s(2 * margin + qMax(osTypeSize.width(), nameSize.width()),
            2 * margin + osTypeSize.height() + space + nameSize.height());
    QImage image(s, QImage::Format_ARGB32);
    image.fill(QColor(Qt::transparent).rgba());
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(QColor(80, 80, 80));
    p.drawRoundedRect(1, 1, s.width() - 1 * 2, s.height() - 1 * 2, 6, 6);
    p.drawPixmap((s.width() - osTypeSize.width()) / 2, margin, osType);
    p.setPen(Qt::white);
    p.setFont(font());
    p.drawText(QRect(margin, margin + osTypeSize.height() + space,  s.width() - 2 * margin, nameSize.height()), Qt::AlignCenter, name);
    /* Transparent icons are not supported on all platforms. */
#if defined(Q_WS_MAC) || defined(Q_WS_WIN)
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.fillRect(image.rect(), QColor(0, 0, 0, 177));
#endif /* defined(Q_WS_MAC) || defined(Q_WS_WIN) */
    p.end();
    /* Some Qt versions seems buggy in creating QPixmap from QImage. Seems they
     * don't clear the background. */
    QPixmap i1(s);
    i1.fill(Qt::transparent);
    QPainter p1(&i1);
    p1.drawImage(image.rect(), image);
    p1.end();
    return i1;
}

QModelIndex UIVMListView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    if (modifiers.testFlag(Qt::ControlModifier))
    {
        switch (cursorAction)
        {
            case QAbstractItemView::MoveUp:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, index.row() - 1);
            }
            case QAbstractItemView::MoveDown:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, index.row() + 2);
            }
            case QAbstractItemView::MovePageUp:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, qMax(0, index.row() - verticalScrollBar()->pageStep()));
            }
            case QAbstractItemView::MovePageDown:
            {
                QModelIndex index = currentIndex();
                return moveItemTo(index, qMin(model()->rowCount(), index.row() + verticalScrollBar()->pageStep() + 1));
            }
            case QAbstractItemView::MoveHome:
                return moveItemTo(currentIndex(), 0);
            case QAbstractItemView::MoveEnd:
                return moveItemTo(currentIndex(), model()->rowCount());
            default:
                break;
        }
    }
    return QListView::moveCursor(cursorAction, modifiers);
}

QModelIndex UIVMListView::moveItemTo(const QModelIndex &index, int row)
{
    if (!index.isValid())
        return QModelIndex();

    UIVMItemModel *pModel = static_cast<UIVMItemModel*>(model());
    if (row < 0 || row > pModel->rowCount())
        return QModelIndex();

    QPersistentModelIndex perIndex(index);
    UIVMItem *pItem = pModel->data(index, UIVMItemModel::UIVMItemPtrRole).value<UIVMItem*>();
    m_fItemInMove = true;
    pModel->insertItem(new UIVMItem(pItem->machine()), row);
    QPersistentModelIndex newIndex = pModel->index(row);
    pModel->removeRows(perIndex.row(), 1, QModelIndex());
    m_fItemInMove = false;
    setUpdatesEnabled(true);
    return QModelIndex(newIndex);
}

/* UIVMItemPainter class */
/*
 +----------------------------------------------+
 |       marg                                   |
 |   +----------+   m                           |
 | m |          | m a  name_string___________ m |
 | a |  OSType  | a r                         a |
 | r |  icon    | r g  +--+                   r |
 | g |          | g /  |si|  state_string     g |
 |   +----------+   2  +--+                     |
 |       marg                                   |
 +----------------------------------------------+

 si = state icon

*/

/* Little helper class for layout calculation */
class QRectList: public QList<QRect *>
{
public:
    void alignVCenterTo(QRect* aWhich)
    {
        QRect b;
        foreach(QRect *rect, *this)
            if(rect != aWhich)
                b |= *rect;
        if (b.width() > aWhich->width())
            aWhich->moveCenter(QPoint(aWhich->center().x(), b.center().y()));
        else
        {
            foreach(QRect *rect, *this)
                if(rect != aWhich)
                    rect->moveCenter(QPoint(rect->center().x(), aWhich->center().y()));
        }
    }
};

QSize UIVMItemPainter::sizeHint(const QStyleOptionViewItem &aOption,
                                const QModelIndex &aIndex) const
{
    /* Get the size of every item */
    QRect osTypeRT = rect(aOption, aIndex, Qt::DecorationRole);
    QRect vmNameRT = rect(aOption, aIndex, Qt::DisplayRole);
    QRect shotRT = rect(aOption, aIndex, UIVMItemModel::SnapShotDisplayRole);
    QRect stateIconRT = rect(aOption, aIndex, UIVMItemModel::MachineStateDecorationRole);
    QRect stateRT = rect(aOption, aIndex, UIVMItemModel::MachineStateDisplayRole);
    /* Calculate the position for every item */
    calcLayout(aIndex, &osTypeRT, &vmNameRT, &shotRT, &stateIconRT, &stateRT);
    /* Calc the bounding rect */
    const QRect boundingRect = osTypeRT | vmNameRT | shotRT | stateIconRT | stateRT;
    /* Return + left/top/right/bottom margin */
    return (boundingRect.size() + QSize(2 * m_Margin, 2 * m_Margin));
}

void UIVMItemPainter::paint(QPainter *pPainter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    /* Generate the key used in the pixmap cache. Needs to be composed with all
     * values which might be changed. */
    QString key = QString("vbox:%1:%2:%3:%4:%5:%6:%7:%8:%9")
        .arg(index.data(Qt::DisplayRole).toString())
        .arg(index.data(UIVMItemModel::OSTypeIdRole).toString())
        .arg(index.data(UIVMItemModel::SnapShotDisplayRole).toString())
        .arg(index.data(UIVMItemModel::MachineStateDisplayRole).toString())
        .arg(index.data(UIVMItemModel::SessionStateDisplayRole).toString())
        .arg(option.state)
        .arg(option.rect.width())
        .arg(qobject_cast<QWidget*>(parent())->hasFocus())
        .arg(qApp->focusWidget() == NULL);

    /* Check if the pixmap already exists in the cache. */
    QPixmap pixmap;
    if (!QPixmapCache::find(key, pixmap))
    {
        /* If not, generate a new one */
        QStyleOptionViewItem tmpOption(option);
        /* Highlight background if an item is selected in any case.
         * (Fix for selector in the windows style.) */
        tmpOption.showDecorationSelected = true;

        /* Create a temporary pixmap and painter to work on.*/
        QPixmap tmpPixmap(option.rect.size());
        tmpPixmap.fill(Qt::transparent);
        QPainter tmpPainter(&tmpPixmap);

        /* Normally we operate on a painter which is in the size of the list
         * view widget. Here we process one item only, so shift all the stuff
         * out of the view. It will be translated back in the following
         * methods. */
        tmpPainter.translate(-option.rect.x(), -option.rect.y());

        /* Start drawing with the background */
        drawBackground(&tmpPainter, tmpOption, index);

        /* Blend the content */
        blendContent(&tmpPainter, tmpOption, index);

        /* Draw a focus rectangle when necessary */
        drawFocus(&tmpPainter, tmpOption, tmpOption.rect);

        /* Finish drawing */
        tmpPainter.end();

        pixmap = tmpPixmap;
        /* Fill the  cache */
        QPixmapCache::insert(key, tmpPixmap);
    }
    pPainter->drawPixmap(option.rect, pixmap);
}

QRect UIVMItemPainter::paintContent(QPainter *pPainter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    /* Name and decoration */
    const QString vmName = index.data(Qt::DisplayRole).toString();
    const QFont nameFont = index.data(Qt::FontRole).value<QFont>();
    const QPixmap osType = index.data(Qt::DecorationRole).value<QIcon>().pixmap(option.decorationSize, iconMode(option.state), iconState(option.state));

    const QString shot = index.data(UIVMItemModel::SnapShotDisplayRole).toString();
    const QFont shotFont = index.data(UIVMItemModel::SnapShotFontRole).value<QFont>();

    const QString state = index.data(UIVMItemModel::MachineStateDisplayRole).toString();
    const QFont stateFont = index.data(UIVMItemModel::MachineStateFontRole).value<QFont>();
    const QPixmap stateIcon = index.data(UIVMItemModel::MachineStateDecorationRole).value<QIcon>().pixmap(QSize(16, 16), iconMode(option.state), iconState(option.state));

    /* Get the sizes for all items */
    QRect osTypeRT = rect(option, index, Qt::DecorationRole);
    QRect vmNameRT = rect(option, index, Qt::DisplayRole);
    QRect shotRT = rect(option, index, UIVMItemModel::SnapShotDisplayRole);
    QRect stateIconRT = rect(option, index, UIVMItemModel::MachineStateDecorationRole);
    QRect stateRT = rect(option, index, UIVMItemModel::MachineStateDisplayRole);

    /* Calculate the positions for all items */
    calcLayout(index, &osTypeRT, &vmNameRT, &shotRT, &stateIconRT, &stateRT);
    /* Get the appropriate pen for the current state */
    QPalette pal = option.palette;
    QPen pen = pal.color(QPalette::Active, QPalette::Text);
    if (option.state & QStyle::State_Selected &&
        (option.state & QStyle::State_HasFocus ||
        QApplication::style()->styleHint(QStyle::SH_ItemView_ChangeHighlightOnFocus, &option) == 0))
        pen =  pal.color(QPalette::Active, QPalette::HighlightedText);
    /* Set the current pen */
    pPainter->setPen(pen);
    /* os type icon */
    pPainter->drawPixmap(osTypeRT, osType);
    /* vm name */
    pPainter->setFont(nameFont);
    pPainter->drawText(vmNameRT, vmName);
    /* current snapshot in braces */
    if (!shot.isEmpty())
    {
        pPainter->setFont(shotFont);
        pPainter->drawText(shotRT, QString("(%1)").arg(shot));
    }
    /* state icon */
    pPainter->drawPixmap(stateIconRT, stateIcon);
    /* textual state */
    pPainter->setFont(stateFont);
    pPainter->drawText(stateRT, state);
    QRect boundingRect = osTypeRT | vmNameRT | shotRT | stateIconRT | stateRT;
    /* For debugging */
//    pPainter->drawRect(boundingRect);
    return boundingRect;
}

void UIVMItemPainter::blendContent(QPainter *pPainter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QRect r = option.rect;
    QWidget *pParent = qobject_cast<QListView *>(parent())->viewport();
    /* This is as always a big fat mess on Mac OS X. We can't use QImage for
     * rendering text, cause this looks like shit. We can't do all the drawing
     * on the widget, cause the composition modes are not working correctly.
     * The same count for doing composition on a QPixmap. The work around is to
     * draw all into a QPixmap (also the background color/gradient, otherwise
     * the antialiasing is messed up), blitting this into a QImage to make the
     * composition stuff and finally blitting this QImage into the QWidget.
     * Yipi a yeah. Btw, no problem on Linux at all. */
    QPixmap basePixmap(r.width(), r.height());//
    /* Initialize with the base image color. */
    basePixmap.fill(pParent->palette().base().color());
    /* Create the painter to operate on. */
    QPainter basePainter(&basePixmap);
    /* Initialize the painter with the corresponding widget */
    basePainter.initFrom(pParent);
    basePainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    /* The translation is necessary, cause drawBackground expect the whole drawing area. */
    basePainter.save();
    basePainter.translate(-option.rect.x(), -option.rect.y());
    drawBackground(&basePainter, option, index);
    basePainter.restore();
    /* Now paint the content. */
    QRect usedRect = paintContent(&basePainter, option, index);
    /* Finished with the OS dependent part. */
    basePainter.end();
    /* Time for the OS independent part (That is, use the QRasterEngine) */
    QImage baseImage(r.width(), r.height(), QImage::Format_ARGB32_Premultiplied);
    QPainter rasterPainter(&baseImage);
    /* Initialize the painter with the corresponding widget */
    rasterPainter.initFrom(pParent);
    rasterPainter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    /* Fully copy the source to the destination */
    rasterPainter.setCompositionMode(QPainter::CompositionMode_Source);
    rasterPainter.drawPixmap(0, 0, basePixmap);
    if (usedRect.x() + usedRect.width() > option.rect.width())
    {
        /* Now use the alpha value of the source to blend the destination in. */
        rasterPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        const int blendWidth = qMin(30, r.width());
        QLinearGradient lg(r.width()-blendWidth, 0, r.width(), 0);
        lg.setColorAt(0, QColor(Qt::white));
        lg.setColorAt(0.95, QColor(Qt::transparent));
        lg.setColorAt(1, QColor(Qt::transparent));
        rasterPainter.fillRect(r.width()-blendWidth, 0, blendWidth, r.height(), lg);
    }
    /* Finished with the OS independent part. */
    rasterPainter.end();

    /* Finally blit our hard work on the widget. */
    pPainter->drawImage(option.rect.x(), option.rect.y(), baseImage);
}

QRect UIVMItemPainter::rect(const QStyleOptionViewItem &aOption,
                            const QModelIndex &aIndex, int aRole) const
{
    switch (aRole)
    {
        case Qt::DisplayRole:
        {
            QString text = aIndex.data(Qt::DisplayRole).toString();
            QFontMetrics fm(fontMetric(aIndex, Qt::FontRole));
            return QRect(QPoint(0, 0), fm.size(0, text));
            break;
        }
        case Qt::DecorationRole:
        {
            QIcon icon = aIndex.data(Qt::DecorationRole).value<QIcon>();
            return QRect(QPoint(0, 0), icon.actualSize(aOption.decorationSize, iconMode(aOption.state), iconState(aOption.state)));
            break;
        }
        case UIVMItemModel::SnapShotDisplayRole:
        {
            QString text = aIndex.data(UIVMItemModel::SnapShotDisplayRole).toString();
            if (!text.isEmpty())
            {
                QFontMetrics fm(fontMetric(aIndex, UIVMItemModel::SnapShotFontRole));
                return QRect(QPoint(0, 0), fm.size(0, QString("(%1)").arg(text)));
            }else
                return QRect();
            break;
        }
        case UIVMItemModel::MachineStateDisplayRole:
        {
            QString text = aIndex.data(UIVMItemModel::MachineStateDisplayRole).toString();
            QFontMetrics fm(fontMetric(aIndex, UIVMItemModel::MachineStateFontRole));
            return QRect(QPoint(0, 0), fm.size(0, text));
            break;
        }
        case UIVMItemModel::MachineStateDecorationRole:
        {
            QIcon icon = aIndex.data(UIVMItemModel::MachineStateDecorationRole).value<QIcon>();
            return QRect(QPoint(0, 0), icon.actualSize(QSize(16, 16), iconMode(aOption.state), iconState(aOption.state)));
            break;
        }
    }
    return QRect();
}

void UIVMItemPainter::calcLayout(const QModelIndex &aIndex,
                                 QRect *aOSType, QRect *aVMName, QRect *aShot,
                                 QRect *aStateIcon, QRect *aState) const
{
    const int nameSpaceWidth = fontMetric(aIndex, Qt::FontRole).width(' ');
    const int stateSpaceWidth = fontMetric(aIndex, UIVMItemModel::MachineStateFontRole).width(' ');
    /* Really basic layout management.
     * First layout as usual */
    aOSType->moveTo(m_Margin, m_Margin);
    aVMName->moveTo(aOSType->right() + m_Spacing, m_Margin);
    aShot->moveTo(aVMName->right() + nameSpaceWidth, aVMName->top());
    aStateIcon->moveTo(aVMName->left(), aOSType->bottom() - aStateIcon->height());
    aState->moveTo(aStateIcon->right() + stateSpaceWidth, aStateIcon->top());
    /* Do grouping for the automatic center routine.
     * First the states group: */
    QRectList statesLayout;
    statesLayout << aStateIcon << aState;
    /* All items in the layout: */
    QRectList allLayout;
    allLayout << aOSType << aVMName << aShot << statesLayout;
    /* Now vertically center the items based on the reference item */
    statesLayout.alignVCenterTo(aStateIcon);
    allLayout.alignVCenterTo(aOSType);
}

