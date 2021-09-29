/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportApp class implementation.
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
#include <QFileInfo>
#include <QPushButton>
#include <QVariant>

/* GUI includes: */
#include "UIAddDiskEncryptionPasswordDialog.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UINotificationCenter.h"
#include "UIWizardExportApp.h"
#include "UIWizardExportAppPageBasic1.h"
#include "UIWizardExportAppPageBasic2.h"
#include "UIWizardExportAppPageBasic3.h"
#include "UIWizardExportAppPageExpert.h"
#include "UIWizardNewCloudVM.h"

/* COM includes: */
#include "CAppliance.h"
#include "CVFSExplorer.h"


UIWizardExportApp::UIWizardExportApp(QWidget *pParent,
                                     const QStringList &predefinedMachineNames /* = QStringList() */,
                                     bool fFastTraverToExportOCI /* = false */)
    : UINativeWizard(pParent, WizardType_ExportAppliance, WizardMode_Auto,
                     fFastTraverToExportOCI ? "cloud-export-oci" : "ovf")
    , m_predefinedMachineNames(predefinedMachineNames)
    , m_fFastTraverToExportOCI(fFastTraverToExportOCI)
    , m_fFormatCloudOne(false)
    , m_enmMACAddressExportPolicy(MACAddressExportPolicy_KeepAllMACs)
    , m_fManifestSelected(false)
    , m_fIncludeISOsSelected(false)
    , m_enmCloudExportMode(CloudExportMode_DoNotAsk)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    setPixmapName(":/wizard_ovf_export.png");
#else
    /* Assign background image: */
    setPixmapName(":/wizard_ovf_export_bg.png");
#endif
}

QStringList UIWizardExportApp::machineNames() const
{
    return m_machineNames;
}

void UIWizardExportApp::setMachineNames(const QStringList &names)
{
    m_machineNames = names;
}

QList<QUuid> UIWizardExportApp::machineIDs() const
{
    return m_machineIDs;
}

void UIWizardExportApp::setMachineIDs(const QList<QUuid> &ids)
{
    m_machineIDs = ids;
}

QString UIWizardExportApp::format() const
{
    return m_strFormat;
}

void UIWizardExportApp::setFormat(const QString &strFormat)
{
    m_strFormat = strFormat;
}

bool UIWizardExportApp::isFormatCloudOne() const
{
    return m_fFormatCloudOne;
}

void UIWizardExportApp::setFormatCloudOne(bool fCloudOne)
{
    m_fFormatCloudOne = fCloudOne;
}

QString UIWizardExportApp::path() const
{
    return m_strPath;
}

void UIWizardExportApp::setPath(const QString &strPath)
{
    m_strPath = strPath;
}

MACAddressExportPolicy UIWizardExportApp::macAddressExportPolicy() const
{
    return m_enmMACAddressExportPolicy;
}

void UIWizardExportApp::setMACAddressExportPolicy(MACAddressExportPolicy enmPolicy)
{
    m_enmMACAddressExportPolicy = enmPolicy;
}

bool UIWizardExportApp::isManifestSelected() const
{
    return m_fManifestSelected;
}

void UIWizardExportApp::setManifestSelected(bool fSelected)
{
    m_fManifestSelected = fSelected;
}

bool UIWizardExportApp::isIncludeISOsSelected() const
{
    return m_fIncludeISOsSelected;
}

void UIWizardExportApp::setIncludeISOsSelected(bool fSelected)
{
    m_fIncludeISOsSelected = fSelected;
}

CAppliance UIWizardExportApp::localAppliance()
{
    return m_comLocalAppliance;
}

void UIWizardExportApp::setLocalAppliance(const CAppliance &comAppliance)
{
    m_comLocalAppliance = comAppliance;
}

QString UIWizardExportApp::profileName() const
{
    return m_strProfileName;
}

void UIWizardExportApp::setProfileName(const QString &strName)
{
    m_strProfileName = strName;
}

CAppliance UIWizardExportApp::cloudAppliance()
{
    return m_comCloudAppliance;
}

void UIWizardExportApp::setCloudAppliance(const CAppliance &comAppliance)
{
    m_comCloudAppliance = comAppliance;
}

CCloudClient UIWizardExportApp::cloudClient()
{
    return m_comCloudClient;
}

void UIWizardExportApp::setCloudClient(const CCloudClient &comClient)
{
    m_comCloudClient = comClient;
}

CVirtualSystemDescription UIWizardExportApp::vsd()
{
    return m_comVsd;
}

void UIWizardExportApp::setVsd(const CVirtualSystemDescription &comDescription)
{
    m_comVsd = comDescription;
}

CVirtualSystemDescriptionForm UIWizardExportApp::vsdExportForm()
{
    return m_comVsdExportForm;
}

