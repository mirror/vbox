/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMDiskPageBasic class implementation.
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
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
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
#include "UIMediumSizeEditor.h"
#include "UIMessageCenter.h"
#include "UIWizardNewVD.h"
#include "UIWizardNewVMDiskPageBasic.h"

/* COM includes: */
#include "CGuestOSType.h"
#include "CSystemProperties.h"

QString UIWizardNewVMDiskPage::defaultExtension(const CMediumFormat &mediumFormatRef)
{
    if (!mediumFormatRef.isNull())
    {
        /* Load extension / device list: */
        QVector<QString> fileExtensions;
        QVector<KDeviceType> deviceTypes;
        CMediumFormat mediumFormat(mediumFormatRef);
        mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
        for (int i = 0; i < fileExtensions.size(); ++i)
            if (deviceTypes[i] == KDeviceType_HardDisk)
                return fileExtensions[i].toLower();
    }
    AssertMsgFailed(("Extension can't be NULL!\n"));
    return QString();
}

QString UIWizardNewVMDiskPage::toFileName(const QString &strName, const QString &strExtension)
{
    /* Convert passed name to native separators (it can be full, actually): */
    QString strFileName = QDir::toNativeSeparators(strName);

    /* Remove all trailing dots to avoid multiple dots before extension: */
    int iLen;
    while (iLen = strFileName.length(), iLen > 0 && strFileName[iLen - 1] == '.')
        strFileName.truncate(iLen - 1);

    /* Add passed extension if its not done yet: */
    if (QFileInfo(strFileName).suffix().toLower() != strExtension)
        strFileName += QString(".%1").arg(strExtension);

    /* Return result: */
    return strFileName;
}

QString UIWizardNewVMDiskPage::absoluteFilePath(const QString &strFileName, const QString &strPath)
{
    /* Wrap file-info around received file name: */
    QFileInfo fileInfo(strFileName);
    /* If path-info is relative or there is no path-info at all: */
    if (fileInfo.fileName() == strFileName || fileInfo.isRelative())
    {
        /* Resolve path on the basis of  path we have: */
        fileInfo = QFileInfo(strPath, strFileName);
    }
    /* Return full absolute hard disk file path: */
    return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
}

QUuid UIWizardNewVMDiskPage::getWithFileOpenDialog(const QString &strOSTypeID,
                                                   const QString &strMachineFolder,
                                                   const QString &strMachineBaseName,
                                                   QWidget *pCaller)
{
    QUuid uMediumId;
    int returnCode = uiCommon().openMediumSelectorDialog(pCaller, UIMediumDeviceType_HardDisk,
                                                         uMediumId,
                                                         strMachineFolder,
                                                         strMachineBaseName,
                                                         strOSTypeID,
                                                         false /* don't show/enable the create action: */);
    if (returnCode != static_cast<int>(UIMediumSelector::ReturnCode_Accepted))
        return QUuid();
    return uMediumId;
}

UIWizardNewVMDiskPageBasic::UIWizardNewVMDiskPageBasic()
    : m_pDiskSourceButtonGroup(0)
    , m_pDiskEmpty(0)
    , m_pDiskNew(0)
    , m_pDiskExisting(0)
    , m_pDiskSelector(0)
    , m_pDiskSelectionButton(0)
    , m_pLabel(0)
    , m_pMediumSizeEditorLabel(0)
    , m_pMediumSizeEditor(0)
    , m_pDescriptionLabel(0)
    , m_pDynamicLabel(0)
    , m_pFixedLabel(0)
    , m_pFixedCheckBox(0)
    , m_enmSelectedDiskSource(SelectedDiskSource_New)
    , m_fRecommendedNoDisk(false)
    , m_fVDIFormatFound(false)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
{
    prepare();
}


void UIWizardNewVMDiskPageBasic::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(createDiskWidgets());

    pMainLayout->addStretch();
    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    setEnableNewDiskWidgets(m_enmSelectedDiskSource == SelectedDiskSource_New);

    createConnections();
}

