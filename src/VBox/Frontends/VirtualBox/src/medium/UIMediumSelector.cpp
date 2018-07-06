/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSelector class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
# include <QAction>
# include <QHeaderView>
# include <QVBoxLayout>
# include <QPushButton>

/* GUI includes: */
# include "QIComboBox.h"
# include "QIDialogButtonBox.h"
# include "QIFileDialog.h"
# include "QILineEdit.h"
# include "QIMessageBox.h"
# include "QITabWidget.h"
# include "QIToolButton.h"
# include "VBoxGlobal.h"
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UIFDCreationDialog.h"
# include "UIMediumSelector.h"
# include "UIMessageCenter.h"
# include "UIIconPool.h"
# include "UIMedium.h"
# include "UIMediumItem.h"
# include "UIToolBar.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMachine.h"
# include "CMediumAttachment.h"
# include "CMediumFormat.h"
# include "CStorageController.h"
# include "CSystemProperties.h"

# ifdef VBOX_WS_MAC
#  include "UIWindowMenuManager.h"
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


class UIMediumSearchWidget : public QWidget
{
    Q_OBJECT;

public:

    enum SearchType
    {
        SearchByName,
        SearchByUUID,
        SearchByMax
    };

signals:

    void sigSearchTypeChanged(int newType);
    void sigSearchTermChanged(QString searchTerm);

public:

    UIMediumSearchWidget(QWidget *pParent = 0);
    SearchType searchType() const;
    QString searchTerm() const;

private:

    void prepareWidgets();
    QIComboBox       *m_pSearchComboxBox;
    QLineEdit         *m_pSearchTermLineEdit;
};


/*********************************************************************************************************************************
*   UIMediumSearchWidget implementation.                                                                                         *
*********************************************************************************************************************************/

UIMediumSearchWidget::UIMediumSearchWidget(QWidget *pParent)
    :QWidget(pParent)
    , m_pSearchComboxBox(0)
    , m_pSearchTermLineEdit(0)
{
    prepareWidgets();
}

void UIMediumSearchWidget::prepareWidgets()
{
    QHBoxLayout *pLayout = new QHBoxLayout;
    setLayout(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    m_pSearchComboxBox = new QIComboBox;
    if (m_pSearchComboxBox)
    {
        m_pSearchComboxBox->setEditable(false);
        m_pSearchComboxBox->insertItem(SearchByName, "Search By Name");
        m_pSearchComboxBox->insertItem(SearchByUUID, "Search By UUID");
        pLayout->addWidget(m_pSearchComboxBox);

        connect(m_pSearchComboxBox, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged),
                this, &UIMediumSearchWidget::sigSearchTypeChanged);

    }

    m_pSearchTermLineEdit = new QLineEdit;
    if (m_pSearchTermLineEdit)
    {
        m_pSearchTermLineEdit->setClearButtonEnabled(true);
        pLayout->addWidget(m_pSearchTermLineEdit);
        connect(m_pSearchTermLineEdit, &QILineEdit::textChanged,
                this, &UIMediumSearchWidget::sigSearchTermChanged);
    }
}

UIMediumSearchWidget::SearchType UIMediumSearchWidget::searchType() const
{
    if (!m_pSearchComboxBox || m_pSearchComboxBox->currentIndex() >= static_cast<int>(SearchByMax))
        return SearchByMax;
    return static_cast<SearchType>(m_pSearchComboxBox->currentIndex());
}

QString UIMediumSearchWidget::searchTerm() const
{
    if (!m_pSearchTermLineEdit)
        return QString();
    return m_pSearchTermLineEdit->text();
}


