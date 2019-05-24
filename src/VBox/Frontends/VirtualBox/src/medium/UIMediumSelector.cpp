/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSelector class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QAction>
#include <QHeaderView>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QPushButton>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#include "QIMessageBox.h"
#include "QITabWidget.h"
#include "QIToolButton.h"
#include "VBoxGlobal.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIFDCreationDialog.h"
#include "UIMediumSearchWidget.h"
#include "UIMediumSelector.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "UIMedium.h"
#include "UIMediumItem.h"
#include "UIToolBar.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CMediumAttachment.h"
#include "CMediumFormat.h"
#include "CStorageController.h"
#include "CSystemProperties.h"

#ifdef VBOX_WS_MAC
# include "UIWindowMenuManager.h"
#endif /* VBOX_WS_MAC */


UIMediumSelector::UIMediumSelector(UIMediumDeviceType enmMediumType, const QString &machineName /* = QString() */,
                                   const QString &machineSettingsFilePath /* = QString() */,
                                   const QString &strMachineGuestOSTypeId /*= QString() */, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_pCentralWidget(0)
    , m_pMainLayout(0)
    , m_pTreeWidget(0)
    , m_enmMediumType(enmMediumType)
    , m_pButtonBox(0)
    , m_pCancelButton(0)
    , m_pChooseButton(0)
    , m_pLeaveEmptyButton(0)
    , m_pMainMenu(0)
    , m_pToolBar(0)
    , m_pActionAdd(0)
    , m_pActionCreate(0)
    , m_pActionRefresh(0)
    , m_pAttachedSubTreeRoot(0)
    , m_pNotAttachedSubTreeRoot(0)
    , m_pParent(pParent)
    , m_pSearchWidget(0)
    , m_iCurrentShownIndex(0)
    , m_strMachineFolder(machineSettingsFilePath)
    , m_strMachineName(machineName)
    , m_strMachineGuestOSTypeId(strMachineGuestOSTypeId)
{
    if (vboxGlobal().uiType() == VBoxGlobal::UIType_RuntimeUI)
        vboxGlobal().startMediumEnumeration();
    configure();
    finalize();
}

void UIMediumSelector::setEnableCreateAction(bool fEnable)
{
    if (!m_pActionCreate)
        return;
    m_pActionCreate->setEnabled(fEnable);
    m_pActionCreate->setVisible(fEnable);
}

QList<QUuid> UIMediumSelector::selectedMediumIds() const
{
    QList<QUuid> selectedIds;
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
    if (m_pMainMenu)
        m_pMainMenu->setTitle(tr("Medium"));

    if (m_pActionAdd)
    {
        m_pActionAdd->setText(tr("&Add..."));
        m_pActionAdd->setToolTip(tr("Add Disk Image"));
        m_pActionAdd->setStatusTip(tr("Add existing disk image file"));
    }

    if (m_pActionCreate)
    {
        m_pActionCreate->setText(tr("&Create..."));
        m_pActionCreate->setToolTip(tr("Create Disk Image"));
        m_pActionCreate->setStatusTip(tr("Create new disk image file"));
    }

    if (m_pActionRefresh)
    {
        m_pActionRefresh->setText(tr("&Refresh"));
        m_pActionRefresh->setToolTip(tr("Refresh Disk Image Files (%1)").arg(m_pActionRefresh->shortcut().toString()));
        m_pActionRefresh->setStatusTip(tr("Refresh the list of disk image files"));
    }

    if (m_pCancelButton)
        m_pCancelButton->setText(tr("Cancel"));
    if (m_pLeaveEmptyButton)
        m_pLeaveEmptyButton->setText(tr("Leave Empty"));
    if (m_pChooseButton)
        m_pChooseButton->setText(tr("Choose"));

    if (m_pTreeWidget)
    {
        m_pTreeWidget->headerItem()->setText(0, tr("Name"));
        m_pTreeWidget->headerItem()->setText(1, tr("Virtual Size"));
        m_pTreeWidget->headerItem()->setText(2, tr("Actual Size"));
    }
}

void UIMediumSelector::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/media_manager_32px.png", ":/media_manager_16px.png"));
    setTitle();
    prepareWidgets();
    prepareActions();
    prepareMenuAndToolBar();
    prepareConnections();
}

