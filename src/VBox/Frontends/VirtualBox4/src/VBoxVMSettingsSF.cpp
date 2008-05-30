/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxVMSettingsSF class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "VBoxVMSettingsSF.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "VBoxUtils.h"
#include "QIDialogButtonBox.h"

/* Qt includes */
#include <QLineEdit>
#include <QPushButton>
#include <QDir>
#include <QHeaderView>
#include <QTimer>
#include <QLabel>

class SFTreeViewItem : public QTreeWidgetItem
{
public:

    enum { SFTreeViewItemId = 1001 };

    enum FormatType
    {
        IncorrectFormat = 0,
        EllipsisStart   = 1,
        EllipsisMiddle  = 2,
        EllipsisEnd     = 3,
        EllipsisFile    = 4
    };

    /* root item */
    SFTreeViewItem (QTreeWidget *aParent, const QStringList &aFields,
                    FormatType aFormat) :
        QTreeWidgetItem (aParent, aFields, SFTreeViewItemId), mFormat (aFormat)
    {
        setFirstColumnSpanned (true);
        setFlags (flags() ^ Qt::ItemIsSelectable);
    }

    /* child item */
    SFTreeViewItem (SFTreeViewItem *aParent, const QStringList &aFields,
                    FormatType aFormat) :
        QTreeWidgetItem (aParent, aFields, SFTreeViewItemId), mFormat (aFormat)
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
        return item && item->type() == SFTreeViewItemId ?
            static_cast<SFTreeViewItem*> (item) : 0;
    }

    QString getText (int aIndex) const
    {
        return aIndex >= 0 && aIndex < (int)mTextList.size() ?
            mTextList [aIndex] : QString::null;
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
        int oldSize = treeWidget()->fontMetrics().width (oneString);
        int indentSize = treeWidget()->fontMetrics().width ("x...x");
        int itemIndent = parent() ? treeWidget()->indentation() * 2 :
                                    treeWidget()->indentation();
        if (aColumn == 0)
            indentSize += itemIndent;
        int cWidth = treeWidget()->columnWidth (aColumn);

        /* compress text */
        int start = 0;
        int finish = 0;
        int position = 0;
        int textWidth = 0;
        do {
            textWidth = treeWidget()->fontMetrics().width (oneString);
            if (textWidth + indentSize > cWidth)
            {
                start  = 0;
                finish = oneString.length();

                /* selecting remove position */
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
        } while (textWidth + indentSize > cWidth);

        if (position || mFormat == EllipsisFile) oneString.insert (position, "...");
        int newSize = treeWidget()->fontMetrics().width (oneString);
        setText (aColumn, newSize < oldSize ? oneString : mTextList [aColumn]);
        setToolTip (aColumn, text (aColumn) == getText (aColumn) ?
                    QString::null : getText (aColumn));
    }

    FormatType  mFormat;
    QStringList mTextList;
};


VBoxVMSettingsSF* VBoxVMSettingsSF::mSettings = 0;

VBoxVMSettingsSF::VBoxVMSettingsSF (QWidget *aParent, int aType)
    : QIWithRetranslateUI<QWidget> (aParent)
    , mDialogType (aType)
    , mIsListViewChanged (false)
{
    /* Apply UI decorations */
    Ui::VBoxVMSettingsSF::setupUi (this);

    mTreeView->header()->setMovable (false);
    mTbAdd->setIcon (VBoxGlobal::iconSet (":/add_shared_folder_16px.png",
                                          ":/add_shared_folder_disabled_16px.png"));
    mTbEdit->setIcon (VBoxGlobal::iconSet (":/edit_shared_folder_16px.png",
                                           ":/edit_shared_folder_disabled_16px.png"));
    mTbRemove->setIcon (VBoxGlobal::iconSet (":/revome_shared_folder_16px.png",
                                             ":/revome_shared_folder_disabled_16px.png"));
    connect (mTbAdd, SIGNAL (clicked()), this, SLOT (tbAddPressed()));
    connect (mTbEdit, SIGNAL (clicked()), this, SLOT (tbEditPressed()));
    connect (mTbRemove, SIGNAL (clicked()), this, SLOT (tbRemovePressed()));
    connect (mTreeView, SIGNAL (currentItemChanged (QTreeWidgetItem*, QTreeWidgetItem*)),
             this, SLOT (processCurrentChanged (QTreeWidgetItem*, QTreeWidgetItem*)));
    connect (mTreeView, SIGNAL (itemDoubleClicked (QTreeWidgetItem*, int)),
             this, SLOT (processDoubleClick (QTreeWidgetItem*, int)));
    connect (mTreeView->header(), SIGNAL (sectionResized (int, int, int)),
             this, SLOT (adjustFields()));

    /* Set mTreeView as the focus proxy for the mGbSharedFolders */
    new QIFocusProxy (mGbSharedFolders, mTreeView);

    /* Create mTreeView root items */
    //if (aType == GlobalType)
    //{
    //    QStringList fields;
    //    fields << tr (" Global Folders") /* name */
    //           << QString::number (GlobalType) /* key */;
    //    new SFTreeViewItem (mTreeView, fields, SFTreeViewItem::EllipsisEnd);
    //}
    if (aType & MachineType)
    {
        QStringList fields;
        fields << tr (" Machine Folders") /* name */
               << QString::number (MachineType) /* key */;
        new SFTreeViewItem (mTreeView, fields, SFTreeViewItem::EllipsisEnd);
    }
    if (aType & ConsoleType)
    {
        QStringList fields;
        fields << tr (" Transient Folders") /* name */
               << QString::number (ConsoleType) /* key */;
        new SFTreeViewItem (mTreeView, fields, SFTreeViewItem::EllipsisEnd);
    }
    mTreeView->sortItems (0, Qt::Ascending);

    retranslateUi();
}


