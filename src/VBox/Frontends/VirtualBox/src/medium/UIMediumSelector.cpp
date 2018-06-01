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
# include <QVBoxLayout>
# include <QPushButton>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIMessageBox.h"
# include "QITabWidget.h"
# include "VBoxGlobal.h"
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataManager.h"
# include "UIMediumSelector.h"
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


UIMediumSelector::UIMediumSelector(UIMediumType enmMediumType, QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QIDialog>(pParent)
    , m_pMainLayout(0)
    , m_pTreeWidget(0)
    , m_enmMediumType(enmMediumType)
    , m_pButtonBox(0)
    , m_pToolBar(0)
    , m_pActionAdd(0)
    , m_pActionRefresh(0)
    , m_pAttachedSubTreeRoot(0)
    , m_pNotAttachedSubTreeRoot(0)
    , m_pParent(pParent)
{
    configure();
    finalize();
    //setAttribute(Qt::WA_DeleteOnClose);
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
        m_pActionAdd->setText(QApplication::translate("UIMediumManager", "&Add from file..."));
        m_pActionAdd->setToolTip(QApplication::translate("UIMediumManager", "Add Disk Image File"));
        m_pActionAdd->setStatusTip(QApplication::translate("UIMediumManager", "Add disk image file"));
    }
    if (m_pActionRefresh)
    {
        m_pActionRefresh->setText(QApplication::translate("UIMediumManager","Re&fresh"));
        m_pActionRefresh->setToolTip(QApplication::translate("UIMediumManager","Refresh Disk Image Files (%1)").arg(m_pActionRefresh->shortcut().toString()));
        m_pActionRefresh->setStatusTip(QApplication::translate("UIMediumManager","Refresh the list of disk image files"));
    }

    if (m_pButtonBox)
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText("Add Selected");

    if (m_pTreeWidget)
    {
        m_pTreeWidget->headerItem()->setText(0, QApplication::translate("UIMediumManager","Name"));
    }
}

void UIMediumSelector::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/diskimage_32px.png", ":/diskimage_16px.png"));


    prepareActions();
    prepareWidgets();
    prepareConnections();

}

void UIMediumSelector::prepareActions()
{
    m_pActionAdd = new QAction(this);
    if (m_pActionAdd)
    {
        /* Configure add-action: */
        m_pActionAdd->setShortcut(QKeySequence("Ctrl+A"));
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

        m_pActionAdd->setIcon(UIIconPool::iconSetFull(QString(":/%1_add_22px.png").arg(strPrefix),
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
    if (m_pActionRefresh)
        connect(m_pActionRefresh, &QAction::triggered, this, &UIMediumSelector::sltHandleRefresh);

    if (m_pTreeWidget)
        connect(m_pTreeWidget, &QITreeWidget::itemSelectionChanged, this, &UIMediumSelector::sltHandleItemSelectionChanged);

    if (m_pButtonBox)
    {
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UIMediumSelector::close);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UIMediumSelector::accept);
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
            return new UIMediumItemHD(medium, pParent);
            break;
    }
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
        if (m_pActionRefresh)
            m_pToolBar->addAction(m_pActionRefresh);

        m_pMainLayout->addWidget(m_pToolBar);
    }

    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        m_pTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        m_pMainLayout->addWidget(m_pTreeWidget);

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
    //m_pMainLayout->addWidget(
}

void UIMediumSelector::sltAddMedium()
{
    QString strDefaultMachineFolder = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    vboxGlobal().openMediumWithFileOpenDialog(m_enmMediumType, this, strDefaultMachineFolder);
}

// void UIMediumSelector::sltHandleCurrentItemChanged()
// {
//     printf("%d\n", m_pTreeWidget->selectedItems().size());
//     updateOkButton();

// }

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
            proposedSize = screenGeometry.size() * 7 / 15;
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
    m_pTreeWidget->clear();
    m_pAttachedSubTreeRoot = 0;
    m_pNotAttachedSubTreeRoot = 0;
    QVector<UIMediumItem*> menuItemList;

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
            menuItemList.push_back(addTreeItem(medium, pParent));
        }
    }
    restoreSelection(selectedMediums, menuItemList);

    updateOkButton();
    if (m_pAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pAttachedSubTreeRoot);

    if (m_pNotAttachedSubTreeRoot)
        m_pTreeWidget->expandItem(m_pNotAttachedSubTreeRoot);

}
