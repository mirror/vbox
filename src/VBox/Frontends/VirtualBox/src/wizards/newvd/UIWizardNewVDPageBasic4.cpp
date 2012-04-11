/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic4 class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes: */
#include "UIWizardNewVDPageBasic4.h"
#include "UIWizardNewVD.h"
#include "VBoxGlobal.h"
#include "QIRichTextLabel.h"

UIWizardNewVDPageBasic4::UIWizardNewVDPageBasic4()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel1 = new QIRichTextLabel(this);
        m_pSummaryText = new QIRichTextLabel(this);
        m_pLabel2 = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel1);
    pMainLayout->addWidget(m_pSummaryText);
    pMainLayout->addWidget(m_pLabel2);
    pMainLayout->addStretch();

    /* Register CMedium class: */
    qRegisterMetaType<CMedium>();
    /* Register 'virtualDisk' field: */
    registerField("virtualDisk", this, "virtualDisk");
}

void UIWizardNewVDPageBasic4::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Summary"));

    m_pLabel1->setText(UIWizardNewVD::tr("You are going to create a new virtual disk with the following parameters:"));
    m_pLabel2->setText(UIWizardNewVD::tr("If the above settings are correct, press the <b>%1</b> button. "
                                         "Once you press it the new virtual disk file will be created.")
                                        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::FinishButton)))));

    /* Compose summary: */
    QString strSummary;
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    qulonglong uVariant = field("mediumVariant").toULongLong();
    QString strMediumPath = field("mediumPath").toString();
    QString sizeFormatted = VBoxGlobal::formatSize(field("mediumSize").toULongLong());
    QString sizeUnformatted = UIWizardNewVD::tr("%1 B").arg(field("mediumSize").toULongLong());
    strSummary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td><nobr>%2</nobr></td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td><nobr>%4</nobr></td></tr>"
        "<tr><td><nobr>%5: </nobr></td><td><nobr>%6</nobr></td></tr>"
        "<tr><td><nobr>%7: </nobr></td><td><nobr>%8 (%9)</nobr></td></tr>"
    )
    .arg(UIWizardNewVD::tr("File type", "summary"), mediumFormat.isNull() ? QString() : VBoxGlobal::removeAccelMark(UIWizardNewVD::fullFormatName(mediumFormat.GetName())))
    .arg(UIWizardNewVD::tr("Details", "summary"), vboxGlobal().toString((KMediumVariant)uVariant))
    .arg(UIWizardNewVD::tr("Location", "summary"), strMediumPath)
    .arg(UIWizardNewVD::tr("Size", "summary"), sizeFormatted, sizeUnformatted);
    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + strSummary + "</table>");
}

void UIWizardNewVDPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Summary should have focus initially: */
    m_pSummaryText->setFocus();
}

bool UIWizardNewVDPageBasic4::validatePage()
{
    /* Try to create virtual-disk: */
    startProcessing();
    bool fResult = qobject_cast<UIWizardNewVD*>(wizard())->createVirtualDisk();
    endProcessing();
    return fResult;
}

