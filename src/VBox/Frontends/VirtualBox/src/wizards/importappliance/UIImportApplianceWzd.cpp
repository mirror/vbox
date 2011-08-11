/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIImportApplianceWzd class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTextStream>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIImportApplianceWzd.h"
#include "QIFileDialog.h"

UIImportLicenseViewer::UIImportLicenseViewer(QWidget *pParent) : QIDialog(pParent)
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setMargin(12);

    m_pCaption = new QLabel(this);
    m_pCaption->setWordWrap(true);
    pMainLayout->addWidget(m_pCaption);

    m_pLicenseText = new QTextEdit(this);
    m_pLicenseText->setReadOnly(true);
    pMainLayout->addWidget(m_pLicenseText);

    m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::No | QDialogButtonBox::Yes, Qt::Horizontal, this);
    m_pPrintButton = new QPushButton(this);
    m_pButtonBox->addButton(m_pPrintButton, QDialogButtonBox::ActionRole);
    m_pSaveButton = new QPushButton(this);
    m_pButtonBox->addButton(m_pSaveButton, QDialogButtonBox::ActionRole);
    m_pButtonBox->button(QDialogButtonBox::Yes)->setDefault(true);
    connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(m_pPrintButton, SIGNAL(clicked()), this, SLOT(sltPrint()));
    connect(m_pSaveButton, SIGNAL(clicked()), this, SLOT(sltSave()));
    pMainLayout->addWidget(m_pButtonBox);

    retranslateUi();
}

void UIImportLicenseViewer::setContents(const QString &strName, const QString &strText)
{
    m_strName = strName;
    m_strText = strText;
    retranslateUi();
}

void UIImportLicenseViewer::retranslateUi()
{
    setWindowTitle(tr("Software License Agreement"));
    m_pCaption->setText(tr("<b>The virtual system \"%1\" requires that you agree to the terms and conditions "
                           "of the software license agreement shown below.</b><br /><br />Click <b>Agree</b> "
                           "to continue or click <b>Disagree</b> to cancel the import.").arg(m_strName));
    m_pLicenseText->setText(m_strText);
    m_pButtonBox->button(QDialogButtonBox::No)->setText(tr("&Disagree"));
    m_pButtonBox->button(QDialogButtonBox::Yes)->setText(tr("&Agree"));
    m_pPrintButton->setText(tr("&Print..."));
    m_pSaveButton->setText(tr("&Save..."));
}

void UIImportLicenseViewer::sltPrint()
{
    QPrinter printer;
    QPrintDialog pd(&printer, this);
    if (pd.exec() == QDialog::Accepted)
        m_pLicenseText->print(&printer);
}

void UIImportLicenseViewer::sltSave()
{
    QString fileName = QIFileDialog::getSaveFileName(vboxGlobal().documentsPath(), tr("Text (*.txt)"),
                                                     this, tr("Save license to file..."));
    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly | QFile::Truncate))
        {
            QTextStream out(&file);
            out << m_pLicenseText->toPlainText();
        }
    }
}

UIImportApplianceWzd::UIImportApplianceWzd(const QString &strFile /* = "" */, QWidget *pParent /* = 0 */)
  : QIWizard(pParent)
{
    /* Create & add pages */
    if (strFile.isEmpty())
        addPage(new UIImportApplianceWzdPage1);
    addPage(new UIImportApplianceWzdPage2);
    if (!strFile.isEmpty())
    {
        UIApplianceImportEditorWidget *applianceWidget = field("applianceWidget").value<ImportAppliancePointer>();

        if (!applianceWidget->setFile(strFile))
            return;
    }

    /* Initial translate */
    retranslateUi();

    /* Initial translate all pages */
    retranslateAllPages();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

    /* Assign watermark */
#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_ovf_import_bg.png");
#else /* Q_WS_MAC */
    assignWatermark(":/vmw_ovf_import.png");
#endif /* Q_WS_MAC */

    /* Configure 'Restore Defaults' button */
    AssertMsg(!field("applianceWidget").value<ImportAppliancePointer>().isNull(), ("Appliance Widget is not set!\n"));
    connect(this, SIGNAL(customButtonClicked(int)), field("applianceWidget").value<ImportAppliancePointer>(), SLOT(restoreDefaults()));
    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(sltCurrentIdChanged(int)));
}

bool UIImportApplianceWzd::isValid() const
{
    bool fResult = false;
    if (UIApplianceImportEditorWidget *applianceWidget = field("applianceWidget").value<ImportAppliancePointer>())
        fResult = applianceWidget->isValid();

    return fResult;
}

void UIImportApplianceWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("Appliance Import Wizard"));

    /* Translate 'Restore Defaults' button */
    setButtonText(QWizard::CustomButton1, tr("Restore Defaults"));
    setButtonText(QWizard::FinishButton, tr("Import"));
}

void UIImportApplianceWzd::sltCurrentIdChanged(int iId)
{
    setOption(QWizard::HaveCustomButton1, iId == 1 /* Page #2 */);
}

UIImportApplianceWzdPage1::UIImportApplianceWzdPage1()
{
    /* Decorate page */
    Ui::UIImportApplianceWzdPage1::setupUi(this);

    /* Setup file selector */
    m_pFileSelector->setMode(VBoxFilePathSelectorWidget::Mode_File_Open);
    m_pFileSelector->setHomeDir(vboxGlobal().documentsPath());

    /* Setup validation */
    connect(m_pFileSelector, SIGNAL(pathChanged(const QString &)), this, SIGNAL(completeChanged()));
}

void UIImportApplianceWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIImportApplianceWzdPage1::retranslateUi(this);

    /* Translate the file selector */
    m_pFileSelector->setFileDialogTitle(tr("Select an appliance to import"));
    m_pFileSelector->setFileFilters(tr("Open Virtualization Format (%1)").arg("*.ova *.ovf"));

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the Appliance Import Wizard!"));

    m_pPage1Text1->setText(tr("<p>This wizard will guide you through importing an appliance.</p>"
                              "<p>%1</p><p>VirtualBox currently supports importing appliances "
                              "saved in the Open Virtualization Format (OVF). To continue, "
                              "select the file to import below:</p>")
                           .arg(standardHelpText()));
}

void UIImportApplianceWzdPage1::initializePage()
{
    /* Fill and translate */
    retranslateUi();
}

bool UIImportApplianceWzdPage1::isComplete() const
{
    const QString &strFile = m_pFileSelector->path().toLower();
    return VBoxGlobal::hasAllowedExtension(strFile, VBoxDefs::OVFFileExts) && QFileInfo(m_pFileSelector->path()).exists();
}

bool UIImportApplianceWzdPage1::validatePage()
{
    AssertMsg(!field("applianceWidget").value<ImportAppliancePointer>().isNull(), ("Appliance Widget is not set!\n"));
    UIApplianceImportEditorWidget *applianceWidget = field("applianceWidget").value<ImportAppliancePointer>();

    /* Set the file path only if something had changed */
    if (m_pFileSelector->isModified())
    {
        if (!applianceWidget->setFile(m_pFileSelector->path()))
            return false;
        /* Reset the modified bit afterwards */
        m_pFileSelector->resetModified();
    }

    /* If we have a valid ovf proceed to the appliance settings page */
    return applianceWidget->isValid();
}

UIImportApplianceWzdPage2::UIImportApplianceWzdPage2()
{
    /* Decorate page */
    Ui::UIImportApplianceWzdPage2::setupUi(this);

    /* Register ImportAppliancePointer class */
    qRegisterMetaType<ImportAppliancePointer>();

    /* Register 'applianceWidget' field! */
    registerField("applianceWidget", this, "applianceWidget");
    m_pApplianceWidget = m_pSettingsCnt;

    /* Fill and translate */
    retranslateUi();
}

void UIImportApplianceWzdPage2::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UIImportApplianceWzdPage2::retranslateUi(this);

    /* Wizard page 2 title */
    setTitle(tr("Appliance Import Settings"));
}

bool UIImportApplianceWzdPage2::validatePage()
{
    startProcessing();
    bool fResult = import();
    endProcessing();
    return fResult;
}

bool UIImportApplianceWzdPage2::import()
{
    /* Make sure the final values are puted back */
    m_pSettingsCnt->prepareImport();
    /* Check if there are license agreements the user must confirm */
    QList < QPair <QString, QString> > licAgreements = m_pSettingsCnt->licenseAgreements();
    if (!licAgreements.isEmpty())
    {
        UIImportLicenseViewer ilv(this);
        for (int i = 0; i < licAgreements.size(); ++ i)
        {
            const QPair <QString, QString> &lic = licAgreements.at(i);
            ilv.setContents(lic.first, lic.second);
            if (ilv.exec() == QDialog::Rejected)
                return false;
        }
    }
    /* Now import all virtual systems */
    return m_pSettingsCnt->import();
}

