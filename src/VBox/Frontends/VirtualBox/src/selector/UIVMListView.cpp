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
#include "VBoxProblemReporter.h"
#include "VBoxSelectorWnd.h"

/* Global includes */
#include <QPainter>
#include <QLinearGradient>
#include <QPixmapCache>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* UIVMItemModel class */

void UIVMItemModel::addItem(UIVMItem *aItem)
{
    Assert(aItem);
    int row = m_VMItemList.count();
    emit layoutAboutToBeChanged();
    beginInsertRows(QModelIndex(), row, row);
    m_VMItemList << aItem;
    endInsertRows();
    refreshItem(aItem);
}

void UIVMItemModel::removeItem(UIVMItem *aItem)
{
    Assert(aItem);
    int row = m_VMItemList.indexOf(aItem);
    removeRows(row, 1);
}

bool UIVMItemModel::removeRows(int aRow, int aCount, const QModelIndex &aParent /* = QModelIndex() */)
{
    emit layoutAboutToBeChanged();
    beginRemoveRows(aParent, aRow, aRow + aCount);
    m_VMItemList.erase(m_VMItemList.begin() + aRow, m_VMItemList.begin() + aRow + aCount);
    endRemoveRows();
    emit layoutChanged();
    return true;
}

/**
 *  Refreshes the item corresponding to the given UUID.
 */
void UIVMItemModel::refreshItem(UIVMItem *aItem)
{
    Assert(aItem);
    if (aItem->recache())
        sort();
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

void UIVMItemModel::sort(int /* aColumn */, Qt::SortOrder aOrder /* = Qt::AscendingOrder */)
{
    emit layoutAboutToBeChanged();
    switch (aOrder)
    {
        case Qt::AscendingOrder: qSort(m_VMItemList.begin(), m_VMItemList.end(), UIVMItemNameCompareLessThan); break;
        case Qt::DescendingOrder: qSort(m_VMItemList.begin(), m_VMItemList.end(), UIVMItemNameCompareGreaterThan); break;
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

QVariant UIVMItemModel::headerData(int /*aSection*/, Qt::Orientation /*aOrientation*/,
                                  int /*aRole = Qt::DisplayRole */) const
{
    return QVariant();
}

bool UIVMItemModel::UIVMItemNameCompareLessThan(UIVMItem* aItem1, UIVMItem* aItem2)
{
    Assert(aItem1);
    Assert(aItem2);
    return aItem1->name().toLower() < aItem2->name().toLower();
}

bool UIVMItemModel::UIVMItemNameCompareGreaterThan(UIVMItem* aItem1, UIVMItem* aItem2)
{
    Assert(aItem1);
    Assert(aItem2);
    return aItem2->name().toLower() < aItem1->name().toLower();
}

/* UIVMListView class */

UIVMListView::UIVMListView(QWidget *aParent /* = 0 */)
    :QIListView(aParent)
{
    /* Create & set our delegation class */
    UIVMItemPainter *delegate = new UIVMItemPainter(this);
    setItemDelegate(delegate);
    /* Default icon size */
    setIconSize(QSize(32, 32));
    /* Publish the activation of items */
    connect(this, SIGNAL(activated(const QModelIndex &)),
            this, SIGNAL(activated()));
    /* Use the correct policy for the context menu */
    setContextMenuPolicy(Qt::CustomContextMenu);
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

UIVMItem * UIVMListView::selectedItem() const
{
    QModelIndexList indexes = selectedIndexes();
    if (indexes.isEmpty())
        return NULL;
    return model()->data(indexes.first(), UIVMItemModel::UIVMItemPtrRole).value <UIVMItem *>();
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
    QString key = QString("vbox:%1:%2:%3:%4:%5:%6:%7")
        .arg(index.data(Qt::DisplayRole).toString())
        .arg(index.data(UIVMItemModel::OSTypeIdRole).toString())
        .arg(index.data(UIVMItemModel::SnapShotDisplayRole).toString())
        .arg(index.data(UIVMItemModel::MachineStateDisplayRole).toString())
        .arg(index.data(UIVMItemModel::SessionStateDisplayRole).toString())
        .arg(option.state)
        .arg(option.rect.width());

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

void UIVMItemPainter::paintContent(QPainter *pPainter, const QStyleOptionViewItem &option,
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
    /* For debugging */
//    QRect boundingRect = osTypeRT | vmNameRT | shotRT | stateIconRT | stateRT;
//    pPainter->drawRect(boundingRect);
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
     * the antialiasing is messed up), bliting this into a QImage to make the
     * composition stuff and finally bliting this QImage into the QWidget.
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
    paintContent(&basePainter, option, index);
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
    /* Now use the alpha value of the source to blend the destination in. */
    rasterPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);

    const int blendWidth = qMin(70, r.width());
    QLinearGradient lg(r.width()-blendWidth, 0, r.width(), 0);
    lg.setColorAt(0, QColor(Qt::white));
    lg.setColorAt(0.95, QColor(Qt::transparent));
    lg.setColorAt(1, QColor(Qt::transparent));
    rasterPainter.fillRect(r.width()-blendWidth, 0, blendWidth, r.height(), lg);
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
    /* Really basic layout managment.
     * First layout as usual */
    aOSType->moveTo(m_Margin, m_Margin);
    aVMName->moveTo(m_Margin + aOSType->width() + m_Spacing, m_Margin);
    aShot->moveTo(aVMName->right() + nameSpaceWidth, aVMName->top());
    aStateIcon->moveTo(aVMName->left(), aVMName->bottom());
    aState->moveTo(aStateIcon->right() + stateSpaceWidth, aStateIcon->top());
    /* Do grouping for the automatic center routine.
     * First the states group: */
    QRectList statesLayout;
    statesLayout << aStateIcon << aState;
    /* All items in the layout: */
    QRectList allLayout;
    allLayout << aOSType << aVMName << aShot << statesLayout;
    /* Now verticaly center the items based on the reference item */
    statesLayout.alignVCenterTo(aStateIcon);
    allLayout.alignVCenterTo(aOSType);
}

