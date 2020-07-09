/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMPageExpert class implementation.
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
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QToolBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemorySlider.h"
#include "UIBaseMemoryEditor.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UINameAndSystemEditor.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageExpert.h"


UIWizardNewVMPageExpert::UIWizardNewVMPageExpert(const QString &strGroup)
    : UIWizardNewVMPage1(strGroup)
    , m_pNameAndSystemContainer(0)
    , m_pToolBox(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pToolBox = new QToolBox;
        m_pNameAndSystemContainer = new QWidget(this);
        QGridLayout *pNameContainerLayout = new QGridLayout(m_pNameAndSystemContainer);
        createNameOSTypeWidgets(pNameContainerLayout, false);
        m_pGAInstallContainer = createGAInstallWidgets();
        m_pUsernameHostnameContainer = createUserNameHostNameWidgets();

        m_pToolBox->insertItem(ExpertToolboxItems_NameAndOSType, m_pNameAndSystemContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_UsernameHostname, m_pUsernameHostnameContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_GAInstall, m_pGAInstallContainer, "");
        m_pToolBox->insertItem(ExpertToolboxItems_ProductKey, createProductKeyWidgets(), "");
        m_pToolBox->insertItem(ExpertToolboxItems_Disk, createDiskWidgets(), "");
        m_pToolBox->insertItem(ExpertToolboxItems_Hardware, createHardwareWidgets(), "");
        pMainLayout->addWidget(m_pToolBox);
        pMainLayout->addStretch();
        updateVirtualDiskSource();
    }

    createConnections();

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("name*", m_pNameAndSystemEditor, "name", SIGNAL(sigNameChanged(const QString &)));
    registerField("type", m_pNameAndSystemEditor, "type", SIGNAL(sigOsTypeChanged()));
    registerField("machineFilePath", this, "machineFilePath");
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
    registerField("baseMemory", this, "baseMemory");
    registerField("virtualDisk", this, "virtualDisk");
    registerField("virtualDiskId", this, "virtualDiskId");
    registerField("virtualDiskLocation", this, "virtualDiskLocation");
    registerField("guestOSFamiyId", this, "guestOSFamiyId");
    registerField("ISOFilePath", this, "ISOFilePath");
    registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
    registerField("startHeadless", this, "startHeadless");
    registerField("detectedOSTypeId", this, "detectedOSTypeId");
    registerField("userName", this, "userName");
    registerField("password", this, "password");
    registerField("hostname", this, "hostname");
    registerField("installGuestAdditions", this, "installGuestAdditions");
    registerField("guestAdditionsISOPath", this, "guestAdditionsISOPath");
    registerField("productKey", this, "productKey");
    registerField("VCPUCount", this, "VCPUCount");
}

