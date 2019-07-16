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
#include <QStackedLayout>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILabelSeparator.h"
#include "QIRichTextLabel.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageBasic3.h"

/* COM includes: */
#include "CAppliance.h"
#include "CMachine.h"
#include "CVirtualSystemDescriptionForm.h"


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage3 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage3::UIWizardExportAppPage3()
    : m_pSettingsCntLayout(0)
{
}

void UIWizardExportAppPage3::updatePageAppearance()
{
    /* Check whether there was cloud target selected: */
    const bool fIsFormatCloudOne = fieldImp("isFormatCloudOne").toBool();
    /* Update page appearance according to chosen source: */
    m_pSettingsCntLayout->setCurrentIndex((int)fIsFormatCloudOne);
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
            CVirtualBox comVBox = uiCommon().virtualBox();
            CMachine comMachine = comVBox.FindMachine(uMachineId.toString());
            if (comVBox.isOk() && comMachine.isNotNull())
            {
                /* Add the export description to our appliance object: */
                CVirtualSystemDescription comVsd = comMachine.ExportTo(*pAppliance, qobject_cast<UIWizardExportApp*>(wizardImp())->uri());
                if (comMachine.isOk() && comVsd.isNotNull())
                {
                    /* Add some additional fields the user may change: */
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
                return msgCenter().cannotFindMachineById(comVBox, uMachineId);
        }
        /* Make sure the settings widget get the new descriptions: */
        m_pApplianceWidget->populate();
    }
}

void UIWizardExportAppPage3::refreshFormPropertiesTable()
{
    /* Acquire VSD form: */
    CVirtualSystemDescriptionForm comForm = fieldImp("vsdExportForm").value<CVirtualSystemDescriptionForm>();
    /* Make sure the properties table get the new description form: */
    if (comForm.isNotNull())
        m_pFormEditor->setVirtualSystemDescriptionForm(comForm);
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

        /* Create settings container layout: */
        m_pSettingsCntLayout = new QStackedLayout;
        if (m_pSettingsCntLayout)
        {
            /* Create appliance widget container: */
            QWidget *pApplianceWidgetCnt = new QWidget(this);
            if (pApplianceWidgetCnt)
            {
                /* Create appliance widget layout: */
                QVBoxLayout *pApplianceWidgetLayout = new QVBoxLayout(pApplianceWidgetCnt);
                if (pApplianceWidgetLayout)
                {
                    pApplianceWidgetLayout->setContentsMargins(0, 0, 0, 0);

                    /* Create appliance widget: */
                    m_pApplianceWidget = new UIApplianceExportEditorWidget;
                    if (m_pApplianceWidget)
                    {
                        m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

                        /* Add into layout: */
                        pApplianceWidgetLayout->addWidget(m_pApplianceWidget);
                    }
                }

                /* Add into layout: */
                m_pSettingsCntLayout->addWidget(pApplianceWidgetCnt);
            }

            /* Create form editor container: */
            QWidget *pFormEditorCnt = new QWidget(this);
            if (pFormEditorCnt)
            {
                /* Create form editor layout: */
                QVBoxLayout *pFormEditorLayout = new QVBoxLayout(pFormEditorCnt);
                if (pFormEditorLayout)
                {
                    pFormEditorLayout->setContentsMargins(0, 0, 0, 0);

                    /* Create form editor widget: */
                    m_pFormEditor = new UIFormEditorWidget(pFormEditorCnt);
                    if (m_pFormEditor)
                    {
                        /* Add into layout: */
                        pFormEditorLayout->addWidget(m_pFormEditor);
                    }
                }

                /* Add into layout: */
                m_pSettingsCntLayout->addWidget(pFormEditorCnt);
            }

            /* Add into layout: */
            pMainLayout->addLayout(m_pSettingsCntLayout);
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

    /* Update page appearance: */
    updatePageAppearance();
}

void UIWizardExportAppPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Check whether there was cloud target selected: */
    const bool fIsFormatCloudOne = field("isFormatCloudOne").toBool();
    if (fIsFormatCloudOne)
        refreshFormPropertiesTable();
    else
        refreshApplianceSettingsWidget();
}

bool UIWizardExportAppPageBasic3::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Check whether there was cloud target selected: */
    const bool fIsFormatCloudOne = fieldImp("isFormatCloudOne").toBool();
    if (fIsFormatCloudOne)
    {
        /* Make sure table has own data committed: */
        m_pFormEditor->makeSureEditorDataCommitted();

        /* Check whether we have proper VSD form: */
        CVirtualSystemDescriptionForm comForm = fieldImp("vsdExportForm").value<CVirtualSystemDescriptionForm>();
        fResult = comForm.isNotNull();
        Assert(fResult);

        /* Give changed VSD back to appliance: */
        if (fResult)
        {
            comForm.GetVirtualSystemDescription();
            fResult = comForm.isOk();
            if (!fResult)
                msgCenter().cannotAcquireVirtualSystemDescriptionFormProperty(comForm);
        }
    }

    /* Try to export appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardExportApp*>(wizard())->exportAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

void UIWizardExportAppPageBasic3::updatePageAppearance()
{
    /* Call to base-class: */
    UIWizardExportAppPage3::updatePageAppearance();

    /* Check whether there was cloud target selected: */
    const bool fIsFormatCloudOne = field("isFormatCloudOne").toBool();
    if (fIsFormatCloudOne)
    {
        m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be used to determine settings "
                                                "for a cloud storage your VM being exported to.  You can change it by double "
                                                "clicking on individual lines."));
        m_pFormEditor->setFocus();
    }
    else
    {
        m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be added to the virtual "
                                                "appliance.  You can change it by double clicking on individual lines."));
        m_pApplianceWidget->setFocus();
    }
}
