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
# include <QFrame>
# include <QHBoxLayout>
# include <QHeaderView>
# include <QLabel>
# include <QMenuBar>
# include <QProgressBar>
# include <QPushButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIFileDialog.h"
# include "QILabel.h"
# include "QIMessageBox.h"
# include "QITabWidget.h"
# include "QITreeWidget.h"
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UIMediumManager.h"
# include "UIMediumSelector.h"
# include "UIWizardCloneVD.h"
# include "UIMessageCenter.h"
# include "UIToolBar.h"
# include "UIIconPool.h"
# include "UIMedium.h"
# include "UIMediumItem.h"

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
{
    configure();
    finalize();
}

void UIMediumSelector::retranslateUi()
{
}

void UIMediumSelector::configure()
{
    /* Apply window icons: */
    setWindowIcon(UIIconPool::iconSetFull(":/diskimage_32px.png", ":/diskimage_16px.png"));
    prepareWidgets();
}

void UIMediumSelector::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout;
    if (!m_pMainLayout)
        return;
    setLayout(m_pMainLayout);

    m_pTreeWidget = new QITreeWidget;
    if (m_pTreeWidget)
    {
        m_pMainLayout->addWidget(m_pTreeWidget);
    }
    repopulateTreeWidget();
    //m_pMainLayout->addWidget(
}

void UIMediumSelector::finalize()
{
    /* Apply language settings: */
    retranslateUi();
}

void UIMediumSelector::repopulateTreeWidget()
{
    // /* Remember current medium-items: */
    // if (UIMediumItem *pMediumItem = mediumItem(UIMediumType_HardDisk))
    //     m_strCurrentIdHD = pMediumItem->id();
    // if (UIMediumItem *pMediumItem = mediumItem(UIMediumType_DVD))
    //     m_strCurrentIdCD = pMediumItem->id();
    // if (UIMediumItem *pMediumItem = mediumItem(UIMediumType_Floppy))
    //     m_strCurrentIdFD = pMediumItem->id();

    // /* Clear tree-widgets: */
    // QITreeWidget *pTreeWidgetHD = treeWidget(UIMediumType_HardDisk);
    // if (pTreeWidgetHD)
    // {
    //     setCurrentItem(pTreeWidgetHD, 0);
    //     pTreeWidgetHD->clear();
    // }
    // QITreeWidget *pTreeWidgetCD = treeWidget(UIMediumType_DVD);
    // if (pTreeWidgetCD)
    // {
    //     setCurrentItem(pTreeWidgetCD, 0);
    //     pTreeWidgetCD->clear();
    // }
    // QITreeWidget *pTreeWidgetFD = treeWidget(UIMediumType_Floppy);
    // if (pTreeWidgetFD)
    // {
    //     setCurrentItem(pTreeWidgetFD, 0);
    //     pTreeWidgetFD->clear();
    // }

    // /* Create medium-items (do not change current one): */
    // m_fPreventChangeCurrentItem = true;
    // foreach (const QString &strMediumID, vboxGlobal().mediumIDs())
    // {
    //     //sltHandleMediumCreated(strMediumID);
    //     UIMedium medium = vboxGlobal().medium(strMediumID);
    //     if (medium.type() == m_enmMediumType)
    //         printf("%s %s\n", qPrintable(strMediumID), qPrintable(medium.name()));
    //     if (m_enmMediumType == UIMediumType_HardDisk)
    //     {
    //         //new UIMediumItemHD(medium, m_pTreeWidget);
    //     }
    // }
    // m_fPreventChangeCurrentItem = false;

    // /* Select first item as current one if nothing selected: */
    // if (pTreeWidgetHD && !mediumItem(UIMediumType_HardDisk))
    //     if (QTreeWidgetItem *pItem = pTreeWidgetHD->topLevelItem(0))
    //         setCurrentItem(pTreeWidgetHD, pItem);
    // if (pTreeWidgetCD && !mediumItem(UIMediumType_DVD))
    //     if (QTreeWidgetItem *pItem = pTreeWidgetCD->topLevelItem(0))
    //         setCurrentItem(pTreeWidgetCD, pItem);
    // if (pTreeWidgetFD && !mediumItem(UIMediumType_Floppy))
    //     if (QTreeWidgetItem *pItem = pTreeWidgetFD->topLevelItem(0))
    //         setCurrentItem(pTreeWidgetFD, pItem);
}
