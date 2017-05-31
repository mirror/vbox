/* $Id$ */
/** @file
 * VBox Qt GUI - UISnapshotDetailsWidget class implementation.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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
*   Class UISnapshotDetailsWidget implementation.                                                                                *
*********************************************************************************************************************************/

UISnapshotDetailsWidget::UISnapshotDetailsWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
{
    /* Prepare: */
    prepare();
}

void UISnapshotDetailsWidget::setData(const UIDataSnapshot &data, const CSnapshot &comSnapshot)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;

    /* Cache snapshot: */
    m_comSnapshot = comSnapshot;

    /* Load snapshot data: */
    loadSnapshotData();
}

void UISnapshotDetailsWidget::clearData()
{
    /* Reset old/new data: */
    m_oldData = UIDataSnapshot();
    m_newData = m_oldData;

    /* Reset snapshot: */
    m_comSnapshot = CSnapshot();

    /* Load snapshot data: */
    loadSnapshotData();
}

bool UISnapshotDetailsWidget::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* We have filter for thumbnail label only: */
    AssertReturn(pObject == mLbThumbnail, false);

    /* For a mouse-press event inside the thumbnail area: */
    if (pEvent->type() == QEvent::MouseButtonPress && !m_pixmapScreenshot.isNull())
    {
        /* We are creating screenshot viewer and show it: */
        UIScreenshotViewer *pViewer = new UIScreenshotViewer(m_pixmapScreenshot,
                                                             m_comSnapshot.GetMachine().GetName(),
                                                             m_comSnapshot.GetName(),
                                                             this);
        pViewer->show();
        pViewer->activateWindow();
    }

    /* Call to base-class: */
    return QWidget::eventFilter(pObject, pEvent);
}

void UISnapshotDetailsWidget::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::VBoxSnapshotDetailsDlg::retranslateUi(this);

    /* And if snapshot is valid: */
    if (!m_comSnapshot.isNull())
    {
        /* Get the machine: */
        const CMachine comMachine = m_comSnapshot.GetMachine();

        /* Translate the picture tool-tip: */
        mLbThumbnail->setToolTip(m_pixmapScreenshot.isNull() ? QString() : tr("Click to enlarge the screenshot."));

        /* Rebuild the details report: */
        mTeDetails->setText(vboxGlobal().detailsReport(comMachine, false /* with links? */));
    }
    else
    {
        /* Clear the picture tool-tip: */
        mLbThumbnail->setToolTip(QString());

        /* Clear the details report: */
        mTeDetails->clear();
    }
}

void UISnapshotDetailsWidget::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QWidget>::showEvent(pEvent);

    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UISnapshotDetailsWidget::polishEvent(QShowEvent * /* pEvent */)
{
    /* If we haven't assigned the thumbnail yet, do it now: */
    if (!mLbThumbnail->pixmap() && !m_pixmapThumbnail.isNull())
    {
        mLbThumbnail->setPixmap(m_pixmapThumbnail.scaled(QSize(1, mLbThumbnail->height()),
                                                         Qt::KeepAspectRatioByExpanding,
                                                         Qt::SmoothTransformation));
        /* Make sure tool-tip is translated afterwards: */
        retranslateUi();
    }
}

void UISnapshotDetailsWidget::sltHandleNameChange()
{
    m_newData.m_strName = mLeName->text();
    //revalidate(m_pErrorPaneName);
    notify();
}

void UISnapshotDetailsWidget::sltHandleDescriptionChange()
{
    m_newData.m_strDescription = mTeDescription->toPlainText();
    //revalidate(m_pErrorPaneName);
    notify();
}

void UISnapshotDetailsWidget::prepare()
{
    /* Apply UI decorations: */
    Ui::VBoxSnapshotDetailsDlg::setupUi(this);

    /* Layout created in the .ui file: */
    {
        /* Name editor created in the .ui file: */
        AssertPtrReturnVoid(mLeName);
        {
            /* Configure editor: */
            connect(mLeName, &QLineEdit::textChanged,
                    this, &UISnapshotDetailsWidget::sltHandleNameChange);
        }

        /* Description editor created in the .ui file: */
        AssertPtrReturnVoid(mTeDescription);
        {
            /* Configure editor: */
            connect(mTeDescription, &QTextEdit::textChanged,
                    this, &UISnapshotDetailsWidget::sltHandleDescriptionChange);
        }

        /* Thumbnail label created in the .ui file: */
        AssertPtrReturnVoid(mLbThumbnail);
        {
            /* Configure thumbnail label: */
            mLbThumbnail->setCursor(Qt::PointingHandCursor);
            mLbThumbnail->installEventFilter(this);
        }

        /* Details browser created in the .ui file: */
        AssertPtrReturnVoid(mTeDetails);
        {
            /* Configure details browser: */
            mTeDetails->viewport()->setAutoFillBackground(false);
            mTeDetails->setFocus();
        }
    }
}

