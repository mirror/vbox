/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UINewHDWzd class implementation
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* Global includes */
#include <QFileDialog>
#include <QRegExpValidator>

/* Local includes */
#include "UINewHDWzd.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "iprt/path.h"

UINewHDWzd::UINewHDWzd(QWidget *pParent) : QIWizard(pParent)
{
    /* Create & add pages */
    addPage(new UINewHDWzdPage1);
    addPage(new UINewHDWzdPage2);
    addPage(new UINewHDWzdPage3);
    addPage(new UINewHDWzdPage4);

    /* Translate */
    retranslateUi();

    /* Resize to 'golden ratio' */
    resizeToGoldenRatio();

#ifdef Q_WS_MAC
    /* Assign background image */
    assignBackground(":/vmw_new_harddisk_bg.png");
#else /* Q_WS_MAC */
    /* Assign watermark */
    assignWatermark(":/vmw_new_harddisk.png");
#endif /* Q_WS_MAC */
}

CMedium UINewHDWzd::hardDisk() const
{
    /* Get 'hardDisk' field value from page 4 */
    return field("hardDisk").value<CMedium>();
}

void UINewHDWzd::setRecommendedName(const QString &strName)
{
    /* Set 'initialName' field value for page 3 */
    setField("initialName", strName);
}

void UINewHDWzd::setRecommendedSize(qulonglong uSize)
{
    /* Set 'initialSize' field value for page 3 */
    setField("initialSize", uSize);
}

QString UINewHDWzd::composeFullFileName(const QString &strFileName)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strHomeFolder = vbox.GetHomeFolder();
    QString strDefaultFolder = vbox.GetSystemProperties().GetDefaultHardDiskFolder();

    QFileInfo fi(strFileName);
    if (fi.fileName() == strFileName)
    {
        /* No path info at all, use strDefaultFolder */
        fi = QFileInfo(strDefaultFolder, strFileName);
    }
    else if (fi.isRelative())
    {
        /* Resolve relatively to strHomeFolder */
        fi = QFileInfo(strHomeFolder, strFileName);
    }

    return QDir::toNativeSeparators(fi.absoluteFilePath());
}

void UINewHDWzd::retranslateUi()
{
    /* Wizard title */
    setWindowTitle(tr("Create New Virtual Disk"));
}

UINewHDWzdPage1::UINewHDWzdPage1()
{
    /* Decorate page */
    Ui::UINewHDWzdPage1::setupUi(this);
}

void UINewHDWzdPage1::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewHDWzdPage1::retranslateUi(this);

    /* Wizard page 1 title */
    setTitle(tr("Welcome to the Create New Virtual Disk Wizard!"));


    m_pPage1Text1->setText(tr("<p>This wizard will help you to create a new virtual hard disk "
                              "for your virtual machine.</p><p>%1</p>")
                           .arg(standardHelpText()));
}

void UINewHDWzdPage1::initializePage()
{
    /* Translate */
    retranslateUi();
}

UINewHDWzdPage2::UINewHDWzdPage2()
    : m_strType(QString())
    , m_bFixed(false)
{
    /* Decorate page */
    Ui::UINewHDWzdPage2::setupUi(this);

    /* Register 'type', 'fixed' fields */
    registerField("type*", this, "type");
    registerField("fixed", this, "fixed");

    /* Setup connections */
    connect (m_pTypeDynamic, SIGNAL(clicked(bool)), this, SLOT(onTypeChanged()));
    connect (m_pTypeFixed, SIGNAL(clicked(bool)), this, SLOT(onTypeChanged()));
}

void UINewHDWzdPage2::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewHDWzdPage2::retranslateUi(this);

    /* Wizard page 2 title */
    setTitle(tr("Hard Disk Storage Type"));
}

void UINewHDWzdPage2::initializePage()
{
    /* Translate */
    retranslateUi();

    /* Prepare initial choice */
    m_pTypeDynamic->click();

    /* 'Dynamic' choice should have focus initially */
    m_pTypeDynamic->setFocus();
}

