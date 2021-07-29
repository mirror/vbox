/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageSizeLocation class implementation.
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
#include <QDir>
#include <QRegExpValidator>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QSpacerItem>

/* GUI includes: */
#include "UIWizardNewVDPageSizeLocation.h"
#include "UIWizardNewVD.h"
#include "UICommon.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIFileDialog.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "QILineEdit.h"
#include "UIMediumSizeEditor.h"
#include "UIWizardDiskEditors.h"

/* COM includes: */
#include "CSystemProperties.h"
#include "CMediumFormat.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>
#include <iprt/path.h>


// UIWizardNewVDPageBaseSizeLocation::UIWizardNewVDPageBaseSizeLocation(const QString &strDefaultName, const QString &strDefaultPath)
//     : m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
//     , m_strDefaultPath(strDefaultPath)
//     , m_uMediumSizeMin(_4M)
//     , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
//     , m_pLocationEditor(0)
//     , m_pLocationOpenButton(0)
//     , m_pMediumSizeEditor(0)
//     , m_pMediumSizeEditorLabel(0)
//     , m_pLocationLabel(0)
//     , m_pSizeLabel(0)
// {
// }

// UIWizardNewVDPageBaseSizeLocation::UIWizardNewVDPageBaseSizeLocation()
//     : m_uMediumSizeMin(_4M)
//     , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
//     , m_pLocationEditor(0)
//     , m_pLocationOpenButton(0)
//     , m_pMediumSizeEditor(0)
//     , m_pMediumSizeEditorLabel(0)
//     , m_pLocationLabel(0)
//     , m_pSizeLabel(0)
// {
// }

// void UIWizardNewVDPageBaseSizeLocation::onSelectLocationButtonClicked()
// {
//     /* Get current folder and filename: */
//     QFileInfo fullFilePath(mediumPath());
//     QDir folder = fullFilePath.path();
//     QString strFileName = fullFilePath.fileName();

//     /* Set the first parent folder that exists as the current: */
//     while (!folder.exists() && !folder.isRoot())
//     {
//         QFileInfo folderInfo(folder.absolutePath());
//         if (folder == QDir(folderInfo.absolutePath()))
//             break;
//         folder = folderInfo.absolutePath();
//     }

//     /* But if it doesn't exists at all: */
//     if (!folder.exists() || folder.isRoot())
//     {
//         /* Use recommended one folder: */
//         QFileInfo defaultFilePath(absoluteFilePath(strFileName, m_strDefaultPath));
//         folder = defaultFilePath.path();
//     }

//     /* Prepare backends list: */
//     QVector<QString> fileExtensions;
//     QVector<KDeviceType> deviceTypes;
//     CMediumFormat mediumFormat = fieldImp("mediumFormat").value<CMediumFormat>();
//     mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
//     QStringList validExtensionList;
//     for (int i = 0; i < fileExtensions.size(); ++i)
//         if (deviceTypes[i] == KDeviceType_HardDisk)
//             validExtensionList << QString("*.%1").arg(fileExtensions[i]);
//     /* Compose full filter list: */
//     QString strBackendsList = QString("%1 (%2)").arg(mediumFormat.GetName()).arg(validExtensionList.join(" "));

//     /* Open corresponding file-dialog: */
//     QString strChosenFilePath = QIFileDialog::getSaveFileName(folder.absoluteFilePath(strFileName),
//                                                               strBackendsList, thisImp(),
//                                                               UICommon::tr("Please choose a location for new virtual hard disk file"));

//     /* If there was something really chosen: */
//     if (!strChosenFilePath.isEmpty())
//     {
//         /* If valid file extension is missed, append it: */
//         if (QFileInfo(strChosenFilePath).suffix().isEmpty())
//             strChosenFilePath += QString(".%1").arg(m_strDefaultExtension);
//         if (m_pLocationEditor)
//         {
//             m_pLocationEditor->setText(QDir::toNativeSeparators(strChosenFilePath));
//             m_pLocationEditor->selectAll();
//             m_pLocationEditor->setFocus();
//         }
//     }
// }

// /* static */
// QString UIWizardNewVDPageBaseSizeLocation::toFileName(const QString &strName, const QString &strExtension)
// {
//     /* Convert passed name to native separators (it can be full, actually): */
//     QString strFileName = QDir::toNativeSeparators(strName);

