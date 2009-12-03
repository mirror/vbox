/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSF class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

/* Global includes */
#include <QHeaderView>
#include <QTimer>

/* Local includes */
#include "VBoxVMSettingsSF.h"
#include "VBoxVMSettingsSFDetails.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxUtils.h"

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

VBoxVMSettingsSF::VBoxVMSettingsSF (int aType, QWidget *aParent)
    : VBoxSettingsPage (aParent)
    , mDialogType (aType)
    , mIsListViewChanged (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSF::setupUi (this);

    /* Prepare actions */
    mNewAction = new QAction (this);
    mEdtAction = new QAction (this);
    mDelAction = new QAction (this);

    mNewAction->setShortcut (QKeySequence ("Ins"));
    mEdtAction->setShortcut (QKeySequence ("Ctrl+Space"));
    mDelAction->setShortcut (QKeySequence ("Del"));

    mNewAction->setIcon (VBoxGlobal::iconSet (":/add_shared_folder_16px.png",
                                              ":/add_shared_folder_disabled_16px.png"));
    mEdtAction->setIcon (VBoxGlobal::iconSet (":/edit_shared_folder_16px.png",
                                              ":/edit_shared_folder_disabled_16px.png"));
    mDelAction->setIcon (VBoxGlobal::iconSet (":/revome_shared_folder_16px.png",
                                              ":/revome_shared_folder_disabled_16px.png"));

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

    /* Create mTwFolders root items */
#if 0
    if (aType == GlobalType)
    {
        QStringList fields;
        fields << tr (" Global Folders") /* name */ << QString::number (GlobalType) /* key */;
        new SFTreeViewItem (mTwFolders, fields, SFTreeViewItem::EllipsisEnd);
    }
#endif
    if (aType & MachineType)
    {
        QStringList fields;
        fields << tr (" Machine Folders") /* name */ << QString::number (MachineType) /* key */;
        new SFTreeViewItem (mTwFolders, fields, SFTreeViewItem::EllipsisEnd);
    }
    if (aType & ConsoleType)
    {
        QStringList fields;
        fields << tr (" Transient Folders") /* name */ << QString::number (ConsoleType) /* key */;
        new SFTreeViewItem (mTwFolders, fields, SFTreeViewItem::EllipsisEnd);
    }
    mTwFolders->sortItems (0, Qt::AscendingOrder);

    retranslateUi();
}

void VBoxVMSettingsSF::getFromGlobal()
{
    AssertMsgFailed (("Global shared folders are not supported now!\n"));
#if 0
    SFTreeViewItem *root = searchRoot (true, GlobalType);
    root->setHidden (false);
    getFrom (vboxGlobal().virtualBox().GetSharedFolders(), root);
#endif
}

void VBoxVMSettingsSF::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;
    SFTreeViewItem *root = searchRoot (true, MachineType);
    root->setHidden (false);
    getFrom (mMachine.GetSharedFolders(), root);
}

void VBoxVMSettingsSF::getFromConsole (const CConsole &aConsole)
{
    mConsole = aConsole;
    SFTreeViewItem *root = searchRoot (true, ConsoleType);
    root->setHidden (false);
    getFrom (mConsole.GetSharedFolders(), root);
}

void VBoxVMSettingsSF::putBackToGlobal()
{
    AssertMsgFailed (("Global shared folders are not supported now!\n"));
#if 0
    if (!mIsListViewChanged)
        return;
    /* This function is only available for GlobalType dialog */
    Assert (mDialogType == GlobalType);
    /* Searching for GlobalType item's root */
    SFTreeViewItem *root = searchRoot (true, GlobalType);
    Assert (root);
    CSharedFolderVector vec = vboxGlobal().virtualBox().GetSharedFolders();
    putBackTo (vec, root);
#endif
}

void VBoxVMSettingsSF::putBackToMachine()
{
    if (!mIsListViewChanged)
        return;
    /* This function is only available for MachineType dialog */
    Assert (mDialogType & MachineType);
    /* Searching for MachineType item's root */
    SFTreeViewItem *root = searchRoot (true,  MachineType);
    Assert (root);
    CSharedFolderVector sfvec = mMachine.GetSharedFolders();
    putBackTo (sfvec, root);
}