void UINewHDWzdPage2::onTypeChanged()
{
    if (m_pTypeDynamic->isChecked())
    {
        /* 'Dynamic' storage type */
        m_strType = VBoxGlobal::removeAccelMark(m_pTypeDynamic->text());
        m_bFixed = false;
    }
    else if (m_pTypeFixed->isChecked())
    {
        /* 'Fixed' storage type */
        m_strType = VBoxGlobal::removeAccelMark(m_pTypeFixed->text());
        m_bFixed = true;
    }
    else
    {
        /* Not complete */
        m_strType.clear();
        m_bFixed = false;
    }
    emit completeChanged();
}

UINewHDWzdPage3::UINewHDWzdPage3()
    : m_strInitialName("NewHardDisk1.vdi")
    , m_strCurrentName(QString())
    , m_strLocation(QString())
    , m_uInitialSize(2 * _1K)
    , m_uCurrentSize(0)
    , m_uMinVDISize(4)
    , m_uMaxVDISize(vboxGlobal().virtualBox().GetSystemProperties().GetMaxVDISize())
    , m_iSliderScale(0)
{
    /* Decorate page */
    Ui::UINewHDWzdPage3::setupUi(this);

    /* Register 'initialName', 'currentName', 'location' &
     * 'initialSize', 'currentSize' fields */
    registerField("initialName", this, "initialName");
    registerField("currentName", this, "currentName");
    registerField("location", this, "location");
    registerField("initialSize", this, "initialSize");
    registerField("currentSize", this, "currentSize");

    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly m_uMaxVDISize */
    int iPower = log2i(m_uMaxVDISize);
    qulonglong uTickMB = qulonglong (1) << iPower;
    if (uTickMB < m_uMaxVDISize)
    {
        qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
        qulonglong uGap = uTickMBNext - m_uMaxVDISize;
        m_iSliderScale = (int)((uTickMBNext - uTickMB) / uGap);
    }
    m_iSliderScale = qMax(m_iSliderScale, 8);

    /* Setup size-editor field */
    m_pSizeEditor->setFixedWidthByText("88888.88 MB");
    m_pSizeEditor->setAlignment(Qt::AlignRight);
    m_pSizeEditor->setValidator(new QRegExpValidator(QRegExp(vboxGlobal().sizeRegexp()), this));

    /* Setup size-slider */
    m_pSizeSlider->setFocusPolicy(Qt::StrongFocus);
    m_pSizeSlider->setPageStep(m_iSliderScale);
    m_pSizeSlider->setSingleStep(m_iSliderScale / 8);
    m_pSizeSlider->setTickInterval(0);
    m_pSizeSlider->setMinimum(sizeMBToSlider(m_uMinVDISize, m_iSliderScale));
    m_pSizeSlider->setMaximum(sizeMBToSlider (m_uMaxVDISize, m_iSliderScale));
    m_pSizeMin->setText(vboxGlobal().formatSize(m_uMinVDISize * _1M));
    m_pSizeMax->setText(vboxGlobal().formatSize(m_uMaxVDISize * _1M));

    /* Attach button icon */
    m_pLocationSelector->setIcon(vboxGlobal().iconSet(":/select_file_16px.png", "select_file_dis_16px.png"));

    /* Setup page connections */
    connect(m_pLocationEditor, SIGNAL(textChanged(const QString &)), this, SLOT(onLocationEditorTextChanged(const QString &)));
    connect(m_pLocationSelector, SIGNAL(clicked()), this, SLOT(onSelectLocationButtonClicked()));
    connect(m_pSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(onSizeSliderValueChanged(int)));
    connect(m_pSizeEditor, SIGNAL(textChanged(const QString &)), this, SLOT(onSizeEditorTextChanged(const QString &)));
}

void UINewHDWzdPage3::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewHDWzdPage3::retranslateUi(this);

    /* Wizard page 3 title */
    setTitle(tr("Virtual Disk Location and Size"));
}

void UINewHDWzdPage3::initializePage()
{
    /* Translate */
    retranslateUi();

    /* Initialise location */
    m_pLocationEditor->setText(m_strInitialName);

    /* Initialise size */
    m_pSizeSlider->setValue(sizeMBToSlider(m_uInitialSize, m_iSliderScale));

    /* 'Size' editor should have focus initially */
    m_pSizeEditor->setFocus();
}

void UINewHDWzdPage3::cleanupPage()
{
    /* Do not call superclass method! */
}

