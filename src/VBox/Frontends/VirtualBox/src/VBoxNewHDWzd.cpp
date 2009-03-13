/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxNewHDWzd class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "VBoxNewHDWzd.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "iprt/path.h"

/* Qt includes */
#include <QFileDialog>
#include <QToolTip>

/** Minimum VDI size in MB */
static const quint64 MinVDISize = 4;

/** Initial VDI size in MB */
static const quint64 InitialVDISize = 2 * _1K;

/**
 * Composes a file name from the given relative or full file name based on the
 * home directory and the default VDI directory. See IMedum::location for
 * details.
 */
static QString composeFullFileName (const QString &aName)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString homeFolder = vbox.GetHomeFolder();
    QString defaultFolder = vbox.GetSystemProperties().GetDefaultHardDiskFolder();

    /* Note: the logic below must match the logic of the IMedum::location
     * setter, otherwise the returned path may differ from the path actually
     * set for the hard disk by the VirtualBox API */

    QFileInfo fi (aName);
    if (fi.fileName() == aName)
    {
        /* no path info at all, use defaultFolder */
        fi = QFileInfo (defaultFolder, aName);
    }
    else if (fi.isRelative())
    {
        /* resolve relatively to homeFolder */
        fi = QFileInfo (homeFolder, aName);
    }

    return QDir::toNativeSeparators (fi.absoluteFilePath());
}

static inline int log2i (quint64 val)
{
    quint64 n = val;
    int pow = -1;
    while (n)
    {
        ++ pow;
        n >>= 1;
    }
    return pow;
}

static inline int sizeMBToSlider (quint64 val, int aSliderScale)
{
    int pow = log2i (val);
    quint64 tickMB = quint64 (1) << pow;
    quint64 tickMBNext = quint64 (1) << (pow + 1);
    int step = (val - tickMB) * aSliderScale / (tickMBNext - tickMB);
    return pow * aSliderScale + step;
}

static inline quint64 sliderToSizeMB (int val, int aSliderScale)
{
    int pow = val / aSliderScale;
    int step = val % aSliderScale;
    quint64 tickMB = quint64 (1) << pow;
    quint64 tickMBNext = quint64 (1) << (pow + 1);
    return tickMB + (tickMBNext - tickMB) * step / aSliderScale;
}


////////////////////////////////////////////////////////////////////////////////


