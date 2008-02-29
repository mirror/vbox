/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Shared Folders" settings dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/


typedef QPair<QString, VBoxSharedFoldersSettings::SFDialogType> SFolderName;
typedef QValueList<SFolderName> SFoldersNameList;


class VBoxRichListItem : public QListViewItem
{
public:

    enum { QIRichListItemId = 1010 };

    enum FormatType
    {
        IncorrectFormat = 0,
        EllipsisStart   = 1,
        EllipsisMiddle  = 2,
        EllipsisEnd     = 3,
        EllipsisFile    = 4
    };

    VBoxRichListItem (FormatType aFormat, QListView *aParent,
                      const QString& aName, const QString& aNull1,
                      const QString& aNull2, const QString& aKey) :
        QListViewItem (aParent, aName, aNull1, aNull2, aKey), mFormat (aFormat)
    {
    }

    VBoxRichListItem (FormatType aFormat, QListViewItem *aParent,
                      const QString& aName, const QString& aPath,
                      const QString& aAccess, const QString& aEdited) :
        QListViewItem (aParent, aName, aPath, aAccess, aEdited), mFormat (aFormat)
    {
        mTextList << aName << aPath << aAccess << aEdited;
    }

    int rtti() const { return QIRichListItemId; }

    int compare (QListViewItem *aItem, int aColumn, bool aAscending) const
    {
        /* Sorting the children always by name: */
        if (parent() && aItem->parent())
            return QListViewItem::compare (aItem, 0, aAscending);
        /* Sorting the root items always by key: */
        else if (!parent() && !aItem->parent())
            return QListViewItem::compare (aItem, 3, aAscending);
        else
            return QListViewItem::compare (aItem, aColumn, aAscending);
    }

    VBoxRichListItem* nextSibling() const
    {
        QListViewItem *item = QListViewItem::nextSibling();
        return item && item->rtti() == QIRichListItemId ?
            static_cast<VBoxRichListItem*> (item) : 0;
    }

    QString getText (int aIndex) const
    {
        return aIndex >= 0 && aIndex < (int)mTextList.size() ?
            mTextList [aIndex] : QString::null;
    }

    void updateText (int aColumn, const QString &aText)
    {
        if (aColumn >= 0 && aColumn < (int)mTextList.size())
            mTextList [aColumn] = aText;
    }

protected:

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aAlign)
    {
        if (!parent())
        {
            /* Make root items occupy the whole width */
            aWidth = listView()->viewport()->width();

            if (aColumn > 0)
            {
                /* For columns other than the first one, paint the overlapping
                 * portion of the first column after correcting the window */
                aPainter->save();
                QRect wnd = aPainter->window();
                int dx = -listView()->treeStepSize();
                for (int i = 0; i < aColumn; ++ i)
                    dx += listView()->columnWidth (i);
                wnd.moveBy (dx, 0);
                aPainter->setWindow (wnd);
                QListViewItem::paintCell (aPainter, aColorGroup, 0, aWidth, aAlign);
                aPainter->restore();
                return;
            }

            QListViewItem::paintCell (aPainter, aColorGroup, aColumn, aWidth, aAlign);
        }
        else
        {
            processColumn (aColumn, aWidth);
            QListViewItem::paintCell (aPainter, aColorGroup, aColumn, aWidth, aAlign);
        }
    }

    int width (const QFontMetrics &aFontMetrics, const QListView *, int aColumn) const
    {
        return parent() ?
               aFontMetrics.boundingRect (getText (aColumn)).width() +
               aFontMetrics.width ("...x") /* indent size */ : 0;
    }

    void processColumn (int aColumn, int aWidth)
    {
        QString oneString = aColumn >= 0 && aColumn < (int)mTextList.size() ?
            mTextList [aColumn] : QString::null;
        if (oneString.isNull())
            return;
        int oldSize = listView()->fontMetrics().width (oneString);
        int indentSize = listView()->fontMetrics().width ("...x");

        /* compress text */
        int start = 0;
        int finish = 0;
        int position = 0;
        int textWidth = 0;
        do {
            textWidth = listView()->fontMetrics().width (oneString);
            if (textWidth + indentSize > aWidth)
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
                        int newFinish = regExp.search (oneString);
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
        } while (textWidth + indentSize > aWidth);
        if (position || mFormat == EllipsisFile) oneString.insert (position, "...");

        int newSize = listView()->fontMetrics().width (oneString);
        setText (aColumn, newSize < oldSize ? oneString : mTextList [aColumn]);
    }

    FormatType  mFormat;
    QStringList mTextList;
};


