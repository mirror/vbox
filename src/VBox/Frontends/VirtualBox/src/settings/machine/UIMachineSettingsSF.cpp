/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineSettingsSF class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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

/* Qt includes: */
# include <QHeaderView>
# include <QTimer>

/* GUI includes: */
# include "UIIconPool.h"
# include "UIMachineSettingsSF.h"
# include "UIMachineSettingsSFDetails.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Machine settings: Shared Folder data structure. */
struct UIDataSettingsSharedFolder
{
    /** Constructs data. */
    UIDataSettingsSharedFolder()
        : m_type(MachineType)
        , m_strName(QString())
        , m_strHostPath(QString())
        , m_fAutoMount(false)
        , m_fWritable(false)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsSharedFolder &other) const
    {
        return true
               && (m_type == other.m_type)
               && (m_strName == other.m_strName)
               && (m_strHostPath == other.m_strHostPath)
               && (m_fAutoMount == other.m_fAutoMount)
               && (m_fWritable == other.m_fWritable)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsSharedFolder &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsSharedFolder &other) const { return !equal(other); }

    /** Holds the shared folder type. */
    UISharedFolderType  m_type;
    /** Holds the shared folder name. */
    QString             m_strName;
    /** Holds the shared folder path. */
    QString             m_strHostPath;
    /** Holds whether the shared folder should be auto-mounted at startup. */
    bool                m_fAutoMount;
    /** Holds whether the shared folder should be writeable. */
    bool                m_fWritable;
};


/** Machine settings: Shared Folders page data structure. */
struct UIDataSettingsSharedFolders
{
    /** Constructs data. */
    UIDataSettingsSharedFolders() {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsSharedFolders & /* other */) const { return true; }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsSharedFolders & /* other */) const { return false; }
};


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
    : m_pActionAdd(0), m_pActionEdit(0), m_pActionRemove(0)
    , m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIMachineSettingsSF::~UIMachineSettingsSF()
{
    /* Cleanup: */
    cleanup();
}

bool UIMachineSettingsSF::changed() const
{
    return m_pCache->wasChanged();
}

void UIMachineSettingsSF::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Load machine (permanent) shared folders into shared folders cache if possible: */
    if (isSharedFolderTypeSupported(MachineType))
        loadToCacheFrom(MachineType);
    /* Load console (temporary) shared folders into shared folders cache if possible: */
    if (isSharedFolderTypeSupported(ConsoleType))
        loadToCacheFrom(ConsoleType);

    /* Upload machine to data: */
    UISettingsPageMachine::uploadData(data);
}

void UIMachineSettingsSF::loadToCacheFrom(UISharedFolderType enmSharedFoldersType)
{
    /* Get current shared folders: */
    CSharedFolderVector sharedFolders = getSharedFolders(enmSharedFoldersType);

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
            sharedFolderData.m_type = enmSharedFoldersType;
            sharedFolderData.m_strName = folder.GetName();
            sharedFolderData.m_strHostPath = folder.GetHostPath();
            sharedFolderData.m_fAutoMount = folder.GetAutoMount();
            sharedFolderData.m_fWritable = folder.GetWritable();
            /* Override shared folder cache key: */
            strSharedFolderKey = sharedFolderData.m_strName;
        }

        /* Cache shared folder data: */
        m_pCache->child(strSharedFolderKey).cacheInitialData(sharedFolderData);
    }
}

void UIMachineSettingsSF::getFromCache()
{
    /* Clear list initially: */
    mTwFolders->clear();

    /* Update root items visibility: */
    updateRootItemsVisibility();

    /* Load shared folders data: */
    for (int iFolderIndex = 0; iFolderIndex < m_pCache->childCount(); ++iFolderIndex)
    {
        /* Get shared folder data: */
        const UIDataSettingsSharedFolder &sharedFolderData = m_pCache->child(iFolderIndex).base();
        /* Prepare item fields: */
        QStringList fields;
        fields << sharedFolderData.m_strName
               << sharedFolderData.m_strHostPath
               << (sharedFolderData.m_fAutoMount ? m_strTrYes : "")
               << (sharedFolderData.m_fWritable ? m_strTrFull : m_strTrReadOnly);
        /* Create new shared folders item: */
        new SFTreeViewItem(root(sharedFolderData.m_type), fields, SFTreeViewItem::EllipsisFile);
    }
    /* Ensure current item fetched: */
    mTwFolders->setCurrentItem(mTwFolders->topLevelItem(0));
    sltHandleCurrentItemChange(mTwFolders->currentItem());

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
            sharedFolderData.m_fAutoMount = pFolderItem->getText(2) == m_strTrYes ? true : false;
            sharedFolderData.m_fWritable = pFolderItem->getText(3) == m_strTrFull ? true : false;
            m_pCache->child(sharedFolderData.m_strName).cacheCurrentData(sharedFolderData);
        }
    }
}

