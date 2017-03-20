/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSF class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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

/* Local includes */
# include "UIIconPool.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "VBoxUtils.h"
# include "UIMachineSettingsSF.h"
# include "UIMachineSettingsSFDetails.h"

/* Global includes */
# include <QHeaderView>
# include <QTimer>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


class SFTreeViewItem : public QITreeWidgetItem
{
public:

    enum FormatType
    {
        IncorrectFormat = 0,
        EllipsisStart   = 1,
        EllipsisMiddle  = 2,
        EllipsisEnd     = 3,
        EllipsisFile    = 4
    };

    /* Root Item */
    SFTreeViewItem (QITreeWidget *aParent, const QStringList &aFields, FormatType aFormat)
        : QITreeWidgetItem (aParent, aFields), mFormat (aFormat)
    {
        setFirstColumnSpanned (true);
        setFlags (flags() ^ Qt::ItemIsSelectable);
    }

    /* Child Item */
    SFTreeViewItem (SFTreeViewItem *aParent, const QStringList &aFields, FormatType aFormat)
        : QITreeWidgetItem (aParent, aFields), mFormat (aFormat)
    {
        updateText (aFields);
    }

    bool operator< (const QTreeWidgetItem &aOther) const
    {
        /* Root items should always been sorted by id-field. */
        return parentItem() ? text (0).toLower() < aOther.text (0).toLower() :
                              text (1).toLower() < aOther.text (1).toLower();
    }

    SFTreeViewItem* child (int aIndex) const
    {
        QTreeWidgetItem *item = QTreeWidgetItem::child (aIndex);
        return item ? static_cast <SFTreeViewItem*> (item) : 0;
    }

    QString getText (int aIndex) const
    {
        return aIndex >= 0 && aIndex < (int)mTextList.size() ? mTextList [aIndex] : QString::null;
    }

    void updateText (const QStringList &aFields)
    {
        mTextList.clear();
        mTextList << aFields;
        adjustText();
    }

    void adjustText()
    {
        for (int i = 0; i < treeWidget()->columnCount(); ++ i)
            processColumn (i);
    }

    /** Returns default text. */
    virtual QString defaultText() const /* override */
    {
        return parentItem() ?
               tr("%1, %2: %3, %4: %5, %6: %7",
                  "col.1 text, col.2 name: col.2 text, col.3 name: col.3 text, col.4 name: col.4 text")
                 .arg(text(0))
                 .arg(parentTree()->headerItem()->text(1)).arg(text(1))
                 .arg(parentTree()->headerItem()->text(2)).arg(text(2))
                 .arg(parentTree()->headerItem()->text(3)).arg(text(3)) :
               text(0);
    }

private:

    void processColumn (int aColumn)
    {
        QString oneString = getText (aColumn);
        if (oneString.isNull())
            return;
        QFontMetrics fm = treeWidget()->fontMetrics();
        int oldSize = fm.width (oneString);
        int indentSize = fm.width (" ... ");
        int itemIndent = parentItem() ? treeWidget()->indentation() * 2 : treeWidget()->indentation();
        if (aColumn == 0)
            indentSize += itemIndent;
        int cWidth = treeWidget()->columnWidth (aColumn);

        /* Compress text */
        int start = 0;
        int finish = 0;
        int position = 0;
        int textWidth = 0;
        do
        {
            textWidth = fm.width (oneString);
            if (textWidth + indentSize > cWidth)
            {
                start  = 0;
                finish = oneString.length();

                /* Selecting remove position */
                switch (mFormat)
                {
                    case EllipsisStart:
                        position = start;
                        break;
                    case EllipsisMiddle:
                        position = (finish - start) / 2;
                        break;
                    case EllipsisEnd:
                        position = finish - 1;
                        break;
                    case EllipsisFile:
                    {
                        QRegExp regExp ("([\\\\/][^\\\\^/]+[\\\\/]?$)");
                        int newFinish = regExp.indexIn (oneString);
                        if (newFinish != -1)
                            finish = newFinish;
                        position = (finish - start) / 2;
                        break;
                    }
                    default:
                        AssertMsgFailed (("Invalid format type\n"));
                }

                if (position == finish)
                   break;

                oneString.remove (position, 1);
            }
        }
        while (textWidth + indentSize > cWidth);

        if (position || mFormat == EllipsisFile) oneString.insert (position, "...");
        int newSize = fm.width (oneString);
        setText (aColumn, newSize < oldSize ? oneString : mTextList [aColumn]);
        setToolTip (aColumn, text (aColumn) == getText (aColumn) ? QString::null : getText (aColumn));

        /* Calculate item's size-hint */
        setSizeHint (aColumn, QSize (fm.width (QString ("  %1  ").arg (getText (aColumn))), 100));
    }

