/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardImportAppPageBasic2 class implementation.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIRichTextLabel.h"
#include "UIWizardImportApp.h"
#include "UIWizardImportAppPageBasic2.h"

/* COM includes: */
#include "CAppliance.h"
#include "CCertificate.h"


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
    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create label: */
        m_pLabel = new QIRichTextLabel(this);
        if (m_pLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pLabel);
        }

        /* Create appliance widget: */
        m_pApplianceWidget = new UIApplianceImportEditorWidget(this);
        {
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            m_pApplianceWidget->setFile(strFileName);

            /* Add into layout: */
            pMainLayout->addWidget(m_pApplianceWidget);
        }

        /* Create certificate label: */
        m_pCertLabel = new QLabel("<cert label>", this);
        if (m_pCertLabel)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pCertLabel);
        }
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
            RT_FALL_THRU();
        case kCertText_Uninitialized:
            m_pCertLabel->setText("<uninitialized page>");
            break;
    }
}

void UIWizardImportAppPageBasic2::initializePage()
{
    /* Acquire appliance: */
    CAppliance *pAppliance = m_pApplianceWidget->appliance();

    /* Check if pAppliance is alive. If not just return here.
     * This prevents crashes when an invalid ova file is supllied: */
    if (!pAppliance)
    {
        if (wizard())
            wizard()->reject();
        return;
    }

    /* Acquire certificate: */
    CCertificate comCertificate = pAppliance->GetCertificate();
    if (comCertificate.isNull())
        m_enmCertText = kCertText_Unsigned;
    else
    {
        /* Pick a 'signed-by' name: */
        m_strSignedBy = comCertificate.GetFriendlyName();

        /* If trusted, just select the right message: */
        if (comCertificate.GetTrusted())
        {
            if (comCertificate.GetSelfSigned())
                m_enmCertText = !comCertificate.GetExpired() ? kCertText_SelfSignedTrusted : kCertText_SelfSignedExpired;
            else
                m_enmCertText = !comCertificate.GetExpired() ? kCertText_IssuedTrusted     : kCertText_IssuedExpired;
        }
        else
        {
            /* Not trusted!  Must ask the user whether to continue in this case: */
            m_enmCertText = comCertificate.GetSelfSigned() ? kCertText_SelfSignedUnverified : kCertText_IssuedUnverified;

            /* Translate page early: */
            retranslateUi();

            /* Instantiate the dialog: */
            QPointer<UIApplianceUnverifiedCertificateViewer> pDialog = new UIApplianceUnverifiedCertificateViewer(this, comCertificate);
            AssertPtrReturnVoid(pDialog.data());

            /* Show viewer in modal mode: */
            const int iResultCode = pDialog->exec();

            /* Leave if viewer destroyed prematurely: */
            if (!pDialog)
                return;
            /* Delete viewer finally: */
            delete pDialog;

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


/*********************************************************************************************************************************
*   Class UIApplianceUnverifiedCertificateViewer implementation.                                                                 *
*********************************************************************************************************************************/

UIApplianceUnverifiedCertificateViewer::UIApplianceUnverifiedCertificateViewer(QWidget *pParent, const CCertificate &comCertificate)
    : QIWithRetranslateUI<QIDialog>(pParent)
    , m_comCertificate(comCertificate)
    , m_pTextLabel(0)
    , m_pTextBrowser(0)
{
    prepare();
}

void UIApplianceUnverifiedCertificateViewer::prepare()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /* Create text-label: */
        m_pTextLabel = new QLabel;
        if (m_pTextLabel)
        {
            m_pTextLabel->setWordWrap(true);

            /* Add into layout: */
            pLayout->addWidget(m_pTextLabel);
        }

        /* Create text-browser: */
        m_pTextBrowser = new QTextBrowser;
        if (m_pTextBrowser)
        {
            m_pTextBrowser->setMinimumSize(500, 300);

            /* Add into layout: */
            pLayout->addWidget(m_pTextBrowser);
        }

        /* Create button-box: */
        QIDialogButtonBox *pButtonBox = new QIDialogButtonBox;
        if (pButtonBox)
        {
            pButtonBox->setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
            pButtonBox->button(QDialogButtonBox::Yes)->setShortcut(Qt::Key_Enter);
            //pButtonBox->button(QDialogButtonBox::No)->setShortcut(Qt::Key_Esc);
            connect(pButtonBox, &QIDialogButtonBox::accepted, this, &UIApplianceUnverifiedCertificateViewer::accept);
            connect(pButtonBox, &QIDialogButtonBox::rejected, this, &UIApplianceUnverifiedCertificateViewer::reject);

            /* Add into layout: */
            pLayout->addWidget(pButtonBox);
        }
    }

    /* Translate UI: */
    retranslateUi();
}

void UIApplianceUnverifiedCertificateViewer::retranslateUi()
{
    /* Translate dialog title: */
    setWindowTitle(tr("Unverifiable Certificate! Continue?"));

    /* Translate text-label caption: */
    if (m_comCertificate.GetSelfSigned())
        m_pTextLabel->setText(tr("<b>The appliance is signed by an unverified self signed certificate issued by '%1'. "
                                 "We recommend to only proceed with the importing if you are sure you should trust this entity.</b>"
                                 ).arg(m_comCertificate.GetFriendlyName()));
    else
        m_pTextLabel->setText(tr("<b>The appliance is signed by an unverified certificate issued to '%1'. "
                                 "We recommend to only proceed with the importing if you are sure you should trust this entity.</b>"
                                 ).arg(m_comCertificate.GetFriendlyName()));

    /* Translate text-browser contents: */
    const QString strTemplateRow = tr("<tr><td>%1:</td><td>%2</td></tr>", "key: value");
    QString strTableContent;
    strTableContent += strTemplateRow.arg(tr("Issuer"),               QStringList(m_comCertificate.GetIssuerName().toList()).join(", "));
    strTableContent += strTemplateRow.arg(tr("Subject"),              QStringList(m_comCertificate.GetSubjectName().toList()).join(", "));
    strTableContent += strTemplateRow.arg(tr("Not Valid Before"),     m_comCertificate.GetValidityPeriodNotBefore());
    strTableContent += strTemplateRow.arg(tr("Not Valid After"),      m_comCertificate.GetValidityPeriodNotAfter());
    strTableContent += strTemplateRow.arg(tr("Serial Number"),        m_comCertificate.GetSerialNumber());
    strTableContent += strTemplateRow.arg(tr("Self-Signed"),          m_comCertificate.GetSelfSigned() ? tr("True") : tr("False"));
    strTableContent += strTemplateRow.arg(tr("Authority (CA)"),       m_comCertificate.GetCertificateAuthority() ? tr("True") : tr("False"));
//    strTableContent += strTemplateRow.arg(tr("Trusted"),              m_comCertificate.GetTrusted() ? tr("True") : tr("False"));
    strTableContent += strTemplateRow.arg(tr("Public Algorithm"),     tr("%1 (%2)", "value (clarification)").arg(m_comCertificate.GetPublicKeyAlgorithm()).arg(m_comCertificate.GetPublicKeyAlgorithmOID()));
    strTableContent += strTemplateRow.arg(tr("Signature Algorithm"),  tr("%1 (%2)", "value (clarification)").arg(m_comCertificate.GetSignatureAlgorithmName()).arg(m_comCertificate.GetSignatureAlgorithmOID()));
    strTableContent += strTemplateRow.arg(tr("X.509 Version Number"), QString::number(m_comCertificate.GetVersionNumber()));
    m_pTextBrowser->setText(QString("<table>%1</table>").arg(strTableContent));
}
