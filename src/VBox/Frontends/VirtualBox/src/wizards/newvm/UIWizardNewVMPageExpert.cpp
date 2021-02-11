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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemoryEditor.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UINameAndSystemEditor.h"
#include "UIToolBox.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageExpert.h"


UIWizardNewVMPageExpert::UIWizardNewVMPageExpert(const QString &strGroup)
    : UIWizardNewVMPage1(strGroup)
    , m_pToolBox(0)
    , m_pInstallationISOContainer(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pToolBox = new UIToolBox;
        m_pToolBox->insertPage(ExpertToolboxItems_NameAndOSType, createNameOSTypeWidgets(/* fCreateLabels */ false), "");
        m_pToolBox->insertPage(ExpertToolboxItems_Unattended, createUnattendedWidgets(), "", false);
        m_pToolBox->insertPage(ExpertToolboxItems_Disk, createDiskWidgets(), "");
        m_pToolBox->insertPage(ExpertToolboxItems_Hardware, createHardwareWidgets(), "");

        m_pToolBox->setCurrentPage(ExpertToolboxItems_NameAndOSType);
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
    registerField("EFIEnabled", this, "EFIEnabled");

    const QPalette pal = palette();
    QColor tabBackgroundColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(110);
    QColor textColor = pal.color(QPalette::Active, QPalette::Text).lighter();
    QColor disabledTextColor = pal.color(QPalette::Disabled, QPalette::Text).lighter();

    m_pToolBox->setStyleSheet(QString::fromUtf8("QToolBox::tab {\n"
                                                "    background: %1; \n"
                                                "    border-radius: 5px;\n"
                                                "    color: %2;\n"
                                                "}\n"
                                                "\n"
                                                "QToolBox::tab:selected {\n"
                                                "    font: bold;\n"
                                                "}\n"
                                                "\n"
                                                "QToolBox::tab:hover {\n"
                                                "    font: bold;\n"
                                                "}\n"
                                                "QToolBox::tab:disabled {\n"
                                                "    font: italic;\n"
                                                "    color: %3;\n"
                                                "}").arg(tabBackgroundColor.name()).arg(textColor.name()).arg(disabledTextColor.name()));

    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());
}