bool UINewHDWzdPage3::isComplete() const
{
    /* Check what 'currentSize' field value feats the bounds &
     *       what 'currentName' field text is not empty! */
    return field("currentSize").toULongLong() >= m_uMinVDISize &&
           field("currentSize").toULongLong() <= m_uMaxVDISize &&
           !field("currentName").toString().trimmed().isEmpty();
}

bool UINewHDWzdPage3::validatePage()
{
    QString location = UINewHDWzd::composeFullFileName(m_strLocation);
    if (QFileInfo(location).exists())
    {
        vboxProblem().sayCannotOverwriteHardDiskStorage(this, location);
        return false;
    }
    return true;
}

void UINewHDWzdPage3::onLocationEditorTextChanged(const QString &strText)
{
    /* Set current name */
    m_strCurrentName = strText;

    /* Set current fileName */
    m_strLocation = toFileName(strText);
}

void UINewHDWzdPage3::onSelectLocationButtonClicked()
{
    /* Set the first parent directory that exists as the current */
    QFileInfo fullFilePath(UINewHDWzd::composeFullFileName(m_strLocation));
    QDir folder = fullFilePath.path();
    QString fileName = fullFilePath.fileName();

    while (!folder.exists() && !folder.isRoot())
        folder = QFileInfo(folder.absolutePath()).dir();

    if (!folder.exists() || folder.isRoot())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        folder = vbox.GetSystemProperties().GetDefaultHardDiskFolder();
        if (!folder.exists())
            folder = vbox.GetHomeFolder();
    }

    QString selected = QFileDialog::getSaveFileName(this, tr("Select a file for the new hard disk image file"),
                                                    folder.absoluteFilePath(fileName), tr("Hard disk images (*.vdi)"));

    if (!selected.isEmpty())
    {
        if (QFileInfo(selected).completeSuffix().isEmpty())
            selected += ".vdi";
        m_pSizeEditor->setText(QDir::toNativeSeparators(selected));
        m_pSizeEditor->selectAll();
        m_pSizeEditor->setFocus();
    }
}

void UINewHDWzdPage3::onSizeSliderValueChanged(int iValue)
{
    m_uCurrentSize = sliderToSizeMB(iValue, m_iSliderScale);
    m_pSizeEditor->setText(vboxGlobal().formatSize(m_uCurrentSize * _1M));
    updateSizeToolTip(m_uCurrentSize * _1M);
}

void UINewHDWzdPage3::onSizeEditorTextChanged(const QString &strValue)
{
    updateSizeToolTip(vboxGlobal().parseSize(strValue));
    m_uCurrentSize = vboxGlobal().parseSize(strValue) / _1M;
    m_pSizeSlider->setValue(sizeMBToSlider(m_uCurrentSize, m_iSliderScale));
}

QString UINewHDWzdPage3::toFileName(const QString &strName)
{
    QString fileName = QDir::toNativeSeparators(strName);

    /* Remove all trailing dots to avoid multiple dots before .vdi */
    int len;
    while (len = fileName.length(), len > 0 && fileName [len - 1] == '.')
        fileName.truncate(len - 1);

    QString ext = QFileInfo(fileName).completeSuffix();

    if (RTPathCompare(ext.toUtf8(), "vdi") != 0)
        fileName += ".vdi";

    return fileName;
}

int UINewHDWzdPage3::log2i(qulonglong uValue)
{
    int iPower = -1;
    while (uValue)
    {
        ++ iPower;
        uValue >>= 1;
    }
    return iPower;
}

int UINewHDWzdPage3::sizeMBToSlider(qulonglong uValue, int iSliderScale)
{
    int iPower = log2i(uValue);
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    int iStep = (uValue - uTickMB) * iSliderScale / (uTickMBNext - uTickMB);
    return iPower * iSliderScale + iStep;
}

qulonglong UINewHDWzdPage3::sliderToSizeMB(int uValue, int iSliderScale)
{
    int iPower = uValue / iSliderScale;
    int iStep = uValue % iSliderScale;
    qulonglong uTickMB = qulonglong (1) << iPower;
    qulonglong uTickMBNext = qulonglong (1) << (iPower + 1);
    return uTickMB + (uTickMBNext - uTickMB) * iStep / iSliderScale;
}

