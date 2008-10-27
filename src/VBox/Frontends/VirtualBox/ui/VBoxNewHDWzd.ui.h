/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "New hard disk" wizard UI include (Qt Designer)
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

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

/** Minimum VDI size in MB */
static const Q_UINT64 MinVDISize = 4;

/** Initial VDI size in MB */
static const Q_UINT64 InitialVDISize = 2 * _1K;

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

    return QDir::convertSeparators (fi.absFilePath());
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

static inline int sizeMBToSlider (Q_UINT64 val, int sliderScale)
{
    int pow = log2i (val);
    Q_UINT64 tickMB = Q_UINT64 (1) << pow;
    Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
    int step = (val - tickMB) * sliderScale / (tickMBNext - tickMB);
    return pow * sliderScale + step;
}

static inline Q_UINT64 sliderToSizeMB (int val, int sliderScale)
{
    int pow = val / sliderScale;
    int step = val % sliderScale;
    Q_UINT64 tickMB = Q_UINT64 (1) << pow;
    Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
    return tickMB + (tickMBNext - tickMB) * step / sliderScale;
}


////////////////////////////////////////////////////////////////////////////////


void VBoxNewHDWzd::init()
{
    /* disable help buttons */
    helpButton()->setShown (false);

    /* fix tab order to get the proper direction
     * (originally the focus goes Next/Finish -> Back -> Cancel -> page) */
    QWidget::setTabOrder (backButton(), nextButton());
    QWidget::setTabOrder (nextButton(), finishButton());
    QWidget::setTabOrder (finishButton(), cancelButton());

    /* setup connections and set validation for pages
     * ---------------------------------------------------------------------- */

    /* setup the label clolors for nice scaling */
    VBoxGlobal::adoptLabelPixmap (pmWelcome);
    VBoxGlobal::adoptLabelPixmap (pmType);
    VBoxGlobal::adoptLabelPixmap (pmNameAndSize);
    VBoxGlobal::adoptLabelPixmap (pmSummary);

    /* Storage type page */

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
            /// @todo (r=dmik) overflow may happen if mMaxVDISize is TOO big
            mSliderScale = (int) ((tickMBNext - tickMB) / gap);
        }
    }
    mSliderScale = QMAX (mSliderScale, 8);

    leName->setValidator (new QRegExpValidator (QRegExp( ".+" ), this));

    leSize->setValidator (new QRegExpValidator (vboxGlobal().sizeRegexp(), this));
    leSize->setAlignment (Qt::AlignRight);

    mWValNameAndSize = new QIWidgetValidator (pageNameAndSize, this);
    connect (mWValNameAndSize, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (mWValNameAndSize, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    /* we ask revalidate only when mCurrentSize is changed after manually
     * editing the line edit field; the slider cannot produce invalid values */
    connect (leSize, SIGNAL (textChanged (const QString &)),
             mWValNameAndSize, SLOT (revalidate()));

    /* Summary page */

    mSummaryText = new QITextEdit (pageSummary);
    mSummaryText->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);
    mSummaryText->setFrameShape (QTextEdit::NoFrame);
    mSummaryText->setReadOnly (TRUE);
    summaryLayout->insertWidget (1, mSummaryText);

    /* filter out Enter keys in order to direct them to the default dlg button */
    QIKeyFilter *ef = new QIKeyFilter (this, Key_Enter);
    ef->watchOn (mSummaryText);

    /* set initial values
     * ---------------------------------------------------------------------- */

    /* Storage type page */

    /* Name and Size page */

    /// @todo NEWMEDIA use extension as reported by CHardDiskFormat

    static ulong HDNumber = 0;
    leName->setText (QString ("NewHardDisk%1.vdi").arg (++ HDNumber));

    slSize->setFocusPolicy (QWidget::StrongFocus);
    slSize->setPageStep (mSliderScale);
    slSize->setLineStep (mSliderScale / 8);
    slSize->setTickInterval (0);
    slSize->setMinValue (sizeMBToSlider (MinVDISize, mSliderScale));
    slSize->setMaxValue (sizeMBToSlider (mMaxVDISize, mSliderScale));

    txSizeMin->setText (vboxGlobal().formatSize (MinVDISize * _1M));
    txSizeMax->setText (vboxGlobal().formatSize (mMaxVDISize * _1M));

    /* limit the max. size of QLineEdit (STUPID Qt has NO correct means for that) */
    leSize->setMaximumSize (
        leSize->fontMetrics().width ("88888.88 MB") + leSize->frameWidth() * 2,
        leSize->height());

    setRecommendedSize (InitialVDISize);

    /* Summary page */

    mSummaryText->setPaper (pageSummary->backgroundBrush());

    /* update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    mWValNameAndSize->revalidate();

    /* the finish button on the Summary page is always enabled */
    setFinishEnabled (pageSummary, true);

    /* setup minimum width for the sizeHint to be calculated correctly */
    int wid = widthSpacer->minimumSize().width();
    txWelcome->setMinimumWidth (wid);
    textLabel1_2->setMinimumWidth (wid);
    txNameComment->setMinimumWidth (wid);
    txSizeComment->setMinimumWidth (wid);
    txSummaryHdr->setMinimumWidth (wid);
    txSummaryFtr->setMinimumWidth (wid);
}