class VBoxAddSFDialog : public QDialog
{
    Q_OBJECT

public:

    enum DialogType { AddDialogType, EditDialogType };

    VBoxAddSFDialog (VBoxSharedFoldersSettings *aParent,
                     VBoxAddSFDialog::DialogType aType,
                     bool aEnableSelector /* for "permanent" checkbox */,
                     const SFoldersNameList &aUsedNames)
        : QDialog (aParent, "VBoxAddSFDialog", true /* modal */)
        , mLePath (0), mLeName (0), mCbPermanent (0), mCbReadonly (0)
        , mUsedNames (aUsedNames)
    {
        switch (aType)
        {
            case AddDialogType:
                setCaption (tr ("Add Share"));
                break;
            case EditDialogType:
                setCaption (tr ("Edit Share"));
                break;
            default:
                AssertMsgFailed (("Incorrect SF Dialog type\n"));
        }
        QVBoxLayout *mainLayout = new QVBoxLayout (this, 10, 10, "mainLayout");

        /* Setup Input layout */
        QGridLayout *inputLayout = new QGridLayout (mainLayout, 3, 3, 10, "inputLayout");
        QLabel *lbPath = new QLabel (tr ("Folder Path"), this);
        mLePath = new QLineEdit (this);
        QToolButton *tbPath = new QToolButton (this);
        QLabel *lbName = new QLabel (tr ("Folder Name"), this);
        mLeName = new QLineEdit (this);
        tbPath->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                 "select_file_dis_16px.png"));
        tbPath->setFocusPolicy (QWidget::TabFocus);
        connect (mLePath, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));
        connect (mLeName, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));
        connect (tbPath, SIGNAL (clicked()), this, SLOT (showFileDialog()));
        QWhatsThis::add (mLePath, tr ("Displays the path to an existing folder on the host PC."));
        QWhatsThis::add (mLeName, tr ("Displays the name of the shared folder "
                                      "(as it will be seen by the guest OS)."));
        QWhatsThis::add (tbPath, tr ("Opens the dialog to select a folder."));

        inputLayout->addWidget (lbPath,  0, 0);
        inputLayout->addWidget (mLePath, 0, 1);
        inputLayout->addWidget (tbPath,  0, 2);
        inputLayout->addWidget (lbName,  1, 0);
        inputLayout->addMultiCellWidget (mLeName, 1, 1, 1, 2);

        mCbReadonly = new QCheckBox (tr ("&Read-only"), this);
        QWhatsThis::add (mCbReadonly,
            tr ("When checked, the guest OS will not be able to write to the "
                "specified shared folder."));
        mCbReadonly->setChecked (false);
        inputLayout->addMultiCellWidget (mCbReadonly, 2, 2, 0, 2);

        if (aEnableSelector)
        {
            mCbPermanent = new QCheckBox (tr ("&Make Permanent"), this);
            mCbPermanent->setChecked (true);
            inputLayout->addMultiCellWidget (mCbPermanent, 3, 3, 0, 2);
            connect (mCbPermanent, SIGNAL (toggled (bool)),
                     this, SLOT (validate()));
        }

        /* Setup Button layout */
        QHBoxLayout *buttonLayout = new QHBoxLayout (mainLayout, 10, "buttonLayout");
        mBtOk = new QPushButton (tr ("&OK"), this, "btOk");
        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        QPushButton *btCancel = new QPushButton (tr ("Cancel"), this, "btCancel");
        connect (mBtOk, SIGNAL (clicked()), this, SLOT (accept()));
        connect (btCancel, SIGNAL (clicked()), this, SLOT (reject()));

        buttonLayout->addWidget (mBtOk);
        buttonLayout->addItem (spacer);
        buttonLayout->addWidget (btCancel);

        /* Validate fields */
        validate();
    }

    ~VBoxAddSFDialog() {}

    QString getPath() { return mLePath->text(); }
    QString getName() { return mLeName->text(); }
    bool getPermanent()
    {
        return mCbPermanent ? mCbPermanent->isChecked() : true;
    }
    bool getWritable() { return !mCbReadonly->isChecked(); }

    void setPath (const QString &aPath) { mLePath->setText (aPath); }
    void setName (const QString &aName) { mLeName->setText (aName); }
    void setPermanent (bool aPermanent)
    {
        if (mCbPermanent)
        {
            mCbPermanent->setChecked (aPermanent);
            mCbPermanent->setEnabled (!aPermanent);
        }
    }
    void setWritable (bool aWritable) { mCbReadonly->setChecked (!aWritable); }

