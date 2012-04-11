/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic4 class implementation
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
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>

/* Local includes: */
#include "UIWizardCloneVDPageBasic4.h"
#include "UIWizardCloneVD.h"
#include "COMDefs.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIFileDialog.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "iprt/path.h"

UIWizardCloneVDPageBasic4::UIWizardCloneVDPageBasic4()
    : m_uMediumSize(0)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pLocationCnt = new QGroupBox(this);
            m_pLocationCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pOptionsLayout = new QHBoxLayout(m_pLocationCnt);
                m_pLocationEditor = new QLineEdit(m_pLocationCnt);
                m_pLocationSelector = new QIToolButton(m_pLocationCnt);
                    m_pLocationSelector->setAutoRaise(true);
                    m_pLocationSelector->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_dis_16px.png"));
            pOptionsLayout->addWidget(m_pLocationEditor);
            pOptionsLayout->addWidget(m_pLocationSelector);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pLocationCnt);
    pMainLayout->addStretch();

    /* Setup page connections: */
    connect(m_pLocationEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltLocationEditorTextChanged(const QString &)));
    connect(m_pLocationSelector, SIGNAL(clicked()), this, SLOT(sltSelectLocationButtonClicked()));

    /* Register 'mediumPath', 'mediumSize' fields: */
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");
}

void UIWizardCloneVDPageBasic4::sltLocationEditorTextChanged(const QString &strMediumName)
{
    /* Compose new medium path: */
    m_strMediumPath = absoluteFilePath(toFileName(strMediumName, m_strDefaultExtension), m_strDefaultPath);
    /* Notify wizard sub-system about complete status changed: */
    emit completeChanged();
}

void UIWizardCloneVDPageBasic4::sltSelectLocationButtonClicked()
{
    /* Get current folder and filename: */
    QFileInfo fullFilePath(m_strMediumPath);
    QDir folder = fullFilePath.path();
    QString strFileName = fullFilePath.fileName();

    /* Set the first parent folder that exists as the current: */
    while (!folder.exists() && !folder.isRoot())
    {
        QFileInfo folderInfo(folder.absolutePath());
        if (folder == QDir(folderInfo.absolutePath()))
            break;
        folder = folderInfo.absolutePath();
    }

    /* But if it doesn't exists at all: */
    if (!folder.exists() || folder.isRoot())
    {
        /* Use recommended one folder: */
        QFileInfo defaultFilePath(absoluteFilePath(strFileName, m_strDefaultPath));
        folder = defaultFilePath.path();
    }

    /* Prepare backends list: */
    CMediumFormat mediumFormat = field("mediumFormat").value<CMediumFormat>();
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
    QStringList validExtensionList;
    for (int i = 0; i < fileExtensions.size(); ++i)
        if (deviceTypes[i] == KDeviceType_HardDisk)
            validExtensionList << QString("*.%1").arg(fileExtensions[i]);
    /* Compose full filter list: */
    QString strBackendsList = QString("%1 (%2)").arg(mediumFormat.GetName()).arg(validExtensionList.join(" "));

    /* Open corresponding file-dialog: */
    QString strChosenFilePath = QIFileDialog::getSaveFileName(folder.absoluteFilePath(strFileName),
                                                              strBackendsList, this,
                                                              UIWizardCloneVD::tr("Select a file for the new hard disk image file"));

    /* If there was something really chosen: */
    if (!strChosenFilePath.isEmpty())
    {
        /* If valid file extension is missed, append it: */
        if (QFileInfo(strChosenFilePath).suffix().isEmpty())
            strChosenFilePath += QString(".%1").arg(m_strDefaultExtension);
        m_pLocationEditor->setText(QDir::toNativeSeparators(strChosenFilePath));
        m_pLocationEditor->selectAll();
        m_pLocationEditor->setFocus();
    }
}

void UIWizardCloneVDPageBasic4::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Virtual disk file location"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardCloneVD::tr("Please type the name of the new virtual disk file into the box below or "
                                          "click on the folder icon to select a different folder to create the file in."));
    m_pLocationCnt->setTitle(UIWizardCloneVD::tr("&Location"));
}

void UIWizardCloneVDPageBasic4::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get source virtual-disk: */
    const CMedium &sourceVirtualDisk = field("sourceVirtualDisk").value<CMedium>();
    /* Get default path: */
    m_strDefaultPath = QFileInfo(sourceVirtualDisk.GetLocation()).absolutePath();
    /* Get default name: */
    QString strMediumName = UIWizardCloneVD::tr("%1_copy", "copied virtual disk name")
                                               .arg(QFileInfo(sourceVirtualDisk.GetLocation()).baseName());
    /* Get virtual-disk size: */
    m_uMediumSize = sourceVirtualDisk.GetLogicalSize();
    /* Get virtual-disk extension: */
    m_strDefaultExtension = defaultExtension(field("mediumFormat").value<CMediumFormat>());
    /* Compose path for cloned virtual-disk: */
    m_strMediumPath = absoluteFilePath(toFileName(strMediumName, m_strDefaultExtension), m_strDefaultPath);
    /* Set text to location editor: */
    m_pLocationEditor->setText(strMediumName);
}

bool UIWizardCloneVDPageBasic4::isComplete() const
{
    /* Check what current name is not empty! */
    return !m_pLocationEditor->text().trimmed().isEmpty();
}

bool UIWizardCloneVDPageBasic4::validatePage()
{
    if (QFileInfo(m_strMediumPath).exists())
    {
        msgCenter().sayCannotOverwriteHardDiskStorage(this, m_strMediumPath);
        return false;
    }
    return true;
}

/* static */
QString UIWizardCloneVDPageBasic4::toFileName(const QString &strName, const QString &strExtension)
{
    /* Convert passed name to native separators (it can be full, actually): */
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
QString UIWizardCloneVDPageBasic4::absoluteFilePath(const QString &strFileName, const QString &strDefaultPath)
{
    /* Wrap file-info around received file name: */
    QFileInfo fileInfo(strFileName);
    /* If path-info is relative or there is no path-info at all: */
    if (fileInfo.fileName() == strFileName || fileInfo.isRelative())
    {
        /* Resolve path on the basis of default path we have: */
        fileInfo = QFileInfo(strDefaultPath, strFileName);
    }
    /* Return full absolute hard disk file path: */
    return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
}

/* static */
QString UIWizardCloneVDPageBasic4::defaultExtension(const CMediumFormat &mediumFormatRef)
{
    /* Load extension / device list: */
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    CMediumFormat mediumFormat(mediumFormatRef);
    mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
    for (int i = 0; i < fileExtensions.size(); ++i)
        if (deviceTypes[i] == KDeviceType_HardDisk)
            return fileExtensions[i].toLower();
    AssertMsgFailed(("Extension can't be NULL!\n"));
    return QString();
}