void UIWizardNewVMPageExpert::sltNameChanged(const QString &strNewText)
{
    /* Call to base-class: */
    onNameChanged(strNewText);

    /* Fetch recommended RAM value: */
    CGuestOSType type = m_pNameAndSystemEditor->type();

    composeMachineFilePath();
    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltPathChanged(const QString &strNewPath)
{
    Q_UNUSED(strNewPath);
    composeMachineFilePath();
}

void UIWizardNewVMPageExpert::sltOsTypeChanged()
{
    /* Call to base-class: */
    onOsTypeChanged();

    /* Fetch recommended RAM value: */
    CGuestOSType type = m_pNameAndSystemEditor->type();
    m_pBaseMemoryEditor->setValue(type.GetRecommendedRAM());

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged()
{
    /* Call to base-class: */
    updateVirtualDiskSource();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGetWithFileOpenDialog()
{
    /* Call to base-class: */
    getWithFileOpenDialog();
}

void UIWizardNewVMPageExpert::sltUnattendedCheckBoxToggle()
{
    if (m_pButtonUnattended)
        disableEnableUnattendedRelatedWidgets(m_pButtonUnattended->isChecked());
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltISOPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    setTypeByISODetectedOSType(m_strDetectedOSTypeId);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle(bool fEnabled)
{
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setEnabled(fEnabled);
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setEnabled(fEnabled);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGAISOPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltOSFamilyTypeChanged()
{
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::retranslateUi()
{
    if (m_pUnattendedLabel)
        m_pUnattendedLabel->setText(UIWizardNewVM::tr("Please choose whether you want to start an unattended guest os install "
                                             "in which case you will have to select a valid installation medium. If not "
                                             "your virtual disk will have an empty virtual hard disk. "
                                             "Additionally you can choose to start the unattended install as a headless vm process."));
    if (m_pButtonSimple)
    {
        m_pButtonSimple->setText(UIWizardNewVM::tr("Leave Disk Empty"));
        m_pButtonSimple->setToolTip(UIWizardNewVM::tr("When checked, no guest OS will be installed after this wizard is closed"));
    }
    if (m_pButtonUnattended)
    {
        m_pButtonUnattended->setText(UIWizardNewVM::tr("Unattended Install"));
        m_pButtonUnattended->setToolTip(UIWizardNewVM::tr("When checked, an unattended guest OS will be initialized after this wizard is closed"));
    }

    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setText(UIWizardNewVM::tr("Image:"));

    if (m_pStartHeadlessLabel)
        m_pStartHeadlessLabel->setText(UIWizardNewVM::tr("Options:"));
    if (m_pStartHeadlessCheckBox)
    {
        m_pStartHeadlessCheckBox->setText(UIWizardNewVM::tr("Start VM Headless"));
        m_pStartHeadlessCheckBox->setToolTip(UIWizardNewVM::tr("When checked, the unattended install will start the virtual machine headless"));
    }



    m_pDiskSkip->setText(UIWizardNewVM::tr("&Do not add a virtual hard disk"));
    m_pDiskCreate->setText(UIWizardNewVM::tr("&Create a virtual hard disk now"));
    m_pDiskPresent->setText(UIWizardNewVM::tr("&Use an existing virtual hard disk file"));
    m_pVMMButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
    if (m_pToolBox)
    {
        m_pToolBox->setItemText(ExpertToolboxItems_NameAndOSType, UIWizardNewVM::tr("Name and operating system"));
        m_pToolBox->setItemText(ExpertToolboxItems_UsernameHostname, UIWizardNewVM::tr("Username and hostname"));
        m_pToolBox->setItemText(ExpertToolboxItems_GAInstall, UIWizardNewVM::tr("Guest additions install"));
        m_pToolBox->setItemText(ExpertToolboxItems_ProductKey, UIWizardNewVM::tr("Product key"));
        m_pToolBox->setItemText(ExpertToolboxItems_Disk, UIWizardNewVM::tr("Hard disk"));
        m_pToolBox->setItemText(ExpertToolboxItems_Hardware, UIWizardNewVM::tr("Hardware"));
    }

    if (m_pHostnameLabel)
        m_pHostnameLabel->setText(UIWizardNewVM::tr("Hostname:"));
    if (m_pInstallGACheckBox)
        m_pInstallGACheckBox->setText(UIWizardNewVM::tr("Install guest additions"));
    if (m_pGAISOPathLabel)
        m_pGAISOPathLabel->setText(UIWizardNewVM::tr("Installation medium:"));
    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->setToolTip(UIWizardNewVM::tr("Please select an installation medium (ISO file)"));
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setText(UIWizardNewVM::tr("Product Key:"));

}

void UIWizardNewVMPageExpert::createConnections()
{
    /* Connections for Name, OS Type, and unattended install stuff: */
    if (m_pNameAndSystemEditor)
    {
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigNameChanged,
                this, &UIWizardNewVMPageExpert::sltNameChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigPathChanged,
                this, &UIWizardNewVMPageExpert::sltPathChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOsTypeChanged,
                this, &UIWizardNewVMPageExpert::sltOsTypeChanged);
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOSFamilyChanged,
                this, &UIWizardNewVMPageExpert::sltOSFamilyTypeChanged);
    }
    if (m_pButtonGroup)
        connect(m_pButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
                this, &UIWizardNewVMPageExpert::sltUnattendedCheckBoxToggle);
    if (m_pISOFilePathSelector)
        connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltISOPathChanged);

    /* Connections for username, password, and hostname: */
    if (m_pUserNamePasswordEditor)
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigSomeTextChanged,
                this, &UIWizardNewVMPageExpert::completeChanged);
    if (m_pInstallGACheckBox)
        connect(m_pInstallGACheckBox, &QCheckBox::toggled, this,
                &UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle);
    if (m_pGAISOFilePathSelector)
        connect(m_pGAISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltGAISOPathChanged);

    /* Connections for disk and hardware stuff: */
    if (m_pDiskSkip)
        connect(m_pDiskSkip, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskCreate)
        connect(m_pDiskCreate, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskPresent)
        connect(m_pDiskPresent, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pVMMButton)
        connect(m_pVMMButton, &QIToolButton::clicked,
                this, &UIWizardNewVMPageExpert::sltGetWithFileOpenDialog);
}

void UIWizardNewVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get recommended 'ram' field value: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();
    m_pBaseMemoryEditor->setValue(recommendedRam);

    /* Prepare initial disk choice: */
    if (field("type").value<CGuestOSType>().GetRecommendedHDD() != 0)
    {
        if (m_pDiskCreate)
        {
            m_pDiskCreate->setFocus();
            m_pDiskCreate->setChecked(true);
        }
        m_fRecommendedNoDisk = false;
    }
    else
    {
        if (m_pDiskSkip)
        {
            m_pDiskSkip->setFocus();
            m_pDiskSkip->setChecked(true);
        }
        m_fRecommendedNoDisk = true;
    }
    if (m_pDiskSelector)
        m_pDiskSelector->setCurrentIndex(0);

    if (m_pButtonUnattended)
        disableEnableUnattendedRelatedWidgets(m_pButtonUnattended->isChecked());
    m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    cleanupMachineFolder();
}

void UIWizardNewVMPageExpert::markWidgets() const
{
    UIWizardNewVMPage1::markWidgets();

    if (m_pGAISOFilePathSelector)
        m_pGAISOFilePathSelector->mark(isUnattendedEnabled() && !checkGAISOFile());
}

bool UIWizardNewVMPageExpert::isComplete() const
{
    markWidgets();

    /* Make sure mandatory fields are complete,
     * 'ram' field feats the bounds,
     * 'virtualDisk' field feats the rules: */
    if (!UIWizardPage::isComplete() &&
        (m_pDiskSkip->isChecked() || !m_pDiskPresent->isChecked() || !uiCommon().medium(m_pDiskSelector->id()).isNull()))
        return false;
    /* Check unattended install related stuff: */
    if (isUnattendedEnabled())
    {
        /* Check the installation medium: */
        if (!isISOFileSelectorComplete())
            return false;
        /* Check the GA installation medium: */
        if (!checkGAISOFile())
            return false;
        if (m_pUserNamePasswordEditor)
        {
            if (!m_pUserNamePasswordEditor->isComplete())
                return false;
        }
    }
    return true;
}

bool UIWizardNewVMPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to create machine folder: */
    if (fResult)
        fResult = createMachineFolder();

    /* Try to assign boot virtual-disk: */
    if (fResult)
    {
        /* Ensure there is no virtual-disk created yet: */
        Assert(m_virtualDisk.isNull());
        if (fResult)
        {
            if (m_pDiskCreate->isChecked())
            {
                /* Show the New Virtual Hard Drive wizard if necessary: */
                fResult = getWithNewVirtualDiskWizard();
            }
        }
    }

    /* Try to create VM: */
    if (fResult)
        fResult = qobject_cast<UIWizardNewVM*>(wizard())->createVM();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

bool UIWizardNewVMPageExpert::isProductKeyWidgetEnabled() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !pWizard->isUnattendedInstallEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMPageExpert::disableEnableUnattendedRelatedWidgets(bool fEnabled)
{
    if (m_pISOSelectorLabel)
        m_pISOSelectorLabel->setEnabled(fEnabled);
    if (m_pISOFilePathSelector)
        m_pISOFilePathSelector->setEnabled(fEnabled);
    if (m_pStartHeadlessLabel)
        m_pStartHeadlessLabel->setEnabled(fEnabled);
    if (m_pStartHeadlessCheckBox)
        m_pStartHeadlessCheckBox->setEnabled(fEnabled);
    if (m_pGAInstallContainer)
        m_pGAInstallContainer->setEnabled(fEnabled);
    if (m_pUsernameHostnameContainer)
        m_pUsernameHostnameContainer->setEnabled(fEnabled);
    if (m_pUserNamePasswordEditor)
        m_pUserNamePasswordEditor->setForceUnmark(!fEnabled);
    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}