private slots:

    void validate()
    {
        VBoxSharedFoldersSettings::SFDialogType dlgType =
            (VBoxSharedFoldersSettings::SFDialogType)
            static_cast<VBoxSharedFoldersSettings*> (parent())->dialogType();
        VBoxSharedFoldersSettings::SFDialogType resultType =
            mCbPermanent && !mCbPermanent->isChecked() ?
            VBoxSharedFoldersSettings::ConsoleType :
            dlgType & VBoxSharedFoldersSettings::MachineType ?
            VBoxSharedFoldersSettings::MachineType :
            VBoxSharedFoldersSettings::GlobalType;
        SFolderName pair = qMakePair (mLeName->text(), resultType);

        mBtOk->setEnabled (!mLePath->text().isEmpty() &&
                           !mLeName->text().isEmpty() &&
                           !mUsedNames.contains (pair));
    }

    void showFileDialog()
    {
        QString folder = vboxGlobal()
            .getExistingDirectory (QDir::rootDirPath(),
                                   this, "addSharedFolderDialog",
                                   tr ("Select a folder to share"));
        if (folder.isNull())
            return;

        QString folderName = QDir::convertSeparators (folder);
        QRegExp commonRule ("[\\\\/]([^\\\\^/]+)[\\\\/]?$");
        QRegExp rootRule ("(([a-zA-Z])[^\\\\^/])?[\\\\/]$");
        if (commonRule.search (folderName) != -1)
        {
            /* processing non-root folder */
            mLePath->setText (folderName.remove (QRegExp ("[\\\\/]$")));
            mLeName->setText (commonRule.cap (1));
        }
        else if (rootRule.search (folderName) != -1)
        {
            /* processing root folder */
            mLePath->setText (folderName);
#if defined (Q_OS_WIN) || defined (Q_OS_OS2)
            mLeName->setText (rootRule.cap (2) + "_DRIVE");
#elif defined (Q_OS_UNIX)
            mLeName->setText ("ROOT");
#endif
        }
        else
            return; /* hm, what type of folder it was? */
    }

private:

    void showEvent (QShowEvent *aEvent)
    {
        setFixedHeight (height());
        QDialog::showEvent (aEvent);
    }

    QPushButton *mBtOk;
    QLineEdit *mLePath;
    QLineEdit *mLeName;
    QCheckBox *mCbPermanent;
    QCheckBox *mCbReadonly;
    SFoldersNameList mUsedNames;
};


void VBoxSharedFoldersSettings::init()
{
    mDialogType = WrongType;
    listView->setSorting (0);
    new QIListViewSelectionPreserver (this, listView);
    listView->setShowToolTips (false);
    listView->setRootIsDecorated (true);
    listView->header()->setMovingEnabled (false);
    tbAdd->setIconSet (VBoxGlobal::iconSet ("add_shared_folder_16px.png",
                                            "add_shared_folder_disabled_16px.png"));
    tbEdit->setIconSet (VBoxGlobal::iconSet ("edit_shared_folder_16px.png",
                                             "edit_shared_folder_disabled_16px.png"));
    tbRemove->setIconSet (VBoxGlobal::iconSet ("revome_shared_folder_16px.png",
                                               "revome_shared_folder_disabled_16px.png"));
    connect (tbAdd, SIGNAL (clicked()), this, SLOT (tbAddPressed()));
    connect (tbEdit, SIGNAL (clicked()), this, SLOT (tbEditPressed()));
    connect (tbRemove, SIGNAL (clicked()), this, SLOT (tbRemovePressed()));
    connect (listView, SIGNAL (currentChanged (QListViewItem *)),
             this, SLOT (processCurrentChanged (QListViewItem *)));

    /* Make after-paining list update to ensure all columns repainted correctly. */
    connect (listView->header(), SIGNAL (sizeChange (int, int, int)),
             this, SLOT (updateList()));

    mIsListViewChanged = false;

    listView->viewport()->installEventFilter (this);

    mTrFull = tr ("Full");
    mTrReadOnly = tr ("Read-only");
}

void VBoxSharedFoldersSettings::showEvent (QShowEvent *aEvent)
{
    QWidget::showEvent (aEvent);

    /* Adjusting size after all pending show events are processed. */
    QTimer::singleShot (0, this, SLOT (adjustList()));
}


