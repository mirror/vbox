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
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIToolButton.h"
#include "UIBaseMemorySlider.h"
#include "UIBaseMemoryEditor.h"
#include "UIIconPool.h"
#include "UIMediaComboBox.h"
#include "UIMedium.h"
#include "UINameAndSystemEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMPageExpert.h"


UIWizardNewVMPageExpert::UIWizardNewVMPageExpert(const QString &strGroup)
    : UIWizardNewVMPageNameType(strGroup)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pNameAndSystemCnt = new QGroupBox(this);
        {
            m_pNameAndSystemCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pNameAndSystemCntLayout = new QHBoxLayout(m_pNameAndSystemCnt);
            {
                m_pNameAndSystemEditor = new UINameAndSystemEditor(m_pNameAndSystemCnt, true, true, true);
                pNameAndSystemCntLayout->addWidget(m_pNameAndSystemEditor);
            }
        }
        m_pMemoryCnt = new QGroupBox(this);
        {
            m_pMemoryCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QGridLayout *pMemoryCntLayout = new QGridLayout(m_pMemoryCnt);
            {
                m_pBaseMemoryEditor = new UIBaseMemoryEditor;
                pMemoryCntLayout->addWidget(m_pBaseMemoryEditor, 0, 0, 1, 4);
            }
        }
        m_pDiskCnt = new QGroupBox(this);
        {
            m_pDiskCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QGridLayout *pDiskCntLayout = new QGridLayout(m_pDiskCnt);
            {
                m_pDiskSkip = new QRadioButton(m_pDiskCnt);
                m_pDiskCreate = new QRadioButton(m_pDiskCnt);
                {
                    m_pDiskCreate->setChecked(true);
                }
                m_pDiskPresent = new QRadioButton(m_pDiskCnt);
                QStyleOptionButton options;
                options.initFrom(m_pDiskPresent);
                int iWidth = m_pDiskPresent->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &options, m_pDiskPresent);
                pDiskCntLayout->setColumnMinimumWidth(0, iWidth);
                m_pDiskSelector = new UIMediaComboBox(m_pDiskCnt);
                {
                    m_pDiskSelector->setType(UIMediumDeviceType_HardDisk);
                    m_pDiskSelector->repopulate();
                }
                m_pVMMButton = new QIToolButton(m_pDiskCnt);
                {
                    m_pVMMButton->setAutoRaise(true);
                    m_pVMMButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_disabled_16px.png"));
                }
                pDiskCntLayout->addWidget(m_pDiskSkip, 0, 0, 1, 3);
                pDiskCntLayout->addWidget(m_pDiskCreate, 1, 0, 1, 3);
                pDiskCntLayout->addWidget(m_pDiskPresent, 2, 0, 1, 3);
                pDiskCntLayout->addWidget(m_pDiskSelector, 3, 1);
                pDiskCntLayout->addWidget(m_pVMMButton, 3, 2);
            }
        }
        pMainLayout->addWidget(m_pNameAndSystemCnt);
        pMainLayout->addWidget(m_pMemoryCnt);
        pMainLayout->addWidget(m_pDiskCnt);
        pMainLayout->addStretch();
        updateVirtualDiskSource();
    }

    /* Setup connections: */
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigNameChanged,
            this, &UIWizardNewVMPageExpert::sltNameChanged);
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigPathChanged,
            this, &UIWizardNewVMPageExpert::sltPathChanged);
    connect(m_pNameAndSystemEditor, &UINameAndSystemEditor::sigOsTypeChanged,
            this, &UIWizardNewVMPageExpert::sltOsTypeChanged);
    connect(m_pDiskSkip, &QRadioButton::toggled,
            this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    connect(m_pDiskCreate, &QRadioButton::toggled,
            this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    connect(m_pDiskPresent, &QRadioButton::toggled,
            this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    connect(m_pDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
            this, &UIWizardNewVMPageExpert::sltVirtualDiskSourceChanged);
    connect(m_pVMMButton, &QIToolButton::clicked,
            this, &UIWizardNewVMPageExpert::sltGetWithFileOpenDialog);

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

void UIWizardNewVMPageExpert::retranslateUi()
{
    /* Translate widgets: */
    m_pNameAndSystemCnt->setTitle(UIWizardNewVM::tr("Name and operating system"));
    m_pMemoryCnt->setTitle(UIWizardNewVM::tr("&Memory size"));
    m_pDiskCnt->setTitle(UIWizardNewVM::tr("Hard disk"));
    m_pDiskSkip->setText(UIWizardNewVM::tr("&Do not add a virtual hard disk"));
    m_pDiskCreate->setText(UIWizardNewVM::tr("&Create a virtual hard disk now"));
    m_pDiskPresent->setText(UIWizardNewVM::tr("&Use an existing virtual hard disk file"));
    m_pVMMButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
}

void UIWizardNewVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get recommended 'ram' field value: */
    CGuestOSType type = field("type").value<CGuestOSType>();
    ULONG recommendedRam = type.GetRecommendedRAM();
    m_pBaseMemoryEditor->setValue(recommendedRam);
}

void UIWizardNewVMPageExpert::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    cleanupMachineFolder();
}

bool UIWizardNewVMPageExpert::isComplete() const
{
    /* Make sure mandatory fields are complete,
     * 'ram' field feats the bounds,
     * 'virtualDisk' field feats the rules: */
    return UIWizardPage::isComplete() &&
           (m_pDiskSkip->isChecked() || !m_pDiskPresent->isChecked() || !uiCommon().medium(m_pDiskSelector->id()).isNull());
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
