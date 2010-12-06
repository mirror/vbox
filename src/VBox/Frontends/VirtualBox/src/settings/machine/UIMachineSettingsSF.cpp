/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIMachineSettingsSF class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxUtils.h"
#include "UIMachineSettingsSF.h"
#include "UIMachineSettingsSFDetails.h"

/* Global includes */
#include <QHeaderView>
#include <QTimer>

class SFTreeViewItem : public QTreeWidgetItem
{
public:

    enum { SFTreeViewItemType = QTreeWidgetItem::UserType + 1 };

    enum FormatType
    {
        IncorrectFormat = 0,
        EllipsisStart   = 1,
        EllipsisMiddle  = 2,
        EllipsisEnd     = 3,
        EllipsisFile    = 4
    };

    /* Root Item */
    SFTreeViewItem (QTreeWidget *aParent, const QStringList &aFields, FormatType aFormat)
        : QTreeWidgetItem (aParent, aFields, SFTreeViewItemType), mFormat (aFormat)
    {
        setFirstColumnSpanned (true);
        setFlags (flags() ^ Qt::ItemIsSelectable);
    }

    /* Child Item */
    SFTreeViewItem (SFTreeViewItem *aParent, const QStringList &aFields, FormatType aFormat)
        : QTreeWidgetItem (aParent, aFields, SFTreeViewItemType), mFormat (aFormat)
    {
        updateText (aFields);
    }

    bool operator< (const QTreeWidgetItem &aOther) const
    {
        /* Root items should always been sorted by id-field. */
        return parent() ? text (0).toLower() < aOther.text (0).toLower() :
                          text (1).toLower() < aOther.text (1).toLower();
    }

