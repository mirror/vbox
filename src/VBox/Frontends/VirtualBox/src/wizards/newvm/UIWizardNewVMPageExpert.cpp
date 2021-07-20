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
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIBaseMemoryEditor.h"
#include "UIConverter.h"
#include "UIHostnameDomainNameEditor.h"
#include "UIFilePathSelector.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UIMediumSizeEditor.h"
#include "UIMessageCenter.h"
#include "UINameAndSystemEditor.h"
#include "UIToolBox.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVMEditors.h"
#include "UIWizardNewVMPageExpert.h"
#include "UIWizardNewVMNameOSTypePageBasic.h"


/* COM includes: */
#include "CSystemProperties.h"

UIWizardNewVMPageExpert::UIWizardNewVMPageExpert()
    : m_pToolBox(0)
    , m_pDiskVariantGroupBox(0)
    , m_pFormatButtonGroup(0)
    , m_pSizeAndLocationGroup(0)
    , m_pNameAndSystemEditor(0)
    , m_pSkipUnattendedCheckBox(0)
    , m_pNameAndSystemLayout(0)
    , m_pHardwareWidgetContainer(0)
    , m_pAdditionalOptionsContainer(0)
    , m_pGAInstallationISOContainer(0)
    , m_pDiskSourceButtonGroup(0)
    , m_pDiskEmpty(0)
    , m_pDiskNew(0)
    , m_pDiskExisting(0)
    , m_pDiskSelector(0)
    , m_pDiskSelectionButton(0)
    , m_enmSelectedDiskSource(SelectedDiskSource_New)
    , m_fRecommendedNoDisk(false)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pToolBox = new UIToolBox;
        m_pToolBox->insertPage(ExpertToolboxItems_NameAndOSType, createNameOSTypeWidgets(), "");
        m_pToolBox->insertPage(ExpertToolboxItems_Unattended, createUnattendedWidgets(), "");
        m_pHardwareWidgetContainer = new UINewVMHardwareContainer;
        m_pToolBox->insertPage(ExpertToolboxItems_Hardware, m_pHardwareWidgetContainer, "");
        m_pToolBox->insertPage(ExpertToolboxItems_Disk, createDiskWidgets(), "");
        m_pToolBox->setCurrentPage(ExpertToolboxItems_NameAndOSType);
        pMainLayout->addWidget(m_pToolBox);
        pMainLayout->addStretch();
    }

    createConnections();

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    //qRegisterMetaType<SelectedDiskSource>();

    /* Register fields: */
//     registerField("name*", m_pNameAndSystemEditor, "name", SIGNAL(sigNameChanged(const QString &)));
//     registerField("type", m_pNameAndSystemEditor, "type", SIGNAL(sigOsTypeChanged()));
//     registerField("machineFilePath", this, "machineFilePath");
//     registerField("machineFolder", this, "machineFolder");
//     registerField("machineBaseName", this, "machineBaseName");
//     registerField("baseMemory", this, "baseMemory");
//     registerField("guestOSFamiyId", this, "guestOSFamiyId");
//     registerField("ISOFilePath", this, "ISOFilePath");
//     registerField("isUnattendedEnabled", this, "isUnattendedEnabled");
//     registerField("startHeadless", this, "startHeadless");
//     registerField("detectedOSTypeId", this, "detectedOSTypeId");
//     registerField("userName", this, "userName");
//     registerField("password", this, "password");
//     registerField("hostname", this, "hostname");
//     registerField("installGuestAdditions", this, "installGuestAdditions");
//     registerField("guestAdditionsISOPath", this, "guestAdditionsISOPath");
//     registerField("productKey", this, "productKey");
//     registerField("VCPUCount", this, "VCPUCount");
//     registerField("EFIEnabled", this, "EFIEnabled");
//     registerField("mediumPath", this, "mediumPath");
//     registerField("mediumFormat", this, "mediumFormat");
//     registerField("mediumSize", this, "mediumSize");
//     registerField("selectedDiskSource", this, "selectedDiskSource");
//     registerField("mediumVariant", this, "mediumVariant");

}

