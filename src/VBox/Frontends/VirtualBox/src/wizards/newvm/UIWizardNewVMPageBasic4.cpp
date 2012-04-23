/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVMPageBasic4 class implementation
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
#include <QMetaType>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>

/* Local includes: */
#include "UIWizardNewVMPageBasic4.h"
#include "UIWizardNewVM.h"
#include "VBoxDefs.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"
#include "UIWizardNewVD.h"
#include "QIRichTextLabel.h"

UIWizardNewVMPage4::UIWizardNewVMPage4()
{
}

void UIWizardNewVMPage4::updateVirtualDiskSource()
{
    /* Enable/disable controls: */
    m_pDiskCreate->setEnabled(m_pDiskCnt->isChecked());
    m_pDiskPresent->setEnabled(m_pDiskCnt->isChecked());
    m_pDiskSelector->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());
    m_pVMMButton->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());

    /* Fetch filed values: */
    if (m_pDiskCnt->isChecked() && m_pDiskPresent->isChecked())
    {
        m_strVirtualDiskId = m_pDiskSelector->id();
        m_strVirtualDiskName = m_pDiskSelector->currentText();
        m_strVirtualDiskLocation = m_pDiskSelector->location();
    }
    else
    {
        m_strVirtualDiskId = QString();
        m_strVirtualDiskName = QString();
        m_strVirtualDiskLocation = QString();
    }
}

void UIWizardNewVMPage4::getWithFileOpenDialog()
{
    /* Get opened medium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(VBoxDefs::MediumType_HardDisk, thisImp());
    if (!strMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pDiskSelector->setCurrentItem(strMediumId);
        /* Update hard disk source: */
        updateVirtualDiskSource();
        /* Focus on hard disk combo: */
        m_pDiskSelector->setFocus();
    }
}

bool UIWizardNewVMPage4::getWithNewVirtualDiskWizard()
{
    /* Create New Virtual Disk wizard: */
    UIWizardNewVD dlg(thisImp(),
                      fieldImp("machineBaseName").toString(),
                      fieldImp("machineFolder").toString(),
                      fieldImp("type").value<CGuestOSType>().GetRecommendedHDD());
    if (dlg.exec() == QDialog::Accepted)
    {
        m_virtualDisk = dlg.virtualDisk();
        m_pDiskSelector->setCurrentItem(m_virtualDisk.GetId());
        m_pDiskPresent->click();
        return true;
    }
    return false;
}

void UIWizardNewVMPage4::ensureNewVirtualDiskDeleted()
{
    /* Make sure virtual-disk exists: */
    if (m_virtualDisk.isNull())
        return;

    /* Remember virtual-disk ID: */
    QString strId = m_virtualDisk.GetId();

    /* 1st step: start delete-storage progress: */
    CProgress progress = m_virtualDisk.DeleteStorage();
    /* Get initial state: */
    bool fSuccess = m_virtualDisk.isOk();

    /* 2nd step: show delete-storage progress: */
    if (fSuccess)
    {
        msgCenter().showModalProgressDialog(progress, thisImp()->windowTitle(), ":/progress_media_delete_90px.png", thisImp(), true);
        fSuccess = progress.isOk() && progress.GetResultCode() == S_OK;
    }

    /* 3rd step: notify GUI about virtual-disk was deleted or show error if any: */
    if (fSuccess)
        vboxGlobal().removeMedium(VBoxDefs::MediumType_HardDisk, strId);
    else
        msgCenter().cannotDeleteHardDiskStorage(thisImp(), m_virtualDisk, progress);

    /* Detach virtual-disk finally: */
    m_virtualDisk.detach();
}

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel1 = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
        m_pDiskCnt = new QGroupBox(this);
        {
            m_pDiskCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pDiskCnt->setCheckable(true);
            QGridLayout *pDiskLayout = new QGridLayout(m_pDiskCnt);
            {
                m_pDiskCreate = new QRadioButton(m_pDiskCnt);
                m_pDiskPresent = new QRadioButton(m_pDiskCnt);
                QStyleOptionButton options;
                options.initFrom(m_pDiskCreate);
                int iWidth = m_pDiskCreate->style()->subElementRect(QStyle::SE_RadioButtonIndicator, &options, m_pDiskCreate).width() +
                             m_pDiskCreate->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, &options, m_pDiskCreate) -
                             pDiskLayout->spacing() - 1;
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
                pDiskLayout->addWidget(m_pDiskCreate, 0, 0, 1, 3);
                pDiskLayout->addWidget(m_pDiskPresent, 1, 0, 1, 3);
                pDiskLayout->addItem(pSpacer, 2, 0);
                pDiskLayout->addWidget(m_pDiskSelector, 2, 1);
                pDiskLayout->addWidget(m_pVMMButton, 2, 2);
            }
        }
        pMainLayout->addWidget(m_pLabel1);
        pMainLayout->addWidget(m_pLabel2);
        pMainLayout->addWidget(m_pDiskCnt);
        pMainLayout->addStretch();
        updateVirtualDiskSource();
    }

    /* Setup connections: */
    connect(m_pDiskCnt, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskCreate, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskPresent, SIGNAL(toggled(bool)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pDiskSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(sltVirtualDiskSourceChanged()));
    connect(m_pVMMButton, SIGNAL(clicked()), this, SLOT(sltGetWithFileOpenDialog()));

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("virtualDisk", this, "virtualDisk");
    registerField("virtualDiskId", this, "virtualDiskId");
    registerField("virtualDiskName", this, "virtualDiskName");
    registerField("virtualDiskLocation", this, "virtualDiskLocation");
}