void UIWizardNewVMPageExpert::sltNameChanged(const QString &strNewText)
{
    /* Call to base-class: */
    onNameChanged(strNewText);

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

    setOSTypeDependedValues();

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

void UIWizardNewVMPageExpert::sltISOPathChanged(const QString &strPath)
{
    determineOSType(strPath);
    setTypeByISODetectedOSType(m_strDetectedOSTypeId);
    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());

    /* Update the global recent ISO path: */
    QFileInfo fileInfo(strPath);
    if (fileInfo.exists() && fileInfo.isReadable())
        uiCommon().updateRecentlyUsedMediumListAndFolder(UIMediumDeviceType_DVD, strPath);

    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGAISOPathChanged(const QString &strPath)
{
    Q_UNUSED(strPath);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltOSFamilyTypeChanged()
{
    disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::retranslateUi()
{
    UIWizardNewVMPage1::retranslateWidgets();
    UIWizardNewVMPage2::retranslateWidgets();
    UIWizardNewVMPage3::retranslateWidgets();
    UIWizardNewVMPage4::retranslateWidgets();

    if (m_pInstallationISOContainer)
        m_pInstallationISOContainer->setTitle(UIWizardNewVM::tr("Installation medium (ISO)"));
    if (m_pToolBox)
    {
        m_pToolBox->setPageTitle(ExpertToolboxItems_NameAndOSType, QString(UIWizardNewVM::tr("Name and operating system")));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Unattended, UIWizardNewVM::tr("Unattended Install"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Disk, UIWizardNewVM::tr("Hard disk"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Hardware, UIWizardNewVM::tr("Hardware"));
    }
}

void UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle(bool fEnabled)
{
    disableEnableGAWidgets(fEnabled);
    emit completeChanged();
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

    if (m_pISOFilePathSelector)
        connect(m_pISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltISOPathChanged);

    /* Connections for username, password, and hostname: */
    if (m_pUserNamePasswordEditor)
        connect(m_pUserNamePasswordEditor, &UIUserNamePasswordEditor::sigSomeTextChanged,
                this, &UIWizardNewVMPageExpert::completeChanged);
    if (m_pGAISOFilePathSelector)
        connect(m_pGAISOFilePathSelector, &UIFilePathSelector::pathChanged,
                this, &UIWizardNewVMPageExpert::sltGAISOPathChanged);

    if (m_pGAInstallCheckBox)
        connect(m_pGAInstallCheckBox, &QCheckBox::toggled,
                this, &UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle);

    /* Connections for disk and hardware stuff: */
    if (m_pDiskSkip)
    {
        connect(m_pDiskSkip, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskSkip, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskCreate)
    {
        connect(m_pDiskCreate, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskCreate, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskPresent)
    {
        connect(m_pDiskPresent, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskPresent, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pVMMButton)
        connect(m_pVMMButton, &QIToolButton::clicked,
                this, &UIWizardNewVMPageExpert::sltGetWithFileOpenDialog);
    if (m_pBaseMemoryEditor)
        connect(m_pBaseMemoryEditor, &UIBaseMemoryEditor::sigValueChanged,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    if (m_pEFICheckBox)
        connect(m_pEFICheckBox, &QCheckBox::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
}

void UIWizardNewVMPageExpert::setOSTypeDependedValues()
{
    if (!field("type").canConvert<CGuestOSType>())
        return;

    /* Get recommended 'ram' field value: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();

    if (m_pBaseMemoryEditor && !m_userSetWidgets.contains(m_pBaseMemoryEditor))
    {
        m_pBaseMemoryEditor->blockSignals(true);
        m_pBaseMemoryEditor->setValue(recommendedRam);
        m_pBaseMemoryEditor->blockSignals(false);
    }

    KFirmwareType fwType = type.GetRecommendedFirmware();
    if (m_pEFICheckBox && !m_userSetWidgets.contains(m_pEFICheckBox))
    {
        m_pEFICheckBox->blockSignals(true);
        m_pEFICheckBox->setChecked(fwType != KFirmwareType_BIOS);
        m_pEFICheckBox->blockSignals(false);
    }

    /* Prepare initial disk choice: */
    if (!m_userSetWidgets.contains(m_pDiskCreate) &&
        !m_userSetWidgets.contains(m_pDiskSkip) &&
        !m_userSetWidgets.contains(m_pDiskPresent))
    {
        if (type.GetRecommendedHDD() != 0)
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
    }

    if (m_pProductKeyLabel)
        m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    if (m_pProductKeyLineEdit)
        m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();
    setOSTypeDependedValues();
    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());
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
    UIWizardNewVMPage2::markWidgets();
}

QWidget *UIWizardNewVMPageExpert::createUnattendedWidgets()
{
    QWidget *pContainerWidget = new QWidget;
    QGridLayout *pLayout = new QGridLayout(pContainerWidget);
    int iRow = 0;

    /* Installation medium selector etc: */
    {
        m_pInstallationISOContainer = new QGroupBox;
        QHBoxLayout *pInstallationISOContainerLayout = new QHBoxLayout(m_pInstallationISOContainer);

        m_pISOFilePathSelector = new UIFilePathSelector;
        if (m_pISOFilePathSelector)
        {
            m_pISOFilePathSelector->setResetEnabled(false);
            m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
            m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
            m_pISOFilePathSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
            pInstallationISOContainerLayout->addWidget(m_pISOFilePathSelector);
        }
        pLayout->addWidget(m_pInstallationISOContainer, iRow++, 0, 1, 4);
    }

    /* Username selector: */
    pLayout->addWidget(createUserNameWidgets(), iRow, 0, 1, 2);

    /* Additional options: */
    pLayout->addWidget(createAdditionalOptionsWidgets(), iRow, 2, 1, 2);

    ++iRow;
    /* Guest additions installation: */
    pLayout->addWidget(createGAInstallWidgets(), iRow, 0, 1, 4);

    return pContainerWidget;
}

bool UIWizardNewVMPageExpert::isComplete() const
{
    markWidgets();
    bool fIsComplete = true;
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Hardware, QIcon());

    if (!UIWizardPage::isComplete())
    {
        m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType,
                                     UIIconPool::iconSet(":/status_error_16px.png"),
                                     UIWizardNewVM::tr("A valid VM name is required"));
        fIsComplete = false;
    }

    if (!m_pDiskSkip->isChecked() && !m_pDiskPresent->isChecked() && uiCommon().medium(m_pDiskSelector->id()).isNull())
    {
        m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk,
                                UIIconPool::iconSet(":/status_error_16px.png"));
        fIsComplete = false;
    }
    /* Check unattended install related stuff: */
    if (isUnattendedEnabled())
    {
        /* Check the installation medium: */
        if (!checkISOFile())
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Invalid path or unreadable ISO file"));
            fIsComplete = false;
        }
        /* Check the GA installation medium: */
        if (m_pGAInstallCheckBox->isChecked() && !checkGAISOFile())
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Invalid path or unreadable ISO file"));

            fIsComplete = false;
        }
        if (m_pUserNamePasswordEditor)
        {
            if (!m_pUserNamePasswordEditor->isComplete())
            {
                m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                             UIIconPool::iconSet(":/status_error_16px.png"),
                                             UIWizardNewVM::tr("Invalid username and password"));
                fIsComplete = false;
            }
        }
    }
    return fIsComplete;
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
    if (!pWizard || !pWizard->isUnattendedEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMPageExpert::disableEnableUnattendedRelatedWidgets(bool fEnabled)
{
    if (m_pUserNameContainer)
        m_pUserNameContainer->setEnabled(fEnabled);
    if (m_pAdditionalOptionsContainer)
        m_pAdditionalOptionsContainer->setEnabled(fEnabled);
    if (m_pGAInstallationISOContainer)
        m_pGAInstallationISOContainer->setEnabled(fEnabled);
    disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
    disableEnableGAWidgets(isGAInstallEnabled());
}

void UIWizardNewVMPageExpert::sltValueModified()
{
    QWidget *pSenderWidget = qobject_cast<QWidget*>(sender());
    if (!pSenderWidget)
        return;

    m_userSetWidgets << pSenderWidget;
}