    FormatType  mFormat;
    QStringList mTextList;
};

UIMachineSettingsSF::UIMachineSettingsSF()
    : mNewAction(0), mEdtAction(0), mDelAction(0)
{
    /* Apply UI decorations */
    Ui::UIMachineSettingsSF::setupUi (this);

    /* Prepare actions */
    mNewAction = new QAction (this);
    mEdtAction = new QAction (this);
    mDelAction = new QAction (this);

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mEdtAction->setShortcut (QKeySequence ("Ctrl+Space"));
    mDelAction->setShortcut (QKeySequence ("Del"));

    mNewAction->setIcon(UIIconPool::iconSet(":/sf_add_16px.png",
                                            ":/sf_add_disabled_16px.png"));
    mEdtAction->setIcon(UIIconPool::iconSet(":/sf_edit_16px.png",
                                            ":/sf_edit_disabled_16px.png"));
    mDelAction->setIcon(UIIconPool::iconSet(":/sf_remove_16px.png",
                                            ":/sf_remove_disabled_16px.png"));

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Prepare tool-bar: */
    m_pFoldersToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
    m_pFoldersToolBar->setOrientation(Qt::Vertical);
    m_pFoldersToolBar->addAction(mNewAction);
    m_pFoldersToolBar->addAction(mEdtAction);
    m_pFoldersToolBar->addAction(mDelAction);

    /* Setup connections */
#if QT_VERSION >= 0x050000
    mTwFolders->header()->setSectionsMovable(false);
#else /* QT_VERSION < 0x050000 */
    mTwFolders->header()->setMovable (false);
#endif /* QT_VERSION < 0x050000 */
    connect (mNewAction, SIGNAL (triggered (bool)), this, SLOT (addTriggered()));
    connect (mEdtAction, SIGNAL (triggered (bool)), this, SLOT (edtTriggered()));
    connect (mDelAction, SIGNAL (triggered (bool)), this, SLOT (delTriggered()));
    connect (mTwFolders, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem *)));
    connect (mTwFolders, SIGNAL (itemDoubleClicked (QTreeWidgetItem *, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem *)));
    connect (mTwFolders, SIGNAL (customContextMenuRequested (const QPoint &)),
             this, SLOT (showContextMenu (const QPoint &)));

    retranslateUi();
}

void UIMachineSettingsSF::resizeEvent (QResizeEvent *aEvent)
{
    NOREF(aEvent);
    adjustList();
}

void UIMachineSettingsSF::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_cache.clear();

    /* Load machine (permanent) shared folders into shared folders cache if possible: */
    if (isSharedFolderTypeSupported(MachineType))
        loadToCacheFrom(MachineType);
    /* Load console (temporary) shared folders into shared folders cache if possible: */
    if (isSharedFolderTypeSupported(ConsoleType))
        loadToCacheFrom(ConsoleType);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSF::loadToCacheFrom(UISharedFolderType sharedFoldersType)
{
    /* Get current shared folders: */
    CSharedFolderVector sharedFolders = getSharedFolders(sharedFoldersType);

    /* For each shared folder: */
    for (int iFolderIndex = 0; iFolderIndex < sharedFolders.size(); ++iFolderIndex)
    {
        /* Prepare cache key & data: */
        QString strSharedFolderKey = QString::number(iFolderIndex);
        UIDataSettingsSharedFolder sharedFolderData;

        /* Check if shared folder exists:  */
        const CSharedFolder &folder = sharedFolders[iFolderIndex];
        if (!folder.isNull())
        {
            /* Gather shared folder values: */
            sharedFolderData.m_type = sharedFoldersType;
            sharedFolderData.m_strName = folder.GetName();
            sharedFolderData.m_strHostPath = folder.GetHostPath();
            sharedFolderData.m_fAutoMount = folder.GetAutoMount();
            sharedFolderData.m_fWritable = folder.GetWritable();
            /* Override shared folder cache key: */
            strSharedFolderKey = sharedFolderData.m_strName;
        }

        /* Cache shared folder data: */
        m_cache.child(strSharedFolderKey).cacheInitialData(sharedFolderData);
    }
}

