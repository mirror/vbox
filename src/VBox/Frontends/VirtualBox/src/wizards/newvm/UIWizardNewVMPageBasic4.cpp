/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageBasic4 class implementation.
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
#include <QButtonGroup>
#include <QGridLayout>
#include <QMetaType>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UIMediumSelector.h"
#include "UIMessageCenter.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVMPageBasic4.h"

UIWizardNewVMPage4::UIWizardNewVMPage4()
    : m_fRecommendedNoDisk(false)
    , m_pDiskEmpty(0)
    , m_pDiskNew(0)
    , m_pDiskExisting(0)
    , m_pDiskSelector(0)
    , m_pDiskSelectionButton(0)
    , m_enmSelectedDiskSource(SelectedDiskSource_New)
{
}

SelectedDiskSource UIWizardNewVMPage4::selectedDiskSource() const
{
    return m_enmSelectedDiskSource;
}

void UIWizardNewVMPage4::setSelectedDiskSource(SelectedDiskSource enmSelectedDiskSource)
{
    m_enmSelectedDiskSource = enmSelectedDiskSource;
}

void UIWizardNewVMPage4::getWithFileOpenDialog()
{
    /* Get opened medium id: */
    QUuid uMediumId;

    int returnCode = uiCommon().openMediumSelectorDialog(thisImp(), UIMediumDeviceType_HardDisk,
                                                           uMediumId,
                                                           fieldImp("machineFolder").toString(),
                                                           fieldImp("machineBaseName").toString(),
                                                           fieldImp("type").value<CGuestOSType>().GetId(),
                                                           false /* don't show/enable the create action: */);

    if (returnCode == static_cast<int>(UIMediumSelector::ReturnCode_Accepted) && !uMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pDiskSelector->setCurrentItem(uMediumId);
        /* Focus on hard disk combo: */
        m_pDiskSelector->setFocus();
        m_virtualDisk = uiCommon().medium(uMediumId).medium();
    }
}

bool UIWizardNewVMPage4::getWithNewVirtualDiskWizard()
{
    /* Create New Virtual Hard Drive wizard: */
    UISafePointerWizardNewVD pWizard = new UIWizardNewVD(thisImp(),
                                                         fieldImp("machineBaseName").toString(),
                                                         fieldImp("machineFolder").toString(),
                                                         fieldImp("type").value<CGuestOSType>().GetRecommendedHDD(),
                                                         wizardImp()->mode());
    pWizard->prepare();
    bool fResult = false;
    if (pWizard->exec() == QDialog::Accepted)
    {
        fResult = true;
        m_virtualDisk = pWizard->virtualDisk();
        m_pDiskSelector->setCurrentItem(m_virtualDisk.GetId());
        m_pDiskExisting->click();
    }
    if (pWizard)
        delete pWizard;
    return fResult;
}

void UIWizardNewVMPage4::ensureNewVirtualDiskDeleted()
{
    /* Make sure virtual-disk valid: */
    if (m_virtualDisk.isNull())
        return;

    /* Remember virtual-disk attributes: */
    QString strLocation = m_virtualDisk.GetLocation();
    /* Prepare delete storage progress: */
    CProgress progress = m_virtualDisk.DeleteStorage();
    if (m_virtualDisk.isOk())
    {
        /* Show delete storage progress: */
        msgCenter().showModalProgressDialog(progress, thisImp()->windowTitle(), ":/progress_media_delete_90px.png", thisImp());
        if (!progress.isOk() || progress.GetResultCode() != 0)
            msgCenter().cannotDeleteHardDiskStorage(progress, strLocation, thisImp());
    }
    else
        msgCenter().cannotDeleteHardDiskStorage(m_virtualDisk, strLocation, thisImp());

    /* Detach virtual-disk anyway: */
    m_virtualDisk.detach();
}

void UIWizardNewVMPage4::retranslateWidgets()
{
    if (m_pDiskEmpty)
        m_pDiskEmpty->setText(UIWizardNewVM::tr("&Do not add a virtual hard disk"));
    if (m_pDiskNew)
        m_pDiskNew->setText(UIWizardNewVM::tr("&Create a virtual hard disk now"));
    if (m_pDiskExisting)
        m_pDiskExisting->setText(UIWizardNewVM::tr("&Use an existing virtual hard disk file"));
    if (m_pDiskSelectionButton)
        m_pDiskSelectionButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
}

void UIWizardNewVMPage4::setEnableDiskSelectionWidgets(bool fEnabled)
{
    if (!m_pDiskSelector || !m_pDiskSelectionButton)
        return;

    m_pDiskSelector->setEnabled(fEnabled);
    m_pDiskSelectionButton->setEnabled(fEnabled);
}

