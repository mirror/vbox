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

#include <VBoxNewHDWzd.h>
#include <VBoxGlobal.h>
#include <VBoxProblemReporter.h>

/* Qt includes */
#include <QFileDialog>
#include <QToolTip>

/** Minimum VDI size in MB */
static const Q_UINT64 MinVDISize = 4;

/** Initial VDI size in MB */
static const Q_UINT64 InitialVDISize = 2 * _1K;

/**
 *  Composes a file name from the given relative or full file name
 *  based on the home directory and the default VDI directory.
 */
static QString composeFullFileName (const QString file)
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString homeFolder = vbox.GetHomeFolder();
    QString defaultFolder = vbox.GetSystemProperties().GetDefaultVDIFolder();

    QFileInfo fi = QFileInfo (file);
    if (fi.fileName() == file)
    {
        /* no path info at all, use defaultFolder */
        fi = QFileInfo (defaultFolder, file);
    }
    else if (fi.isRelative())
    {
        /* resolve relatively to homeFolder */
        fi = QFileInfo (homeFolder, file);
    }

    return QDir::convertSeparators (fi.absoluteFilePath());
}

static inline int log2i (Q_UINT64 val)
{
    Q_UINT64 n = val;
    int pow = -1;
    while (n)
    {
        ++ pow;
        n >>= 1;
    }
    return pow;
}

static inline int sizeMBToSlider (Q_UINT64 val, int aSliderScale)
{
    int pow = log2i (val);
    Q_UINT64 tickMB = Q_UINT64 (1) << pow;
    Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
    int step = (val - tickMB) * aSliderScale / (tickMBNext - tickMB);
    return pow * aSliderScale + step;
}

static inline Q_UINT64 sliderToSizeMB (int val, int aSliderScale)
{
    int pow = val / aSliderScale;
    int step = val % aSliderScale;
    Q_UINT64 tickMB = Q_UINT64 (1) << pow;
    Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
    return tickMB + (tickMBNext - tickMB) * step / aSliderScale;
}


VBoxNewHDWzd::VBoxNewHDWzd (QWidget *aParent)
    : QIAbstractWizard (aParent)
{
    /* Apply UI decorations */
    setupUi (this);

    /* Initialize wizard hdr */
    initializeWizardHdr();

    /* Image type page */

    /* Name and Size page */
    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();
    mMaxVDISize = sysProps.GetMaxVDISize();
    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly mMaxVDISize */
    mSliderScale = 0;
    {
        int pow = log2i (mMaxVDISize);
        Q_UINT64 tickMB = Q_UINT64 (1) << pow;
        if (tickMB < mMaxVDISize)
        {
            Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
            Q_UINT64 gap = tickMBNext - mMaxVDISize;
            mSliderScale = (int) ((tickMBNext - tickMB) / gap);
        }
    }
    mSliderScale = QMAX (mSliderScale, 8);
    mLeName->setValidator (new QRegExpValidator (QRegExp( ".+" ), this));
    mLeSize->setValidator (new QRegExpValidator (QRegExp(vboxGlobal().sizeRegexp()), this));
    mLeSize->setAlignment (Qt::AlignRight);
    mWvalNameAndSize = new QIWidgetValidator (mPageNameAndSize, this);
    connect (mWvalNameAndSize, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWvalNameAndSize, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    connect (mLeSize, SIGNAL (textChanged (const QString &)),
             mWvalNameAndSize, SLOT (revalidate()));
    connect (mTbNameSelect, SIGNAL (clicked()), this, SLOT (tbNameSelectClicked()));
    connect (mSlSize, SIGNAL (valueChanged (int)), this, SLOT (slSizeValueChanged (int)));
    connect (mLeSize, SIGNAL (textChanged (const QString&)),
             this, SLOT (leSizeTextChanged (const QString&)));

    /* Summary page */


    /* Image type page */

    /* Name and Size page */
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
    mWvalNameAndSize->revalidate();

    /* Initialize wizard ftr */
    initializeWizardFtr();
}

void VBoxNewHDWzd::setRecommendedFileName (const QString &aName)
{
    mLeName->setText (aName);
}

void VBoxNewHDWzd::setRecommendedSize (Q_UINT64 aSize)
{
    AssertReturnVoid (aSize >= MinVDISize && aSize <= mMaxVDISize);
    mCurrentSize = aSize;
    mSlSize->setValue (sizeMBToSlider (mCurrentSize, mSliderScale));
    mLeSize->setText (vboxGlobal().formatSize (mCurrentSize * _1M));
    updateSizeToolTip (mCurrentSize * _1M);
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
        fld = QFileInfo (vbox.GetSystemProperties().GetDefaultVDIFolder());
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
        mLeName->setText (QDir::convertSeparators (selected));
        mLeName->selectAll();
        mLeName->setFocus();
    }
}

void VBoxNewHDWzd::revalidate (QIWidgetValidator *aWval)
{
    /* do individual validations for pages */
    bool valid = aWval->isOtherValid();

    if (aWval == mWvalNameAndSize)
        valid = mCurrentSize >= MinVDISize && mCurrentSize <= mMaxVDISize;

    aWval->setOtherValid (valid);
}

