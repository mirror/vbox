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
#include "VBoxToolBar.h"

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

/* VBoxSettingsTreeViewSelector */

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

VBoxSettingsTreeViewSelector::VBoxSettingsTreeViewSelector (QWidget *aParent /* = NULL */)
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

QWidget *VBoxSettingsTreeViewSelector::widget() const
{
    return mTwSelector;
}

void VBoxSettingsTreeViewSelector::addItem (const QIcon &aIcon, 
                                        const QString &aText, 
                                        int aId, 
                                        const QString &aLink)
{
    QTreeWidgetItem *item = new QTreeWidgetItem (mTwSelector, QStringList() << QString (" %1 ").arg (aText)
                                                                            << idToString (aId)
                                                                            << aLink);
    item->setIcon (treeWidget_Category, aIcon);
}

QString VBoxSettingsTreeViewSelector::itemText (int aId) const
{
    return pagePath (idToString (aId));
}

int VBoxSettingsTreeViewSelector::currentId () const
{
    int id = -1;
    QTreeWidgetItem *item = mTwSelector->currentItem();
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}

int VBoxSettingsTreeViewSelector::idToIndex (int aId) const
{
    int index = -1;
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Id);
    if (item)
        index = mTwSelector->indexOfTopLevelItem (item);
    return index;
}

int VBoxSettingsTreeViewSelector::indexToId (int aIndex) const
{
    int id = -1;
    QTreeWidgetItem *item = mTwSelector->topLevelItem (aIndex);
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}

int VBoxSettingsTreeViewSelector::linkToId (const QString &aLink) const
{
    int id = -1;
    QTreeWidgetItem *item = findItem (mTwSelector, aLink, treeWidget_Link);
    if (item)
        id = item->text (treeWidget_Id).toInt();
    return id;
}


void VBoxSettingsTreeViewSelector::selectById (int aId)
{
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Id);
    if (item)
        mTwSelector->setCurrentItem (item);
}

void VBoxSettingsTreeViewSelector::setVisibleById (int aId, bool aShow)
{
    QTreeWidgetItem *item = findItem (mTwSelector, idToString (aId), treeWidget_Category);
    if (item)
        item->setHidden (aShow);
}

void VBoxSettingsTreeViewSelector::clear()
{
    mTwSelector->clear();
}

void VBoxSettingsTreeViewSelector::polish()
{
    mTwSelector->setFixedWidth (static_cast<QAbstractItemView*> (mTwSelector)
        ->sizeHintForColumn (treeWidget_Category) + 2 * mTwSelector->frameWidth());

    /* Sort selector by the id column */
    mTwSelector->sortItems (treeWidget_Id, Qt::AscendingOrder);
    mTwSelector->resizeColumnToContents (treeWidget_Category);

    /* Add some margin to every item in the tree */
    mTwSelector->addTopBottomMarginToItems (12);
}