void VBoxVMSettingsSF::getFromMachineEx (const CMachine &aMachine,
                                         QWidget *aPage)
{
    mSettings = new VBoxVMSettingsSF (aPage, MachineType);
    QVBoxLayout *layout = new QVBoxLayout (aPage);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget (mSettings);
    mSettings->getFromMachine (aMachine);
}

void VBoxVMSettingsSF::putBackToMachineEx()
{
    mSettings->putBackToMachine();
}


void VBoxVMSettingsSF::getFromGlobal()
{
    AssertMsgFailed (("Global shared folders are not implemented yet\n"));
    //SFTreeViewItem *root = searchRoot (true, GlobalType);
    //root->setHidden (false);
    //getFrom (vboxGlobal().virtualBox().GetSharedFolders().Enumerate(), root);
}

void VBoxVMSettingsSF::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;
    SFTreeViewItem *root = searchRoot (true, MachineType);
    root->setHidden (false);
    getFrom (mMachine.GetSharedFolders().Enumerate(), root);
}

void VBoxVMSettingsSF::getFromConsole (const CConsole &aConsole)
{
    mConsole = aConsole;
    SFTreeViewItem *root = searchRoot (true, ConsoleType);
    root->setHidden (false);
    getFrom (mConsole.GetSharedFolders().Enumerate(), root);
}


void VBoxVMSettingsSF::putBackToGlobal()
{
    AssertMsgFailed (("Global shared folders are not implemented yet\n"));
    //if (!mIsListViewChanged)
    //    return;
    //
    ///* This function is only available for GlobalType dialog */
    //Assert (mDialogType == GlobalType);
    ///* Searching for GlobalType item's root */
    //SFTreeViewItem *root = searchRoot (true, GlobalType);
    //Assert (root);
    //CSharedFolderEnumerator en =
    //    vboxGlobal().virtualBox().GetSharedFolders().Enumerate();
    //putBackTo (en, root);
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
    CSharedFolderEnumerator en = mMachine.GetSharedFolders().Enumerate();
    putBackTo (en, root);
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
    CSharedFolderEnumerator en = mConsole.GetSharedFolders().Enumerate();
    putBackTo (en, root);
}

void VBoxVMSettingsSF::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxVMSettingsSF::retranslateUi (this);

    mTrFull = tr ("Full");
    mTrReadOnly = tr ("Read-only");
}

void VBoxVMSettingsSF::tbAddPressed()
{
    /* Invoke Add-Box Dialog */
    VBoxAddSFDialog dlg (this, VBoxAddSFDialog::AddDialogType,
                         mDialogType & ConsoleType, usedList (true));
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString name = dlg.getName();
    QString path = dlg.getPath();
    bool isPermanent = dlg.getPermanent();
    /* Shared folder's name & path could not be empty */
    Assert (!name.isEmpty() && !path.isEmpty());
    /* Searching root for the new listview item */
    SFTreeViewItem *root = searchRoot (isPermanent);
    Assert (root);
    /* Appending a new listview item to the root */
    QStringList fields;
    fields << name /* name */ << path /* path */
           << (dlg.getWritable() ? mTrFull : mTrReadOnly /* writable? */)
           << "edited" /* mark item as edited */;
    SFTreeViewItem *item = new SFTreeViewItem (root, fields,
                                               SFTreeViewItem::EllipsisFile);
    mTreeView->sortItems (0, Qt::Ascending);
    mTreeView->scrollToItem (item);
    mTreeView->setCurrentItem (item);
    processCurrentChanged (item);
    mTreeView->setFocus();
    adjustList();

    mIsListViewChanged = true;
}