QWidget *UIWizardNewVMDiskPageBasic::createNewDiskWidgets()
{
    QWidget *pWidget = new QWidget;
    if (pWidget)
    {
        QVBoxLayout *pLayout = new QVBoxLayout(pWidget);
        if (pLayout)
        {
            pLayout->setContentsMargins(0, 0, 0, 0);

            /* Prepare size layout: */
            QGridLayout *pSizeLayout = new QGridLayout;
            if (pSizeLayout)
            {
                pSizeLayout->setContentsMargins(0, 0, 0, 0);

                /* Prepare Hard disk size label: */
                m_pMediumSizeEditorLabel = new QLabel(pWidget);
                if (m_pMediumSizeEditorLabel)
                {
                    m_pMediumSizeEditorLabel->setAlignment(Qt::AlignRight);
                    m_pMediumSizeEditorLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                    pSizeLayout->addWidget(m_pMediumSizeEditorLabel, 0, 0, Qt::AlignBottom);
                }
                /* Prepare Hard disk size editor: */
                m_pMediumSizeEditor = new UIMediumSizeEditor(pWidget);
                if (m_pMediumSizeEditor)
                {
                    m_pMediumSizeEditorLabel->setBuddy(m_pMediumSizeEditor);
                    pSizeLayout->addWidget(m_pMediumSizeEditor, 0, 1, 2, 1);
                }
                pLayout->addLayout(pSizeLayout);
            }
            /* Hard disk variant (dynamic vs. fixed) widgets: */
            pLayout->addWidget(createMediumVariantWidgets(false /* bool fWithLabels */));
        }
    }
    return pWidget;
}

void UIWizardNewVMDiskPageBasic::createConnections()
{
    if (m_pDiskSourceButtonGroup)
        connect(m_pDiskSourceButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
                this, &UIWizardNewVMDiskPageBasic::sltSelectedDiskSourceChanged);
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMDiskPageBasic::sltMediaComboBoxIndexChanged);
    if (m_pDiskSelectionButton)
        connect(m_pDiskSelectionButton, &QIToolButton::clicked,
                this, &UIWizardNewVMDiskPageBasic::sltGetWithFileOpenDialog);
    if (m_pMediumSizeEditor)
    {
        connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMDiskPageBasic::completeChanged);
        connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMDiskPageBasic::sltHandleSizeEditorChange);
    }
}

void UIWizardNewVMDiskPageBasic::sltSelectedDiskSourceChanged()
{
    if (!m_pDiskSourceButtonGroup)
        return;

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

void UIWizardNewVMDiskPageBasic::sltMediaComboBoxIndexChanged()
{
    setVirtualDiskFromDiskCombo();
    emit completeChanged();
}

void UIWizardNewVMDiskPageBasic::sltGetWithFileOpenDialog()
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);
    const CGuestOSType &comOSType = pWizard->guestOSType();
    AssertReturnVoid(!comOSType.isNull());
    QUuid uMediumId = UIWizardNewVMDiskPage::getWithFileOpenDialog(comOSType.GetId(),
                                                                   pWizard->machineFolder(),
                                                                   pWizard->machineBaseName(),
                                                                   this);
    if (!uMediumId.isNull())
    {
        m_pDiskSelector->setCurrentItem(uMediumId);
        m_pDiskSelector->setFocus();
    }
}

void UIWizardNewVMDiskPageBasic::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Virtual Hard disk"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>If you wish you can add a virtual hard disk to the new machine. "
                                            "You can either create a new hard disk file or select an existing one. "
                                            "Alternatively you can create a virtual machine without a virtual hard disk.</p>"));

    if (m_pDiskEmpty)
        m_pDiskEmpty->setText(UIWizardNewVM::tr("&Do Not Add a Virtual Hard Disk"));
    if (m_pDiskNew)
        m_pDiskNew->setText(UIWizardNewVM::tr("&Create a Virtual Hard Disk Now"));
    if (m_pDiskExisting)
        m_pDiskExisting->setText(UIWizardNewVM::tr("U&se an Existing Virtual Hard Disk File"));
    if (m_pDiskSelectionButton)
        m_pDiskSelectionButton->setToolTip(UIWizardNewVM::tr("Choose a Virtual Hard Fisk File..."));

    if (m_pMediumSizeEditorLabel)
        m_pMediumSizeEditorLabel->setText(UIWizardNewVD::tr("D&isk Size:"));

    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setText(UIWizardNewVD::tr("Pre-allocate &Full Size"));
        m_pFixedCheckBox->setToolTip(UIWizardNewVD::tr("<p>When checked, the virtual disk image will be fully allocated at "
                                                       "VM creation time, rather than being allocated dynamically at VM run-time.</p>"));
    }

    /* Translate rich text labels: */
    if (m_pDescriptionLabel)
        m_pDescriptionLabel->setText(UIWizardNewVD::tr("Please choose whether the new virtual hard disk file should grow as it is used "
                                                       "(dynamically allocated) or if it should be created at its maximum size (fixed size)."));
    if (m_pDynamicLabel)
        m_pDynamicLabel->setText(UIWizardNewVD::tr("<p>A <b>dynamically allocated</b> hard disk file will only use space "
                                                   "on your physical hard disk as it fills up (up to a maximum <b>fixed size</b>), "
                                                   "although it will not shrink again automatically when space on it is freed.</p>"));
    if (m_pFixedLabel)
        m_pFixedLabel->setText(UIWizardNewVD::tr("<p>A <b>fixed size</b> hard disk file may take longer to create on some "
                                                 "systems but is often faster to use.</p>"));
}

