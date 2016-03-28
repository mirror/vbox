/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic2 class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Global includes: */
# include <QVBoxLayout>
# include <QTextBrowser>
# include <QPushButton>

/* Local includes: */
# include "UIWizardImportAppPageBasic2.h"
# include "UIWizardImportApp.h"
# include "QIRichTextLabel.h"
# include "QIDialogButtonBox.h"

/* COM includes: */
# include "CAppliance.h"
# include "CCertificate.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardImportAppPage2::UIWizardImportAppPage2()
{
}

UIWizardImportAppPageBasic2::UIWizardImportAppPageBasic2(const QString &strFileName)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pApplianceWidget = new UIApplianceImportEditorWidget(this);
        {
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            m_pApplianceWidget->setFile(strFileName);
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pApplianceWidget);
    }

    /* Register classes: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register fields: */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardImportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardImportApp::tr("These are the virtual machines contained in the appliance "
                                            "and the suggested settings of the imported VirtualBox machines. "
                                            "You can change many of the properties shown by double-clicking "
                                            "on the items and disable others using the check boxes below."));
}

void UIWizardImportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
    /* Create dialog: */
    QDialog *pDialog = new QDialog(this, Qt::Dialog);
    AssertPtrReturnVoid(pDialog);

    CAppliance* l_appl = m_pApplianceWidget->appliance();
    CCertificate aCertificateInfo = l_appl->GetCertificate();
    /* check emptyness of certificate. object exists but hasn't fullfilled */
    BOOL fExisting = false;
    BOOL fVerified = false;
    fExisting = aCertificateInfo.CheckExistence();
    fVerified = aCertificateInfo.IsVerified();
    if (fExisting && !fVerified)
    {
        /* Create layout: */
        QVBoxLayout *pLayout = new QVBoxLayout(pDialog);
        AssertPtrReturnVoid(pLayout);
        {
            /* Prepare dialog: */
            pDialog->resize(500, 500);
            /* Create text-field: */
            QTextBrowser *pTextBrowser = new QTextBrowser;
            AssertPtrReturnVoid(pTextBrowser);

            QString fullCertInfo("");

            {
                /* Configure text-field: */
                // TODO: Load text to text-browser here:
                QString data = aCertificateInfo.GetVersionNumber();
                fullCertInfo.append("Certificate version number: ").append(data).append("\n\n");

                data = aCertificateInfo.GetSerialNumber();
                fullCertInfo.append("Certificate serial number: 0x").append(data).append("\n\n");

                bool flag = aCertificateInfo.GetCertificateAuthority();
                data = (flag == true) ? "True":"False";
                fullCertInfo.append("Certificate Authority (CA): ").append(data).append("\n\n");

                flag = aCertificateInfo.GetSelfSigned();
                data = (flag == true) ? "True":"False";
                fullCertInfo.append("Certificate Self-Signed : ").append(data).append("\n\n");

                flag = aCertificateInfo.GetTrusted();
                data = (flag == true) ? "True":"False";
                fullCertInfo.append("Certificate Trusted: ").append(data).append("\n\n");

                QVector<QString> name = aCertificateInfo.GetIssuerName();
                fullCertInfo.append("Certificate Issuer: ").append("\n");
                if (!name.isEmpty())
                {
                    for (int i = 0; i < name.size()-1; ++i)
                    {
                        fullCertInfo.append(name.at(i)).append(", ");
                    }

                    QString field = name.at(name.size()-1);
                    fullCertInfo.append(field).append("\n\n");
                }
                name.clear();
                name = aCertificateInfo.GetSubjectName();
                fullCertInfo.append("Certificate Subject: ").append("\n");
                if (!name.isEmpty())
                {
                    for (int i = 0; i < name.size()-1; ++i)
                    {
                        fullCertInfo.append(name.at(i)).append(", ");
                    }

                    QString field = name.at(name.size()-1);
                    fullCertInfo.append(field).append("\n\n");
                }

                data = aCertificateInfo.GetPublicKeyAlgorithm();
                fullCertInfo.append("Certificate Public Algorithm: ").append(data).append("\n\n");

                data = aCertificateInfo.GetSignatureAlgorithmName();
                fullCertInfo.append("Certificate Signature Algorithm: ").append(data).append("\n\n");

                data = aCertificateInfo.GetSignatureAlgorithmOID();
                fullCertInfo.append("Certificate Signature Algorithm OID: ").append(data).append("\n\n");

                data = aCertificateInfo.GetValidityPeriodNotBefore();
                fullCertInfo.append("Certificate Validity Period Not Before: ").append(data).append("\n\n");

                data = aCertificateInfo.GetValidityPeriodNotAfter();
                fullCertInfo.append("Certificate Validity Period Not After: ").append(data).append("\n\n");

                pTextBrowser->setText(fullCertInfo);
                /* Add text-browser into layout: */
                pLayout->addWidget(pTextBrowser);
            }
            /* Create button-box: */
            QIDialogButtonBox *pButtonBox = new QIDialogButtonBox;
            AssertPtrReturnVoid(pButtonBox);
            {
                /* Configure button-box: */
                pButtonBox->setStandardButtons(QDialogButtonBox::Ok);
                pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(Qt::Key_Enter);
                connect(pButtonBox, SIGNAL(accepted()), pDialog, SLOT(close()));
                /* Add button-box into layout: */
                pLayout->addWidget(pButtonBox);
            }
        }
        /* Show dialog in modal mode: */
        pDialog->exec();
        /* Delete dialog finally: */
        delete pDialog;
        pDialog = 0;
    }
}

void UIWizardImportAppPageBasic2::cleanupPage()
{
    /* Rollback settings: */
    m_pApplianceWidget->restoreDefaults();
    /* Call to base-class: */
    UIWizardPage::cleanupPage();
}

bool UIWizardImportAppPageBasic2::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to import appliance: */
    if (fResult)
        fResult = qobject_cast<UIWizardImportApp*>(wizard())->importAppliance();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

