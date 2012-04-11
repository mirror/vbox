/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportAppPageBasic4 class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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
#include <QVBoxLayout>

/* Local includes: */
#include "UIWizardExportAppPageBasic4.h"
#include "UIWizardExportApp.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "QIRichTextLabel.h"

UIWizardExportAppPageBasic4::UIWizardExportAppPageBasic4()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pApplianceWidget = new UIApplianceExportEditorWidget(this);
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pApplianceWidget);

    /* Register ExportAppliancePointer class: */
    qRegisterMetaType<ExportAppliancePointer>();
    /* Register 'applianceWidget' field! */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardExportAppPageBasic4::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Appliance Export Settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("Here you can change additional configuration "
                                            "values of the selected virtual machines. "
                                            "You can modify most of the properties shown "
                                            "by double-clicking on the items."));
}

void UIWizardExportAppPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Prepare settings widget: */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    CAppliance *pAppliance = m_pApplianceWidget->init();
    bool fResult = pAppliance->isOk();
    if (fResult)
    {
        /* Iterate over all the selected machine ids: */
        QStringList uuids = field("machineIDs").toStringList();
        foreach (const QString &uuid, uuids)
        {
            /* Get the machine with the uuid: */
            CMachine machine = vbox.FindMachine(uuid);
            fResult = machine.isOk();
            if (fResult)
            {
                /* Add the export description to our appliance object: */
                CVirtualSystemDescription vsd = machine.Export(*pAppliance, qobject_cast<UIWizardExportApp*>(wizard())->uri());
                fResult = machine.isOk();
                if (!fResult)
                {
                    msgCenter().cannotExportAppliance(machine, pAppliance, this);
                    return;
                }
                /* Now add some new fields the user may change: */
                vsd.AddDescription(KVirtualSystemDescriptionType_Product, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_ProductUrl, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_Vendor, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_VendorUrl, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_Version, "", "");
                vsd.AddDescription(KVirtualSystemDescriptionType_License, "", "");
            }
            else
                break;
        }
        /* Make sure the settings widget get the new descriptions: */
        m_pApplianceWidget->populate();
    }
    if (!fResult)
        msgCenter().cannotExportAppliance(pAppliance, this);
}

bool UIWizardExportAppPageBasic4::validatePage()
{
    /* Try to export appliance: */
    startProcessing();
    bool fResult = qobject_cast<UIWizardExportApp*>(wizard())->exportAppliance();
    endProcessing();
    return fResult;
}

