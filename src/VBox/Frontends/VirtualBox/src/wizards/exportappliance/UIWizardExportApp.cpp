/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class implementation.
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
# include <QFileInfo>
# include <QVariant>

/* GUI includes: */
# include "UIAddDiskEncryptionPasswordDialog.h"
# include "UIMessageCenter.h"
# include "UIWizardExportApp.h"
# include "UIWizardExportAppDefs.h"
# include "UIWizardExportAppPageBasic1.h"
# include "UIWizardExportAppPageBasic2.h"
# include "UIWizardExportAppPageBasic3.h"
# include "UIWizardExportAppPageBasic4.h"
# include "UIWizardExportAppPageExpert.h"

/* COM includes: */
# include "CAppliance.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* COM includes: */
#include "CVFSExplorer.h"


UIWizardExportApp::UIWizardExportApp(QWidget *pParent, const QStringList &selectedVMNames)
    : UIWizard(pParent, WizardType_ExportAppliance)
    , m_selectedVMNames(selectedVMNames)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_ovf_export.png");
#else
    /* Assign background image: */
    assignBackground(":/wizard_ovf_export_bg.png");
#endif
}

bool UIWizardExportApp::exportAppliance()
{
    /* We need to know every filename which will be created, so that we can ask the user for confirmation of overwriting.
     * For that we iterating over all virtual systems & fetch all descriptions of the type HardDiskImage. Also add the
     * manifest file to the check. In the .ova case only the target file itself get checked. */

    /* Get export appliance widget & fetch all settings from the appliance editor: */
    UIApplianceExportEditorWidget *pExportApplianceWidget = field("applianceWidget").value<ExportAppliancePointer>();
    AssertPtrReturn(pExportApplianceWidget, false);
    pExportApplianceWidget->prepareExport();

    /* Get the appliance: */
    CAppliance *pComAppliance = pExportApplianceWidget->appliance();
    AssertPtrReturn(pComAppliance, false);

    /* Compose a list of all required files: */
    QFileInfo fi(field("path").toString());
    QVector<QString> files;

    /* Add arhive itself: */
    files << fi.fileName();

    /* If archive is of .ovf type: */
    if (fi.suffix().toLower() == "ovf")
    {
        /* Add manifest file if requested: */
        if (field("manifestSelected").toBool())
            files << fi.baseName() + ".mf";

        /* Add all hard disk images: */
        CVirtualSystemDescriptionVector vsds = pComAppliance->GetVirtualSystemDescriptions();
        for (int i = 0; i < vsds.size(); ++i)
        {
            QVector<KVirtualSystemDescriptionType> types;
            QVector<QString> refs, origValues, configValues, extraConfigValues;
            vsds[i].GetDescriptionByType(KVirtualSystemDescriptionType_HardDiskImage, types,
                                         refs, origValues, configValues, extraConfigValues);
            foreach (const QString &strValue, origValues)
                files << QString("%2").arg(strValue);
        }
    }

    /* Initialize VFS explorer: */
    CVFSExplorer comExplorer = pComAppliance->CreateVFSExplorer(uri(false /* fWithFile */));
    if (comExplorer.isNotNull())
    {
        CProgress comProgress = comExplorer.Update();
        if (comExplorer.isOk() && comProgress.isNotNull())
        {
            msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIWizardExportApp", "Checking files ..."),
                                                ":/progress_refresh_90px.png", this);
            if (comProgress.GetCanceled())
                return false;
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                return msgCenter().cannotCheckFiles(comProgress, this);
        }
        else
            return msgCenter().cannotCheckFiles(comExplorer, this);
    }
    else
        return msgCenter().cannotCheckFiles(*pComAppliance, this);

    /* Confirm overwriting for existing files: */
    QVector<QString> exists = comExplorer.Exists(files);
    if (!msgCenter().confirmOverridingFiles(exists, this))
        return false;

    /* DELETE all the files which exists after everything is confirmed: */
    if (!exists.isEmpty())
    {
        CProgress comProgress = comExplorer.Remove(exists);
        if (comExplorer.isOk() && comProgress.isNotNull())
        {
            msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIWizardExportApp", "Removing files ..."),
                                                ":/progress_delete_90px.png", this);
            if (comProgress.GetCanceled())
                return false;
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                return msgCenter().cannotRemoveFiles(comProgress, this);
        }
        else
            return msgCenter().cannotCheckFiles(comExplorer, this);
    }

    /* Export the VMs, on success we are finished: */
    return exportVMs(*pComAppliance);
}

