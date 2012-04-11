/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic1 class implementation
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

/* Global includes: */
#include <QVBoxLayout>
#include <QRadioButton>
#include <QGroupBox>
#include <QButtonGroup>

/* Local includes: */
#include "UIWizardNewVDPageBasic1.h"
#include "UIWizardNewVD.h"
#include "VBoxGlobal.h"
#include "QIRichTextLabel.h"

UIWizardNewVDPageBasic1::UIWizardNewVDPageBasic1()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pFormatContainer = new QGroupBox(this);
            m_pFormatContainer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pFormatsLayout = new QVBoxLayout(m_pFormatContainer);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pFormatContainer);
    pMainLayout->addStretch();

    /* Greate button group: */
    m_pButtonGroup = new QButtonGroup(this);

    /* Enumerate supportable formats: */
    CSystemProperties systemProperties = vboxGlobal().virtualBox().GetSystemProperties();
    const QVector<CMediumFormat> &medFormats = systemProperties.GetMediumFormats();
    /* Search for default (VDI) format first: */
    for (int i = 0; i < medFormats.size(); ++i)
    {
        /* Get iterated medium format: */
        const CMediumFormat &medFormat = medFormats[i];
        QString strFormatName(medFormat.GetName());
        if (strFormatName == "VDI")
        {
            QRadioButton *pButton = addFormatButton(pFormatsLayout, medFormat);
            if (pButton)
            {
                m_formats << medFormat;
                m_formatNames << strFormatName;
                m_pButtonGroup->addButton(pButton, m_formatNames.size() - 1);
            }
        }
    }
    /* Look for other formats: */
    for (int i = 0; i < medFormats.size(); ++i)
    {
        /* Get iterated medium format: */
        const CMediumFormat &medFormat = medFormats[i];
        QString strFormatName(medFormat.GetName());
        if (strFormatName != "VDI")
        {
            QRadioButton *pButton = addFormatButton(pFormatsLayout, medFormat);
            if (pButton)
            {
                m_formats << medFormat;
                m_formatNames << strFormatName;
                m_pButtonGroup->addButton(pButton, m_formatNames.size() - 1);
            }
        }
    }

    /* Setup connections: */
    connect(m_pButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SIGNAL(completeChanged()));

    /* Initialize connections: */
    m_pButtonGroup->button(0)->click();
    m_pButtonGroup->button(0)->setFocus();

    /* Register CMediumFormat class: */
    qRegisterMetaType<CMediumFormat>();
    /* Register 'mediumFormat' field: */
    registerField("mediumFormat", this, "mediumFormat");
}

void UIWizardNewVDPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Welcome to the virtual disk creation wizard"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVD::tr("<p>This wizard will help you to create a new virtual disk for your virtual machine.</p>"));
    m_pLabel->setText(m_pLabel->text() + QString("<p>%1</p>").arg(standardHelpText()));
    m_pLabel->setText(m_pLabel->text() + UIWizardNewVD::tr("<p>Please choose the type of file that you would like "
                                                           "to use for the new virtual disk. If you do not need "
                                                           "to use it with other virtualization software you can "
                                                           "leave this setting unchanged.</p>"));
    m_pFormatContainer->setTitle(UIWizardNewVD::tr("File type"));

    /* Translate 'format' buttons: */
    QList<QAbstractButton*> buttons = m_pButtonGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i)
    {
        QAbstractButton *pButton = buttons[i];
        pButton->setText(UIWizardNewVD::fullFormatName(m_formatNames[m_pButtonGroup->id(pButton)]));
    }
}

void UIWizardNewVDPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewVDPageBasic1::isComplete() const
{
    return !mediumFormat().isNull();
}

int UIWizardNewVDPageBasic1::nextId() const
{
    CMediumFormat medFormat = mediumFormat();
    ULONG uCapabilities = medFormat.GetCapabilities();
    int cTest = 0;
    if (uCapabilities & KMediumFormatCapabilities_CreateDynamic)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateFixed)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateSplit2G)
        ++cTest;
    if (cTest > 1)
        return UIWizardNewVD::Page2;
    return UIWizardNewVD::Page3;
}

QRadioButton* UIWizardNewVDPageBasic1::addFormatButton(QVBoxLayout *pFormatsLayout, CMediumFormat medFormat)
{
    /* Check that medium format supports creation: */
    ULONG uFormatCapabilities = medFormat.GetCapabilities();
    if (!(uFormatCapabilities & MediumFormatCapabilities_CreateFixed ||
          uFormatCapabilities & MediumFormatCapabilities_CreateDynamic))
        return 0;

    /* Check that medium format supports creation of virtual hard-disks: */
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    medFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
    if (!deviceTypes.contains(KDeviceType_HardDisk))
        return 0;

    /* Create/add corresponding radio-button: */
    QRadioButton *pFormatButton = new QRadioButton(m_pFormatContainer);
    pFormatsLayout->addWidget(pFormatButton);
    return pFormatButton;
}

CMediumFormat UIWizardNewVDPageBasic1::mediumFormat() const
{
    return m_pButtonGroup->checkedButton() ? m_formats[m_pButtonGroup->checkedId()] : CMediumFormat();
}

void UIWizardNewVDPageBasic1::setMediumFormat(const CMediumFormat &mediumFormat)
{
    int iPosition = m_formats.indexOf(mediumFormat);
    if (iPosition >= 0)
    {
        m_pButtonGroup->button(iPosition)->click();
        m_pButtonGroup->button(iPosition)->setFocus();
    }
}