void UIMediumSelector::prepareActions()
{
    QString strPrefix("hd");
    switch (m_enmMediumType)
    {
        case UIMediumDeviceType_DVD:
            strPrefix = "cd";
            break;
        case UIMediumDeviceType_Floppy:
            strPrefix = "fd";
            break;
        case UIMediumDeviceType_HardDisk:
        case UIMediumDeviceType_All:
        case UIMediumDeviceType_Invalid:
        default:
            strPrefix = "hd";
            break;
    }

    m_pActionAdd = new QAction(this);
    if (m_pActionAdd)
    {
        /* Configure add-action: */
        m_pActionAdd->setShortcut(QKeySequence(""));

        m_pActionAdd->setIcon(UIIconPool::iconSetFull(QString(":/%1_add_32px.png").arg(strPrefix),
                                                      QString(":/%1_add_16px.png").arg(strPrefix),
                                                      QString(":/%1_add_disabled_32px.png").arg(strPrefix),
                                                      QString(":/%1_add_disabled_16px.png").arg(strPrefix)));

    }

    m_pActionCreate = new QAction(this);
    if (m_pActionCreate)
    {
        m_pActionCreate->setShortcut(QKeySequence(""));
        m_pActionCreate->setIcon(UIIconPool::iconSetFull(QString(":/%1_create_32px.png").arg(strPrefix),
                                                         QString(":/%1_create_16px.png").arg(strPrefix),
                                                         QString(":/%1_create_disabled_32px.png").arg(strPrefix),
                                                         QString(":/%1_create_disabled_16px.png").arg(strPrefix)));
    }


    m_pActionRefresh = new QAction(this);
    if (m_pActionRefresh)
    {
        m_pActionRefresh->setShortcut(QKeySequence());
        if (m_pActionRefresh && m_pActionRefresh->icon().isNull())
            m_pActionRefresh->setIcon(UIIconPool::iconSetFull(":/refresh_32px.png", ":/refresh_16px.png",
                                                              ":/refresh_disabled_32px.png", ":/refresh_disabled_16px.png"));
    }
}

void UIMediumSelector::prepareMenuAndToolBar()
{
    if (!m_pMainMenu || !m_pToolBar)
        return;

    m_pMainMenu->addAction(m_pActionAdd);
    m_pMainMenu->addAction(m_pActionCreate);
    m_pMainMenu->addSeparator();
    m_pMainMenu->addAction(m_pActionRefresh);

    m_pToolBar->addAction(m_pActionAdd);
    m_pToolBar->addAction(m_pActionCreate);
    m_pToolBar->addSeparator();
    m_pToolBar->addAction(m_pActionRefresh);
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
    {
        connect(m_pTreeWidget, &QITreeWidget::itemSelectionChanged, this, &UIMediumSelector::sltHandleItemSelectionChanged);
        connect(m_pTreeWidget, &QITreeWidget::itemDoubleClicked, this, &UIMediumSelector::sltHandleTreeWidgetDoubleClick);
        connect(m_pTreeWidget, &QITreeWidget::customContextMenuRequested, this, &UIMediumSelector::sltHandleTreeContextMenuRequest);
    }

    if (m_pCancelButton)
        connect(m_pCancelButton, &QPushButton::clicked, this, &UIMediumSelector::sltButtonCancel);
    if (m_pChooseButton)
        connect(m_pChooseButton, &QPushButton::clicked, this, &UIMediumSelector::sltButtonChoose);
    if (m_pLeaveEmptyButton)
        connect(m_pLeaveEmptyButton, &QPushButton::clicked, this, &UIMediumSelector::sltButtonLeaveEmpty);


    if (m_pSearchWidget)
    {
        connect(m_pSearchWidget, &UIMediumSearchWidget::sigPerformSearch,
                this, &UIMediumSelector::sltHandlePerformSearch);
    }
}

UIMediumItem* UIMediumSelector::addTreeItem(const UIMedium &medium, QITreeWidgetItem *pParent)
{
    if (!pParent)
        return 0;
    switch (m_enmMediumType)
    {
        case UIMediumDeviceType_DVD:
            return new UIMediumItemCD(medium, pParent);
            break;
        case UIMediumDeviceType_Floppy:
            return new UIMediumItemFD(medium, pParent);
            break;
        case UIMediumDeviceType_HardDisk:
        case UIMediumDeviceType_All:
        case UIMediumDeviceType_Invalid:
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
                AssertMsgFailed(("Parent medium with ID={%s} was not found!\n", medium.parentID().toString().toUtf8().constData()));
            /* Try to create parent medium-item: */
            else
                pParentMediumItem = createHardDiskItem(parentMedium, pParent);
        }
        if (pParentMediumItem)
        {
            pMediumItem = new UIMediumItemHD(medium, pParentMediumItem);
            LogRel2(("UIMediumManager: Child hard-disk medium-item with ID={%s} created.\n", medium.id().toString().toUtf8().constData()));
        }
        else
            AssertMsgFailed(("Parent medium with ID={%s} could not be created!\n", medium.parentID().toString().toUtf8().constData()));
    }

    /* No parents, thus just create item as top-level one: */
    else
    {
        pMediumItem = new UIMediumItemHD(medium, pParent);
        LogRel2(("UIMediumManager: Root hard-disk medium-item with ID={%s} created.\n", medium.id().toString().toUtf8().constData()));
    }
    return pMediumItem;
}

