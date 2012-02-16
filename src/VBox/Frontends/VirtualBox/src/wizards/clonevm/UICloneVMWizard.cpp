/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UICloneVMWizard class implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include "UIMessageCenter.h"
#include "QIFileDialog.h"
#include "UIIconPool.h"
#include "UICloneVMWizard.h"
#include "iprt/path.h"

UICloneVMWizard::UICloneVMWizard(QWidget *pParent, const CMachine &machine, CSnapshot snapshot /* = CSnapshot() */)
    : QIWizard(pParent)
    , m_machine(machine)
    , m_snapshot(snapshot)
{
    /* Create & add pages: */
    setPage(PageIntro, new UICloneVMWizardPage1(m_machine.GetName()));
    /* If we are having a snapshot we can show the "Linked" option. */
    setPage(PageType, new UICloneVMWizardPage2(snapshot.isNull()));
    /* If the machine has no snapshots, we don't bother the user about options
     * for it. */
    if (m_machine.GetSnapshotCount() > 0)
        setPage(PageMode, new UICloneVMWizardPage3(snapshot.isNull() ? false : snapshot.GetChildrenCount() > 0));

    /* Translate wizard: */
    retranslateUi();

    /* Translate wizard pages: */
    retranslateAllPages();

#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_clone.png");
#else /* Q_WS_MAC */
    setMinimumSize(QSize(600, 400));
    /* Assign background image: */
    assignBackground(":/vmw_clone_bg.png");
#endif /* Q_WS_MAC */

    /* Resize wizard to 'golden ratio': */
    resizeToGoldenRatio();
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

    CMachine srcMachine = m_machine;
    /* If the user like to create a linked clone from the current machine, we
     * have to take a little bit more action. First we create an snapshot, so
     * that new differencing images on the source VM are created. Based on that
     * we could use the new snapshot machine for cloning. */
    if (   fLinked
        && m_snapshot.isNull())
    {
        const QString &strId = m_machine.GetId();
        CSession session = vboxGlobal().openSession(strId);
        if (session.isNull())
            return false;
        CConsole console = session.GetConsole();

        /* Take the snapshot */
        QString strSnapshotName = tr("Linked Base for %1 and %2").arg(m_machine.GetName()).arg(strName);
        CProgress progress = console.TakeSnapshot(strSnapshotName, "");

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog */
            msgCenter().showModalProgressDialog(progress, m_machine.GetName(), ":/progress_snapshot_create_90px.png", this, true);

            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotTakeSnapshot(progress);
                return false;
            }
        }
        else
        {
            msgCenter().cannotTakeSnapshot(console);
            return false;
        }

        /* Unlock machine finally: */
        session.UnlockMachine();

        /* Get the new snapshot and the snapshot machine. */
        const CSnapshot &newSnapshot = m_machine.FindSnapshot(strSnapshotName);
        if (newSnapshot.isNull())
        {
            msgCenter().cannotFindSnapshotByName(this, m_machine, strSnapshotName);
            return false;
        }
        srcMachine = newSnapshot.GetMachine();
    }

    /* Create a new machine object. */
    CMachine cloneMachine = vbox.CreateMachine(strSettingsFile, strName, QString::null, QString::null, false);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMachine(vbox, this);
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
    CProgress progress = srcMachine.CloneTo(cloneMachine, mode, options);
    if (!srcMachine.isOk())
    {
        msgCenter().cannotCreateClone(srcMachine, this);
        return false;
    }

    /* Wait until done. */
    msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_clone_90px.png", this, true);
    if (progress.GetCanceled())
        return false;
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateClone(srcMachine, progress, this);
        return false;
    }

    /* Finally register the clone machine. */
    vbox.RegisterMachine(cloneMachine);
    if (!vbox.isOk())
    {
        msgCenter().cannotRegisterMachine(vbox, cloneMachine, this);
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

UICloneVMWizardPage2::UICloneVMWizardPage2(bool fAdditionalInfo)
  : m_fAdditionalInfo(fAdditionalInfo)
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
    /* On older Qt versions the content of the current page isn't updated when
     * using setFinalPage. So switch back and for to simulate it. */
#if QT_VERSION < 0x040700
    wizard()->back();
    wizard()->next();
#endif
}

void UICloneVMWizardPage2::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UICloneVMWizardPage2::retranslateUi(this);

    /* Set 'Page2' page title: */
    setTitle(tr("Cloning Configuration"));

    QString strLabel = tr("<p>Please select the type of the clone.</p><p>If you choose <b>Full Clone</b> an exact copy (including all virtual disk images) of the original VM will be created. If you select <b>Linked Clone</b>, a new VM will be created, but the virtual disk images will point to the virtual disk images of original VM.</p>");
    if (m_fAdditionalInfo)
        strLabel += tr("<p>Note that a new snapshot within the source VM is created in case you select <b>Linked Clone</b>.</p>");
    m_pLabel->setText(strLabel);
}

void UICloneVMWizardPage2::initializePage()
{
    /* Retranslate page: */
    retranslateUi();
}

int UICloneVMWizardPage2::nextId() const
{
    return m_pFullCloneRadio->isChecked() && wizard()->page(UICloneVMWizard::PageMode) ? UICloneVMWizard::PageMode : -1;
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
        bool fResult = static_cast<UICloneVMWizard*>(wizard())->createClone(strName, KCloneMode_MachineState, fReinitMACs, m_pLinkedCloneRadio->isChecked());
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