//     /* Remove all trailing dots to avoid multiple dots before extension: */
//     int iLen;
//     while (iLen = strFileName.length(), iLen > 0 && strFileName[iLen - 1] == '.')
//         strFileName.truncate(iLen - 1);

//     /* Add passed extension if its not done yet: */
//     if (QFileInfo(strFileName).suffix().toLower() != strExtension)
//         strFileName += QString(".%1").arg(strExtension);

//     /* Return result: */
//     return strFileName;
// }

// /* static */
// QString UIWizardNewVDPageBaseSizeLocation::absoluteFilePath(const QString &strFileName, const QString &strPath)
// {
//     /* Wrap file-info around received file name: */
//     QFileInfo fileInfo(strFileName);
//     /* If path-info is relative or there is no path-info at all: */
//     if (fileInfo.fileName() == strFileName || fileInfo.isRelative())
//     {
//         /* Resolve path on the basis of  path we have: */
//         fileInfo = QFileInfo(strPath, strFileName);
//     }
//     /* Return full absolute hard disk file path: */
//     return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
// }

// /*static */
// QString UIWizardNewVDPageBaseSizeLocation::absoluteFilePath(const QString &strFileName, const QString &strPath, const QString &strExtension)
// {
//     QString strFilePath = absoluteFilePath(strFileName, strPath);
//     if (QFileInfo(strFilePath).suffix().isEmpty())
//         strFilePath += QString(".%1").arg(strExtension);
//     return strFilePath;
// }

// /* static */
// QString UIWizardNewVDPageBaseSizeLocation::defaultExtension(const CMediumFormat &mediumFormatRef)
// {
//     if (!mediumFormatRef.isNull())
//     {
//         /* Load extension / device list: */
//         QVector<QString> fileExtensions;
//         QVector<KDeviceType> deviceTypes;
//         CMediumFormat mediumFormat(mediumFormatRef);
//         mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
//         for (int i = 0; i < fileExtensions.size(); ++i)
//             if (deviceTypes[i] == KDeviceType_HardDisk)
//                 return fileExtensions[i].toLower();
//     }
//     AssertMsgFailed(("Extension can't be NULL!\n"));
//     return QString();
// }

// /* static */
// bool UIWizardNewVDPageBaseSizeLocation::checkFATSizeLimitation(const qulonglong uVariant, const QString &strMediumPath, const qulonglong uSize)
// {
//     /* If the hard disk is split into 2GB parts then no need to make further checks: */
//     if (uVariant & KMediumVariant_VmdkSplit2G)
//         return true;

//     RTFSTYPE enmType;
//     int rc = RTFsQueryType(QFileInfo(strMediumPath).absolutePath().toLatin1().constData(), &enmType);
//     if (RT_SUCCESS(rc))
//     {
//         if (enmType == RTFSTYPE_FAT)
//         {
//             /* Limit the medium size to 4GB. minus 128 MB for file overhead: */
//             qulonglong fatLimit = _4G - _128M;
//             if (uSize >= fatLimit)
//                 return false;
//         }
//     }

//     return true;
// }

// QString UIWizardNewVDPageBaseSizeLocation::mediumPath() const
// {
//     if (!m_pLocationEditor)
//         return QString();
//     return absoluteFilePath(toFileName(m_pLocationEditor->text(), m_strDefaultExtension), m_strDefaultPath);
// }

// qulonglong UIWizardNewVDPageBaseSizeLocation::mediumSize() const
// {
//     return m_pMediumSizeEditor ? m_pMediumSizeEditor->mediumSize() : 0;
// }

// void UIWizardNewVDPageBaseSizeLocation::setMediumSize(qulonglong uMediumSize)
// {
//     if (m_pMediumSizeEditor)
//         m_pMediumSizeEditor->setMediumSize(uMediumSize);
// }

// /* static */
// QString UIWizardNewVDPageBaseSizeLocation::stripFormatExtension(const QString &strFileName, const QStringList &formatExtensions)
// {
//     QString result(strFileName);
//     foreach (const QString &strExtension, formatExtensions)
//     {
//         if (strFileName.endsWith(strExtension, Qt::CaseInsensitive))
//         {
//             /* Add the dot to extenstion: */
//             QString strExtensionWithDot(strExtension);
//             strExtensionWithDot.prepend('.');
//             int iIndex = strFileName.lastIndexOf(strExtensionWithDot, -1, Qt::CaseInsensitive);
//             result.remove(iIndex, strExtensionWithDot.length());
//         }
//     }
//     return result;
// }