void UIWizardNewVMDiskPageBasic::initializePage()
{
    retranslateUi();

    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);

    LONG64 iRecommendedSize = 0;
    CGuestOSType type = pWizard->guestOSType();
    if (!type.isNull())
    {
        iRecommendedSize = type.GetRecommendedHDD();
        if (iRecommendedSize != 0)
        {
            if (m_pDiskNew)
            {
                m_pDiskNew->setFocus();
                m_pDiskNew->setChecked(true);
            }
            m_enmSelectedDiskSource = SelectedDiskSource_New;
            m_fRecommendedNoDisk = false;
        }
        else
        {
            if (m_pDiskEmpty)
            {
                m_pDiskEmpty->setFocus();
                m_pDiskEmpty->setChecked(true);
            }
            m_enmSelectedDiskSource = SelectedDiskSource_Empty;
            m_fRecommendedNoDisk = true;
        }
    }

    if (m_pDiskSelector)
        m_pDiskSelector->setCurrentIndex(0);
    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    setEnableNewDiskWidgets(m_enmSelectedDiskSource == SelectedDiskSource_New);

    if (!m_fVDIFormatFound)
    {
        /* We do not have any UI elements for HDD format selection since we default to VDI in case of guided wizard mode: */
        CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
        const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
        foreach (const CMediumFormat &format, formats)
        {
            if (format.GetName() == "VDI")
            {
                newVMWizardPropertySet(MediumFormat, format);
                m_fVDIFormatFound = true;
            }
        }
        if (!m_fVDIFormatFound)
            AssertMsgFailed(("No medium format corresponding to VDI could be found!"));
        setWidgetVisibility(pWizard->mediumFormat());
    }
    QString strDefaultExtension =  UIWizardNewVMDiskPage::defaultExtension(pWizard->mediumFormat());

    /* We set the medium name and path according to machine name/path and do not allow user change these in the guided mode: */
    QString strDefaultName = pWizard->machineBaseName().isEmpty() ? QString("NewVirtualDisk1") : pWizard->machineBaseName();
    const QString &strMachineFolder = pWizard->machineFolder();
    QString strMediumPath =
        UIWizardNewVMDiskPage::absoluteFilePath(UIWizardNewVMDiskPage::toFileName(strDefaultName,
                                                                                  strDefaultExtension), strMachineFolder);
    newVMWizardPropertySet(MediumPath, strMediumPath);

    /* Set the recommended disk size if user has already not done so: */
    if (m_pMediumSizeEditor && !m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizeEditor->blockSignals(true);
        m_pMediumSizeEditor->setMediumSize(iRecommendedSize);
        m_pMediumSizeEditor->blockSignals(false);
        newVMWizardPropertySet(MediumSize, iRecommendedSize);
    }
}

void UIWizardNewVMDiskPageBasic::cleanupPage()
{
    //UIWizardPage::cleanupPage();
}

bool UIWizardNewVMDiskPageBasic::isComplete() const
{
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturn(pWizard, false);

    const qulonglong uSize = pWizard->mediumSize();
    if (m_enmSelectedDiskSource == SelectedDiskSource_New)
        return uSize >= m_uMediumSizeMin && uSize <= m_uMediumSizeMax;

    if (m_enmSelectedDiskSource == SelectedDiskSource_Existing)
        return !pWizard->virtualDisk().isNull();

    if (m_pDiskNew && m_pDiskNew->isChecked() && uSize <= 0)
        return false;

    return true;
}

