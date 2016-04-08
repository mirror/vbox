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
*   Class UIApplianceCertificateViewer.                                                                                          *
*********************************************************************************************************************************/

UIApplianceUnverifiedCertificate::UIApplianceUnverifiedCertificate(QWidget *pParent, const CCertificate &certificate)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_certificate(certificate)
    , m_pTextLabel(NULL)
    , m_pTextBrowser(NULL)
{
    /* Prepare: */
    prepare();
}

void UIApplianceUnverifiedCertificate::prepare()
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
            pButtonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);

            pButtonBox->button(QDialogButtonBox::Yes)->setShortcut(Qt::Key_Enter);
            connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));

            //pButtonBox->button(QDialogButtonBox::No)->setShortcut(Qt::Key_Esc);
            connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

            /* Add button-box into layout: */
            pLayout->addWidget(pButtonBox);
        }
    }
    /* Translate UI: */
    retranslateUi();
}

void UIApplianceUnverifiedCertificate::retranslateUi()
{
    /* Translate dialog title: */
    setWindowTitle(tr("Unverifiable Certificate! Continue?"));

    /* Translate text-label caption: */
    if (m_certificate.GetSelfSigned())
        m_pTextLabel->setText(tr("<b>The appliance is signed by an unverified self signed certificate issued by '%1'. "
                                 "We recommend to only proceed with the importing if you are sure you should trust this entity.</b>"
                                 ).arg(m_certificate.GetFriendlyName()));
    else
        m_pTextLabel->setText(tr("<b>The appliance is signed by an unverified certificate issued to '%1'. "
                                 "We recommend to only proceed with the importing if you are sure you should trust this entity.</b>"
                                 ).arg(m_certificate.GetFriendlyName()));

    /* Translate text-browser contents: */
    QStringList info;
    KCertificateVersion ver = m_certificate.GetVersionNumber();
    info << tr("Issuer:               %1").arg(QStringList(m_certificate.GetIssuerName().toList()).join(", "));
    info << tr("Subject:              %1").arg(QStringList(m_certificate.GetSubjectName().toList()).join(", "));
    info << tr("Not Valid Before:     %1").arg(m_certificate.GetValidityPeriodNotBefore());
    info << tr("Not Valid After:      %1").arg(m_certificate.GetValidityPeriodNotAfter());
    info << tr("Serial Number:        %1").arg(m_certificate.GetSerialNumber());
    info << tr("Self-Signed:          %1").arg(m_certificate.GetSelfSigned() ? tr("True") : tr("False"));
    info << tr("Authority (CA):       %1").arg(m_certificate.GetCertificateAuthority() ? tr("True") : tr("False"));
    //info << tr("Trusted:              %1").arg(m_certificate.GetTrusted() ? tr("True") : tr("False")); - no, that's why we're here
    info << tr("Public Algorithm:     %1 (%2)").arg(m_certificate.GetPublicKeyAlgorithm()).arg(m_certificate.GetPublicKeyAlgorithmOID());
    info << tr("Signature Algorithm:  %1 (%2)").arg(m_certificate.GetSignatureAlgorithmName()).arg(m_certificate.GetSignatureAlgorithmOID());
    info << tr("X.509 Version Number: %1").arg(ver);
    m_pTextBrowser->setText(info.join("<br>"));
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
    : m_enmCertText(kCertText_Uninitialized)
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
        m_pCertLabel = new QLabel("<cert label>", this);
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pApplianceWidget);
        pMainLayout->addWidget(m_pCertLabel);
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
    switch (m_enmCertText)
    {
        case kCertText_Unsigned:
            m_pCertLabel->setText(UIWizardImportApp::tr("Appliance is not signed"));
            break;
        case kCertText_IssuedTrusted:
            m_pCertLabel->setText(UIWizardImportApp::tr("Appliance signed by %1 (trusted)").arg(m_strSignedBy));
            break;
        case kCertText_IssuedExpired:
            m_pCertLabel->setText(UIWizardImportApp::tr("Appliance signed by %1 (expired!)").arg(m_strSignedBy));
            break;
        case kCertText_IssuedUnverified:
            m_pCertLabel->setText(UIWizardImportApp::tr("Unverified signature by %1!").arg(m_strSignedBy));
            break;
        case kCertText_SelfSignedTrusted:
            m_pCertLabel->setText(UIWizardImportApp::tr("Self signed by %1 (trusted)").arg(m_strSignedBy));
            break;
        case kCertText_SelfSignedExpired:
            m_pCertLabel->setText(UIWizardImportApp::tr("Self signed by %1 (expired!)").arg(m_strSignedBy));
            break;
        case kCertText_SelfSignedUnverified:
            m_pCertLabel->setText(UIWizardImportApp::tr("Unverified self signed signature by %1!").arg(m_strSignedBy));
            break;
        default:
            AssertFailed();
        case kCertText_Uninitialized:
            m_pCertLabel->setText("<uninitialized page>");
            break;
    }
}

void UIWizardImportAppPageBasic2::initializePage()
{
    /* Acquire appliance and certificate: */
    CAppliance *pAppliance = m_pApplianceWidget->appliance();
    CCertificate certificate = pAppliance->GetCertificate();
    if (certificate.isNull())
        m_enmCertText = kCertText_Unsigned;
    else
    {
        /* Pick a 'signed-by' name. */
        m_strSignedBy = certificate.GetFriendlyName();

        /*
         * If trusted, just select the right message.
         */
        if (certificate.GetTrusted())
        {
            if (certificate.GetSelfSigned())
                m_enmCertText = !certificate.GetExpired() ? kCertText_SelfSignedTrusted : kCertText_SelfSignedExpired;
            else
                m_enmCertText = !certificate.GetExpired() ? kCertText_IssuedTrusted     : kCertText_IssuedExpired;
        }
        else
        {
            /*
             * Not trusted!  Must ask the user whether to continue in this case.
             */
            m_enmCertText = !certificate.GetExpired() ? kCertText_SelfSignedUnverified : kCertText_SelfSignedUnverified;
            retranslateUi();

            /* Instantiate the dialog: */
            QPointer<UIApplianceUnverifiedCertificate> pDialog = new UIApplianceUnverifiedCertificate(this, certificate);
            AssertPtrReturnVoid(pDialog.data());

            /* Show viewer in modal mode: */
            int iResultCode = pDialog->exec();

            /* Leave if destroyed prematurely: */
            if (!pDialog)
                return; /** @todo r=bird: what happened to this dialog in that case??, will check this case later. */
            /* Delete viewer: */
            if (pDialog)
            {
                delete pDialog;
                pDialog = NULL;
            }

            /* Dismiss the entire import-appliance wizard if user rejects certificate: */
            if (iResultCode == QDialog::Rejected)
                wizard()->reject();
        }
    }

    /* Translate page: */
    retranslateUi();
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

