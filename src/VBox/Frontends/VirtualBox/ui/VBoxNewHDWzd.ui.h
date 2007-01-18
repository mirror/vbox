/**
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * "New hard disk" wizard UI include (Qt Designer)
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

static const int MinVDISize = 4;

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
        // no path info at all, use defaultFolder
        fi = QFileInfo (defaultFolder, file);
    }
    else if (fi.isRelative())
    {
        // resolve relatively to homeFolder
        fi = QFileInfo (homeFolder, file);
    }

    return QDir::convertSeparators (fi.absFilePath());
}


class QISizeValidator : public QValidator
{
public:
    QISizeValidator (QObject * aParent, ULONG64 aMinSize, ULONG64 aMaxSize,
                     const char * aName = 0) :
        QValidator (aParent, aName), mMinSize(aMinSize), mMaxSize(aMaxSize) {}

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

            ULONG64 sizeMB = vboxGlobal().parseSize (input) / _1M;
            if (sizeMB > mMaxSize || sizeMB < mMinSize)
                warning = true;

            if (result)
                return warning ? Intermediate : Acceptable;
            else
                return Invalid;
        }
    }

protected:
    ULONG64 mMinSize;
    ULONG64 mMaxSize;
};


void VBoxNewHDWzd::init()
{
    // disable help buttons
    helpButton()->setShown( false );

    // fix tab order to get the proper direction
    // (originally the focus goes Next/Finish -> Back -> Cancel -> page)
    QWidget::setTabOrder( backButton(), nextButton() );
    QWidget::setTabOrder( nextButton(), finishButton() );
    QWidget::setTabOrder( finishButton(), cancelButton() );

    // setup connections and set validation for pages
    // ----------------------------------------------------------------------

    // Image type page

    // Name and Size page

    CSystemProperties sysProps = vboxGlobal().virtualBox().GetSystemProperties();

    MaxVDISize = sysProps.GetMaxVDISize();

    leName->setValidator (new QRegExpValidator (QRegExp( ".+" ), this));

    leSize->setValidator (new QISizeValidator (this, MinVDISize, MaxVDISize));
    leSize->setAlignment (Qt::AlignRight);

    wvalNameAndSize = new QIWidgetValidator (pageNameAndSize, this );
    connect(
        wvalNameAndSize, SIGNAL( validityChanged (const QIWidgetValidator *) ),
        this, SLOT( enableNext (const QIWidgetValidator *) )
    );

    // Summary page

    // filter out Enter keys in order to direct them to the default dlg button
    QIKeyFilter *ef = new QIKeyFilter (this, Key_Enter);
    ef->watchOn (teSummary) ;

    // set initial values
    // ----------------------------------------------------------------------

    // Image type page

    // Name and Size page

    static ulong HDNumber = 0;
    leName->setText (QString ("NewHardDisk%1.vdi").arg (++ HDNumber));

    sliderStep = 50;
    slSize->setFocusPolicy (QWidget::StrongFocus);
    slSize->setPageStep (sliderStep);
    slSize->setLineStep (sliderStep);
    slSize->setTickInterval (0);
    slSize->setMinValue ((int)(log10 ((double)MinVDISize)       / log10 (2.0) * sliderStep));
    slSize->setMaxValue ((int)(log10 ((double)(MaxVDISize + 1)) / log10 (2.0) * sliderStep));
    txSizeMin->setText (vboxGlobal().formatSize (MinVDISize * _1M));
    txSizeMax->setText (vboxGlobal().formatSize (MaxVDISize * _1M));
    // initial HD size value = pow(2.0, 11) = 2048 MB
    currentSize = 2 * _1K;
    leSize->setText (vboxGlobal().formatSize (currentSize * _1M));
    slSize->setValue ((int)(log10 ((double)currentSize) / log10 (2.0) * sliderStep));
    // limit the max. size of QLineEdit (STUPID Qt has NO correct means for that)
    leSize->setMaximumSize(
        leSize->fontMetrics().width ("88888.88 MB") + leSize->frameWidth() * 2,
        leSize->height()
    );

    // Summary page

    teSummary->setPaper (pageSummary->backgroundBrush());

    // update the next button state for pages with validation
    // (validityChanged() connected to enableNext() will do the job)
    wvalNameAndSize->revalidate();

    // the finish button on the Summary page is always enabled
    setFinishEnabled (pageSummary, true);
}


void VBoxNewHDWzd::setRecommendedFileName (const QString &aName)
{
    leName->setText (aName);
}


void VBoxNewHDWzd::setRecommendedSize (ulong aSize)
{
    AssertReturn (aSize >= (ulong) MinVDISize &&
                  aSize <= (ulong) MaxVDISize, (void) 0);
    currentSize = aSize;
    slSize->setValue ((int)(log10 ((double)currentSize) / log10 (2.0) * sliderStep));
    leSize->setText (vboxGlobal().formatSize(aSize * _1M));
}


QString VBoxNewHDWzd::imageFileName()
{
    QString name = QDir::convertSeparators (leName->text());
    if (QFileInfo (name).extension().isEmpty())
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


void VBoxNewHDWzd::enableNext( const QIWidgetValidator *wval )
{
    setNextEnabled (wval->widget(), wval->isValid());
}


void VBoxNewHDWzd::revalidate( QIWidgetValidator * /*wval*/ )
{
    // do individual validations for pages

// template:
//    QWidget *pg = wval->widget();
//    bool valid = wval->isOtherValid();
//
//    if ( pg == pageXXX ) {
//        valid = XXX;
//    }
//
//    wval->setOtherValid( valid );
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
        type.remove ('&');

        // compose summary
        QString summary = QString (tr(
            "<table>"
            "<tr><td>Type:</td><td>%1</td></tr>"
            "<tr><td>Location:</td><td>%2</td></tr>"
            "<tr><td>Size:</td><td>%3&nbsp;MB</td></tr>"
            "</table>"
        ))
            .arg (type)
            .arg (composeFullFileName (imageFileName()))
            .arg (imageSize());
        teSummary->setText (summary);
        // set Finish to default
        finishButton()->setDefault (true);
    }
    else
    {
        // always set Next to default
        nextButton()->setDefault (true);
    }

    QWizard::showPage (page);

    // fix focus on the last page. when we go to the last page
    // having the Next in focus the focus goes to the Cancel
    // button because when the Next hides Finish is not yet shown.
    if (page == pageSummary && focusWidget() == cancelButton())
        finishButton()->setFocus();

    // setup focus for individual pages
    if (page == pageType) {
        bgType->setFocus();
    } else if (page == pageNameAndSize) {
        leName->setFocus();
    } else if (page == pageSummary) {
        teSummary->setFocus();
    }
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
    CHardDisk hd = vbox.CreateHardDisk (CEnums::VirtualDiskImage);

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
        currentSize = (ULONG64)(pow (2.0, ((double)val / sliderStep)));
        leSize->setText (vboxGlobal().formatSize (currentSize * _1M));
    }
}


void VBoxNewHDWzd::leSize_textChanged( const QString &text )
{
    if (focusWidget() == leSize)
    {
        currentSize = vboxGlobal().parseSize (text) / _1M;
        slSize->setValue ((int)(log10 ((double)currentSize) / log10 (2.0) * sliderStep));
    }
}


void VBoxNewHDWzd::tbNameSelect_clicked()
{
    // set the first parent directory that exists as the current
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