void UIMachineSettingsSF::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to machine: */
    UISettingsPageMachine::fetchData(data);

    /* Check if shared folders data was changed at all: */
    if (m_pCache->wasChanged())
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

void UIMachineSettingsSF::saveFromCacheTo(UISharedFolderType enmSharedFoldersType)
{
    /* For each shared folder data set: */
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < m_pCache->childCount(); ++iSharedFolderIndex)
    {
        /* Check if this shared folder data was actually changed: */
        const UISettingsCacheSharedFolder &sharedFolderCache = m_pCache->child(iSharedFolderIndex);
        if (sharedFolderCache.wasChanged())
        {
            /* If shared folder was removed: */
            if (sharedFolderCache.wasRemoved())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.base();
                /* Check if thats shared folder of required type before removing: */
                if (sharedFolderData.m_type == enmSharedFoldersType)
                    removeSharedFolder(sharedFolderCache);
            }

            /* If shared folder was created: */
            if (sharedFolderCache.wasCreated())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.data();
                /* Check if thats shared folder of required type before creating: */
                if (sharedFolderData.m_type == enmSharedFoldersType)
                    createSharedFolder(sharedFolderCache);
            }

            /* If shared folder was changed: */
            if (sharedFolderCache.wasUpdated())
            {
                /* Get shared folder data: */
                const UIDataSettingsSharedFolder &sharedFolderData = sharedFolderCache.data();
                /* Check if thats shared folder of required type before recreating: */
                if (sharedFolderData.m_type == enmSharedFoldersType)
                {
                    removeSharedFolder(sharedFolderCache);
                    createSharedFolder(sharedFolderCache);
                }
            }
        }
    }
}

void UIMachineSettingsSF::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIMachineSettingsSF::retranslateUi(this);

    m_pActionAdd->setText(tr("Add Shared Folder"));
    m_pActionEdit->setText(tr("Edit Shared Folder"));
    m_pActionRemove->setText(tr("Remove Shared Folder"));

    m_pActionAdd->setWhatsThis(tr("Adds new shared folder."));
    m_pActionEdit->setWhatsThis(tr("Edits selected shared folder."));
    m_pActionRemove->setWhatsThis(tr("Removes selected shared folder."));

    m_pActionAdd->setToolTip(m_pActionAdd->whatsThis());
    m_pActionEdit->setToolTip(m_pActionEdit->whatsThis());
    m_pActionRemove->setToolTip(m_pActionRemove->whatsThis());

    m_strTrFull = tr("Full");
    m_strTrReadOnly = tr("Read-only");
    m_strTrYes = tr("Yes");
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

void UIMachineSettingsSF::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    UISettingsPageMachine::showEvent(pEvent);

    /* Connect header-resize signal just before widget is shown after all the items properly loaded and initialized: */
    connect(mTwFolders->header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(sltAdjustTreeFields()));

    /* Adjusting size after all pending show events are processed: */
    QTimer::singleShot(0, this, SLOT(sltAdjustTree()));
}

void UIMachineSettingsSF::resizeEvent(QResizeEvent * /* pEvent */)
{
    sltAdjustTree();
}

void UIMachineSettingsSF::sltAddSharedFolder()
{
    /* Invoke Add-Box Dialog: */
    UIMachineSettingsSFDetails dlg(UIMachineSettingsSFDetails::AddType, isSharedFolderTypeSupported(ConsoleType), usedList(true), this);
    if (dlg.exec() == QDialog::Accepted)
    {
        const QString strName = dlg.name();
        const QString strPath = dlg.path();
        const bool fPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty: */
        Assert(!strName.isEmpty() && !strPath.isEmpty());
        /* Appending a new listview item to the root: */
        QStringList fields;
        fields << strName /* name */ << strPath /* path */
               << (dlg.isAutoMounted() ? m_strTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? m_strTrFull : m_strTrReadOnly /* writable? */);
        SFTreeViewItem *pItem = new SFTreeViewItem(root(fPermanent ? MachineType : ConsoleType),
                                                   fields, SFTreeViewItem::EllipsisFile);
        mTwFolders->sortItems(0, Qt::AscendingOrder);
        mTwFolders->scrollToItem(pItem);
        mTwFolders->setCurrentItem(pItem);
        sltHandleCurrentItemChange(pItem);
        mTwFolders->setFocus();
        sltAdjustTree();
    }
}