    SFTreeViewItem* child (int aIndex) const
    {
        QTreeWidgetItem *item = QTreeWidgetItem::child (aIndex);
        return item && item->type() == SFTreeViewItemType ? static_cast <SFTreeViewItem*> (item) : 0;
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

private:

    void processColumn (int aColumn)
    {
        QString oneString = getText (aColumn);
        if (oneString.isNull())
            return;
        QFontMetrics fm = treeWidget()->fontMetrics();
        int oldSize = fm.width (oneString);
        int indentSize = fm.width (" ... ");
        int itemIndent = parent() ? treeWidget()->indentation() * 2 : treeWidget()->indentation();
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
    : m_type(WrongType)
    , mNewAction(0), mEdtAction(0), mDelAction(0)
    , mIsListViewChanged(false)
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

    mNewAction->setIcon(UIIconPool::iconSet(":/add_shared_folder_16px.png",
                                            ":/add_shared_folder_disabled_16px.png"));
    mEdtAction->setIcon(UIIconPool::iconSet(":/edit_shared_folder_16px.png",
                                            ":/edit_shared_folder_disabled_16px.png"));
    mDelAction->setIcon(UIIconPool::iconSet(":/remove_shared_folder_16px.png",
                                            ":/remove_shared_folder_disabled_16px.png"));

    /* Prepare toolbar */
    mTbFolders->setUsesTextLabel (false);
    mTbFolders->setIconSize (QSize (16, 16));
    mTbFolders->setOrientation (Qt::Vertical);
    mTbFolders->addAction (mNewAction);
    mTbFolders->addAction (mEdtAction);
    mTbFolders->addAction (mDelAction);

    /* Setup connections */
    mTwFolders->header()->setMovable (false);
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

void UIMachineSettingsSF::loadDirectlyFrom(const CConsole &console)
{
    loadToCacheFromMachine(console.GetMachine());
    loadToCacheFromConsole(console);
    getFromCache();
}

void UIMachineSettingsSF::saveDirectlyTo(CConsole &console)
{
    putToCache();
    saveFromCacheToConsole(console);
    CMachine machine = console.GetMachine();
    saveFromCacheToMachine(machine);
}

void UIMachineSettingsSF::resizeEvent (QResizeEvent *aEvent)
{
    NOREF(aEvent);
    adjustList();
}

/* Load data to cashe from corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsSF::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Fill internal variables with corresponding values: */
    loadToCacheFromMachine(m_machine);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSF::loadToCacheFromMachine(const CMachine &machine)
{
    /* Update dialog type: */
    if (m_type == WrongType)
        m_type = MachineType;
    /* Load machine items into internal cache: */
    loadToCacheFromVector(machine.GetSharedFolders(), MachineType);
}

void UIMachineSettingsSF::loadToCacheFromConsole(const CConsole &console)
{
    /* Update dialog type: */
    if (m_type == WrongType || m_type == MachineType)
        m_type = ConsoleType;
    /* Load console items into internal cache: */
    loadToCacheFromVector(console.GetSharedFolders(), ConsoleType);
}

void UIMachineSettingsSF::loadToCacheFromVector(const CSharedFolderVector &vector, UISharedFolderType type)
{
    /* Cache shared folders in internal variables: */
    for (int iFolderIndex = 0; iFolderIndex < vector.size(); ++iFolderIndex)
    {
        const CSharedFolder &folder = vector[iFolderIndex];
        UISharedFolderData data;
        data.m_type = type;
        data.m_strName = folder.GetName();
        data.m_strHostPath = folder.GetHostPath();
        data.m_fAutoMount = folder.GetAutoMount();
        data.m_fWritable = folder.GetWritable();
        m_cache.m_items << data;
    }
}

/* Load data to corresponding widgets from cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsSF::getFromCache()
{
    /* Apply internal variables data to QWidget(s): */
    if (m_type == MachineType || m_type == ConsoleType)
        root(MachineType);
    if (m_type == ConsoleType)
        root(ConsoleType);
    for (int iFolderIndex = 0; iFolderIndex < m_cache.m_items.size(); ++iFolderIndex)
    {
        /* Get iterated folder's data: */
        const UISharedFolderData &data = m_cache.m_items[iFolderIndex];
        /* Prepare list item's fields: */
        QStringList fields;
        fields << data.m_strName << data.m_strHostPath
               << (data.m_fAutoMount ? mTrYes : "")
               << (data.m_fWritable ? mTrFull : mTrReadOnly);
        /* Searching for item's root: */
        SFTreeViewItem *pItemsRoot = root(data.m_type);
        /* Create new folder list item: */
        new SFTreeViewItem(pItemsRoot, fields, SFTreeViewItem::EllipsisFile);
    }
    /* Sort populated item's list: */
    mTwFolders->sortItems(0, Qt::AscendingOrder);
    /* Ensure current item fetched: */
    mTwFolders->setCurrentItem(mTwFolders->topLevelItem(0));
    processCurrentChanged(mTwFolders->currentItem());
}

/* Save data from corresponding widgets to cache,
 * this task SHOULD be performed in GUI thread only: */
void UIMachineSettingsSF::putToCache()
{
    /* Reset cache: */
    m_cache.m_items.clear();
    /* Gather internal variables data from QWidget(s): */
    QTreeWidgetItem *pMainRootItem = mTwFolders->invisibleRootItem();
    /* Iterate other all the list top-level items: */
    for (int iFodlersTypeIndex = 0; iFodlersTypeIndex < pMainRootItem->childCount(); ++iFodlersTypeIndex)
    {
        SFTreeViewItem *pFolderTypeRoot = static_cast<SFTreeViewItem*>(pMainRootItem->child(iFodlersTypeIndex));
        UISharedFolderType type = (UISharedFolderType)pFolderTypeRoot->text(1).toInt();
        AssertMsg(type != WrongType, ("Incorrent folders type!"));
        /* Iterate other all the folder items: */
        for (int iFoldersIndex = 0; iFoldersIndex < pFolderTypeRoot->childCount(); ++iFoldersIndex)
        {
            SFTreeViewItem *pFolderItem = static_cast<SFTreeViewItem*>(pFolderTypeRoot->child(iFoldersIndex));
            UISharedFolderData data;
            data.m_type = type;
            data.m_strName = pFolderItem->getText(0);
            data.m_strHostPath = pFolderItem->getText(1);
            data.m_fAutoMount = pFolderItem->getText(2) == mTrYes ? true : false;
            data.m_fWritable = pFolderItem->getText(3) == mTrFull ? true : false;
            m_cache.m_items << data;
        }
    }
}

/* Save data from cache to corresponding external object(s),
 * this task COULD be performed in other than GUI thread: */
void UIMachineSettingsSF::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Gather corresponding values from internal variables: */
    saveFromCacheToMachine(m_machine);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSF::saveFromCacheToMachine(CMachine &machine)
{
    /* Check if items were NOT changed: */
    if (!mIsListViewChanged)
        return;

    /* Delete all machine folders first: */
    const CSharedFolderVector &folders = machine.GetSharedFolders();
    for (int iFolderIndex = 0; iFolderIndex < folders.size() && !failed(); ++iFolderIndex)
    {
        const CSharedFolder &folder = folders[iFolderIndex];
        QString strFolderName = folder.GetName();
        QString strFolderPath = folder.GetHostPath();
        machine.RemoveSharedFolder(strFolderName);
        if (!machine.isOk())
        {
            /* Mark the page as failed: */
            setFailed(true);
            /* Show error message: */
            vboxProblem().cannotRemoveSharedFolder(machine, strFolderName, strFolderPath);
        }
    }

    /* Save all new machine folders: */
    for (int iFolderIndex = 0; iFolderIndex < m_cache.m_items.size() && !failed(); ++iFolderIndex)
    {
        const UISharedFolderData &data = m_cache.m_items[iFolderIndex];
        if (data.m_type == MachineType)
        {
            machine.CreateSharedFolder(data.m_strName, data.m_strHostPath, data.m_fWritable, data.m_fAutoMount);
            if (!machine.isOk())
            {
                /* Mark the page as failed: */
                setFailed(true);
                /* Show error message: */
                vboxProblem().cannotCreateSharedFolder(machine, data.m_strName, data.m_strHostPath);
            }
        }
    }
}

void UIMachineSettingsSF::saveFromCacheToConsole(CConsole &console)
{
    /* Check if items were NOT changed: */
    if (!mIsListViewChanged)
        return;

    /* Delete all console folders first: */
    const CSharedFolderVector &folders = console.GetSharedFolders();
    for (int iFolderIndex = 0; iFolderIndex < folders.size() && !failed(); ++iFolderIndex)
    {
        const CSharedFolder &folder = folders[iFolderIndex];
        QString strFolderName = folder.GetName();
        QString strFolderPath = folder.GetHostPath();
        console.RemoveSharedFolder(strFolderName);
        if (!console.isOk())
        {
            /* Mark the page as failed: */
            setFailed(true);
            /* Show error message: */
            vboxProblem().cannotRemoveSharedFolder(console, strFolderName, strFolderPath);
        }
    }

    /* Save all new console folders: */
    for (int iFolderIndex = 0; iFolderIndex < m_cache.m_items.size() && !failed(); ++iFolderIndex)
    {
        const UISharedFolderData &data = m_cache.m_items[iFolderIndex];
        if (data.m_type == ConsoleType)
        {
            console.CreateSharedFolder(data.m_strName, data.m_strHostPath, data.m_fWritable, data.m_fAutoMount);
            if (!console.isOk())
            {
                /* Mark the page as failed: */
                setFailed(true);
                /* Show error message: */
                vboxProblem().cannotCreateSharedFolder(console, data.m_strName, data.m_strHostPath);
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
    /* Translate uic generated strings */
    Ui::UIMachineSettingsSF::retranslateUi (this);

    mNewAction->setText (tr ("&Add Shared Folder"));
    mEdtAction->setText (tr ("&Edit Shared Folder"));
    mDelAction->setText (tr ("&Remove Shared Folder"));

    mNewAction->setToolTip (mNewAction->text().remove ('&') +
        QString (" (%1)").arg (mNewAction->shortcut().toString()));
    mEdtAction->setToolTip (mEdtAction->text().remove ('&') +
        QString (" (%1)").arg (mEdtAction->shortcut().toString()));
    mDelAction->setToolTip (mDelAction->text().remove ('&') +
        QString (" (%1)").arg (mDelAction->shortcut().toString()));

    mNewAction->setWhatsThis (tr ("Adds a new shared folder definition."));
    mEdtAction->setWhatsThis (tr ("Edits the selected shared folder definition."));
    mDelAction->setWhatsThis (tr ("Removes the selected shared folder definition."));

    mTrFull = tr ("Full");
    mTrReadOnly = tr ("Read-only");
    mTrYes = tr ("Yes"); /** @todo Need to figure out if this string is necessary at all! */
}

void UIMachineSettingsSF::addTriggered()
{
    /* Invoke Add-Box Dialog */
    UIMachineSettingsSFDetails dlg (UIMachineSettingsSFDetails::AddType, m_type == ConsoleType, usedList (true), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString name = dlg.name();
        QString path = dlg.path();
        bool isPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty */
        Assert (!name.isEmpty() && !path.isEmpty());
        /* Searching root for the new listview item */
        SFTreeViewItem *pRoot = root(isPermanent ? MachineType : ConsoleType);
        Assert (pRoot);
        /* Appending a new listview item to the root */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isAutoMounted() ? mTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */);
        SFTreeViewItem *item = new SFTreeViewItem (pRoot, fields, SFTreeViewItem::EllipsisFile);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        mTwFolders->scrollToItem (item);
        mTwFolders->setCurrentItem (item);
        processCurrentChanged (item);
        mTwFolders->setFocus();
        adjustList();

        mIsListViewChanged = true;
    }
}

void UIMachineSettingsSF::edtTriggered()
{
    /* Check selected item */
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    SFTreeViewItem *item = selectedItem && selectedItem->type() == SFTreeViewItem::SFTreeViewItemType ?
                           static_cast <SFTreeViewItem*> (selectedItem) : 0;
    Assert (item);
    Assert (item->parent());

    /* Invoke Edit-Box Dialog */
    UIMachineSettingsSFDetails dlg (UIMachineSettingsSFDetails::EditType, m_type == ConsoleType, usedList (false), this);
    dlg.setPath (item->getText (1));
    dlg.setName (item->getText (0));
    dlg.setPermanent ((UISharedFolderType)item->parent()->text (1).toInt() != ConsoleType);
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
        Assert (pRoot);
        /* Updating an edited listview item */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isAutoMounted() ? mTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */);
        item->updateText (fields);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        if (item->parent() != pRoot)
        {
            /* Move the selected item into new location */
            item->parent()->takeChild (item->parent()->indexOfChild (item));
            pRoot->insertChild (pRoot->childCount(), item);
            mTwFolders->scrollToItem (item);
            mTwFolders->setCurrentItem (item);
            processCurrentChanged (item);
            mTwFolders->setFocus();
        }
        adjustList();

        mIsListViewChanged = true;
    }
}

void UIMachineSettingsSF::delTriggered()
{
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    Assert (selectedItem);
    delete selectedItem;
    adjustList();
    mIsListViewChanged = true;
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

void UIMachineSettingsSF::showContextMenu (const QPoint &aPos)
{
    QMenu menu;
    QTreeWidgetItem *item = mTwFolders->itemAt (aPos);
    if (item && item->flags() & Qt::ItemIsSelectable)
    {
        menu.addAction (mEdtAction);
        menu.addAction (mDelAction);
    }
    else
    {
        menu.addAction (mNewAction);
    }
    menu.exec (mTwFolders->viewport()->mapToGlobal (aPos));
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
            SFTreeViewItem *item = subRoot->child (j) &&
                                   subRoot->child (j)->type() == SFTreeViewItem::SFTreeViewItemType ?
                                   static_cast <SFTreeViewItem*> (subRoot->child (j)) : 0;
            if (item)
                item->adjustText();
        }
    }
}

void UIMachineSettingsSF::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);

    /* Connect header-resize signal just before widget is shown after all the items properly loaded and initialized. */
    connect (mTwFolders->header(), SIGNAL (sectionResized (int, int, int)), this, SLOT (adjustFields()));

    /* Adjusting size after all pending show events are processed. */
    QTimer::singleShot (0, this, SLOT (adjustList()));
}