void UIWizardNewVMPageExpert::sltNameChanged(const QString &strNewName)
{
    if (!m_userModifiedParameters.contains("GuestOSType"))
        UIWizardNewVMNameOSTypePage::guessOSTypeFromName(m_pNameAndSystemEditor, strNewName);
    UIWizardNewVMNameOSTypePage::composeMachineFilePath(m_pNameAndSystemEditor, qobject_cast<UIWizardNewVM*>(wizard()));
    if (!m_userModifiedParameters.contains("MediumPath"))
        updateVirtualMediumPathFromMachinePathName();
    if (!m_userModifiedParameters.contains("HostnameDomainName"))
        updateHostnameDomainNameFromMachineName();
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltPathChanged(const QString &strNewPath)
{
    Q_UNUSED(strNewPath);
    UIWizardNewVMNameOSTypePage::composeMachineFilePath(m_pNameAndSystemEditor, qobject_cast<UIWizardNewVM*>(wizard()));
    if (!m_userModifiedParameters.contains("MediumPath"))
        updateVirtualMediumPathFromMachinePathName();
}

void UIWizardNewVMPageExpert::sltOsTypeChanged()
{
    m_userModifiedParameters << "GuestOSType";
    if (m_pNameAndSystemEditor)
        newVMWizardPropertySet(GuestOSType, m_pNameAndSystemEditor->type());
    //setOSTypeDependedValues() ??!!!
}

void UIWizardNewVMPageExpert::sltGetWithFileOpenDialog()
{
    //getWithFileOpenDialog();
}

void UIWizardNewVMPageExpert::sltISOPathChanged(const QString &strISOPath)
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);

    UIWizardNewVMNameOSTypePage::determineOSType(strISOPath, pWizard);

    if (!pWizard->detectedOSTypeId().isEmpty() && !m_userModifiedParameters.contains("GuestOSType"))
            UIWizardNewVMNameOSTypePage::guessOSTypeFromName(m_pNameAndSystemEditor, pWizard->detectedOSTypeId());
    newVMWizardPropertySet(ISOFilePath, strISOPath);

    /* Update the global recent ISO path: */
    QFileInfo fileInfo(strISOPath);
    if (fileInfo.exists() && fileInfo.isReadable())
        uiCommon().updateRecentlyUsedMediumListAndFolder(UIMediumDeviceType_DVD, strISOPath);
    setSkipCheckBoxEnable();
    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltGAISOPathChanged(const QString &strPath)
{
    m_userModifiedParameters << "GuestAdditionsISOPath";
    newVMWizardPropertySet(GuestAdditionsISOPath, strPath);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle(bool fEnabled)
{
    newVMWizardPropertySet(InstallGuestAdditions, fEnabled);
    m_userModifiedParameters << "InstallGuestAdditions";
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltOSFamilyTypeChanged(const QString &strGuestOSFamilyType)
{
    if (m_pAdditionalOptionsContainer)
        m_pAdditionalOptionsContainer->disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
    m_userModifiedParameters << "GuestOSFamilyId";
    newVMWizardPropertySet(GuestOSFamilyId, strGuestOSFamilyType);
}

void UIWizardNewVMPageExpert::retranslateUi()
{
    if (m_pSkipUnattendedCheckBox)
    {
        m_pSkipUnattendedCheckBox->setText(UIWizardNewVM::tr("&Skip Unattended Installation"));
        m_pSkipUnattendedCheckBox->setToolTip(UIWizardNewVM::tr("<p>When checked selected ISO file will be mounted to the CD "
                                                                "drive of the virtual machine but the unattended installation "
                                                                "will not start.</p>"));
    }

    if (m_pToolBox)
    {
        m_pToolBox->setPageTitle(ExpertToolboxItems_NameAndOSType, QString(UIWizardNewVM::tr("Name and &Operating System")));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Unattended, UIWizardNewVM::tr("&Unattended Install"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Disk, UIWizardNewVM::tr("Hard Dis&k"));
        m_pToolBox->setPageTitle(ExpertToolboxItems_Hardware, UIWizardNewVM::tr("H&ardware"));
    }

    if (m_pDiskEmpty)
        m_pDiskEmpty->setText(UIWizardNewVM::tr("&Do Not Add a Virtual Hard Disk"));
    if (m_pDiskNew)
        m_pDiskNew->setText(UIWizardNewVM::tr("&Create a Virtual Hard Disk Now"));
    if (m_pDiskExisting)
        m_pDiskExisting->setText(UIWizardNewVM::tr("U&se an Existing Virtual Hard Disk File"));
    if (m_pDiskSelectionButton)
        m_pDiskSelectionButton->setToolTip(UIWizardNewVM::tr("Choose a Virtual Hard Fisk File..."));

    if (m_pNameAndSystemLayout && m_pNameAndSystemEditor)
        m_pNameAndSystemLayout->setColumnMinimumWidth(0, m_pNameAndSystemEditor->firstColumnWidth());
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
        connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigImageChanged,
                this, &UIWizardNewVMPageExpert::sltISOPathChanged);
    }

    if (m_pHardwareWidgetContainer)
    {
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigMemorySizeChanged,
                this, &UIWizardNewVMPageExpert::sltMemorySizeChanged);
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigCPUCountChanged,
                this, &UIWizardNewVMPageExpert::sltCPUCountChanged);
        connect(m_pHardwareWidgetContainer, &UINewVMHardwareContainer::sigEFIEnabledChanged,
                this, &UIWizardNewVMPageExpert::sltEFIEnabledChanged);
    }
    /* Connections for username, password, and hostname, etc: */
    if (m_pGAInstallationISOContainer)
    {
        connect(m_pGAInstallationISOContainer, &UIGAInstallationGroupBox::sigPathChanged,
                this, &UIWizardNewVMPageExpert::sltGAISOPathChanged);
        connect(m_pGAInstallationISOContainer, &UIGAInstallationGroupBox::toggled,
                this, &UIWizardNewVMPageExpert::sltInstallGACheckBoxToggle);

    }

    if (m_pUserNamePasswordGroupBox)
    {
        connect(m_pUserNamePasswordGroupBox, &UIUserNamePasswordGroupBox::sigPasswordChanged,
                this, &UIWizardNewVMPageExpert::sltPasswordChanged);
        connect(m_pUserNamePasswordGroupBox, &UIUserNamePasswordGroupBox::sigUserNameChanged,
                this, &UIWizardNewVMPageExpert::sltUserNameChanged);
    }


    if (m_pAdditionalOptionsContainer)
    {
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigHostnameDomainNameChanged,
                this, &UIWizardNewVMPageExpert::sltHostnameDomainNameChanged);
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigProductKeyChanged,
                this, &UIWizardNewVMPageExpert::sltProductKeyChanged);
        connect(m_pAdditionalOptionsContainer, &UIAdditionalUnattendedOptions::sigStartHeadlessChanged,
                this, &UIWizardNewVMPageExpert::sltStartHeadlessChanged);
    }

    if (m_pDiskSourceButtonGroup)
        connect(m_pDiskSourceButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
                this, &UIWizardNewVMPageExpert::sltSelectedDiskSourceChanged);

    connect(m_pSkipUnattendedCheckBox, &QCheckBox::toggled, this, &UIWizardNewVMPageExpert::sltSkipUnattendedCheckBoxChecked);

    if (m_pSizeAndLocationGroup)
    {
        connect(m_pSizeAndLocationGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
                this, &UIWizardNewVMPageExpert::sltMediumSizeChanged);
        connect(m_pSizeAndLocationGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
                this, &UIWizardNewVMPageExpert::sltMediumPathChanged);

    // /* Virtual disk related connections: */

    // if (m_pDiskSelectionButton)
    //     connect(m_pDiskSelectionButton, &QIToolButton::clicked,
    //             this, &UIWizardNewVMPageExpert::sltGetWithFileOpenDialog);

    // if (m_pDiskSelector)
    //     connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
    //             this, &UIWizardNewVMPageExpert::sltMediaComboBoxIndexChanged);

    }
    // if (m_pFormatButtonGroup)
    //     connect(m_pFormatButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
    //             this, &UIWizardNewVMPageExpert::sltMediumFormatChanged);



    // if (m_pLocationOpenButton)
    //     connect(m_pLocationOpenButton, &QIToolButton::clicked, this, &UIWizardNewVMPageExpert::sltSelectLocationButtonClicked);
}

