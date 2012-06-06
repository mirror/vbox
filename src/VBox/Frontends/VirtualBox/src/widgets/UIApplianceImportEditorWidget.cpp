/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIApplianceImportEditorWidget class implementation
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
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
#include "UIApplianceImportEditorWidget.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAppliance.h"

////////////////////////////////////////////////////////////////////////////////
// ImportSortProxyModel

class ImportSortProxyModel: public VirtualSystemSortProxyModel
{
public:
    ImportSortProxyModel(QObject *pParent = NULL)
      : VirtualSystemSortProxyModel(pParent)
    {
        m_filterList << KVirtualSystemDescriptionType_License;
    }
};

////////////////////////////////////////////////////////////////////////////////
// UIApplianceImportEditorWidget

UIApplianceImportEditorWidget::UIApplianceImportEditorWidget(QWidget *pParent)
    : UIApplianceEditorWidget(pParent)
{
    /* Show the MAC check box */
    m_pReinitMACsCheckBox->setHidden(false);
}

bool UIApplianceImportEditorWidget::setFile(const QString& strFile)
{
    bool fResult = false;
    if (!strFile.isEmpty())
    {
        CProgress progress;
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Create a appliance object */
        m_pAppliance = new CAppliance(vbox.CreateAppliance());
        fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Read the appliance */
            progress = m_pAppliance->Read(strFile);
            fResult = m_pAppliance->isOk();
            if (fResult)
            {
                /* Show some progress, so the user know whats going on */
                msgCenter().showModalProgressDialog(progress, tr("Reading Appliance ..."), "", this);
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    fResult = false;
                else
                {
                    /* Now we have to interpret that stuff */
                    m_pAppliance->Interpret();
                    fResult = m_pAppliance->isOk();
                    if (fResult)
                    {
                        if (m_pModel)
                            delete m_pModel;

                        QVector<CVirtualSystemDescription> vsds = m_pAppliance->GetVirtualSystemDescriptions();

                        m_pModel = new VirtualSystemModel(vsds, this);

                        ImportSortProxyModel *pProxy = new ImportSortProxyModel(this);
                        pProxy->setSourceModel(m_pModel);
                        pProxy->sort(DescriptionSection, Qt::DescendingOrder);

                        VirtualSystemDelegate *pDelegate = new VirtualSystemDelegate(pProxy, this);

                        /* Set our own model */
                        m_pTvSettings->setModel(pProxy);
                        /* Set our own delegate */
                        m_pTvSettings->setItemDelegate(pDelegate);
                        /* For now we hide the original column. This data is displayed as tooltip
                           also. */
                        m_pTvSettings->setColumnHidden(OriginalValueSection, true);
                        m_pTvSettings->expandAll();

                        /* Check for warnings & if there are one display them. */
                        bool fWarningsEnabled = false;
                        QVector<QString> warnings = m_pAppliance->GetWarnings();
                        if (warnings.size() > 0)
                        {
                            foreach (const QString& text, warnings)
                                mWarningTextEdit->append("- " + text);
                            fWarningsEnabled = true;
                        }
                        m_pWarningWidget->setShown(fWarningsEnabled);
                    }
                }
            }
        }
        if (!fResult)
        {
            if (progress.isNull())
                msgCenter().cannotImportAppliance(m_pAppliance, this);
            else
                msgCenter().cannotImportAppliance(progress, m_pAppliance, this);
            /* Delete the appliance in a case of an error */
            delete m_pAppliance;
            m_pAppliance = NULL;
        }
    }
    return fResult;
}

void UIApplianceImportEditorWidget::prepareImport()
{
    if (m_pAppliance)
        m_pModel->putBack();
}

bool UIApplianceImportEditorWidget::import()
{
    if (m_pAppliance)
    {
        /* Start the import asynchronously */
        CProgress progress;
        QVector<KImportOptions> options;
        if (!m_pReinitMACsCheckBox->isChecked())
            options.append(KImportOptions_KeepAllMACs);
        progress = m_pAppliance->ImportMachines(options);
        bool fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            msgCenter().showModalProgressDialog(progress, tr("Importing Appliance ..."), ":/progress_import_90px.png", this, true);
            if (progress.GetCanceled())
                return false;
            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotImportAppliance(progress, m_pAppliance, this);
                return false;
            }
            else
                return true;
        }
        if (!fResult)
            msgCenter().cannotImportAppliance(m_pAppliance, this);
    }
    return false;
}

QList<QPair<QString, QString> > UIApplianceImportEditorWidget::licenseAgreements() const
{
    QList<QPair<QString, QString> > list;

    CVirtualSystemDescriptionVector vsds = m_pAppliance->GetVirtualSystemDescriptions();
    for (int i = 0; i < vsds.size(); ++i)
    {
        QVector<QString> strLicense;
        strLicense = vsds[i].GetValuesByType(KVirtualSystemDescriptionType_License,
                                             KVirtualSystemDescriptionValueType_Original);
        if (!strLicense.isEmpty())
        {
            QVector<QString> strName;
            strName = vsds[i].GetValuesByType(KVirtualSystemDescriptionType_Name,
                                              KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString>(strName.first(), strLicense.first());
        }
    }

    return list;
}

