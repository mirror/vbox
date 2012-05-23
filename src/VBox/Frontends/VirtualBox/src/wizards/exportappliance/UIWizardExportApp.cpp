/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardExportApp class implementation
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

/* Global includes: */
#include <QVariant>
#include <QFileInfo>

/* Local includes: */
#include "UIWizardExportApp.h"
#include "UIWizardExportAppDefs.h"
#include "UIWizardExportAppPageBasic1.h"
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportAppPageBasic3.h"
#include "UIWizardExportAppPageBasic4.h"
#include "UIWizardExportAppPageExpert.h"
#include "COMDefs.h"
#include "UIMessageCenter.h"

UIWizardExportApp::UIWizardExportApp(QWidget *pParent, const QStringList &selectedVMNames)
    : UIWizard(pParent, UIWizardType_ExportAppliance)
    , m_selectedVMNames(selectedVMNames)
{
#ifndef Q_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/vmw_ovf_export.png");
#else /* Q_WS_MAC */
    /* Assign background image: */
    assignBackground(":/vmw_ovf_export_bg.png");
#endif /* Q_WS_MAC */
}

bool UIWizardExportApp::exportAppliance()
{
    /* Get export appliance widget: */
    UIApplianceExportEditorWidget *pExportApplianceWidget = field("applianceWidget").value<ExportAppliancePointer>();
    /* Fetch all settings from the appliance editor. */
    pExportApplianceWidget->prepareExport();
    /* Get the appliance. */
    CAppliance *pAppliance = pExportApplianceWidget->appliance();
    /* We need to know every filename which will be created, so that we can
     * ask the user for confirmation of overwriting. For that we iterating
     * over all virtual systems & fetch all descriptions of the type
     * HardDiskImage. Also add the manifest file to the check. In the ova
     * case only the target file itself get checked. */
    QFileInfo fi(field("path").toString());
    QVector<QString> files;
    files << fi.fileName();
    if (fi.suffix().toLower() == "ovf")
    {
        if (field("manifestSelected").toBool())
            files << fi.baseName() + ".mf";
        CVirtualSystemDescriptionVector vsds = pAppliance->GetVirtualSystemDescriptions();
        for (int i = 0; i < vsds.size(); ++ i)
        {
            QVector<KVirtualSystemDescriptionType> types;
            QVector<QString> refs, origValues, configValues, extraConfigValues;
            vsds[i].GetDescriptionByType(KVirtualSystemDescriptionType_HardDiskImage, types,
                                         refs, origValues, configValues, extraConfigValues);
            foreach (const QString &s, origValues)
                files << QString("%2").arg(s);
        }
    }
    CVFSExplorer explorer = pAppliance->CreateVFSExplorer(uri(false /* fWithFile */));
    CProgress progress = explorer.Update();
    bool fResult = explorer.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on: */
        msgCenter().showModalProgressDialog(progress, QApplication::translate("UIWizardExportApp", "Checking files ..."), "", this);
        if (progress.GetCanceled())
            return false;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            msgCenter().cannotCheckFiles(progress, this);
            return false;
        }
    }
    QVector<QString> exists = explorer.Exists(files);
    /* Check if the file exists already, if yes get confirmation for overwriting from the user. */
    if (!msgCenter().askForOverridingFiles(exists, this))
        return false;
    /* Ok all is confirmed so delete all the files which exists: */
    if (!exists.isEmpty())
    {
        CProgress progress1 = explorer.Remove(exists);
        fResult = explorer.isOk();
        if (fResult)
        {
            /* Show some progress, so the user know whats going on: */
            msgCenter().showModalProgressDialog(progress1, QApplication::translate("UIWizardExportApp", "Removing files ..."), "", this);
            if (progress1.GetCanceled())
                return false;
            if (!progress1.isOk() || progress1.GetResultCode() != 0)
            {
                msgCenter().cannotRemoveFiles(progress1, this);
                return false;
            }
        }
    }

    /* Export the VMs, on success we are finished: */
    if (exportVMs(*pAppliance))
    {
#if 0
        /* Save attributes to GUI extra data: */
        StorageType storageType = field("storageType").value<StorageType>();
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_StorageType, QString::number(storageType));
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Username, m_pLeUsername->text());
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Hostname, m_pLeHostname->text());
        vboxGlobal().virtualBox().SetExtraData(VBoxDefs::GUI_Export_Bucket, m_pLeBucket->text());
