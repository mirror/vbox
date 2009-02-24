/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxImportAppliance class implementation
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

#include "VBoxImportApplianceWzd.h"
#include "VBoxGlobal.h"
#include "QIWidgetValidator.h"
#include "VBoxProblemReporter.h"

#include <QFileInfo>

////////////////////////////////////////////////////////////////////////////////
// VBoxImportAppliance

int VBoxImportAppliance::mMinGuestRAM = -1;
int VBoxImportAppliance::mMaxGuestRAM = -1;
int VBoxImportAppliance::mMinGuestCPUCount = -1;
int VBoxImportAppliance::mMaxGuestCPUCount = -1;

/* static */
void VBoxImportAppliance::import (QWidget *aParent /* = NULL */)
{
    initSystemSettings();

    VBoxImportApplianceWzd importWzd (aParent);
    if (importWzd.exec() == QDialog::Accepted)
    {
    }
    return;

#if 0
    /* We need a file to import; request one from the user */
    QString file = VBoxGlobal::getOpenFileName ("",
                                                VBoxGlobal::tr ("Open Virtualization Format (%1)").arg ("*.ovf"),
                                                aParent,
                                                VBoxGlobal::tr ("Select an appliance to import"));
//    QString file = "/home/poetzsch/projects/innotek/plan9.ovf";
//    QString file = "/Users/poetzsch/projects/innotek/plan9.ovf";
    if (!file.isEmpty())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        /* Create a appliance object */
        CAppliance appliance = vbox.CreateAppliance();
        bool fResult = appliance.isOk();
        if (fResult)
        {
            /* Read the appliance */
            appliance.Read (file);
            fResult = appliance.isOk();
            if (fResult)
            {
                /* Now we have to interpret that stuff */
                appliance.Interpret();
                fResult = appliance.isOk();
                if (fResult)
                {
                    /* Let the user do some tuning */
                    VBoxImportApplianceWgt settingsDlg (&appliance, aParent);
                    if (settingsDlg.exec() == QDialog::Accepted)
                    {
                        /* Start the import asynchronously */
                        CProgress progress;
                        progress = appliance.ImportMachines();
                        fResult = appliance.isOk();
                        if (fResult)
                        {
                            /* Show some progress, so the user know whats going on */
                            vboxProblem().showModalProgressDialog (progress, VBoxImportApplianceWgt::tr ("Importing Appliance ..."), aParent);
                            if (!progress.isOk() || progress.GetResultCode() != 0)
                            {
                                vboxProblem().cannotImportAppliance (progress, appliance);
                                return;
                            }
                        }
                    }
                }
            }
        }
        if (!fResult)
            vboxProblem().cannotImportAppliance (appliance);
    }
#endif
}

/* static */
void VBoxImportAppliance::initSystemSettings()
{
    if (mMinGuestRAM == -1)
    {
        /* We need some global defaults from the current VirtualBox
           installation */
        CSystemProperties sp = vboxGlobal().virtualBox().GetSystemProperties();
        mMinGuestRAM = sp.GetMinGuestRAM();
        mMaxGuestRAM = sp.GetMaxGuestRAM();
        mMinGuestCPUCount = sp.GetMinGuestCPUCount();
        mMaxGuestCPUCount = sp.GetMaxGuestCPUCount();
    }
}

////////////////////////////////////////////////////////////////////////////////
// VBoxImportApplianceWzd

VBoxImportApplianceWzd::VBoxImportApplianceWzd (QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxImportApplianceWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Configure the file selector */
    mFileSelector->setMode (VBoxFilePathSelectorWidget::Mode_File);
    mFileSelector->setResetEnabled (false);
    mFileSelector->setFileDialogTitle (tr ("Select an appliance to import"));
    mFileSelector->setFileFilters (tr ("Open Virtualization Format (%1)").arg ("*.ovf"));
//    mFileSelector->setPath ("/home/poetzsch/downloads/Appliances/Plan9.ovf");
#ifdef Q_WS_MAC
    /* Editable boxes are uncommon on the Mac */
    mFileSelector->setEditable (false);
#endif /* Q_WS_MAC */

    /* Validator for the file selector page */
    mWValFileSelector = new QIWidgetValidator (mFileSelectPage, this);
    connect (mWValFileSelector, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValFileSelector, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mFileSelector, SIGNAL (pathChanged (const QString &)),
             mWValFileSelector, SLOT (revalidate()));

    /* Connect the restore button with the settings widget */
    connect (mBtnRestore, SIGNAL (clicked()),
             mImportSettingsWgt, SLOT (restoreDefaults()));

    mWValFileSelector->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

void VBoxImportApplianceWzd::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxImportApplianceWzd::retranslateUi (this);
}

void VBoxImportApplianceWzd::revalidate (QIWidgetValidator *aWval)
{
    /* Do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWValFileSelector)
        valid = mFileSelector->path().endsWith (".ovf") &&
                QFileInfo (mFileSelector->path()).exists();

    aWval->setOtherValid (valid);
}

void VBoxImportApplianceWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxImportApplianceWzd::accept()
{
    if (mImportSettingsWgt->import())
        QIAbstractWizard::accept();
}

void VBoxImportApplianceWzd::showNextPage()
{
    if (sender() == mBtnNext1)
    {
        if (mFileSelector->isModified())
        {
            /* Set the file path only if something has changed */
            mImportSettingsWgt->setFile (mFileSelector->path());
            /* Reset the modified bit afterwards */
            mFileSelector->resetModified();
        }

        /* If we have a valid ovf proceed to the appliance settings page */
        if (mImportSettingsWgt->isValid())
            QIAbstractWizard::showNextPage();
    }
    else
        QIAbstractWizard::showNextPage();
}

void VBoxImportApplianceWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mSettingsPage)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

