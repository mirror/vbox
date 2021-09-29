/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class implementation.
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
#include <QStackedWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILabelSeparator.h"
#include "QIRichTextLabel.h"
#include "UIApplianceExportEditorWidget.h"
#include "UICommon.h"
#include "UIFormEditorWidget.h"
#include "UIMessageCenter.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageBasic3.h"

/* COM includes: */
#include "CMachine.h"
#include "CVirtualSystemDescriptionForm.h"

/* Namespaces: */
using namespace UIWizardExportAppPage3;


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage3 implementation.                                                                                 *
*********************************************************************************************************************************/

void UIWizardExportAppPage3::refreshStackedWidget(QStackedWidget *pStackedWidget,
                                                  bool fIsFormatCloudOne)
{
    /* Update stack appearance according to chosen format: */
    pStackedWidget->setCurrentIndex((int)fIsFormatCloudOne);
}

void UIWizardExportAppPage3::refreshApplianceSettingsWidget(UIApplianceExportEditorWidget *pApplianceWidget,
                                                            const QList<QUuid> &machineIDs,
                                                            const QString &strUri,
                                                            bool fIsFormatCloudOne)
{
    /* Nothing for cloud case? */
    if (fIsFormatCloudOne)
        return;

    /* Acquire appliance: */
    CAppliance *pAppliance = pApplianceWidget->init();
    if (pAppliance->isOk())
    {
        /* Iterate over all the selected machine uuids: */
        foreach (const QUuid &uMachineId, machineIDs)
        {
            /* Get the machine with the uMachineId: */
            CVirtualBox comVBox = uiCommon().virtualBox();
            CMachine comMachine = comVBox.FindMachine(uMachineId.toString());
            if (comVBox.isOk() && comMachine.isNotNull())
            {
                /* Add the export description to our appliance object: */
                CVirtualSystemDescription comVsd = comMachine.ExportTo(*pAppliance, strUri);
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
                    return msgCenter().cannotExportAppliance(comMachine, pAppliance->GetPath());
            }
            else
                return msgCenter().cannotFindMachineById(comVBox, uMachineId);
        }
        /* Make sure the settings widget get the new descriptions: */
        pApplianceWidget->populate();
    }
}

void UIWizardExportAppPage3::refreshFormPropertiesTable(UIFormEditorWidget *pFormEditor,
                                                        const CVirtualSystemDescriptionForm &comVsdExportForm,
                                                        bool fIsFormatCloudOne)
{
    /* Nothing for local case? */
    if (!fIsFormatCloudOne)
        return;

    /* Make sure the properties table get the new description form: */
    if (comVsdExportForm.isNotNull())
        pFormEditor->setVirtualSystemDescriptionForm(comVsdExportForm);
}


/*********************************************************************************************************************************
*   Class UIWizardExportAppPageBasic3 implementation.                                                                            *
*********************************************************************************************************************************/

UIWizardExportAppPageBasic3::UIWizardExportAppPageBasic3()
    : m_pLabel(0)
    , m_pSettingsWidget2(0)
    , m_pApplianceWidget(0)
    , m_pFormEditor(0)
{
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
            pMainLayout->addWidget(m_pLabel);

        /* Create settings widget 2: */
        m_pSettingsWidget2 = new QStackedWidget(this);
        if (m_pSettingsWidget2)
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
                    m_pApplianceWidget = new UIApplianceExportEditorWidget(pApplianceWidgetCnt);
                    if (m_pApplianceWidget)
                    {
                        m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
                        pApplianceWidgetLayout->addWidget(m_pApplianceWidget);
                    }
                }

                /* Add into layout: */
                m_pSettingsWidget2->addWidget(pApplianceWidgetCnt);
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
                        pFormEditorLayout->addWidget(m_pFormEditor);
                }

                /* Add into layout: */
                m_pSettingsWidget2->addWidget(pFormEditorCnt);
            }

            /* Add into layout: */
            pMainLayout->addWidget(m_pSettingsWidget2);
        }
    }
}

UIWizardExportApp *UIWizardExportAppPageBasic3::wizard() const
{
    return qobject_cast<UIWizardExportApp*>(UINativeWizardPage::wizard());
}

void UIWizardExportAppPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardExportApp::tr("Virtual system settings"));

    /* Translate label: */
    if (wizard()->isFormatCloudOne())
        m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be used to determine settings "
                                                "for a cloud storage your VM being exported to.  You can change it by double "
                                                "clicking on individual lines."));
    else
        m_pLabel->setText(UIWizardExportApp::tr("This is the descriptive information which will be added to the virtual "
                                                "appliance.  You can change it by double clicking on individual lines."));
}

void UIWizardExportAppPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Refresh settings widget state: */
    refreshStackedWidget(m_pSettingsWidget2, wizard()->isFormatCloudOne());
    /* Refresh corresponding widgets: */
    refreshApplianceSettingsWidget(m_pApplianceWidget, wizard()->machineIDs(), wizard()->uri(), wizard()->isFormatCloudOne());
    refreshFormPropertiesTable(m_pFormEditor, wizard()->vsdExportForm(), wizard()->isFormatCloudOne());

    /* Choose initially focused widget: */
    if (wizard()->isFormatCloudOne())
        m_pFormEditor->setFocus();
    else
        m_pApplianceWidget->setFocus();
}

bool UIWizardExportAppPageBasic3::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Check whether there was cloud target selected: */
    if (wizard()->isFormatCloudOne())
    {
        /* Make sure table has own data committed: */
        m_pFormEditor->makeSureEditorDataCommitted();

        /* Check whether we have proper VSD form: */
        CVirtualSystemDescriptionForm comForm = wizard()->vsdExportForm();
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
    /* Otherwise if there was local target selected: */
    else
    {
        /* Prepare export: */
        m_pApplianceWidget->prepareExport();
        wizard()->setLocalAppliance(*m_pApplianceWidget->appliance());
    }

    /* Try to export appliance: */
    if (fResult)
        fResult = wizard()->exportAppliance();

    /* Return result: */
    return fResult;
}