// void UIWizardNewVDPageBaseSizeLocation::updateLocationEditorAfterFormatChange(const CMediumFormat &mediumFormat, const QStringList &formatExtensions)
// {
//     /* Compose virtual-disk extension: */
//     m_strDefaultExtension = defaultExtension(mediumFormat);
//     /* Update m_pLocationEditor's text if necessary: */
//     if (!m_pLocationEditor->text().isEmpty() && !m_strDefaultExtension.isEmpty())
//     {
//         QFileInfo fileInfo(m_pLocationEditor->text());
//         if (fileInfo.suffix() != m_strDefaultExtension)
//         {
//             QFileInfo newFileInfo(fileInfo.absolutePath(),
//                                   QString("%1.%2").
//                                   arg(stripFormatExtension(fileInfo.fileName(), formatExtensions)).
//                                   arg(m_strDefaultExtension));
//             m_pLocationEditor->setText(newFileInfo.absoluteFilePath());
//         }
//     }
// }

// void UIWizardNewVDPageBaseSizeLocation::retranslateWidgets()
// {
//     if (m_pLocationOpenButton)
//         m_pLocationOpenButton->setToolTip(UIWizardNewVD::tr("Choose a location for new virtual hard disk file..."));


//     if (m_pMediumSizeEditorLabel)
//         m_pMediumSizeEditorLabel->setText(UIWizardNewVD::tr("D&isk Size:"));
// }

UIWizardNewVDPageSizeLocation::UIWizardNewVDPageSizeLocation(const QString &strDefaultName,
                                                             const QString &strDefaultPath, qulonglong uDefaultSize)
    : m_pMediumSizePathGroup(0)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(uiCommon().virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uDefaultSize(uDefaultSize)
{



//     /* Setup connections: */
//     connect(m_pLocationEditor, &QLineEdit::textChanged,    this, &UIWizardNewVDPageSizeLocation::completeChanged);
//     connect(m_pLocationOpenButton, &QIToolButton::clicked, this, &UIWizardNewVDPageSizeLocation::sltSelectLocationButtonClicked);
//     connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged, this, &UIWizardNewVDPageSizeLocation::completeChanged);

//     /* Register fields: */
//     registerField("mediumPath", this, "mediumPath");
//     registerField("mediumSize", this, "mediumSize");
    prepare();
}

void UIWizardNewVDPageSizeLocation::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    AssertReturnVoid(pMainLayout);
    m_pMediumSizePathGroup = new UIMediumSizeAndPathGroupBox(false /* fExpertMode */, 0);
//     {
//         m_pLocationLabel = new QIRichTextLabel(this);
//         QHBoxLayout *pLocationLayout = new QHBoxLayout;
//         {
//             m_pLocationEditor = new QLineEdit(this);
//             m_pLocationOpenButton = new QIToolButton(this);
//             {
//                 m_pLocationOpenButton->setAutoRaise(true);
//                 m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
//             }
//             pLocationLayout->addWidget(m_pLocationEditor);
//             pLocationLayout->addWidget(m_pLocationOpenButton);
//         }
//         m_pSizeLabel = new QIRichTextLabel(this);
//         m_pMediumSizeEditor = new UIMediumSizeEditor;
//         setMediumSize(uDefaultSize);
//         pMainLayout->addWidget(m_pLocationLabel);
//         pMainLayout->addLayout(pLocationLayout);
//         pMainLayout->addWidget(m_pSizeLabel);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumSizeChanged,
            this, &UIWizardNewVDPageSizeLocation::sltMediumSizeChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumPathChanged,
            this, &UIWizardNewVDPageSizeLocation::sltMediumPathChanged);
    connect(m_pMediumSizePathGroup, &UIMediumSizeAndPathGroupBox::sigMediumLocationButtonClicked,
            this, &UIWizardNewVDPageSizeLocation::sltSelectLocationButtonClicked);
    pMainLayout->addWidget(m_pMediumSizePathGroup);
    pMainLayout->addStretch();
