/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxExportApplianceWgt class implementation
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* VBox includes */
#include "VBoxExportApplianceWgt.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

////////////////////////////////////////////////////////////////////////////////
// ExportSortProxyModel

class ExportSortProxyModel: public VirtualSystemSortProxyModel
{
public:
    ExportSortProxyModel (QObject *aParent = NULL)
      : VirtualSystemSortProxyModel (aParent)
    {
        mFilterList
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
            << KVirtualSystemDescriptionType_HardDiskControllerSCSI;
    }
};

////////////////////////////////////////////////////////////////////////////////
// VBoxExportApplianceWgt

VBoxExportApplianceWgt::VBoxExportApplianceWgt (QWidget *aParent /* = NULL */)
  : VBoxApplianceEditorWgt (aParent)
{
}

CAppliance* VBoxExportApplianceWgt::init()
{
    if (mAppliance)
        delete mAppliance;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    /* Create a appliance object */
    mAppliance = new CAppliance(vbox.CreateAppliance());
//    bool fResult = mAppliance->isOk();
    return mAppliance;
}

void VBoxExportApplianceWgt::populate()
{
    if (mModel)
        delete mModel;

    QVector<CVirtualSystemDescription> vsds = mAppliance->GetVirtualSystemDescriptions();

    mModel = new VirtualSystemModel (vsds, this);

    ExportSortProxyModel *proxy = new ExportSortProxyModel (this);
    proxy->setSourceModel (mModel);
    proxy->sort (DescriptionSection, Qt::DescendingOrder);

    VirtualSystemDelegate *delegate = new VirtualSystemDelegate (proxy, this);

    /* Set our own model */
    mTvSettings->setModel (proxy);
    /* Set our own delegate */
    mTvSettings->setItemDelegate (delegate);
    /* For now we hide the original column. This data is displayed as tooltip
       also. */
    mTvSettings->setColumnHidden (OriginalValueSection, true);
    mTvSettings->expandAll();

    /* Check for warnings & if there are one display them. */
    bool fWarningsEnabled = false;
    QVector<QString> warnings = mAppliance->GetWarnings();
    if (warnings.size() > 0)
    {
        foreach (const QString& text, warnings)
            mWarningTextEdit->append ("- " + text);
        fWarningsEnabled = true;
    }
    mWarningWidget->setShown (fWarningsEnabled);
}

void VBoxExportApplianceWgt::prepareExport()
{
    if (mAppliance)
        mModel->putBack();
}