VBoxNewHDWzd::VBoxNewHDWzd (QWidget *aParent)
    : QIWithRetranslateUI<QIAbstractWizard> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxNewHDWzd::setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Storage type page */

    /* Name and Size page */
    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    mMaxVDISize = sysProps.GetMaxVDISize();
    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly mMaxVDISize */
    mSliderScale = 0;
    {
        int pow = log2i (mMaxVDISize);
        quint64 tickMB = quint64 (1) << pow;
        if (tickMB < mMaxVDISize)
        {
            quint64 tickMBNext = quint64 (1) << (pow + 1);
            quint64 gap = tickMBNext - mMaxVDISize;
            mSliderScale = (int) ((tickMBNext - tickMB) / gap);
        }
    }
    mSliderScale = qMax (mSliderScale, 8);
    mLeName->setValidator (new QRegExpValidator (QRegExp (".+"), this));
    mLeSize->setValidator (new QRegExpValidator (QRegExp (vboxGlobal().sizeRegexp()), this));
    mLeSize->setAlignment (Qt::AlignRight);
    mWValNameAndSize = new QIWidgetValidator (mPageNameAndSize, this);
    connect (mWValNameAndSize, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValNameAndSize, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mLeSize, SIGNAL (textChanged (const QString &)),
             mWValNameAndSize, SLOT (revalidate()));
    connect (mTbNameSelect, SIGNAL (clicked()), this, SLOT (tbNameSelectClicked()));
    connect (mSlSize, SIGNAL (valueChanged (int)), this, SLOT (slSizeValueChanged (int)));
    connect (mLeSize, SIGNAL (textChanged (const QString&)),
             this, SLOT (leSizeTextChanged (const QString&)));

    /* Summary page */

    /* Image type page */

    /* Name and Size page */
    /// @todo NEWMEDIA use extension as reported by CHardDiskFormat
    static ulong HDNumber = 0;
    mLeName->setText (QString ("NewHardDisk%1.vdi").arg (++ HDNumber));
    mSlSize->setFocusPolicy (Qt::StrongFocus);
    mSlSize->setPageStep (mSliderScale);
    mSlSize->setSingleStep (mSliderScale / 8);
    mSlSize->setTickInterval (0);
    mSlSize->setMinimum (sizeMBToSlider (MinVDISize, mSliderScale));
    mSlSize->setMaximum (sizeMBToSlider (mMaxVDISize, mSliderScale));
    mTxSizeMin->setText (vboxGlobal().formatSize (MinVDISize * _1M));
    mTxSizeMax->setText (vboxGlobal().formatSize (mMaxVDISize * _1M));
    /* limit the max. size of QILineEdit */
    mLeSize->setFixedWidthByText ("88888.88 MB");
    setRecommendedSize (InitialVDISize);

    /* Summary page */
    /* Update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    mWValNameAndSize->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();

    retranslateUi();
}

void VBoxNewHDWzd::setRecommendedFileName (const QString &aName)
{
    mLeName->setText (aName);
}

void VBoxNewHDWzd::setRecommendedSize (quint64 aSize)
{
    AssertReturnVoid (aSize >= MinVDISize && aSize <= mMaxVDISize);
    mCurrentSize = aSize;
    mSlSize->setValue (sizeMBToSlider (mCurrentSize, mSliderScale));
    mLeSize->setText (vboxGlobal().formatSize (mCurrentSize * _1M));
    updateSizeToolTip (mCurrentSize * _1M);
}

void VBoxNewHDWzd::retranslateUi()
{
   /* Translate uic generated strings */
    Ui::VBoxNewHDWzd::retranslateUi (this);

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageSummary)
    {
        QString type = mRbDynamicType->isChecked() ? mRbDynamicType->text()
                                                   : mRbFixedType->text();
        type = VBoxGlobal::removeAccelMark (type);

        quint64 sizeB = mCurrentSize * _1M;

        /* compose summary */
        QString summary = QString (
            "<table>"
            "<tr><td><nobr>%1:&nbsp;</nobr></td><td><nobr>%2</nobr></td></tr>"
            "<tr><td><nobr>%3:&nbsp;</nobr></td><td><nobr>%4</nobr></td></tr>"
            "<tr><td><nobr>%5:&nbsp;</nobr></td><td><nobr>%6&nbsp;(%7&nbsp;%8)</nobr></td></tr>"
            "</table>"
        )
            .arg (tr ("Type", "summary")).arg (type)
            .arg (tr ("Location", "summary")).arg (composeFullFileName (location()))
            .arg (tr ("Size", "summary")).arg (VBoxGlobal::formatSize (sizeB))
            .arg (sizeB).arg (tr ("Bytes", "summary"));

        mTeSummary->setText (summary);
    }
}

void VBoxNewHDWzd::accept()
{
    /* Try to create the hard disk when the Finish button is pressed.
     * On failure, the wisard will remain open to give it another try. */
    if (createHardDisk())
        QIAbstractWizard::accept();
}

void VBoxNewHDWzd::slSizeValueChanged (int aVal)
{
    if (focusWidget() == mSlSize)
    {
        mCurrentSize = sliderToSizeMB (aVal, mSliderScale);
        mLeSize->setText (vboxGlobal().formatSize (mCurrentSize * _1M));
        updateSizeToolTip (mCurrentSize * _1M);
    }
}

void VBoxNewHDWzd::leSizeTextChanged (const QString &aText)
{
    if (focusWidget() == mLeSize)
    {
        mCurrentSize = vboxGlobal().parseSize (aText);
        updateSizeToolTip (mCurrentSize);
        mCurrentSize /= _1M;
        mSlSize->setValue (sizeMBToSlider (mCurrentSize, mSliderScale));
    }
}

