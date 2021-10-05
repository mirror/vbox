/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class implementation.
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
#include <QTextEdit>

/* GUI includes: */
#include "QITreeView.h"
#include "UIApplianceImportEditorWidget.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UINotificationCenter.h"
#include "UIWizardImportApp.h"

/* COM includes: */
#include "CAppliance.h"


/** UIApplianceSortProxyModel subclass for Export Appliance wizard. */
class ImportSortProxyModel : public UIApplianceSortProxyModel
{
public:

    /** Constructs proxy model passing @a pParent to the base-class. */
    ImportSortProxyModel(QObject *pParent = 0)
        : UIApplianceSortProxyModel(pParent)
    {
        m_aFilteredList << KVirtualSystemDescriptionType_License;
    }
};


/*********************************************************************************************************************************
*   Class UIApplianceImportEditorWidget implementation.                                                                          *
*********************************************************************************************************************************/

UIApplianceImportEditorWidget::UIApplianceImportEditorWidget(QWidget *pParent)
    : UIApplianceEditorWidget(pParent)
{
}

bool UIApplianceImportEditorWidget::setFile(const QString& strFile)
{
    bool fResult = false;
    if (!strFile.isEmpty())
    {
        CProgress comProgress;

        /* Create a appliance object: */
        CVirtualBox comVBox = uiCommon().virtualBox();
        m_pAppliance = new CAppliance(comVBox.CreateAppliance());
        fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Read the appliance: */
            comProgress = m_pAppliance->Read(strFile);
            fResult = m_pAppliance->isOk();
            if (fResult)
            {
                /* Show some progress, so the user know whats going on: */
                msgCenter().showModalProgressDialog(comProgress, tr("Reading Appliance ..."),
                                                    ":/progress_reading_appliance_90px.png", this);
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    fResult = false;
                else
                {
                    /* Now we have to interpret that stuff: */
                    m_pAppliance->Interpret();
                    fResult = m_pAppliance->isOk();
                    if (fResult)
                    {
                        /* Cleanup previous stuff: */
                        if (m_pModel)
                            delete m_pModel;

                        /* Prepare model: */
                        QVector<CVirtualSystemDescription> vsds = m_pAppliance->GetVirtualSystemDescriptions();
                        m_pModel = new UIApplianceModel(vsds, m_pTreeViewSettings);
                        if (m_pModel)
                        {
                            /* Create proxy model: */
                            ImportSortProxyModel *pProxy = new ImportSortProxyModel(m_pModel);
                            if (pProxy)
                            {
                                pProxy->setSourceModel(m_pModel);
                                pProxy->sort(ApplianceViewSection_Description, Qt::DescendingOrder);

                                /* Set our own model */
                                m_pTreeViewSettings->setModel(pProxy);
                                /* Set our own delegate */
                                UIApplianceDelegate *pDelegate = new UIApplianceDelegate(pProxy);
                                if (pDelegate)
                                    m_pTreeViewSettings->setItemDelegate(pDelegate);

                                /* For now we hide the original column. This data is displayed as tooltip also. */
                                m_pTreeViewSettings->setColumnHidden(ApplianceViewSection_OriginalValue, true);
                                m_pTreeViewSettings->expandAll();
                                /* Set model root index and make it current: */
                                m_pTreeViewSettings->setRootIndex(pProxy->mapFromSource(m_pModel->root()));
                                m_pTreeViewSettings->setCurrentIndex(pProxy->mapFromSource(m_pModel->root()));
                            }
                        }

                        /* Check for warnings & if there are one display them: */
                        const QVector<QString> warnings = m_pAppliance->GetWarnings();
                        bool fWarningsEnabled = warnings.size() > 0;
                        foreach (const QString &strText, warnings)
                            m_pTextEditWarning->append("- " + strText);
                        m_pPaneWarning->setVisible(fWarningsEnabled);
                    }
                }
            }
        }

        if (!fResult)
        {
            if (!m_pAppliance->isOk())
                msgCenter().cannotImportAppliance(*m_pAppliance, this);
            else if (!comProgress.isNull() && (!comProgress.isOk() || comProgress.GetResultCode() != 0))
                msgCenter().cannotImportAppliance(comProgress, m_pAppliance->GetPath(), this);
            /* Delete the appliance in a case of an error: */
            delete m_pAppliance;
            m_pAppliance = 0;
        }
    }

    return fResult;
}

void UIApplianceImportEditorWidget::prepareImport()
{
    if (m_pAppliance)
        m_pModel->putBack();
}

bool UIApplianceImportEditorWidget::import(const QVector<KImportOptions> &options)
{
    if (m_pAppliance)
    {
        /* Import appliance: */
        UINotificationProgressApplianceImport *pNotification = new UINotificationProgressApplianceImport(*m_pAppliance,
                                                                                                         options);
        gpNotificationCenter->append(pNotification);

        /* Positive: */
        return true;
    }
    return false;
}

QList<QPair<QString, QString> > UIApplianceImportEditorWidget::licenseAgreements() const
{
    QList<QPair<QString, QString> > list;

    foreach (CVirtualSystemDescription comVsd, m_pAppliance->GetVirtualSystemDescriptions())
    {
        QVector<QString> strLicense;
        strLicense = comVsd.GetValuesByType(KVirtualSystemDescriptionType_License,
                                            KVirtualSystemDescriptionValueType_Original);
        if (!strLicense.isEmpty())
        {
            QVector<QString> strName;
            strName = comVsd.GetValuesByType(KVirtualSystemDescriptionType_Name,
                                             KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString>(strName.first(), strLicense.first());
        }
    }

    return list;
}