void UIMachineSettingsSF::getFromCache()
{
    /* Clear list initially: */
    mTwFolders->clear();

    /* Update root items visibility: */
    updateRootItemsVisibility();

    /* Load shared folders data: */
    for (int iFolderIndex = 0; iFolderIndex < m_cache.childCount(); ++iFolderIndex)
    {
        /* Get shared folder data: */
        const UIDataSettingsSharedFolder &sharedFolderData = m_cache.child(iFolderIndex).base();
        /* Prepare item fields: */
        QStringList fields;
        fields << sharedFolderData.m_strName
               << sharedFolderData.m_strHostPath
               << (sharedFolderData.m_fAutoMount ? mTrYes : "")
               << (sharedFolderData.m_fWritable ? mTrFull : mTrReadOnly);
        /* Create new shared folders item: */
        new SFTreeViewItem(root(sharedFolderData.m_type), fields, SFTreeViewItem::EllipsisFile);
    }
    /* Ensure current item fetched: */
    mTwFolders->setCurrentItem(mTwFolders->topLevelItem(0));
    processCurrentChanged(mTwFolders->currentItem());

    /* Polish page finally: */
    polishPage();
}

void UIMachineSettingsSF::putToCache()
{
    /* For each shared folder type: */
    QTreeWidgetItem *pMainRootItem = mTwFolders->invisibleRootItem();
    for (int iFodlersTypeIndex = 0; iFodlersTypeIndex < pMainRootItem->childCount(); ++iFodlersTypeIndex)
    {
        /* Get shared folder root item: */
        SFTreeViewItem *pFolderTypeRoot = static_cast<SFTreeViewItem*>(pMainRootItem->child(iFodlersTypeIndex));
        UISharedFolderType sharedFolderType = (UISharedFolderType)pFolderTypeRoot->text(1).toInt();
        /* For each shared folder of current type: */
        for (int iFoldersIndex = 0; iFoldersIndex < pFolderTypeRoot->childCount(); ++iFoldersIndex)
        {
            SFTreeViewItem *pFolderItem = static_cast<SFTreeViewItem*>(pFolderTypeRoot->child(iFoldersIndex));
            UIDataSettingsSharedFolder sharedFolderData;
            sharedFolderData.m_type = sharedFolderType;
            sharedFolderData.m_strName = pFolderItem->getText(0);
            sharedFolderData.m_strHostPath = pFolderItem->getText(1);
            sharedFolderData.m_fAutoMount = pFolderItem->getText(2) == mTrYes ? true : false;
            sharedFolderData.m_fWritable = pFolderItem->getText(3) == mTrFull ? true : false;
            m_cache.child(sharedFolderData.m_strName).cacheCurrentData(sharedFolderData);
        }
    }
}