void UIWizardNewVMPageExpert::setOSTypeDependedValues()
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);

    /* Get recommended 'ram' field value: */
    const CGuestOSType &type = pWizard->guestOSType();
    ULONG recommendedRam = type.GetRecommendedRAM();

    /* Set memory size of the widget and (through signals) wizard: */
    if (m_pHardwareWidgetContainer && !m_userModifiedParameters.contains("MemorySize"))
        m_pHardwareWidgetContainer->setMemorySize(recommendedRam);


    KFirmwareType fwType = type.GetRecommendedFirmware();
    if (m_pHardwareWidgetContainer && !m_userModifiedParameters.contains("EFIEnabled"))
    {
        m_pHardwareWidgetContainer->blockSignals(true);
        m_pHardwareWidgetContainer->setEFIEnabled(fwType != KFirmwareType_BIOS);
        m_pHardwareWidgetContainer->blockSignals(false);
    }
    LONG64 iRecommendedDiskSize = type.GetRecommendedHDD();
    /* Prepare initial disk choice: */
    if (!m_userModifiedParameters.contains("SelectedDiskSource"))
    {
        if (iRecommendedDiskSize != 0)
        {
            if (m_pDiskNew)
                m_pDiskNew->setChecked(true);
            m_enmSelectedDiskSource = SelectedDiskSource_New;
            setEnableDiskSelectionWidgets(false);
            setEnableNewDiskWidgets(true);
            m_fRecommendedNoDisk = false;
        }
        else
        {
            if (m_pDiskEmpty)
                m_pDiskEmpty->setChecked(true);
            m_enmSelectedDiskSource = SelectedDiskSource_Empty;
            setEnableDiskSelectionWidgets(false);
            setEnableNewDiskWidgets(false);
            m_fRecommendedNoDisk = true;
        }
        if (m_pDiskSelector)
            m_pDiskSelector->setCurrentIndex(0);
    }

    if (m_pSizeAndLocationGroup  && !m_userModifiedParameters.contains("MediumSize"))
        m_pSizeAndLocationGroup->setMediumSize(iRecommendedDiskSize);

    // if (m_pProductKeyLabel)
    //     m_pProductKeyLabel->setEnabled(isProductKeyWidgetEnabled());
    // if (m_pProductKeyLineEdit)
    //     m_pProductKeyLineEdit->setEnabled(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::initializePage()
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);
    /* Initialize wizard properties: */
    {
        if (m_pNameAndSystemEditor)
        {
            /* Guest OS type: */
            newVMWizardPropertySet(GuestOSFamilyId, m_pNameAndSystemEditor->familyId());
            newVMWizardPropertySet(GuestOSType, m_pNameAndSystemEditor->type());
            /* Vm name, folder, file path etc. will be initilized by composeMachineFilePath: */
        }

        /* Medium related properties: */
        if (m_pFormatButtonGroup)
            newVMWizardPropertySet(MediumFormat, m_pFormatButtonGroup->mediumFormat());
        if (!m_userModifiedParameters.contains("MediumPath"))
            updateVirtualMediumPathFromMachinePathName();
    }

    /* Initialize user/password if they are not modified by the user: */
    if (m_pUserNamePasswordGroupBox)
    {
        m_pUserNamePasswordGroupBox->blockSignals(true);
        if (!m_userModifiedParameters.contains("UserName"))
            m_pUserNamePasswordGroupBox->setUserName(pWizard->userName());
        if (!m_userModifiedParameters.contains("Password"))
            m_pUserNamePasswordGroupBox->setPassword(pWizard->password());
        m_pUserNamePasswordGroupBox->blockSignals(false);
    }
    if (!m_userModifiedParameters.contains("HostnameDomainName"))
        updateHostnameDomainNameFromMachineName();

    if (m_pGAInstallationISOContainer && !m_userModifiedParameters.contains("InstallGuestAdditions"))
    {
        m_pGAInstallationISOContainer->blockSignals(true);
        m_pGAInstallationISOContainer->setChecked(pWizard->installGuestAdditions());
        m_pGAInstallationISOContainer->blockSignals(false);
    }

    setOSTypeDependedValues();
    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());
    // updateWidgetAterMediumFormatChange();
    // setSkipCheckBoxEnable();
    retranslateUi();
}