void UIMediumSelector::restoreSelection(const QList<QUuid> &selectedMediums, QVector<UIMediumItem*> &mediumList)
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
    m_pCentralWidget = new QWidget;
    if (!m_pCentralWidget)
        return;
    setCentralWidget(m_pCentralWidget);

    m_pMainLayout = new QVBoxLayout;
    m_pCentralWidget->setLayout(m_pMainLayout);

    if (!m_pMainLayout || !menuBar())
        return;

    m_pMainMenu = menuBar()->addMenu(tr("Medium"));

    m_pToolBar = new UIToolBar;
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        m_pTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pMainLayout->addWidget(m_pTreeWidget);
        m_pTreeWidget->setAlternatingRowColors(true);
        int iColumnCount = (m_enmMediumType == UIMediumDeviceType_HardDisk) ? 3 : 2;
        m_pTreeWidget->setColumnCount(iColumnCount);
        m_pTreeWidget->setSortingEnabled(true);
        m_pTreeWidget->sortItems(0, Qt::AscendingOrder);
        m_pTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
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
        m_pCancelButton = m_pButtonBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

        /* Only DVDs and Floppies can be left empty: */
        if (m_enmMediumType == UIMediumDeviceType_DVD || m_enmMediumType == UIMediumDeviceType_Floppy)
            m_pLeaveEmptyButton = m_pButtonBox->addButton(tr("Leave Empty"), QDialogButtonBox::ActionRole);

        m_pChooseButton = m_pButtonBox->addButton(tr("Choose"), QDialogButtonBox::AcceptRole);
        m_pCancelButton->setShortcut(Qt::Key_Escape);

        /* Add button-box into main layout: */
        m_pMainLayout->addWidget(m_pButtonBox);
    }

    repopulateTreeWidget();
}

void UIMediumSelector::sltButtonChoose()
{
    done(static_cast<int>(ReturnCode_Accepted));
}

void UIMediumSelector::sltButtonCancel()
{
    done(static_cast<int>(ReturnCode_Rejected));
}

void UIMediumSelector::sltButtonLeaveEmpty()
{
    done(static_cast<int>(ReturnCode_LeftEmpty));
}

void UIMediumSelector::sltAddMedium()
{
    QUuid uMediumID = vboxGlobal().openMediumWithFileOpenDialog(m_enmMediumType, this, m_strMachineFolder);
    if (uMediumID.isNull())
        return;
    repopulateTreeWidget();
    selectMedium(uMediumID);
}

void UIMediumSelector::sltCreateMedium()
{
    QUuid uMediumId = vboxGlobal().openMediumCreatorDialog(this, m_enmMediumType, m_strMachineFolder,
                                                           m_strMachineName, m_strMachineGuestOSTypeId);
    if (uMediumId.isNull())
        return;
    /* Update the tree widget making sure we show the new item: */
    repopulateTreeWidget();
    /* Select the new item: */
    selectMedium(uMediumId);
    /* Update the search: */
    m_pSearchWidget->search(m_pTreeWidget);
}

void UIMediumSelector::sltHandleItemSelectionChanged()
{
    updateChooseButton();
}

void UIMediumSelector::sltHandleTreeWidgetDoubleClick(QTreeWidgetItem * item, int column)
{
    Q_UNUSED(column);
    if (!dynamic_cast<UIMediumItem*>(item))
        return;
    accept();
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
    /* Update the search: */
    m_pSearchWidget->search(m_pTreeWidget);
}

void UIMediumSelector::sltHandlePerformSearch()
{
    //performMediumSearch();
    if (!m_pSearchWidget)
        return;
    m_pSearchWidget->search(m_pTreeWidget);
}

void UIMediumSelector::sltHandleTreeContextMenuRequest(const QPoint &point)
{
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    if (!pSender)
        return;

    QMenu menu;
    QAction *pExpandAll = menu.addAction(tr("Expand All"));
    QAction *pCollapseAll = menu.addAction(tr("Collapse All"));
    if (!pExpandAll || !pCollapseAll)
        return;

    pExpandAll->setIcon(UIIconPool::iconSet(":/expand_all_16px.png"));
    pCollapseAll->setIcon(UIIconPool::iconSet(":/collapse_all_16px.png"));

    connect(pExpandAll, &QAction::triggered, this, &UIMediumSelector::sltHandleTreeExpandAllSignal);
    connect(pCollapseAll, &QAction::triggered, this, &UIMediumSelector::sltHandleTreeCollapseAllSignal);

    menu.exec(pSender->mapToGlobal(point));
}