void VBoxVMSettingsSF::tbEditPressed()
{
    /* Check selected item */
    QTreeWidgetItem *selectedItem = mTreeView->selectedItems().size() == 1 ?
                                    mTreeView->selectedItems() [0] : 0;
    SFTreeViewItem *item = selectedItem &&
        selectedItem->type() == SFTreeViewItem::SFTreeViewItemId ?
        static_cast<SFTreeViewItem*> (selectedItem) : 0;
    Assert (item);
    Assert (item->parent());

    /* Invoke Edit-Box Dialog */
    VBoxAddSFDialog dlg (this, VBoxAddSFDialog::EditDialogType,
                         mDialogType & ConsoleType, usedList (false));
    dlg.setPath (item->getText (1));
    dlg.setName (item->getText (0));
    dlg.setPermanent ((SFDialogType)item->parent()->text (1).toInt()
                      != ConsoleType);
    dlg.setWritable (item->getText (2) == mTrFull);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QString name = dlg.getName();
    QString path = dlg.getPath();
    bool isPermanent = dlg.getPermanent();
    /* Shared folder's name & path could not be empty */
    Assert (!name.isEmpty() && !path.isEmpty());
    /* Searching new root for the selected listview item */
    SFTreeViewItem *root = searchRoot (isPermanent);
    Assert (root);
    /* Updating an edited listview item */
    QStringList fields;
    fields << name /* name */ << path /* path */
           << (dlg.getWritable() ? mTrFull : mTrReadOnly /* writable? */)
           << "edited" /* mark item as edited */;
    item->updateText (fields);
    mTreeView->sortItems (0, Qt::Ascending);
    if (item->parent() != root)
    {
        /* Move the selected item into new location */
        item->parent()->takeChild (item->parent()->indexOfChild (item));
        root->insertChild (root->childCount(), item);
        mTreeView->scrollToItem (item);
        mTreeView->setCurrentItem (item);
        processCurrentChanged (item);
        mTreeView->setFocus();
    }
    adjustList();

    mIsListViewChanged = true;
}

void VBoxVMSettingsSF::tbRemovePressed()
{
    QTreeWidgetItem *selectedItem = mTreeView->selectedItems().size() == 1 ?
                                    mTreeView->selectedItems() [0] : 0;
    Assert (selectedItem);
    delete selectedItem;
    adjustList();
    mIsListViewChanged = true;
}


void VBoxVMSettingsSF::processCurrentChanged (
    QTreeWidgetItem *aCurrentItem, QTreeWidgetItem* /* aPreviousItem */)
{
    if (aCurrentItem && aCurrentItem->parent() && !aCurrentItem->isSelected())
        aCurrentItem->setSelected (true);
    QString key = !aCurrentItem ? QString::null :
        aCurrentItem->parent() ? aCurrentItem->parent()->text (1) :
        aCurrentItem->text (1);
    bool addEnabled = aCurrentItem && isEditable (key);
    bool removeEnabled = addEnabled && aCurrentItem->parent();
    mTbAdd->setEnabled (addEnabled);
    mTbEdit->setEnabled (removeEnabled);
    mTbRemove->setEnabled (removeEnabled);
}

void VBoxVMSettingsSF::processDoubleClick (QTreeWidgetItem *aItem,
                                                    int /* aColumn */)
{
    bool editEnabled = aItem && aItem->parent() &&
        isEditable (aItem->parent()->text (1));
    if (editEnabled)
        tbEditPressed();
}


void VBoxVMSettingsSF::adjustList()
{
    /* Adjust two columns size.
     * Watching columns 0&2 to feat 1/3 of total width. */
    int total = mTreeView->viewport()->width();

    mTreeView->resizeColumnToContents (0);
    int w0 = mTreeView->columnWidth (0) < total / 3 ?
             mTreeView->columnWidth (0) : total / 3;

    mTreeView->resizeColumnToContents (2);
    int w2 = mTreeView->columnWidth (2) < total / 3 ?
             mTreeView->columnWidth (2) : total / 3;

    /* We are adjusting columns 0 and 2 and resizing column 1 to feat
     * visible mTreeView' width according two adjusted columns. Due to
     * adjusting column 2 influent column 0 restoring all widths. */
    mTreeView->setColumnWidth (0, w0);
    mTreeView->setColumnWidth (1, total - w0 - w2);
    mTreeView->setColumnWidth (2, w2);
}

