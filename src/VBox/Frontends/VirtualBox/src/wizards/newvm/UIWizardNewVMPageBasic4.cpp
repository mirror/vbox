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
#include <QCheckBox>
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
#include "UIWizardNewVMPageBasic4.h"

/* COM includes: */
#include "CGuestOSType.h"
#include "CSystemProperties.h"

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
    QUuid uMediumId;
    int returnCode = uiCommon().openMediumSelectorDialog(thisImp(), UIMediumDeviceType_HardDisk,
                                                           uMediumId,
                                                           fieldImp("machineFolder").toString(),
                                                           fieldImp("machineBaseName").toString(),
                                                           fieldImp("type").value<CGuestOSType>().GetId(),
                                                           false /* don't show/enable the create action: */);
    if (returnCode == static_cast<int>(UIMediumSelector::ReturnCode_Accepted) && !uMediumId.isNull())
    {
        m_pDiskSelector->setCurrentItem(uMediumId);
        m_pDiskSelector->setFocus();
    }
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

QWidget *UIWizardNewVMPage4::createNewDiskWidgets()
{
    return new QWidget();
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
    pDiskLayout->addWidget(m_pDiskNew, 0, 0, 1, 6);
    pDiskLayout->addWidget(createNewDiskWidgets(), 1, 2, 3, 4);
    pDiskLayout->addWidget(m_pDiskExisting, 4, 0, 1, 6);
    pDiskLayout->addWidget(m_pDiskSelector, 5, 2, 1, 3);
    pDiskLayout->addWidget(m_pDiskSelectionButton, 5, 5, 1, 1);
    pDiskLayout->addWidget(m_pDiskEmpty, 6, 0, 1, 6);
    return pDiskContainer;
}

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
    : m_pLabel(0)
    , m_fUserSetSize(false)

{
    prepare();
    qRegisterMetaType<CMedium>();
    qRegisterMetaType<SelectedDiskSource>();
    registerField("selectedDiskSource", this, "selectedDiskSource");

    registerField("mediumFormat", this, "mediumFormat");
    registerField("mediumVariant" /* KMediumVariant */, this, "mediumVariant");
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");

    /* We do not have any UI elements for HDD format selection since we default to VDI in case of guided wizard mode: */
    bool fFoundVDI = false;
    CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
    foreach (const CMediumFormat &format, formats)
    {
        if (format.GetName() == "VDI")
        {
            m_mediumFormat = format;
            fFoundVDI = true;
        }
    }
    if (!fFoundVDI)
        AssertMsgFailed(("No medium format corresponding to VDI could be found!"));

    m_strDefaultExtension =  defaultExtension(m_mediumFormat);

    /* Since the medium format is static we can decide widget visibility here: */
    setWidgetVisibility(m_mediumFormat);
}

CMediumFormat UIWizardNewVMPageBasic4::mediumFormat() const
{
    return m_mediumFormat;
}

QString UIWizardNewVMPageBasic4::mediumPath() const
{
    return absoluteFilePath(toFileName(m_strDefaultName, m_strDefaultExtension), m_strDefaultPath);
}

void UIWizardNewVMPageBasic4::prepare()
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

QWidget *UIWizardNewVMPageBasic4::createNewDiskWidgets()
{
    QWidget *pWidget = new QWidget;
    QVBoxLayout *pLayout = new QVBoxLayout(pWidget);
    pLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *pSizeLayout = new QHBoxLayout;
    pSizeLayout->setContentsMargins(0, 0, 0, 0);
    /* Hard disk size relate widgets: */
    m_pSizeEditor = new UIMediumSizeEditor;
    m_pSizeEditorLabel = new QLabel;
    if (m_pSizeEditorLabel)
    {
        pSizeLayout->addWidget(m_pSizeEditorLabel);
        m_pSizeEditorLabel->setBuddy(m_pSizeEditor);
    }
    pSizeLayout->addWidget(m_pSizeEditor);
    pLayout->addLayout(pSizeLayout);
    /* Hard disk variant (dynamic vs. fixed) widgets: */
    pLayout->addWidget(createMediumVariantWidgets(false /* bool fWithLabels */));

    return pWidget;
}

void UIWizardNewVMPageBasic4::createConnections()
{
    if (m_pDiskSourceButtonGroup)
        connect(m_pDiskSourceButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
                this, &UIWizardNewVMPageBasic4::sltSelectedDiskSourceChanged);
    if (m_pDiskSelector)
        connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
                this, &UIWizardNewVMPageBasic4::sltMediaComboBoxIndexChanged);
    if (m_pDiskSelectionButton)
        connect(m_pDiskSelectionButton, &QIToolButton::clicked,
                this, &UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog);
    if (m_pSizeEditor)
    {
        connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMPageBasic4::completeChanged);
        connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMPageBasic4::sltHandleSizeEditorChange);
    }
}

