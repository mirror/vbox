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
#include "QIRichTextLabel.h"
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
#include "iprt/fs.h"
#include "CSystemProperties.h"


/*********************************************************************************************************************************
*   UIDiskEditorGroupBox implementation.                                                                                   *
*********************************************************************************************************************************/

UIDiskEditorGroupBox::UIDiskEditorGroupBox(bool fExpertMode, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QGroupBox>(pParent)
    , m_fExpertMode(fExpertMode)
{
    if (!m_fExpertMode)
        setFlat(true);
}

/* static */
QString UIDiskEditorGroupBox::appendExtension(const QString &strName, const QString &strExtension)
{
    /* Convert passed name to native separators: */
    QString strFileName = QDir::toNativeSeparators(strName);

    /* Remove all trailing dots to avoid multiple dots before extension: */
    int iLen;
    while (iLen = strFileName.length(), iLen > 0 && strFileName[iLen - 1] == '.')
        strFileName.truncate(iLen - 1);

    /* Add passed extension if its not done yet: */
    if (QFileInfo(strFileName).suffix().toLower() != strExtension)
        strFileName += QString(".%1").arg(strExtension);

    /* Return result: */
    return strFileName;
}

/* static */
QString UIDiskEditorGroupBox::constructMediumFilePath(const QString &strFileName, const QString &strPath)
{
    /* Wrap file-info around received file name: */
    QFileInfo fileInfo(strFileName);
    /* If path-info is relative or there is no path-info at all: */
    if (fileInfo.fileName() == strFileName || fileInfo.isRelative())
    {
        /* Resolve path on the basis of  path we have: */
        fileInfo = QFileInfo(strPath, strFileName);
    }
    /* Return full absolute hard disk file path: */
    return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
}

/* static */
QString UIDiskEditorGroupBox::defaultExtensionForMediumFormat(const CMediumFormat &mediumFormatRef)
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

/* static */
bool UIDiskEditorGroupBox::checkFATSizeLimitation(const qulonglong uVariant, const QString &strMediumPath, const qulonglong uSize)
{
    /* If the hard disk is split into 2GB parts then no need to make further checks: */
    if (uVariant & KMediumVariant_VmdkSplit2G)
        return true;
    RTFSTYPE enmType;
    int rc = RTFsQueryType(QFileInfo(strMediumPath).absolutePath().toLatin1().constData(), &enmType);
    if (RT_SUCCESS(rc))
    {
        if (enmType == RTFSTYPE_FAT)
        {
            /* Limit the medium size to 4GB. minus 128 MB for file overhead: */
            qulonglong fatLimit = _4G - _128M;
            if (uSize >= fatLimit)
                return false;
        }
    }
    return true;
}



/*********************************************************************************************************************************
*   UIDiskFormatsGroupBox implementation.                                                                                   *
*********************************************************************************************************************************/

UIDiskFormatsGroupBox::UIDiskFormatsGroupBox(bool fExpertMode, QWidget *pParent /* = 0 */)
    : UIDiskEditorGroupBox(fExpertMode, pParent)
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

const CMediumFormat &UIDiskFormatsGroupBox::VDIMediumFormat() const
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

    if (m_fExpertMode)
    {
        foreach (const QString &strId, others.keys())
            addFormatButton(pContainerLayout, others.value(strId));
    }

    /* Select VDI: */
    if (!m_pFormatButtonGroup->buttons().isEmpty())
    {
        m_pFormatButtonGroup->button(0)->click();
        m_pFormatButtonGroup->button(0)->setFocus();
    }

    connect(m_pFormatButtonGroup, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, &UIDiskFormatsGroupBox::sigMediumFormatChanged);

    retranslateUi();
}

