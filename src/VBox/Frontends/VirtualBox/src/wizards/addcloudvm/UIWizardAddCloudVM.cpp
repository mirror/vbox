/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVM class implementation.
 */

/*
 * Copyright (C) 2009-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageBasic1.h"
#include "UIWizardAddCloudVMPageExpert.h"

/* COM includes: */
#include "CCloudMachine.h"
#include "CProgress.h"


UIWizardAddCloudVM::UIWizardAddCloudVM(QWidget *pParent,
                                       const QString &strFullGroupName /* = QString() */,
                                       WizardMode enmMode /* = WizardMode_Auto */)
    : UINativeWizard(pParent, WizardType_AddCloudVM, enmMode)
    , m_strFullGroupName(strFullGroupName)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_new_cloud_vm.png");
#else
    /* Assign background image: */
    setPixmapName(":/wizard_new_cloud_vm_bg.png");
#endif
}

bool UIWizardAddCloudVM::addCloudVMs()
{
    /* Prepare result: */
    bool fResult = false;

    /* Acquire prepared client: */
    CCloudClient comClient = client();
    AssertReturn(comClient.isNotNull(), fResult);

    /* For each cloud instance name we have: */
    foreach (const QString &strInstanceName, instanceIds())
    {
        /* Initiate cloud VM add procedure: */
        CCloudMachine comMachine;
        CProgress comProgress = comClient.AddCloudMachine(strInstanceName, comMachine);
        /* Check for immediate errors: */
        if (!comClient.isOk())
        {
            msgCenter().cannotCreateCloudMachine(comClient, this);
            break;
        }
        else
        {
            /* Show "Add cloud machine" progress: */
            msgCenter().showModalProgressDialog(comProgress, QString(),
                                                ":/progress_new_cloud_vm_90px.png", this, 0);
            /* Check for canceled progress: */
            if (comProgress.GetCanceled())
                break;
            else
            {
                /* Check for progress errors: */
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                {
                    msgCenter().cannotCreateCloudMachine(comProgress, this);
                    break;
                }
                else
                {
                    /* Check whether VM really added: */
                    if (comMachine.isNotNull())
                    {
                        /* Notify GUI about VM was added: */
                        uiCommon().notifyCloudMachineRegistered(shortProviderName(),
                                                                profileName(),
                                                                comMachine);

                        /* Finally, success: */
                        fResult = true;
                    }
                }
            }
        }
    }

    /* Return result: */
    return fResult;
}

void UIWizardAddCloudVM::populatePages()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            addPage(new UIWizardAddCloudVMPageBasic1);
            break;
        }
        case WizardMode_Expert:
        {
            addPage(new UIWizardAddCloudVMPageExpert);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

void UIWizardAddCloudVM::retranslateUi()
{
    /* Call to base-class: */
    UINativeWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Add Cloud Virtual Machine"));
    /// @todo implement this?
    //setButtonText(QWizard::FinishButton, tr("Add"));
}