UIMediumSelector::UIMediumSelector(UIMediumType enmMediumType, const QString &machineName /* = QString() */,
                                   const QString &machineSettigFilePath /* = QString() */, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QIDialog>(pParent)
    , m_pMainLayout(0)
    , m_pTreeWidget(0)
    , m_enmMediumType(enmMediumType)
    , m_pButtonBox(0)
    , m_pToolBar(0)
    , m_pActionAdd(0)
    , m_pActionCreate(0)
    , m_pActionRefresh(0)
    , m_pAttachedSubTreeRoot(0)
    , m_pNotAttachedSubTreeRoot(0)
    , m_pParent(pParent)
    , m_pSearchWidget(0)
    , m_strMachineSettingsFilePath(machineSettigFilePath)
    , m_strMachineName(machineName)
{
    configure();
    finalize();
}

QStringList UIMediumSelector::selectedMediumIds() const
{
    QStringList selectedIds;
    if (!m_pTreeWidget)
        return selectedIds;
    QList<QTreeWidgetItem*> selectedItems = m_pTreeWidget->selectedItems();
    for (int i = 0; i < selectedItems.size(); ++i)
    {
        UIMediumItem *item = dynamic_cast<UIMediumItem*>(selectedItems.at(i));
        if (item)
            selectedIds.push_back(item->medium().id());
    }
    return selectedIds;
}


void UIMediumSelector::retranslateUi()
{
    if (m_pActionAdd)
    {
        m_pActionAdd->setText(QApplication::translate("UIMediumManager", "&Add"));
        m_pActionAdd->setToolTip(QApplication::translate("UIMediumManager", "Add Disk Image File"));
        m_pActionAdd->setStatusTip(QApplication::translate("UIMediumManager", "Add disk image file"));
    }

    if (m_pActionCreate)
    {
        m_pActionCreate->setText(QApplication::translate("UIMediumManager", "&Create"));
        m_pActionCreate->setToolTip(QApplication::translate("UIMediumManager", "Create an Empty Disk Image"));
        m_pActionCreate->setStatusTip(QApplication::translate("UIMediumManager", "Create an Empty Disk Image"));
    }

    if (m_pActionRefresh)
    {
        m_pActionRefresh->setText(QApplication::translate("UIMediumManager","Re&fresh"));
        m_pActionRefresh->setToolTip(QApplication::translate("UIMediumManager","Refresh Disk Image Files (%1)").arg(m_pActionRefresh->shortcut().toString()));
        m_pActionRefresh->setStatusTip(QApplication::translate("UIMediumManager","Refresh the list of disk image files"));
    }

    if (m_pButtonBox)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText("Choose");

    if (m_pTreeWidget)
    {
        m_pTreeWidget->headerItem()->setText(0, QApplication::translate("UIMediumManager","Name"));
        m_pTreeWidget->headerItem()->setText(1, QApplication::translate("UIMediumManager","Virtual Size"));
        m_pTreeWidget->headerItem()->setText(2, QApplication::translate("UIMediumManager","Actual Size"));
    }
}

void UIMediumSelector::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/media_manager_32px.png", ":/media_manager_16px.png"));
    prepareActions();
    prepareWidgets();
    prepareConnections();

}

