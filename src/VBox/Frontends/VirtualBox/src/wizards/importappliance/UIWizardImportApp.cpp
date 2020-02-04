/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportApp class implementation.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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
#include <QDialogButtonBox>
#include <QLabel>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialog.h"
#include "QIFileDialog.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic1.h"
#include "UIWizardImportAppPageBasic2.h"
#include "UIWizardImportAppPageExpert.h"

/* COM includes: */
#include "CProgress.h"


/* Import license viewer: */
class UIImportLicenseViewer : public QIDialog
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIImportLicenseViewer(QWidget *pParent)
        : QIDialog(pParent)
    {
        /* Create widgets: */
        QVBoxLayout *pMainLayout = new QVBoxLayout(this);
            pMainLayout->setMargin(12);
            m_pCaption = new QLabel(this);
                m_pCaption->setWordWrap(true);
            m_pLicenseText = new QTextEdit(this);
                m_pLicenseText->setReadOnly(true);
            m_pPrintButton = new QPushButton(this);
            m_pSaveButton = new QPushButton(this);
            m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::No | QDialogButtonBox::Yes, Qt::Horizontal, this);
                m_pButtonBox->addButton(m_pPrintButton, QDialogButtonBox::ActionRole);
                m_pButtonBox->addButton(m_pSaveButton, QDialogButtonBox::ActionRole);
                m_pButtonBox->button(QDialogButtonBox::Yes)->setDefault(true);
        pMainLayout->addWidget(m_pCaption);
        pMainLayout->addWidget(m_pLicenseText);
        pMainLayout->addWidget(m_pButtonBox);

        /* Translate */
        retranslateUi();

        /* Setup connections: */
        connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &UIImportLicenseViewer::reject);
        connect(m_pButtonBox, &QDialogButtonBox::accepted, this, &UIImportLicenseViewer::accept);
        connect(m_pPrintButton, &QPushButton::clicked,     this, &UIImportLicenseViewer::sltPrint);
        connect(m_pSaveButton,  &QPushButton::clicked,     this, &UIImportLicenseViewer::sltSave);
    }

    /* Content setter: */
    void setContents(const QString &strName, const QString &strText)
    {
        m_strName = strName;
        m_strText = strText;
        retranslateUi();
    }

private slots:

    /* Print stuff: */
    void sltPrint()
    {
        QPrinter printer;
        QPrintDialog pd(&printer, this);
        if (pd.exec() == QDialog::Accepted)
            m_pLicenseText->print(&printer);
    }

    /* Save stuff: */
    void sltSave()
    {
        QString fileName = QIFileDialog::getSaveFileName(uiCommon().documentsPath(), tr("Text (*.txt)"),
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

private:

    /* Translation stuff: */
    void retranslateUi()
    {
        /* Translate dialog: */
        setWindowTitle(tr("Software License Agreement"));
        /* Translate widgets: */
        m_pCaption->setText(tr("<b>The virtual system \"%1\" requires that you agree to the terms and conditions "
                               "of the software license agreement shown below.</b><br /><br />Click <b>Agree</b> "
                               "to continue or click <b>Disagree</b> to cancel the import.").arg(m_strName));
        m_pLicenseText->setText(m_strText);
        m_pButtonBox->button(QDialogButtonBox::No)->setText(tr("&Disagree"));
        m_pButtonBox->button(QDialogButtonBox::Yes)->setText(tr("&Agree"));
        m_pPrintButton->setText(tr("&Print..."));
        m_pSaveButton->setText(tr("&Save..."));
    }

    /* Variables: */
    QLabel *m_pCaption;
    QTextEdit *m_pLicenseText;
    QDialogButtonBox *m_pButtonBox;
    QPushButton *m_pPrintButton;
    QPushButton *m_pSaveButton;
    QString m_strName;
    QString m_strText;
};


/*********************************************************************************************************************************
*   Class UIWizardImportApp implementation.                                                                                      *
*********************************************************************************************************************************/

UIWizardImportApp::UIWizardImportApp(QWidget *pParent, bool fImportFromOCIByDefault, const QString &strFileName)
    : UIWizard(pParent, WizardType_ImportAppliance)
    , m_fImportFromOCIByDefault(fImportFromOCIByDefault)
    , m_strFileName(strFileName)
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_ovf_import.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/wizard_ovf_import_bg.png");
#endif /* VBOX_WS_MAC */
}

