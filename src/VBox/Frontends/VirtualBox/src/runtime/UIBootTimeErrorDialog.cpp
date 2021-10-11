/* $Id$ */
/** @file
 * VBox Qt GUI - UIBootTimeErrorDialog class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QLabel>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QPushButton>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIToolButton.h"
#include "QIRichTextLabel.h"

#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"

#include "UIFilePathSelector.h"
#include "UIBootTimeErrorDialog.h"

#include "UIIconPool.h"
#include "UIModalWindowManager.h"
#include "UIMessageCenter.h"


#include "CMediumAttachment.h"
#include "CStorageController.h"

UIBootTimeErrorDialog::UIBootTimeErrorDialog(QWidget *pParent, const CMachine &comMachine)
    :QIWithRetranslateUI<QIMainDialog>(pParent)
    , m_pParent(pParent)
    , m_pCentralWidget(0)
    , m_pMainLayout(0)
    , m_pButtonBox(0)
    , m_pCloseButton(0)
    , m_pResetButton(0)
    , m_pLabel(0)
    , m_pBootImageSelector(0)
    , m_pBootImageLabel(0)
    , m_comMachine(comMachine)
{
    configure();
}

QString UIBootTimeErrorDialog::bootMediumPath() const
{
    if (!m_pBootImageSelector)
        return QString();
    return m_pBootImageSelector->path();
}

void UIBootTimeErrorDialog::retranslateUi()
{
    if (m_pCloseButton)
        m_pCloseButton->setText(tr("&Close"));
    if (m_pResetButton)
        m_pResetButton->setText(tr("&Reset"));

    if (m_pLabel)
        m_pLabel->setText(tr("The virtual machine failed to boot. That might be caused by a missing operating system "
                             "or misconfigured boot order. Mounting an operation install DVD might solve this problem. "
                             "Selecting an ISO file will attemt to mount it immediately to the guest machine."));
    if (m_pBootImageLabel)
        m_pBootImageLabel->setText(tr("Boot DVD:"));
}

void UIBootTimeErrorDialog::configure()
{
    setWindowIcon(UIIconPool::iconSetFull(":/media_manager_32px.png", ":/media_manager_16px.png"));
    setTitle();
    prepareWidgets();
    prepareConnections();
}

void UIBootTimeErrorDialog::prepareConnections()
{
    if (m_pCloseButton)
        connect(m_pCloseButton, &QPushButton::clicked, this, &UIBootTimeErrorDialog::sltCancel);
    if (m_pResetButton)
        connect(m_pResetButton, &QPushButton::clicked, this, &UIBootTimeErrorDialog::sltReset);
}

void UIBootTimeErrorDialog::prepareWidgets()
{
    m_pCentralWidget = new QWidget;
    if (!m_pCentralWidget)
        return;
    setCentralWidget(m_pCentralWidget);

    m_pMainLayout = new QVBoxLayout;
    m_pCentralWidget->setLayout(m_pMainLayout);

    if (!m_pMainLayout || !menuBar())
        return;

    m_pLabel = new QIRichTextLabel;
    if (m_pLabel)
        m_pMainLayout->addWidget(m_pLabel);

    QHBoxLayout *pSelectorLayout = new QHBoxLayout;
    pSelectorLayout->setContentsMargins(0, 0, 0, 0);
    m_pBootImageLabel = new QLabel;

    if (m_pBootImageLabel)
        pSelectorLayout->addWidget(m_pBootImageLabel);

    m_pBootImageSelector = new UIFilePathSelector;
    if (m_pBootImageSelector)
    {
        m_pBootImageSelector->setMode(UIFilePathSelector::Mode_File_Open);
        m_pBootImageSelector->setFileDialogFilters("ISO Images(*.iso *.ISO)");
        m_pBootImageSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
        m_pBootImageSelector->setRecentMediaListType(UIMediumDeviceType_DVD);
        if (m_pBootImageLabel)
            m_pBootImageLabel->setBuddy(m_pBootImageSelector);
        pSelectorLayout->addWidget(m_pBootImageSelector);
        connect(m_pBootImageSelector, &UIFilePathSelector::pathChanged,
                this, &UIBootTimeErrorDialog::sltFileSelectorPathChanged);
    }
    m_pMainLayout->addLayout(pSelectorLayout);
    m_pButtonBox = new QIDialogButtonBox;
    if (m_pButtonBox)
    {
        m_pCloseButton = m_pButtonBox->addButton(QString(), QDialogButtonBox::RejectRole);
        m_pResetButton = m_pButtonBox->addButton(QString(), QDialogButtonBox::ActionRole);
        m_pCloseButton->setShortcut(Qt::Key_Escape);

        m_pMainLayout->addWidget(m_pButtonBox);
    }
    m_pMainLayout->addStretch();
    retranslateUi();
}

void UIBootTimeErrorDialog::sltCancel()
{
    done(static_cast<int>(ReturnCode_Close));
}

void UIBootTimeErrorDialog::sltReset()
{
    done(static_cast<int>(ReturnCode_Reset));
}

void UIBootTimeErrorDialog::showEvent(QShowEvent *pEvent)
{
    Q_UNUSED(pEvent);

    /* Try to determine the initial size: */
    // QSize proposedSize;
    // int iHostScreen = 0;
    // if (m_pParent)
    //     iHostScreen = gpDesktop->screenNumber(m_pParent);
    // else
    //     iHostScreen = gpDesktop->screenNumber(this);
    // if (iHostScreen >= 0 && iHostScreen < gpDesktop->screenCount())
    // {
    //     /* On the basis of current host-screen geometry if possible: */
    //     const QRect screenGeometry = gpDesktop->screenGeometry(iHostScreen);
    //     if (screenGeometry.isValid())
    //         proposedSize = screenGeometry.size() * 5 / 15;
    // }
    // /* Fallback to default size if we failed: */
    // if (proposedSize.isNull())
    //     proposedSize = QSize(800, 600);
    // /* Resize to initial size: */
    // resize(proposedSize);

    if (m_pParent)
        UIDesktopWidgetWatchdog::centerWidget(this, m_pParent, false);

}