void UIMachineSettingsSF::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if shared folders data was changed at all: */
    if (m_cache.wasChanged())
    {
        /* Save machine (permanent) shared folders if possible: */
        if (isSharedFolderTypeSupported(MachineType))
            saveFromCacheTo(MachineType);
        /* Save console (temporary) shared folders if possible: */
        if (isSharedFolderTypeSupported(ConsoleType))
            saveFromCacheTo(ConsoleType);
    }

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSF::saveFromCacheTo(UISharedFolderType sharedFoldersType)
{
    /* For each shared folder data set: */
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < m_cache.childCount(); ++iSharedFolderIndex)
    {
        /* Check if this shared folder data was actually changed: */
        const UISettingsCacheSharedFolder &sharedFolderCache = m_cache.child(iSharedFolderIndex);
        if (sharedFolderCache.wasChanged())
        {
            /* If shared folder was removed: */
            if (sharedFolderCache.wasRemoved())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.base();
                /* Check if thats shared folder of required type before removing: */
                if (sharedFolderData.m_type == sharedFoldersType)
                    removeSharedFolder(sharedFolderCache);
            }

            /* If shared folder was created: */
            if (sharedFolderCache.wasCreated())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.data();
                /* Check if thats shared folder of required type before creating: */
                if (sharedFolderData.m_type == sharedFoldersType)
                    createSharedFolder(sharedFolderCache);
            }

            /* If shared folder was changed: */
            if (sharedFolderCache.wasUpdated())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.data();
                /* Check if thats shared folder of required type before recreating: */
                if (sharedFolderData.m_type == sharedFoldersType)
                {
                    removeSharedFolder(sharedFolderCache);
                    createSharedFolder(sharedFolderCache);
                }
            }
        }
    }
}

void UIMachineSettingsSF::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwFolders);
}

void UIMachineSettingsSF::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSF::retranslateUi(this);

    mNewAction->setText(tr("Add Shared Folder"));
    mEdtAction->setText(tr("Edit Shared Folder"));
    mDelAction->setText(tr("Remove Shared Folder"));

    mNewAction->setWhatsThis(tr("Adds new shared folder."));
    mEdtAction->setWhatsThis(tr("Edits selected shared folder."));
    mDelAction->setWhatsThis(tr("Removes selected shared folder."));

    mNewAction->setToolTip(mNewAction->whatsThis());
    mEdtAction->setToolTip(mEdtAction->whatsThis());
    mDelAction->setToolTip(mDelAction->whatsThis());

    mTrFull = tr("Full");
    mTrReadOnly = tr("Read-only");
    mTrYes = tr("Yes");
}

void UIMachineSettingsSF::addTriggered()
{
    /* Invoke Add-Box Dialog */
    UIMachineSettingsSFDetails dlg (UIMachineSettingsSFDetails::AddType, isSharedFolderTypeSupported(ConsoleType), usedList (true), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString name = dlg.name();
        QString path = dlg.path();
        bool isPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty */
        Assert (!name.isEmpty() && !path.isEmpty());
        /* Appending a new listview item to the root */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isAutoMounted() ? mTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */);
        SFTreeViewItem *item = new SFTreeViewItem (root(isPermanent ? MachineType : ConsoleType),
                                                   fields, SFTreeViewItem::EllipsisFile);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        mTwFolders->scrollToItem (item);
        mTwFolders->setCurrentItem (item);
        processCurrentChanged (item);
        mTwFolders->setFocus();
        adjustList();
    }
}

void UIMachineSettingsSF::edtTriggered()
{
    /* Check selected item */
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    SFTreeViewItem *item = selectedItem ? static_cast <SFTreeViewItem*> (selectedItem) : 0;
    Assert (item);
    Assert (item->parentItem());

    /* Invoke Edit-Box Dialog */
    UIMachineSettingsSFDetails dlg (UIMachineSettingsSFDetails::EditType, isSharedFolderTypeSupported(ConsoleType), usedList (false), this);
    dlg.setPath (item->getText (1));
    dlg.setName (item->getText (0));
    dlg.setPermanent ((UISharedFolderType)item->parentItem()->text (1).toInt() != ConsoleType);
    dlg.setAutoMount (item->getText (2) == mTrYes);
    dlg.setWriteable (item->getText (3) == mTrFull);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString name = dlg.name();
        QString path = dlg.path();
        bool isPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty */
        Assert (!name.isEmpty() && !path.isEmpty());
        /* Searching new root for the selected listview item */
        SFTreeViewItem *pRoot = root(isPermanent ? MachineType : ConsoleType);
        /* Updating an edited listview item */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isAutoMounted() ? mTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */);
        item->updateText (fields);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        if (item->parentItem() != pRoot)
        {
            /* Move the selected item into new location */
            item->parentItem()->takeChild (item->parentItem()->indexOfChild (item));
            pRoot->insertChild (pRoot->childCount(), item);
            mTwFolders->scrollToItem (item);
            mTwFolders->setCurrentItem (item);
            processCurrentChanged (item);
            mTwFolders->setFocus();
        }
        adjustList();
    }
}