void VBoxNewHDWzd::enableNext (const QIWidgetValidator *aWval)
{
    nextButton (aWval->widget())->setEnabled (aWval->isValid());
}

void VBoxNewHDWzd::onPageShow()
{
    QWidget *page = mPageStack->currentWidget();

    if (page == mPageWelcome)
        nextButton (page)->setFocus();
    else if (page == mPageType)
        mRbDynamicType->isChecked() ? mRbDynamicType->setFocus() :
                                      mRbFixedType->setFocus();
    else if (page == mPageNameAndSize)
        mLeName->setFocus();
    else if (page == mPageSummary)
    {
        QString type = mRbDynamicType->isChecked() ? mRbDynamicType->text()
                                                   : mRbFixedType->text();
        type = VBoxGlobal::removeAccelMark (type);

        Q_UINT64 sizeB = imageSize() * _1M;

        /* compose summary */
        QString summary = QString (tr (
            "<table cellspacing=0 cellpadding=2>"
            "<tr><td><nobr>Type:</nobr></td><td><nobr>%1</nobr></td></tr>"
            "<tr><td><nobr>Location:</nobr></td><td><nobr>%2</nobr></td></tr>"
            "<tr><td><nobr>Size:</nobr></td><td><nobr>%3&nbsp;(%4&nbsp;Bytes)</nobr></td></tr>"
            "</table>"
        ))
            .arg (type)
            .arg (composeFullFileName (imageFileName()))
            .arg (VBoxGlobal::formatSize (sizeB))
            .arg (sizeB);

        mTeSummary->setText (summary);
        mTeSummary->setFocus();
    }

    if (page == mPageSummary)
        finishButton()->setDefault (true);
    else
        nextButton (page)->setDefault (true);
}

void VBoxNewHDWzd::showNextPage()
{
    /* Warn user about already present hard-disk */
    if (sender() == mBtnNext3 &&
        QFileInfo (imageFileName()).exists())
    {
        vboxProblem().sayCannotOverwriteHardDiskImage (this, imageFileName());
        return;
    }

    /* Switch to the next page */
    QIAbstractWizard::showNextPage();
}


QString VBoxNewHDWzd::imageFileName()
{
    QString name = QDir::convertSeparators (mLeName->text());

    /* remove all trailing dots to avoid multiple dots before .vdi */
    int len;
    while (len = name.length(), len > 0 && name [len - 1] == '.')
        name.truncate (len - 1);

    QString ext = QFileInfo (name).completeSuffix();
    /* compare against the proper case */
#if defined (Q_OS_FREEBSD) || defined (Q_OS_LINUX) || defined (Q_OS_NETBSD) || defined (Q_OS_OPENBSD) || defined (Q_OS_SOLARIS)
#elif defined (Q_OS_WIN) || defined (Q_OS_OS2) || defined (Q_OS_MACX)
    ext = ext.toLower();
#else
#endif

    if (ext != "vdi")
        name += ".vdi";

    return name;
}

Q_UINT64 VBoxNewHDWzd::imageSize()
{
    return mCurrentSize;
}

bool VBoxNewHDWzd::isDynamicImage()
{
    return mRbDynamicType->isChecked();
}

void VBoxNewHDWzd::updateSizeToolTip (Q_UINT64 aSizeB)
{
    QString tip = tr ("<nobr>%1 Bytes</nobr>").arg (aSizeB);
    mSlSize->setToolTip (tip);
    mLeSize->setToolTip (tip);
}

bool VBoxNewHDWzd::createHardDisk()
{
    QString src = imageFileName();
    Q_UINT64 size = imageSize();

    AssertReturn (!src.isEmpty(), false);
    AssertReturn (size > 0, false);

    CVirtualBox vbox = vboxGlobal().virtualBox();

    CProgress progress;
    CHardDisk hd = vbox.CreateHardDisk (KHardDiskStorageType_VirtualDiskImage);

    /// @todo (dmik) later, change wrappers so that converting
    //  to CUnknown is not necessary for cross-assignments
    CVirtualDiskImage vdi = CUnknown (hd);

    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateHardDiskImage (this,
                                                 vbox, src, vdi, progress);
        return false;
    }

    vdi.SetFilePath (src);

    if (isDynamicImage())
        progress = vdi.CreateDynamicImage (size);
    else
        progress = vdi.CreateFixedImage (size);

    if (!vdi.isOk())
    {
        vboxProblem().cannotCreateHardDiskImage (this,
                                                 vbox, src, vdi, progress);
        return false;
    }

    vboxProblem().showModalProgressDialog (progress, windowTitle(), parentWidget());

    if (progress.GetResultCode() != 0)
    {
        vboxProblem().cannotCreateHardDiskImage (this,
                                                 vbox, src, vdi, progress);
        return false;
    }

    vbox.RegisterHardDisk (hd);
    if (!vbox.isOk())
    {
        vboxProblem().cannotRegisterMedia (this, vbox, VBoxDefs::HD,
                                           vdi.GetFilePath());
        /* delete the image file on failure */
        vdi.DeleteImage();
        return false;
    }

    mChd = hd;
    return true;
}