SFTreeViewItem* UIMachineSettingsSF::root(UISharedFolderType type)
{
    /* Prepare empty item: */
    SFTreeViewItem *pRootItem = 0;
    /* Get top-level root item: */
    QTreeWidgetItem *pMainRootItem = mTwFolders->invisibleRootItem();
    /* Iterate other the all root items: */
    for (int iFolderTypeIndex = 0; iFolderTypeIndex < pMainRootItem->childCount(); ++iFolderTypeIndex)
    {
        /* Get iterated item: */
        QTreeWidgetItem *pIteratedItem = pMainRootItem->child(iFolderTypeIndex);
        /* If iterated item's type is what we are looking for: */
        if (pIteratedItem->text(1).toInt() == type)
        {
            /* Remember the item: */
            pRootItem = static_cast<SFTreeViewItem*>(pIteratedItem);
            /* And break further search: */
            break;
        }
    }
    /* If root item we are looking for still not found: */
    if (!pRootItem)
    {
        /* Preparing fields: */
        QStringList fields;
        /* Depending on folder type: */
        switch (type)
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
        /* Creating root: */
        pRootItem = new SFTreeViewItem(mTwFolders, fields, SFTreeViewItem::EllipsisEnd);
        /* We should show it (its not?): */
        pRootItem->setHidden(false);
        /* Expand it: */
        pRootItem->setExpanded(true);
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
        if ((*it)->parent() && (aIncludeSelected || !(*it)->isSelected()) &&
            (*it)->type() == SFTreeViewItem::SFTreeViewItemType)
        {
            SFTreeViewItem *item = static_cast <SFTreeViewItem*> (*it);
            UISharedFolderType type = (UISharedFolderType) item->parent()->text (1).toInt();
            list << qMakePair (item->getText (0), type);
        }
        ++ it;
    }
    return list;
}

