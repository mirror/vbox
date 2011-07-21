/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UICloneVMWizard class implementation
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <QCheckBox>
#include <QRadioButton>
#include <QRegExpValidator>

/* Local includes: */
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIFileDialog.h"
#include "UIIconPool.h"
#include "UICloneVMWizard.h"
#include "iprt/path.h"

UICloneVMWizard::UICloneVMWizard(QWidget *pParent, CMachine machine, CSnapshot snapshot /* = CSnapshot() */)
    : QIWizard(pParent)
    , m_machine(machine)
{
    /* Create & add pages: */
    setPage(PageIntro, new UICloneVMWizardPage1(machine.GetName()));
    /* If we are having a snapshot we can show the "Linked" option. */
    if (!snapshot.isNull())
        setPage(PageType, new UICloneVMWizardPage2());
    /* If the machine has no snapshots, we don't bother the user about options
     * for it. */
    if (machine.GetSnapshotCount() > 0)
        setPage(PageMode, new UICloneVMWizardPage3(snapshot.isNull() ? false : snapshot.GetChildrenCount() > 0));

    /* Translate wizard: */
    retranslateUi();

    /* Translate wizard pages: */
    retranslateAllPages();

    /* Resize wizard to 'golden ratio': */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    setMinimumSize(QSize(600, 400));
    /* Assign background image: */
    assignBackground(":/vmw_clone_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark: */
    assignWatermark(":/vmw_clone.png");
#endif /* Q_WS_MAC */
}

void UICloneVMWizard::retranslateUi()
{
    /* Assign wizard title: */
    setWindowTitle(tr("Clone a virtual machine"));

    setButtonText(QWizard::FinishButton, tr("Clone"));
}

bool UICloneVMWizard::createClone(const QString &strName, KCloneMode mode, bool fReinitMACs, bool fLinked /* = false */)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    const QString &strSettingsFile = vbox.ComposeMachineFilename(strName, QString::null);

    /* Create a new machine object. */
    CMachine cloneMachine = vbox.CreateMachine(strSettingsFile, strName, QString::null, QString::null, false);
    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateMachine(vbox, this);
        return false;
    }

    /* Add the keep all MACs option to the import settings when requested. */
    QVector<KCloneOptions> options;
    if (!fReinitMACs)
        options.append(KCloneOptions_KeepAllMACs);
    /* Linked clones requested? */
    if (fLinked)
        options.append(KCloneOptions_Link);

    /* Start cloning. */
    CProgress progress = m_machine.CloneTo(cloneMachine, mode, options);
    if (!m_machine.isOk())
    {
        vboxProblem().cannotCreateClone(m_machine, this);
        return false;
    }

    /* Wait until done. */
    vboxProblem().showModalProgressDialog(progress, windowTitle(), ":/progress_clone_90px.png", this, true);
    if (progress.GetCanceled())
        return false;
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        vboxProblem().cannotCreateClone(m_machine, progress, this);
        return false;
    }

    /* Finally register the clone machine. */
    vbox.RegisterMachine(cloneMachine);
    if (!vbox.isOk())
    {
        vboxProblem().cannotRegisterMachine(vbox, m_machine, this);
        return false;
    }

    return true;
}

UICloneVMWizardPage1::UICloneVMWizardPage1(const QString &strOriName)
  : m_strOriName(strOriName)
{
    /* Decorate page: */
    Ui::UICloneVMWizardPage1::setupUi(this);

    registerField("cloneName", this, "cloneName");
    registerField("reinitMACs", this, "reinitMACs");

    connect(m_pNameEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltNameEditorTextChanged(const QString &)));
}

QString UICloneVMWizardPage1::cloneName() const
{
    return m_pNameEditor->text();
}

void UICloneVMWizardPage1::setCloneName(const QString &strName)
{
    m_pNameEditor->setText(strName);
}

bool UICloneVMWizardPage1::isReinitMACsChecked() const
{
    return mReinitMACsCheckBox->isChecked();
}

void UICloneVMWizardPage1::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UICloneVMWizardPage1::retranslateUi(this);

    /* Set 'Page1' page title: */
    setTitle(tr("Welcome to the virtual machine clone wizard"));

    /* Append page text with common part: */
    QString strCommonPart = QString("<p>%1</p>").arg(standardHelpText());
    m_pLabel->setText(m_pLabel->text() + strCommonPart);
}

void UICloneVMWizardPage1::initializePage()
{
    /* Retranslate page: */
    retranslateUi();

    m_pNameEditor->setText(tr("%1 Clone").arg(m_strOriName));
}

bool UICloneVMWizardPage1::isComplete() const
{
    QString strName = m_pNameEditor->text().trimmed();
    return !strName.isEmpty() && strName != m_strOriName;
}

