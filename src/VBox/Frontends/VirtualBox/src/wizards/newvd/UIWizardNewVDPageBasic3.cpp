/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDPageBasic3 class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDir>
# include <QRegExpValidator>
# include <QVBoxLayout>
# include <QHBoxLayout>
# include <QLineEdit>
# include <QSlider>
# include <QLabel>
# include <QSpacerItem>

/* GUI includes: */
# include "UIWizardNewVDPageBasic3.h"
# include "UIWizardNewVD.h"
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "UIIconPool.h"
# include "QIFileDialog.h"
# include "QIRichTextLabel.h"
# include "QIToolButton.h"
# include "QILineEdit.h"

/* COM includes: */
# include "CSystemProperties.h"
# include "CMediumFormat.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardNewVDPage3::UIWizardNewVDPage3(const QString &strDefaultName, const QString &strDefaultPath)
    : m_strDefaultName(strDefaultName.isEmpty() ? QString("NewVirtualDisk1") : strDefaultName)
    , m_strDefaultPath(strDefaultPath)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(vboxGlobal().virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_iSliderScale(calculateSliderScale(m_uMediumSizeMax))
{
}

void UIWizardNewVDPage3::onSelectLocationButtonClicked()
{
    /* Get current folder and filename: */
    QFileInfo fullFilePath(mediumPath());
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
    QVector<QString> fileExtensions;
    QVector<KDeviceType> deviceTypes;
    CMediumFormat mediumFormat = fieldImp("mediumFormat").value<CMediumFormat>();
    mediumFormat.DescribeFileExtensions(fileExtensions, deviceTypes);
    QStringList validExtensionList;
    for (int i = 0; i < fileExtensions.size(); ++i)
        if (deviceTypes[i] == KDeviceType_HardDisk)
            validExtensionList << QString("*.%1").arg(fileExtensions[i]);
    /* Compose full filter list: */
    QString strBackendsList = QString("%1 (%2)").arg(mediumFormat.GetName()).arg(validExtensionList.join(" "));

    /* Open corresponding file-dialog: */
    QString strChosenFilePath = QIFileDialog::getSaveFileName(folder.absoluteFilePath(strFileName),
                                                              strBackendsList, thisImp(),
                                                              VBoxGlobal::tr("Please choose a location for new virtual hard disk file"));

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

void UIWizardNewVDPage3::onSizeSliderValueChanged(int iValue)
{
    /* Get full size: */
    qulonglong uMediumSize = sliderToSizeMB(iValue, m_iSliderScale);
    /* Update tooltips: */
    updateSizeToolTips(uMediumSize);
    /* Notify size-editor about size had changed (preventing callback): */
    m_pSizeEditor->blockSignals(true);
    m_pSizeEditor->setText(vboxGlobal().formatSize(uMediumSize));
    m_pSizeEditor->blockSignals(false);
}

void UIWizardNewVDPage3::onSizeEditorTextChanged(const QString &strValue)
{
    /* Get full size: */
    qulonglong uMediumSize = vboxGlobal().parseSize(strValue);
    /* Update tooltips: */
    updateSizeToolTips(uMediumSize);
    /* Notify size-slider about size had changed (preventing callback): */
    m_pSizeSlider->blockSignals(true);
    m_pSizeSlider->setValue(sizeMBToSlider(uMediumSize, m_iSliderScale));
    m_pSizeSlider->blockSignals(false);
}

/* static */
QString UIWizardNewVDPage3::toFileName(const QString &strName, const QString &strExtension)
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
QString UIWizardNewVDPage3::absoluteFilePath(const QString &strFileName, const QString &strDefaultPath)
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
QString UIWizardNewVDPage3::defaultExtension(const CMediumFormat &mediumFormatRef)
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
int UIWizardNewVDPage3::calculateSliderScale(qulonglong uMaximumMediumSize)
{
    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly that we need: */
    int iSliderScale = 0;
    int iPower = log2i(uMaximumMediumSize);
    qulonglong uTickMB = (qulonglong)1 << iPower;
    if (uTickMB < uMaximumMediumSize)
    {
        qulonglong uTickMBNext = (qulonglong)1 << (iPower + 1);
        qulonglong uGap = uTickMBNext - uMaximumMediumSize;
        iSliderScale = (int)((uTickMBNext - uTickMB) / uGap);
#ifdef VBOX_WS_MAC
        // WORKAROUND:
        // There is an issue with Qt5 QSlider under OSX:
        // Slider tick count (maximum - minimum) is limited with some
        // "magical number" - 588351, having it more than that brings
        // unpredictable results like slider token jumping and disappearing,
        // so we are limiting tick count by lowering slider-scale 128 times.
        iSliderScale /= 128;
#endif /* VBOX_WS_MAC */
    }
    return qMax(iSliderScale, 8);
}

/* static */
int UIWizardNewVDPage3::log2i(qulonglong uValue)
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
int UIWizardNewVDPage3::sizeMBToSlider(qulonglong uValue, int iSliderScale)
{
    /* Make sure *any* slider value is multiple of 512: */
    uValue /= 512;

    /* Calculate result: */
    int iPower = log2i(uValue);
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    int iStep = (uValue - uTickMB) * iSliderScale / (uTickMBNext - uTickMB);
    int iResult = iPower * iSliderScale + iStep;

    /* Return result: */
    return iResult;
}

/* static */
qulonglong UIWizardNewVDPage3::sliderToSizeMB(int uValue, int iSliderScale)
{
    /* Calculate result: */
    int iPower = uValue / iSliderScale;
    int iStep = uValue % iSliderScale;
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    qulonglong uResult = uTickMB + (uTickMBNext - uTickMB) * iStep / iSliderScale;

    /* Make sure *any* slider value is multiple of 512: */
    uResult *= 512;

    /* Return result: */
    return uResult;
}

void UIWizardNewVDPage3::updateSizeToolTips(qulonglong uSize)
{
    QString strToolTip = UIWizardNewVD::tr("<nobr>%1 (%2 B)</nobr>").arg(vboxGlobal().formatSize(uSize)).arg(uSize);
    m_pSizeSlider->setToolTip(strToolTip);
    m_pSizeEditor->setToolTip(strToolTip);
}

QString UIWizardNewVDPage3::mediumPath() const
{
    return absoluteFilePath(toFileName(m_pLocationEditor->text(), m_strDefaultExtension), m_strDefaultPath);
}

qulonglong UIWizardNewVDPage3::mediumSize() const
{
    return sliderToSizeMB(m_pSizeSlider->value(), m_iSliderScale);
}

void UIWizardNewVDPage3::setMediumSize(qulonglong uMediumSize)
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

UIWizardNewVDPageBasic3::UIWizardNewVDPageBasic3(const QString &strDefaultName, const QString &strDefaultPath, qulonglong uDefaultSize)
    : UIWizardNewVDPage3(strDefaultName, strDefaultPath)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLocationLabel = new QIRichTextLabel(this);
        QHBoxLayout *pLocationLayout = new QHBoxLayout;
        {
            m_pLocationEditor = new QLineEdit(this);
            m_pLocationOpenButton = new QIToolButton(this);
            {
                m_pLocationOpenButton->setAutoRaise(true);
                m_pLocationOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
            }
            pLocationLayout->addWidget(m_pLocationEditor);
            pLocationLayout->addWidget(m_pLocationOpenButton);
        }
        m_pSizeLabel = new QIRichTextLabel(this);
        QGridLayout *m_pSizeLayout = new QGridLayout;
        {
            m_pSizeSlider = new QSlider(this);
            {
                m_pSizeSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                m_pSizeSlider->setOrientation(Qt::Horizontal);
                m_pSizeSlider->setTickPosition(QSlider::TicksBelow);
                m_pSizeSlider->setFocusPolicy(Qt::StrongFocus);
                m_pSizeSlider->setPageStep(m_iSliderScale);
                m_pSizeSlider->setSingleStep(m_iSliderScale / 8);
                m_pSizeSlider->setTickInterval(0);
                m_pSizeSlider->setMinimum(sizeMBToSlider(m_uMediumSizeMin, m_iSliderScale));
                m_pSizeSlider->setMaximum(sizeMBToSlider(m_uMediumSizeMax, m_iSliderScale));
            }
            m_pSizeEditor = new QILineEdit(this);
            {
                m_pSizeEditor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                m_pSizeEditor->setFixedWidthByText("88888.88 MB");
                m_pSizeEditor->setAlignment(Qt::AlignRight);
                m_pSizeEditor->setValidator(new QRegExpValidator(QRegExp(vboxGlobal().sizeRegexp()), this));
            }
            QLabel *m_pSizeMin = new QLabel(this);
            {
                m_pSizeMin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
                m_pSizeMin->setText(vboxGlobal().formatSize(m_uMediumSizeMin));
            }
            QLabel *m_pSizeMax = new QLabel(this);
            {
                m_pSizeMax->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
                m_pSizeMax->setText(vboxGlobal().formatSize(m_uMediumSizeMax));
            }
            m_pSizeLayout->addWidget(m_pSizeSlider, 0, 0, 1, 3);
            m_pSizeLayout->addWidget(m_pSizeEditor, 0, 3);
            m_pSizeLayout->addWidget(m_pSizeMin, 1, 0);
            m_pSizeLayout->setColumnStretch(1, 1);
            m_pSizeLayout->addWidget(m_pSizeMax, 1, 2);
        }
        setMediumSize(uDefaultSize);
        pMainLayout->addWidget(m_pLocationLabel);
        pMainLayout->addLayout(pLocationLayout);
        pMainLayout->addWidget(m_pSizeLabel);
        pMainLayout->addLayout(m_pSizeLayout);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pLocationEditor, SIGNAL(textChanged(const QString &)), this, SIGNAL(completeChanged()));
    connect(m_pLocationOpenButton, SIGNAL(clicked()), this, SLOT(sltSelectLocationButtonClicked()));
    connect(m_pSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(sltSizeSliderValueChanged(int)));
    connect(m_pSizeEditor, SIGNAL(textChanged(const QString &)), this, SLOT(sltSizeEditorTextChanged(const QString &)));

    /* Register fields: */
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");
}

