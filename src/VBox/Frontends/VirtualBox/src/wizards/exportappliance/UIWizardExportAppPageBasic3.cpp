/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class implementation.
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

/* Qt includes: */
#include <QVBoxLayout>

/* GUI includes: */
#include "QILabelSeparator.h"
#include "QIRichTextLabel.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageBasic3.h"

/* COM includes: */
#include "CAppliance.h"
#include "CMachine.h"


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage3 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage3::UIWizardExportAppPage3()
{
}

void UIWizardExportAppPage3::refreshApplianceSettingsWidget()
{
    /* Acquire appliance: */
    CAppliance *pAppliance = m_pApplianceWidget->init();
    if (pAppliance->isOk())
    {
        /* Iterate over all the selected machine uuids: */
        const QList<QUuid> uuids = fieldImp("machineIDs").value<QList<QUuid> >();
        foreach (const QUuid &uMachineId, uuids)
        {
            /* Get the machine with the uMachineId: */
            CVirtualBox comVBox = vboxGlobal().virtualBox();
            CMachine comMachine = comVBox.FindMachine(uMachineId.toString());
            if (comVBox.isOk() && comMachine.isNotNull())
            {
                /* Add the export description to our appliance object: */
                CVirtualSystemDescription comVsd = comMachine.ExportTo(*pAppliance, qobject_cast<UIWizardExportApp*>(wizardImp())->uri());
                if (comMachine.isOk() && comVsd.isNotNull())
                {
                    if (fieldImp("isFormatCloudOne").toBool())
                    {
                        /* Acquire Cloud Client parameters: */
                        const AbstractVSDParameterList cloudClientParameters =
                            fieldImp("cloudClientParameters").value<AbstractVSDParameterList>();
                        /* Pass them as a list of hints to help editor with names/values: */
                        m_pApplianceWidget->setVsdHints(cloudClientParameters);
                        /* Add corresponding Cloud Client fields with default values: */
                        foreach (const AbstractVSDParameter &parameter, cloudClientParameters)
                        {
                            QString strValue;
                            switch (parameter.kind)
                            {
                                case ParameterKind_Bool:
                                    strValue = QString("true");
                                    break;
                                case ParameterKind_Double:
                                    strValue = QString::number(parameter.get.value<AbstractVSDParameterDouble>().minimum);
                                    break;
                                case ParameterKind_String:
                                    strValue = parameter.get.value<AbstractVSDParameterString>().value;
                                    break;
                                case ParameterKind_Array:
                                {
                                    const QString strFirst = parameter.get.value<AbstractVSDParameterArray>().values.value(0).first;
                                    const QString strSecond = parameter.get.value<AbstractVSDParameterArray>().values.value(0).second;
                                    strValue = strSecond.isNull() ? strFirst : strSecond;
                                    break;
                                }
                                default:
                                    break;
                            }
                            comVsd.AddDescription(parameter.type, strValue, "");
                        }
                    }
                    else
                    {
                        /* Add some additional fields the user may change: */
                        comVsd.AddDescription(KVirtualSystemDescriptionType_Product, "", "");
                        comVsd.AddDescription(KVirtualSystemDescriptionType_ProductUrl, "", "");
                        comVsd.AddDescription(KVirtualSystemDescriptionType_Vendor, "", "");
                        comVsd.AddDescription(KVirtualSystemDescriptionType_VendorUrl, "", "");
                        comVsd.AddDescription(KVirtualSystemDescriptionType_Version, "", "");
                        comVsd.AddDescription(KVirtualSystemDescriptionType_License, "", "");
                    }
                }
                else
                    return msgCenter().cannotExportAppliance(comMachine, pAppliance->GetPath(), thisImp());
            }
            else
                return msgCenter().cannotFindMachineById(comVBox, uMachineId);
        }
        /* Make sure the settings widget get the new descriptions: */
        m_pApplianceWidget->populate();
    }
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic3 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic3::UIWizardExportAppPageBasic3()
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

void UIWizardExportAppPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Virtual system settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be added "
                                            "to the virtual appliance.  You can change it by double "
                                            "clicking on individual lines."));
}

void UIWizardExportAppPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Refresh appliance settings widget: */
    refreshApplianceSettingsWidget();
}

bool UIWizardExportAppPageBasic3::validatePage()
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
