/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVM class implementation
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

/* Local includes: */
#include "UIWizardCloneVM.h"
#include "UIWizardCloneVMPageBasic1.h"
#include "UIWizardCloneVMPageBasic2.h"
#include "UIWizardCloneVMPageBasic3.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

UIWizardCloneVM::UIWizardCloneVM(QWidget *pParent, const CMachine &machine, CSnapshot snapshot /* = CSnapshot() */)
    : UIWizard(pParent)
    , m_machine(machine)
    , m_snapshot(snapshot)
{
#ifdef Q_WS_WIN
    /* Hide window icon: */
    setWindowIcon(QIcon());
#endif /* Q_WS_WIN */

    /* Create & add pages: */
    setPage(Page1, new UIWizardCloneVMPageBasic1(m_machine.GetName()));
    /* If we are having a snapshot we can show the "Linked" option. */
    setPage(Page2, new UIWizardCloneVMPageBasic2(snapshot.isNull()));
    /* If the machine has no snapshots, we don't bother the user about options for it. */
    if (m_machine.GetSnapshotCount() > 0)
        setPage(Page3, new UIWizardCloneVMPageBasic3(snapshot.isNull() ? false : snapshot.GetChildrenCount() > 0));

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
    resizeToGoldenRatio(UIWizardType_CloneVM);
}

void UIWizardCloneVM::retranslateUi()
{
    /* Translate wizard: */
    setWindowTitle(tr("Clone Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Clone"));
}

bool UIWizardCloneVM::cloneVM()
{
    /* Get clone name: */
    QString strName = field("cloneName").toString();
    /* Should we reinit mac status? */
    bool fReinitMACs = field("reinitMACs").toBool();
    /* Should we create linked clone? */
    bool fLinked = field("linkedClone").toBool();
    /* Get clone mode: */
    KCloneMode mode = page(Page3) ? field("cloneMode").value<KCloneMode>() : KCloneMode_MachineState;

    CVirtualBox vbox = vboxGlobal().virtualBox();
    const QString &strSettingsFile = vbox.ComposeMachineFilename(strName, QString::null);

    CMachine srcMachine = m_machine;
    /* If the user like to create a linked clone from the current machine, we
     * have to take a little bit more action. First we create an snapshot, so
     * that new differencing images on the source VM are created. Based on that
     * we could use the new snapshot machine for cloning. */
    if (fLinked && m_snapshot.isNull())
    {
        const QString &strId = m_machine.GetId();
        CSession session = vboxGlobal().openSession(strId);
        if (session.isNull())
            return false;
        CConsole console = session.GetConsole();

        /* Take the snapshot: */
        QString strSnapshotName = tr("Linked Base for %1 and %2").arg(m_machine.GetName()).arg(strName);
        CProgress progress = console.TakeSnapshot(strSnapshotName, "");

        if (console.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog: */
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