void VBoxVMSettingsSF::adjustFields()
{
    QTreeWidgetItem *mainRoot = mTreeView->invisibleRootItem();
    for (int i = 0; i < mainRoot->childCount(); ++ i)
    {
        QTreeWidgetItem *subRoot = mainRoot->child (i);
        for (int j = 0; j < subRoot->childCount(); ++ j)
        {
            SFTreeViewItem *item = subRoot->child (j) &&
                subRoot->child (j)->type() == SFTreeViewItem::SFTreeViewItemId ?
                static_cast<SFTreeViewItem*> (subRoot->child (j)) : 0;
            if (item)
                item->adjustText();
        }
    }
}


void VBoxVMSettingsSF::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);

    /* Adjusting size after all pending show events are processed. */
    QTimer::singleShot (0, this, SLOT (adjustList()));
}


void VBoxVMSettingsSF::removeSharedFolder (const QString & aName,
                                                    const QString & aPath,
                                                    SFDialogType aType)
{
    switch (aType)
    {
        case GlobalType:
        {
            /* This feature is not implemented yet */
            AssertMsgFailed (("Global shared folders are not implemented yet\n"));
            break;
        }
        case MachineType:
        {
            Assert (!mMachine.isNull());
            mMachine.RemoveSharedFolder (aName);
            if (!mMachine.isOk())
                vboxProblem().cannotRemoveSharedFolder (this, mMachine,
                                                        aName, aPath);
            break;
        }
        case ConsoleType:
        {
            Assert (!mConsole.isNull());
            mConsole.RemoveSharedFolder (aName);
            if (!mConsole.isOk())
                vboxProblem().cannotRemoveSharedFolder (this, mConsole,
                                                        aName, aPath);
            break;
        }
        default:
        {
            AssertMsgFailed (("Incorrect shared folder type\n"));
        }
    }
}

void VBoxVMSettingsSF::createSharedFolder (const QString & aName,
                                                    const QString & aPath,
                                                    bool aWritable,
                                                    SFDialogType aType)
{
    switch (aType)
    {
        case GlobalType:
        {
            /* This feature is not implemented yet */
            AssertMsgFailed (("Global shared folders are not implemented yet\n"));
            break;
        }
        case MachineType:
        {
            Assert (!mMachine.isNull());
            mMachine.CreateSharedFolder (aName, aPath, aWritable);
            if (!mMachine.isOk())
                vboxProblem().cannotCreateSharedFolder (this, mMachine,
                                                        aName, aPath);
            break;
        }
        case ConsoleType:
        {
            Assert (!mConsole.isNull());
            mConsole.CreateSharedFolder (aName, aPath, aWritable);
            if (!mConsole.isOk())
                vboxProblem().cannotCreateSharedFolder (this, mConsole,
                                                        aName, aPath);
            break;
        }
        default:
        {
            AssertMsgFailed (("Incorrect shared folder type\n"));
        }
    }
}


void VBoxVMSettingsSF::getFrom (const CSharedFolderEnumerator &aEn,
                                         SFTreeViewItem *aRoot)
{
    while (aEn.HasMore())
    {
        CSharedFolder sf = aEn.GetNext();
        QStringList fields;
        fields << sf.GetName() /* name */
               << sf.GetHostPath() /* path */
               << (sf.GetWritable() ? mTrFull : mTrReadOnly /* writable? */)
               << "not edited" /* initially not edited */;
        new SFTreeViewItem (aRoot, fields, SFTreeViewItem::EllipsisFile);
    }
    aRoot->setExpanded (true);
    mTreeView->sortItems (0, Qt::Ascending);
    mTreeView->setCurrentItem (aRoot->childCount() ? aRoot->child (0) : aRoot);
    processCurrentChanged (aRoot->childCount() ? aRoot->child (0) : aRoot);
}