void VBoxSharedFoldersSettings::updateList()
{
    /* Updating list after all pending cell-repaint enevts. */
    QTimer::singleShot (0, listView, SLOT (updateContents()));
}

void VBoxSharedFoldersSettings::adjustList()
{
    /* Adjust two columns size.
     * Watching columns 0&2 to feat 1/3 of total width. */
    int total = listView->viewport()->width();

    listView->adjustColumn (0);
    int w0 = listView->columnWidth (0) < total / 3 ?
             listView->columnWidth (0) : total / 3;

    listView->adjustColumn (2);
    int w2 = listView->columnWidth (2) < total / 3 ?
             listView->columnWidth (2) : total / 3;

    /* We are adjusting columns 0 and 2 and resizing column 1 to feat
     * visible listView' width according two adjusted columns. Due to
     * adjusting column 2 influent column 0 restoring all widths. */
    listView->setColumnWidth (0, w0);
    listView->setColumnWidth (1, total - w0 - w2);
    listView->setColumnWidth (2, w2);
}

bool VBoxSharedFoldersSettings::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process & show auto Tool-Tip for partially hidden listview items. */
    if (aObject == listView->viewport() && aEvent->type() == QEvent::MouseMove)
    {
        QMouseEvent *e = static_cast<QMouseEvent*> (aEvent);
        QListViewItem *i = listView->itemAt (e->pos());
        VBoxRichListItem *item = i && i->rtti() == VBoxRichListItem::QIRichListItemId ?
            static_cast<VBoxRichListItem*> (i) : 0;
        if (item)
        {
            int delta = e->pos().x();
            int id = 0;
            for (; id < listView->columns(); ++ id)
            {
                if (delta < listView->columnWidth (id))
                    break;
                delta -= listView->columnWidth (id);
            }

            QString curText = QToolTip::textFor (listView->viewport());
            QString newText = item->text (id) != item->getText (id) ?
                              item->getText (id) : QString::null;

            if (newText != curText)
            {
                QToolTip::remove (listView->viewport());
                QToolTip::add (listView->viewport(), newText);
            }
        }
        else
            QToolTip::remove (listView->viewport());
    }

    return QWidget::eventFilter (aObject, aEvent);
}

void VBoxSharedFoldersSettings::setDialogType (int aType)
{
    mDialogType = aType;
}