void UIWizardNewVMPageExpert::cleanupPage()
{
    //cleanupMachineFolder();
}

void UIWizardNewVMPageExpert::markWidgets() const
{
    if (m_pNameAndSystemEditor)
    {
        m_pNameAndSystemEditor->markNameEditor(m_pNameAndSystemEditor->name().isEmpty());
        m_pNameAndSystemEditor->markImageEditor(!UIWizardNewVMNameOSTypePage::checkISOFile(m_pNameAndSystemEditor),
                                                UIWizardNewVM::tr("Invalid file path or unreadable file"));
    }
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (pWizard && pWizard->installGuestAdditions() && m_pGAInstallationISOContainer)
        m_pGAInstallationISOContainer->mark();
    if (isUnattendedEnabled())
        m_pAdditionalOptionsContainer->mark();
}

QWidget *UIWizardNewVMPageExpert::createUnattendedWidgets()
{
    QWidget *pContainerWidget = new QWidget;
    QGridLayout *pLayout = new QGridLayout(pContainerWidget);
    pLayout->setContentsMargins(0, 0, 0, 0);
    int iRow = 0;
    m_pUserNamePasswordGroupBox = new UIUserNamePasswordGroupBox;
    AssertReturn(m_pUserNamePasswordGroupBox, 0);
    pLayout->addWidget(m_pUserNamePasswordGroupBox, iRow, 0, 1, 2);

    m_pAdditionalOptionsContainer = new UIAdditionalUnattendedOptions;
    AssertReturn(m_pAdditionalOptionsContainer, 0);
    pLayout->addWidget(m_pAdditionalOptionsContainer, iRow, 2, 1, 2);

    ++iRow;

    /* Guest additions installation: */
    m_pGAInstallationISOContainer = new UIGAInstallationGroupBox;
    AssertReturn(m_pGAInstallationISOContainer, 0);
    pLayout->addWidget(m_pGAInstallationISOContainer, iRow, 0, 1, 4);

    return pContainerWidget;
}