void UIWizardNewVDPageBasic3::sltSelectLocationButtonClicked()
{
    /* Call to base-class: */
    onSelectLocationButtonClicked();
}

void UIWizardNewVDPageBasic3::sltSizeSliderValueChanged(int iValue)
{
    /* Call to base-class: */
    onSizeSliderValueChanged(iValue);

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVDPageBasic3::sltSizeEditorTextChanged(const QString &strValue)
{
    /* Call to base-class: */
    onSizeEditorTextChanged(strValue);

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardNewVDPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardNewVD::tr("File location and size"));

    /* Translate widgets: */
    m_pLocationLabel->setText(UIWizardNewVD::tr("Please type the name of the new virtual hard disk file into the box below or "
                                                "click on the folder icon to select a different folder to create the file in."));
    m_pLocationOpenButton->setToolTip(UIWizardNewVD::tr("Choose a location for new virtual hard disk file..."));
    m_pSizeLabel->setText(UIWizardNewVD::tr("Select the size of the virtual hard disk in megabytes. "
                                            "This size is the limit on the amount of file data "
                                            "that a virtual machine will be able to store on the hard disk."));
}

void UIWizardNewVDPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();

    /* Get default extension for new virtual-disk: */
    m_strDefaultExtension = defaultExtension(field("mediumFormat").value<CMediumFormat>());
    /* Set default name as text for location editor: */
    m_pLocationEditor->setText(m_strDefaultName);
}

bool UIWizardNewVDPageBasic3::isComplete() const
{
    /* Make sure current name is not empty and current size feats the bounds: */
    return !m_pLocationEditor->text().trimmed().isEmpty() &&
           mediumSize() >= m_uMediumSizeMin && mediumSize() <= m_uMediumSizeMax;
}

bool UIWizardNewVDPageBasic3::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Make sure such file doesn't exists already: */
    QString strMediumPath(mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);

    if (fResult)
    {
        /* Lock finish button: */
        startProcessing();

        /* Try to create virtual hard drive file: */
        fResult = qobject_cast<UIWizardNewVD*>(wizard())->createVirtualDisk();

        /* Unlock finish button: */
        endProcessing();
    }

    /* Return result: */
    return fResult;
}