void VBoxSettingsTreeViewSelector::settingsGroupChanged (QTreeWidgetItem *aItem,
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
QString VBoxSettingsTreeViewSelector::pagePath (const QString &aMatch) const
{
    QTreeWidgetItem *li =
        findItem (mTwSelector,
                  aMatch,
                  treeWidget_Id);
    return ::path (li);
}

/* Returns first item of 'aView' matching required 'aMatch' value
 * searching the 'aColumn' column. */
QTreeWidgetItem* VBoxSettingsTreeViewSelector::findItem (QTreeWidget *aView,
                                                     const QString &aMatch,
                                                     int aColumn) const
{
    QList<QTreeWidgetItem*> list =
        aView->findItems (aMatch, Qt::MatchExactly, aColumn);

    return list.count() ? list [0] : 0;
}

QString VBoxSettingsTreeViewSelector::idToString (int aId) const
{
    return QString ("%1").arg (aId, 2, 10, QLatin1Char ('0'));
}

/* VBoxSettingsToolBarSelector */

class SelectorAction: public QAction
{
public:
    SelectorAction (const QIcon &aIcon, const QString &aText, int aId, const QString &aLink, QObject *aParent)
        : QAction (aIcon, aText, aParent)
        , mId (aId)
        , mLink (aLink)
    {
        setCheckable (true);
    }

    int id() const { return mId; }
    QString link() const { return mLink; }

private:
    int mId;
    QString mLink;
};

VBoxSettingsToolBarSelector::VBoxSettingsToolBarSelector (QWidget *aParent /* = NULL */)
    :VBoxSettingsSelector (aParent)
{
    /* Init the toolbar */
    mTbSelector = new VBoxToolBar (aParent);
    mTbSelector->setUsesTextLabel (true);
    mTbSelector->setUsesBigPixmaps (true);
    /* Init the action group for house keeping */
    mActionGroup = new QActionGroup (this);
    mActionGroup->setExclusive (true);
    connect (mActionGroup, SIGNAL (triggered (QAction*)),
             this, SLOT (settingsGroupChanged (QAction*)));
}

QWidget *VBoxSettingsToolBarSelector::widget() const
{
    return mTbSelector;
}

void VBoxSettingsToolBarSelector::addItem (const QIcon &aIcon, 
                                        const QString &aText, 
                                        int aId, 
                                        const QString &aLink)
{
    SelectorAction *action = new SelectorAction (aIcon, aText, aId, aLink, this);

    mActionGroup->addAction (action);
    mTbSelector->addAction (action);
}

QString VBoxSettingsToolBarSelector::itemText (int aId) const
{
    QString result;
    SelectorAction *action = findAction (aId);
    if (action)
        result = action->text();
    return result;
}

int VBoxSettingsToolBarSelector::currentId () const
{
    SelectorAction *action = static_cast<SelectorAction*> (mActionGroup->checkedAction());
    int id = -1;
    if (action)
        id = action->id();
    return id;
}

int VBoxSettingsToolBarSelector::idToIndex (int aId) const
{
    int index = -1;
    QList<QAction*> list = mActionGroup->actions();
    for (int a = 0; a < list.count(); ++a)
    {
        SelectorAction *sa = static_cast<SelectorAction*> (list.at(a));
        if (sa &&
            sa->id() == aId)
        {
            index = a;
            break;
        }
    }
    return index;
}

int VBoxSettingsToolBarSelector::indexToId (int aIndex) const
{
    int id = -1;
    QList<QAction*> list = mActionGroup->actions();
    if (aIndex > -1 && 
        aIndex < list.count())
    {
        SelectorAction *sa = static_cast<SelectorAction*> (list.at (aIndex));
        if (sa)
            id = sa->id();
    }
    return id;
}

int VBoxSettingsToolBarSelector::linkToId (const QString &aLink) const
{
    int id = -1;
    QList<QAction*> list = mActionGroup->actions();
    for (int a = 0; a < list.count(); ++a)
    {
        SelectorAction *sa = static_cast<SelectorAction*> (list.at(a));
        if (sa &&
            sa->link() == aLink)
        {
            id = sa->id();
            break;
        }
    }
    return id;
}


void VBoxSettingsToolBarSelector::selectById (int aId)
{
    SelectorAction *action = findAction (aId);

    if (action)
        action->trigger();
}

void VBoxSettingsToolBarSelector::setVisibleById (int aId, bool aShow)
{
    SelectorAction *action = findAction (aId);

    if (action)
        action->setVisible (aShow);
}

void VBoxSettingsToolBarSelector::clear()
{
    QList<QAction*> list = mActionGroup->actions();
    foreach (QAction *action, list)
       delete action; 
}

int VBoxSettingsToolBarSelector::minWidth() const
{
    return mTbSelector->sizeHint().width() + 2 * 10;
}

void VBoxSettingsToolBarSelector::settingsGroupChanged (QAction *aAction)
{
    SelectorAction *action = static_cast<SelectorAction*> (aAction);
    if (action)
        emit categoryChanged (action->id());
}

SelectorAction* VBoxSettingsToolBarSelector::findAction (int aId) const
{
    int index = idToIndex (aId);
    SelectorAction *sa = NULL;
    QList<QAction*> list = mActionGroup->actions();
    if (index > -1 &&
        index < list.count())
        sa = static_cast<SelectorAction*> (list.at (index));
    return sa;
}

