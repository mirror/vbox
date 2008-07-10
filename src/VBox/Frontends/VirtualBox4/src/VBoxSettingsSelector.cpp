/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxSettingsSelector class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBoxSettingsSelector.h"
#include "VBoxSettingsUtils.h"
#include "QITreeWidget.h"

enum
{
    /* mTwSelector column numbers */
    treeWidget_Category = 0,
    treeWidget_Id,
    treeWidget_Link
};

VBoxSettingsSelector::VBoxSettingsSelector (QWidget *aParent /* = NULL */)
    :QObject (aParent)
{
}

/* VBoxSettingsTreeSelector */

/* Returns the path to the item in the form of 'grandparent > parent > item'
 * using the text of the first column of every item. */
static QString path (QTreeWidgetItem *aItem)
{
    static QString sep = ": ";
    QString p;
    QTreeWidgetItem *cur = aItem;
    while (cur)
    {
        if (!p.isNull())
            p = sep + p;
        p = cur->text (treeWidget_Category).simplified() + p;
        cur = cur->parent();
    }
    return p;
}

VBoxSettingsTreeSelector::VBoxSettingsTreeSelector (QWidget *aParent /* = NULL */)
    :VBoxSettingsSelector (aParent)
{
    mTwSelector = new QITreeWidget (aParent);
    /* Configure the selector */
    QSizePolicy sizePolicy (QSizePolicy::Minimum, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch (0);
    sizePolicy.setVerticalStretch (0);
    sizePolicy.setHeightForWidth (mTwSelector->sizePolicy().hasHeightForWidth());
    mTwSelector->setSizePolicy (sizePolicy);
    mTwSelector->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    mTwSelector->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    mTwSelector->setRootIsDecorated (false);
    mTwSelector->setUniformRowHeights (true);
    /* Add the columns */
    mTwSelector->headerItem()->setText (treeWidget_Category, "Category");
    mTwSelector->headerItem()->setText (treeWidget_Id, "[id]");
    mTwSelector->headerItem()->setText (treeWidget_Link, "[link]");
    /* Hide unnecessary columns and header */
    mTwSelector->header()->hide();
    mTwSelector->hideColumn (treeWidget_Id);
    mTwSelector->hideColumn (treeWidget_Link);
    /* Setup connections */
    connect (mTwSelector, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (settingsGroupChanged (QTreeWidgetItem *, QTreeWidgetItem*)));
}

QWidget *VBoxSettingsTreeSelector::widget() const
{
    return mTwSelector;
}

void VBoxSettingsTreeSelector::addItem (const QIcon &aIcon, 
                                        const QString &aText, 
                                        int aId, 
                                        const QString &aLink)
{
    QTreeWidgetItem *item = NULL;

    item = new QTreeWidgetItem (mTwSelector, QStringList() << QString (" %1 ").arg (aText)
                                                           << idToString (aId)
                                                           << aLink);
    item->setIcon (treeWidget_Category, aIcon);
}

QString VBoxSettingsTreeSelector::itemText (int aId) const
{
    return pagePath (idToString (aId));
}

int VBoxSettingsTreeSelector::currentId () const
{
    int id = -1;
    QTreeWidgetItem *item = mTwSelector->currentItem();
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}

int VBoxSettingsTreeSelector::idToIndex (int aId) const
{
    int index = -1;
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Id);
    if (item)
        index = mTwSelector->indexOfTopLevelItem (item);
    return index;
}

int VBoxSettingsTreeSelector::indexToId (int aIndex) const
{
    int id = -1;
    QTreeWidgetItem *item = mTwSelector->topLevelItem (aIndex);
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}

int VBoxSettingsTreeSelector::linkToId (const QString &aLink) const
{
    int id = -1;
    QTreeWidgetItem *item = findItem (mTwSelector, aLink, treeWidget_Link);
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}


void VBoxSettingsTreeSelector::selectById (int aId)
{
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Id);
    if (item)
        mTwSelector->setCurrentItem (item);
}

void VBoxSettingsTreeSelector::setVisibleById (int aId, bool aShow)
{
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Category);
    if (item)
        item->setHidden (aShow);
}

void VBoxSettingsTreeSelector::clear()
{
    mTwSelector->clear();
}

void VBoxSettingsTreeSelector::retranslateUi()
{
    mTwSelector->setFixedWidth (static_cast<QAbstractItemView*> (mTwSelector)
        ->sizeHintForColumn (treeWidget_Category) + 2 * mTwSelector->frameWidth());

    /* Sort selector by the id column */
    mTwSelector->sortItems (treeWidget_Id, Qt::AscendingOrder);
    mTwSelector->resizeColumnToContents (treeWidget_Category);

    /* Add some margin to every item in the tree */
    mTwSelector->addTopBottomMarginToItems (12);
}

void VBoxSettingsTreeSelector::settingsGroupChanged (QTreeWidgetItem *aItem,
                                                     QTreeWidgetItem * /* aPrevItem */)
{
    if (aItem)
    {
        int id = aItem->text (treeWidget_Id).toInt();
        Assert (id >= 0);
        emit categoryChanged (id);
    }
}

/**
 *  Returns a path to the given page of this settings dialog. See ::path() for
 *  details.
 */
QString VBoxSettingsTreeSelector::pagePath (const QString &aMatch) const
{
    QTreeWidgetItem *li =
        findItem (mTwSelector,
                  aMatch,
                  treeWidget_Id);
    return ::path (li);
}

/* Returns first item of 'aView' matching required 'aMatch' value
 * searching the 'aColumn' column. */
QTreeWidgetItem* VBoxSettingsTreeSelector::findItem (QTreeWidget *aView,
                                                     const QString &aMatch,
                                                     int aColumn) const
{
    QList<QTreeWidgetItem*> list =
        aView->findItems (aMatch, Qt::MatchExactly, aColumn);

    return list.count() ? list [0] : 0;
}

QString VBoxSettingsTreeSelector::idToString (int aId) const
{
    return QString ("%1").arg (aId, 2, 10, QLatin1Char ('0'));
}