//     }
    retranslateUi();
}

void UIWizardNewVDPageSizeLocation::sltSelectLocationButtonClicked()
{
    //m_userModifiedParameters
    /* Call to base-class: */
    //onSelectLocationButtonClicked();
}

void UIWizardNewVDPageSizeLocation::sltMediumSizeChanged(qulonglong /*uSize*/)
{
    AssertReturnVoid(m_pMediumSizePathGroup);
    m_userModifiedParameters << "MediumSize";
    newVDWizardPropertySet(MediumSize, m_pMediumSizePathGroup->mediumSize());

}

void UIWizardNewVDPageSizeLocation::sltMediumPathChanged(const QString &/*strPath*/)
{
    AssertReturnVoid(m_pMediumSizePathGroup);
    m_userModifiedParameters << "MediumPath";
    newVDWizardPropertySet(MediumPath, m_pMediumSizePathGroup->mediumPath());
}

void UIWizardNewVDPageSizeLocation::retranslateUi()
{
    setTitle(UIWizardNewVD::tr("File location and size"));
}

void UIWizardNewVDPageSizeLocation::initializePage()
{
    // /* Translate page: */


    // /* Get default extension for new virtual-disk: */
    // m_strDefaultExtension = defaultExtension(field("mediumFormat").value<CMediumFormat>());
    // /* Set default name as text for location editor: */
    // if (m_pLocationEditor)
    //     m_pLocationEditor->setText(absoluteFilePath(m_strDefaultName, m_strDefaultPath, m_strDefaultExtension));

    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturnVoid(pWizard && m_pMediumSizePathGroup);

    if (!m_userModifiedParameters.contains("MediumPath"))
    {
        const CMediumFormat comMediumFormat = pWizard->mediumFormat();
        AssertReturnVoid(!comMediumFormat.isNull());
        QString strExtension = UIDiskEditorGroupBox::defaultExtensionForMediumFormat(comMediumFormat);
        QString strMediumFilePath =
            UIDiskEditorGroupBox::constructMediumFilePath(UIDiskVariantGroupBox::appendExtension(m_strDefaultName,
                                                                                                    strExtension), m_strDefaultPath);
        m_pMediumSizePathGroup->blockSignals(true);
        m_pMediumSizePathGroup->setMediumPath(strMediumFilePath);
        m_pMediumSizePathGroup->blockSignals(false);
        newVDWizardPropertySet(MediumPath, m_pMediumSizePathGroup->mediumPath());
    }
    if (!m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizePathGroup->blockSignals(true);
        m_pMediumSizePathGroup->setMediumSize(m_uDefaultSize > m_uMediumSizeMin && m_uDefaultSize < m_uMediumSizeMax ? m_uDefaultSize : m_uMediumSizeMin);
        m_pMediumSizePathGroup->blockSignals(false);
        newVDWizardPropertySet(MediumSize, m_pMediumSizePathGroup->mediumSize());
    }
    retranslateUi();
}

bool UIWizardNewVDPageSizeLocation::isComplete() const
{
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturn(pWizard, false);
    if (pWizard->mediumPath().isEmpty())
        return false;
    if (pWizard->mediumSize() > m_uMediumSizeMax || pWizard->mediumSize() < m_uMediumSizeMin)
        return false;
    return true;
}

bool UIWizardNewVDPageSizeLocation::validatePage()
{
    bool fResult = true;
    UIWizardNewVD *pWizard = qobject_cast<UIWizardNewVD*>(wizard());
    AssertReturn(pWizard, false);
    /* Make sure such file doesn't exist already: */
    const QString strMediumPath(pWizard->mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
    {
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);
        return fResult;
    }

    /* Make sure we are passing FAT size limitation: */
    fResult = UIDiskEditorGroupBox::checkFATSizeLimitation(pWizard->mediumVariant(),
                                     pWizard->mediumPath(),
                                     pWizard->mediumSize());
    if (!fResult)
    {
        msgCenter().cannotCreateHardDiskStorageInFAT(strMediumPath, this);
        return fResult;
    }

    // /* Lock finish button: */
    // startProcessing();
    /* Try to create virtual-disk: */
    fResult = pWizard->createVirtualDisk();
    // /* Unlock finish button: */
    // endProcessing();

    /* Return result: */
    return fResult;
}