void VBoxSharedFoldersSettings::removeSharedFolder (const QString & aName,
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

void VBoxSharedFoldersSettings::createSharedFolder (const QString & aName,
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


void VBoxSharedFoldersSettings::getFromGlobal()
{
    /* This feature is not implemented yet */
    AssertMsgFailed (("Global shared folders are not implemented yet\n"));

    /*
    QString name = tr (" Global Folders");
    QString key (QString::number (GlobalType));
    VBoxRichListItem *root = new VBoxRichListItem (VBoxRichListItem::EllipsisEnd,
        listView, name, QString::null, QString::null, key);
    getFrom (vboxGlobal().virtualBox().GetSharedFolders().Enumerate(), root);
    */
}

void VBoxSharedFoldersSettings::getFromMachine (const CMachine &aMachine)
{
    mMachine = aMachine;
    QString name = tr (" Machine Folders");
    QString key (QString::number (MachineType));
    VBoxRichListItem *root = new VBoxRichListItem (VBoxRichListItem::EllipsisEnd,
        listView, name, QString::null, QString::null, key);
    getFrom (mMachine.GetSharedFolders().Enumerate(), root);
}

void VBoxSharedFoldersSettings::getFromConsole (const CConsole &aConsole)
{
    mConsole = aConsole;
    QString name = tr (" Transient Folders");
    QString key (QString::number (ConsoleType));
    VBoxRichListItem *root = new VBoxRichListItem (VBoxRichListItem::EllipsisEnd,
        listView, name, QString::null, QString::null, key);
    getFrom (mConsole.GetSharedFolders().Enumerate(), root);
}

void VBoxSharedFoldersSettings::getFrom (const CSharedFolderEnumerator &aEn,
                                         QListViewItem *aRoot)
{
    aRoot->setSelectable (false);
    while (aEn.HasMore())
    {
        CSharedFolder sf = aEn.GetNext();
        new VBoxRichListItem (VBoxRichListItem::EllipsisFile, aRoot,
                              sf.GetName(), sf.GetHostPath(),
                              sf.GetWritable() ? mTrFull : mTrReadOnly,
                              "not edited");
    }
    listView->setOpen (aRoot, true);
    listView->setCurrentItem (aRoot->firstChild() ? aRoot->firstChild() : aRoot);
    processCurrentChanged (aRoot->firstChild() ? aRoot->firstChild() : aRoot);
}


void VBoxSharedFoldersSettings::putBackToGlobal()
{
    /* This feature is not implemented yet */
    AssertMsgFailed (("Global shared folders are not implemented yet\n"));

    /*
    if (!mIsListViewChanged) return;
    // This function is only available for GlobalType dialog
    Assert (mDialogType == GlobalType);
    // Searching for GlobalType item's root
    QListViewItem *root = listView->findItem (QString::number (GlobalType), 3);
    Assert (root);
    CSharedFolderEnumerator en = vboxGlobal().virtualBox().GetSharedFolders().Enumerate();
    putBackTo (en, root);
    */
}

void VBoxSharedFoldersSettings::putBackToMachine()
{
    if (!mIsListViewChanged)
        return;

    /* This function is only available for MachineType dialog */
    Assert (mDialogType & MachineType);
    /* Searching for MachineType item's root */
    QListViewItem *root = listView->findItem (QString::number (MachineType), 3);
    Assert (root);
    CSharedFolderEnumerator en = mMachine.GetSharedFolders().Enumerate();
    putBackTo (en, root);
}

void VBoxSharedFoldersSettings::putBackToConsole()
{
    if (!mIsListViewChanged)
        return;

    /* This function is only available for ConsoleType dialog */
    Assert (mDialogType & ConsoleType);
    /* Searching for ConsoleType item's root */
    QListViewItem *root = listView->findItem (QString::number (ConsoleType), 3);
    Assert (root);
    CSharedFolderEnumerator en = mConsole.GetSharedFolders().Enumerate();
    putBackTo (en, root);
}

void VBoxSharedFoldersSettings::putBackTo (CSharedFolderEnumerator &aEn,
                                           QListViewItem *aRoot)
{
    Assert (!aRoot->text (3).isNull());
    SFDialogType type = (SFDialogType)aRoot->text (3).toInt();

    /* deleting all changed folders of the list */
    while (aEn.HasMore())
    {
        CSharedFolder sf = aEn.GetNext();

        /* Search for this root's items */
        QListViewItem *firstItem = aRoot->firstChild();
        VBoxRichListItem *item = firstItem &&
            firstItem->rtti() == VBoxRichListItem::QIRichListItemId ?
            static_cast<VBoxRichListItem*> (firstItem) : 0;
        while (item)
        {
            if (item->getText (0) == sf.GetName() &&
                item->getText (1) == sf.GetHostPath() &&
                item->getText (3) == "not edited")
                break;
            item = item->nextSibling();
        }
        if (item)
            continue;
        removeSharedFolder (sf.GetName(), sf.GetHostPath(), type);
    }

    /* saving all machine related list view items */
    QListViewItem *iterator = aRoot->firstChild();
    while (iterator)
    {
        VBoxRichListItem *item = 0;
        if (iterator->rtti() == VBoxRichListItem::QIRichListItemId)
            item = static_cast<VBoxRichListItem*> (iterator);
        if (item && !item->getText (0).isNull() && !item->getText (1).isNull()
            && item->getText (3) == "edited")
            createSharedFolder (item->getText (0), item->getText (1),
                                item->getText (2) == mTrFull ? true : false, type);
        iterator = iterator->nextSibling();
    }
}


QListViewItem* VBoxSharedFoldersSettings::searchRoot (bool aIsPermanent)
{
    if (!aIsPermanent)
        return listView->findItem (QString::number (ConsoleType), 3);
    else if (mDialogType & MachineType)
        return listView->findItem (QString::number (MachineType), 3);
    else
        return listView->findItem (QString::number (GlobalType), 3);
}

void VBoxSharedFoldersSettings::tbAddPressed()
{
    /* Make the used names list: */
    SFoldersNameList usedList;
    QListViewItemIterator it (listView);
    while (*it)
    {
        if ((*it)->parent() && (*it)->rtti() == VBoxRichListItem::QIRichListItemId)
        {
            VBoxRichListItem *item = static_cast<VBoxRichListItem*> (*it);
            SFDialogType type = (SFDialogType)item->parent()->text (3).toInt();
            usedList << qMakePair (item->getText (0), type);
        }
        ++ it;
    }

    /* Invoke Add-Box Dialog */
    VBoxAddSFDialog dlg (this, VBoxAddSFDialog::AddDialogType,
                         mDialogType & ConsoleType, usedList);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString name = dlg.getName();
    QString path = dlg.getPath();
    bool isPermanent = dlg.getPermanent();
    /* Shared folder's name & path could not be empty */
    Assert (!name.isEmpty() && !path.isEmpty());
    /* Searching root for the new listview item */
    QListViewItem *root = searchRoot (isPermanent);
    Assert (root);
    /* Appending a new listview item to the root */
    VBoxRichListItem *item = new VBoxRichListItem (
        VBoxRichListItem::EllipsisFile, root, name, path,
        dlg.getWritable() ? mTrFull : mTrReadOnly, "edited");
    /* Make the created item selected */
    listView->ensureItemVisible (item);
    listView->setCurrentItem (item);
    processCurrentChanged (item);
    listView->setFocus();

    mIsListViewChanged = true;
}

void VBoxSharedFoldersSettings::tbEditPressed()
{
    /* Make the used names list: */
    SFoldersNameList usedList;
    QListViewItemIterator it (listView);
    while (*it)
    {
        if ((*it)->parent() && !(*it)->isSelected() &&
            (*it)->rtti() == VBoxRichListItem::QIRichListItemId)
        {
            VBoxRichListItem *item = static_cast<VBoxRichListItem*> (*it);
            SFDialogType type = (SFDialogType)item->parent()->text (3).toInt();
            usedList << qMakePair (item->getText (0), type);
        }
        ++ it;
    }

    /* Check selected item */
    QListViewItem *selectedItem = listView->selectedItem();
    VBoxRichListItem *item =
        selectedItem->rtti() == VBoxRichListItem::QIRichListItemId ?
        static_cast<VBoxRichListItem*> (selectedItem) : 0;
    Assert (item);
    Assert (item->parent());
    /* Invoke Edit-Box Dialog */
    VBoxAddSFDialog dlg (this, VBoxAddSFDialog::EditDialogType,
                         mDialogType & ConsoleType, usedList);
    dlg.setPath (item->getText (1));
    dlg.setName (item->getText (0));
    dlg.setPermanent ((SFDialogType)item->parent()->text (3).toInt()
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
    QListViewItem *root = searchRoot (isPermanent);
    Assert (root);
    /* Updating an edited listview item */
    item->updateText (3, "edited");
    item->updateText (2, dlg.getWritable() ? mTrFull : mTrReadOnly);
    item->updateText (1, path);
    item->updateText (0, name);
    if (item->parent() != root)
    {
        /* Move the selected item into new location */
        item->parent()->takeItem (item);
        root->insertItem (item);

        /* Make the created item selected */
        listView->ensureItemVisible (item);
        listView->setCurrentItem (item);
        processCurrentChanged (item);
        listView->setFocus();
    }
    item->repaint();

    mIsListViewChanged = true;
}

void VBoxSharedFoldersSettings::tbRemovePressed()
{
    Assert (listView->selectedItem());
    delete listView->selectedItem();
    mIsListViewChanged = true;
}


void VBoxSharedFoldersSettings::processCurrentChanged (QListViewItem *aItem)
{
    if (aItem && aItem->isSelectable() && listView->selectedItem() != aItem)
        listView->setSelected (aItem, true);
    bool addEnabled = aItem &&
                      (isEditable (aItem->text (3)) ||
                       (aItem->parent() && isEditable (aItem->parent()->text (3))));
    bool removeEnabled = aItem && aItem->parent() &&
                         isEditable (aItem->parent()->text (3));
    tbAdd->setEnabled (addEnabled);
    tbEdit->setEnabled (removeEnabled);
    tbRemove->setEnabled (removeEnabled);
}

void VBoxSharedFoldersSettings::processDoubleClick (QListViewItem *aItem)
{
    bool editEnabled = aItem && aItem->parent() &&
        isEditable (aItem->parent()->text (3));
    if (editEnabled)
        tbEditPressed();
}

bool VBoxSharedFoldersSettings::isEditable (const QString &aKey)
{
    /* mDialogType should be setup already */
    Assert (mDialogType);

    SFDialogType type = (SFDialogType)aKey.toInt();
    if (!type) return false;
    return mDialogType & type;
}


#include "VBoxSharedFoldersSettings.ui.moc"
