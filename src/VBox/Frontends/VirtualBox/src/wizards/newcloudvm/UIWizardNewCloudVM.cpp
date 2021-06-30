/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVM class implementation.
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

/* Qt includes: */
#include <QPushButton>

/* GUI includes: */
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic1.h"
#include "UIWizardNewCloudVMPageBasic2.h"
#include "UIWizardNewCloudVMPageExpert.h"

/* COM includes: */
#include "CCloudMachine.h"
#include "CProgress.h"


UIWizardNewCloudVM::UIWizardNewCloudVM(QWidget *pParent,
                                       const QString &strFullGroupName /* = QString() */,
                                       const CCloudClient &comClient /* = CCloudClient() */,
                                       const CVirtualSystemDescription &comVSD /* = CVirtualSystemDescription() */,
                                       WizardMode enmMode /* = WizardMode_Auto */)
    : UINativeWizard(pParent, WizardType_NewCloudVM, enmMode)
    , m_strFullGroupName(strFullGroupName)
    , m_comClient(comClient)
    , m_comVSD(comVSD)
    , m_fFullWizard(m_comClient.isNull() || m_comVSD.isNull())
    , m_fFinalStepPrevented(false)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_new_cloud_vm.png");
#else
    /* Assign background image: */
    setPixmapName(":/wizard_new_cloud_vm_bg.png");
#endif
}

bool UIWizardNewCloudVM::createVSDForm()
{
    /* Prepare result: */
    bool fResult = false;

    /* Main API request sequence, can be interrupted after any step: */
    do
    {
        /* Acquire prepared client and description: */
        CCloudClient comClient = client();
        CVirtualSystemDescription comVSD = vsd();
        AssertReturn(comClient.isNotNull() && comVSD.isNotNull(), false);

        /* Read Cloud Client description form: */
        CVirtualSystemDescriptionForm comForm;
        CProgress comProgress = comClient.GetLaunchDescriptionForm(comVSD, comForm);
        if (!comClient.isOk())
        {
            msgCenter().cannotAcquireCloudClientParameter(comClient);
            break;
        }

        /* Show "Acquire launch form" progress: */
        msgCenter().showModalProgressDialog(comProgress, QString(), ":/progress_refresh_90px.png", this, 0);
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            msgCenter().cannotAcquireCloudClientParameter(comProgress);
            break;
        }

        /* Remember Virtual System Description Form: */
        setVSDForm(comForm);

        /* Finally, success: */
        fResult = true;
    }
    while (0);

    /* Return result: */
    return fResult;
}

bool UIWizardNewCloudVM::createCloudVM()
{
    /* Prepare result: */
    bool fResult = false;

    /* Main API request sequence, can be interrupted after any step: */
    do
    {
        /* Do nothing if prevented: */
        if (m_fFinalStepPrevented)
        {
            fResult = true;
            break;
        }

        /* Acquire prepared client and description: */
        CCloudClient comClient = client();
        CVirtualSystemDescription comVSD = vsd();
        AssertReturn(comClient.isNotNull() && comVSD.isNotNull(), false);

        /* Initiate cloud VM creation procedure: */
        CCloudMachine comMachine;
        CProgress comProgress = comClient.CreateCloudMachine(comVSD, comMachine);
        if (!comClient.isOk())
        {
            msgCenter().cannotCreateCloudMachine(comClient, this);
            break;
        }

        /* Show "Create Cloud Machine" progress: */
        msgCenter().showModalProgressDialog(comProgress, QString(), ":/progress_new_cloud_vm_90px.png", this, 0);
        if (comProgress.GetCanceled())
            break;
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
        {
            msgCenter().cannotCreateCloudMachine(comProgress, this);
            break;
        }

        /* Check whether VM really added: */
        if (comMachine.isNotNull())
            uiCommon().notifyCloudMachineRegistered(shortProviderName(),
                                                    profileName(),
                                                    comMachine);

        /* Finally, success: */
        fResult = true;
    }
    while (0);

    /* Return result: */
    return fResult;
}

void UIWizardNewCloudVM::scheduleAutoFinish()
{
    QMetaObject::invokeMethod(this, "sltTriggerFinishButton", Qt::QueuedConnection);
}

void UIWizardNewCloudVM::populatePages()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            if (m_fFullWizard)
                addPage(new UIWizardNewCloudVMPageBasic1);
            addPage(new UIWizardNewCloudVMPageBasic2);
            break;
        }
        case WizardMode_Expert:
        {
            addPage(new UIWizardNewCloudVMPageExpert(m_fFullWizard));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

void UIWizardNewCloudVM::retranslateUi()
{
    /* Call to base-class: */
    UINativeWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Create Cloud Virtual Machine"));
    /// @todo implement this?
    //setButtonText(QWizard::FinishButton, tr("Create"));
}

void UIWizardNewCloudVM::sltTriggerFinishButton()
{
    wizardButton(WizardButtonType_Next)->click();
}