bool UIWizardNewVMDiskPageBasic::validatePage()
{
    bool fResult = true;
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturn(pWizard, false);

    /* Make sure user really intents to creae a vm with no hard drive: */
    if (m_enmSelectedDiskSource == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        if (!m_fRecommendedNoDisk)
        {
            if (!msgCenter().confirmHardDisklessMachine(this))
                return false;
        }
    }
    else if (m_enmSelectedDiskSource == SelectedDiskSource_New)
    {
        /* Check if the path we will be using for hard drive creation exists: */
        const QString &strMediumPath = pWizard->mediumPath();
        fResult = !QFileInfo(strMediumPath).exists();
        if (!fResult)
        {
            msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
            return fResult;
        }
        /* Check FAT size limitation of the host hard drive: */
        fResult = UIWizardNewVDPageBaseSizeLocation::checkFATSizeLimitation(pWizard->mediumVariant(),
                                                                            pWizard->mediumPath(),
                                                                            pWizard->mediumSize());
        if (!fResult)
        {
            msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
            return fResult;
        }
    }

    // startProcessing();
    if (pWizard)
    {
            if (m_enmSelectedDiskSource == SelectedDiskSource_New)
            {
                /* Try to create the hard drive:*/
                fResult = pWizard->createVirtualDisk();
                /*Don't show any error message here since UIWizardNewVM::createVirtualDisk already does so: */
                if (!fResult)
                    return fResult;
            }

            fResult = pWizard->createVM();
            /* Try to delete the hard disk: */
            if (!fResult)
                pWizard->deleteVirtualDisk();
    }
    // endProcessing();

    return fResult;
}

void UIWizardNewVMDiskPageBasic::sltHandleSizeEditorChange()
{
    m_userModifiedParameters << "MediumSize";
}

void UIWizardNewVMDiskPageBasic::setEnableNewDiskWidgets(bool fEnable)
{
    if (m_pMediumSizeEditor)
        m_pMediumSizeEditor->setEnabled(fEnable);
    if (m_pMediumSizeEditorLabel)
        m_pMediumSizeEditorLabel->setEnabled(fEnable);
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setEnabled(fEnable);
}

void UIWizardNewVMDiskPageBasic::setVirtualDiskFromDiskCombo()
{
    AssertReturnVoid(m_pDiskSelector);
    UIWizardNewVM *pWizard = qobject_cast<UIWizardNewVM*>(wizard());
    AssertReturnVoid(pWizard);
    pWizard->setVirtualDisk(m_pDiskSelector->id());
}

QWidget *UIWizardNewVMDiskPageBasic::createDiskWidgets()
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

QWidget *UIWizardNewVMDiskPageBasic::createMediumVariantWidgets(bool fWithLabels)
{
    QWidget *pContainerWidget = new QWidget;
    QVBoxLayout *pMainLayout = new QVBoxLayout(pContainerWidget);
    if (pMainLayout)
    {
        if (fWithLabels)
        {
            m_pDescriptionLabel = new QIRichTextLabel;
            m_pDynamicLabel = new QIRichTextLabel;
            m_pFixedLabel = new QIRichTextLabel;
        }
        QVBoxLayout *pVariantLayout = new QVBoxLayout;
        if (pVariantLayout)
        {
            m_pFixedCheckBox = new QCheckBox;
            pVariantLayout->addWidget(m_pFixedCheckBox);
        }
        if (fWithLabels)
        {
            pMainLayout->addWidget(m_pDescriptionLabel);
            pMainLayout->addWidget(m_pDynamicLabel);
            pMainLayout->addWidget(m_pFixedLabel);
        }
        pMainLayout->addLayout(pVariantLayout);
        pMainLayout->addStretch();
        pMainLayout->setContentsMargins(0, 0, 0, 0);
    }
    return pContainerWidget;
}

void UIWizardNewVMDiskPageBasic::setEnableDiskSelectionWidgets(bool fEnabled)
{
    if (!m_pDiskSelector || !m_pDiskSelectionButton)
        return;

    m_pDiskSelector->setEnabled(fEnabled);
    m_pDiskSelectionButton->setEnabled(fEnabled);
}

void UIWizardNewVMDiskPageBasic::setWidgetVisibility(const CMediumFormat &mediumFormat)
{
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    if (m_pFixedCheckBox)
    {
        if (!fIsCreateDynamicPossible)
        {
            m_pFixedCheckBox->setChecked(true);
            m_pFixedCheckBox->setEnabled(false);
        }
        if (!fIsCreateFixedPossible)
        {
            m_pFixedCheckBox->setChecked(false);
            m_pFixedCheckBox->setEnabled(false);
        }
    }
    if (m_pDynamicLabel)
        m_pDynamicLabel->setHidden(!fIsCreateDynamicPossible);
    if (m_pFixedLabel)
        m_pFixedLabel->setHidden(!fIsCreateFixedPossible);
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setHidden(!fIsCreateFixedPossible);
}