void UIMachineSettingsSF::sltEditSharedFolder()
{
    /* Check selected item: */
    QTreeWidgetItem *pSelectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems()[0] : 0;
    SFTreeViewItem *pItem = pSelectedItem ? static_cast<SFTreeViewItem*>(pSelectedItem) : 0;
    Assert(pItem);
    Assert(pItem->parentItem());

    /* Invoke Edit-Box Dialog: */
    UIMachineSettingsSFDetails dlg(UIMachineSettingsSFDetails::EditType, isSharedFolderTypeSupported(ConsoleType), usedList(false), this);
    dlg.setPath(pItem->getText(1));
    dlg.setName(pItem->getText(0));
    dlg.setPermanent((UISharedFolderType)pItem->parentItem()->text(1).toInt() != ConsoleType);
    dlg.setAutoMount(pItem->getText(2) == m_strTrYes);
    dlg.setWriteable(pItem->getText(3) == m_strTrFull);
    if (dlg.exec() == QDialog::Accepted)
    {
        const QString strName = dlg.name();
        const QString strPath = dlg.path();
        const bool fPermanent = dlg.isPermanent();
        /* Shared folder's name & path could not be empty: */
        Assert(!strName.isEmpty() && !strPath.isEmpty());
        /* Searching new root for the selected listview item: */
        SFTreeViewItem *pRoot = root(fPermanent ? MachineType : ConsoleType);
        /* Updating an edited listview item: */
        QStringList fields;
        fields << strName /* name */ << strPath /* path */
               << (dlg.isAutoMounted() ? m_strTrYes : "" /* auto mount? */)
               << (dlg.isWriteable() ? m_strTrFull : m_strTrReadOnly /* writable? */);
        pItem->updateText(fields);
        mTwFolders->sortItems(0, Qt::AscendingOrder);
        if (pItem->parentItem() != pRoot)
        {
            /* Move the selected item into new location: */
            pItem->parentItem()->takeChild(pItem->parentItem()->indexOfChild(pItem));
            pRoot->insertChild(pRoot->childCount(), pItem);
            mTwFolders->scrollToItem(pItem);
            mTwFolders->setCurrentItem(pItem);
            sltHandleCurrentItemChange(pItem);
            mTwFolders->setFocus();
        }
        sltAdjustTree();
    }
}

void UIMachineSettingsSF::sltRemoveSharedFolder()
{
    QTreeWidgetItem *pSelectedItem = mTwFolders->selectedItems().size() == 1 ? mTwFolders->selectedItems()[0] : 0;
    Assert(pSelectedItem);
    delete pSelectedItem;
    sltAdjustTree();
}

void UIMachineSettingsSF::sltHandleCurrentItemChange(QTreeWidgetItem *pCurrentItem)
{
    if (pCurrentItem && pCurrentItem->parent() && !pCurrentItem->isSelected())
        pCurrentItem->setSelected(true);
    const bool fAddEnabled = pCurrentItem;
    const bool fRemoveEnabled = fAddEnabled && pCurrentItem->parent();
    m_pActionAdd->setEnabled(fAddEnabled);
    m_pActionEdit->setEnabled(fRemoveEnabled);
    m_pActionRemove->setEnabled(fRemoveEnabled);
}

void UIMachineSettingsSF::sltHandleDoubleClick(QTreeWidgetItem *pItem)
{
    const bool fEditEnabled = pItem && pItem->parent();
    if (fEditEnabled)
        sltEditSharedFolder();
}

void UIMachineSettingsSF::sltHandleContextMenuRequest(const QPoint &position)
{
    QMenu menu;
    QTreeWidgetItem *pItem = mTwFolders->itemAt(position);
    if (mTwFolders->isEnabled() && pItem && pItem->flags() & Qt::ItemIsSelectable)
    {
        menu.addAction(m_pActionEdit);
        menu.addAction(m_pActionRemove);
    }
    else
    {
        menu.addAction(m_pActionAdd);
    }
    if (!menu.isEmpty())
        menu.exec(mTwFolders->viewport()->mapToGlobal(position));
}

