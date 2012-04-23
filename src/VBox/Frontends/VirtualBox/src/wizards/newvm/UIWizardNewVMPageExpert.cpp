/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageExpert class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QSpacerItem>
#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>

/* Local includes: */
#include "UIWizardNewVMPageExpert.h"
#include "UIWizardNewVM.h"
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "VBoxOSTypeSelectorWidget.h"
#include "VBoxGuestRAMSlider.h"
#include "VBoxMediaComboBox.h"
#include "QILineEdit.h"
#include "QIToolButton.h"

UIWizardNewVMPageExpert::UIWizardNewVMPageExpert()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        pMainLayout->setContentsMargins(8, 6, 8, 6);
        m_pNameCnt = new QGroupBox(this);
        {
            m_pNameCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pNameCntLayout = new QHBoxLayout(m_pNameCnt);
            {
                m_pNameEditor = new QLineEdit(m_pNameCnt);
                pNameCntLayout->addWidget(m_pNameEditor);
            }
        }
        m_pTypeCnt = new QGroupBox(this);
        {
            m_pTypeCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pTypeCntLayout = new QHBoxLayout(m_pTypeCnt);
            {
                m_pTypeSelector = new VBoxOSTypeSelectorWidget(m_pTypeCnt);
                {
                    m_pTypeSelector->activateLayout();
                }
                pTypeCntLayout->addWidget(m_pTypeSelector);
            }
        }
        m_pMemoryCnt = new QGroupBox(this);
        {
            m_pMemoryCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QGridLayout *pMemoryCntLayout = new QGridLayout(m_pMemoryCnt);
            {
                m_pRamSlider = new VBoxGuestRAMSlider(m_pMemoryCnt);
                {
                    m_pRamSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    m_pRamSlider->setOrientation(Qt::Horizontal);
                    m_pRamSlider->setTickPosition(QSlider::TicksBelow);
                    m_pRamSlider->setValue(m_pTypeSelector->type().GetRecommendedRAM());
                }
                m_pRamEditor = new QILineEdit(m_pMemoryCnt);
                {
                    m_pRamEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    m_pRamEditor->setFixedWidthByText("88888");
                    m_pRamEditor->setAlignment(Qt::AlignRight);
                    m_pRamEditor->setValidator(new QIntValidator(m_pRamSlider->minRAM(), m_pRamSlider->maxRAM(), this));
                    m_pRamEditor->setText(QString::number(m_pTypeSelector->type().GetRecommendedRAM()));
                }
                m_pRamUnits = new QLabel(m_pMemoryCnt);
                {
                    m_pRamUnits->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                }
                m_pRamMin = new QLabel(m_pMemoryCnt);
                {
                    m_pRamMin->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                }
                QSpacerItem *m_pRamSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding);
                m_pRamMax = new QLabel(m_pMemoryCnt);
                {
                    m_pRamMax->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                }
                pMemoryCntLayout->addWidget(m_pRamSlider, 0, 0, 1, 3);
                pMemoryCntLayout->addWidget(m_pRamEditor, 0, 3);
                pMemoryCntLayout->addWidget(m_pRamUnits, 0, 4);
                pMemoryCntLayout->addWidget(m_pRamMin, 1, 0);
                pMemoryCntLayout->addItem(m_pRamSpacer, 1, 1);
                pMemoryCntLayout->addWidget(m_pRamMax, 1, 2);
            }
        }
        m_pDiskCnt = new QGroupBox(this);
        {
            m_pDiskCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pDiskCnt->setCheckable(true);
            m_pDiskCnt->setChecked(true);
            QGridLayout *pDiskCntLayout = new QGridLayout(m_pDiskCnt);
            {
                m_pDiskCreate = new QRadioButton(m_pDiskCnt);
                {
                    m_pDiskCreate->setChecked(true);
                }
                m_pDiskPresent = new QRadioButton(m_pDiskCnt);
                QStyleOptionButton options;
                options.initFrom(m_pDiskCreate);
                int iWidth = m_pDiskCreate->style()->subElementRect(QStyle::SE_RadioButtonIndicator, &options, m_pDiskCreate).width() +
                             m_pDiskCreate->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, &options, m_pDiskCreate) -
                             pDiskCntLayout->spacing() - 1;
                QSpacerItem *pSpacer = new QSpacerItem(iWidth, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
                m_pDiskSelector = new VBoxMediaComboBox(m_pDiskCnt);
                {
                    m_pDiskSelector->setType(VBoxDefs::MediumType_HardDisk);
                    m_pDiskSelector->repopulate();
                }
                m_pVMMButton = new QIToolButton(m_pDiskCnt);
                {
                    m_pVMMButton->setAutoRaise(true);
                    m_pVMMButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
                }
                pDiskCntLayout->addWidget(m_pDiskCreate, 0, 0, 1, 3);
                pDiskCntLayout->addWidget(m_pDiskPresent, 1, 0, 1, 3);
                pDiskCntLayout->addItem(pSpacer, 2, 0);
                pDiskCntLayout->addWidget(m_pDiskSelector, 2, 1);
                pDiskCntLayout->addWidget(m_pVMMButton, 2, 2);
            }
        }
        pMainLayout->addWidget(m_pNameCnt);
        pMainLayout->addWidget(m_pTypeCnt);
        pMainLayout->addWidget(m_pMemoryCnt);
        pMainLayout->addWidget(m_pDiskCnt);
        pMainLayout->addStretch();
        updateVirtualDiskSource();
    }

    /* Setup connections: */
    connect(m_pNameEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltNameChanged(const QString &)));
    connect(m_pTypeSelector, SIGNAL(osTypeChanged()), this, SLOT(sltOsTypeChanged()));
    connect(m_pRamSlider, SIGNAL(valueChanged(int)), this, SLOT(sltRamSliderValueChanged(int)));
    connect(m_pRamEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltRamEditorTextChanged(const QString &)));
    connect(m_pDiskCnt, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskCreate, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskPresent, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pVMMButton, SIGNAL(clicked()), this, SLOT(sltGetWithFileOpenDialog()));

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("name*", m_pNameEditor);
    registerField("type", m_pTypeSelector, "type", SIGNAL(osTypeChanged()));
    registerField("machineFolder", this, "machineFolder");
    registerField("machineBaseName", this, "machineBaseName");
    registerField("ram", m_pRamSlider, "value", SIGNAL(valueChanged(int)));
    registerField("virtualDisk", this, "virtualDisk");
    registerField("virtualDiskId", this, "virtualDiskId");
    registerField("virtualDiskLocation", this, "virtualDiskLocation");
}

