/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic1 class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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
# include <QVBoxLayout>

/* GUI includes: */
# include "QILabelSeparator.h"
# include "QIRichTextLabel.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageBasic1.h"

/* COM includes: */
# include "CMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage1 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage1::UIWizardExportAppPage1()
{
}

void UIWizardExportAppPage1::populateVMSelectorItems(const QStringList &selectedVMNames)
{
    /* Add all VM items into VM selector: */
    foreach (const CMachine &machine, vboxGlobal().virtualBox().GetMachines())
    {
        QPixmap pixIcon;
        QString strName;
        QUuid uUuid;
        bool fInSaveState = false;
        bool fEnabled = false;
        const QStyle *pStyle = QApplication::style();
        const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
        if (machine.GetAccessible())
        {
            pixIcon = vboxGlobal().vmUserPixmapDefault(machine);
            if (pixIcon.isNull())
                pixIcon = vboxGlobal().vmGuestOSTypePixmapDefault(machine.GetOSTypeId());
            strName = machine.GetName();
            uUuid = machine.GetId();
            fEnabled = machine.GetSessionState() == KSessionState_Unlocked;
            fInSaveState = machine.GetState() == KMachineState_Saved;
        }
        else
        {
            QString settingsFile = machine.GetSettingsFilePath();
            QFileInfo fi(settingsFile);
            strName = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxFileExts) ? fi.completeBaseName() : fi.fileName();
            pixIcon = QPixmap(":/os_other.png").scaled(iIconMetric, iIconMetric, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        QListWidgetItem *pItem = new UIVMListWidgetItem(pixIcon, strName, uUuid, fInSaveState, m_pVMSelector);
        if (!fEnabled)
            pItem->setFlags(0);
        m_pVMSelector->addItem(pItem);
    }
    m_pVMSelector->sortItems();

    /* Choose initially selected items (if passed): */
    for (int i = 0; i < selectedVMNames.size(); ++i)
    {
        QList<QListWidgetItem*> list = m_pVMSelector->findItems(selectedVMNames[i], Qt::MatchExactly);
        if (list.size() > 0)
        {
            if (m_pVMSelector->selectedItems().isEmpty())
                m_pVMSelector->setCurrentItem(list.first());
            else
                list.first()->setSelected(true);
        }
    }
}

QStringList UIWizardExportAppPage1::machineNames() const
{
    /* Prepare list: */
    QStringList machineNames;
    /* Iterate over all the selected items: */
    foreach (QListWidgetItem *pItem, m_pVMSelector->selectedItems())
        machineNames << pItem->text();
    /* Return result list: */
    return machineNames;
}

QList<QUuid> UIWizardExportAppPage1::machineIDs() const
{
    /* Prepare list: */
    QList<QUuid> machineIDs;
    /* Iterate over all the selected items: */
    foreach (QListWidgetItem *pItem, m_pVMSelector->selectedItems())
        machineIDs.append(static_cast<UIVMListWidgetItem*>(pItem)->uuid());
    /* Return result list: */
    return machineIDs;
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic1 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic1::UIWizardExportAppPageBasic1(const QStringList &selectedVMNames)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel;
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create VM selector: */
        m_pVMSelector = new QListWidget;
        if (m_pVMSelector)
        {
            m_pVMSelector->setAlternatingRowColors(true);
            m_pVMSelector->setSelectionMode(QAbstractItemView::ExtendedSelection);

            /* Add into layout: */
            pMainLayout->addWidget(m_pVMSelector);
        }
    }

    /* Populate VM selector items: */
    populateVMSelectorItems(selectedVMNames);

    /* Setup connections: */
    connect(m_pVMSelector, &QListWidget::itemSelectionChanged, this, &UIWizardExportAppPageBasic1::completeChanged);

    /* Register fields: */
    registerField("machineNames", this, "machineNames");
    registerField("machineIDs", this, "machineIDs");
}

void UIWizardExportAppPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Virtual machines to export"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("<p>Please select the virtual machines that should be added to the appliance. "
                                            "You can select more than one. Please note that these machines have to be "
                                            "turned off before they can be exported.</p>"));
}

void UIWizardExportAppPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardExportAppPageBasic1::isComplete() const
{
    /* There should be at least one VM selected: */
    return m_pVMSelector->selectedItems().size() > 0;
}

bool UIWizardExportAppPageBasic1::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Ask user about machines which are in Saved state currently: */
    QStringList savedMachines;
    QList<QListWidgetItem*> items = m_pVMSelector->selectedItems();
    for (int i=0; i < items.size(); ++i)
    {
        if (static_cast<UIVMListWidgetItem*>(items.at(i))->isInSaveState())
            savedMachines << items.at(i)->text();
    }
    if (!savedMachines.isEmpty())
        fResult = msgCenter().confirmExportMachinesInSaveState(savedMachines, this);

    /* Return result: */
    return fResult;
}
