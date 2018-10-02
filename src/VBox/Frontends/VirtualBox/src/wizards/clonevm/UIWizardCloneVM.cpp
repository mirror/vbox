/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVM class implementation.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIWizardCloneVM.h"
# include "UIWizardCloneVMPageBasic1.h"
# include "UIWizardCloneVMPageBasic2.h"
# include "UIWizardCloneVMPageBasic3.h"
# include "UIWizardCloneVMPageExpert.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"

/* COM includes: */
# include "CConsole.h"
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardCloneVM::UIWizardCloneVM(QWidget *pParent, const CMachine &machine,
                                 const QString &strGroup, CSnapshot snapshot /* = CSnapshot() */)
    : UIWizard(pParent, WizardType_CloneVM)
    , m_machine(machine)
    , m_snapshot(snapshot)
    , m_strGroup(strGroup)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_clone.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/wizard_clone_bg.png");
#endif /* VBOX_WS_MAC */
}

bool UIWizardCloneVM::cloneVM()
{
    /* Get the clone name: */
    QString strName = field("cloneName").toString();
    /* Get the clone setting file path: */
    QString strSettingsFile = field("cloneFilePath").toString();

    /* Should we create linked clone? */
    bool fLinked = field("linkedClone").toBool();
    /* Get clone mode: */
    KCloneMode cloneMode = (mode() == WizardMode_Basic && page(Page3)) ||
                           (mode() == WizardMode_Expert && page(PageExpert)) ?
                           field("cloneMode").value<KCloneMode>() : KCloneMode_MachineState;

    /* Get VBox object: */
    CVirtualBox vbox = vboxGlobal().virtualBox();

    /* Prepare machine for cloning: */
    CMachine srcMachine = m_machine;

    /* If the user like to create a linked clone from the current machine, we
     * have to take a little bit more action. First we create an snapshot, so
     * that new differencing images on the source VM are created. Based on that
     * we could use the new snapshot machine for cloning. */
    if (fLinked && m_snapshot.isNull())
    {
        /* Open session: */
        CSession session = vboxGlobal().openSession(m_machine.GetId());
        if (session.isNull())
            return false;

        /* Prepare machine: */
        CMachine machine = session.GetMachine();

        /* Take the snapshot: */
        QString strSnapshotName = tr("Linked Base for %1 and %2").arg(m_machine.GetName()).arg(strName);
        QString strSnapshotId;
        CProgress progress = machine.TakeSnapshot(strSnapshotName, "", true, strSnapshotId);

        if (machine.isOk())
        {
            /* Show the "Taking Snapshot" progress dialog: */
            msgCenter().showModalProgressDialog(progress, m_machine.GetName(), ":/progress_snapshot_create_90px.png", this);

            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotTakeSnapshot(progress, m_machine.GetName(), this);
                return false;
            }
        }
        else
        {
            msgCenter().cannotTakeSnapshot(machine, m_machine.GetName(), this);
            return false;
        }

        /* Unlock machine finally: */
        session.UnlockMachine();

        /* Get the new snapshot and the snapshot machine. */
        const CSnapshot &newSnapshot = m_machine.FindSnapshot(strSnapshotId);
        if (newSnapshot.isNull())
        {
            msgCenter().cannotFindSnapshotByName(m_machine, strSnapshotName, this);
            return false;
        }
        srcMachine = newSnapshot.GetMachine();
    }

    /* Create a new machine object. */
    CMachine cloneMachine = vbox.CreateMachine(strSettingsFile, strName, QVector<QString>(), QString::null, QString::null);
    if (!vbox.isOk())
    {
        msgCenter().cannotCreateMachine(vbox, this);
        return false;
    }

    /* Clone options vector to pass to cloning: */
    QVector<KCloneOptions> options;
    /* Set the selected MAC address policy: */
    switch (field("macAddressClonePolicy").value<MACAddressClonePolicy>())
    {
        case MACAddressClonePolicy_KeepAllMACs:
            options.append(KCloneOptions_KeepAllMACs);
            break;
        case MACAddressClonePolicy_KeepNATMACs:
            options.append(KCloneOptions_KeepNATMACs);
            break;
        default:
            break;
    }

    if (field("keepDiskNames").value<bool>())
        options.append(KCloneOptions_KeepDiskNames);
    if (field("keepHWUUIDs").value<bool>())
        options.append(KCloneOptions_KeepHwUUIDs);

    /* Linked clones requested? */
    if (fLinked)
        options.append(KCloneOptions_Link);

    /* Start cloning. */
    CProgress progress = srcMachine.CloneTo(cloneMachine, cloneMode, options);
    if (!srcMachine.isOk())
    {
        msgCenter().cannotCreateClone(srcMachine, this);
        return false;
    }

    /* Wait until done. */
    msgCenter().showModalProgressDialog(progress, windowTitle(), ":/progress_clone_90px.png", this);
    if (progress.GetCanceled())
        return false;
    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateClone(progress, srcMachine.GetName(), this);
        return false;
    }

    /* Finally register the clone machine. */
    vbox.RegisterMachine(cloneMachine);
    if (!vbox.isOk())
    {
        msgCenter().cannotRegisterMachine(vbox, cloneMachine.GetName(), this);
        return false;
    }

    return true;
}

void UIWizardCloneVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Clone Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Clone"));
}

void UIWizardCloneVM::prepare()
{
    QString strDefaultMachineFolder = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardCloneVMPageBasic1(m_machine.GetName(), strDefaultMachineFolder, m_strGroup));
            setPage(Page2, new UIWizardCloneVMPageBasic2(m_snapshot.isNull()));
            if (m_machine.GetSnapshotCount() > 0)
                setPage(Page3, new UIWizardCloneVMPageBasic3(m_snapshot.isNull() ? false : m_snapshot.GetChildrenCount() > 0));
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardCloneVMPageExpert(m_machine.GetName(),
                                                              strDefaultMachineFolder,
                                                              m_snapshot.isNull(),
                                                              m_snapshot.isNull() ? false : m_snapshot.GetChildrenCount() > 0,
                                                              m_strGroup));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}
