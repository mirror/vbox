/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIApplianceExportEditorWidget class implementation
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

/* GUI includes: */
#include "UIApplianceExportEditorWidget.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CAppliance.h"

////////////////////////////////////////////////////////////////////////////////
// ExportSortProxyModel

class ExportSortProxyModel: public VirtualSystemSortProxyModel
{
public:
    ExportSortProxyModel(QObject *pParent = NULL)
      : VirtualSystemSortProxyModel(pParent)
    {
        m_filterList
            << KVirtualSystemDescriptionType_OS
            << KVirtualSystemDescriptionType_CPU
            << KVirtualSystemDescriptionType_Memory
            << KVirtualSystemDescriptionType_Floppy
            << KVirtualSystemDescriptionType_CDROM
            << KVirtualSystemDescriptionType_USBController
            << KVirtualSystemDescriptionType_SoundCard
            << KVirtualSystemDescriptionType_NetworkAdapter
            << KVirtualSystemDescriptionType_HardDiskControllerIDE
            << KVirtualSystemDescriptionType_HardDiskControllerSATA
            << KVirtualSystemDescriptionType_HardDiskControllerSCSI
            << KVirtualSystemDescriptionType_HardDiskControllerSAS;
    }
};

////////////////////////////////////////////////////////////////////////////////
// UIApplianceExportEditorWidget

UIApplianceExportEditorWidget::UIApplianceExportEditorWidget(QWidget *pParent /* = NULL */)
  : UIApplianceEditorWidget(pParent)
{
}

CAppliance* UIApplianceExportEditorWidget::init()
{
    if (m_pAppliance)
        delete m_pAppliance;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Create a appliance object */
    m_pAppliance = new CAppliance(vbox.CreateAppliance());
//    bool fResult = m_pAppliance->isOk();
    return m_pAppliance;
}

void UIApplianceExportEditorWidget::populate()
{
    if (m_pModel)
        delete m_pModel;

    QVector<CVirtualSystemDescription> vsds = m_pAppliance->GetVirtualSystemDescriptions();

    m_pModel = new VirtualSystemModel(vsds, this);

    ExportSortProxyModel *pProxy = new ExportSortProxyModel(this);
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

void UIApplianceExportEditorWidget::prepareExport()
{
    if (m_pAppliance)
        m_pModel->putBack();
}

