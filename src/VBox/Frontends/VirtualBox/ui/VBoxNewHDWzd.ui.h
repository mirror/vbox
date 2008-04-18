/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "New hard disk" wizard UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

    return QDir::convertSeparators (fi.absFilePath());
}

/// @todo (r=dmik) not currently used
#if 0
class QISizeValidator : public QValidator
{
public:

    QISizeValidator (QObject * aParent,
                     Q_UINT64 aMinSize, Q_UINT64 aMaxSize,
                     const char * aName = 0) :
        QValidator (aParent, aName), mMinSize (aMinSize), mMaxSize (aMaxSize) {}

    ~QISizeValidator() {}

    State validate (QString &input, int &/*pos*/) const
    {
        QRegExp regexp ("^([^M^G^T^P^\\d\\s]*)(\\d+(([^\\s^\\d^M^G^T^P]+)(\\d*))?)?(\\s*)([MGTP]B?)?$");
        int position = regexp.search (input);
        if (position == -1)
            return Invalid;
        else
        {
            if (!regexp.cap (1).isEmpty() ||
                 regexp.cap (4).length() > 1 ||
                 regexp.cap (5).length() > 2 ||
                 regexp.cap (6).length() > 1)
                return Invalid;

            if (regexp.cap (7).length() == 1)
                return Intermediate;

            QString size = regexp.cap (2);
            if (size.isEmpty())
                return Intermediate;

            bool result = false;
            bool warning = false;
            if (!regexp.cap (4).isEmpty() && regexp.cap (5).isEmpty())
            {
                size += "0";
                warning = true;
            }
            QLocale::system().toDouble (size, &result);

            Q_UINT64 sizeB = vboxGlobal().parseSize (input);
            if (sizeB > mMaxSize || sizeB < mMinSize)
                warning = true;

            if (result)
                return warning ? Intermediate : Acceptable;
            else
                return Invalid;
        }
    }

protected:

    Q_UINT64 mMinSize;
    Q_UINT64 mMaxSize;
};
#endif

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

    /* Image type page */

    /* Name and Size page */

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();

    maxVDISize = sysProps.GetMaxVDISize();

    /* Detect how many steps to recognize between adjacent powers of 2
     * to ensure that the last slider step is exactly maxVDISize */
    sliderScale = 0;
    {
        int pow = log2i (maxVDISize);
        Q_UINT64 tickMB = Q_UINT64 (1) << pow;
        if (tickMB < maxVDISize)
        {
            Q_UINT64 tickMBNext = Q_UINT64 (1) << (pow + 1);
            Q_UINT64 gap = tickMBNext - maxVDISize;
            /// @todo (r=dmik) overflow may happen if maxVDISize is TOO big
            sliderScale = (int) ((tickMBNext - tickMB) / gap);
        }
    }
    sliderScale = QMAX (sliderScale, 8);

    leName->setValidator (new QRegExpValidator (QRegExp( ".+" ), this));

    leSize->setValidator (new QRegExpValidator (vboxGlobal().sizeRegexp(), this));
    leSize->setAlignment (Qt::AlignRight);

    wvalNameAndSize = new QIWidgetValidator (pageNameAndSize, this);
    connect (wvalNameAndSize, SIGNAL (validityChanged (const QIWidgetValidator *)),
             this, SLOT (enableNext (const QIWidgetValidator *)));
    connect (wvalNameAndSize, SIGNAL (isValidRequested (QIWidgetValidator *)),
             this, SLOT (revalidate (QIWidgetValidator *)));
    /* we ask revalidate only when currentSize is changed after manually
     * editing the line edit field; the slider cannot produce invalid values */
    connect (leSize, SIGNAL (textChanged (const QString &)),
             wvalNameAndSize, SLOT (revalidate()));

    /* Summary page */

    teSummary = new QITextEdit (pageSummary);
    teSummary->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Minimum);
    teSummary->setFrameShape (QTextEdit::NoFrame);
    teSummary->setReadOnly (TRUE);
    summaryLayout->insertWidget (1, teSummary);

    /* filter out Enter keys in order to direct them to the default dlg button */
    QIKeyFilter *ef = new QIKeyFilter (this, Key_Enter);
    ef->watchOn (teSummary);

    /* set initial values
     * ---------------------------------------------------------------------- */

    /* Image type page */

    /* Name and Size page */

    static ulong HDNumber = 0;
    leName->setText (QString ("NewHardDisk%1.vdi").arg (++ HDNumber));

    slSize->setFocusPolicy (QWidget::StrongFocus);
    slSize->setPageStep (sliderScale);
    slSize->setLineStep (sliderScale / 8);
    slSize->setTickInterval (0);
    slSize->setMinValue (sizeMBToSlider (MinVDISize, sliderScale));
    slSize->setMaxValue (sizeMBToSlider (maxVDISize, sliderScale));

    txSizeMin->setText (vboxGlobal().formatSize (MinVDISize * _1M));
    txSizeMax->setText (vboxGlobal().formatSize (maxVDISize * _1M));

    /* limit the max. size of QLineEdit (STUPID Qt has NO correct means for that) */
    leSize->setMaximumSize (
        leSize->fontMetrics().width ("88888.88 MB") + leSize->frameWidth() * 2,
        leSize->height());

    setRecommendedSize (InitialVDISize);

    /* Summary page */

    teSummary->setPaper (pageSummary->backgroundBrush());

    /* update the next button state for pages with validation
     * (validityChanged() connected to enableNext() will do the job) */
    wvalNameAndSize->revalidate();

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
    AssertReturnVoid (aSize >= MinVDISize && aSize <= maxVDISize);
    currentSize = aSize;
    slSize->setValue (sizeMBToSlider (currentSize, sliderScale));
    leSize->setText (vboxGlobal().formatSize (currentSize * _1M));
    updateSizeToolTip (currentSize * _1M);
}