QString UIWizardExportApp::uri(bool fWithFile) const
{
    /* Prepare storage path: */
    QString strPath = field("path").toString();
    /* Append file name if requested: */
    if (!fWithFile)
    {
        QFileInfo fi(strPath);
        strPath = fi.path();
    }

    /* For known storage types: */
    StorageType enmType = field("storageType").value<StorageType>();
    switch (enmType)
    {
        case Filesystem:
        {
            return strPath;
        }
        case SunCloud:
        {
            QString strUri("SunCloud://");
            if (!field("username").toString().isEmpty())
                strUri = QString("%1%2").arg(strUri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                strUri = QString("%1:%2").arg(strUri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("password").toString().isEmpty())
                strUri = QString("%1@").arg(strUri);
            strUri = QString("%1%2/%3/%4").arg(strUri).arg("object.storage.network.com").arg(field("bucket").toString()).arg(strPath);
            return strUri;
        }
        case S3:
        {
            QString strUri("S3://");
            if (!field("username").toString().isEmpty())
                strUri = QString("%1%2").arg(strUri).arg(field("username").toString());
            if (!field("password").toString().isEmpty())
                strUri = QString("%1:%2").arg(strUri).arg(field("password").toString());
            if (!field("username").toString().isEmpty() || !field("password").toString().isEmpty())
                strUri = QString("%1@").arg(strUri);
            strUri = QString("%1%2/%3/%4").arg(strUri).arg(field("hostname").toString()).arg(field("bucket").toString()).arg(strPath);
            return strUri;
        }
    }

    /* Nothing by default: */
    return QString();
}

void UIWizardExportApp::sltCurrentIdChanged(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCurrentIdChanged(iId);

    /* Enable 2nd button (Reset to Defaults) for 4th and Expert pages only! */
    setOption(QWizard::HaveCustomButton2,    (mode() == WizardMode_Basic && iId == Page4)
                                          || (mode() == WizardMode_Expert && iId == PageExpert));
}

void UIWizardExportApp::sltCustomButtonClicked(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCustomButtonClicked(iId);

    /* Handle 2nd button: */
    if (iId == CustomButton2)
    {
        /* Get appliance widget and make sure it's valid: */
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
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardExportAppPageBasic1(m_selectedVMNames));
            setPage(Page2, new UIWizardExportAppPageBasic2);
            setPage(Page3, new UIWizardExportAppPageBasic3);
            setPage(Page4, new UIWizardExportAppPageBasic4);
            break;
        }
        case WizardMode_Expert:
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

bool UIWizardExportApp::exportVMs(CAppliance &comAppliance)
{
    /* Get the map of the password IDs: */
    EncryptedMediumMap encryptedMediums;
    foreach (const QString &strPasswordId, comAppliance.GetPasswordIds())
        foreach (const QString &strMediumId, comAppliance.GetMediumIdsForPasswordId(strPasswordId))
            encryptedMediums.insert(strPasswordId, strMediumId);

    /* Ask for the disk encryption passwords if necessary: */
    if (!encryptedMediums.isEmpty())
    {
        /* Modal dialog can be destroyed in own event-loop as a part of application
         * termination procedure. We have to make sure that the dialog pointer is
         * always up to date. So we are wrapping created dialog with QPointer. */
        QPointer<UIAddDiskEncryptionPasswordDialog> pDlg =
             new UIAddDiskEncryptionPasswordDialog(this,
                                                   window()->windowTitle(),
                                                   encryptedMediums);

        /* Execute the dialog: */
        if (pDlg->exec() == QDialog::Accepted)
        {
            /* Acquire the passwords provided: */
            const EncryptionPasswordMap encryptionPasswords = pDlg->encryptionPasswords();

            /* Delete the dialog: */
            delete pDlg;

            /* Make sure the passwords were really provided: */
            AssertReturn(!encryptionPasswords.isEmpty(), false);

            /* Provide appliance with passwords if possible: */
            comAppliance.AddPasswords(encryptionPasswords.keys().toVector(),
                                      encryptionPasswords.values().toVector());
            if (!comAppliance.isOk())
                return msgCenter().cannotAddDiskEncryptionPassword(comAppliance);
        }
        else
        {
            /* Delete the dialog: */
            delete pDlg;
            return false;
        }
    }

    /* Write the appliance: */
    QVector<KExportOptions> options;
    if (field("manifestSelected").toBool())
        options.append(KExportOptions_CreateManifest);
    CProgress comProgress = comAppliance.Write(field("format").toString(), options, uri());
    if (comAppliance.isOk() && comProgress.isNotNull())
    {
        msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIWizardExportApp", "Exporting Appliance ..."),
                                            ":/progress_export_90px.png", this);
        if (comProgress.GetCanceled())
            return false;
        if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            return msgCenter().cannotExportAppliance(comProgress, comAppliance.GetPath(), this);
    }
    else
        return msgCenter().cannotExportAppliance(comAppliance, this);

    /* True finally: */
    return true;
}
