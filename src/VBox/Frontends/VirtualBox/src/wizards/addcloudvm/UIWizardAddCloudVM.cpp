/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardAddCloudVM class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include "UIMessageCenter.h"
#include "UIWizardAddCloudVM.h"
#include "UIWizardAddCloudVMPageBasic1.h"
#include "UIWizardAddCloudVMPageExpert.h"

/* COM includes: */
#include "CProgress.h"


UIWizardAddCloudVM::UIWizardAddCloudVM(QWidget *pParent,
                                       const CCloudClient &comClient /* = CCloudClient() */,
                                       WizardMode enmMode /* = WizardMode_Auto */)
    : UIWizard(pParent, WizardType_AddCloudVM, enmMode)
    , m_comClient(comClient)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_new_cloud_vm.png");
#else
    /* Assign background image: */
    assignBackground(":/wizard_new_cloud_vm_bg.png");
#endif
}

void UIWizardAddCloudVM::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardAddCloudVMPageBasic1);
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardAddCloudVMPageExpert);
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

bool UIWizardAddCloudVM::addCloudVMs()
{
    /* Acquire prepared client and description: */
    CCloudClient comClient = client();
    AssertReturn(comClient.isNotNull(), false);

    /* For each cloud instance name we have: */
    foreach (const QString &strInstanceName, field("instanceIds").toStringList())
    {
        /* Initiate cloud VM add procedure: */
        CProgress comProgress = comClient.AddCloudMachine(strInstanceName);
        if (!comClient.isOk())
        {
            msgCenter().cannotCreateCloudMachine(comClient, this);
            return false;
        }

        /* Show "Add cloud machine" progress: */
        msgCenter().showModalProgressDialog(comProgress, tr("Add cloud machine ..."),
                                            ":/progress_new_cloud_vm_90px.png", this, 0);
        if (comProgress.GetCanceled())
            return false;
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            msgCenter().cannotCreateCloudMachine(comProgress, this);
            return false;
        }
    }

    /* Success by default: */
    return true;
}

void UIWizardAddCloudVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Add Cloud Virtual Machine"));
    setButtonText(QWizard::FinishButton, tr("Add"));
}
