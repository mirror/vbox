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
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILineEdit.h"
#include "QIToolButton.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIFilePathSelector.h"
#include "UIHostnameDomainNameEditor.h"
#include "UIIconPool.h"
#include "UIMediumSizeEditor.h"
#include "UIUserNamePasswordEditor.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMDiskPageBasic.h"

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

CMediumFormat UIDiskFormatsGroupBox::mediumFormat() const
{
    return m_pFormatButtonGroup && m_pFormatButtonGroup->checkedButton() ? m_formats[m_pFormatButtonGroup->checkedId()] : CMediumFormat();
}

void UIDiskFormatsGroupBox::setMediumFormat(const CMediumFormat &mediumFormat)
{
    int iPosition = m_formats.indexOf(mediumFormat);
    if (iPosition >= 0)
    {
        m_pFormatButtonGroup->button(iPosition)->click();
        m_pFormatButtonGroup->button(iPosition)->setFocus();
    }
}

const CMediumFormat &UIDiskFormatsGroupBox::VDIMeiumFormat() const
{
    return m_comVDIMediumFormat;
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
        if (format.GetName() == "VDI")
        {
            vdi[format.GetId()] = format;
            m_comVDIMediumFormat = format;
        }
        else
        {
            const QVector<KMediumFormatCapabilities> &capabilities = format.GetCapabilities();
            if (capabilities.contains(KMediumFormatCapabilities_Preferred))
                preferred[format.GetId()] = format;
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
    setTitle(tr("Hard Disk File &Type"));

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
        m_formatExtensions << UIWizardNewVMDiskPage::defaultExtension(medFormat);
    }
}

const QStringList UIDiskFormatsGroupBox::formatExtensions() const
{
    return m_formatExtensions;
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
    setTitle(tr("Storage on Physical Hard Disk"));
    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setText(tr("Pre-allocate &Full Size"));
        m_pFixedCheckBox->setToolTip(tr("<p>When checked, the virtual disk image will be fully allocated at "
                                                       "VM creation time, rather than being allocated dynamically at VM run-time.</p>"));
    }
    m_pSplitBox->setText(tr("&Split into files of less than 2GB"));

}

qulonglong UIDiskVariantGroupBox::mediumVariant() const
{
    /* Initial value: */
    qulonglong uMediumVariant = (qulonglong)KMediumVariant_Max;

    /* Exclusive options: */
    if (m_pFixedCheckBox && m_pFixedCheckBox->isChecked())
        uMediumVariant = (qulonglong)KMediumVariant_Fixed;
    else
        uMediumVariant = (qulonglong)KMediumVariant_Standard;

    /* Additional options: */
    if (m_pSplitBox && m_pSplitBox->isChecked())
        uMediumVariant |= (qulonglong)KMediumVariant_VmdkSplit2G;

    /* Return options: */
    return uMediumVariant;
}

void UIDiskVariantGroupBox::setMediumVariant(qulonglong uMediumVariant)
{
    /* Exclusive options: */
    if (uMediumVariant & (qulonglong)KMediumVariant_Fixed)
    {
        m_pFixedCheckBox->click();
        m_pFixedCheckBox->setFocus();
    }

    /* Additional options: */
    m_pSplitBox->setChecked(uMediumVariant & (qulonglong)KMediumVariant_VmdkSplit2G);
}

void UIDiskVariantGroupBox::setWidgetVisibility(CMediumFormat &mediumFormat)
{
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    if (m_pFixedCheckBox)
    {
        if (!fIsCreateDynamicPossible)
        {
            m_pFixedCheckBox->setChecked(true);
            m_pFixedCheckBox->setEnabled(false);
        }
        if (!fIsCreateFixedPossible)
        {
            m_pFixedCheckBox->setChecked(false);
            m_pFixedCheckBox->setEnabled(false);
        }
    }
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setHidden(!fIsCreateFixedPossible);
    if (m_pSplitBox)
        m_pSplitBox->setHidden(!fIsCreateSplitPossible);
}

void UIDiskVariantGroupBox::updateMediumVariantWidgetsAfterFormatChange(const CMediumFormat &mediumFormat)
{
    /* Enable/disable widgets: */
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;

    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setEnabled(fIsCreateDynamicPossible || fIsCreateFixedPossible);
        if (!fIsCreateDynamicPossible)
            m_pFixedCheckBox->setChecked(true);
        if (!fIsCreateFixedPossible)
            m_pFixedCheckBox->setChecked(false);
    }
    m_pSplitBox->setEnabled(fIsCreateSplitPossible);
}

