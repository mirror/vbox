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
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "VBoxDefs.h"
#include "QIRichTextLabel.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"
#include "UIWizardNewVD.h"

UIWizardNewVMPageBasic4::UIWizardNewVMPageBasic4()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel1 = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
        m_pBootHDCnt = new QGroupBox(this);
            m_pBootHDCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pBootHDCnt->setCheckable(true);
            QGridLayout *pDiskLayout = new QGridLayout(m_pBootHDCnt);
                m_pDiskCreate = new QRadioButton(m_pBootHDCnt);
                m_pDiskPresent = new QRadioButton(m_pBootHDCnt);
                QStyleOptionButton options;
                options.initFrom(m_pDiskCreate);
                int wid = m_pDiskCreate->style()->subElementRect(QStyle::SE_RadioButtonIndicator, &options, m_pDiskCreate).width() +
                          m_pDiskCreate->style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, &options, m_pDiskCreate) -
                          pDiskLayout->spacing() - 1;
                QSpacerItem *pSpacer = new QSpacerItem(wid, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
                m_pDiskSelector = new VBoxMediaComboBox(m_pBootHDCnt);
                    m_pDiskSelector->setType(VBoxDefs::MediumType_HardDisk);
                    m_pDiskSelector->repopulate();
                m_pVMMButton = new QIToolButton(m_pBootHDCnt);
                    m_pVMMButton->setAutoRaise(true);
                    m_pVMMButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
            pDiskLayout->addWidget(m_pDiskCreate, 0, 0, 1, 3);
            pDiskLayout->addWidget(m_pDiskPresent, 1, 0, 1, 3);
            pDiskLayout->addItem(pSpacer, 2, 0);
            pDiskLayout->addWidget(m_pDiskSelector, 2, 1);
            pDiskLayout->addWidget(m_pVMMButton, 2, 2);
    pMainLayout->addWidget(m_pLabel1);
    pMainLayout->addWidget(m_pLabel2);
    pMainLayout->addWidget(m_pBootHDCnt);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pBootHDCnt, SIGNAL(toggled(bool)), this, SLOT(virtualDiskSourceChanged()));
    connect(m_pDiskCreate, SIGNAL(toggled(bool)), this, SLOT(virtualDiskSourceChanged()));
    connect(m_pDiskPresent, SIGNAL(toggled(bool)), this, SLOT(virtualDiskSourceChanged()));
    connect(m_pDiskSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(virtualDiskSourceChanged()));
    connect(m_pVMMButton, SIGNAL(clicked()), this, SLOT(getWithFileOpenDialog()));

    /* Initialise connections: */
    virtualDiskSourceChanged();

    /* Register CMedium class: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("virtualDisk", this, "virtualDisk");
    registerField("virtualDiskId", this, "virtualDiskId");
    registerField("virtualDiskName", this, "virtualDiskName");
    registerField("virtualDiskLocation", this, "virtualDiskLocation");
}

void UIWizardNewVMPageBasic4::ensureNewVirtualDiskDeleted()
{
    if (m_virtualDisk.isNull())
        return;

    QString strId = m_virtualDisk.GetId();

    bool fSuccess = false;

    CProgress progress = m_virtualDisk.DeleteStorage();
    if (m_virtualDisk.isOk())
    {
        msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_media_delete_90px.png", this, true);
        if (progress.isOk() && progress.GetResultCode() == S_OK)
            fSuccess = true;
    }

    if (fSuccess)
        vboxGlobal().removeMedium(VBoxDefs::MediumType_HardDisk, strId);
    else
        msgCenter().cannotDeleteHardDiskStorage(this, m_virtualDisk, progress);

    m_virtualDisk.detach();
}

void UIWizardNewVMPageBasic4::virtualDiskSourceChanged()
{
    m_pDiskCreate->setEnabled(m_pBootHDCnt->isChecked());
    m_pDiskPresent->setEnabled(m_pBootHDCnt->isChecked());
    m_pDiskSelector->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());
    m_pVMMButton->setEnabled(m_pDiskPresent->isEnabled() && m_pDiskPresent->isChecked());

    if (m_pBootHDCnt->isChecked() && m_pDiskPresent->isChecked())
    {
        m_strVirtualDiskId = m_pDiskSelector->id();
        m_strVirtualDiskName = m_pDiskSelector->currentText();
        m_strVirtualDiskLocation = m_pDiskSelector->location();
    }
    else
    {
        m_strVirtualDiskId.clear();
        m_strVirtualDiskName.clear();
        m_strVirtualDiskLocation.clear();
    }

    emit completeChanged();
}

void UIWizardNewVMPageBasic4::getWithFileOpenDialog()
{
    /* Get opened vboxMedium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(VBoxDefs::MediumType_HardDisk, this);
    if (!strMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pDiskSelector->setCurrentItem(strMediumId);
        /* Update hard disk source: */
        virtualDiskSourceChanged();
        /* Focus on hard disk combo: */
        m_pDiskSelector->setFocus();
    }
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
    m_pBootHDCnt->setTitle(UIWizardNewVM::tr("Start-up &Disk"));
    m_pDiskCreate->setText(UIWizardNewVM::tr("&Create new hard disk"));
    m_pDiskPresent->setText(UIWizardNewVM::tr("&Use existing hard disk"));
    m_pVMMButton->setToolTip(UIWizardNewVM::tr("Choose a virtual hard disk file..."));
}

void UIWizardNewVMPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Prepare initial choice: */
    m_pBootHDCnt->setChecked(true);
    m_pDiskSelector->setCurrentIndex(0);
    m_pDiskCreate->setChecked(true);

    /* 'Create new hard-disk' should have focus initially: */
    m_pDiskCreate->setFocus();
}

void UIWizardNewVMPageBasic4::cleanupPage()
{
    /* Clean medium if present */
    ensureNewVirtualDiskDeleted();
    /* Clean fields of that page */
    UIWizardPage::cleanupPage();
}

bool UIWizardNewVMPageBasic4::isComplete() const
{
    /* Check what virtualDisk feats the rules: */
    return !m_pBootHDCnt->isChecked() ||
           !m_pDiskPresent->isChecked() ||
           !vboxGlobal().findMedium(m_pDiskSelector->id()).isNull();
}

bool UIWizardNewVMPageBasic4::validatePage()
{
    /* Ensure unused virtual-disk is deleted: */
    if (!m_pBootHDCnt->isChecked() || m_pDiskCreate->isChecked() || (!m_virtualDisk.isNull() && m_strVirtualDiskId != m_virtualDisk.GetId()))
        ensureNewVirtualDiskDeleted();

    /* Ask user about disk-less machine: */
    if (!m_pBootHDCnt->isChecked() && !msgCenter().confirmHardDisklessMachine(this))
        return false;

    /* Show the New Virtual Disk wizard: */
    if (m_pBootHDCnt->isChecked() && m_pDiskCreate->isChecked() && !getWithNewVirtualDiskWizard())
        return false;

    return true;
}

bool UIWizardNewVMPageBasic4::getWithNewVirtualDiskWizard()
{
    UIWizardNewVD dlg(this, field("machineBaseName").toString(), field("machineFolder").toString(), field("type").value<CGuestOSType>().GetRecommendedHDD());
    if (dlg.exec() == QDialog::Accepted)
    {
        m_virtualDisk = dlg.virtualDisk();
        m_pDiskSelector->setCurrentItem(m_virtualDisk.GetId());
        m_pDiskPresent->click();
        return true;
    }
    return false;
}

CMedium UIWizardNewVMPageBasic4::virtualDisk() const
{
    return m_virtualDisk;
}

void UIWizardNewVMPageBasic4::setVirtualDisk(const CMedium &virtualDisk)
{
    m_virtualDisk = virtualDisk;
}

QString UIWizardNewVMPageBasic4::virtualDiskId() const
{
    return m_strVirtualDiskId;
}

void UIWizardNewVMPageBasic4::setVirtualDiskId(const QString &strVirtualDiskId)
{
    m_strVirtualDiskId = strVirtualDiskId;
}

QString UIWizardNewVMPageBasic4::virtualDiskName() const
{
    return m_strVirtualDiskName;
}

void UIWizardNewVMPageBasic4::setVirtualDiskName(const QString &strVirtualDiskName)
{
    m_strVirtualDiskName = strVirtualDiskName;
}

QString UIWizardNewVMPageBasic4::virtualDiskLocation() const
{
    return m_strVirtualDiskLocation;
}

void UIWizardNewVMPageBasic4::setVirtualDiskLocation(const QString &strVirtualDiskLocation)
{
    m_strVirtualDiskLocation = strVirtualDiskLocation;
}