void UINewHDWzdPage3::updateSizeToolTip(qulonglong uSize)
{
    QString strToolTip = tr("<nobr>%1 (%2 B)</nobr>").arg(vboxGlobal().formatSize(uSize)).arg(uSize);
    m_pSizeSlider->setToolTip(strToolTip);
    m_pSizeEditor->setToolTip(strToolTip);
}

UINewHDWzdPage4::UINewHDWzdPage4()
{
    /* Decorate page */
    Ui::UINewHDWzdPage4::setupUi(this);

    /* Register CMedium class */
    qRegisterMetaType<CMedium>();

    /* Register 'hardDisk' field */
    registerField("hardDisk", this, "hardDisk");

    /* Disable the background painting of the summary widget */
    m_pSummaryText->viewport()->setAutoFillBackground (false);
    /* Make the summary field read-only */
    m_pSummaryText->setReadOnly (true);
}

void UINewHDWzdPage4::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::UINewHDWzdPage4::retranslateUi(this);

    /* Wizard page 4 title */
    setTitle(tr("Summary"));

    /* Compose common summary */
    QString summary;

    QString type = field("type").toString();
    QString location = UINewHDWzd::composeFullFileName(field("location").toString());
    QString sizeFormatted = VBoxGlobal::formatSize(field("currentSize").toULongLong() * _1M);
    QString sizeUnformatted = tr("%1 B").arg(field("currentSize").toULongLong() * _1M);

    summary += QString
    (
        "<tr><td><nobr>%1: </nobr></td><td><nobr>%2</nobr></td></tr>"
        "<tr><td><nobr>%3: </nobr></td><td><nobr>%4</nobr></td></tr>"
        "<tr><td><nobr>%5: </nobr></td><td><nobr>%6 (%7)</nobr></td></tr>"
    )
    .arg (tr("Type", "summary"), type)
    .arg (tr("Location", "summary"), location)
    .arg (tr("Size", "summary"), sizeFormatted, sizeUnformatted)
    ;
    /* Feat summary to 3 lines */
    setSummaryFieldLinesNumber(m_pSummaryText, 3);

    m_pSummaryText->setText("<table cellspacing=0 cellpadding=0>" + summary + "</table>");

    m_pPage4Text2->setText(tr("If the above settings are correct, press the <b>%1</b> button. "
                              "Once you press it, a new hard disk will be created.")
                           .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::FinishButton)))));
}

void UINewHDWzdPage4::initializePage()
{
    /* Translate */
    retranslateUi();

    /* Summary should have focus initially */
    m_pSummaryText->setFocus();
}

bool UINewHDWzdPage4::validatePage()
{
    /* Try to construct hard disk */
    return createHardDisk();
}

bool UINewHDWzdPage4::createHardDisk()
{
    KMediumVariant variant = KMediumVariant_Standard;
    QString loc = field("location").toString();
    qulonglong size = field("currentSize").toULongLong();
    bool isFixed = field("fixed").toBool();

    AssertReturn(!loc.isNull(), false);
    AssertReturn(size > 0, false);

    CVirtualBox vbox = vboxGlobal().virtualBox();

    CProgress progress;

    CMedium hardDisk = vbox.CreateHardDisk(QString("VDI"), loc);

    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage(this, vbox, loc, hardDisk, progress);
        return false;
    }

    if (isFixed)
        variant = (KMediumVariant)(KMediumVariant_Standard | KMediumVariant_Fixed);

    progress = hardDisk.CreateBaseStorage(size, variant);

    if (!hardDisk.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage(this, vbox, loc, hardDisk, progress);
        return false;
    }

    vboxProblem().showModalProgressDialog(progress, windowTitle(), parentWidget());

    if (progress.GetCanceled())
        return false;

    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        vboxProblem().cannotCreateHardDiskStorage(this, vbox, loc, hardDisk, progress);
        return false;
    }

    /* Inform everybody there is a new medium */
    vboxGlobal().addMedium(VBoxMedium(CMedium(hardDisk), VBoxDefs::MediumType_HardDisk, KMediumState_Created));

    m_HardDisk = hardDisk;
    return true;
}