QWidget *UIWizardNewVMPage4::createDiskWidgets()
{
    QWidget *pDiskContainer = new QWidget;
    QGridLayout *pDiskLayout = new QGridLayout(pDiskContainer);
    m_pDiskSourceButtonGroup = new QButtonGroup;
    m_pDiskEmpty = new QRadioButton;
    m_pDiskNew = new QRadioButton;
    m_pDiskExisting = new QRadioButton;
    m_pDiskSourceButtonGroup->addButton(m_pDiskEmpty);
    m_pDiskSourceButtonGroup->addButton(m_pDiskNew);
    m_pDiskSourceButtonGroup->addButton(m_pDiskExisting);
    QStyleOptionButton options;
    options.initFrom(m_pDiskExisting);
    int iWidth = m_pDiskExisting->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &options, m_pDiskExisting);
    pDiskLayout->setColumnMinimumWidth(0, iWidth);
    m_pDiskSelector = new UIMediaComboBox;
    {
        m_pDiskSelector->setType(UIMediumDeviceType_HardDisk);
        m_pDiskSelector->repopulate();
    }
    m_pDiskSelectionButton = new QIToolButton;
    {
        m_pDiskSelectionButton->setAutoRaise(true);
        m_pDiskSelectionButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_disabled_16px.png"));
    }
    pDiskLayout->addWidget(m_pDiskEmpty, 0, 0, 1, 3);
    pDiskLayout->addWidget(m_pDiskNew, 1, 0, 1, 3);
    pDiskLayout->addWidget(m_pDiskExisting, 2, 0, 1, 3);
    pDiskLayout->addWidget(m_pDiskSelector, 3, 1);
    pDiskLayout->addWidget(m_pDiskSelectionButton, 3, 2);
    return pDiskContainer;
}

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
    : m_pLabel(0)
{
    prepare();
    qRegisterMetaType<CMedium>();
    qRegisterMetaType<SelectedDiskSource>();
    registerField("virtualDisk", this, "virtualDisk");
    registerField("diskSource", this, "diskSource");
}

int UIWizardNewVMPageBasic4::nextId() const
{
    if (m_pDiskNew->isChecked())
        return UIWizardNewVM::Page5;
    return UIWizardNewVM::Page8;
}

void UIWizardNewVMPageBasic4::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(createDiskWidgets());

    pMainLayout->addStretch();
    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    createConnections();
}

void UIWizardNewVMPageBasic4::createConnections()
{
    connect(m_pDiskSourceButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
            this, &UIWizardNewVMPageBasic4::sltHandleSelectedDiskSourceChange);
    connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
            this, &UIWizardNewVMPageBasic4::sltVirtualSelectedDiskSourceChanged);
    connect(m_pDiskSelectionButton, &QIToolButton::clicked,
            this, &UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog);
}

void UIWizardNewVMPageBasic4::sltHandleSelectedDiskSourceChange()
{
    if (!m_pDiskSourceButtonGroup)
        return;

    if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskEmpty)
        setSelectedDiskSource(SelectedDiskSource_Empty);
    else if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskExisting)
        setSelectedDiskSource(SelectedDiskSource_Existing);
    else
        setSelectedDiskSource(SelectedDiskSource_New);

    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    completeChanged();
}

void UIWizardNewVMPageBasic4::sltVirtualSelectedDiskSourceChanged()
{
    emit completeChanged();
}

void UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog()
{
    /* Call to base-class: */
    getWithFileOpenDialog();
}

void UIWizardNewVMPageBasic4::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Virtual Hard disk"));

    QString strRecommendedHDD = field("type").value<CGuestOSType>().isNull() ? QString() :
                                UICommon::formatSize(field("type").value<CGuestOSType>().GetRecommendedHDD());
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>If you wish you can add a virtual hard disk to the new machine. "
                                            "You can either create a new hard disk file or select one from the list "
                                            "or from another location using the folder icon. "
                                            "If you need a more complex storage set-up you can skip this step "
                                            "and make the changes to the machine settings once the machine is created. "
                                            "The recommended size of the hard disk is <b>%1</b>.</p>")
                          .arg(strRecommendedHDD));

    retranslateWidgets();
}

void UIWizardNewVMPageBasic4::initializePage()
{
    retranslateUi();

    if (!field("type").canConvert<CGuestOSType>())
        return;

    CGuestOSType type = field("type").value<CGuestOSType>();

    if (type.GetRecommendedHDD() != 0)
    {
        if (m_pDiskNew)
        {
            m_pDiskNew->setFocus();
            m_pDiskNew->setChecked(true);
        }
        m_fRecommendedNoDisk = false;
    }
    else
    {
        if (m_pDiskEmpty)
        {
            m_pDiskEmpty->setFocus();
            m_pDiskEmpty->setChecked(true);
        }
        m_fRecommendedNoDisk = true;
     }
    if (m_pDiskSelector)
        m_pDiskSelector->setCurrentIndex(0);
}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    if (!m_pDiskEmpty)
        return false;
    return m_pDiskEmpty->isChecked() ||
        !m_pDiskExisting->isChecked() ||
        !uiCommon().medium(m_pDiskSelector->id()).isNull();
}

// bool UIWizardNewVMPageBasic4::validatePage()
// {
//     /* Initial result: */
//     bool fResult = true;

//     /* Ensure unused virtual-disk is deleted: */
//     if (m_pDiskEmpty->isChecked() || m_pDiskNew->isChecked() || (!m_virtualDisk.isNull() && m_uVirtualDiskId != m_virtualDisk.GetId()))
//         ensureNewVirtualDiskDeleted();

//     if (m_pDiskEmpty->isChecked())
//     {
//         /* Ask user about disk-less machine unless that's the recommendation: */
//         if (!m_fRecommendedNoDisk)
//             fResult = msgCenter().confirmHardDisklessMachine(thisImp());
//     }
//     else if (m_pDiskNew->isChecked())
//     {
//         /* Show the New Virtual Hard Drive wizard: */
//         fResult = getWithNewVirtualDiskWizard();
//     }

//     if (fResult)
//     {
//         /* Lock finish button: */
//         startProcessing();

//         /* Try to create VM: */
//         fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVM();

//         /* Unlock finish button: */
//         endProcessing();
//     }

//     /* Return result: */
//     return fResult;
// }