void UIDiskFormatsGroupBox::retranslateUi()
{
    if (m_fExpertMode)
        setTitle(tr("Hard Disk File &Type"));

    QList<QAbstractButton*> buttons = m_pFormatButtonGroup ? m_pFormatButtonGroup->buttons() : QList<QAbstractButton*>();
    for (int i = 0; i < buttons.size(); ++i)
    {
        QAbstractButton *pButton = buttons[i];
        UIMediumFormat enmFormat = gpConverter->fromInternalString<UIMediumFormat>(m_formatNames[m_pFormatButtonGroup->id(pButton)]);
        pButton->setText(gpConverter->toString(enmFormat));
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
        if (fPreferred && m_fExpertMode)
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

const QStringList UIDiskFormatsGroupBox::formatExtensions() const
{
    return m_formatExtensions;
}

/* static */
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


UIDiskVariantGroupBox::UIDiskVariantGroupBox(bool fExpertMode, QWidget *pParent /* = 0 */)
    : UIDiskEditorGroupBox(fExpertMode, pParent)
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
    connect(m_pFixedCheckBox, &QCheckBox::toggled, this, &UIDiskVariantGroupBox::sltVariantChanged);
    connect(m_pSplitBox, &QCheckBox::toggled, this, &UIDiskVariantGroupBox::sltVariantChanged);
    pVariantLayout->addWidget(m_pFixedCheckBox);
    pVariantLayout->addWidget(m_pSplitBox);
    pVariantLayout->addStretch();
    retranslateUi();
}

void UIDiskVariantGroupBox::retranslateUi()
{
    if (m_fExpertMode)
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

    bool m_fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool m_fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool m_fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    if (m_pFixedCheckBox)
    {
        if (!m_fIsCreateDynamicPossible)
        {
            m_pFixedCheckBox->setChecked(true);
            m_pFixedCheckBox->setEnabled(false);
        }
        if (!m_fIsCreateFixedPossible)
        {
            m_pFixedCheckBox->setChecked(false);
            m_pFixedCheckBox->setEnabled(false);
        }
    }
    if (m_pFixedCheckBox)
        m_pFixedCheckBox->setHidden(!m_fIsCreateFixedPossible);
    if (m_pSplitBox)
        m_pSplitBox->setHidden(!m_fIsCreateSplitPossible);
}

void UIDiskVariantGroupBox::updateMediumVariantWidgetsAfterFormatChange(const CMediumFormat &mediumFormat,
                                                                        bool fHideDisabled /* = false */)
{
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mediumFormat.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    m_fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    m_fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    m_fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;

    if (m_pFixedCheckBox)
    {
        m_pFixedCheckBox->setEnabled(m_fIsCreateDynamicPossible || m_fIsCreateFixedPossible);
        if (!m_fIsCreateDynamicPossible)
            m_pFixedCheckBox->setChecked(true);
        if (!m_fIsCreateFixedPossible)
            m_pFixedCheckBox->setChecked(false);
    }
    m_pSplitBox->setEnabled(m_fIsCreateSplitPossible);

    if (fHideDisabled)
    {
        m_pFixedCheckBox->setHidden(!m_pFixedCheckBox->isEnabled());
        m_pSplitBox->setHidden(!m_pSplitBox->isEnabled());
    }
    emit sigMediumVariantChanged(mediumVariant());
}

bool UIDiskVariantGroupBox::isComplete() const
{
    /* Make sure medium variant is correct: */
    return mediumVariant() != (qulonglong)KMediumVariant_Max;
}

bool UIDiskVariantGroupBox::isCreateDynamicPossible() const
{
    return m_fIsCreateDynamicPossible;
}

bool UIDiskVariantGroupBox::isCreateFixedPossible() const
{
    return m_fIsCreateFixedPossible;
}

bool UIDiskVariantGroupBox::isCreateSplitPossible() const
{
    return m_fIsCreateSplitPossible;
}

void UIDiskVariantGroupBox::sltVariantChanged()
{
    emit sigMediumVariantChanged(mediumVariant());
}


/*********************************************************************************************************************************
*   UIMediumSizeAndPathGroupBox implementation.                                                                                  *
*********************************************************************************************************************************/

UIMediumSizeAndPathGroupBox::UIMediumSizeAndPathGroupBox(bool fExpertMode, QWidget *pParent /* = 0 */)
    : UIDiskEditorGroupBox(fExpertMode, pParent)
    , m_pLocationEditor(0)
    , m_pLocationOpenButton(0)
    , m_pMediumSizeEditor(0)
    , m_pLocationLabel(0)
    , m_pSizeLabel(0)
{
    prepare();
}

void UIMediumSizeAndPathGroupBox::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    /* Location widgets: */
    if (!m_fExpertMode)
        m_pLocationLabel = new QIRichTextLabel;
    QHBoxLayout *pLocationLayout = new QHBoxLayout;
    m_pLocationEditor = new QILineEdit;
    m_pLocationOpenButton = new QIToolButton;
    if (m_pLocationOpenButton)
    {
        m_pLocationOpenButton->setAutoRaise(true);
        m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
    }
    pLocationLayout->addWidget(m_pLocationEditor);
    pLocationLayout->addWidget(m_pLocationOpenButton);

    /* Size widgets: */
    if (!m_fExpertMode)
        m_pSizeLabel = new QIRichTextLabel;
    m_pMediumSizeEditor = new UIMediumSizeEditor;

    /* Add widgets to main layout: */
    if (m_pLocationLabel)
        pMainLayout->addWidget(m_pLocationLabel);
    pMainLayout->addLayout(pLocationLayout);

    if (m_pSizeLabel)
        pMainLayout->addWidget(m_pSizeLabel);
    pMainLayout->addWidget(m_pMediumSizeEditor);

    connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
            this, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged);

    connect(m_pLocationEditor, &QILineEdit::textChanged,
            this, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged);

    connect(m_pLocationOpenButton, &QIToolButton::clicked,
            this, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked);

    retranslateUi();
}
void UIMediumSizeAndPathGroupBox::retranslateUi()
{
    if (m_fExpertMode)
        setTitle(tr("Hard Disk File Location and Size"));
    if (m_pLocationOpenButton)
        m_pLocationOpenButton->setToolTip(tr("Choose a location for new virtual hard disk file..."));

    if (!m_fExpertMode && m_pLocationLabel)
        m_pLocationLabel->setText(tr("Please type the name of the new virtual hard disk file into the box below or "
                                                    "click on the folder icon to select a different folder to create the file in."));
    if (!m_fExpertMode && m_pSizeLabel)
        m_pSizeLabel->setText(tr("Select the size of the virtual hard disk in megabytes. "
                                                "This size is the limit on the amount of file data "
                                                "that a virtual machine will be able to store on the hard disk."));
}

QString UIMediumSizeAndPathGroupBox::mediumPath() const
{
    if (m_pLocationEditor)
        return m_pLocationEditor->text();
    return QString();
}

void UIMediumSizeAndPathGroupBox::setMediumPath(const QString &strMediumPath)
{
    if (!m_pLocationEditor)
        return;
    m_pLocationEditor->setText(strMediumPath);
}

void UIMediumSizeAndPathGroupBox::updateMediumPath(const CMediumFormat &mediumFormat, const QStringList &formatExtensions)
{
    /* Compose virtual-disk extension: */
    QString strDefaultExtension = UIDiskFormatsGroupBox::defaultExtension(mediumFormat);
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
            setMediumPath(newFileInfo.absoluteFilePath());
        }
    }
}

qulonglong UIMediumSizeAndPathGroupBox::mediumSize() const
{
    if (m_pMediumSizeEditor)
        return m_pMediumSizeEditor->mediumSize();
    return 0;
}

void UIMediumSizeAndPathGroupBox::setMediumSize(qulonglong uSize)
{
    if (m_pMediumSizeEditor)
        return m_pMediumSizeEditor->setMediumSize(uSize);
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