void UIMachineSettingsSF::sltAdjustTree()
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
    QAbstractItemView *pItemView = mTwFolders;
    QHeaderView *pItemHeader = mTwFolders->header();
    const int iTotal = mTwFolders->viewport()->width();

    const int mw0 = qMax(pItemView->sizeHintForColumn(0), pItemHeader->sectionSizeHint(0));
    const int mw2 = qMax(pItemView->sizeHintForColumn(2), pItemHeader->sectionSizeHint(2));
    const int mw3 = qMax(pItemView->sizeHintForColumn(3), pItemHeader->sectionSizeHint(3));

    const int w0 = mw0 < iTotal / 4 ? mw0 : iTotal / 4;
    const int w2 = mw2 < iTotal / 4 ? mw2 : iTotal / 4;
    const int w3 = mw3 < iTotal / 4 ? mw3 : iTotal / 4;

    /* Giving 1st column all the available space. */
    mTwFolders->setColumnWidth(0, w0);
    mTwFolders->setColumnWidth(1, iTotal - w0 - w2 - w3);
    mTwFolders->setColumnWidth(2, w2);
    mTwFolders->setColumnWidth(3, w3);
}

void UIMachineSettingsSF::sltAdjustTreeFields()
{
    QTreeWidgetItem *pMainRoot = mTwFolders->invisibleRootItem();
    for (int i = 0; i < pMainRoot->childCount(); ++i)
    {
        QTreeWidgetItem *pSubRoot = pMainRoot->child(i);
        for (int j = 0; j < pSubRoot->childCount(); ++j)
        {
            SFTreeViewItem *pItem = pSubRoot->child(j) ? static_cast <SFTreeViewItem*>(pSubRoot->child(j)) : 0;
            if (pItem)
                pItem->adjustText();
        }
    }
}

