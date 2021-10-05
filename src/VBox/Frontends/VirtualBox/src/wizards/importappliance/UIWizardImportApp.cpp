/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportApp class implementation.
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
#include "UINotificationCenter.h"
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
    enableHelpButton("ovf");
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

CAppliance UIWizardImportApp::appliance() const
{
    return m_comAppliance;
}

bool UIWizardImportApp::setFile(const QString &strFileName)
{
    /* Clear object: */
    m_comAppliance = CAppliance();

    if (strFileName.isEmpty())
        return false;

    /* Create an appliance object: */
    CVirtualBox comVBox = uiCommon().virtualBox();
    CAppliance comAppliance = comVBox.CreateAppliance();
    if (!comVBox.isOk())
    {
        msgCenter().cannotCreateAppliance(comVBox, this);
        return false;
    }

    /* Read the file to appliance: */
    CProgress comProgress = comAppliance.Read(strFileName);
    if (!comAppliance.isOk())
    {
        msgCenter().cannotImportAppliance(comAppliance, this);
        return false;
    }

    /* Show Reading Appliance progress: */
    msgCenter().showModalProgressDialog(comProgress, tr("Reading Appliance ..."),
                                        ":/progress_reading_appliance_90px.png", this);
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotImportAppliance(comProgress, comAppliance.GetPath(), this);
        return false;
    }

    /* Now we have to interpret that stuff: */
    comAppliance.Interpret();
    if (!comAppliance.isOk())
    {
        msgCenter().cannotImportAppliance(comAppliance, this);
        return false;
    }

    /* Remember appliance: */
    m_comAppliance = comAppliance;

    /* Success finally: */
    return true;
}

bool UIWizardImportApp::isValid() const
{
    return m_comAppliance.isNotNull();
}

bool UIWizardImportApp::importAppliance()
{
    /* Check whether there was cloud source selected: */
    const bool fIsSourceCloudOne = field("isSourceCloudOne").toBool();
    if (fIsSourceCloudOne)
    {
        /* Acquire prepared appliance: */
        CAppliance comAppliance = field("cloudAppliance").value<CAppliance>();
        AssertReturn(!comAppliance.isNull(), false);

        /* No options for cloud VMs for now: */
        QVector<KImportOptions> options;

        /* Import appliance: */
        UINotificationProgressApplianceImport *pNotification = new UINotificationProgressApplianceImport(comAppliance,
                                                                                                         options);
        gpNotificationCenter->append(pNotification);

        /* Positive: */
        return true;
    }
    else
    {
        /* Check if there are license agreements the user must confirm: */
        QList < QPair <QString, QString> > licAgreements = licenseAgreements();
        if (!licAgreements.isEmpty())
        {
            UIImportLicenseViewer ilv(this);
            for (int i = 0; i < licAgreements.size(); ++ i)
            {
                const QPair<QString, QString> &lic = licAgreements.at(i);
                ilv.setContents(lic.first, lic.second);
                if (ilv.exec() == QDialog::Rejected)
                    return false;
            }
        }

        /* Gather import options: */
        QVector<KImportOptions> options;
        const MACAddressImportPolicy enmPolicy = field("macAddressImportPolicy").value<MACAddressImportPolicy>();
        switch (enmPolicy)
        {
            case MACAddressImportPolicy_KeepAllMACs: options.append(KImportOptions_KeepAllMACs); break;
            case MACAddressImportPolicy_KeepNATMACs: options.append(KImportOptions_KeepNATMACs); break;
            default: break;
        }
        bool fImportHDsAsVDI = field("importHDsAsVDI").toBool();
        if (fImportHDsAsVDI)
            options.append(KImportOptions_ImportToVDI);

        /* Import appliance: */
        UINotificationProgressApplianceImport *pNotification = new UINotificationProgressApplianceImport(m_comAppliance,
                                                                                                         options);
        gpNotificationCenter->append(pNotification);

        /* Positive: */
        return true;
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

QList<QPair<QString, QString> > UIWizardImportApp::licenseAgreements() const
{
    QList<QPair<QString, QString> > list;

    foreach (CVirtualSystemDescription comVsd, m_comAppliance.GetVirtualSystemDescriptions())
    {
        QVector<QString> strLicense;
        strLicense = comVsd.GetValuesByType(KVirtualSystemDescriptionType_License,
                                            KVirtualSystemDescriptionValueType_Original);
        if (!strLicense.isEmpty())
        {
            QVector<QString> strName;
            strName = comVsd.GetValuesByType(KVirtualSystemDescriptionType_Name,
                                             KVirtualSystemDescriptionValueType_Auto);
            list << QPair<QString, QString>(strName.first(), strLicense.first());
        }
    }

    return list;
}


#include "UIWizardImportApp.moc"