void UIMachineSettingsSF::delTriggered()
{
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    Assert (selectedItem);
    delete selectedItem;
    adjustList();
}

void UIMachineSettingsSF::processCurrentChanged (QTreeWidgetItem *aCurrentItem)
{
    if (aCurrentItem && aCurrentItem->parent() && !aCurrentItem->isSelected())
        aCurrentItem->setSelected (true);
    QString key = !aCurrentItem ? QString::null : aCurrentItem->parent() ?
                  aCurrentItem->parent()->text (1) : aCurrentItem->text (1);
    bool addEnabled = aCurrentItem;
    bool removeEnabled = addEnabled && aCurrentItem->parent();
    mNewAction->setEnabled (addEnabled);
    mEdtAction->setEnabled (removeEnabled);
    mDelAction->setEnabled (removeEnabled);
}

void UIMachineSettingsSF::processDoubleClick (QTreeWidgetItem *aItem)
{
    bool editEnabled = aItem && aItem->parent();
    if (editEnabled)
        edtTriggered();
}

void UIMachineSettingsSF::showContextMenu(const QPoint &pos)
{
    QMenu menu;
    QTreeWidgetItem *pItem = mTwFolders->itemAt(pos);
    if (mTwFolders->isEnabled() && pItem && pItem->flags() & Qt::ItemIsSelectable)
    {
        menu.addAction(mEdtAction);
        menu.addAction(mDelAction);
    }
    else
    {
        menu.addAction(mNewAction);
    }
    if (!menu.isEmpty())
        menu.exec(mTwFolders->viewport()->mapToGlobal(pos));
}

void UIMachineSettingsSF::adjustList()
{
    /*
     * Calculates required columns sizes to max out column 2
     * and let all other columns stay at their minimum sizes.
     *
     * Columns
     * 0 = Tree view
     * 1 = Shared Folder name
     * 2 = Auto-mount flag
     * 3 = Writable flag
     */
    QAbstractItemView *itemView = mTwFolders;
    QHeaderView *itemHeader = mTwFolders->header();
    int total = mTwFolders->viewport()->width();

    int mw0 = qMax (itemView->sizeHintForColumn (0), itemHeader->sectionSizeHint (0));
    int mw2 = qMax (itemView->sizeHintForColumn (2), itemHeader->sectionSizeHint (2));
    int mw3 = qMax (itemView->sizeHintForColumn (3), itemHeader->sectionSizeHint (3));

    int w0 = mw0 < total / 4 ? mw0 : total / 4;
    int w2 = mw2 < total / 4 ? mw2 : total / 4;
    int w3 = mw3 < total / 4 ? mw3 : total / 4;

    /* Giving 1st column all the available space. */
    mTwFolders->setColumnWidth (0, w0);
    mTwFolders->setColumnWidth (1, total - w0 - w2 - w3);
    mTwFolders->setColumnWidth (2, w2);
    mTwFolders->setColumnWidth (3, w3);
}

void UIMachineSettingsSF::adjustFields()
{
    QTreeWidgetItem *mainRoot = mTwFolders->invisibleRootItem();
    for (int i = 0; i < mainRoot->childCount(); ++ i)
    {
        QTreeWidgetItem *subRoot = mainRoot->child (i);
        for (int j = 0; j < subRoot->childCount(); ++ j)
        {
            SFTreeViewItem *item = subRoot->child (j) ? static_cast <SFTreeViewItem*> (subRoot->child (j)) : 0;
            if (item)
                item->adjustText();
        }
    }
}

void UIMachineSettingsSF::showEvent (QShowEvent *aEvent)
{
    UISettingsPageMachine::showEvent (aEvent);

    /* Connect header-resize signal just before widget is shown after all the items properly loaded and initialized. */
    connect (mTwFolders->header(), SIGNAL (sectionResized (int, int, int)), this, SLOT (adjustFields()));

    /* Adjusting size after all pending show events are processed. */
    QTimer::singleShot (0, this, SLOT (adjustList()));
}

