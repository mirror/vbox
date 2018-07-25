/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic3 class implementation.
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
# include <QJsonArray>
# include <QJsonDocument>
# include <QJsonObject>
# include <QJsonValue>
# include <QVBoxLayout>

/* GUI includes: */
# include "QILabelSeparator.h"
# include "QIRichTextLabel.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppPageBasic3.h"

/* COM includes: */
# include "CAppliance.h"
# include "CCloudClient.h"
# include "CCloudUserProfiles.h"
# include "CMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIWizardExportAppPage3 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardExportAppPage3::UIWizardExportAppPage3()
{
}

void UIWizardExportAppPage3::populateCloudClientParameters()
{
    /* Forget current parameters: */
    m_listCloudClientParameters.clear();

    /* Acquire Cloud User-profiles: */
    CCloudUserProfiles comCloudUserProfiles = fieldImp("profiles").value<CCloudUserProfiles>();
    AssertMsgReturnVoid(comCloudUserProfiles.isNotNull(),
                        ("Cloud User-profiles object is undefined!"));

    /* Create Cloud Client: */
    CCloudClient comCloudClient = comCloudUserProfiles.CreateCloudClient(fieldImp("profile").toString());
    AssertMsgReturnVoid(comCloudUserProfiles.isOk() && comCloudClient.isNotNull(),
                        ("Can't create Cloud Client object!"));

    /* Read Cloud Client parameters for Export VM operation: */
    const QString strJSON = comCloudClient.GetOperationParameters(KCloudOperation_exportVM);

    /* Create JSON document on the basis of it, make sure it isn't empty: */
    const QJsonDocument document = QJsonDocument::fromJson(strJSON.toUtf8());
    AssertMsgReturnVoid(!document.isEmpty(), ("Document is empty!"));

    /* Parse JSON document: */
    m_listCloudClientParameters = parseJsonDocument(document);
}

/* static */
QList<AbstractVSDParameter> UIWizardExportAppPage3::parseJsonDocument(const QJsonDocument &document)
{
    /* Prepare parameters: */
    QList<AbstractVSDParameter> parameters;

    /* Convert document to object, make sure it isn't empty: */
    QJsonObject documentObject = document.object();
    AssertMsgReturn(!documentObject.isEmpty(), ("Document object is empty!"), parameters);

    /* Iterate through document object values: */
    foreach (const QString &strElementName, documentObject.keys())
    {
        //printf("Element name: \"%s\"\n", strElementName.toUtf8().constData());

        /* Prepare parameter: */
        AbstractVSDParameter parameter;

        /* Assign name: */
        parameter.name = strElementName;

        /* Acquire element, make sure it's an object: */
        const QJsonValue element = documentObject.value(strElementName);
        AssertMsg(element.isObject(), ("Element '%s' has wrong structure!", strElementName.toUtf8().constData()));
        if (!element.isObject())
            continue;

        /* Convert element to object, make sure it isn't empty: */
        const QJsonObject elementObject = element.toObject();
        AssertMsg(!elementObject.isEmpty(), ("Element '%s' object has wrong structure!", strElementName.toUtf8().constData()));
        if (elementObject.isEmpty())
            continue;

        /* Iterate through element object values: */
        foreach (const QString &strFieldName, elementObject.keys())
        {
            //printf(" Field name: \"%s\"\n", strFieldName.toUtf8().constData());

            /* Acquire field: */
            const QJsonValue field = elementObject.value(strFieldName);

            /* Parse known fields: */
            if (strFieldName == "type")
                parameter.type = (KVirtualSystemDescriptionType)(int)parseJsonFieldDouble(strFieldName, field);
            else
            if (strFieldName == "bool")
            {
                AbstractVSDParameterBool get;
                get.value = parseJsonFieldBool(strFieldName, field);
                parameter.get = QVariant::fromValue(get);
                parameter.kind = ParameterKind_Bool;
            }
            else
            if (strFieldName == "min")
            {
                AbstractVSDParameterDouble get = parameter.get.value<AbstractVSDParameterDouble>();
                get.minimum = parseJsonFieldDouble(strFieldName, field);
                parameter.get = QVariant::fromValue(get);
                parameter.kind = ParameterKind_Double;
            }
            else
            if (strFieldName == "max")
            {
                AbstractVSDParameterDouble get = parameter.get.value<AbstractVSDParameterDouble>();
                get.maximum = parseJsonFieldDouble(strFieldName, field);
                parameter.get = QVariant::fromValue(get);
                parameter.kind = ParameterKind_Double;
            }
            else
            if (strFieldName == "unit")
            {
                AbstractVSDParameterDouble get = parameter.get.value<AbstractVSDParameterDouble>();
                get.unit = parseJsonFieldString(strFieldName, field);
                parameter.get = QVariant::fromValue(get);
                parameter.kind = ParameterKind_Double;
            }
            else
            if (strFieldName == "items")
            {
                AbstractVSDParameterArray get;
                get.values = parseJsonFieldArray(strFieldName, field);
                parameter.get = QVariant::fromValue(get);
                parameter.kind = ParameterKind_Array;
            }
        }

        /* Append parameter: */
        parameters << parameter;
    }

    /* Return parameters: */
    return parameters;
}