QString VBoxNewHDWzd::imageFileName()
{
    QString name = QDir::convertSeparators (leName->text());

    /* remove all trailing dots to avoid multiple dots before .vdi */
    int len;
    while (len = name.length(), len > 0 && name [len - 1] == '.')
        name.truncate (len - 1);

    QString ext = QFileInfo (name).extension();
    /* compare against the proper case */
#if defined (Q_OS_FREEBSD) || defined (Q_OS_LINUX) || defined (Q_OS_NETBSD) || defined (Q_OS_OPENBSD) || defined (Q_OS_SOLARIS)
#elif defined (Q_OS_WIN) || defined (Q_OS_OS2) || defined (Q_OS_MACX)
    ext = ext.lower();
#else
    #error Port me!
#endif

    if (ext != "vdi")
        name += ".vdi";

    return name;
}


Q_UINT64 VBoxNewHDWzd::imageSize()
{
    return currentSize;
}


bool VBoxNewHDWzd::isDynamicImage()
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
        valid = currentSize >= MinVDISize && currentSize <= maxVDISize;
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
        if (QFileInfo (imageFileName()).exists())
        {
            vboxProblem().sayCannotOverwriteHardDiskImage (this, imageFileName());
            return;
        }
    }

    if (page == pageSummary)
    {
        QString type = rbDynamicType->isOn() ? rbDynamicType->text()
                                             : rbFixedType->text();
        type = VBoxGlobal::removeAccelMark (type);

        Q_UINT64 sizeB = imageSize() * _1M;

        // compose summary
        QString summary = QString (tr(
            "<table>"
            "<tr><td>Type:</td><td>%1</td></tr>"
            "<tr><td>Location:</td><td>%2</td></tr>"
            "<tr><td>Size:</td><td>%3&nbsp;(%4&nbsp;Bytes)</td></tr>"
            "</table>"
        ))
            .arg (type)
            .arg (composeFullFileName (imageFileName()))
            .arg (VBoxGlobal::formatSize (sizeB))
            .arg (sizeB);
        teSummary->setText (summary);
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
        teSummary->setFocus();
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
 *  Performs steps necessary to create a hard disk. This method handles all
 *  errors and shows the corresponding messages when appropriate.
 *
 *  @return     wheter the creation was successful or not
 */
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

    vboxProblem().showModalProgressDialog (progress, caption(), parentWidget());

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

    chd = hd;
    return true;
}


void VBoxNewHDWzd::slSize_valueChanged( int val )
{
    if (focusWidget() == slSize)
    {
        currentSize = sliderToSizeMB (val, sliderScale);
        leSize->setText (vboxGlobal().formatSize (currentSize * _1M));
        updateSizeToolTip (currentSize * _1M);
    }
}


void VBoxNewHDWzd::leSize_textChanged( const QString &text )
{
    if (focusWidget() == leSize)
    {
        currentSize = vboxGlobal().parseSize (text);
        updateSizeToolTip (currentSize);
        currentSize /= _1M;
        slSize->setValue (sizeMBToSlider (currentSize, sliderScale));
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
        fld = QFileInfo (vbox.GetSystemProperties().GetDefaultVDIFolder());
        if (!fld.exists())
            fld = vbox.GetHomeFolder();
    }

//    QFileDialog fd (this, "NewDiskImageDialog", TRUE);
//    fd.setMode (QFileDialog::AnyFile);
//    fd.setViewMode (QFileDialog::List);
//    fd.addFilter (tr( "Hard disk images (*.vdi)" ));
//    fd.setCaption (tr( "Select a file for the new hard disk image file" ));
//    fd.setDir (d);

    QString selected = QFileDialog::getSaveFileName (
        fld.absFilePath(),
        tr ("Hard disk images (*.vdi)"),
        this,
        "NewDiskImageDialog",
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