void VBoxVMSettingsSF::putBackTo (CSharedFolderEnumerator &aEn,
                                  SFTreeViewItem *aRoot)
{
    Assert (!aRoot->text (1).isNull());
    SFDialogType type = (SFDialogType)aRoot->text (1).toInt();

    /* delete all changed folders from vm */
    while (aEn.HasMore())
    {
        CSharedFolder sf = aEn.GetNext();

        /* Iterate through this root's children */
        int i = 0;
        for (; i < aRoot->childCount(); ++ i)
        {
            SFTreeViewItem *item = aRoot->child (i);
            if (item->getText (0) == sf.GetName() &&
                item->getText (3) == "not edited")
                break;
        }

        if (i == aRoot->childCount())
            removeSharedFolder (sf.GetName(), sf.GetHostPath(), type);
    }

    /* save all edited tree widget items as folders */
    for (int i = 0; i < aRoot->childCount(); ++ i)
    {
        SFTreeViewItem *item = aRoot->child (i);

        if (!item->getText (0).isNull() && !item->getText (1).isNull() &&
            item->getText (3) == "edited")
            createSharedFolder (item->getText (0), item->getText (1),
                item->getText (2) == mTrFull ? true : false, type);
    }
}


SFTreeViewItem* VBoxVMSettingsSF::searchRoot (bool aIsPermanent,
                                              SFDialogType aType)
{
    QString type = aType != WrongType ? QString::number (aType) :
        !aIsPermanent ? QString::number (ConsoleType) :
        mDialogType & MachineType ? QString::number (MachineType) :
        QString::number (GlobalType);
    QTreeWidgetItem *mainRoot = mTreeView->invisibleRootItem();

    int i = 0;
    for (; i < mainRoot->childCount(); ++ i)
    {
        if (mainRoot->child (i)->text (1) == type)
            break;
    }

    Assert (i < mainRoot->childCount());
    return i < mainRoot->childCount() &&
           mainRoot->child (i)->type() == SFTreeViewItem::SFTreeViewItemId ?
           static_cast<SFTreeViewItem*> (mainRoot->child (i)) : 0;
}

bool VBoxVMSettingsSF::isEditable (const QString &aKey)
{
    /* mDialogType should be correct */
    Assert (mDialogType);

    SFDialogType type = (SFDialogType)aKey.toInt();
    if (!type) return false;
    return mDialogType & type;
}

SFoldersNameList VBoxVMSettingsSF::usedList (bool aIncludeSelected)
{
    /* Make the used names list: */
    SFoldersNameList list;
    QTreeWidgetItemIterator it (mTreeView);
    while (*it)
    {
        if ((*it)->parent() && (aIncludeSelected || !(*it)->isSelected()) &&
            (*it)->type() == SFTreeViewItem::SFTreeViewItemId)
        {
            SFTreeViewItem *item = static_cast<SFTreeViewItem*> (*it);
            SFDialogType type = (SFDialogType)item->parent()->text (1).toInt();
            list << qMakePair (item->getText (0), type);
        }
        ++ it;
    }
    return list;
}


VBoxAddSFDialog::VBoxAddSFDialog (VBoxVMSettingsSF *aParent,
                                  VBoxAddSFDialog::DialogType aType,
                                  bool aEnableSelector,
                                  const SFoldersNameList &aUsedNames)
    : QIWithRetranslateUI<QDialog> (aParent)
    , mType(aType), mLePath (0), mLeName (0), mCbPermanent (0), mCbReadonly (0)
    , mUsedNames (aUsedNames)
{
    setModal (true);

    QVBoxLayout *mainLayout = new QVBoxLayout (this);

    /* Setup Input layout */
    QGridLayout *inputLayout = new QGridLayout ();
    mainLayout->addLayout (inputLayout);
    mLbPath = new QLabel (this);
    mLePath = new QLineEdit (this);
    mTbPath = new QToolButton (this);
    mTbPath->setAutoRaise (true);
    mLbName = new QLabel (this);
    mLeName = new QLineEdit (this);
    mTbPath->setIcon (VBoxGlobal::iconSet (":/select_file_16px.png",
                                          ":/select_file_dis_16px.png"));
    mTbPath->setFocusPolicy (Qt::TabFocus);
    connect (mLePath, SIGNAL (textChanged (const QString &)),
             this, SLOT (validate()));
    connect (mLeName, SIGNAL (textChanged (const QString &)),
             this, SLOT (validate()));
    connect (mTbPath, SIGNAL (clicked()), this, SLOT (showFileDialog()));
    mCbReadonly = new QCheckBox (this);
    mCbReadonly->setChecked (false);

    inputLayout->addWidget (mLbPath,  0, 0);
    inputLayout->addWidget (mLePath, 0, 1);
    inputLayout->addWidget (mTbPath,  0, 2);
    inputLayout->addWidget (mLbName,  1, 0);
    inputLayout->addWidget (mLeName, 1, 1, 1, 2);
    inputLayout->addWidget (mCbReadonly, 2, 0, 1, 3);

    if (aEnableSelector)
    {
        mCbPermanent = new QCheckBox (this);
        mCbPermanent->setChecked (true);
        inputLayout->addWidget (mCbPermanent, 3, 0, 1, 3);
        connect (mCbPermanent, SIGNAL (toggled (bool)),
                 this, SLOT (validate()));
    }

    /* Setup Button layout */
    mButtonBox = new QIDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mButtonBox->setCenterButtons (true);
    connect (mButtonBox, SIGNAL (accepted()), this, SLOT (accept()));
    connect (mButtonBox, SIGNAL (rejected()), this, SLOT (reject()));
    mainLayout->addWidget (mButtonBox);

    /* Validate fields */
    validate();

    retranslateUi();
}