void UIMediumSelector::prepareActions()
{
    QString strPrefix("hd");
    switch (m_enmMediumType)
    {
        case UIMediumType_DVD:
            strPrefix = "cd";
            break;
        case UIMediumType_Floppy:
            strPrefix = "fd";
            break;
        case UIMediumType_HardDisk:
        case UIMediumType_All:
        case UIMediumType_Invalid:
        default:
            strPrefix = "hd";
            break;
    }

    m_pActionAdd = new QAction(this);
    if (m_pActionAdd)
    {
        /* Configure add-action: */
        m_pActionAdd->setShortcut(QKeySequence("Ctrl+A"));

        m_pActionAdd->setIcon(UIIconPool::iconSetFull(QString(":/%1_add_22px.png").arg(strPrefix),
                                                      QString(":/%1_add_16px.png").arg(strPrefix),
                                                      QString(":/%1_add_disabled_22px.png").arg(strPrefix),
                                                      QString(":/%1_add_disabled_16px.png").arg(strPrefix)));
    }

    /* Currently create is supported only for Floppy: */
    if (m_enmMediumType == UIMediumType_Floppy)
    {
        m_pActionCreate = new QAction(this);
    }
    if (m_pActionCreate)
    {

        m_pActionCreate->setShortcut(QKeySequence("Ctrl+C"));
        m_pActionCreate->setIcon(UIIconPool::iconSetFull(QString(":/%1_add_22px.png").arg(strPrefix),
                                                         QString(":/%1_add_16px.png").arg(strPrefix),
                                                         QString(":/%1_add_disabled_22px.png").arg(strPrefix),
                                                         QString(":/%1_add_disabled_16px.png").arg(strPrefix)));
    }


    m_pActionRefresh = new QAction(this);
    if (m_pActionRefresh)
    {
        m_pActionRefresh->setShortcut(QKeySequence(QKeySequence::Refresh));
        if (m_pActionRefresh && m_pActionRefresh->icon().isNull())
            m_pActionRefresh->setIcon(UIIconPool::iconSetFull(":/refresh_22px.png", ":/refresh_16px.png",
                                                              ":/refresh_disabled_22px.png", ":/refresh_disabled_16px.png"));
    }
}

void UIMediumSelector::prepareConnections()
{
    /* Configure medium-enumeration connections: */
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumEnumerationStarted,
            this, &UIMediumSelector::sltHandleMediumEnumerationStart);
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumEnumerated,
            this, &UIMediumSelector::sltHandleMediumEnumerated);
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumEnumerationFinished,
            this, &UIMediumSelector::sltHandleMediumEnumerationFinish);
    if (m_pActionAdd)
        connect(m_pActionAdd, &QAction::triggered, this, &UIMediumSelector::sltAddMedium);
    if (m_pActionCreate)
        connect(m_pActionCreate, &QAction::triggered, this, &UIMediumSelector::sltCreateMedium);
    if (m_pActionRefresh)
        connect(m_pActionRefresh, &QAction::triggered, this, &UIMediumSelector::sltHandleRefresh);

    if (m_pTreeWidget)
        connect(m_pTreeWidget, &QITreeWidget::itemSelectionChanged, this, &UIMediumSelector::sltHandleItemSelectionChanged);

    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIMediumSelector::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIMediumSelector::accept);
    }

    if (m_pSearchWidget)
    {
        connect(m_pSearchWidget, &UIMediumSearchWidget::sigSearchTypeChanged,
                this, &UIMediumSelector::sltHandleSearchTypeChange);
        connect(m_pSearchWidget, &UIMediumSearchWidget::sigSearchTermChanged,
                this, &UIMediumSelector::sltHandleSearchTermChange);
    }
}

UIMediumItem* UIMediumSelector::addTreeItem(const UIMedium &medium, QITreeWidgetItem *pParent)
{
    if (!pParent)
        return 0;
    switch (m_enmMediumType)
    {
        case UIMediumType_DVD:
            return new UIMediumItemCD(medium, pParent);
            break;
        case UIMediumType_Floppy:
            return new UIMediumItemFD(medium, pParent);
            break;
        case UIMediumType_HardDisk:
        case UIMediumType_All:
        case UIMediumType_Invalid:
        default:
            return createHardDiskItem(medium, pParent);
            break;
    }
}