void VBoxNewHDWzd::tbNameSelectClicked()
{
    /* set the first parent directory that exists as the current */
    QFileInfo fld (composeFullFileName (mLeName->text()));
    do
    {
        QString dp = fld.path ();
        fld = QFileInfo (dp);
    }
    while (!fld.exists() && !QDir (fld.absoluteFilePath()).isRoot());

    if (!fld.exists())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        fld = QFileInfo (vbox.GetSystemProperties().GetDefaultHardDiskFolder());
        if (!fld.exists())
            fld = vbox.GetHomeFolder();
    }

    QString selected = QFileDialog::getSaveFileName (
        this,
        tr ("Select a file for the new hard disk image file"),
        fld.absoluteFilePath(),
        tr ("Hard disk images (*.vdi)"));

    if (!selected.isEmpty())
    {
        if (QFileInfo (selected).completeSuffix().isEmpty())
            selected += ".vdi";
        mLeName->setText (QDir::toNativeSeparators (selected));
        mLeName->selectAll();
        mLeName->setFocus();
    }
}

void VBoxNewHDWzd::revalidate (QIWidgetValidator *aWval)
{
    /* do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWValNameAndSize)
        valid = mCurrentSize >= MinVDISize && mCurrentSize <= mMaxVDISize;

    aWval->setOtherValid (valid);
}

void VBoxNewHDWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxNewHDWzd::onPageShow()
{
    /* Make sure all is properly translated & composed */
    retranslateUi();

    QWidget *page = mPageStack->currentWidget();

    if (page == mPageWelcome)
        nextButton (page)->setFocus();
    else if (page == mPageType)
        mRbDynamicType->isChecked() ? mRbDynamicType->setFocus() :
                                      mRbFixedType->setFocus();
    else if (page == mPageNameAndSize)
        mLeName->setFocus();
    else if (page == mPageSummary)
        mTeSummary->setFocus();

    if (page == mPageSummary)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxNewHDWzd::showNextPage()
{
    /* Warn user about already present hard-disk */
    if (sender() == mBtnNext3 &&
        QFileInfo (location()).exists())
    {
        vboxProblem().sayCannotOverwriteHardDiskStorage (this, location());
        return;
    }

    /* Switch to the next page */
    QIAbstractWizard::showNextPage();
}


QString VBoxNewHDWzd::location()
{
    QString name = QDir::toNativeSeparators (mLeName->text());

    /* remove all trailing dots to avoid multiple dots before .vdi */
    int len;
    while (len = name.length(), len > 0 && name [len - 1] == '.')
        name.truncate (len - 1);

    QString ext = QFileInfo (name).completeSuffix();

    if (RTPathCompare (ext.toUtf8(), "vdi") != 0)
        name += ".vdi";

    return name;
}

bool VBoxNewHDWzd::isDynamicStorage()
{
    return mRbDynamicType->isChecked();
}

void VBoxNewHDWzd::updateSizeToolTip (quint64 aSizeB)
{
    QString tip = tr ("<nobr>%1 Bytes</nobr>").arg (aSizeB);
    mSlSize->setToolTip (tip);
    mLeSize->setToolTip (tip);
}

/**
 * Performs steps necessary to create a hard disk. This method handles all
 * errors and shows the corresponding messages when appropriate.
 *
 * @return Wheter the creation was successful or not.
 */
bool VBoxNewHDWzd::createHardDisk()
{
    QString loc = location();

    AssertReturn (!loc.isEmpty(), false);
    AssertReturn (mCurrentSize > 0, false);

    CVirtualBox vbox = vboxGlobal().virtualBox();

    CProgress progress;

    CHardDisk hd = vbox.CreateHardDisk(QString ("VDI"), loc);

    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    if (isDynamicStorage())
        progress = hd.CreateDynamicStorage (mCurrentSize, KHardDiskVariant_Standard);
    else
        progress = hd.CreateFixedStorage (mCurrentSize, KHardDiskVariant_Standard);

    if (!hd.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    vboxProblem().showModalProgressDialog (progress, windowTitle(), parentWidget());

    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    /* Inform everybody there is a new medium */
    vboxGlobal().addMedium (VBoxMedium (CMedium (hd),
                                        VBoxDefs::MediaType_HardDisk,
                                        KMediaState_Created));

    mHD = hd;
    return true;
}

