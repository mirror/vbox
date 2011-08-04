/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotDetailsDlg class implementation
 */

/*
 * Copyright (C) 2008-2009 Oracle Corporation
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
# include "precomp.h"
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */
/* Global includes */
#include <QDateTime>
#include <QPushButton>
#include <QScrollArea>

/* Local includes */
#include <VBoxGlobal.h>
#include <UIMessageCenter.h>
#include <VBoxSnapshotDetailsDlg.h>
#include <VBoxUtils.h>
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

VBoxSnapshotDetailsDlg::VBoxSnapshotDetailsDlg (QWidget *aParent)
    : QIWithRetranslateUI <QDialog> (aParent)
{
    /* Apply UI decorations */
    Ui::VBoxSnapshotDetailsDlg::setupUi (this);

    /* Setup mLbThumbnail label */
    mLbThumbnail->setCursor (Qt::PointingHandCursor);
    mLbThumbnail->installEventFilter (this);

    /* Setup mTeDetails browser */
    mTeDetails->viewport()->setAutoFillBackground (false);
    mTeDetails->setFocus();

    /* Setup connections */
    connect (mLeName, SIGNAL (textChanged (const QString&)), this, SLOT (onNameChanged (const QString&)));
    connect (mButtonBox, SIGNAL (helpRequested()), &msgCenter(), SLOT (sltShowHelpHelpDialog()));
}

void VBoxSnapshotDetailsDlg::getFromSnapshot (const CSnapshot &aSnapshot)
{
    mSnapshot = aSnapshot;
    CMachine machine = mSnapshot.GetMachine();

    /* Get general properties */
    mLeName->setText (aSnapshot.GetName());
    mTeDescription->setText (aSnapshot.GetDescription());

    /* Get timestamp info */
    QDateTime timestamp;
    timestamp.setTime_t (mSnapshot.GetTimeStamp() / 1000);
    bool dateTimeToday = timestamp.date() == QDate::currentDate();
    QString dateTime = dateTimeToday ? timestamp.time().toString (Qt::LocalDate) : timestamp.toString (Qt::LocalDate);
    mTxTaken->setText (dateTime);

    /* Get thumbnail if present */
    ULONG width = 0, height = 0;
    QVector <BYTE> thumbData = machine.ReadSavedThumbnailToArray (0, true, width, height);
    mThumbnail = thumbData.size() != 0 ? QPixmap::fromImage (QImage (thumbData.data(), width, height, QImage::Format_RGB32).copy()) : QPixmap();
    QVector <BYTE> screenData = machine.ReadSavedScreenshotPNGToArray (0, width, height);
    mScreenshot = screenData.size() != 0 ? QPixmap::fromImage (QImage::fromData (screenData.data(), screenData.size(), "PNG")) : QPixmap();

    QGridLayout *lt = qobject_cast <QGridLayout*> (layout());
    Assert (lt);
    if (mThumbnail.isNull())
    {
        lt->removeWidget (mLbThumbnail);
        mLbThumbnail->setHidden (true);

        lt->removeWidget (mLeName);
        lt->removeWidget (mTxTaken);
        lt->addWidget (mLeName, 0, 1, 1, 2);
        lt->addWidget (mTxTaken, 1, 1, 1, 2);
    }
    else
    {
        lt->removeWidget (mLeName);
        lt->removeWidget (mTxTaken);
        lt->addWidget (mLeName, 0, 1);
        lt->addWidget (mTxTaken, 1, 1);

        lt->removeWidget (mLbThumbnail);
        lt->addWidget (mLbThumbnail, 0, 2, 2, 1);
        mLbThumbnail->setHidden (false);
    }

    retranslateUi();
}

void VBoxSnapshotDetailsDlg::putBackToSnapshot()
{
    AssertReturn (!mSnapshot.isNull(), (void) 0);

    /* We need a session when we manipulate the snapshot data of a machine. */
    CSession session = vboxGlobal().openSession(mSnapshot.GetMachine().GetId(), true);
    if (session.isNull())
        return;

    mSnapshot.SetName(mLeName->text());
    mSnapshot.SetDescription(mTeDescription->toPlainText());

    /* Close the session again. */
    session.UnlockMachine();
}