void VBoxVMSettingsSF::putBackToConsole()
{
    if (!mIsListViewChanged)
        return;
    /* This function is only available for ConsoleType dialog */
    Assert (mDialogType & ConsoleType);
    /* Searching for ConsoleType item's root */
    SFTreeViewItem *root = searchRoot (true, ConsoleType);
    Assert (root);
    CSharedFolderVector sfvec = mConsole.GetSharedFolders();
    putBackTo (sfvec, root);
}

int VBoxVMSettingsSF::dialogType() const
{
     return mDialogType;
}

void VBoxVMSettingsSF::getFrom (const CMachine &aMachine)
{
    getFromMachine (aMachine);
}

void VBoxVMSettingsSF::putBackTo()
{
    putBackToMachine();
}

void VBoxVMSettingsSF::setOrderAfter (QWidget *aWidget)
{
    setTabOrder (aWidget, mTwFolders);
}

void VBoxVMSettingsSF::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsSF::retranslateUi (this);

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
}

void VBoxVMSettingsSF::addTriggered()
{
    /* Invoke Add-Box Dialog */
    VBoxVMSettingsSFDetails dlg (VBoxVMSettingsSFDetails::AddType, mDialogType & ConsoleType, usedList (true), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString name = dlg.name();
        QString path = dlg.path();
        bool isPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty */
        Assert (!name.isEmpty() && !path.isEmpty());
        /* Searching root for the new listview item */
        SFTreeViewItem *root = searchRoot (isPermanent);
        Assert (root);
        /* Appending a new listview item to the root */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */)
               << "edited" /* mark item as edited */;
        SFTreeViewItem *item = new SFTreeViewItem (root, fields, SFTreeViewItem::EllipsisFile);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        mTwFolders->scrollToItem (item);
        mTwFolders->setCurrentItem (item);
        processCurrentChanged (item);
        mTwFolders->setFocus();
        adjustList();

        mIsListViewChanged = true;
    }
}

void VBoxVMSettingsSF::edtTriggered()
{
    /* Check selected item */
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    SFTreeViewItem *item = selectedItem && selectedItem->type() == SFTreeViewItem::SFTreeViewItemType ?
                           static_cast <SFTreeViewItem*> (selectedItem) : 0;
    Assert (item);
    Assert (item->parent());

    /* Invoke Edit-Box Dialog */
    VBoxVMSettingsSFDetails dlg (VBoxVMSettingsSFDetails::EditType, mDialogType & ConsoleType, usedList (false), this);
    dlg.setPath (item->getText (1));
    dlg.setName (item->getText (0));
    dlg.setPermanent ((SFDialogType)item->parent()->text (1).toInt() != ConsoleType);
    dlg.setWriteable (item->getText (2) == mTrFull);
    if (dlg.exec() == QDialog::Accepted)
    {
        QString name = dlg.name();
        QString path = dlg.path();
        bool isPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty */
        Assert (!name.isEmpty() && !path.isEmpty());
        /* Searching new root for the selected listview item */
        SFTreeViewItem *root = searchRoot (isPermanent);
        Assert (root);
        /* Updating an edited listview item */
        QStringList fields;
        fields << name /* name */ << path /* path */
               << (dlg.isWriteable() ? mTrFull : mTrReadOnly /* writable? */)
               << "edited" /* mark item as edited */;
        item->updateText (fields);
        mTwFolders->sortItems (0, Qt::AscendingOrder);
        if (item->parent() != root)
        {
            /* Move the selected item into new location */
            item->parent()->takeChild (item->parent()->indexOfChild (item));
            root->insertChild (root->childCount(), item);
            mTwFolders->scrollToItem (item);
            mTwFolders->setCurrentItem (item);
            processCurrentChanged (item);
            mTwFolders->setFocus();
        }
        adjustList();

        mIsListViewChanged = true;
    }
}