UIMediumItem* UIMediumSelector::createHardDiskItem(const UIMedium &medium, QITreeWidgetItem *pParent)
{
    if (medium.medium().isNull())
        return 0;
    if (!m_pTreeWidget)
        return 0;
    /* Search the tree to see if we already have the item: */
    UIMediumItem *pMediumItem = searchItem(0, medium.id());
    if (pMediumItem)
        return pMediumItem;
    /* Check if the corresponding medium has a parent */
    if (medium.parentID() != UIMedium::nullID())
    {
        UIMediumItem *pParentMediumItem = searchItem(0, medium.parentID());
        /* If parent medium-item was not found we create it: */
        if (!pParentMediumItem)
        {
            /* Make sure corresponding parent medium is already cached! */
            UIMedium parentMedium = vboxGlobal().medium(medium.parentID());
            if (parentMedium.isNull())
                AssertMsgFailed(("Parent medium with ID={%s} was not found!\n", medium.parentID().toUtf8().constData()));
            /* Try to create parent medium-item: */
            else
                pParentMediumItem = createHardDiskItem(parentMedium, pParent);
            /* If parent medium-item was found: */
            if (pParentMediumItem)
            {
                pMediumItem = new UIMediumItemHD(medium, pParentMediumItem);
                LogRel2(("UIMediumManager: Child hard-disk medium-item with ID={%s} created.\n", medium.id().toUtf8().constData()));
            }
            else
                AssertMsgFailed(("Parent medium with ID={%s} could not be created!\n", medium.parentID().toUtf8().constData()));

        }
    }
    /* Else just create item as top-level one: */
    else
    {
        pMediumItem = new UIMediumItemHD(medium, pParent);
        LogRel2(("UIMediumManager: Root hard-disk medium-item with ID={%s} created.\n", medium.id().toUtf8().constData()));
    }
    return pMediumItem;
}

void UIMediumSelector::restoreSelection(const QStringList &selectedMediums, QVector<UIMediumItem*> &mediumList)
{
    if (!m_pTreeWidget)
        return;
    if (selectedMediums.isEmpty())
    {
        m_pTreeWidget->setCurrentItem(0);
        return;
    }
    bool selected = false;
    for (int i = 0; i < mediumList.size(); ++i)
    {
        if (!mediumList[i])
            continue;
        if (selectedMediums.contains(mediumList[i]->medium().id()))
        {
            mediumList[i]->setSelected(true);
            selected = true;
        }
    }

    if (!selected)
        m_pTreeWidget->setCurrentItem(0);
    return;
}

void UIMediumSelector::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout;
    if (!m_pMainLayout)
        return;

    setLayout(m_pMainLayout);

    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.375);
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        /* Add toolbar actions: */
        if (m_pActionAdd)
            m_pToolBar->addAction(m_pActionAdd);
        if (m_pActionCreate)
            m_pToolBar->addAction(m_pActionCreate);
        if (m_pActionRefresh)
            m_pToolBar->addAction(m_pActionRefresh);

        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        m_pTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pMainLayout->addWidget(m_pTreeWidget);
        m_pTreeWidget->setAlternatingRowColors(true);
        int iColumnCount = (m_enmMediumType == UIMediumType_HardDisk) ? 3 : 2;
        m_pTreeWidget->setColumnCount(iColumnCount);
        m_pTreeWidget->setSortingEnabled(true);
        m_pTreeWidget->sortItems(0, Qt::AscendingOrder);
    }

    m_pSearchWidget = new UIMediumSearchWidget;
    if (m_pSearchWidget)
    {
        m_pMainLayout->addWidget(m_pSearchWidget);
    }

    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        /* Configure button-box: */
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);

        /* Add button-box into main layout: */
        m_pMainLayout->addWidget(m_pButtonBox);
    }

    repopulateTreeWidget();
}

void UIMediumSelector::sltAddMedium()
{
    QString strDefaultMachineFolder = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    vboxGlobal().openMediumWithFileOpenDialog(m_enmMediumType, this, strDefaultMachineFolder);
}

void UIMediumSelector::sltCreateMedium()
{
    QString strMachineFolder = QFileInfo(m_strMachineSettingsFilePath).absolutePath();
    UIFDCreationDialog *pDialog = new UIFDCreationDialog(this, m_strMachineName, strMachineFolder);
    if (pDialog->exec())
    {
        repopulateTreeWidget();
        UIMediumItem *pMediumItem = searchItem(0, pDialog->mediumID());
        if (pMediumItem)
        {
            m_pTreeWidget->setCurrentItem(pMediumItem);

        }
    }
    delete pDialog;
}