void UIBootTimeErrorDialog::setTitle()
{
}

bool UIBootTimeErrorDialog::insertBootMedium(const QUuid &uMediumId)
{
    AssertReturn(!uMediumId.isNull(), false);

    CVirtualBox comVBox = uiCommon().virtualBox();
    const CGuestOSType &comOsType = comVBox.GetGuestOSType(m_comMachine.GetOSTypeId());
    /* Get recommended controller bus & type: */
    const KStorageBus enmRecommendedDvdBus = comOsType.GetRecommendedDVDStorageBus();
    const KStorageControllerType enmRecommendedDvdType = comOsType.GetRecommendedDVDStorageController();

    CMediumAttachment comAttachment;
    /* Search for an attachment of required bus & type: */
    foreach (const CMediumAttachment &comCurrentAttachment, m_comMachine.GetMediumAttachments())
    {
        /* Determine current attachment's controller: */
        const CStorageController &comCurrentController = m_comMachine.GetStorageControllerByName(comCurrentAttachment.GetController());

        if (   comCurrentController.GetBus() == enmRecommendedDvdBus
            && comCurrentController.GetControllerType() == enmRecommendedDvdType
            && comCurrentAttachment.GetType() == KDeviceType_DVD)
        {
            comAttachment = comCurrentAttachment;
            break;
        }
    }
    AssertMsgReturn(!comAttachment.isNull(), ("Storage Controller is NOT properly configured!\n"), false);

    const UIMedium guiMedium = uiCommon().medium(uMediumId);
    const CMedium comMedium = guiMedium.medium();

    /* Mount medium to the predefined port/device: */
    m_comMachine.MountMedium(comAttachment.GetController(), comAttachment.GetPort(), comAttachment.GetDevice(), comMedium, false /* force */);
    bool fSuccess = m_comMachine.isOk();

    QWidget *pParent = windowManager().realParentWindow(this);

    /* Show error message if necessary: */
    if (!fSuccess)
        msgCenter().cannotRemountMedium(m_comMachine, guiMedium, true /* mount? */, false /* retry? */, pParent);
    else
    {
        /* Save machine settings: */
        m_comMachine.SaveSettings();
        fSuccess = m_comMachine.isOk();

        /* Show error message if necessary: */
        if (!fSuccess)
            msgCenter().cannotSaveMachineSettings(m_comMachine, pParent);
    }
    return fSuccess;
}

void UIBootTimeErrorDialog::sltFileSelectorPathChanged(const QString &strPath)
{
    insertBootMedium(uiCommon().openMedium(UIMediumDeviceType_DVD, strPath));
}