QWidget *UIWizardNewVMPageExpert::createNewDiskWidgets()
{
    QWidget *pNewDiskContainerWidget = new QWidget;
    QGridLayout *pDiskContainerLayout = new QGridLayout(pNewDiskContainerWidget);

    m_pSizeAndLocationGroup = new UIMediumSizeAndPathGroupBox;
    pDiskContainerLayout->addWidget(m_pSizeAndLocationGroup, 0, 0, 2, 2);
    m_pFormatButtonGroup = new UIDiskFormatsGroupBox;
    pDiskContainerLayout->addWidget(m_pFormatButtonGroup, 2, 0, 4, 1);
    m_pDiskVariantGroupBox  = new UIDiskVariantGroupBox;
    pDiskContainerLayout->addWidget(m_pDiskVariantGroupBox, 2, 1, 2, 1);

    return pNewDiskContainerWidget;
}

QWidget *UIWizardNewVMPageExpert::createDiskWidgets()
{
    QWidget *pDiskContainer = new QWidget;
    QGridLayout *pDiskLayout = new QGridLayout(pDiskContainer);
    pDiskLayout->setContentsMargins(0, 0, 0, 0);
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
    pDiskLayout->addWidget(m_pDiskNew, 0, 0, 1, 6);
    pDiskLayout->addWidget(createNewDiskWidgets(), 1, 2, 3, 4);
    pDiskLayout->addWidget(m_pDiskExisting, 4, 0, 1, 6);
    pDiskLayout->addWidget(m_pDiskSelector, 5, 2, 1, 3);
    pDiskLayout->addWidget(m_pDiskSelectionButton, 5, 5, 1, 1);
    pDiskLayout->addWidget(m_pDiskEmpty, 6, 0, 1, 6);
    return pDiskContainer;
}