void UIMediumSelector::sltHandleItemSelectionChanged()
{
    updateOkButton();
}

void UIMediumSelector::sltHandleMediumEnumerationStart()
{
    /* Disable controls. Left Alone button box 'Ok' button. it is handle by tree population: */
    if (m_pActionRefresh)
        m_pActionRefresh->setEnabled(false);
}

void UIMediumSelector::sltHandleMediumEnumerated()
{
}

void UIMediumSelector::sltHandleMediumEnumerationFinish()
{
    repopulateTreeWidget();
    if (m_pActionRefresh)
        m_pActionRefresh->setEnabled(true);
}

void UIMediumSelector::sltHandleRefresh()
{
    /* Initialize media enumation: */
    vboxGlobal().startMediumEnumeration();
}

void UIMediumSelector::sltHandleSearchTypeChange(int type)
{
    Q_UNUSED(type);
    performMediumSearch();
}

void UIMediumSelector::sltHandleSearchTermChange(QString searchTerm)
{
    Q_UNUSED(searchTerm);
    performMediumSearch();
}

void UIMediumSelector::updateOkButton()
{

    if (!m_pTreeWidget || !m_pButtonBox || !m_pButtonBox->button(QDialogButtonBox::Ok))
        return;
    QList<QTreeWidgetItem*> selectedItems = m_pTreeWidget->selectedItems();
    if (selectedItems.isEmpty())
    {
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }

    /* check if at least one of the selected items is a UIMediumItem */
    bool mediumItemSelected = false;
    for (int i = 0; i < selectedItems.size() && !mediumItemSelected; ++i)
    {
        if (dynamic_cast<UIMediumItem*>(selectedItems.at(i)))
            mediumItemSelected = true;
    }
    if (mediumItemSelected)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    else
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void UIMediumSelector::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UIMediumSelector::showEvent(QShowEvent *pEvent)
{
    Q_UNUSED(pEvent);

    /* Try to determine the initial size: */
    QSize proposedSize;
    int iHostScreen = 0;
    if (m_pParent)
        iHostScreen = gpDesktop->screenNumber(m_pParent);
    else
        iHostScreen = gpDesktop->screenNumber(this);
    if (iHostScreen >= 0 && iHostScreen < gpDesktop->screenCount())
    {
        /* On the basis of current host-screen geometry if possible: */
        const QRect screenGeometry = gpDesktop->screenGeometry(iHostScreen);
        if (screenGeometry.isValid())
            proposedSize = screenGeometry.size() * 5 / 15;
    }
    /* Fallback to default size if we failed: */
    if (proposedSize.isNull())
        proposedSize = QSize(800, 600);
    /* Resize to initial size: */
    resize(proposedSize);

    if (m_pParent)
        VBoxGlobal::centerWidget(this, m_pParent, false);

}

void UIMediumSelector::repopulateTreeWidget()
{
    if (!m_pTreeWidget)
        return;
    /* Cache the currently selected items: */
    QList<QTreeWidgetItem*> selectedItems = m_pTreeWidget->selectedItems();
    QStringList selectedMediums = selectedMediumIds();
    /* uuid list of selected items: */
    /* Reset the related data structure: */
    m_mediumItemList.clear();
    m_pTreeWidget->clear();
    m_pAttachedSubTreeRoot = 0;
    m_pNotAttachedSubTreeRoot = 0;
    QVector<UIMediumItem*> menuItemVector;

    foreach (const QString &strMediumID, vboxGlobal().mediumIDs())
    {
        UIMedium medium = vboxGlobal().medium(strMediumID);

        if (medium.type() == m_enmMediumType)
        {
            bool isMediumAttached = !(medium.medium().GetMachineIds().isEmpty());
            QITreeWidgetItem *pParent = 0;
            if (isMediumAttached)
            {
                if (!m_pAttachedSubTreeRoot)
                {
                    QStringList strList;
                    strList << "Attached";
                    m_pAttachedSubTreeRoot = new QITreeWidgetItem(m_pTreeWidget, strList);
                }
                pParent = m_pAttachedSubTreeRoot;

            }
            else
            {
                if (!m_pNotAttachedSubTreeRoot)
                {
                    QStringList strList;
                    strList << "Not Attached";
                    m_pNotAttachedSubTreeRoot = new QITreeWidgetItem(m_pTreeWidget, strList);
                }
                pParent = m_pNotAttachedSubTreeRoot;
            }
            UIMediumItem *treeItem = addTreeItem(medium, pParent);
            m_mediumItemList.append(treeItem);
            menuItemVector.push_back(treeItem);
        }
    }
    restoreSelection(selectedMediums, menuItemVector);
    saveDefaultForeground();
    updateOkButton();
    if (m_pAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pAttachedSubTreeRoot);

    if (m_pNotAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pNotAttachedSubTreeRoot);

    m_pTreeWidget->resizeColumnToContents(0);
    performMediumSearch();
}

void UIMediumSelector::saveDefaultForeground()
{
    if (!m_pTreeWidget)
        return;
    if (m_defaultItemForeground == QBrush() && m_pTreeWidget->topLevelItemCount() >= 1)
    {
        QTreeWidgetItem *item = m_pTreeWidget->topLevelItem(0);
        if (item)
        {
            QVariant data = item->data(0, Qt::ForegroundRole);
            if (data.canConvert<QBrush>())
            {
                m_defaultItemForeground = data.value<QBrush>();
            }
        }
    }
}

UIMediumItem* UIMediumSelector::searchItem(const QTreeWidgetItem *pParent, const QString &mediumId)
{
    if (!m_pTreeWidget)
        return 0;
    if (!pParent)
    {
        pParent = m_pTreeWidget->invisibleRootItem();
    }
    if (!pParent)
        return 0;

    for (int i = 0; i < pParent->childCount(); ++i)
    {
        QTreeWidgetItem *pChild = pParent->child(i);
        if (!pChild)
            continue;
        UIMediumItem *mediumItem = dynamic_cast<UIMediumItem*>(pChild);
        if (mediumItem)
        {
            if (mediumItem->id() == mediumId)
            {
                return mediumItem;
            }
        }
        UIMediumItem *result = searchItem(pChild, mediumId);
        if (result)
            return result;
    }
    return 0;
}

void UIMediumSelector::performMediumSearch()
{
    if (!m_pSearchWidget || !m_pTreeWidget)
        return;
    /* Unmark all tree items to remove the highltights: */
    for (int i = 0; i < m_mediumItemList.size(); ++i)
    {
        for (int j = 0; j < m_pTreeWidget->columnCount(); ++j)
        {
            if (m_mediumItemList[i])
                m_mediumItemList[i]->setData(j, Qt::ForegroundRole, m_defaultItemForeground);
        }
    }


    UIMediumSearchWidget::SearchType searchType =
        m_pSearchWidget->searchType();
    if (searchType >= UIMediumSearchWidget::SearchByMax)
        return;
    QString strTerm = m_pSearchWidget->searchTerm();
    if (strTerm.isEmpty())
        return;

    for (int i = 0; i < m_mediumItemList.size(); ++i)
    {
        if (!m_mediumItemList[i])
            continue;
        QString strMedium;
        if (searchType == UIMediumSearchWidget::SearchByName)
            strMedium = m_mediumItemList[i]->medium().name();
        else if(searchType == UIMediumSearchWidget::SearchByUUID)
            strMedium = m_mediumItemList[i]->medium().id();
        if (strMedium.isEmpty())
            continue;
        if (strMedium.contains(strTerm, Qt::CaseInsensitive))
        {
            // mark the item
            for (int j = 0; j < m_pTreeWidget->columnCount(); ++j)
                m_mediumItemList[i]->setData(j, Qt::ForegroundRole, QBrush(QColor(255, 0, 0)));
        }
    }
}

#include "UIMediumSelector.moc"
