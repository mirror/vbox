/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxImportApplianceWgt class implementation
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
#include "VBoxImportApplianceWgt.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"

////////////////////////////////////////////////////////////////////////////////
// ImportSortProxyModel

class ImportSortProxyModel: public VirtualSystemSortProxyModel
{
public:
    ImportSortProxyModel (QObject *aParent = NULL)
      : VirtualSystemSortProxyModel (aParent)
    {
        mFilterList << KVirtualSystemDescriptionType_License;
    }
};

////////////////////////////////////////////////////////////////////////////////
// VBoxImportApplianceWgt

VBoxImportApplianceWgt::VBoxImportApplianceWgt (QWidget *aParent)
    : VBoxApplianceEditorWgt (aParent)
{
}

bool VBoxImportApplianceWgt::setFile (const QString& aFile)
{
    bool fResult = false;
    if (!aFile.isEmpty())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Create a appliance object */
        mAppliance = new CAppliance(vbox.CreateAppliance());
        fResult = mAppliance->isOk();
        if (fResult)
        {
            /* Read the appliance */
            mAppliance->Read (aFile);
            fResult = mAppliance->isOk();
            if (fResult)
            {
                /* Now we have to interpret that stuff */
                mAppliance->Interpret();
                fResult = mAppliance->isOk();
                if (fResult)
                {
                    if (mModel)
                        delete mModel;

                    QVector<CVirtualSystemDescription> vsds = mAppliance->GetVirtualSystemDescriptions();

                    mModel = new VirtualSystemModel (vsds, this);

                    ImportSortProxyModel *proxy = new ImportSortProxyModel (this);
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
            }
        }
        if (!fResult)
        {
            vboxProblem().cannotImportAppliance (mAppliance, this);
            /* Delete the appliance in a case of an error */
            delete mAppliance;
            mAppliance = NULL;
        }
    }
    return fResult;
}

void VBoxImportApplianceWgt::prepareImport()
{
    if (mAppliance)
        mModel->putBack();
}

bool VBoxImportApplianceWgt::import()
{
    if (mAppliance)
    {
        /* Start the import asynchronously */
        CProgress progress;
        progress = mAppliance->ImportMachines();
        bool fResult = mAppliance->isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on */
            vboxProblem().showModalProgressDialog (progress, tr ("Importing Appliance ..."), this);
            if (!progress.isOk() || progress.GetResultCode() != 0)
            {
                vboxProblem().cannotImportAppliance (progress, mAppliance, this);
                return false;
            }
            else
                return true;
        }
        if (!fResult)
            vboxProblem().cannotImportAppliance (mAppliance, this);
    }
    return false;
}

QList < QPair<QString, QString> > VBoxImportApplianceWgt::licenseAgreements() const
{
    QList < QPair<QString, QString> > list;

    CVirtualSystemDescriptionVector vsds = mAppliance->GetVirtualSystemDescriptions();
    for (int i=0; i < vsds.size(); ++i)
    {
        QVector<QString> license;
        license = vsds[i].GetValuesByType (KVirtualSystemDescriptionType_License,
                                           KVirtualSystemDescriptionValueType_Original);
        if (!license.isEmpty())
        {
            QVector<QString> name;
            name = vsds[i].GetValuesByType (KVirtualSystemDescriptionType_Name,
                                            KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString> (name.first(), license.first());
        }
    }

    return list;
}

