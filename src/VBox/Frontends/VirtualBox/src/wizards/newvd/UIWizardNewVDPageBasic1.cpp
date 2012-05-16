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
#include <QButtonGroup>
#include <QGroupBox>
#include <QRadioButton>

/* Local includes: */
#include "UIWizardNewVDPageBasic1.h"
#include "UIWizardNewVD.h"
#include "VBoxGlobal.h"
#include "QIRichTextLabel.h"

UIWizardNewVDPage1::UIWizardNewVDPage1()
{
}

QRadioButton* UIWizardNewVDPage1::addFormatButton(QVBoxLayout *pFormatsLayout, CMediumFormat mf)
{
    /* Check that medium format supports creation: */
    ULONG uFormatCapabilities = mf.GetCapabilities();
    if (!(uFormatCapabilities & MediumFormatCapabilities_CreateFixed ||
          uFormatCapabilities & MediumFormatCapabilities_CreateDynamic))
        return 0;

    /* Check that medium format supports creation of virtual hard-disks: */
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    mf.DescribeFileExtensions(fileExtensions, deviceTypes);
    if (!deviceTypes.contains(KDeviceType_HardDisk))
        return 0;

    /* Create/add corresponding radio-button: */
    QRadioButton *pFormatButton = new QRadioButton(m_pFormatCnt);
    pFormatsLayout->addWidget(pFormatButton);
    return pFormatButton;
}

CMediumFormat UIWizardNewVDPage1::mediumFormat() const
{
    return m_pFormatButtonGroup->checkedButton() ? m_formats[m_pFormatButtonGroup->checkedId()] : CMediumFormat();
}

void UIWizardNewVDPage1::setMediumFormat(const CMediumFormat &mediumFormat)
{
    int iPosition = m_formats.indexOf(mediumFormat);
    if (iPosition >= 0)
    {
        m_pFormatButtonGroup->button(iPosition)->click();
        m_pFormatButtonGroup->button(iPosition)->setFocus();
    }
}

UIWizardNewVDPageBasic1::UIWizardNewVDPageBasic1()
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pFormatCnt = new QGroupBox(this);
        {
            m_pFormatCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pFormatButtonGroup = new QButtonGroup(m_pFormatCnt);
            {
                QVBoxLayout *pFormatsLayout = new QVBoxLayout(m_pFormatCnt);
                {
                    CSystemProperties systemProperties = vboxGlobal().virtualBox().GetSystemProperties();
                    const QVector<CMediumFormat> &medFormats = systemProperties.GetMediumFormats();
                    for (int i = 0; i < medFormats.size(); ++i)
                    {
                        const CMediumFormat &medFormat = medFormats[i];
                        QString strFormatName(medFormat.GetName());
                        if (strFormatName == "VDI")
                        {
                            QRadioButton *pButton = addFormatButton(pFormatsLayout, medFormat);
                            if (pButton)
                            {
                                m_formats << medFormat;
                                m_formatNames << strFormatName;
                                m_pFormatButtonGroup->addButton(pButton, m_formatNames.size() - 1);
                            }
                        }
                    }
                    for (int i = 0; i < medFormats.size(); ++i)
                    {
                        const CMediumFormat &medFormat = medFormats[i];
                        QString strFormatName(medFormat.GetName());
                        if (strFormatName != "VDI")
                        {
                            QRadioButton *pButton = addFormatButton(pFormatsLayout, medFormat);
                            if (pButton)
                            {
                                m_formats << medFormat;
                                m_formatNames << strFormatName;
                                m_pFormatButtonGroup->addButton(pButton, m_formatNames.size() - 1);
                            }
                        }
                    }
                }
                m_pFormatButtonGroup->button(0)->click();
                m_pFormatButtonGroup->button(0)->setFocus();
            }
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pFormatCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pFormatButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SIGNAL(completeChanged()));

    /* Register classes: */
    qRegisterMetaType<CMediumFormat>();
    /* Register fields: */
    registerField("mediumFormat", this, "mediumFormat");
}

void UIWizardNewVDPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Welcome to the New Virtual Disk wizard!"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardNewVD::tr("<p>This wizard will help you to create a new virtual disk for your virtual machine.</p>"));
    m_pLabel->setText(m_pLabel->text() + QString("<p>%1</p>").arg(standardHelpText()));
    m_pLabel->setText(m_pLabel->text() + UIWizardNewVD::tr("<p>Please choose the type of file that you would like "
                                                           "to use for the new virtual disk. If you do not need "
                                                           "to use it with other virtualization software you can "
                                                           "leave this setting unchanged.</p>"));
    m_pFormatCnt->setTitle(UIWizardNewVD::tr("File &type"));
    QList<QAbstractButton*> buttons = m_pFormatButtonGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i)
    {
        QAbstractButton *pButton = buttons[i];
        pButton->setText(VBoxGlobal::fullMediumFormatName(m_formatNames[m_pFormatButtonGroup->id(pButton)]));
    }
}

void UIWizardNewVDPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardNewVDPageBasic1::isComplete() const
{
    /* Make sure medium format is correct: */
    return !mediumFormat().isNull();
}

int UIWizardNewVDPageBasic1::nextId() const
{
    /* Show variant page only if there is something to show: */
    CMediumFormat mf = mediumFormat();
    ULONG uCapabilities = mf.GetCapabilities();
    int cTest = 0;
    if (uCapabilities & KMediumFormatCapabilities_CreateDynamic)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateFixed)
        ++cTest;
    if (uCapabilities & KMediumFormatCapabilities_CreateSplit2G)
        ++cTest;
    if (cTest > 1)
        return UIWizardNewVD::Page2;
    /* Skip otherwise: */
    return UIWizardNewVD::Page3;
}