void UIWizardImportApp::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            if (m_fImportFromOCIByDefault || m_strFileName.isEmpty())
                setPage(Page1, new UIWizardImportAppPageBasic1(m_fImportFromOCIByDefault));
            setPage(Page2, new UIWizardImportAppPageBasic2(m_strFileName));
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardImportAppPageExpert(m_fImportFromOCIByDefault, m_strFileName));
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

bool UIWizardImportApp::isValid() const
{
    bool fResult = false;
    if (UIApplianceImportEditorWidget *pImportApplianceWidget = field("applianceWidget").value<ImportAppliancePointer>())
        fResult = pImportApplianceWidget->isValid();
    return fResult;
}

bool UIWizardImportApp::importAppliance()
{
    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = field("isSourceCloudOne").toBool();
    if (fIsSourceCloudOne)
    {
        /* Acquire prepared appliance: */
        CAppliance comAppliance = field("appliance").value<CAppliance>();
        AssertReturn(!comAppliance.isNull(), false);

        /* No options for cloud VMs for now: */
        QVector<KImportOptions> options;

        /* Initiate import porocedure: */
        CProgress comProgress = comAppliance.ImportMachines(options);

        /* Show error message if necessary: */
        if (!comAppliance.isOk())
            msgCenter().cannotImportAppliance(comAppliance, this);
        else
        {
            /* Show "Import appliance" progress: */
            msgCenter().showModalProgressDialog(comProgress, tr("Importing Appliance ..."), ":/progress_import_90px.png", this, 0);
            if (!comProgress.GetCanceled())
            {
                /* Show error message if necessary: */
                if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                    msgCenter().cannotImportAppliance(comProgress, comAppliance.GetPath(), this);
                else
                    return true;
            }
        }

        /* Failure by default: */
        return false;
    }
    else
    {
        /* Get import appliance widget: */
        UIApplianceImportEditorWidget *pImportApplianceWidget = field("applianceWidget").value<ImportAppliancePointer>();
        /* Make sure the final values are puted back: */
        pImportApplianceWidget->prepareImport();
        /* Check if there are license agreements the user must confirm: */
        QList < QPair <QString, QString> > licAgreements = pImportApplianceWidget->licenseAgreements();
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
        /* Now import all virtual systems: */
        return pImportApplianceWidget->import();
    }
}

void UIWizardImportApp::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Import Virtual Appliance"));
    setButtonText(QWizard::CustomButton2, tr("Restore Defaults"));
    setButtonText(QWizard::FinishButton, tr("Import"));
}

void UIWizardImportApp::sltCurrentIdChanged(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCurrentIdChanged(iId);
    /* Enable 2nd button (Reset to Defaults) for 2nd and Expert pages only! */
    setOption(QWizard::HaveCustomButton2, (mode() == WizardMode_Basic && iId == Page2) ||
                                          (mode() == WizardMode_Expert && iId == PageExpert));
}

void UIWizardImportApp::sltCustomButtonClicked(int iId)
{
    /* Call to base-class: */
    UIWizard::sltCustomButtonClicked(iId);

    /* Handle 2nd button: */
    if (iId == CustomButton2)
    {
        /* Get appliance widget: */
        ImportAppliancePointer pApplianceWidget = field("applianceWidget").value<ImportAppliancePointer>();
        AssertMsg(!pApplianceWidget.isNull(), ("Appliance Widget is not set!\n"));
        /* Reset it to default: */
        pApplianceWidget->restoreDefaults();
    }
}


#include "UIWizardImportApp.moc"