bool UIWizardNewVMPageExpert::isComplete() const
{
    markWidgets();
    bool fIsComplete = true;
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk, QIcon());
    m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Hardware, QIcon());

    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturn(pWizard, false);

    /* Check unattended install related stuff: */
    if (isUnattendedEnabled())
    {
        /* Check the installation medium: */
        if (!UIWizardNewVMNameOSTypePage::checkISOFile(m_pNameAndSystemEditor))
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Invalid path or unreadable ISO file"));
            fIsComplete = false;
        }
        /* Check the GA installation medium: */
        if (m_pGAInstallationISOContainer && !m_pGAInstallationISOContainer->isComplete())
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Invalid path or unreadable ISO file"));

            fIsComplete = false;
        }
        if (m_pUserNamePasswordGroupBox)
        {
            if (!m_pUserNamePasswordGroupBox->isComplete())
            {
                m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                             UIIconPool::iconSet(":/status_error_16px.png"),
                                             UIWizardNewVM::tr("Invalid username and/or password"));
                fIsComplete = false;
            }
        }
        if (m_pAdditionalOptionsContainer)
        {
            if (!m_pAdditionalOptionsContainer->isComplete())
            {
                m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Unattended,
                                             UIIconPool::iconSet(":/status_error_16px.png"),
                                             UIWizardNewVM::tr("Invalid hostname or domain name"));
                fIsComplete = false;
            }
        }
    }

    if (m_pNameAndSystemEditor)
    {
        if (m_pNameAndSystemEditor->name().isEmpty())
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Virtual machine name is invalid"));
            fIsComplete = false;
        }
        if (!UIWizardNewVMNameOSTypePage::checkISOFile(m_pNameAndSystemEditor))
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_NameAndOSType,
                                         UIIconPool::iconSet(":/status_error_16px.png"),
                                         UIWizardNewVM::tr("Invalid ISO file"));
            fIsComplete = false;
        }
    }

    if (m_enmSelectedDiskSource == SelectedDiskSource_New && uiCommon().medium(m_pDiskSelector->id()).isNull())
    {
        m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk,
                                     UIIconPool::iconSet(":/status_error_16px.png"), UIWizardNewVM::tr("No valid disk is selected"));
        fIsComplete = false;
    }

    if (m_enmSelectedDiskSource == SelectedDiskSource_New)
    {

        qulonglong uSize = pWizard->mediumSize();
        if( uSize >= m_uMediumSizeMin && uSize <= m_uMediumSizeMax)
        {
            m_pToolBox->setPageTitleIcon(ExpertToolboxItems_Disk,
                                         UIIconPool::iconSet(":/status_error_16px.png"), UIWizardNewVM::tr("Invalid disk size"));
            fIsComplete = false;
        }
    }


    return fIsComplete;
}

bool UIWizardNewVMPageExpert::validatePage()
{
    bool fResult = true;

    // if (selectedDiskSource() == SelectedDiskSource_New)
    // {
    //     /* Check if the path we will be using for hard drive creation exists: */
    //     const QString strMediumPath(fieldImp("mediumPath").toString());
    //     fResult = !QFileInfo(strMediumPath).exists();
    //     if (!fResult)
    //     {
    //         msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
    //         return fResult;
    //     }
    //     /* Check FAT size limitation of the host hard drive: */
    //     fResult = UIWizardNewVDPageBaseSizeLocation::checkFATSizeLimitation(fieldImp("mediumVariant").toULongLong(),
    //                                                          fieldImp("mediumPath").toString(),
    //                                                          fieldImp("mediumSize").toULongLong());
    //     if (!fResult)
    //     {
    //         msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
    //         return fResult;
    //     }
    // }

    // startProcessing();
    // UIWizardNewVM *pWizard = wizardImp();
    // AssertReturn(pWizard, false);
    // if (selectedDiskSource() == SelectedDiskSource_New)
    // {
    //     /* Try to create the hard drive:*/
    //     fResult = pWizard->createVirtualDisk();
    //     /*Don't show any error message here since UIWizardNewVM::createVirtualDisk already does so: */
    //     if (!fResult)
    //         return fResult;
    // }

    // fResult = pWizard->createVM();
    // /* Try to delete the hard disk: */
    // if (!fResult)
    //     pWizard->deleteVirtualDisk();

    // endProcessing();

    return fResult;
}

bool UIWizardNewVMPageExpert::isProductKeyWidgetEnabled() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    if (!pWizard || !isUnattendedEnabled() || !pWizard->isGuestOSTypeWindows())
        return false;
    return true;
}

void UIWizardNewVMPageExpert::disableEnableUnattendedRelatedWidgets(bool fEnabled)
{
    if (m_pUserNamePasswordGroupBox)
        m_pUserNamePasswordGroupBox->setEnabled(fEnabled);
    if (m_pAdditionalOptionsContainer)
        m_pAdditionalOptionsContainer->setEnabled(fEnabled);
    if (m_pGAInstallationISOContainer)
        m_pGAInstallationISOContainer->setEnabled(fEnabled);
    m_pAdditionalOptionsContainer->disableEnableProductKeyWidgets(isProductKeyWidgetEnabled());
}

