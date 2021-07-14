/* $Id$ */
/** @file
 * VBox Qt GUI - UIUserNamePasswordEditor class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIHostnameDomainNameEditor.h"
#include "UIFilePathSelector.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardDiskEditors.h"

/* Other VBox includes: */
#include "iprt/assert.h"
#include "CSystemProperties.h"

/*********************************************************************************************************************************
*   UIDiskFormatsGroupBox implementation.                                                                                   *
*********************************************************************************************************************************/

UIDiskFormatsGroupBox::UIDiskFormatsGroupBox(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pFormatButtonGroup(0)
{
    prepare();
}

void UIDiskFormatsGroupBox::prepare()
{
    QVBoxLayout *pContainerLayout = new QVBoxLayout(this);

    m_pFormatButtonGroup = new QButtonGroup;
    AssertReturnVoid(m_pFormatButtonGroup);
    /* Enumerate medium formats in special order: */
    CSystemProperties properties = uiCommon().virtualBox().GetSystemProperties();
    const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
    QMap<QString, CMediumFormat> vdi, preferred, others;
    foreach (const CMediumFormat &format, formats)
    {
        /* VDI goes first: */
        if (format.GetName() == "VDI")
            vdi[format.GetId()] = format;
        else
        {
            const QVector<KMediumFormatCapabilities> &capabilities = format.GetCapabilities();
            /* Then goes preferred: */
            if (capabilities.contains(KMediumFormatCapabilities_Preferred))
                preferred[format.GetId()] = format;
            /* Then others: */
            else
                others[format.GetId()] = format;
        }
    }

    /* Create buttons for VDI, preferred and others: */
    foreach (const QString &strId, vdi.keys())
        addFormatButton(pContainerLayout, vdi.value(strId), true);
    foreach (const QString &strId, preferred.keys())
        addFormatButton(pContainerLayout, preferred.value(strId), true);
    foreach (const QString &strId, others.keys())
        addFormatButton(pContainerLayout, others.value(strId));


    if (!m_pFormatButtonGroup->buttons().isEmpty())
    {
        m_pFormatButtonGroup->button(0)->click();
        m_pFormatButtonGroup->button(0)->setFocus();
    }
    retranslateUi();
}

void UIDiskFormatsGroupBox::retranslateUi()
{
    setTitle(UIDiskFormatsGroupBox::tr("Hard Disk File &Type"));

    if (m_pFormatButtonGroup)
    {
        QList<QAbstractButton*> buttons = m_pFormatButtonGroup->buttons();
        for (int i = 0; i < buttons.size(); ++i)
        {
            QAbstractButton *pButton = buttons[i];
            UIMediumFormat enmFormat = gpConverter->fromInternalString<UIMediumFormat>(m_formatNames[m_pFormatButtonGroup->id(pButton)]);
            pButton->setText(gpConverter->toString(enmFormat));
        }
    }
}

void UIDiskFormatsGroupBox::addFormatButton(QVBoxLayout *pFormatLayout, CMediumFormat medFormat, bool fPreferred /* = false */)
{
    /* Check that medium format supports creation: */
    ULONG uFormatCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = medFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uFormatCapabilities |= capabilities[i];

    if (!(uFormatCapabilities & KMediumFormatCapabilities_CreateFixed ||
          uFormatCapabilities & KMediumFormatCapabilities_CreateDynamic))
        return;

    /* Check that medium format supports creation of virtual hard-disks: */
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    medFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
    if (!deviceTypes.contains(KDeviceType_HardDisk))
        return;

    /* Create/add corresponding radio-button: */
    QRadioButton *pFormatButton = new QRadioButton;
    AssertPtrReturnVoid(pFormatButton);
    {
        /* Make the preferred button font bold: */
        if (fPreferred)
        {
            QFont font = pFormatButton->font();
            font.setBold(true);
            pFormatButton->setFont(font);
        }
        pFormatLayout->addWidget(pFormatButton);
        m_formats << medFormat;
        m_formatNames << medFormat.GetName();
        m_pFormatButtonGroup->addButton(pFormatButton, m_formatNames.size() - 1);
        m_formatExtensions << defaultExtension(medFormat);
    }
}

QString UIDiskFormatsGroupBox::defaultExtension(const CMediumFormat &mediumFormatRef)
{
    if (!mediumFormatRef.isNull())
    {
        /* Load extension / device list: */
        QVector<QString> fileExtensions;
        QVector<KDeviceType> deviceTypes;
        CMediumFormat mediumFormat(mediumFormatRef);
        mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
        for (int i = 0; i < fileExtensions.size(); ++i)
            if (deviceTypes[i] == KDeviceType_HardDisk)
                return fileExtensions[i].toLower();
    }
    AssertMsgFailed(("Extension can't be NULL!\n"));
    return QString();
}

/*********************************************************************************************************************************
*   UIDiskVariantGroupBox implementation.                                                                                   *
*********************************************************************************************************************************/


UIDiskVariantGroupBox::UIDiskVariantGroupBox(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pFixedCheckBox(0)
    , m_pSplitBox(0)
{
    prepare();
}

void UIDiskVariantGroupBox::prepare()
{
    QVBoxLayout *pVariantLayout = new QVBoxLayout(this);
    AssertReturnVoid(pVariantLayout);
    m_pFixedCheckBox = new QCheckBox;
    m_pSplitBox = new QCheckBox;
    pVariantLayout->addWidget(m_pFixedCheckBox);
    pVariantLayout->addWidget(m_pSplitBox);
    pVariantLayout->addStretch();
    retranslateUi();
}

void UIDiskVariantGroupBox::retranslateUi()
{
    setTitle(UIDiskVariantGroupBox::tr("Storage on Physical Hard Disk"));
    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setText(UIDiskVariantGroupBox::tr("Pre-allocate &Full Size"));
        m_pFixedCheckBox->setToolTip(UIDiskVariantGroupBox::tr("<p>When checked, the virtual disk image will be fully allocated at "
                                                       "VM creation time, rather than being allocated dynamically at VM run-time.</p>"));
    }
    m_pSplitBox->setText(UIDiskVariantGroupBox::tr("&Split into files of less than 2GB"));

}
