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

/* Qt includes */
#include <QFileInfo>
#include <QDialogButtonBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextStream>

////////////////////////////////////////////////////////////////////////////////
// VBoxImportLicenseViewer

VBoxImportLicenseViewer::VBoxImportLicenseViewer (QWidget *aParent /* = NULL */)
  : QIDialog (aParent)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout (this);
    pMainLayout->setMargin (12);

    mCaption = new QLabel (this);
    mCaption->setWordWrap (true);
    pMainLayout->addWidget (mCaption);

    mLicenseText = new QTextEdit (this);
    mLicenseText->setReadOnly (true);
    pMainLayout->addWidget (mLicenseText);

    mButtonBox = new QDialogButtonBox (QDialogButtonBox::No | QDialogButtonBox::Yes, Qt::Horizontal, this);
    mPrintBtn = new QPushButton (this);
    mButtonBox->addButton (mPrintBtn, QDialogButtonBox::ActionRole);
    mSaveBtn = new QPushButton (this);
    mButtonBox->addButton (mSaveBtn, QDialogButtonBox::ActionRole);
    mButtonBox->button (QDialogButtonBox::Yes)->setDefault (true);
    connect (mButtonBox, SIGNAL (rejected()),
             this, SLOT (reject()));
    connect (mButtonBox, SIGNAL (accepted()),
             this, SLOT (accept()));
    connect (mPrintBtn, SIGNAL (clicked()),
             this, SLOT (print()));
    connect (mSaveBtn, SIGNAL (clicked()),
             this, SLOT (save()));
    pMainLayout->addWidget (mButtonBox);

    retranslateUi();
}

void VBoxImportLicenseViewer::setContent (const QString &aName, const QString &aText)
{ 
    mName = aName; 
    mText = aText; 
    mCaption->setText (tr ("<b>To continue importing the Appliance you must agree to the terms of the software license agreement for the Virtual System \"%1\".</b><br /><br />Click <b>Agree</b> to continue or click <b>Disagree</b> to cancel the import.").arg (mName));
    mLicenseText->setText (mText);

}

void VBoxImportLicenseViewer::retranslateUi()
{
    setWindowTitle (tr ("Software License Agreement"));
    mButtonBox->button (QDialogButtonBox::No)->setText (tr ("&Disagree"));
    mButtonBox->button (QDialogButtonBox::Yes)->setText (tr ("&Agree"));
    mPrintBtn->setText (tr ("&Print..."));
    mSaveBtn->setText (tr ("&Save..."));

    setContent (mName, mText);
}

void VBoxImportLicenseViewer::print()
{
    QPrinter printer;
    QPrintDialog pd (&printer, this);
    if (pd.exec() == QDialog::Accepted) 
        mLicenseText->print (&printer);
}

void VBoxImportLicenseViewer::save()
{
    QString fileName = vboxGlobal().getSaveFileName (vboxGlobal().documentsPath(), tr("Text (*.txt)"), this, tr("Select a file to save into..."));
    if (!fileName.isEmpty())
    {
        QFile file (fileName);
        if (file.open(QFile::WriteOnly | QFile::Truncate)) 
        {
            QTextStream out (&file);
            out << mLicenseText->toPlainText();
        }
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
    mFileSelector->setMode (VBoxFilePathSelectorWidget::Mode_File_Open);
    mFileSelector->setResetEnabled (false);
    mFileSelector->setFileDialogTitle (tr ("Select an appliance to import"));
    mFileSelector->setFileFilters (tr ("Open Virtualization Format (%1)").arg ("*.ovf"));
    mFileSelector->setHomeDir (vboxGlobal().documentsPath());
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
        valid = mFileSelector->path().toLower().endsWith (".ovf") &&
                QFileInfo (mFileSelector->path()).exists();

    aWval->setOtherValid (valid);
}

void VBoxImportApplianceWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxImportApplianceWzd::accept()
{
    /* Make sure the final values are puted back. */
    mImportSettingsWgt->prepareImport();
    /* Check if there are license agreements the user must confirm */
    QList < QPair <QString, QString> > licAgreements = mImportSettingsWgt->licenseAgreements();
    if (!licAgreements.isEmpty())
    {
        VBoxImportLicenseViewer ilv (this);
        for (int i=0; i < licAgreements.size(); ++i)
        {
            const QPair <QString, QString> &lic = licAgreements.at (i);
            ilv.setContent (lic.first, lic.second);
            if (ilv.exec() == QDialog::Rejected)
                return;
        }
    }
    /* Now import all virtual systems */
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

