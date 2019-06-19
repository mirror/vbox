/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewCloudVM class implementation.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#include "UIWizardNewCloudVM.h"
#include "UIWizardNewCloudVMPageBasic1.h"
#include "UIWizardNewCloudVMPageBasic2.h"
#include "UIWizardNewCloudVMPageExpert.h"

/* COM includes: */
#include "CProgress.h"


UIWizardNewCloudVM::UIWizardNewCloudVM(QWidget *pParent, bool fImportFromOCIByDefault)
    : UIWizard(pParent, WizardType_ImportAppliance)
    , m_fImportFromOCIByDefault(fImportFromOCIByDefault)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_ovf_import.png");
#else
    /* Assign background image: */
    assignBackground(":/wizard_ovf_import_bg.png");
#endif
}

void UIWizardNewCloudVM::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardNewCloudVMPageBasic1(m_fImportFromOCIByDefault));
            setPage(Page2, new UIWizardNewCloudVMPageBasic2);
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardNewCloudVMPageExpert(m_fImportFromOCIByDefault));
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

bool UIWizardNewCloudVM::importAppliance()
{
    /* Acquire prepared appliance: */
    CAppliance comAppliance = field("appliance").value<CAppliance>();
    AssertReturn(!comAppliance.isNull(), false);

    /* No options for cloud VMs for now: */
    QVector<KImportOptions> options;

    /* Initiate import porocedure: */
    CProgress comProgress = comAppliance.ImportMachines(options);

    /* Show error message if necessary: */
    if (!comAppliance.isOk())
        msgCenter().cannotImportAppliance(comAppliance, this);
    else
    {
        /* Show "Import appliance" progress: */
        msgCenter().showModalProgressDialog(comProgress, tr("Importing Appliance ..."), ":/progress_import_90px.png", this, 0);
        if (!comProgress.GetCanceled())
        {
            /* Show error message if necessary: */
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                msgCenter().cannotImportAppliance(comProgress, comAppliance.GetPath(), this);
            else
                return true;
        }
    }

    /* Failure by default: */
    return false;
}

void UIWizardNewCloudVM::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Import Virtual Appliance"));
    setButtonText(QWizard::FinishButton, tr("Import"));
}