#endif
        return true;
    }
    return false;
}

bool UIWizardExportApp::exportVMs(CAppliance &appliance)
{
    /* Write the appliance: */
    const QString strVersion = field("OVF09Selected").toBool() ? "ovf-0.9" : "ovf-1.0";
    CProgress progress = appliance.Write(strVersion, field("manifestSelected").toBool() /* fManifest */, uri());
    bool fResult = appliance.isOk();
    if (fResult)
    {
        /* Show some progress, so the user know whats going on: */
        msgCenter().showModalProgressDialog(progress, QApplication::translate("UIWizardExportApp", "Exporting Appliance ..."),
                                            ":/progress_export_90px.png", this, true);
        if (progress.GetCanceled())
            return false;
        if (!progress.isOk() || progress.GetResultCode() != 0)
        {
            msgCenter().cannotExportAppliance(progress, &appliance, this);
            return false;
        }
        else
            return true;
    }
    if (!fResult)
        msgCenter().cannotExportAppliance(&appliance, this);
    return false;
}

QString UIWizardExportApp::uri(bool fWithFile) const
{
    StorageType type = field("storageType").value<StorageType>();

    QString path = field("path").toString();
    if (!fWithFile)
    {
        QFileInfo fi(path);
        path = fi.path();
    }
    switch (type)
    {
        case Filesystem:
        {
            return path;
        }
        case SunCloud:
        {
            QString uri("SunCloud://");
            if (!field("username").toString().isEmpty())
                uri = QString("%1%2").arg(uri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                uri = QString("%1:%2").arg(uri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("username").toString().isEmpty())
                uri = QString("%1@").arg(uri);
            uri = QString("%1%2/%3/%4").arg(uri).arg("object.storage.network.com").arg(field("bucket").toString()).arg(path);
            return uri;
        }
        case S3:
        {
            QString uri("S3://");
            if (!field("username").toString().isEmpty())
                uri = QString("%1%2").arg(uri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                uri = QString("%1:%2").arg(uri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("password").toString().isEmpty())
                uri = QString("%1@").arg(uri);
            uri = QString("%1%2/%3/%4").arg(uri).arg(field("hostname").toString()).arg(field("bucket").toString()).arg(path);
            return uri;
        }
    }
    return QString();
}

void UIWizardExportApp::sltCurrentIdChanged(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCurrentIdChanged(iId);
    /* Enable 2nd button (Reset to Defaults) for 4th and Expert pages only! */
    setOption(QWizard::HaveCustomButton2, (mode() == UIWizardMode_Basic && iId == Page4) ||
                                          (mode() == UIWizardMode_Expert && iId == PageExpert));
}

void UIWizardExportApp::sltCustomButtonClicked(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCustomButtonClicked(iId);

    /* Handle 2nd button: */
    if (iId == CustomButton2)
    {
        /* Get appliance widget: */
        ExportAppliancePointer pApplianceWidget = field("applianceWidget").value<ExportAppliancePointer>();
        AssertMsg(!pApplianceWidget.isNull(), ("Appliance Widget is not set!\n"));
        /* Reset it to default: */
        pApplianceWidget->restoreDefaults();
    }
}

void UIWizardExportApp::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Export Virtual Appliance"));
    setButtonText(QWizard::CustomButton2, tr("Restore Defaults"));
    setButtonText(QWizard::FinishButton, tr("Export"));
}

void UIWizardExportApp::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case UIWizardMode_Basic:
        {
            setPage(Page1, new UIWizardExportAppPageBasic1(m_selectedVMNames));
            setPage(Page2, new UIWizardExportAppPageBasic2);
            setPage(Page3, new UIWizardExportAppPageBasic3);
            setPage(Page4, new UIWizardExportAppPageBasic4);
            break;
        }
        case UIWizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardExportAppPageExpert(m_selectedVMNames));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}

