/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "Shared Folders" settings dialog UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
                      const QString& aLabel1, const QString& aLabel2) :
        QListViewItem (aParent, aLabel1, aLabel2), mFormat (aFormat)
    {
        mTextList << aLabel1 << aLabel2;
    }

    int rtti() const { return QIRichListItemId; }

    const QString& getText (int aIndex) { return mTextList [aIndex]; }

protected:

    void paintCell (QPainter *aPainter, const QColorGroup &aColorGroup,
                    int aColumn, int aWidth, int aAlign)
    {
        processColumn (aColumn, aWidth);
        QListViewItem::paintCell (aPainter, aColorGroup, aColumn, aWidth, aAlign);
    }

    void processColumn (int aColumn, int aWidth)
    {
        QString oneString = mTextList [aColumn];
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
                        QRegExp regExp ("([\\/][^\\^/]+[\\/]?$)");
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

    VBoxAddSFDialog (QWidget *aParent) :
        QDialog (aParent, "VBoxAddSFDialog", true /* modal */),
        mLePath (0), mLeName (0)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout (this, 10, 10, "mainLayout");

        /* Setup Input layout */
        QGridLayout *inputLayout = new QGridLayout (mainLayout, 2, 3, 10, "inputLayout");
        QLabel *lbPath = new QLabel ("Folder Path", this);
        mLePath = new QLineEdit (this);
        QToolButton *tbPath = new QToolButton (this);
        QLabel *lbName = new QLabel ("Folder Name", this);
        mLeName = new QLineEdit (this);
        tbPath->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                                 "select_file_dis_16px.png"));
        tbPath->setFocusPolicy (QWidget::TabFocus);
        connect (mLePath, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));
        connect (mLeName, SIGNAL (textChanged (const QString &)),
                 this, SLOT (validate()));
        connect (tbPath, SIGNAL (clicked()), this, SLOT (showFileDialog()));
        QWhatsThis::add (mLePath, tr ("Enter existing path for the shared folder here"));
        QWhatsThis::add (mLeName, tr ("Enter name for the shared folder to be created"));
        QWhatsThis::add (tbPath, tr ("Click to invoke <open folder> dialog"));

        inputLayout->addWidget (lbPath,  0, 0);
        inputLayout->addWidget (mLePath, 0, 1);
        inputLayout->addWidget (tbPath,  0, 2);
        inputLayout->addWidget (lbName,  1, 0);
        inputLayout->addMultiCellWidget (mLeName, 1, 1, 1, 2);

        /* Setup Button layout */
        QHBoxLayout *buttonLayout = new QHBoxLayout (mainLayout, 10, "buttonLayout");
        mBtOk = new QPushButton ("OK", this, "btOk");
        QSpacerItem *spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        QPushButton *btCancel = new QPushButton ("Cancel", this, "btCancel");
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

private slots:

    void validate()
    {
        mBtOk->setEnabled (!mLePath->text().isEmpty() && !mLeName->text().isEmpty());
    }

    void showFileDialog()
    {
        QString folderName = QFileDialog::getExistingDirectory (
                                          QDir::rootDirPath(),
                                          this, "AddSharedFolderDialog",
                                          tr ("Select a folder to share"),
                                          false /* show dir only */);
        /* compose folder path */
        mLePath->setText (folderName);
        /* compose folder name */
        QRegExp regExp ("[\\/]([^\\^/]+)[\\/]?$");
        int lastFolderPos = regExp.search (folderName);
        if (lastFolderPos != -1)
            mLeName->setText (regExp.cap (1));
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
};


void VBoxSharedFoldersSettings::init()
{
    new QIListViewSelectionPreserver (this, listView);
    tbAdd->setIconSet (VBoxGlobal::iconSet ("select_file_16px.png",
                                            "select_file_dis_16px.png"));
    tbRemove->setIconSet (VBoxGlobal::iconSet ("eraser_16px.png",
                                               "eraser_disabled_16px.png"));
    connect (tbAdd, SIGNAL (clicked()), this, SLOT (tbAddPressed()));
    connect (tbRemove, SIGNAL (clicked()), this, SLOT (tbRemovePressed()));
    connect (listView, SIGNAL (currentChanged (QListViewItem *)),
             this, SLOT (processCurrentChanged (QListViewItem *)));

    isListViewChanged = false;
}


void VBoxSharedFoldersSettings::getFromGlobal()
{
    loadFrom (vboxGlobal().virtualBox().GetSharedFolders().Enumerate());
}

void VBoxSharedFoldersSettings::getFromMachine (const CMachine &aMachine)
{
    loadFrom (aMachine.GetSharedFolders().Enumerate());
}

void VBoxSharedFoldersSettings::getFromConsole (const CConsole &aConsole)
{
    loadFrom (aConsole.GetSharedFolders().Enumerate());
}

void VBoxSharedFoldersSettings::loadFrom (const CSharedFolderEnumerator &aEn)
{
    while (aEn.HasMore())
    {
        CSharedFolder sf = aEn.GetNext();
        new VBoxRichListItem (VBoxRichListItem::EllipsisFile, listView,
                              sf.GetName(), sf.GetHostPath());
    }
    listView->setCurrentItem (listView->firstChild());
    processCurrentChanged (listView->firstChild());
}


void VBoxSharedFoldersSettings::putBackToGlobal()
{
    /* This feature is not implemented yet */
    AssertMsgFailed (("Global shared folders are not implemented yet\n"));
}

void VBoxSharedFoldersSettings::putBackToMachine (CMachine &aMachine)
{
    /* first deleting all existing folders if the list is changed */
    if (isListViewChanged)
    {
        CSharedFolderEnumerator en = aMachine.GetSharedFolders().Enumerate();
        while (en.HasMore())
            aMachine.RemoveSharedFolder (en.GetNext().GetName());
    }
    else return;

    /* saving all created list view items */
    QListViewItemIterator it (listView);
    while (it.current())
    {
        VBoxRichListItem *item = 0;
        if (it.current()->rtti() == VBoxRichListItem::QIRichListItemId)
            item = static_cast<VBoxRichListItem*> (it.current());
        if (item)
            aMachine.CreateSharedFolder (item->getText (0), item->getText (1));
        else
            AssertMsgFailed (("Incorrect listview item type\n"));
        ++it;
    }
}

void VBoxSharedFoldersSettings::putBackToConsole (CConsole &/*aConsole*/)
{
    /* This feature is not implemented yet */
    AssertMsgFailed (("Transient shared folders are not implemented yet\n"));
}


void VBoxSharedFoldersSettings::tbAddPressed()
{
    VBoxAddSFDialog dlg (this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString name = dlg.getName();
    QString path = dlg.getPath();
    /* Shared folder's name & path could not be empty */
    Assert (!name.isEmpty() && !path.isEmpty());

    VBoxRichListItem *item = new VBoxRichListItem (VBoxRichListItem::EllipsisFile,
                                                   listView, name, path);
    listView->setCurrentItem (item);
    processCurrentChanged (item);
    listView->setFocus();
    isListViewChanged = true;
}

void VBoxSharedFoldersSettings::tbRemovePressed()
{
    Assert (listView->selectedItem());
    delete listView->selectedItem();
    isListViewChanged = true;
}


void VBoxSharedFoldersSettings::processCurrentChanged (QListViewItem *aItem)
{
    if (aItem && listView->selectedItem() != aItem)
        listView->setSelected (aItem, true);
    tbRemove->setEnabled (!!aItem);
}


#include "VBoxSharedFoldersSettings.ui.moc"

