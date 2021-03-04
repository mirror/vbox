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
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemoryEditor.h"
#include "UIConverter.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UIMediumSizeEditor.h"
#include "UINameAndSystemEditor.h"
#include "UIToolBox.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageExpert.h"


UIWizardNewVMPageExpert::UIWizardNewVMPageExpert(const QString &strGroup)
    : UIWizardNewVMPage1(strGroup)
    , m_pToolBox(0)
    , m_pInstallationISOGroupBox(0)
    , m_pDiskFormatGroupBox(0)
    , m_pDiskVariantGroupBox(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pToolBox = new UIToolBox;
        m_pToolBox->insertPage(ExpertToolboxItems_NameAndOSType, createNameOSTypeWidgets(/* fCreateLabels */ false), "");
        m_pToolBox->insertPage(ExpertToolboxItems_Unattended, createUnattendedWidgets(), "", false);
        m_pToolBox->insertPage(ExpertToolboxItems_Hardware, createHardwareWidgets(), "");
        m_pToolBox->insertPage(ExpertToolboxItems_Disk, createDiskWidgets(), "");

        m_pToolBox->setCurrentPage(ExpertToolboxItems_NameAndOSType);
        pMainLayout->addWidget(m_pToolBox);

        pMainLayout->addStretch();
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
    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGetWithFileOpenDialog()
{
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
    UIWizardNewVDPage1::retranslateWidgets();
    UIWizardNewVDPage2::retranslateWidgets();
    UIWizardNewVDPage3::retranslateWidgets();

    if (m_pInstallationISOGroupBox)
        m_pInstallationISOGroupBox->setTitle(UIWizardNewVM::tr("Installation medium (ISO)"));
    if (m_pToolBox)
    {
        m_pToolBox->setPageTitle(ExpertToolboxItems_NameAndOSType, QString(UIWizardNewVM::tr("Name and operating system")));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Unattended, UIWizardNewVM::tr("Unattended Install"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Disk, UIWizardNewVM::tr("Hard disk"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Hardware, UIWizardNewVM::tr("Hardware"));
    }

    if (m_pDiskFormatGroupBox)
        m_pDiskFormatGroupBox->setTitle(UIWizardNewVM::tr("Hard disk file &type"));
    if (m_pFormatButtonGroup)
    {
        QList<QAbstractButton*> buttons = m_pFormatButtonGroup->buttons();
        for (int i = 0; i < buttons.size(); ++i)
        {
            QAbstractButton *pButton = buttons[i];
            UIMediumFormat enmFormat = gpConverter->fromInternalString<UIMediumFormat>(m_formatNames[m_pFormatButtonGroup->id(pButton)]);
            pButton->setText(gpConverter->toString(enmFormat));
        }
    }
    if (m_pDiskVariantGroupBox)
        m_pDiskVariantGroupBox->setTitle(UIWizardNewVM::tr("Storage on physical hard disk"));

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

    if (m_pDiskEmpty)
    {
        connect(m_pDiskEmpty, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskEmpty, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskNew)
    {
        connect(m_pDiskNew, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskNew, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskExisting)
    {
        connect(m_pDiskExisting, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
        connect(m_pDiskExisting, &QRadioButton::toggled,
                this, &UIWizardNewVMPageExpert::sltValueModified);
    }
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    if (m_pDiskSelectionButton)
        connect(m_pDiskSelectionButton, &QIToolButton::clicked,
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
    if (!m_userSetWidgets.contains(m_pDiskNew) &&
        !m_userSetWidgets.contains(m_pDiskEmpty) &&
        !m_userSetWidgets.contains(m_pDiskExisting))
    {
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
        m_pInstallationISOGroupBox = new QGroupBox;
        QHBoxLayout *pInstallationISOContainerLayout = new QHBoxLayout(m_pInstallationISOGroupBox);

        m_pISOFilePathSelector = new UIFilePathSelector;
        if (m_pISOFilePathSelector)
        {
            m_pISOFilePathSelector->setResetEnabled(false);
            m_pISOFilePathSelector->setMode(UIFilePathSelector::Mode_File_Open);
            m_pISOFilePathSelector->setFileDialogFilters("*.iso *.ISO");
            m_pISOFilePathSelector->setInitialPath(uiCommon().defaultFolderPathForType(UIMediumDeviceType_DVD));
            pInstallationISOContainerLayout->addWidget(m_pISOFilePathSelector);
        }
        pLayout->addWidget(m_pInstallationISOGroupBox, iRow++, 0, 1, 4);
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

QWidget *UIWizardNewVMPageExpert::createNewDiskWidgets()
{
    QWidget *pNewDiskContainerWidget = new QWidget;
    QGridLayout *pDiskContainerLayout = new QGridLayout(pNewDiskContainerWidget);

    /* Disk location widgets: */

    QLabel *pLocationLabel = new QLabel("Disk Location:");
    pLocationLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_pLocationEditor = new QLineEdit;
    m_pLocationOpenButton = new QIToolButton;
    if (m_pLocationOpenButton)
    {
        m_pLocationOpenButton->setAutoRaise(true);
        m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
    }
    pLocationLabel->setBuddy(m_pLocationEditor);

    /* Disk file size widgets: */
    QLabel *pSizeEditorLabel = new QLabel("Disk Size:");
    pSizeEditorLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_pSizeEditor = new UIMediumSizeEditor;
    pSizeEditorLabel->setBuddy(m_pSizeEditor);

    /* Disk file format widgets: */
    m_pDiskFormatGroupBox = new QGroupBox;
    QHBoxLayout *pDiskFormatLayout = new QHBoxLayout(m_pDiskFormatGroupBox);
    m_pSizeEditor = new UIMediumSizeEditor;
    pDiskFormatLayout->addWidget(createFormatButtonGroup(true));

    /* Disk variant and dik split widgets: */
    m_pDiskVariantGroupBox  = new QGroupBox;
    QVBoxLayout *pDiskVariantLayout = new QVBoxLayout(m_pDiskVariantGroupBox);
    pDiskVariantLayout->addWidget(createMediumVariantWidgets(false /* fWithLabels */));

    pDiskContainerLayout->addWidget(pLocationLabel, 0, 0, 1, 1);
    pDiskContainerLayout->addWidget(m_pLocationEditor, 0, 1, 1, 2);
    pDiskContainerLayout->addWidget(m_pLocationOpenButton, 0, 3, 1, 1);

    pDiskContainerLayout->addWidget(pSizeEditorLabel, 1, 0, 1, 1);
    pDiskContainerLayout->addWidget(m_pSizeEditor, 1, 1, 1, 3);

    pDiskContainerLayout->addWidget(m_pDiskFormatGroupBox, 2, 0, 1, 2);
    pDiskContainerLayout->addWidget(m_pDiskVariantGroupBox, 2, 2, 1, 2);

    return pNewDiskContainerWidget;
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

    if (m_pDiskExisting && m_pDiskExisting->isChecked() && uiCommon().medium(m_pDiskSelector->id()).isNull())
    {
        m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk,
                                     UIIconPool::iconSet(":/status_error_16px.png"), UIWizardNewVM::tr("No valid disk is selected"));
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
                                             UIWizardNewVM::tr("Invalid username and/or password"));
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
            if (m_pDiskNew->isChecked())
            {
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