void UISnapshotDetailsWidget::loadSnapshotData()
{
    /* Read general snapshot properties: */
    mLeName->setText(m_newData.m_strName);
    mTeDescription->setText(m_newData.m_strDescription);

    /* If there is a snapshot: */
    if (m_comSnapshot.isNotNull())
    {
        /* Calculate snapshot timestamp info: */
        QDateTime timestamp;
        timestamp.setTime_t(m_comSnapshot.GetTimeStamp() / 1000);
        bool fDateTimeToday = timestamp.date() == QDate::currentDate();
        QString dateTime = fDateTimeToday ? timestamp.time().toString(Qt::LocalDate) : timestamp.toString(Qt::LocalDate);
        mTxTaken->setText(dateTime);

        /* Read snapshot display contents: */
        CMachine comMachine = m_comSnapshot.GetMachine();
        ULONG iWidth = 0, iHeight = 0;

        /* Get thumbnail if present: */
        QVector<BYTE> thumbData = comMachine.ReadSavedThumbnailToArray(0, KBitmapFormat_BGR0, iWidth, iHeight);
        m_pixmapThumbnail = thumbData.size() != 0 ? QPixmap::fromImage(QImage(thumbData.data(),
                                                                              iWidth, iHeight,
                                                                              QImage::Format_RGB32).copy())
                                                  : QPixmap();

        /* Get screenshot if present: */
        QVector<BYTE> screenData = comMachine.ReadSavedScreenshotToArray(0, KBitmapFormat_PNG, iWidth, iHeight);
        m_pixmapScreenshot = screenData.size() != 0 ? QPixmap::fromImage(QImage::fromData(screenData.data(),
                                                                                          screenData.size(),
                                                                                          "PNG"))
                                                    : QPixmap();

        // TODO: Check whether layout manipulations are really
        //       necessary, they looks a bit dangerous to me..
        QGridLayout *pLayout = qobject_cast<QGridLayout*>(layout());
        AssertPtrReturnVoid(pLayout);
        if (m_pixmapThumbnail.isNull())
        {
            pLayout->removeWidget(mLbThumbnail);
            mLbThumbnail->setHidden(true);

            pLayout->removeWidget(mLeName);
            pLayout->removeWidget(mTxTaken);
            pLayout->addWidget(mLeName, 0, 1, 1, 2);
            pLayout->addWidget(mTxTaken, 1, 1, 1, 2);
        }
        else
        {
            pLayout->removeWidget(mLeName);
            pLayout->removeWidget(mTxTaken);
            pLayout->addWidget(mLeName, 0, 1);
            pLayout->addWidget(mTxTaken, 1, 1);

            pLayout->removeWidget(mLbThumbnail);
            pLayout->addWidget(mLbThumbnail, 0, 2, 2, 1);
            mLbThumbnail->setHidden(false);
        }
    }
    else
    {
        /* Clear snapshot timestamp info: */
        mTxTaken->clear();

        // TODO: Check whether layout manipulations are really
        //       necessary, they looks a bit dangerous to me..
        QGridLayout *pLayout = qobject_cast<QGridLayout*>(layout());
        AssertPtrReturnVoid(pLayout);
        {
            pLayout->removeWidget(mLbThumbnail);
            mLbThumbnail->setHidden(true);

            pLayout->removeWidget(mLeName);
            pLayout->removeWidget(mTxTaken);
            pLayout->addWidget(mLeName, 0, 1, 1, 2);
            pLayout->addWidget(mTxTaken, 1, 1, 1, 2);
        }
    }

    /* Retranslate: */
    retranslateUi();
}

void UISnapshotDetailsWidget::notify()
{
//    if (m_oldData != m_newData)
//        printf("Snapshot: %s, %s\n",
//               m_newData.m_strName.toUtf8().constData(),
//               m_newData.m_strDescription.toUtf8().constData());

    emit sigDataChanged(m_oldData != m_newData);
}

#include "VBoxSnapshotDetailsDlg.moc"