void UIWizardNewVMPageBasic4::sltVirtualDiskSourceChanged()
{
    /* Call to base-class: */
    updateVirtualDiskSource();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVMPageBasic4::sltGetWithFileOpenDialog()
{
    /* Call to base-class: */
    getWithFileOpenDialog();
}

void UIWizardNewVMPageBasic4::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVM::tr("Virtual Hard Disk"));

    /* Translate widgets: */
    m_pLabel1->setText(UIWizardNewVM::tr("<p>If you wish you can now add a start-up disk to the new machine. "
                                         "You can either create a new virtual disk or select one from the list "
                                         "or from another location using the folder icon.</p>"
                                         "<p>If you need a more complex virtual disk setup you can skip this step "
                                         "and make the changes to the machine settings once the machine is created.</p>"));
    QString strRecommendedHDD = field("type").value<CGuestOSType>().isNull() ? QString() :
                                VBoxGlobal::formatSize(field("type").value<CGuestOSType>().GetRecommendedHDD());
    m_pLabel2->setText(UIWizardNewVM::tr("The recommended size of the start-up disk is <b>%1</b>.").arg(strRecommendedHDD));
    m_pDiskCnt->setTitle(UIWizardNewVM::tr("Start-up &Disk"));
    m_pDiskCreate->setText(UIWizardNewVM::tr("&Create new hard disk"));
    m_pDiskPresent->setText(UIWizardNewVM::tr("&Use existing hard disk"));
    m_pVMMButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
}

void UIWizardNewVMPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Prepare initial choice: */
    m_pDiskCnt->setChecked(true);
    m_pDiskSelector->setCurrentIndex(0);
    m_pDiskCreate->setChecked(true);

    /* 'Create new hard-disk' should have focus initially: */
    m_pDiskCreate->setFocus();
}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    /* Call to base-class: */
    ensureNewVirtualDiskDeleted();
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    /* Make sure 'virtualDisk' field feats the rules: */
    return !m_pDiskCnt->isChecked() ||
           !m_pDiskPresent->isChecked() ||
           !vboxGlobal().findMedium(m_pDiskSelector->id()).isNull();
}

bool UIWizardNewVMPageBasic4::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Ensure unused virtual-disk is deleted: */
    if (!m_pDiskCnt->isChecked() || m_pDiskCreate->isChecked() || (!m_virtualDisk.isNull() && m_strVirtualDiskId != m_virtualDisk.GetId()))
        ensureNewVirtualDiskDeleted();

    if (!m_pDiskCnt->isChecked())
    {
        /* Ask user about disk-less machine: */
        fResult = msgCenter().confirmHardDisklessMachine(this);
    }
    else if (m_pDiskCreate->isChecked())
    {
        /* Show the New Virtual Disk wizard: */
        fResult = getWithNewVirtualDiskWizard();
    }

    /* Return result: */
    return fResult;
}