void VBoxNewHDWzd::showEvent (QShowEvent *e)
{
    QDialog::showEvent (e);

    /* one may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */

    layout()->activate();

    /* resize to the miminum possible size */
    resize (minimumSize());

    VBoxGlobal::centerWidget (this, parentWidget());
}


void VBoxNewHDWzd::setRecommendedFileName (const QString &aName)
{
    leName->setText (aName);
}


void VBoxNewHDWzd::setRecommendedSize (Q_UINT64 aSize)
{
    AssertReturnVoid (aSize >= MinVDISize && aSize <= mMaxVDISize);
    mCurrentSize = aSize;
    slSize->setValue (sizeMBToSlider (mCurrentSize, mSliderScale));
    leSize->setText (vboxGlobal().formatSize (mCurrentSize * _1M));
    updateSizeToolTip (mCurrentSize * _1M);
}


QString VBoxNewHDWzd::location()
{
    QString name = QDir::convertSeparators (leName->text());

    /* remove all trailing dots to avoid multiple dots before .vdi */
    int len;
    while (len = name.length(), len > 0 && name [len - 1] == '.')
        name.truncate (len - 1);

    QString ext = QFileInfo (name).extension();

    if (RTPathCompare (ext.utf8(), "vdi") != 0)
        name += ".vdi";

    return name;
}


bool VBoxNewHDWzd::isDynamicStorage()
{
    return rbDynamicType->isOn();
}


void VBoxNewHDWzd::enableNext (const QIWidgetValidator *wval)
{
    setNextEnabled (wval->widget(), wval->isValid());
}


void VBoxNewHDWzd::revalidate (QIWidgetValidator *wval)
{
    /* do individual validations for pages */

    QWidget *pg = wval->widget();
    bool valid = wval->isOtherValid();

    if (pg == pageNameAndSize)
    {
        valid = mCurrentSize >= MinVDISize && mCurrentSize <= mMaxVDISize;
    }

    wval->setOtherValid (valid);
}


void VBoxNewHDWzd::updateSizeToolTip (Q_UINT64 sizeB)
{
    QString tip = tr ("<nobr>%1 Bytes</nobr>").arg (sizeB);
    QToolTip::add (slSize, tip);
    QToolTip::add (leSize, tip);
}

void VBoxNewHDWzd::showPage( QWidget *page )
{
    if (currentPage() == pageNameAndSize)
    {
        if (QFileInfo (location()).exists())
        {
            vboxProblem().sayCannotOverwriteHardDiskStorage (this, location());
            return;
        }
    }

    if (page == pageSummary)
    {
        QString type = rbDynamicType->isOn() ? rbDynamicType->text()
                                             : rbFixedType->text();
        type = VBoxGlobal::removeAccelMark (type);

        Q_UINT64 sizeB = mCurrentSize * _1M;

        // compose summary
        QString summary = QString (tr(
            "<table>"
            "<tr><td>Type:</td><td>%1</td></tr>"
            "<tr><td>Location:</td><td>%2</td></tr>"
            "<tr><td>Size:</td><td>%3&nbsp;(%4&nbsp;Bytes)</td></tr>"
            "</table>"
        ))
            .arg (type)
            .arg (composeFullFileName (location()))
            .arg (VBoxGlobal::formatSize (sizeB))
            .arg (sizeB);
        mSummaryText->setText (summary);
        /* set Finish to default */
        finishButton()->setDefault (true);
    }
    else
    {
        /* always set Next to default */
        nextButton()->setDefault (true);
    }

    QWizard::showPage (page);

    /* fix focus on the last page. when we go to the last page
     * having the Next in focus the focus goes to the Cancel
     * button because when the Next hides Finish is not yet shown. */
    if (page == pageSummary && focusWidget() == cancelButton())
        finishButton()->setFocus();

    /* setup focus for individual pages */
    if (page == pageType)
    {
        bgType->setFocus();
    }
    else if (page == pageNameAndSize)
    {
        leName->setFocus();
    }
    else if (page == pageSummary)
    {
        mSummaryText->setFocus();
    }

    page->layout()->activate();
}


