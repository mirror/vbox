/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotDetailsDlg class implementation.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
# include <QDateTime>
# include <QPushButton>
# include <QScrollArea>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIMessageCenter.h"
# include "VBoxSnapshotDetailsDlg.h"
# include "VBoxUtils.h"

/* COM includes: */
# include "CMachine.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QWiget extension providing GUI with snapshot screenshot viewer widget. */
class UIScreenshotViewer : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs screenshow viewer passing @a pParent to the base-class.
      * @param  pixmapScreenshot  Brings the screenshot to show.
      * @param  strSnapshotName   Brings the snapshot name.
      * @param  strMachineName    Brings the machine name. */
    UIScreenshotViewer(const QPixmap &pixmapScreenshot,
                       const QString &strSnapshotName,
                       const QString &strMachineName,
                       QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles mouse press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles key press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts picture. */
    void adjustPicture();

    /** Holds whether this widget was polished. */
    bool m_fPolished;

    /** Holds the screenshot to show. */
    QPixmap m_pixmapScreenshot;
    /** Holds the snapshot name. */
    QString m_strSnapshotName;
    /** Holds the machine name. */
    QString m_strMachineName;

    /** Holds the scroll-area instance. */
    QScrollArea *m_pScrollArea;
    /** Holds the picture label instance. */
    QLabel *m_pLabelPicture;

    /** Holds whether we are in zoom mode. */
    bool m_fZoomMode;
};


/*********************************************************************************************************************************
*   Class UIScreenshotViewer implementation.                                                                                   *
*********************************************************************************************************************************/

UIScreenshotViewer::UIScreenshotViewer(const QPixmap &pixmapScreenshot,
                                       const QString &strSnapshotName,
                                       const QString &strMachineName,
                                       QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QWidget>(pParent, Qt::Tool)
    , m_fPolished(false)
    , m_pixmapScreenshot(pixmapScreenshot)
    , m_strSnapshotName(strSnapshotName)
    , m_strMachineName(strMachineName)
    , m_pScrollArea(0)
    , m_pLabelPicture(0)
    , m_fZoomMode(true)
{
    /* Prepare: */
    prepare();
}

void UIScreenshotViewer::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Screenshot of %1 (%2)").arg(m_strSnapshotName).arg(m_strMachineName));
}

void UIScreenshotViewer::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::showEvent(pEvent);

    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIScreenshotViewer::polishEvent(QShowEvent * /* pEvent */)
{
    /* Adjust the picture: */
    adjustPicture();
}

void UIScreenshotViewer::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::resizeEvent(pEvent);

    /* Adjust the picture: */
    adjustPicture();
}

void UIScreenshotViewer::mousePressEvent(QMouseEvent *pEvent)
{
    /* Toggle the zoom mode: */
    m_fZoomMode = !m_fZoomMode;

    /* Adjust the picture: */
    adjustPicture();

    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::mousePressEvent(pEvent);
}

void UIScreenshotViewer::keyPressEvent(QKeyEvent *pEvent)
{
    /* Close on escape: */
    if (pEvent->key() == Qt::Key_Escape)
        close();

    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::keyPressEvent(pEvent);
}

void UIScreenshotViewer::prepare()
{
    /* Screenshot viewer is an application-modal window: */
    setWindowModality(Qt::ApplicationModal);
    /* With the pointing-hand cursor: */
    setCursor(Qt::PointingHandCursor);
    /* And it's being deleted when closed: */
    setAttribute(Qt::WA_DeleteOnClose);

    /* Create layout: */
    new QVBoxLayout(this);
    AssertPtrReturnVoid(layout());
    {
        /* Configure layout: */
        layout()->setContentsMargins(0, 0, 0, 0);

        /* Create scroll-area: */
        m_pScrollArea = new QScrollArea;
        AssertPtrReturnVoid(m_pScrollArea);
        {
            /* Configure scroll-area: */
            m_pScrollArea->setWidgetResizable (true);

            /* Create picture label: */
            m_pLabelPicture = new QLabel;
            AssertPtrReturnVoid(m_pLabelPicture);
            {
                /* Add into scroll-area: */
                m_pScrollArea->setWidget(m_pLabelPicture);
            }

            /* Add into layout: */
            layout()->addWidget(m_pScrollArea);
        }
    }

    /* Calculate aspect-ratio: */
    double dAspectRatio = (double)m_pixmapScreenshot.height() / m_pixmapScreenshot.width();
    /* Calculate maximum window size: */
    const QSize maxSize = m_pixmapScreenshot.size() + QSize(m_pScrollArea->frameWidth() * 2, m_pScrollArea->frameWidth() * 2);
    /* Calculate initial window size: */
    const QSize initSize = QSize(640, (int)(640 * dAspectRatio)).boundedTo(maxSize);

    /* Apply maximum window size restrictions: */
    setMaximumSize(maxSize);
    /* Apply initial window size: */
    resize(initSize);

    /* Apply language settings: */
    retranslateUi();

    /* Center according requested widget: */
    VBoxGlobal::centerWidget(this, parentWidget(), false);
}

void UIScreenshotViewer::adjustPicture()
{
    if (m_fZoomMode)
    {
        m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pLabelPicture->setPixmap(m_pixmapScreenshot.scaled(m_pScrollArea->viewport()->size(),
                                                             Qt::IgnoreAspectRatio,
                                                             Qt::SmoothTransformation));
        m_pLabelPicture->setToolTip(tr("Click to view non-scaled screenshot."));
    }
    else
    {
        m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_pLabelPicture->setPixmap(m_pixmapScreenshot);
        m_pLabelPicture->setToolTip(tr("Click to view scaled screenshot."));
    }
}


/*********************************************************************************************************************************
*   Class VBoxSnapshotDetailsDlg implementation.                                                                                 *
*********************************************************************************************************************************/

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
    QVector <BYTE> thumbData = machine.ReadSavedThumbnailToArray (0, KBitmapFormat_BGR0, width, height);
    mThumbnail = thumbData.size() != 0 ? QPixmap::fromImage (QImage (thumbData.data(), width, height, QImage::Format_RGB32).copy()) : QPixmap();
    QVector <BYTE> screenData = machine.ReadSavedScreenshotToArray (0, KBitmapFormat_PNG, width, height);
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
    CSession session = vboxGlobal().openExistingSession(mSnapshot.GetMachine().GetId());
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
        UIScreenshotViewer *pViewer = new UIScreenshotViewer(mScreenshot,
                                                             mSnapshot.GetMachine().GetName(),
                                                             mSnapshot.GetName(),
                                                             this);
        pViewer->show();
        pViewer->activateWindow();
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

#include "VBoxSnapshotDetailsDlg.moc"