void UIWizardNewVMPageBasic4::sltSelectedDiskSourceChanged()
{
    if (!m_pDiskSourceButtonGroup)
        return;

    if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskEmpty)
        setSelectedDiskSource(SelectedDiskSource_Empty);
    else if (m_pDiskSourceButtonGroup->checkedButton() == m_pDiskExisting)
    {
        setSelectedDiskSource(SelectedDiskSource_Existing);
        setVirtualDiskFromDiskCombo();
    }
    else
        setSelectedDiskSource(SelectedDiskSource_New);

    setEnableDiskSelectionWidgets(m_enmSelectedDiskSource == SelectedDiskSource_Existing);
    setEnableNewDiskWidgets(m_enmSelectedDiskSource == SelectedDiskSource_New);

    completeChanged();
}

void UIWizardNewVMPageBasic4::sltMediaComboBoxIndexChanged()
{
    /* Make sure to set m_virtualDisk: */
    setVirtualDiskFromDiskCombo();
    emit completeChanged();
}

void UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog()
{
    getWithFileOpenDialog();
}

void UIWizardNewVMPageBasic4::retranslateUi()
{
    setTitle(UIWizardNewVM::tr("Virtual Hard disk"));

    QString strRecommendedHDD = field("type").value<CGuestOSType>().isNull() ? QString() :
                                UICommon::formatSize(field("type").value<CGuestOSType>().GetRecommendedHDD());
    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("<p>If you wish you can add a virtual hard disk to the new machine. "
                                            "You can either create a new hard disk file or select an existing one. "
                                            "Alternatively you can create a virtual machine without a virtual hard disk.</p>"));

    UIWizardNewVMPage4::retranslateWidgets();
    UIWizardNewVDPage1::retranslateWidgets();
    UIWizardNewVDPage2::retranslateWidgets();
    UIWizardNewVDPage3::retranslateWidgets();
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

    /* We set the medium name and path according to machine name/path and do let user change these in the guided mode: */
    QString strDefaultName = fieldImp("machineBaseName").toString();
    m_strDefaultName = strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName;
    m_strDefaultPath = fieldImp("machineFolder").toString();
    /* Set the recommended disk size if user has already not done so: */
    if (m_pSizeEditor && !m_fUserSetSize)
    {
        m_pSizeEditor->blockSignals(true);
        setMediumSize(fieldImp("type").value<CGuestOSType>().GetRecommendedHDD());
        m_pSizeEditor->blockSignals(false);
    }
}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    if (selectedDiskSource() == SelectedDiskSource_New)
        return mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
    UIWizardNewVM *pWizard = wizardImp();
    AssertReturn(pWizard, false);
    if (selectedDiskSource() == SelectedDiskSource_Existing)
        return !pWizard->virtualDisk().isNull();

    return true;
}

bool UIWizardNewVMPageBasic4::validatePage()
{
    bool fResult = true;

    /* Make sure user really intents to creae a vm with no hard drive: */
    if (selectedDiskSource() == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        if (!m_fRecommendedNoDisk)
        {
            if (!msgCenter().confirmHardDisklessMachine(thisImp()))
                return false;
        }
    }
    else if (selectedDiskSource() == SelectedDiskSource_New)
    {
        /* Check if the path we will be using for hard drive creation exists: */
        const QString strMediumPath(fieldImp("mediumPath").toString());
        fResult = !QFileInfo(strMediumPath).exists();
        if (!fResult)
        {
            msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
            return fResult;
        }
        /* Check FAT size limitation of the host hard drive: */
        fResult = UIWizardNewVDPage3::checkFATSizeLimitation(fieldImp("mediumVariant").toULongLong(),
                                                             fieldImp("mediumPath").toString(),
                                                             fieldImp("mediumSize").toULongLong());
        if (!fResult)
        {
            msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
            return fResult;
        }
    }

    startProcessing();
    UIWizardNewVM *pWizard = wizardImp();
    if (pWizard)
    {
        if (selectedDiskSource() == SelectedDiskSource_New)
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
    endProcessing();

    return fResult;
}

void UIWizardNewVMPageBasic4::sltHandleSizeEditorChange()
{
    m_fUserSetSize = true;
}

void UIWizardNewVMPageBasic4::setEnableNewDiskWidgets(bool fEnable)
{
    if (m_pSizeEditor)
        m_pSizeEditor->setEnabled(fEnable);
    if (m_pSizeEditorLabel)
        m_pSizeEditorLabel->setEnabled(fEnable);
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setEnabled(fEnable);
}

void UIWizardNewVMPageBasic4::setVirtualDiskFromDiskCombo()
{
    AssertReturnVoid(m_pDiskSelector);
    UIWizardNewVM *pWizard = wizardImp();
    AssertReturnVoid(pWizard);
    pWizard->setVirtualDisk(m_pDiskSelector->id());
}