void VBoxNewHDWzd::accept()
{
    /*
     *  Try to create the hard disk when the Finish button is pressed.
     *  On failure, the wisard will remain open to give it another try.
     */
    if (createHardDisk())
        QWizard::accept();
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

    CHardDisk2 hd = vbox.CreateHardDisk2 (QString ("VDI"), loc);

    if (!vbox.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    if (isDynamicStorage())
        progress = hd.CreateDynamicStorage (mCurrentSize);
    else
        progress = hd.CreateFixedStorage (mCurrentSize);

    if (!hd.isOk())
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    vboxProblem().showModalProgressDialog (progress, caption(), parentWidget());

    if (!progress.isOk() || progress.GetResultCode() != 0)
    {
        vboxProblem().cannotCreateHardDiskStorage (this, vbox, loc, hd,
                                                   progress);
        return false;
    }

    /* inform everybody there is a new medium */
    vboxGlobal().addMedium (VBoxMedium (CMedium (hd),
                                        VBoxDefs::MediaType_HardDisk,
                                        KMediaState_Created));

    mHD = hd;
    return true;
}


void VBoxNewHDWzd::slSize_valueChanged( int val )
{
    if (focusWidget() == slSize)
    {
        mCurrentSize = sliderToSizeMB (val, mSliderScale);
        leSize->setText (vboxGlobal().formatSize (mCurrentSize * _1M));
        updateSizeToolTip (mCurrentSize * _1M);
    }
}


void VBoxNewHDWzd::leSize_textChanged( const QString &text )
{
    if (focusWidget() == leSize)
    {
        mCurrentSize = vboxGlobal().parseSize (text);
        updateSizeToolTip (mCurrentSize);
        mCurrentSize /= _1M;
        slSize->setValue (sizeMBToSlider (mCurrentSize, mSliderScale));
    }
}


void VBoxNewHDWzd::tbNameSelect_clicked()
{
    /* set the first parent directory that exists as the current */
    QFileInfo fld (composeFullFileName (leName->text()));
    do
    {
        QString dp = fld.dirPath (false);
        fld = QFileInfo (dp);
    }
    while (!fld.exists() && !QDir (fld.absFilePath()).isRoot());

    if (!fld.exists())
    {
        CVirtualBox vbox = vboxGlobal().virtualBox();
        fld = QFileInfo (vbox.GetSystemProperties().GetDefaultHardDiskFolder());
        if (!fld.exists())
            fld = vbox.GetHomeFolder();
    }

//    QFileDialog fd (this, "NewMediaDialog", TRUE);
//    fd.setMode (QFileDialog::AnyFile);
//    fd.setViewMode (QFileDialog::List);
//    fd.addFilter (tr( "Hard disk images (*.vdi)" ));
//    fd.setCaption (tr( "Select a file for the new hard disk image file" ));
//    fd.setDir (d);

    QString selected = QFileDialog::getSaveFileName (
        fld.absFilePath(),
        tr ("Hard disk images (*.vdi)"),
        this,
        "NewMediaDialog",
        tr ("Select a file for the new hard disk image file"));

//    if ( fd.exec() == QDialog::Accepted ) {
//        leName->setText (QDir::convertSeparators (fd.selectedFile()));
    if (selected)
    {
        if (QFileInfo (selected).extension().isEmpty())
            selected += ".vdi";
        leName->setText (QDir::convertSeparators (selected));
        leName->selectAll();
        leName->setFocus();
    }
}
