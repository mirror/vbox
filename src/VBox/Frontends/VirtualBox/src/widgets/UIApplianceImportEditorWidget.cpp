/* $Id$ */
/** @file
 * VBox Qt GUI - UIApplianceImportEditorWidget class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
# include <QCheckBox>
# include <QLabel>
# include <QTextEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIRichTextLabel.h"
# include "QITreeView.h"
# include "UIApplianceImportEditorWidget.h"
# include "UIFilePathSelector.h"
# include "UIMessageCenter.h"
# include "UIWizardImportApp.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CAppliance.h"
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


////////////////////////////////////////////////////////////////////////////////
// ImportSortProxyModel

class ImportSortProxyModel: public UIApplianceSortProxyModel
{
public:
    ImportSortProxyModel(QObject *pParent = NULL)
      : UIApplianceSortProxyModel(pParent)
    {
        m_aFilteredList << KVirtualSystemDescriptionType_License;
    }
};

////////////////////////////////////////////////////////////////////////////////
// UIApplianceImportEditorWidget

UIApplianceImportEditorWidget::UIApplianceImportEditorWidget(QWidget *pParent)
    : UIApplianceEditorWidget(pParent)
    , m_pPathSelectorLabel(0)
    , m_pPathSelector(0)
    , m_pCheckBoxReinitMACs(0)
    , m_pImportHDsAsVDI(0)

{
    prepareWidgets();
}

void UIApplianceImportEditorWidget::prepareWidgets()
{
    m_pPathSelectorLabel = new QIRichTextLabel(this);
    if (m_pPathSelectorLabel)
    {
        m_pLayout->addWidget(m_pPathSelectorLabel);
    }
    m_pPathSelector = new UIFilePathSelector(this);
    if (m_pPathSelector)
    {
        m_pPathSelector->setResetEnabled(true);
        m_pPathSelector->setDefaultPath(vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
        m_pPathSelector->setPath(vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder());
        connect(m_pPathSelector, &UIFilePathSelector::pathChanged, this, &UIApplianceImportEditorWidget::sltHandlePathChanged);
        m_pLayout->addWidget(m_pPathSelector);
    }
    m_pCheckBoxReinitMACs = new QCheckBox;
    {
        m_pLayout->addWidget(m_pCheckBoxReinitMACs);
    }

    m_pImportHDsAsVDI = new QCheckBox;
    {
        m_pLayout->addWidget(m_pImportHDsAsVDI);
        m_pImportHDsAsVDI->setCheckState(Qt::Checked);
    }

    retranslateUi();
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
                msgCenter().showModalProgressDialog(progress, tr("Reading Appliance ..."), ":/progress_reading_appliance_90px.png", this);
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

                        m_pModel = new UIApplianceModel(vsds, m_pTreeViewSettings);

                        ImportSortProxyModel *pProxy = new ImportSortProxyModel(this);
                        pProxy->setSourceModel(m_pModel);
                        pProxy->sort(ApplianceViewSection_Description, Qt::DescendingOrder);

                        UIApplianceDelegate *pDelegate = new UIApplianceDelegate(pProxy, this);

                        /* Set our own model */
                        m_pTreeViewSettings->setModel(pProxy);
                        /* Set our own delegate */
                        m_pTreeViewSettings->setItemDelegate(pDelegate);
                        /* For now we hide the original column. This data is displayed as tooltip
                           also. */
                        m_pTreeViewSettings->setColumnHidden(ApplianceViewSection_OriginalValue, true);
                        m_pTreeViewSettings->expandAll();
                        /* Set model root index and make it current: */
                        m_pTreeViewSettings->setRootIndex(pProxy->mapFromSource(m_pModel->root()));
                        m_pTreeViewSettings->setCurrentIndex(pProxy->mapFromSource(m_pModel->root()));

                        /* Check for warnings & if there are one display them. */
                        bool fWarningsEnabled = false;
                        QVector<QString> warnings = m_pAppliance->GetWarnings();
                        if (warnings.size() > 0)
                        {
                            foreach (const QString& text, warnings)
                                m_pTextEditWarning->append("- " + text);
                            fWarningsEnabled = true;
                        }
                        m_pPaneWarning->setVisible(fWarningsEnabled);
                    }
                }
            }
        }
        if (!fResult)
        {
            if (!m_pAppliance->isOk())
                msgCenter().cannotImportAppliance(*m_pAppliance, this);
            else if (!progress.isNull() && (!progress.isOk() || progress.GetResultCode() != 0))
                msgCenter().cannotImportAppliance(progress, m_pAppliance->GetPath(), this);
            /* Delete the appliance in a case of an error */
            delete m_pAppliance;
            m_pAppliance = NULL;
        }
    }
    /* Make sure we initialize model items with correct base folder path: */
    if (m_pPathSelector)
        sltHandlePathChanged(m_pPathSelector->path());

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
        if (!m_pCheckBoxReinitMACs->isChecked())
            options.append(KImportOptions_KeepAllMACs);
        if (m_pImportHDsAsVDI->isChecked())
            options.append(KImportOptions_ImportToVDI);
        progress = m_pAppliance->ImportMachines(options);
        bool fResult = m_pAppliance->isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            msgCenter().showModalProgressDialog(progress, tr("Importing Appliance ..."), ":/progress_import_90px.png", this);
            if (progress.GetCanceled())
                return false;
            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                msgCenter().cannotImportAppliance(progress, m_pAppliance->GetPath(), this);
                return false;
            }
            else
                return true;
        }
        if (!fResult)
            msgCenter().cannotImportAppliance(*m_pAppliance, this);
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

void UIApplianceImportEditorWidget::retranslateUi()
{
    UIApplianceEditorWidget::retranslateUi();
    if (m_pPathSelectorLabel)
        m_pPathSelectorLabel->setText(UIWizardImportApp::tr("You can modify the base folder which will host all the virtual machines.\n"
                                                            "Home folders can also be individually (per virtual machine)  modified."));
    if (m_pCheckBoxReinitMACs)
    {
        m_pCheckBoxReinitMACs->setText(tr("&Reinitialize the MAC address of all network cards"));
        m_pCheckBoxReinitMACs->setToolTip(tr("When checked a new unique MAC address will assigned to all configured network cards."));
    }

    if (m_pImportHDsAsVDI)
    {
        m_pImportHDsAsVDI->setText(tr("&Import hard drives as VDI"));
        m_pImportHDsAsVDI->setToolTip(tr("When checked a all the hard drives that belong to this appliance will be imported in VDI format"));
    }
}

void UIApplianceImportEditorWidget::sltHandlePathChanged(const QString &newPath)
{
    setVirtualSystemBaseFolder(newPath);
}