SFTreeViewItem* UIMachineSettingsSF::root(UISharedFolderType sharedFolderType)
{
    /* Search for the corresponding root item among all the top-level items: */
    SFTreeViewItem *pRootItem = 0;
    QTreeWidgetItem *pMainRootItem = mTwFolders->invisibleRootItem();
    for (int iFolderTypeIndex = 0; iFolderTypeIndex < pMainRootItem->childCount(); ++iFolderTypeIndex)
    {
        /* Get iterated item: */
        QTreeWidgetItem *pIteratedItem = pMainRootItem->child(iFolderTypeIndex);
        /* If iterated item type is what we are looking for: */
        if (pIteratedItem->text(1).toInt() == sharedFolderType)
        {
            /* Remember the item: */
            pRootItem = static_cast<SFTreeViewItem*>(pIteratedItem);
            /* And break further search: */
            break;
        }
    }
    /* Return root item: */
    return pRootItem;
}

SFoldersNameList UIMachineSettingsSF::usedList (bool aIncludeSelected)
{
    /* Make the used names list: */
    SFoldersNameList list;
    QTreeWidgetItemIterator it (mTwFolders);
    while (*it)
    {
        if ((*it)->parent() && (aIncludeSelected || !(*it)->isSelected()))
        {
            SFTreeViewItem *item = static_cast <SFTreeViewItem*> (*it);
            UISharedFolderType type = (UISharedFolderType) item->parentItem()->text (1).toInt();
            list << qMakePair (item->getText (0), type);
        }
        ++ it;
    }
    return list;
}

bool UIMachineSettingsSF::isSharedFolderTypeSupported(UISharedFolderType sharedFolderType) const
{
    bool fIsSharedFolderTypeSupported = false;
    switch (sharedFolderType)
    {
        case MachineType:
            fIsSharedFolderTypeSupported = isMachineInValidMode();
            break;
        case ConsoleType:
            fIsSharedFolderTypeSupported = isMachineOnline();
            break;
        default:
            break;
    }
    return fIsSharedFolderTypeSupported;
}

void UIMachineSettingsSF::updateRootItemsVisibility()
{
    /* Update (show/hide) machine (permanent) root item: */
    setRootItemVisible(MachineType, isSharedFolderTypeSupported(MachineType));
    /* Update (show/hide) console (temporary) root item: */
    setRootItemVisible(ConsoleType, isSharedFolderTypeSupported(ConsoleType));
}

void UIMachineSettingsSF::setRootItemVisible(UISharedFolderType sharedFolderType, bool fVisible)
{
    /* Search for the corresponding root item among all the top-level items: */
    SFTreeViewItem *pRootItem = root(sharedFolderType);
    /* If root item, we are looking for, still not found: */
    if (!pRootItem)
    {
        /* Prepare fields for the new root item: */
        QStringList fields;
        /* Depending on folder type: */
        switch (sharedFolderType)
        {
            case MachineType:
                fields << tr(" Machine Folders") << QString::number(MachineType);
                break;
            case ConsoleType:
                fields << tr(" Transient Folders") << QString::number(ConsoleType);
                break;
            default:
                break;
        }
        /* And create the new root item: */
        pRootItem = new SFTreeViewItem(mTwFolders, fields, SFTreeViewItem::EllipsisEnd);
    }
    /* Expand/collaps it if necessary: */
    pRootItem->setExpanded(fVisible);
    /* And hide/show it if necessary: */
    pRootItem->setHidden(!fVisible);
}

void UIMachineSettingsSF::polishPage()
{
    /* Update widgets availability: */
    mNameSeparator->setEnabled(isMachineInValidMode());
    m_pFoldersToolBar->setEnabled(isMachineInValidMode());
    m_pFoldersToolBar->setEnabled(isMachineInValidMode());

    /* Update root items visibility: */
    updateRootItemsVisibility();
}