void VBoxSnapshotDetailsDlg::retranslateUi()
{
    /* Translate uic generated strings */
    Ui::VBoxSnapshotDetailsDlg::retranslateUi (this);

    if(mSnapshot.isNull())
        return;

    CMachine machine = mSnapshot.GetMachine();

    setWindowTitle (tr ("Details of %1 (%2)").arg (mSnapshot.GetName()).arg (machine.GetName()));

    mLbThumbnail->setToolTip (mScreenshot.isNull() ? QString() : tr ("Click to enlarge the screenshot."));

    mTeDetails->setText (vboxGlobal().detailsReport (machine, false /* with links? */));
}

bool VBoxSnapshotDetailsDlg::eventFilter (QObject *aObject, QEvent *aEvent)
{
    Assert (aObject == mLbThumbnail);
    if (aEvent->type() == QEvent::MouseButtonPress && !mScreenshot.isNull())
    {
        VBoxScreenshotViewer *viewer = new VBoxScreenshotViewer (this, mScreenshot, mSnapshot.GetMachine().GetName(), mSnapshot.GetName());
        viewer->show();
    }
    return QDialog::eventFilter (aObject, aEvent);
}

void VBoxSnapshotDetailsDlg::showEvent (QShowEvent *aEvent)
{
    if (!mLbThumbnail->pixmap() && !mThumbnail.isNull())
    {
        mLbThumbnail->setPixmap (mThumbnail.scaled (QSize (1, mLbThumbnail->height()),
                                                    Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        retranslateUi();
    }

    QDialog::showEvent (aEvent);
}

void VBoxSnapshotDetailsDlg::onNameChanged (const QString &aText)
{
    mButtonBox->button (QDialogButtonBox::Ok)->setEnabled (!aText.trimmed().isEmpty());
}

VBoxScreenshotViewer::VBoxScreenshotViewer (QWidget *aParent, const QPixmap &aScreenshot,
                                            const QString &aSnapshotName, const QString &aMachineName)
    : QIWithRetranslateUI2 <QWidget> (aParent, Qt::Tool)
    , mArea (new QScrollArea (this))
    , mPicture (new QLabel)
    , mScreenshot (aScreenshot)
    , mSnapshotName (aSnapshotName)
    , mMachineName (aMachineName)
    , mZoomMode (true)
{
    setWindowModality (Qt::ApplicationModal);
    setCursor (Qt::PointingHandCursor);
    QVBoxLayout *layout = new QVBoxLayout (this);
    layout->setMargin (0);

    mArea->setWidget (mPicture);
    mArea->setWidgetResizable (true);
    layout->addWidget (mArea);

    double aspectRatio = (double) aScreenshot.height() / aScreenshot.width();
    QSize maxSize = aScreenshot.size() + QSize (mArea->frameWidth() * 2, mArea->frameWidth() * 2);
    QSize initSize = QSize (640, (int)(640 * aspectRatio)).boundedTo (maxSize);

    setMaximumSize (maxSize);

    QRect geo (QPoint (0, 0), initSize);
    geo.moveCenter (parentWidget()->geometry().center());
    setGeometry (geo);

    retranslateUi();
}

void VBoxScreenshotViewer::retranslateUi()
{
    setWindowTitle (tr ("Screenshot of %1 (%2)").arg (mSnapshotName).arg (mMachineName));
}

void VBoxScreenshotViewer::showEvent (QShowEvent *aEvent)
{
    adjustPicture();
    QIWithRetranslateUI2 <QWidget>::showEvent (aEvent);
}

void VBoxScreenshotViewer::resizeEvent (QResizeEvent *aEvent)
{
    adjustPicture();
    QIWithRetranslateUI2 <QWidget>::resizeEvent (aEvent);
}

void VBoxScreenshotViewer::mousePressEvent (QMouseEvent *aEvent)
{
    mZoomMode = !mZoomMode;
    adjustPicture();
    QIWithRetranslateUI2 <QWidget>::mousePressEvent (aEvent);
}

void VBoxScreenshotViewer::keyPressEvent (QKeyEvent *aEvent)
{
    if (aEvent->key() == Qt::Key_Escape)
        deleteLater();
    QIWithRetranslateUI2 <QWidget>::keyPressEvent (aEvent);
}

void VBoxScreenshotViewer::adjustPicture()
{
    if (mZoomMode)
    {
        mArea->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        mArea->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        mPicture->setPixmap (mScreenshot.scaled (mArea->viewport()->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        mPicture->setToolTip (tr ("Click to view non-scaled screenshot."));
    }
    else
    {
        mArea->setVerticalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        mArea->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);
        mPicture->setPixmap (mScreenshot);
        mPicture->setToolTip (tr ("Click to view scaled screenshot."));
    }
}