void VBoxVMSettingsSF::delTriggered()
{
    QTreeWidgetItem *selectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems() [0] : 0;
    Assert (selectedItem);
    delete selectedItem;
    adjustList();
    mIsListViewChanged = true;
}

void VBoxVMSettingsSF::processCurrentChanged (QTreeWidgetItem *aCurrentItem)
{
    if (aCurrentItem && aCurrentItem->parent() && !aCurrentItem->isSelected())
        aCurrentItem->setSelected (true);
    QString key = !aCurrentItem ? QString::null : aCurrentItem->parent() ?
                  aCurrentItem->parent()->text (1) : aCurrentItem->text (1);
    bool addEnabled = aCurrentItem && isEditable (key);
    bool removeEnabled = addEnabled && aCurrentItem->parent();
    mNewAction->setEnabled (addEnabled);
    mEdtAction->setEnabled (removeEnabled);
    mDelAction->setEnabled (removeEnabled);
}

void VBoxVMSettingsSF::processDoubleClick (QTreeWidgetItem *aItem)
{
    bool editEnabled = aItem && aItem->parent() && isEditable (aItem->parent()->text (1));
    if (editEnabled)
        edtTriggered();
}

void VBoxVMSettingsSF::showContextMenu (const QPoint &aPos)
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

void VBoxVMSettingsSF::adjustList()
{
    /* Calculating required columns size & watching those columns (0 and 2) to feat 1/3 of total width. */
    QAbstractItemView *itemView = mTwFolders;
    QHeaderView *itemHeader = mTwFolders->header();
    int total = mTwFolders->viewport()->width();
    int mw0 = qMax (itemView->sizeHintForColumn (0), itemHeader->sectionSizeHint (0));
    int mw2 = qMax (itemView->sizeHintForColumn (2), itemHeader->sectionSizeHint (2));
    int w0 = mw0 < total / 3 ? mw0 : total / 3;
    int w2 = mw2 < total / 3 ? mw2 : total / 3;

    /* Giving 1st column all the available space. */
    mTwFolders->setColumnWidth (0, w0);
    mTwFolders->setColumnWidth (1, total - w0 - w2);
    mTwFolders->setColumnWidth (2, w2);
}

void VBoxVMSettingsSF::adjustFields()
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

void VBoxVMSettingsSF::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);

    /* Connect header-resize signal just before widget is shown after all the items properly loaded and initialized. */
    connect (mTwFolders->header(), SIGNAL (sectionResized (int, int, int)), this, SLOT (adjustFields()));

    /* Adjusting size after all pending show events are processed. */
    QTimer::singleShot (0, this, SLOT (adjustList()));
}

void VBoxVMSettingsSF::createSharedFolder (const QString &aName, const QString &aPath, bool aWritable, SFDialogType aType)
{
    switch (aType)
    {
        case GlobalType:
        {
            /* This feature is not supported now */
            AssertMsgFailed (("Global shared folders are not supported now!\n"));
            break;
        }
        case MachineType:
        {
            Assert (!mMachine.isNull());
            mMachine.CreateSharedFolder (aName, aPath, aWritable);
            if (!mMachine.isOk())
                vboxProblem().cannotCreateSharedFolder (this, mMachine, aName, aPath);
            break;
        }
        case ConsoleType:
        {
            Assert (!mConsole.isNull());
            mConsole.CreateSharedFolder (aName, aPath, aWritable);
            if (!mConsole.isOk())
                vboxProblem().cannotCreateSharedFolder (this, mConsole, aName, aPath);
            break;
        }
        default:
        {
            AssertMsgFailed (("Incorrect shared folder type\n"));
        }
    }
}