CSharedFolderVector UIMachineSettingsSF::getSharedFolders(UISharedFolderType sharedFoldersType)
{
    CSharedFolderVector sharedFolders;
    if (isSharedFolderTypeSupported(sharedFoldersType))
    {
        switch (sharedFoldersType)
        {
            case MachineType:
            {
                AssertMsg(!m_machine.isNull(), ("Machine is NOT set!\n"));
                sharedFolders = m_machine.GetSharedFolders();
                break;
            }
            case ConsoleType:
            {
                AssertMsg(!m_console.isNull(), ("Console is NOT set!\n"));
                sharedFolders = m_console.GetSharedFolders();
                break;
            }
            default:
                break;
        }
    }
    return sharedFolders;
}

bool UIMachineSettingsSF::removeSharedFolder(const UISettingsCacheSharedFolder &folderCache)
{
    /* Get shared folder data: */
    const UIDataSettingsSharedFolder &folderData = folderCache.base();
    QString strName = folderData.m_strName;
    QString strPath = folderData.m_strHostPath;
    UISharedFolderType sharedFoldersType = folderData.m_type;

    /* Get current shared folders: */
    CSharedFolderVector sharedFolders = getSharedFolders(sharedFoldersType);
    /* Check that such shared folder really exists: */
    CSharedFolder sharedFolder;
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < sharedFolders.size(); ++iSharedFolderIndex)
        if (sharedFolders[iSharedFolderIndex].GetName() == strName)
            sharedFolder = sharedFolders[iSharedFolderIndex];
    if (!sharedFolder.isNull())
    {
        /* Remove existing shared folder: */
        switch(sharedFoldersType)
        {
            case MachineType:
            {
                m_machine.RemoveSharedFolder(strName);
                if (!m_machine.isOk())
                {
                    /* Mark the page as failed: */
                    setFailed(true);
                    /* Show error message: */
                    msgCenter().cannotRemoveSharedFolder(m_machine, strName, strPath, this);
                    /* Finish early: */
                    return false;
                }
                break;
            }
            case ConsoleType:
            {
                m_console.RemoveSharedFolder(strName);
                if (!m_console.isOk())
                {
                    /* Mark the page as failed: */
                    setFailed(true);
                    /* Show error message: */
                    msgCenter().cannotRemoveSharedFolder(m_console, strName, strPath, this);
                    /* Finish early: */
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

bool UIMachineSettingsSF::createSharedFolder(const UISettingsCacheSharedFolder &folderCache)
{
    /* Get shared folder data: */
    const UIDataSettingsSharedFolder &folderData = folderCache.data();
    QString strName = folderData.m_strName;
    QString strPath = folderData.m_strHostPath;
    bool fIsWritable = folderData.m_fWritable;
    bool fIsAutoMount = folderData.m_fAutoMount;
    UISharedFolderType sharedFoldersType = folderData.m_type;

    /* Get current shared folders: */
    CSharedFolderVector sharedFolders = getSharedFolders(sharedFoldersType);
    /* Check if such shared folder do not exists: */
    CSharedFolder sharedFolder;
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < sharedFolders.size(); ++iSharedFolderIndex)
        if (sharedFolders[iSharedFolderIndex].GetName() == strName)
            sharedFolder = sharedFolders[iSharedFolderIndex];
    if (sharedFolder.isNull())
    {
        /* Create new shared folder: */
        switch(sharedFoldersType)
        {
            case MachineType:
            {
                m_machine.CreateSharedFolder(strName, strPath, fIsWritable, fIsAutoMount);
                if (!m_machine.isOk())
                {
                    /* Mark the page as failed: */
                    setFailed(true);
                    /* Show error message: */
                    msgCenter().cannotCreateSharedFolder(m_machine, strName, strPath, this);
                    /* Finish early: */
                    return false;
                }
                break;
            }
            case ConsoleType:
            {
                /* Create new shared folder: */
                m_console.CreateSharedFolder(strName, strPath, fIsWritable, fIsAutoMount);
                if (!m_console.isOk())
                {
                    /* Mark the page as failed: */
                    setFailed(true);
                    /* Show error message: */
                    msgCenter().cannotCreateSharedFolder(m_console, strName, strPath, this);
                    /* Finish early: */
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