void UIWizardNewVMPageExpert::sltSkipUnattendedCheckBoxChecked(bool fSkip)
{
    m_userModifiedParameters << "SkipUnattendedInstall";
    newVMWizardPropertySet(SkipUnattendedInstall, fSkip);
    disableEnableUnattendedRelatedWidgets(isUnattendedEnabled());
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltMediumFormatChanged()
{
    updateWidgetAterMediumFormatChange();
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltMediumSizeChanged(qulonglong uSize)
{
    m_userModifiedParameters << "MediumSize";
    newVMWizardPropertySet(MemorySize, uSize);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltMediumPathChanged(const QString &strPath)
{
    m_userModifiedParameters << "MediumPath";
    newVMWizardPropertySet(MediumPath, strPath);
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltMediaComboBoxIndexChanged()
{
    /* Make sure to set m_virtualDisk: */
    setVirtualDiskFromDiskCombo();
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltSelectedDiskSourceChanged()
{
    if (!m_pDiskSourceButtonGroup)
        return;
    m_userModifiedParameters << "SelectedDiskSource";
    if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskEmpty)
        m_enmSelectedDiskSource = SelectedDiskSource_Empty;
    else if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskExisting)
    {
        m_enmSelectedDiskSource = SelectedDiskSource_Existing;
        setVirtualDiskFromDiskCombo();
    }
    else
        m_enmSelectedDiskSource = SelectedDiskSource_New;

    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    setEnableNewDiskWidgets(m_enmSelectedDiskSource == SelectedDiskSource_New);

    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltSelectLocationButtonClicked()
{
    //onSelectLocationButtonClicked();
}

void UIWizardNewVMPageExpert::sltMemorySizeChanged(int iValue)
{
    newVMWizardPropertySet(MemorySize, iValue);
    m_userModifiedParameters << "MemorySize";
}

void UIWizardNewVMPageExpert::sltCPUCountChanged(int iCount)
{
    newVMWizardPropertySet(CPUCount, iCount);
}

void UIWizardNewVMPageExpert::sltEFIEnabledChanged(bool fEnabled)
{
    newVMWizardPropertySet(EFIEnabled, fEnabled);
    m_userModifiedParameters << "EFIEnabled";
}

void UIWizardNewVMPageExpert::sltPasswordChanged(const QString &strPassword)
{
    newVMWizardPropertySet(Password, strPassword);
    m_userModifiedParameters << "Password";
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltUserNameChanged(const QString &strUserName)
{
    newVMWizardPropertySet(UserName, strUserName);
    m_userModifiedParameters << "UserName";
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltHostnameDomainNameChanged(const QString &strHostnameDomainName)
{
    newVMWizardPropertySet(HostnameDomainName, strHostnameDomainName);
    m_userModifiedParameters << "HostnameDomainName";
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltProductKeyChanged(const QString &strProductKey)
{
    m_userModifiedParameters << "ProductKey";
    newVMWizardPropertySet(ProductKey, strProductKey);
}

void UIWizardNewVMPageExpert::sltStartHeadlessChanged(bool fStartHeadless)
{
    m_userModifiedParameters << "StartHeadless";
    newVMWizardPropertySet(StartHeadless, fStartHeadless);
}


void UIWizardNewVMPageExpert::updateVirtualMediumPathFromMachinePathName()
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);
    QString strDiskFileName = pWizard->machineBaseName().isEmpty() ? QString("NewVirtualDisk1") : pWizard->machineBaseName();
    QString strMediumPath = pWizard->machineFolder();
    if (strMediumPath.isEmpty())
    {
        if (m_pNameAndSystemEditor)
            strMediumPath = m_pNameAndSystemEditor->path();
        else
            strMediumPath = uiCommon().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    }
    QString strExtension = UIWizardNewVMDiskPage::defaultExtension(pWizard->mediumFormat());
    if (m_pSizeAndLocationGroup)
    {
        QString strMediumFilePath =
            UIWizardNewVMDiskPage::absoluteFilePath(UIWizardNewVMDiskPage::toFileName(strDiskFileName,
                                                                                      strExtension), strMediumPath);
        m_pSizeAndLocationGroup->setMediumPath(strMediumFilePath);
    }
}

void UIWizardNewVMPageExpert::updateWidgetAterMediumFormatChange()
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard && m_pDiskVariantGroupBox && m_pSizeAndLocationGroup && m_pFormatButtonGroup);
    const CMediumFormat &comMediumFormat = pWizard->mediumFormat();
    AssertReturnVoid(!comMediumFormat.isNull());
    m_pDiskVariantGroupBox->updateMediumVariantWidgetsAfterFormatChange(comMediumFormat);
    m_pSizeAndLocationGroup->updateMediumPath(comMediumFormat, m_pFormatButtonGroup->formatExtensions());
}

void UIWizardNewVMPageExpert::setEnableNewDiskWidgets(bool fEnable)
{
    if (m_pSizeAndLocationGroup)
        m_pSizeAndLocationGroup->setEnabled(fEnable);
    if (m_pFormatButtonGroup)
        m_pFormatButtonGroup->setEnabled(fEnable);
    if (m_pDiskVariantGroupBox)
        m_pDiskVariantGroupBox->setEnabled(fEnable);
}

void UIWizardNewVMPageExpert::setVirtualDiskFromDiskCombo()
{
    // AssertReturnVoid(m_pDiskSelector);
    // UIWizardNewVM *pWizard = wizardImp();
    // AssertReturnVoid(pWizard);
    // pWizard->setVirtualDisk(m_pDiskSelector->id());
}


QWidget *UIWizardNewVMPageExpert::createNameOSTypeWidgets()
{
    QWidget *pContainerWidget = new QWidget;
    AssertReturn(pContainerWidget, 0);
    m_pNameAndSystemLayout = new QGridLayout(pContainerWidget);
    AssertReturn(m_pNameAndSystemLayout, 0);
    m_pNameAndSystemLayout->setContentsMargins(0, 0, 0, 0);
    m_pNameAndSystemEditor = new UINameAndSystemEditor(0,
                                                       true /* fChooseName? */,
                                                       true /* fChoosePath? */,
                                                       true /* fChooseImage? */,
                                                       true /* fChooseType? */);
    if (m_pNameAndSystemEditor)
        m_pNameAndSystemLayout->addWidget(m_pNameAndSystemEditor, 0, 0, 1, 2);
    m_pSkipUnattendedCheckBox = new QCheckBox;
    if (m_pSkipUnattendedCheckBox)
        m_pNameAndSystemLayout->addWidget(m_pSkipUnattendedCheckBox, 1, 1);
    return pContainerWidget;
}

void UIWizardNewVMPageExpert::setSkipCheckBoxEnable()
{
    if (!m_pSkipUnattendedCheckBox)
        return;
    if (m_pNameAndSystemEditor)
    {
        const QString &strPath = m_pNameAndSystemEditor->image();
        m_pSkipUnattendedCheckBox->setEnabled(!strPath.isNull() && !strPath.isEmpty());
    }
}

void UIWizardNewVMPageExpert::updateHostnameDomainNameFromMachineName()
{
    if (!m_pAdditionalOptionsContainer)
        return;
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);

    m_pAdditionalOptionsContainer->blockSignals(true);
    m_pAdditionalOptionsContainer->setHostname(pWizard->machineBaseName());
    m_pAdditionalOptionsContainer->setDomainName("myguest.virtualbox.org");
    /* Initialize unattended hostname here since we cannot get the default value from CUnattended this early (unlike username etc): */
    newVMWizardPropertySet(HostnameDomainName, m_pAdditionalOptionsContainer->hostnameDomainName());

    m_pAdditionalOptionsContainer->blockSignals(false);
}

bool UIWizardNewVMPageExpert::isUnattendedEnabled() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturn(pWizard, false);
    return pWizard->isUnattendedEnabled();
}

void UIWizardNewVMPageExpert::setEnableDiskSelectionWidgets(bool fEnabled)
{
    if (!m_pDiskSelector || !m_pDiskSelectionButton)
        return;

    m_pDiskSelector->setEnabled(fEnabled);
    m_pDiskSelectionButton->setEnabled(fEnabled);
}
