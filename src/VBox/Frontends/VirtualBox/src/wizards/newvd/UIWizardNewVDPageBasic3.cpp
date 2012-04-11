/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardNewVDPageBasic3 class implementation
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
#include <QRegExpValidator>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QSpacerItem>

/* Local includes: */
#include "UIWizardNewVDPageBasic3.h"
#include "UIWizardNewVD.h"
#include "COMDefs.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIFileDialog.h"
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "QILineEdit.h"
#include "iprt/path.h"

UIWizardNewVDPageBasic3::UIWizardNewVDPageBasic3(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
    : m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(vboxGlobal().virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_iSliderScale(0)
{
    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly that we need: */
    int iPower = log2i(m_uMediumSizeMax);
    qulonglong uTickMB = qulonglong (1) << iPower;
    if (uTickMB < m_uMediumSizeMax)
    {
        qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
        qulonglong uGap = uTickMBNext - m_uMediumSizeMax;
        m_iSliderScale = (int)((uTickMBNext - uTickMB) / uGap);
    }
    m_iSliderScale = qMax(m_iSliderScale, 8);

    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLocationLabel = new QIRichTextLabel(this);
        m_pLocationCnt = new QGroupBox(this);
            m_pLocationCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pOptionsLayout = new QHBoxLayout(m_pLocationCnt);
                m_pLocationEditor = new QLineEdit(m_pLocationCnt);
                m_pLocationSelector = new QIToolButton(m_pLocationCnt);
                    m_pLocationSelector->setAutoRaise(true);
                    m_pLocationSelector->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_dis_16px.png"));
            pOptionsLayout->addWidget(m_pLocationEditor);
            pOptionsLayout->addWidget(m_pLocationSelector);
        m_pSizeLabel = new QIRichTextLabel(this);
        m_pSizeCnt = new QGroupBox(this);
            m_pSizeCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QGridLayout *m_pSizeLayout = new QGridLayout(m_pSizeCnt);
                m_pSizeSlider = new QSlider(m_pSizeCnt);
                    m_pSizeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    m_pSizeSlider->setOrientation(Qt::Horizontal);
                    m_pSizeSlider->setTickPosition(QSlider::TicksBelow);
                    m_pSizeSlider->setFocusPolicy(Qt::StrongFocus);
                    m_pSizeSlider->setPageStep(m_iSliderScale);
                    m_pSizeSlider->setSingleStep(m_iSliderScale / 8);
                    m_pSizeSlider->setTickInterval(0);
                    m_pSizeSlider->setMinimum(sizeMBToSlider(m_uMediumSizeMin, m_iSliderScale));
                    m_pSizeSlider->setMaximum(sizeMBToSlider(m_uMediumSizeMax, m_iSliderScale));
                m_pSizeEditor = new QILineEdit(m_pSizeCnt);
                    m_pSizeEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                    m_pSizeEditor->setFixedWidthByText("88888.88 MB");
                    m_pSizeEditor->setAlignment(Qt::AlignRight);
                    m_pSizeEditor->setValidator(new QRegExpValidator(QRegExp(vboxGlobal().sizeRegexp()), this));
                QLabel *m_pSizeMin = new QLabel(m_pSizeCnt);
                    m_pSizeMin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
                    m_pSizeMin->setText(vboxGlobal().formatSize(m_uMediumSizeMin));
                QSpacerItem *m_pSizeSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);
                QLabel *m_pSizeMax = new QLabel(m_pSizeCnt);
                    m_pSizeMax->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
                    m_pSizeMax->setText(vboxGlobal().formatSize(m_uMediumSizeMax));
            m_pSizeLayout->addWidget(m_pSizeSlider, 0, 0, 1, 3);
            m_pSizeLayout->addWidget(m_pSizeEditor, 0, 3);
            m_pSizeLayout->addWidget(m_pSizeMin, 1, 0);
            m_pSizeLayout->addItem(m_pSizeSpacer, 1, 1);
            m_pSizeLayout->addWidget(m_pSizeMax, 1, 2);
    pMainLayout->addWidget(m_pLocationLabel);
    pMainLayout->addWidget(m_pLocationCnt);
    pMainLayout->addWidget(m_pSizeLabel);
    pMainLayout->addWidget(m_pSizeCnt);
    pMainLayout->addStretch();

    /* Setup connections: */
    connect(m_pLocationEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltLocationEditorTextChanged(const QString &)));
    connect(m_pLocationSelector, SIGNAL(clicked()), this, SLOT(sltSelectLocationButtonClicked()));
    connect(m_pSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(sltSizeSliderValueChanged(int)));
    connect(m_pSizeEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltSizeEditorTextChanged(const QString &)));

    /* Initialize connections: */
    setMediumSize(uDefaultSize);

    /* Register 'mediumPath', 'mediumSize' fields: */
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");
}

void UIWizardNewVDPageBasic3::sltLocationEditorTextChanged(const QString &strMediumName)
{
    /* Compose new medium path: */
    m_strMediumPath = absoluteFilePath(toFileName(strMediumName, m_strDefaultExtension), m_strDefaultPath);
    /* Notify wizard sub-system about complete status changed: */
    emit completeChanged();
}

void UIWizardNewVDPageBasic3::sltSelectLocationButtonClicked()
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
                                                              UIWizardNewVD::tr("Select a file for the new hard disk image file"));

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