void UIWizardExportApp::setVsdExportForm(const CVirtualSystemDescriptionForm &comForm)
{
    m_comVsdExportForm = comForm;
}

CloudExportMode UIWizardExportApp::cloudExportMode() const
{
    return m_enmCloudExportMode;
}

void UIWizardExportApp::setCloudExportMode(const CloudExportMode &enmMode)
{
    m_enmCloudExportMode = enmMode;
}

void UIWizardExportApp::goForward()
{
    wizardButton(WizardButtonType_Next)->click();
}

QString UIWizardExportApp::uri(bool fWithFile) const
{
    /* For Cloud formats: */
    if (isFormatCloudOne())
        return QString("%1://").arg(format());
    else
    {
        /* Prepare storage path: */
        QString strPath = path();
        /* Append file name if requested: */
        if (!fWithFile)
        {
            QFileInfo fi(strPath);
            strPath = fi.path();
        }

        /* Just path by default: */
        return strPath;
    }
}

bool UIWizardExportApp::exportAppliance()
{
    /* Check whether there was cloud target selected: */
    if (isFormatCloudOne())
    {
        /* Get appliance: */
        CAppliance comAppliance = cloudAppliance();
        AssertReturn(comAppliance.isNotNull(), false);

        /* Export the VMs, on success we are finished: */
        return exportVMs(comAppliance);
    }
    else
    {
        /* Get appliance: */
        CAppliance comAppliance = localAppliance();
        AssertReturn(comAppliance.isNotNull(), false);

        /* We need to know every filename which will be created, so that we can ask the user for confirmation of overwriting.
         * For that we iterating over all virtual systems & fetch all descriptions of the type HardDiskImage. Also add the
         * manifest file to the check. In the .ova case only the target file itself get checked. */

        /* Compose a list of all required files: */
        QFileInfo fi(path());
        QVector<QString> files;

        /* Add arhive itself: */
        files << fi.fileName();

        /* If archive is of .ovf type: */
        if (fi.suffix().toLower() == "ovf")
        {
            /* Add manifest file if requested: */
            if (isManifestSelected())
                files << fi.baseName() + ".mf";

            /* Add all hard disk images: */
            CVirtualSystemDescriptionVector vsds = comAppliance.GetVirtualSystemDescriptions();
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
        CVFSExplorer comExplorer = comAppliance.CreateVFSExplorer(uri(false /* fWithFile */));
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
            return msgCenter().cannotCheckFiles(comAppliance, this);

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
        return exportVMs(comAppliance);
    }
}

void UIWizardExportApp::populatePages()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            addPage(new UIWizardExportAppPageBasic1(m_predefinedMachineNames, m_fFastTraverToExportOCI));
            addPage(new UIWizardExportAppPageBasic2(m_fFastTraverToExportOCI));
            addPage(new UIWizardExportAppPageBasic3);
            break;
        }
        case WizardMode_Expert:
        {
            addPage(new UIWizardExportAppPageExpert(m_predefinedMachineNames, m_fFastTraverToExportOCI));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
}

void UIWizardExportApp::retranslateUi()
{
    /* Call to base-class: */
    UINativeWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Export Virtual Appliance"));
    /// @todo implement this?
    //setButtonText(QWizard::FinishButton, tr("Export"));
}