void UIMediumSelector::sltHandleTreeExpandAllSignal()
{
    if (m_pTreeWidget)
        m_pTreeWidget->expandAll();
}

void UIMediumSelector::sltHandleTreeCollapseAllSignal()
{
    if (m_pTreeWidget)
        m_pTreeWidget->collapseAll();

    if (m_pAttachedSubTreeRoot)
        m_pTreeWidget->setExpanded(m_pTreeWidget->itemIndex(m_pAttachedSubTreeRoot), true);
    if (m_pNotAttachedSubTreeRoot)
        m_pTreeWidget->setExpanded(m_pTreeWidget->itemIndex(m_pNotAttachedSubTreeRoot), true);
}

void UIMediumSelector::selectMedium(const QUuid &uMediumID)
{
    if (!m_pTreeWidget)
        return;
    UIMediumItem *pMediumItem = searchItem(0, uMediumID);
    if (pMediumItem)
    {
        m_pTreeWidget->setCurrentItem(pMediumItem);

    }
}

void UIMediumSelector::updateChooseButton()
{

    if (!m_pTreeWidget || !m_pChooseButton)
        return;
    QList<QTreeWidgetItem*> selectedItems = m_pTreeWidget->selectedItems();
    if (selectedItems.isEmpty())
    {
        m_pChooseButton->setEnabled(false);
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
        m_pChooseButton->setEnabled(true);
    else
        m_pChooseButton->setEnabled(false);
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
    QList<QUuid> selectedMedia = selectedMediumIds();
    /* uuid list of selected items: */
    /* Reset the related data structure: */
    m_mediumItemList.clear();
    m_pTreeWidget->clear();
    m_pAttachedSubTreeRoot = 0;
    m_pNotAttachedSubTreeRoot = 0;
    QVector<UIMediumItem*> menuItemVector;

    foreach (const QUuid &uMediumID, vboxGlobal().mediumIDs())
    {
        UIMedium medium = vboxGlobal().medium(uMediumID);
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
    restoreSelection(selectedMedia, menuItemVector);
    saveDefaultForeground();
    updateChooseButton();
    if (m_pAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pAttachedSubTreeRoot);

    if (m_pNotAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pNotAttachedSubTreeRoot);

    m_pTreeWidget->resizeColumnToContents(0);
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

UIMediumItem* UIMediumSelector::searchItem(const QTreeWidgetItem *pParent, const QUuid &mediumId)
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
        UIMediumItem *pResult = searchItem(pChild, mediumId);
        if (pResult)
            return pResult;
    }
    return 0;
}

void UIMediumSelector::scrollToItem(UIMediumItem* pItem)
{
    if (!pItem)
        return;

    QModelIndex itemIndex = m_pTreeWidget->itemIndex(pItem);
    for (int i = 0; i < m_mediumItemList.size(); ++i)
    {
        QFont font = m_mediumItemList[i]->font(0);
        font.setBold(false);
        m_mediumItemList[i]->setFont(0, font);
    }
    QFont font = pItem->font(0);
    font.setBold(true);
    pItem->setFont(0, font);

    m_pTreeWidget->scrollTo(itemIndex);
}

void UIMediumSelector::setTitle()
{
    switch (m_enmMediumType)
    {
        case UIMediumDeviceType_DVD:
            if (!m_strMachineName.isEmpty())
                setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Optical Disk Selector")));
            else
                setWindowTitle(QString("%1").arg(tr("Optical Disk Selector")));
            break;
        case UIMediumDeviceType_Floppy:
            if (!m_strMachineName.isEmpty())
                setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Floppy Disk Selector")));
            else
                setWindowTitle(QString("%1").arg(tr("Floppy Disk Selector")));
            break;
        case UIMediumDeviceType_HardDisk:
            if (!m_strMachineName.isEmpty())
                setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Hard Disk Selector")));
            else
                setWindowTitle(QString("%1").arg(tr("Hard Disk Selector")));
            break;
        case UIMediumDeviceType_All:
        case UIMediumDeviceType_Invalid:
        default:
            if (!m_strMachineName.isEmpty())
                setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Virtual Medium Selector")));
            else
                setWindowTitle(QString("%1").arg(tr("Virtual Medium Selector")));
            break;
    }
}