void UIWizardNewVDPageBasic3::sltSizeSliderValueChanged(int iValue)
{
    /* Get full size: */
    qulonglong uMediumSize = sliderToSizeMB(iValue, m_iSliderScale);
    /* Update tooltips: */
    updateSizeToolTips(uMediumSize);
    /* Notify size-editor about size had changed (preventing callback): */
    m_pSizeEditor->blockSignals(true);
    m_pSizeEditor->setText(vboxGlobal().formatSize(uMediumSize));
    m_pSizeEditor->blockSignals(false);
    /* Notify wizard sub-system about complete status changed: */
    emit completeChanged();
}

void UIWizardNewVDPageBasic3::sltSizeEditorTextChanged(const QString &strValue)
{
    /* Get full size: */
    qulonglong uMediumSize = vboxGlobal().parseSize(strValue);
    /* Update tooltips: */
    updateSizeToolTips(uMediumSize);
    /* Notify size-slider about size had changed (preventing callback): */
    m_pSizeSlider->blockSignals(true);
    m_pSizeSlider->setValue(sizeMBToSlider(uMediumSize, m_iSliderScale));
    m_pSizeSlider->blockSignals(false);
    /* Notify wizard sub-system about complete status changed: */
    emit completeChanged();
}

void UIWizardNewVDPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("Virtual disk file location and size"));

    /* Translate widgets: */
    m_pLocationLabel->setText(UIWizardNewVD::tr("Please type the name of the new virtual disk file into the box below or "
                                                "click on the folder icon to select a different folder to create the file in."));
    m_pLocationCnt->setTitle(tr("&Location"));
    m_pSizeLabel->setText(UIWizardNewVD::tr("Select the size of the virtual disk in megabytes. This size will be reported "
                                            "to the Guest OS as the maximum size of this virtual disk."));
    m_pSizeCnt->setTitle(UIWizardNewVD::tr("&Size"));
}

void UIWizardNewVDPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Reset name to default: */
    m_pLocationEditor->setText(m_strDefaultName);
    /* Get virtual-disk extension: */
    m_strDefaultExtension = defaultExtension(field("mediumFormat").value<CMediumFormat>());
    /* Compose path for new virtual-disk: */
    m_strMediumPath = absoluteFilePath(toFileName(m_strDefaultName, m_strDefaultExtension), m_strDefaultPath);
}

bool UIWizardNewVDPageBasic3::isComplete() const
{
    /* Check what current name is not empty & current size feats the bounds: */
    return !m_pLocationEditor->text().trimmed().isEmpty() &&
           mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
}

bool UIWizardNewVDPageBasic3::validatePage()
{
    if (QFileInfo(m_strMediumPath).exists())
    {
        msgCenter().sayCannotOverwriteHardDiskStorage(this, m_strMediumPath);
        return false;
    }
    return true;
}

/* static */
QString UIWizardNewVDPageBasic3::toFileName(const QString &strName, const QString &strExtension)
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
QString UIWizardNewVDPageBasic3::absoluteFilePath(const QString &strFileName, const QString &strDefaultPath)
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
QString UIWizardNewVDPageBasic3::defaultExtension(const CMediumFormat &mediumFormatRef)
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

/* static */
int UIWizardNewVDPageBasic3::log2i(qulonglong uValue)
{
    int iPower = -1;
    while (uValue)
    {
        ++iPower;
        uValue >>= 1;
    }
    return iPower;
}

/* static */
int UIWizardNewVDPageBasic3::sizeMBToSlider(qulonglong uValue, int iSliderScale)
{
    int iPower = log2i(uValue);
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    int iStep = (uValue - uTickMB) * iSliderScale / (uTickMBNext - uTickMB);
    return iPower * iSliderScale + iStep;
}

/* static */
qulonglong UIWizardNewVDPageBasic3::sliderToSizeMB(int uValue, int iSliderScale)
{
    int iPower = uValue / iSliderScale;
    int iStep = uValue % iSliderScale;
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    return uTickMB + (uTickMBNext - uTickMB) * iStep / iSliderScale;
}

void UIWizardNewVDPageBasic3::updateSizeToolTips(qulonglong uSize)
{
    QString strToolTip = UIWizardNewVD::tr("<nobr>%1 (%2 B)</nobr>").arg(vboxGlobal().formatSize(uSize)).arg(uSize);
    m_pSizeSlider->setToolTip(strToolTip);
    m_pSizeEditor->setToolTip(strToolTip);
}

qulonglong UIWizardNewVDPageBasic3::mediumSize() const
{
    /* Return slider value scaled at 'm_iSliderScale': */
    return sliderToSizeMB(m_pSizeSlider->value(), m_iSliderScale);
}

void UIWizardNewVDPageBasic3::setMediumSize(qulonglong uMediumSize)
{
    /* Block signals: */
    m_pSizeSlider->blockSignals(true);
    m_pSizeEditor->blockSignals(true);
    /* Set values: */
    m_pSizeSlider->setValue(sizeMBToSlider(uMediumSize, m_iSliderScale));
    m_pSizeEditor->setText(vboxGlobal().formatSize(uMediumSize));
    updateSizeToolTips(uMediumSize);
    /* Unblock signals: */
    m_pSizeSlider->blockSignals(false);
    m_pSizeEditor->blockSignals(false);
}