bool UICloneVMWizardPage1::validatePage()
{
    if (isFinalPage())
    {
        /* Start performing long-time operation: */
        startProcessing();
        /* Try to create the clone: */
        bool fResult = static_cast<UICloneVMWizard*>(wizard())->createClone(cloneName(), KCloneMode_MachineState, isReinitMACsChecked());
        /* Finish performing long-time operation: */
        endProcessing();
        /* Return operation result: */
        return fResult;
    }else
        return true;
}

void UICloneVMWizardPage1::sltNameEditorTextChanged(const QString & /* strText */)
{
    /* Notify wizard sub-system about complete status changed: */
    emit completeChanged();
}

UICloneVMWizardPage2::UICloneVMWizardPage2()
{
    /* Decorate page: */
    Ui::UICloneVMWizardPage2::setupUi(this);

    QButtonGroup *pButtonGroup = new QButtonGroup(this);
    pButtonGroup->addButton(m_pFullCloneRadio);
    pButtonGroup->addButton(m_pLinkedCloneRadio);

    connect(pButtonGroup, SIGNAL(buttonClicked(QAbstractButton *)),
            this, SLOT(buttonClicked(QAbstractButton *)));
}

void UICloneVMWizardPage2::buttonClicked(QAbstractButton *pButton)
{
    setFinalPage(pButton != m_pFullCloneRadio);
}

void UICloneVMWizardPage2::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UICloneVMWizardPage2::retranslateUi(this);

    /* Set 'Page2' page title: */
    setTitle(tr("Cloning Configuration"));
}

void UICloneVMWizardPage2::initializePage()
{
    /* Retranslate page: */
    retranslateUi();
}

int UICloneVMWizardPage2::nextId() const
{
    return m_pFullCloneRadio->isChecked() ? UICloneVMWizard::PageMode : -1;
}

bool UICloneVMWizardPage2::validatePage()
{
    if (isFinalPage())
    {
        /* Start performing long-time operation: */
        startProcessing();
        /* Try to create the clone: */
        QString strName = field("cloneName").toString();
        bool fReinitMACs = field("reinitMACs").toBool();
        bool fResult = static_cast<UICloneVMWizard*>(wizard())->createClone(strName, KCloneMode_MachineState, fReinitMACs, true);
        /* Finish performing long-time operation: */
        endProcessing();
        /* Return operation result: */
        return fResult;
    }
    else
        return true;
}

UICloneVMWizardPage3::UICloneVMWizardPage3(bool fShowChildsOption /* = true */)
  : m_fShowChildsOption(fShowChildsOption)
{
    /* Decorate page: */
    Ui::UICloneVMWizardPage3::setupUi(this);

    if (!fShowChildsOption)
       m_pMachineAndChildsRadio->hide();
}

void UICloneVMWizardPage3::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UICloneVMWizardPage3::retranslateUi(this);

    /* Set 'Page3' page title: */
    setTitle(tr("Cloning Configuration"));

    const QString strGeneral = tr("Please choose which parts of the virtual machine should be cloned.");
    const QString strOpt1    = tr("If you select <b>Current machine state</b>, only the current state of the virtual machine is cloned.");
    const QString strOpt2    = tr("If you select <b>Current machine and all child states</b> the current state of the virtual machine and any states of child snapshots are cloned.");
    const QString strOpt3    = tr("If you select <b>All states</b>, the current machine state and all snapshots are cloned.");
    if (m_fShowChildsOption)
        m_pLabel->setText(QString("<p>%1</p><p>%2 %3 %4</p>")
                          .arg(strGeneral)
                          .arg(strOpt1)
                          .arg(strOpt2)
                          .arg(strOpt3));
    else
        m_pLabel->setText(QString("<p>%1</p><p>%2 %3</p>")
                          .arg(strGeneral)
                          .arg(strOpt1)
                          .arg(strOpt3));
}

void UICloneVMWizardPage3::initializePage()
{
    /* Retranslate page: */
    retranslateUi();
}

bool UICloneVMWizardPage3::validatePage()
{
    /* Start performing long-time operation: */
    startProcessing();
    /* Try to create the clone: */
    QString strName = field("cloneName").toString();
    bool fReinitMACs = field("reinitMACs").toBool();
    bool fResult = static_cast<UICloneVMWizard*>(wizard())->createClone(strName, cloneMode(), fReinitMACs);
    /* Finish performing long-time operation: */
    endProcessing();
    /* Return operation result: */
    return fResult;
}

KCloneMode UICloneVMWizardPage3::cloneMode() const
{
    if (m_pMachineRadio->isChecked())
        return KCloneMode_MachineState;
    else if (m_pMachineAndChildsRadio->isChecked())
        return KCloneMode_MachineAndChildStates;
    return KCloneMode_AllStates;
}

void UICloneVMWizardPage3::setCloneMode(KCloneMode mode)
{
    switch(mode)
    {
        case KCloneMode_MachineState: m_pMachineRadio->setChecked(true); break;
        case KCloneMode_MachineAndChildStates: m_pMachineAndChildsRadio->setChecked(true); break;
        case KCloneMode_AllStates: m_pAllRadio->setChecked(true); break;
    }
}

