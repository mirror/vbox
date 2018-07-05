/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic4 class implementation.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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

/* Qt includes: */
# include <QVBoxLayout>

/* GUI includes: */
# include "QILabelSeparator.h"
# include "QIRichTextLabel.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppPageBasic4.h"

/* COM includes: */
# include "CAppliance.h"
# include "CMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage4 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage4::UIWizardExportAppPage4()
{
}

void UIWizardExportAppPage4::refreshApplianceSettingsWidget()
{
    /* Refresh settings widget: */
    CVirtualBox comVBox = vboxGlobal().virtualBox();
    CAppliance *pAppliance = m_pApplianceWidget->init();
    if (pAppliance->isOk())
    {
        /* Iterate over all the selected machine ids: */
        QStringList uuids = fieldImp("machineIDs").toStringList();
        foreach (const QString &uuid, uuids)
        {
            /* Get the machine with the uuid: */
            CMachine comMachine = comVBox.FindMachine(uuid);
            if (comVBox.isOk() && comMachine.isNotNull())
            {
                /* Add the export description to our appliance object: */
                CVirtualSystemDescription comVsd = comMachine.ExportTo(*pAppliance, qobject_cast<UIWizardExportApp*>(wizardImp())->uri());
                if (comMachine.isOk() && comVsd.isNotNull())
                {
                    /* Now add some new fields the user may change: */
                    comVsd.AddDescription(KVirtualSystemDescriptionType_Product, "", "");
                    comVsd.AddDescription(KVirtualSystemDescriptionType_ProductUrl, "", "");
                    comVsd.AddDescription(KVirtualSystemDescriptionType_Vendor, "", "");
                    comVsd.AddDescription(KVirtualSystemDescriptionType_VendorUrl, "", "");
                    comVsd.AddDescription(KVirtualSystemDescriptionType_Version, "", "");
                    comVsd.AddDescription(KVirtualSystemDescriptionType_License, "", "");
                }
                else
                    return msgCenter().cannotExportAppliance(comMachine, pAppliance->GetPath(), thisImp());
            }
            else
                return msgCenter().cannotFindMachineById(comVBox, uuid);
        }
        /* Make sure the settings widget get the new descriptions: */
        m_pApplianceWidget->populate();
    }
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic4 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic4::UIWizardExportAppPageBasic4()
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel;
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create appliance widget: */
        m_pApplianceWidget = new UIApplianceExportEditorWidget;
        if (m_pApplianceWidget)
        {
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

            /* Add into layout: */
            pMainLayout->addWidget(m_pApplianceWidget);
        }
    }

    /* Register classes: */
    qRegisterMetaType<ExportAppliancePointer>();

    /* Register fields: */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardExportAppPageBasic4::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Appliance settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be added "
                                            "to the virtual appliance.  You can change it by double "
                                            "clicking on individual lines."));
}

void UIWizardExportAppPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Refresh appliance settings widget: */
    refreshApplianceSettingsWidget();
}

bool UIWizardExportAppPageBasic4::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to export appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardExportApp*>(wizard())->exportAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}