bool UIDiskVariantGroupBox::isComplete() const
{
    /* Make sure medium variant is correct: */
    return mediumVariant() != (qulonglong)KMediumVariant_Max;
}


/*********************************************************************************************************************************
*   UIMediumSizeAndPathGroupBox implementation.                                                                                  *
*********************************************************************************************************************************/

UIMediumSizeAndPathGroupBox::UIMediumSizeAndPathGroupBox(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QGroupBox>(pParent)
    , m_pLocationLabel(0)
    , m_pLocationEditor(0)
    , m_pLocationOpenButton(0)
    , m_pMediumSizeEditorLabel(0)
    , m_pMediumSizeEditor(0)
{
    prepare();
}

void UIMediumSizeAndPathGroupBox::prepare()
{
    QGridLayout *pDiskContainerLayout = new QGridLayout(this);

    /* Disk location widgets: */
    m_pLocationLabel = new QLabel;
    m_pLocationLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_pLocationEditor = new QILineEdit;
    m_pLocationOpenButton = new QIToolButton;
    if (m_pLocationOpenButton)
    {
        m_pLocationOpenButton->setAutoRaise(true);
        m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
    }
    m_pLocationLabel->setBuddy(m_pLocationEditor);

    /* Disk file size widgets: */
    m_pMediumSizeEditorLabel = new QLabel;
    m_pMediumSizeEditorLabel->setAlignment(Qt::AlignRight);
    m_pMediumSizeEditorLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_pMediumSizeEditor = new UIMediumSizeEditor;
    m_pMediumSizeEditorLabel->setBuddy(m_pMediumSizeEditor);


    pDiskContainerLayout->addWidget(m_pLocationLabel, 0, 0, 1, 1);
    pDiskContainerLayout->addWidget(m_pLocationEditor, 0, 1, 1, 2);
    pDiskContainerLayout->addWidget(m_pLocationOpenButton, 0, 3, 1, 1);

    pDiskContainerLayout->addWidget(m_pMediumSizeEditorLabel, 1, 0, 1, 1, Qt::AlignBottom);
    pDiskContainerLayout->addWidget(m_pMediumSizeEditor, 1, 1, 2, 3);

    retranslateUi();
}
void UIMediumSizeAndPathGroupBox::retranslateUi()
{
    setTitle(tr("Hard Disk File Location and Size"));
}

QString UIMediumSizeAndPathGroupBox::mediumPath() const
{
    if (m_pLocationEditor)
        return m_pLocationEditor->text();
    return QString();
}

void UIMediumSizeAndPathGroupBox::setMediumPath(const QString &strMediumPath)
{
    if (m_pLocationEditor)
        m_pLocationEditor->setText(strMediumPath);
}

void UIMediumSizeAndPathGroupBox::updateMediumPath(const CMediumFormat &mediumFormat, const QStringList &formatExtensions)
{
    /* Compose virtual-disk extension: */
    QString strDefaultExtension = UIWizardNewVMDiskPage::defaultExtension(mediumFormat);
    /* Update m_pLocationEditor's text if necessary: */
    if (!m_pLocationEditor->text().isEmpty() && !strDefaultExtension.isEmpty())
    {
        QFileInfo fileInfo(m_pLocationEditor->text());
        if (fileInfo.suffix() != strDefaultExtension)
        {
            QFileInfo newFileInfo(QDir(fileInfo.absolutePath()),
                                  QString("%1.%2").
                                  arg(stripFormatExtension(fileInfo.fileName(), formatExtensions)).
                                  arg(strDefaultExtension));
            m_pLocationEditor->setText(newFileInfo.absoluteFilePath());
        }
    }
}

/* static */
QString UIMediumSizeAndPathGroupBox::stripFormatExtension(const QString &strFileName, const QStringList &formatExtensions)
{
    QString result(strFileName);
    foreach (const QString &strExtension, formatExtensions)
    {
        if (strFileName.endsWith(strExtension, Qt::CaseInsensitive))
        {
            /* Add the dot to extenstion: */
            QString strExtensionWithDot(strExtension);
            strExtensionWithDot.prepend('.');
            int iIndex = strFileName.lastIndexOf(strExtensionWithDot, -1, Qt::CaseInsensitive);
            result.remove(iIndex, strExtensionWithDot.length());
        }
    }
    return result;
}
