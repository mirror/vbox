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
#include <QGroupBox>
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
    , m_pMediumVariantContainer(0)
    , m_pSizeContainer(0)
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
    if (m_pMediumVariantContainer)
        m_pMediumVariantContainer->setTitle(UIWizardNewVM::tr("Storage on physical hard disk"));
    if (m_pSizeContainer)
        m_pSizeContainer->setTitle(UIWizardNewVM::tr("File size"));
}

void UIWizardNewVMPage4::setEnableDiskSelectionWidgets(bool fEnabled)
{
    if (!m_pDiskSelector || !m_pDiskSelectionButton)
        return;

    m_pDiskSelector->setEnabled(fEnabled);
    m_pDiskSelectionButton->setEnabled(fEnabled);
}

QWidget *UIWizardNewVMPage4::createDiskVariantAndSizeWidgets()
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
    pDiskLayout->addWidget(m_pDiskEmpty, 0, 0, 1, 4);
    pDiskLayout->addWidget(m_pDiskNew, 1, 0, 1, 4);
    pDiskLayout->addWidget(createDiskVariantAndSizeWidgets(), 2, 1, 3, 3);
    pDiskLayout->addWidget(m_pDiskExisting, 5, 0, 1, 4);
    pDiskLayout->addWidget(m_pDiskSelector, 6, 1, 1, 2);
    pDiskLayout->addWidget(m_pDiskSelectionButton, 6, 3, 1, 1);
    return pDiskContainer;
}

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
    : m_pLabel(0)
    , m_fUserSetSize(false)

{
    prepare();
    qRegisterMetaType<CMedium>();
    qRegisterMetaType<SelectedDiskSource>();
    registerField("virtualDisk", this, "virtualDisk");
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

QWidget *UIWizardNewVMPageBasic4::createDiskVariantAndSizeWidgets()
{

    QWidget *pWidget = new QWidget;
    QHBoxLayout *pLayout = new QHBoxLayout(pWidget);
    pLayout->setContentsMargins(0, 0, 0, 0);

    m_pMediumVariantContainer = new QGroupBox;
    QVBoxLayout *pMediumVariantLayout = new QVBoxLayout(m_pMediumVariantContainer);
    pMediumVariantLayout->addWidget(createMediumVariantWidgets(false /* no labels */));
    pLayout->addWidget(m_pMediumVariantContainer);

    m_pSizeContainer = new QGroupBox;
    QVBoxLayout *pSizeLayout = new QVBoxLayout(m_pSizeContainer);
    m_pSizeLabel = new QIRichTextLabel;
    m_pSizeEditor = new UIMediumSizeEditor;
    pSizeLayout->addWidget(m_pSizeLabel);
    pSizeLayout->addWidget(m_pSizeEditor);
    pSizeLayout->addStretch();
    pLayout->addWidget(m_pSizeContainer);

    return pWidget;
}

void UIWizardNewVMPageBasic4::createConnections()
{
    connect(m_pDiskSourceButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
            this, &UIWizardNewVMPageBasic4::sltHandleSelectedDiskSourceChange);
    connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
            this, &UIWizardNewVMPageBasic4::sltVirtualSelectedDiskSourceChanged);
    connect(m_pDiskSelectionButton, &QIToolButton::clicked,
            this, &UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog);
    connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
            this, &UIWizardNewVMPageBasic4::completeChanged);
    connect(m_pSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
            this, &UIWizardNewVMPageBasic4::sltHandleSizeEditorChange);
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
    setEnableNewDiskWidgets(m_enmSelectedDiskSource == SelectedDiskSource_New);

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
    if (m_pSizeEditor && !m_fUserSetSize)
    {
        m_pSizeEditor->blockSignals(true);
        setMediumSize(fieldImp("type").value<CGuestOSType>().GetRecommendedHDD());
        m_pSizeEditor->blockSignals(false);
    }

}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    if (selectedDiskSource() == SelectedDiskSource_New)
        return mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
    if (selectedDiskSource() == SelectedDiskSource_Existing)
        return m_virtualDisk.isNull();
    return true;
}

bool UIWizardNewVMPageBasic4::validatePage()
{
    bool fResult = true;
    if (selectedDiskSource() == SelectedDiskSource_Empty)
    {
        /* Ask user about disk-less machine unless that's the recommendation: */
        if (!m_fRecommendedNoDisk)
            fResult = msgCenter().confirmHardDisklessMachine(thisImp());
    }
    return fResult;
}

void UIWizardNewVMPageBasic4::sltHandleSizeEditorChange()
{
    m_fUserSetSize = true;
}

void UIWizardNewVMPageBasic4::setEnableNewDiskWidgets(bool fEnable)
{
    if (m_pMediumVariantContainer)
        m_pMediumVariantContainer->setEnabled(fEnable);

    if (m_pSizeContainer)
        m_pSizeContainer->setEnabled(fEnable);
}