void VBoxVMSettingsSF::removeSharedFolder (const QString &aName, const QString &aPath, SFDialogType aType)
{
    switch (aType)
    {
        case GlobalType:
        {
            /* This feature is not supported now */
            AssertMsgFailed (("Global shared folders are not supported now!\n"));
            break;
        }
        case MachineType:
        {
            Assert (!mMachine.isNull());
            mMachine.RemoveSharedFolder (aName);
            if (!mMachine.isOk())
                vboxProblem().cannotRemoveSharedFolder (this, mMachine, aName, aPath);
            break;
        }
        case ConsoleType:
        {
            Assert (!mConsole.isNull());
            mConsole.RemoveSharedFolder (aName);
            if (!mConsole.isOk())
                vboxProblem().cannotRemoveSharedFolder (this, mConsole, aName, aPath);
            break;
        }
        default:
        {
            AssertMsgFailed (("Incorrect shared folder type\n"));
        }
    }
}

void VBoxVMSettingsSF::getFrom (const CSharedFolderVector &aVec, SFTreeViewItem *aRoot)
{
    for (int i = 0; i < aVec.size(); ++ i)
    {
        CSharedFolder sf = aVec [i];
        QStringList fields;
        fields << sf.GetName() /* name */ << sf.GetHostPath() /* path */
               << (sf.GetWritable() ? mTrFull : mTrReadOnly /* writable? */)
               << "not edited" /* initially not edited */;
        new SFTreeViewItem (aRoot, fields, SFTreeViewItem::EllipsisFile);
    }
    aRoot->setExpanded (true);
    mTwFolders->sortItems (0, Qt::AscendingOrder);
    mTwFolders->setCurrentItem (aRoot->childCount() ? aRoot->child (0) : aRoot);
    processCurrentChanged (aRoot->childCount() ? aRoot->child (0) : aRoot);
}

void VBoxVMSettingsSF::putBackTo (CSharedFolderVector &aVec, SFTreeViewItem *aRoot)
{
    Assert (!aRoot->text (1).isNull());
    SFDialogType type = (SFDialogType) aRoot->text (1).toInt();

    /* Delete all changed folders from vm */
    for (int idx = 0; idx < aVec.size(); ++ idx)
    {
        CSharedFolder sf = aVec [idx];

        /* Iterate through this root's children */
        int i = 0;
        for (; i < aRoot->childCount(); ++ i)
        {
            SFTreeViewItem *item = aRoot->child (i);
            if (item->getText (0) == sf.GetName() && item->getText (3) == "not edited")
                break;
        }

        if (i == aRoot->childCount())
            removeSharedFolder (sf.GetName(), sf.GetHostPath(), type);
    }

    /* Save all edited tree widget items as folders */
    for (int i = 0; i < aRoot->childCount(); ++ i)
    {
        SFTreeViewItem *item = aRoot->child (i);

        if (!item->getText (0).isNull() && !item->getText (1).isNull() && item->getText (3) == "edited")
            createSharedFolder (item->getText (0), item->getText (1), item->getText (2) == mTrFull ? true : false, type);
    }
}

SFTreeViewItem* VBoxVMSettingsSF::searchRoot (bool aIsPermanent, SFDialogType aType)
{
    QString type = aType != WrongType ? QString::number (aType) : !aIsPermanent ? QString::number (ConsoleType) :
                   mDialogType & MachineType ? QString::number (MachineType) : QString::number (GlobalType);
    QTreeWidgetItem *mainRoot = mTwFolders->invisibleRootItem();

    int i = 0;
    for (; i < mainRoot->childCount(); ++ i)
    {
        if (mainRoot->child (i)->text (1) == type)
            break;
    }

    Assert (i < mainRoot->childCount());
    return i < mainRoot->childCount() && mainRoot->child (i)->type() == SFTreeViewItem::SFTreeViewItemType ?
           static_cast <SFTreeViewItem*> (mainRoot->child (i)) : 0;
}

bool VBoxVMSettingsSF::isEditable (const QString &aKey)
{
    /* mDialogType should be correct */
    Assert (mDialogType);

    SFDialogType type = (SFDialogType) aKey.toInt();
    if (!type) return false;
    return mDialogType & type;
}

SFoldersNameList VBoxVMSettingsSF::usedList (bool aIncludeSelected)
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
            SFDialogType type = (SFDialogType) item->parent()->text (1).toInt();
            list << qMakePair (item->getText (0), type);
        }
        ++ it;
    }
    return list;
}