void UIMachineSettingsSF::prepare()
{
    /* Apply UI decorations: */
    Ui::UIMachineSettingsSF::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheSharedFolders;
    AssertPtrReturnVoid(m_pCache);

    /* Layout created in the .ui file. */
    {
        /* Prepare shared folders tree: */
        prepareFoldersTree();
        /* Prepare shared folders toolbar: */
        prepareFoldersToolbar();
        /* Prepare connections: */
        prepareConnections();
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIMachineSettingsSF::prepareFoldersTree()
{
    /* Shared Folders tree-widget created in the .ui file. */
    AssertPtrReturnVoid(mTwFolders);
    {
        /* Configure tree-widget: */
        mTwFolders->header()->setSectionsMovable(false);
    }
}

void UIMachineSettingsSF::prepareFoldersToolbar()
{
    /* Shared Folders toolbar created in the .ui file. */
    AssertPtrReturnVoid(m_pFoldersToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pFoldersToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pFoldersToolBar->setOrientation(Qt::Vertical);

        /* Create 'Add Shared Folder' action: */
        m_pActionAdd = m_pFoldersToolBar->addAction(UIIconPool::iconSet(":/sf_add_16px.png",
                                                                        ":/sf_add_disabled_16px.png"),
                                                    QString(), this, SLOT(sltAddSharedFolder()));
        AssertPtrReturnVoid(m_pActionAdd);
        {
            /* Configure action: */
            m_pActionAdd->setShortcuts(QList<QKeySequence>() << QKeySequence("Ins") << QKeySequence("Ctrl+N"));
        }

        /* Create 'Edit Shared Folder' action: */
        m_pActionEdit = m_pFoldersToolBar->addAction(UIIconPool::iconSet(":/sf_edit_16px.png",
                                                                         ":/sf_edit_disabled_16px.png"),
                                                     QString(), this, SLOT(sltEditSharedFolder()));
        AssertPtrReturnVoid(m_pActionEdit);
        {
            /* Configure action: */
            m_pActionEdit->setShortcuts(QList<QKeySequence>() << QKeySequence("Space") << QKeySequence("F2"));
        }

        /* Create 'Remove Shared Folder' action: */
        m_pActionRemove = m_pFoldersToolBar->addAction(UIIconPool::iconSet(":/sf_remove_16px.png",
                                                                           ":/sf_remove_disabled_16px.png"),
                                                       QString(), this, SLOT(sltRemoveSharedFolder()));
        AssertPtrReturnVoid(m_pActionRemove);
        {
            /* Configure action: */
            m_pActionRemove->setShortcuts(QList<QKeySequence>() << QKeySequence("Del") << QKeySequence("Ctrl+R"));
        }
    }
}

void UIMachineSettingsSF::prepareConnections()
{
    /* Configure tree-widget connections: */
    connect(mTwFolders, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
            this, SLOT(sltHandleCurrentItemChange(QTreeWidgetItem *)));
    connect(mTwFolders, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
            this, SLOT(sltHandleDoubleClick(QTreeWidgetItem *)));
    connect(mTwFolders, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(sltHandleContextMenuRequest(const QPoint &)));
}

void UIMachineSettingsSF::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

SFTreeViewItem *UIMachineSettingsSF::root(UISharedFolderType enmSharedFolderType)
{
    /* Search for the corresponding root item among all the top-level items: */
    SFTreeViewItem *pRootItem = 0;
    QTreeWidgetItem *pMainRootItem = mTwFolders->invisibleRootItem();
    for (int iFolderTypeIndex = 0; iFolderTypeIndex < pMainRootItem->childCount(); ++iFolderTypeIndex)
    {
        /* Get iterated item: */
        QTreeWidgetItem *pIteratedItem = pMainRootItem->child(iFolderTypeIndex);
        /* If iterated item type is what we are looking for: */
        if (pIteratedItem->text(1).toInt() == enmSharedFolderType)
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

QStringList UIMachineSettingsSF::usedList(bool fIncludeSelected)
{
    /* Make the used names list: */
    QStringList list;
    QTreeWidgetItemIterator it(mTwFolders);
    while (*it)
    {
        if ((*it)->parent() && (fIncludeSelected || !(*it)->isSelected()))
            list << static_cast<SFTreeViewItem*>(*it)->getText(0);
        ++it;
    }
    return list;
}

bool UIMachineSettingsSF::isSharedFolderTypeSupported(UISharedFolderType enmSharedFolderType) const
{
    bool fIsSharedFolderTypeSupported = false;
    switch (enmSharedFolderType)
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

void UIMachineSettingsSF::setRootItemVisible(UISharedFolderType enmSharedFolderType, bool fVisible)
{
    /* Search for the corresponding root item among all the top-level items: */
    SFTreeViewItem *pRootItem = root(enmSharedFolderType);
    /* If root item, we are looking for, still not found: */
    if (!pRootItem)
    {
        /* Prepare fields for the new root item: */
        QStringList fields;
        /* Depending on folder type: */
        switch (enmSharedFolderType)
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

CSharedFolderVector UIMachineSettingsSF::getSharedFolders(UISharedFolderType enmSharedFoldersType)
{
    CSharedFolderVector sharedFolders;
    if (isSharedFolderTypeSupported(enmSharedFoldersType))
    {
        switch (enmSharedFoldersType)
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

bool UIMachineSettingsSF::createSharedFolder(const UISettingsCacheSharedFolder &folderCache)
{
    /* Get shared folder data: */
    const UIDataSettingsSharedFolder &folderData = folderCache.data();
    const QString strName = folderData.m_strName;
    const QString strPath = folderData.m_strHostPath;
    const bool fIsWritable = folderData.m_fWritable;
    const bool fIsAutoMount = folderData.m_fAutoMount;
    const UISharedFolderType enmSharedFoldersType = folderData.m_type;

    /* Get current shared folders: */
    const CSharedFolderVector sharedFolders = getSharedFolders(enmSharedFoldersType);
    /* Check if such shared folder do not exists: */
    CSharedFolder sharedFolder;
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < sharedFolders.size(); ++iSharedFolderIndex)
        if (sharedFolders[iSharedFolderIndex].GetName() == strName)
            sharedFolder = sharedFolders[iSharedFolderIndex];
    if (sharedFolder.isNull())
    {
        /* Create new shared folder: */
        switch(enmSharedFoldersType)
        {
            case MachineType:
            {
                /* Create new shared folder: */
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

bool UIMachineSettingsSF::removeSharedFolder(const UISettingsCacheSharedFolder &folderCache)
{
    /* Get shared folder data: */
    const UIDataSettingsSharedFolder &folderData = folderCache.base();
    const QString strName = folderData.m_strName;
    const QString strPath = folderData.m_strHostPath;
    const UISharedFolderType enmSharedFoldersType = folderData.m_type;

    /* Get current shared folders: */
    const CSharedFolderVector sharedFolders = getSharedFolders(enmSharedFoldersType);
    /* Check that such shared folder really exists: */
    CSharedFolder sharedFolder;
    for (int iSharedFolderIndex = 0; iSharedFolderIndex < sharedFolders.size(); ++iSharedFolderIndex)
        if (sharedFolders[iSharedFolderIndex].GetName() == strName)
            sharedFolder = sharedFolders[iSharedFolderIndex];
    if (!sharedFolder.isNull())
    {
        /* Remove existing shared folder: */
        switch(enmSharedFoldersType)
        {
            case MachineType:
            {
                /* Remove existing shared folder: */
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
                /* Remove existing shared folder: */
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