QString VBoxAddSFDialog::getPath()
{
    return mLePath->text();
}
QString VBoxAddSFDialog::getName()
{
    return mLeName->text();
}
bool VBoxAddSFDialog::getPermanent()
{
    return mCbPermanent ? mCbPermanent->isChecked() : true;
}
bool VBoxAddSFDialog::getWritable()
{
    return !mCbReadonly->isChecked();
}

void VBoxAddSFDialog::setPath (const QString &aPath)
{
    mLePath->setText (aPath);
}
void VBoxAddSFDialog::setName (const QString &aName)
{
    mLeName->setText (aName);
}
void VBoxAddSFDialog::setPermanent (bool aPermanent)
{
    if (mCbPermanent)
    {
        mCbPermanent->setChecked (aPermanent);
        mCbPermanent->setEnabled (!aPermanent);
    }
}
void VBoxAddSFDialog::setWritable (bool aWritable)
{
    mCbReadonly->setChecked (!aWritable);
}

void VBoxAddSFDialog::retranslateUi()
{
    switch (mType)
    {
        case AddDialogType:
            setWindowTitle (tr ("Add Share"));
            break;
        case EditDialogType:
            setWindowTitle (tr ("Edit Share"));
            break;
        default:
            AssertMsgFailed (("Incorrect SF Dialog type\n"));
    }

    mLbPath->setText (tr ("Folder Path"));
    mLePath->setWhatsThis (tr ("Displays the path to an existing folder "
                               "on the host PC."));

    mLbName->setText (tr ("Folder Name"));
    mLeName->setWhatsThis (tr ("Displays the name of the shared folder "
                               "(as it will be seen by the guest OS)."));

    mTbPath->setWhatsThis (tr ("Opens the dialog to select a folder."));

    mCbReadonly->setText (tr ("&Read-only"));
    mCbReadonly->setWhatsThis (tr ("When checked, the guest OS will not "
                                   "be able to write to the specified "
                                   "shared folder."));

    if (mCbPermanent)
        mCbPermanent->setText (tr ("&Make Permanent"));
}

void VBoxAddSFDialog::validate()
{
    int dlgType = static_cast<VBoxVMSettingsSF*>
                  (parent())->dialogType();
    SFDialogType resultType =
        mCbPermanent && !mCbPermanent->isChecked() ? ConsoleType :
        dlgType & MachineType ? MachineType : GlobalType;
    SFolderName pair = qMakePair (mLeName->text(), resultType);

    mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (!mLePath->text().isEmpty() &&
                                                           !mLeName->text().isEmpty() &&
                                                           !mUsedNames.contains (pair));
}

void VBoxAddSFDialog::showFileDialog()
{
    QString folderName = vboxGlobal()
        .getExistingDirectory (mLePath->text().isEmpty() ? QDir::homePath() : mLePath->text(),
                               this,
                               tr ("Select a folder to share"));
    if (folderName.isNull())
        return;

    QDir folder (folderName);
    mLePath->setText (QDir::toNativeSeparators (folder.absolutePath()));
    if (!folder.isRoot())
        /* processing non-root folder */
        mLeName->setText (folder.dirName());
    else
    {
        /* processing root folder */
#if defined (Q_OS_WIN) || defined (Q_OS_OS2)
        mLeName->setText (folderName.toUpper()[0] + "_DRIVE");
#elif defined (Q_OS_UNIX)
        mLeName->setText ("ROOT");
#endif
    }
}

void VBoxAddSFDialog::showEvent (QShowEvent *aEvent)
{
    setFixedHeight (height());
    QDialog::showEvent (aEvent);
}