/* static */
bool UIWizardExportAppPage3::parseJsonFieldBool(const QString &strFieldName, const QJsonValue &field)
{
    /* Make sure field is bool: */
    AssertMsgReturn(field.isBool(), ("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()), false);
    LogRel(("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()));
    const bool fFieldValue = field.toBool();
    //printf("  Field value: \"%s\"\n", fFieldValue ? "true" : "false");

    return fFieldValue;
}

/* static */
double UIWizardExportAppPage3::parseJsonFieldDouble(const QString &strFieldName, const QJsonValue &field)
{
    /* Make sure field is double: */
    AssertMsgReturn(field.isDouble(), ("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()), 0);
    LogRel(("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()));
    const double dFieldValue = field.toDouble();
    //printf("  Field value: \"%f\"\n", dFieldValue);

    return dFieldValue;
}

/* static */
QString UIWizardExportAppPage3::parseJsonFieldString(const QString &strFieldName, const QJsonValue &field)
{
    /* Make sure field is string: */
    AssertMsgReturn(field.isString(), ("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()), QString());
    LogRel(("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()));
    const QString strFieldValue = field.toString();
    //printf("  Field value: \"%s\"\n", strFieldValue.toUtf8().constData());

    return strFieldValue;
}

/* static */
QStringList UIWizardExportAppPage3::parseJsonFieldArray(const QString &strFieldName, const QJsonValue &field)
{
    /* Make sure field is array: */
    AssertMsgReturn(field.isArray(), ("Item '%s' has wrong structure!", strFieldName.toUtf8().constData()), QStringList());
    LogRel(("Field '%s' has wrong structure!", strFieldName.toUtf8().constData()));
    const QJsonArray fieldValueArray = field.toArray();
    QStringList fieldValueStirngList;
    for (int i = 0; i < fieldValueArray.count(); ++i)
        fieldValueStirngList << fieldValueArray[i].toString();
    //printf("  Field value: \"%s\"\n", fieldValueStirngList.join(", ").toUtf8().constData());

    return fieldValueStirngList;
}

void UIWizardExportAppPage3::refreshApplianceSettingsWidget()
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
                    if (fieldImp("format").toString() == "csp-1.0")
                    {
                        /* Populate Cloud Client parameters: */
                        populateCloudClientParameters();
                        /* Pass them as a list of hints to help editor with names/values: */
                        m_pApplianceWidget->setVsdHints(m_listCloudClientParameters);
                        /* Add corresponding Cloud Client fields with default values: */
                        foreach (const AbstractVSDParameter &parameter, m_listCloudClientParameters)
                        {
                            QString strValue;
                            switch (parameter.kind)
                            {
                                case ParameterKind_Bool:
                                    strValue = QString();
                                    break;
                                case ParameterKind_Double:
                                    strValue = QString::number(parameter.get.value<AbstractVSDParameterDouble>().minimum);
                                    break;
                                case ParameterKind_String:
                                    strValue = parameter.get.value<AbstractVSDParameterString>().value;
                                    break;
                                case ParameterKind_Array:
                                    strValue = parameter.get.value<AbstractVSDParameterArray>().values.value(0);
                                    break;
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
                return msgCenter().cannotFindMachineById(comVBox, uuid);
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