bool UIWizardExportApp::exportVMs(CAppliance &comAppliance)
{
    /* Prepare result: */
    bool fResult = false;

    /* New Cloud VM wizard can be created
     * in certain cases and should be cleaned up
     * afterwards, thus this is a global variable: */
    UISafePointerWizardNewCloudVM pNewCloudVMWizard;

    /* Main API request sequence, can be interrupted after any step: */
    do
    {
        /* Get the map of the password IDs: */
        EncryptedMediumMap encryptedMedia;
        foreach (const QString &strPasswordId, comAppliance.GetPasswordIds())
            foreach (const QUuid &uMediumId, comAppliance.GetMediumIdsForPasswordId(strPasswordId))
                encryptedMedia.insert(strPasswordId, uMediumId);

        /* Ask for the disk encryption passwords if necessary: */
        if (!encryptedMedia.isEmpty())
        {
            /* Modal dialog can be destroyed in own event-loop as a part of application
             * termination procedure. We have to make sure that the dialog pointer is
             * always up to date. So we are wrapping created dialog with QPointer. */
            QPointer<UIAddDiskEncryptionPasswordDialog> pDlg =
                new UIAddDiskEncryptionPasswordDialog(this,
                                                      window()->windowTitle(),
                                                      encryptedMedia);

            /* Execute the dialog: */
            if (pDlg->exec() != QDialog::Accepted)
            {
                /* Delete the dialog: */
                delete pDlg;
                break;
            }

            /* Acquire the passwords provided: */
            const EncryptionPasswordMap encryptionPasswords = pDlg->encryptionPasswords();

            /* Delete the dialog: */
            delete pDlg;

            /* Provide appliance with passwords if possible: */
            comAppliance.AddPasswords(encryptionPasswords.keys().toVector(),
                                      encryptionPasswords.values().toVector());
            if (!comAppliance.isOk())
            {
                msgCenter().cannotAddDiskEncryptionPassword(comAppliance);
                break;
            }
        }

        /* Prepare export options: */
        QVector<KExportOptions> options;
        switch (macAddressExportPolicy())
        {
            case MACAddressExportPolicy_StripAllNonNATMACs: options.append(KExportOptions_StripAllNonNATMACs); break;
            case MACAddressExportPolicy_StripAllMACs: options.append(KExportOptions_StripAllMACs); break;
            default: break;
        }
        if (isManifestSelected())
            options.append(KExportOptions_CreateManifest);
        if (isIncludeISOsSelected())
            options.append(KExportOptions_ExportDVDImages);

        /* Is this VM being exported to cloud? */
        if (isFormatCloudOne())
        {
            /* We can have wizard and it's result
             * should be distinguishable: */
            int iWizardResult = -1;

            switch (cloudExportMode())
            {
                case CloudExportMode_AskThenExport:
                {
                    /* Get the required parameters to init short wizard mode: */
                    CCloudClient comClient = cloudClient();
                    CVirtualSystemDescription comDescription = vsd();
                    /* Create and run wizard as modal dialog, but prevent final step: */
                    QWidget *pWizardParent = windowManager().realParentWindow(this);
                    const QString strGroupName = QString("/%1/%2").arg(format(), profileName());
                    pNewCloudVMWizard = new UIWizardNewCloudVM(pWizardParent, strGroupName, comClient, comDescription, mode());
                    windowManager().registerNewParent(pNewCloudVMWizard, pWizardParent);
                    pNewCloudVMWizard->setFinalStepPrevented(true);
                    iWizardResult = pNewCloudVMWizard->exec();
                    break;
                }
                default:
                    break;
            }

            /* We should stop everything only if
             * there was wizard and it was rejected: */
            if (iWizardResult == QDialog::Rejected)
                break;

            /* Prepare Export VM progress: */
            CProgress comProgress = comAppliance.Write(format(), options, uri());
            if (!comAppliance.isOk())
            {
                msgCenter().cannotExportAppliance(comAppliance, this);
                break;
            }

            /* Show Export VM progress: */
            msgCenter().showModalProgressDialog(comProgress, QApplication::translate("UIWizardExportApp", "Exporting Appliance ..."),
                                                ":/progress_export_90px.png", this);
            if (comProgress.GetCanceled())
                break;
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
            {
                msgCenter().cannotExportAppliance(comProgress, comAppliance.GetPath(), this);
                break;
            }

            /* We can have wizard and it's result
             * should be distinguishable: */
            iWizardResult = -1;

            switch (cloudExportMode())
            {
                case CloudExportMode_AskThenExport:
                {
                    /* Run the wizard as modal dialog again,
                     * moreover in auto-finish mode and
                     * do not prevent final step. */
                    pNewCloudVMWizard->setFinalStepPrevented(false);
                    pNewCloudVMWizard->scheduleAutoFinish();
                    iWizardResult = pNewCloudVMWizard->exec();
                    break;
                }
                case CloudExportMode_ExportThenAsk:
                {
                    /* Get the required parameters to init short wizard mode: */
                    CCloudClient comClient = cloudClient();
                    CVirtualSystemDescription comDescription = vsd();
                    /* Create and run short wizard mode as modal dialog: */
                    QWidget *pWizardParent = windowManager().realParentWindow(this);
                    const QString strGroupName = QString("/%1/%2").arg(format(), profileName());
                    pNewCloudVMWizard = new UIWizardNewCloudVM(pWizardParent, strGroupName, comClient, comDescription, mode());
                    windowManager().registerNewParent(pNewCloudVMWizard, pWizardParent);
                    iWizardResult = pNewCloudVMWizard->exec();
                    break;
                }
                default:
                    break;
            }

            /* We should stop everything only if
             * there was wizard and it was rejected: */
            if (iWizardResult == QDialog::Rejected)
                break;
        }
        /* Is this VM being exported locally? */
        else
        {
            /* Export appliance: */
            UINotificationProgressApplianceExport *pNotification = new UINotificationProgressApplianceExport(comAppliance,
                                                                                                             format(),
                                                                                                             options,
                                                                                                             uri());
            gpNotificationCenter->append(pNotification);
        }

        /* Success finally: */
        fResult = true;
    }
    while (0);

    /* Cleanup New Cloud VM wizard if any: */
    delete pNewCloudVMWizard;

    /* Return result: */
    return fResult;
}
