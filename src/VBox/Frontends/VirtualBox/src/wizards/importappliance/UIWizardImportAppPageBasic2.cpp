/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
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
# include <QPointer>

/* Local includes: */
# include "UIWizardImportAppPageBasic2.h"
# include "UIWizardImportApp.h"
# include "QIRichTextLabel.h"
# include "QIDialogButtonBox.h"

/* COM includes: */
# include "CAppliance.h"
# include "CCertificate.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class UIApplianceCertificateViewer implementation.                                                                           *
*********************************************************************************************************************************/

UIApplianceCertificateViewer::UIApplianceCertificateViewer(QWidget *pParent, const CCertificate &certificate)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_certificate(certificate)
    , m_pTextLabel(0)
    , m_pTextBrowser(0)
{
    /* Prepare: */
    prepare();
}

void UIApplianceCertificateViewer::prepare()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Create text-label: */
        m_pTextLabel = new QLabel;
        AssertPtrReturnVoid(m_pTextLabel);
        {
            /* Configure text-label: */
            m_pTextLabel->setWordWrap(true);
            /* Add text-label into layout: */
            pLayout->addWidget(m_pTextLabel);
        }
        /* Create text-browser: */
        m_pTextBrowser = new QTextBrowser;
        AssertPtrReturnVoid(m_pTextBrowser);
        {
            /* Configure text-browser: */
            m_pTextBrowser->setMinimumSize(500, 300);
            /* Add text-browser into layout: */
            pLayout->addWidget(m_pTextBrowser);
        }
        /* Create button-box: */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox;
        AssertPtrReturnVoid(pButtonBox);
        {
            /* Configure button-box: */
            pButtonBox->setStandardButtons(QDialogButtonBox::Ok);
            pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(Qt::Key_Enter);
            connect(pButtonBox, SIGNAL(accepted()), this, SLOT(close()));
            /* Add button-box into layout: */
            pLayout->addWidget(pButtonBox);
        }
    }
    /* Translate UI: */
    retranslateUi();
}

void UIApplianceCertificateViewer::retranslateUi()
{
    /* Translate dialog title: */
    setWindowTitle(tr("Certificate Information"));
    /* Translate text-label caption: */
    m_pTextLabel->setText(tr("<b>The X509 certificate exists but hasn't been verified or trusted. "
                             "You can proceed with the importing but should understand the risks. "
                             "If you are not sure - just stop here and interrupt the importing process.</b>"));
    /* Translate text-browser contents: */
    QStringList info;
    info << tr("Certificate Version Number: %1").arg(m_certificate.GetVersionNumber());
    info << tr("Certificate Serial Number: 0x%1").arg(m_certificate.GetSerialNumber());
    info << tr("Certificate Authority (CA): %1").arg(m_certificate.GetCertificateAuthority() ? tr("True") : tr("False"));
    info << tr("Certificate Self-Signed: %1").arg(m_certificate.GetSelfSigned() ? tr("True") : tr("False"));
    info << tr("Certificate Trusted: %1").arg(m_certificate.GetTrusted() ? tr("True") : tr("False"));
    info << tr("Certificate Issuer: %1").arg(QStringList(m_certificate.GetIssuerName().toList()).join(", "));
    info << tr("Certificate Subject: %1").arg(QStringList(m_certificate.GetSubjectName().toList()).join(", "));
    info << tr("Certificate Public Algorithm: %1").arg(m_certificate.GetPublicKeyAlgorithm());
    info << tr("Certificate Signature Algorithm: %1").arg(m_certificate.GetSignatureAlgorithmName());
    info << tr("Certificate Signature Algorithm OID: %1").arg(m_certificate.GetSignatureAlgorithmOID());
    info << tr("Certificate Validity Period Not Before: %1").arg(m_certificate.GetValidityPeriodNotBefore());
    info << tr("Certificate Validity Period Not After: %1").arg(m_certificate.GetValidityPeriodNotAfter());
    m_pTextBrowser->setText(info.join("<br><br>"));
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPage2 implementation.                                                                                 *
*********************************************************************************************************************************/

UIWizardImportAppPage2::UIWizardImportAppPage2()
{
}


/*********************************************************************************************************************************
*   Class UIWizardImportAppPageBasic2 implementation.                                                                            *
*********************************************************************************************************************************/

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

    /* Acquire appliance and certificate: */
    CAppliance *pAppliance = m_pApplianceWidget->appliance();
    CCertificate certificate = pAppliance->GetCertificate();
    /* Check whether certificate exists and verified: */
    if (certificate.CheckExistence() && !certificate.IsVerified())
    {
        /* Create certificate viewer to notify user about it is not verified: */
        QPointer<UIApplianceCertificateViewer> pDialog =
            new UIApplianceCertificateViewer(this, certificate);
        AssertPtrReturnVoid(pDialog.data());
        {
            /* Show viewer in modal mode: */
            pDialog->exec();
            /* Leave if destroyed prematurely: */
            if (!pDialog)
                return;
            /* Delete viewer finally: */
            delete pDialog;
            pDialog = 0;
        }
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