void UIWizardNewVMPageExpert::sltNameChanged(const QString &strNewText)
{
    /* Call to base-class: */
    onNameChanged(strNewText);

    /* Fetch recommended RAM value: */
    CGuestOSType type = m_pTypeSelector->type();
    m_pRamSlider->setValue(type.GetRecommendedRAM());
    m_pRamEditor->setText(QString::number(type.GetRecommendedRAM()));

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltOsTypeChanged()
{
    /* Call to base-class: */
    onOsTypeChanged();

    /* Fetch recommended RAM value: */
    CGuestOSType type = m_pTypeSelector->type();
    m_pRamSlider->setValue(type.GetRecommendedRAM());
    m_pRamEditor->setText(QString::number(type.GetRecommendedRAM()));

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltRamSliderValueChanged(int iValue)
{
    /* Call to base-class: */
    onRamSliderValueChanged(iValue);

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageExpert::sltRamEditorTextChanged(const QString &strText)
{
    /* Call to base-class: */
    onRamEditorTextChanged(strText);

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
    m_pNameCnt->setTitle(UIWizardNewVM::tr("&Name"));
    m_pTypeCnt->setTitle(UIWizardNewVM::tr("OS &Type"));
    m_pMemoryCnt->setTitle(UIWizardNewVM::tr("Base &Memory Size"));
    m_pRamUnits->setText(VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes"));
    m_pRamMin->setText(QString("%1 %2").arg(m_pRamSlider->minRAM()).arg(VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes")));
    m_pRamMax->setText(QString("%1 %2").arg(m_pRamSlider->maxRAM()).arg(VBoxGlobal::tr("MB", "size suffix MBytes=1024 KBytes")));
    m_pDiskCnt->setTitle(UIWizardNewVM::tr("Start-up &Disk"));
    m_pDiskCreate->setText(UIWizardNewVM::tr("&Create new hard disk"));
    m_pDiskPresent->setText(UIWizardNewVM::tr("&Use existing hard disk"));
    m_pVMMButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
}

void UIWizardNewVMPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* 'Name' field should have focus initially: */
    m_pNameEditor->setFocus();
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
           (m_pRamSlider->value() >= qMax(1, (int)m_pRamSlider->minRAM()) && m_pRamSlider->value() <= (int)m_pRamSlider->maxRAM()) &&
           (!m_pDiskCnt->isChecked() || !m_pDiskPresent->isChecked() || !vboxGlobal().findMedium(m_pDiskSelector->id()).isNull());
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
            if (!m_pDiskCnt->isChecked())
            {
                /* Ask user about disk-less machine: */
                fResult = msgCenter().confirmHardDisklessMachine(this);
            }
            else if (m_pDiskCreate->isChecked())
            {
                /* Show the New Virtual Disk wizard if necessary: */
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

