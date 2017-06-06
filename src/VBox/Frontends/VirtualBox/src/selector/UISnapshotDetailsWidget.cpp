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
# include <QHBoxLayout>
# include <QDateTime>
# include <QDir>
# include <QGridLayout>
# include <QLabel>
# include <QLineEdit>
# include <QPushButton>
# include <QScrollArea>
# include <QStackedLayout>
# include <QTabWidget>
# include <QTextEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIConverter.h"
# include "UIDesktopWidgetWatchdog.h"
# include "UIExtraDataDefs.h"
# include "UIIconPool.h"
# include "UISnapshotDetailsWidget.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"

/* COM includes: */
# include "CAudioAdapter.h"
# include "CMachine.h"
# include "CMedium.h"
# include "CMediumAttachment.h"
# include "CNetworkAdapter.h"
# ifdef VBOX_WITH_PARALLEL_PORTS
#  include "CParallelPort.h"
# endif
# include "CSerialPort.h"
# include "CSharedFolder.h"
# include "CStorageController.h"
# include "CSystemProperties.h"
# include "CUSBController.h"
# include "CUSBDeviceFilter.h"
# include "CUSBDeviceFilters.h"
# include "CVRDEServer.h"

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

    /** Adjusts window size. */
    void adjustWindowSize();

    /** Adjusts picture. */
    void adjustPicture();

    /** Holds whether this widget was polished. */
    bool  m_fPolished;

    /** Holds the screenshot to show. */
    QPixmap  m_pixmapScreenshot;
    /** Holds the snapshot name. */
    QString  m_strSnapshotName;
    /** Holds the machine name. */
    QString  m_strMachineName;

    /** Holds the scroll-area instance. */
    QScrollArea *m_pScrollArea;
    /** Holds the picture label instance. */
    QLabel      *m_pLabelPicture;

    /** Holds whether we are in zoom mode. */
    bool  m_fZoomMode;
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

    /* Adjust the windiow size: */
    adjustWindowSize();
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

    /* Apply language settings: */
    retranslateUi();

    /* Adjust window size: */
    adjustWindowSize();

    /* Center according requested widget: */
    VBoxGlobal::centerWidget(this, parentWidget(), false);
}

void UIScreenshotViewer::adjustWindowSize()
{
    /* Acquire current host-screen size, fallback to 1024x768 if failed: */
    QSize screenSize = gpDesktop->screenGeometry(parentWidget()).size();
    if (!screenSize.isValid())
        screenSize = QSize(1024, 768);
    const int iInitWidth = screenSize.width() * .50 /* 50% of host-screen width */;

    /* Calculate screenshot aspect-ratio: */
    const double dAspectRatio = (double)m_pixmapScreenshot.height() / m_pixmapScreenshot.width();

    /* Calculate maximum window size: */
    const QSize maxSize = m_fZoomMode
                        ? screenSize * .9 /* 90% of host-screen size */ +
                          QSize(m_pScrollArea->frameWidth() * 2, m_pScrollArea->frameWidth() * 2)
                        : m_pixmapScreenshot.size() /* just the screenshot size */ +
                          QSize(m_pScrollArea->frameWidth() * 2, m_pScrollArea->frameWidth() * 2);

    /* Calculate initial window size: */
    const QSize initSize = QSize(iInitWidth, (int)(iInitWidth * dAspectRatio)).boundedTo(maxSize);

    /* Apply maximum window size restrictions: */
    setMaximumSize(maxSize);
    /* Apply initial window size: */
    resize(initSize);
}

void UIScreenshotViewer::adjustPicture()
{
    if (m_fZoomMode)
    {
        /* Adjust visual aspects: */
        m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pLabelPicture->setPixmap(m_pixmapScreenshot.scaled(m_pScrollArea->viewport()->size(),
                                                             Qt::IgnoreAspectRatio,
                                                             Qt::SmoothTransformation));
        m_pLabelPicture->setToolTip(tr("Click to view non-scaled screenshot."));
    }
    else
    {
        /* Adjust visual aspects: */
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
    , m_pStackedLayout(0)
    , m_pEmptyWidget(0)
    , m_pEmptyWidgetLabel(0)
    , m_pTabWidget(0)
    , m_pLayoutOptions(0)
    , m_pLabelTaken(0), m_pLabelTakenText(0)
    , m_pLabelName(0), m_pEditorName(0)
    , m_pLabelThumbnail(0)
    , m_pLabelDescription(0), m_pBrowserDescription(0)
    , m_pLayoutDetails(0)
    , m_pBrowserDetails(0)
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
    AssertReturn(pObject == m_pLabelThumbnail, false);

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
    /* Translate labels: */
    m_pEmptyWidgetLabel->setText("<p>You have the <b>Current State</b> item selected.<br>"
                                 "Press <b>Take</b> button if you wish to take a new snapshot.</p>");
    m_pTabWidget->setTabText(0, tr("&Attributes"));
    m_pTabWidget->setTabText(1, tr("D&etails"));
    m_pLabelName->setText(tr("&Name:"));
    m_pLabelTaken->setText(tr("Taken:"));
    m_pLabelDescription->setText(tr("&Description:"));

    /* And if snapshot is valid: */
    if (!m_comSnapshot.isNull())
    {
        /* Get the machine: */
        const CMachine comMachine = m_comSnapshot.GetMachine();

        /* Translate the picture tool-tip: */
        m_pLabelThumbnail->setToolTip(m_pixmapScreenshot.isNull() ? QString() : tr("Click to enlarge the screenshot."));

        /* Rebuild the details report: */
        m_pBrowserDetails->setText(detailsReport(comMachine));
    }
    else
    {
        /* Clear the picture tool-tip: */
        m_pLabelThumbnail->setToolTip(QString());

        /* Clear the details report: */
        // WORKAROUND:
        // How stupid Qt *is* to wipe out registered icons not just on clear()
        // call but on setText(QString()) and even setText("") as well.
        // Nice way to oversmart itself..
        m_pBrowserDetails->setText("<empty>");
    }
}

void UISnapshotDetailsWidget::sltHandleNameChange()
{
    m_newData.m_strName = m_pEditorName->text();
    // TODO: Validate
    //revalidate(m_pErrorPaneName);
    notify();
}

void UISnapshotDetailsWidget::sltHandleDescriptionChange()
{
    m_newData.m_strDescription = m_pBrowserDescription->toPlainText();
    // TODO: Validate
    //revalidate(m_pErrorPaneName);
    notify();
}

void UISnapshotDetailsWidget::prepare()
{
    /* Create stacked layout: */
    m_pStackedLayout = new QStackedLayout(this);
    AssertPtrReturnVoid(m_pStackedLayout);
    {
        /* Prepare empty-widget: */
        prepareEmptyWidget();
        /* Prepare tab-widget: */
        prepareTabWidget();
    }
}

void UISnapshotDetailsWidget::prepareEmptyWidget()
{
    /* Create empty-widget: */
    m_pEmptyWidget = new QWidget;
    AssertPtrReturnVoid(m_pEmptyWidget);
    {
        /* Create empty-widget layout: */
        new QVBoxLayout(m_pEmptyWidget);
        AssertPtrReturnVoid(m_pEmptyWidget->layout());
        {
            /* Create empty-widget label: */
            m_pEmptyWidgetLabel = new QLabel;
            {
                /* Configure label: */
                QFont font = m_pEmptyWidgetLabel->font();
                font.setPointSize(font.pointSize() * 1.5);
                m_pEmptyWidgetLabel->setAlignment(Qt::AlignCenter);
                m_pEmptyWidgetLabel->setFont(font);

                /* Add into layout: */
                m_pEmptyWidget->layout()->addWidget(m_pEmptyWidgetLabel);
            }
        }

        /* Add into layout: */
        m_pStackedLayout->addWidget(m_pEmptyWidget);
    }
}

void UISnapshotDetailsWidget::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QTabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare 'Options' tab: */
        prepareTabOptions();
        /* Prepare 'Details' tab: */
        prepareTabDetails();

        /* Add into layout: */
        m_pStackedLayout->addWidget(m_pTabWidget);
    }
}

void UISnapshotDetailsWidget::prepareTabOptions()
{
    /* Create widget itself: */
    QWidget *pWidget = new QWidget;
    AssertPtrReturnVoid(pWidget);
    {
        /* Create 'Options' layout: */
        m_pLayoutOptions = new QGridLayout(pWidget);
        AssertPtrReturnVoid(m_pLayoutOptions);
        {
            /* Create taken label: */
            m_pLabelTaken = new QLabel;
            AssertPtrReturnVoid(m_pLabelTaken);
            {
                /* Configure label: */
                m_pLabelTaken->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelTaken, 0, 0);
            }
            /* Create taken text: */
            m_pLabelTakenText = new QLabel;
            AssertPtrReturnVoid(m_pLabelTakenText);
            {
                /* Configure label: */
                QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
                policy.setHorizontalStretch(1);
                m_pLabelTakenText->setSizePolicy(policy);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelTakenText, 0, 1);
            }

            /* Create name label: */
            m_pLabelName = new QLabel;
            AssertPtrReturnVoid(m_pLabelName);
            {
                /* Configure label: */
                m_pLabelName->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelName, 1, 0);
            }
            /* Create name editor: */
            m_pEditorName = new QLineEdit;
            AssertPtrReturnVoid(m_pEditorName);
            {
                /* Configure editor: */
                m_pLabelName->setBuddy(m_pEditorName);
                QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
                policy.setHorizontalStretch(1);
                m_pEditorName->setSizePolicy(policy);
                connect(m_pEditorName, &QLineEdit::textChanged,
                        this, &UISnapshotDetailsWidget::sltHandleNameChange);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pEditorName, 1, 1);
            }

            /* Create thumbnail label: */
            m_pLabelThumbnail = new QLabel;
            AssertPtrReturnVoid(m_pLabelThumbnail);
            {
                /* Configure label: */
                m_pLabelThumbnail->installEventFilter(this);
                m_pLabelThumbnail->setFrameShape(QFrame::Panel);
                m_pLabelThumbnail->setFrameShadow(QFrame::Sunken);
                m_pLabelThumbnail->setCursor(Qt::PointingHandCursor);
                m_pLabelThumbnail->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
                m_pLabelThumbnail->setAutoFillBackground(true);
                QPalette pal = m_pLabelThumbnail->palette();
                pal.setColor(QPalette::Window, Qt::black);
                m_pLabelThumbnail->setPalette(pal);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelThumbnail, 0, 2, 2, 1);
            }

            /* Create description label: */
            m_pLabelDescription = new QLabel;
            AssertPtrReturnVoid(m_pLabelDescription);
            {
                /* Configure label: */
                m_pLabelDescription->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignTop);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelDescription, 2, 0);
            }
            /* Create description browser: */
            m_pBrowserDescription = new QTextEdit;
            AssertPtrReturnVoid(m_pBrowserDescription);
            {
                /* Configure browser: */
                m_pLabelDescription->setBuddy(m_pBrowserDescription);
                m_pBrowserDescription->setTabChangesFocus(true);
                m_pBrowserDescription->setAcceptRichText(false);
                QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                policy.setHorizontalStretch(1);
                m_pBrowserDescription->setSizePolicy(policy);
                connect(m_pBrowserDescription, &QTextEdit::textChanged,
                        this, &UISnapshotDetailsWidget::sltHandleDescriptionChange);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pBrowserDescription, 2, 1, 1, 2);
            }
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(pWidget, QString());
    }
}

void UISnapshotDetailsWidget::prepareTabDetails()
{
    /* Create widget itself: */
    QWidget *pWidget = new QWidget;
    AssertPtrReturnVoid(pWidget);
    {
        /* Create 'Details' layout: */
        m_pLayoutDetails = new QVBoxLayout(pWidget);
        AssertPtrReturnVoid(m_pLayoutDetails);
        {
            /* Create details browser: */
            m_pBrowserDetails = new QTextEdit;
            AssertPtrReturnVoid(m_pBrowserDetails);
            {
                /* Configure browser: */
                m_pBrowserDetails->setReadOnly(true);
                m_pBrowserDetails->setFrameShadow(QFrame::Plain);
                m_pBrowserDetails->setFrameShape(QFrame::NoFrame);
                m_pBrowserDetails->viewport()->setAutoFillBackground(false);
                QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                policy.setHorizontalStretch(1);
                m_pBrowserDetails->setSizePolicy(policy);
                m_pBrowserDetails->setFocus();
                /* Determine icon metric: */
                const QStyle *pStyle = QApplication::style();
                const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
                const QSize iconSize = QSize(iIconMetric, iIconMetric);
                /* Register DetailsElementType icons in the m_pBrowserDetails document: */
                // WORKAROUND:
                // Unfortunatelly we can't just enumerate types within
                // cycle since they have exceptions like 'parallel' case.
                QList<DetailsElementType> types;
                types << DetailsElementType_General
                      << DetailsElementType_System
                      << DetailsElementType_Preview
                      << DetailsElementType_Display
                      << DetailsElementType_Storage
                      << DetailsElementType_Audio
                      << DetailsElementType_Network
                      << DetailsElementType_Serial
#ifdef VBOX_WITH_PARALLEL_PORTS
                      << DetailsElementType_Parallel
#endif
                      << DetailsElementType_USB
                      << DetailsElementType_SF
                      << DetailsElementType_UI
                      << DetailsElementType_Description;
                foreach (const DetailsElementType &enmType, types)
                    m_pBrowserDetails->document()->addResource(
                        QTextDocument::ImageResource,
                        QUrl(QString("details://%1").arg(gpConverter->toInternalString(enmType))),
                        QVariant(gpConverter->toIcon(enmType).pixmap(iconSize)));

                /* Add into layout: */
                m_pLayoutDetails->addWidget(m_pBrowserDetails);
            }
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(pWidget, QString());
    }
}

void UISnapshotDetailsWidget::loadSnapshotData()
{
    /* Read general snapshot properties: */
    m_pEditorName->setText(m_newData.m_strName);
    m_pBrowserDescription->setText(m_newData.m_strDescription);

    /* If there is a snapshot: */
    if (m_comSnapshot.isNotNull())
    {
        /* Choose the tab-widget as current one: */
        m_pStackedLayout->setCurrentWidget(m_pTabWidget);

        /* Calculate snapshot timestamp info: */
        QDateTime timestamp;
        timestamp.setTime_t(m_comSnapshot.GetTimeStamp() / 1000);
        bool fDateTimeToday = timestamp.date() == QDate::currentDate();
        QString dateTime = fDateTimeToday ? timestamp.time().toString(Qt::LocalDate) : timestamp.toString(Qt::LocalDate);
        m_pLabelTakenText->setText(dateTime);

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
        if (m_pixmapThumbnail.isNull())
        {
            m_pLabelThumbnail->setPixmap(QPixmap());

            m_pLayoutOptions->removeWidget(m_pLabelThumbnail);
            m_pLabelThumbnail->setHidden(true);

            m_pLayoutOptions->removeWidget(m_pLabelTakenText);
            m_pLayoutOptions->removeWidget(m_pEditorName);
            m_pLayoutOptions->addWidget(m_pLabelTakenText, 0, 1, 1, 2);
            m_pLayoutOptions->addWidget(m_pEditorName, 1, 1, 1, 2);
        }
        else
        {
            const QStyle *pStyle = QApplication::style();
            const int iIconMetric = pStyle->pixelMetric(QStyle::PM_LargeIconSize);
            m_pLabelThumbnail->setPixmap(m_pixmapThumbnail.scaled(QSize(1, iIconMetric),
                                                                  Qt::KeepAspectRatioByExpanding,
                                                                  Qt::SmoothTransformation));

            m_pLayoutOptions->removeWidget(m_pLabelTakenText);
            m_pLayoutOptions->removeWidget(m_pEditorName);
            m_pLayoutOptions->addWidget(m_pLabelTakenText, 0, 1);
            m_pLayoutOptions->addWidget(m_pEditorName, 1, 1);

            m_pLayoutOptions->removeWidget(m_pLabelThumbnail);
            m_pLayoutOptions->addWidget(m_pLabelThumbnail, 0, 2, 2, 1);
            m_pLabelThumbnail->setHidden(false);
        }
    }
    else
    {
        /* Choose the empty-widget as current one: */
        m_pStackedLayout->setCurrentWidget(m_pEmptyWidget);

        /* Clear snapshot timestamp info: */
        m_pLabelTakenText->clear();

        // TODO: Check whether layout manipulations are really
        //       necessary, they looks a bit dangerous to me..
        {
            m_pLabelThumbnail->setPixmap(QPixmap());

            m_pLayoutOptions->removeWidget(m_pLabelThumbnail);
            m_pLabelThumbnail->setHidden(true);

            m_pLayoutOptions->removeWidget(m_pEditorName);
            m_pLayoutOptions->removeWidget(m_pLabelTakenText);
            m_pLayoutOptions->addWidget(m_pLabelTakenText, 0, 1, 1, 2);
            m_pLayoutOptions->addWidget(m_pEditorName, 1, 1, 1, 2);
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

QString UISnapshotDetailsWidget::detailsReport(const CMachine &comMachine)
{
    /* Details templates: */
    static const char *sTableTpl =
        "<table border=0 cellspacing=1 cellpadding=0>%1</table>";
    static const char *sSectionBoldTpl =
        "<tr><td width=%6 rowspan=%1 align=left><img src='%2'></td>"
            "<td colspan=3><!-- %3 --><b><nobr>%4</nobr></b></td></tr>"
            "%5"
        "<tr><td colspan=3><font size=1>&nbsp;</font></td></tr>";
    static const char *sSectionItemTpl1 =
        "<tr><td width=40%><nobr><i>%1</i></nobr></td><td/><td/></tr>";
    static const char *sSectionItemTpl2 =
        "<tr><td width=40%><nobr>%1:</nobr></td><td/><td>%2</td></tr>";
    static const char *sSectionItemTpl3 =
        "<tr><td width=40%><nobr>%1</nobr></td><td/><td/></tr>";

    /* Use the const ref on the basis of implicit QString constructor: */
    const QString &strSectionTpl = sSectionBoldTpl;

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);
    const int iIndentMetric = iIconMetric * 1.375;

    /* Compose details report: */
    QString strReport;

    /* General: */
    {
        /* Name, OS Type: */
        int iRowCount = 2;
        QString strItem = QString(sSectionItemTpl2).arg(tr("Name", "details report"),
                                                        comMachine.GetName())
                        + QString(sSectionItemTpl2).arg(tr("OS Type", "details report"),
                                                        vboxGlobal().vmGuestOSTypeDescription(comMachine.GetOSTypeId()));

        /* Group(s)? */
        const QStringList &groups = comMachine.GetGroups().toList();
        if (   groups.size() > 1
            || (groups.size() > 0 && groups.at(0) != "/"))
        {
            ++iRowCount;
            strItem += QString(sSectionItemTpl2).arg(tr("Group(s)", "details report"),
                                                     groups.join(", "));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://general", /* icon */
                 "#general", /* link */
                 tr("General", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* System: */
    {
        /* Base Memory, Processor(s): */
        int iRowCount = 2;
        QString strItem = QString(sSectionItemTpl2).arg(tr("Base Memory", "details report"),
                                                        tr("<nobr>%1 MB</nobr>", "details report")
                                                           .arg(QString::number(comMachine.GetMemorySize())))
                        + QString(sSectionItemTpl2).arg(tr("Processor(s)", "details report"),
                                                        tr("<nobr>%1</nobr>", "details report")
                                                           .arg(QString::number(comMachine.GetCPUCount())));

        /* Execution Cap? */
        ULONG uExecutionCap = comMachine.GetCPUExecutionCap();
        if (uExecutionCap < 100)
        {
            ++iRowCount;
            strItem += QString(sSectionItemTpl2).arg(tr("Execution Cap", "details report"),
                                                     tr("<nobr>%1%</nobr>", "details report")
                                                        .arg(comMachine.GetCPUExecutionCap()));
        }

        /* Boot Order: */
        ++iRowCount;
        QStringList bootOrder;
        for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++i)
        {
            const KDeviceType enmDevice = comMachine.GetBootOrder(i);
            if (enmDevice != KDeviceType_Null)
                bootOrder << gpConverter->toString(enmDevice);
        }
        if (bootOrder.isEmpty())
            bootOrder << gpConverter->toString(KDeviceType_Null);
        strItem += QString(sSectionItemTpl2).arg(tr("Boot Order", "details report"),
                                                 bootOrder.join(", "));

#ifdef VBOX_WITH_FULL_DETAILS_REPORT

        /* Acquire BIOS Settings: */
        const CBIOSSettings &comBiosSettings = comMachine.GetBIOSSettings();

        /* ACPI: */
        ++iRowCount;
        const QString strAcpi = comBiosSettings.GetACPIEnabled()
                              ? tr("Enabled", "details report (ACPI)")
                              : tr("Disabled", "details report (ACPI)");
        strItem += QString(sSectionItemTpl2).arg(tr("ACPI", "details report"), strAcpi);

        /* I/O APIC: */
        ++iRowCount;
        const QString strIoapic = comBiosSettings.GetIOAPICEnabled()
                                ? tr("Enabled", "details report (I/O APIC)")
                                : tr("Disabled", "details report (I/O APIC)");
        strItem += QString(sSectionItemTpl2).arg(tr("I/O APIC", "details report"), strIoapic);

        /* PAE/NX: */
        ++iRowCount;
        const QString strPae = comMachine.GetCPUProperty(KCPUPropertyType_PAE)
                             ? tr("Enabled", "details report (PAE/NX)")
                             : tr("Disabled", "details report (PAE/NX)");
        strItem += QString(sSectionItemTpl2).arg(tr("PAE/NX", "details report"), strPae);

#endif /* VBOX_WITH_FULL_DETAILS_REPORT */

        /* VT-x/AMD-V availability: */
        const bool fVTxAMDVSupported = vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx);
        if (fVTxAMDVSupported)
        {
            /* VT-x/AMD-V: */
            ++iRowCount;
            const QString strVirt = comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled)
                                  ? tr("Enabled", "details report (VT-x/AMD-V)")
                                  : tr("Disabled", "details report (VT-x/AMD-V)");
            strItem += QString(sSectionItemTpl2).arg(tr("VT-x/AMD-V", "details report"), strVirt);

            /* Nested Paging: */
            ++iRowCount;
            const QString strNested = comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging)
                                    ? tr("Enabled", "details report (Nested Paging)")
                                    : tr("Disabled", "details report (Nested Paging)");
            strItem += QString(sSectionItemTpl2).arg(tr("Nested Paging", "details report"), strNested);
        }

        /* Paravirtualization Interface: */
        ++iRowCount;
        const QString strParavirtProvider = gpConverter->toString(comMachine.GetParavirtProvider());
        strItem += QString(sSectionItemTpl2).arg(tr("Paravirtualization Interface", "details report"), strParavirtProvider);

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://system", /* icon */
                 "#system", /* link */
                 tr("System", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Display: */
    {
        /* Video Memory: */
        int iRowCount = 1;
        QString strItem = QString(sSectionItemTpl2).arg(tr("Video Memory", "details report"),
                                                        tr("<nobr>%1 MB</nobr>", "details report")
                                                           .arg(QString::number(comMachine.GetVRAMSize())));

        /* Screens? */
        const int cGuestScreens = comMachine.GetMonitorCount();
        if (cGuestScreens > 1)
        {
            ++iRowCount;
            strItem += QString(sSectionItemTpl2).arg(tr("Screens", "details report"),
                                                     QString::number(cGuestScreens));
        }

        /* 3D Acceleration: */
        ++iRowCount;
        QString strAcc3d = comMachine.GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable()
                         ? tr("Enabled", "details report (3D Acceleration)")
                         : tr("Disabled", "details report (3D Acceleration)");
        strItem += QString(sSectionItemTpl2).arg(tr("3D Acceleration", "details report"), strAcc3d);

#ifdef VBOX_WITH_VIDEOHWACCEL
        /* 2D Video Acceleration: */
        ++iRowCount;
        QString strAcc2dVideo = comMachine.GetAccelerate2DVideoEnabled()
                              ? tr("Enabled", "details report (2D Video Acceleration)")
                              : tr("Disabled", "details report (2D Video Acceleration)");
        strItem += QString(sSectionItemTpl2).arg(tr("2D Video Acceleration", "details report"), strAcc2dVideo);
#endif

        /* Remote Desktop Server: */
        const CVRDEServer comServer = comMachine.GetVRDEServer();
        if (!comServer.isNull())
        {
            ++iRowCount;
            if (comServer.GetEnabled())
                strItem += QString(sSectionItemTpl2).arg(tr("Remote Desktop Server Port", "details report (VRDE Server)"),
                                                         comServer.GetVRDEProperty("TCP/Ports"));
            else
                strItem += QString(sSectionItemTpl2).arg(tr("Remote Desktop Server", "details report (VRDE Server)"),
                                                         tr("Disabled", "details report (VRDE Server)"));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://display", /* icon */
                 "#display", /* link */
                 tr("Display", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Storage: */
    {
        /* Nothing: */
        int iRowCount = 0;
        QString strItem;

        /* Iterate over the all machine controllers: */
        foreach (const CStorageController &comController, comMachine.GetStorageControllers())
        {
            /* Add controller information: */
            ++iRowCount;
            const QString strControllerName = QApplication::translate("UIMachineSettingsStorage", "Controller: %1");
            strItem += QString(sSectionItemTpl3).arg(strControllerName.arg(comController.GetName()));

            /* Populate sorted map with attachments information: */
            QMap<StorageSlot,QString> attachmentsMap;
            foreach (const CMediumAttachment &comAttachment, comMachine.GetMediumAttachmentsOfController(comController.GetName()))
            {
                /* Prepare current storage slot: */
                const StorageSlot attachmentSlot(comController.GetBus(), comAttachment.GetPort(), comAttachment.GetDevice());
                // TODO: Fix that NLS bug one day..
                /* Append 'device slot name' with 'device type name' for optical devices only: */
                QString strDeviceType = comAttachment.GetType() == KDeviceType_DVD ? tr("(Optical Drive)") : QString();
                if (!strDeviceType.isNull())
                    strDeviceType.prepend(' ');
                /* Prepare current medium object: */
                const CMedium &medium = comAttachment.GetMedium();
                /* Prepare information about current medium & attachment: */
                QString strAttachmentInfo = !comAttachment.isOk()
                                          ? QString()
                                          : QString(sSectionItemTpl2)
                                            .arg(QString("&nbsp;&nbsp;") +
                                                 gpConverter->toString(StorageSlot(comController.GetBus(),
                                                                                   comAttachment.GetPort(),
                                                                                   comAttachment.GetDevice())) + strDeviceType)
                                            .arg(vboxGlobal().details(medium, false));
                /* Insert that attachment into map: */
                if (!strAttachmentInfo.isNull())
                    attachmentsMap.insert(attachmentSlot, strAttachmentInfo);
            }

            /* Iterate over the sorted map with attachments information: */
            QMapIterator<StorageSlot, QString> it(attachmentsMap);
            while (it.hasNext())
            {
                /* Add controller information: */
                it.next();
                ++iRowCount;
                strItem += it.value();
            }
        }

        /* Handle side-case: */
        if (strItem.isNull())
        {
            ++iRowCount;
            strItem = QString(sSectionItemTpl1).arg(tr("Not Attached", "details report (Storage)"));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://storage", /* icon */
                 "#storage", /* link */
                 tr("Storage", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Audio: */
    {
        /* Nothing: */
        int iRowCount = 0;
        QString strItem;

        /* Host Driver, Controller? */
        const CAudioAdapter &comAudio = comMachine.GetAudioAdapter();
        if (comAudio.GetEnabled())
        {
            iRowCount += 2;
            strItem = QString(sSectionItemTpl2).arg(tr("Host Driver", "details report (audio)"),
                                                    gpConverter->toString(comAudio.GetAudioDriver()))
                    + QString(sSectionItemTpl2).arg(tr("Controller", "details report (audio)"),
                                                    gpConverter->toString(comAudio.GetAudioController()));
        }
        else
        {
            ++iRowCount;
            strItem = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (audio)"));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://audio", /* icon */
                 "#audio", /* link */
                 tr("Audio", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Network: */
    {
        /* Nothing: */
        int iRowCount = 0;
        QString strItem;

        /* Enumerate all the network adapters (up to acquired/limited count): */
        const ulong iCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(comMachine.GetChipsetType());
        for (ulong iSlot = 0; iSlot < iCount; ++iSlot)
        {
            /* Get current network adapter: */
            const CNetworkAdapter &comNetwork = comMachine.GetNetworkAdapter(iSlot);
            if (comNetwork.GetEnabled())
            {
                /* Determine attachment type: */
                const KNetworkAttachmentType enmType = comNetwork.GetAttachmentType();
                QString attType = gpConverter->toString(comNetwork.GetAdapterType())
                                  .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                /* Don't use the adapter type string for types that have
                 * an additional symbolic network/interface name field,
                 * use this name instead: */
                switch (enmType)
                {
                    case KNetworkAttachmentType_Bridged:
                        attType = attType.arg(tr("Bridged adapter, %1", "details report (network)")
                                                 .arg(comNetwork.GetBridgedInterface()));
                        break;
                    case KNetworkAttachmentType_Internal:
                        attType = attType.arg(tr("Internal network, '%1'", "details report (network)")
                                                 .arg(comNetwork.GetInternalNetwork()));
                        break;
                    case KNetworkAttachmentType_HostOnly:
                        attType = attType.arg(tr("Host-only adapter, '%1'", "details report (network)")
                                                 .arg(comNetwork.GetHostOnlyInterface()));
                        break;
                    case KNetworkAttachmentType_Generic:
                        attType = attType.arg(tr("Generic, '%1'", "details report (network)")
                                                 .arg(comNetwork.GetGenericDriver()));
                        break;
                    case KNetworkAttachmentType_NATNetwork:
                        attType = attType.arg(tr("NAT network, '%1'", "details report (network)")
                                                 .arg(comNetwork.GetNATNetwork()));
                        break;
                    default:
                        attType = attType.arg(gpConverter->toString(enmType));
                        break;
                }
                /* Here goes the record: */
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(tr("Adapter %1", "details report (network)")
                                                            .arg(comNetwork.GetSlot() + 1),
                                                         attType);
            }
        }

        /* Handle side-case: */
        if (strItem.isNull())
        {
            ++iRowCount;
            strItem = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (network)"));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://network", /* icon */
                 "#network", /* link */
                 tr("Network", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Serial Ports: */
    {
        /* Nothing: */
        int iRowCount = 0;
        QString strItem;

        /* Enumerate all the serial ports (up to acquired/limited count): */
        const ulong iCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
        for (ulong iSlot = 0; iSlot < iCount; ++iSlot)
        {
            /* Get current serial port: */
            const CSerialPort &comSerial = comMachine.GetSerialPort(iSlot);
            if (comSerial.GetEnabled())
            {
                /* Determine port mode: */
                const KPortMode enmMode = comSerial.GetHostMode();
                /* Compose the data: */
                QString strData = vboxGlobal().toCOMPortName(comSerial.GetIRQ(), comSerial.GetIOBase()) + ", ";
                if (   enmMode == KPortMode_HostPipe
                    || enmMode == KPortMode_HostDevice
                    || enmMode == KPortMode_TCP
                    || enmMode == KPortMode_RawFile)
                    strData += QString("%1 (<nobr>%2</nobr>)").arg(gpConverter->toString(enmMode))
                                                              .arg(QDir::toNativeSeparators(comSerial.GetPath()));
                else
                    strData += gpConverter->toString(enmMode);
                /* Here goes the record: */
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(tr("Port %1", "details report (serial ports)")
                                                            .arg(comSerial.GetSlot() + 1),
                                                         strData);
            }
        }

        /* Handle side-case: */
        if (strItem.isNull())
        {
            ++iRowCount;
            strItem = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (serial ports)"));
        }

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://serialPorts", /* icon */
                 "#serialPorts", /* link */
                 tr("Serial Ports", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

#ifdef VBOX_WITH_PARALLEL_PORTS
    /* Parallel Ports: */
    {
        /* Nothing: */
        int iRowCount = 0;
        QString strItem;

        /* Enumerate all the serial ports (up to acquired/limited count): */
        const ulong iCount = vboxGlobal().virtualBox().GetSystemProperties().GetParallelPortCount();
        for (ulong iSlot = 0; iSlot < iCount; ++iSlot)
        {
            /* Get current parallel port: */
            const CParallelPort &comParallel = comMachine.GetParallelPort(iSlot);
            if (comParallel.GetEnabled())
            {
                /* Compose the data: */
                QString strData = toLPTPortName(comParallel.GetIRQ(), comParallel.GetIOBase())
                                + QString(" (<nobr>%1</nobr>)").arg(QDir::toNativeSeparators(comParallel.GetPath()));
                /* Here goes the record: */
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(tr("Port %1", "details report (parallel ports)")
                                                            .arg(comParallel.GetSlot() + 1),
                                                         strData);
            }
        }

        /* Handle side-case: */
        if (strItem.isNull())
        {
            ++iRowCount;
            strItem = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (parallel ports)"));
        }

        /* Temporary disabled: */
        const QString dummy = strSectionTpl /* strReport += strSectionTpl */
            .arg(2 + iRowCount) /* rows */
            .arg("details://parallelPorts", /* icon */
                 "#parallelPorts", /* link */
                 tr("Parallel Ports", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
        Q_UNUSED(dummy);
    }
#endif /* VBOX_WITH_PARALLEL_PORTS */

    /* USB */
    {
        /* Acquire USB filters object: */
        const CUSBDeviceFilters &comFilters = comMachine.GetUSBDeviceFilters();
        if (   !comFilters.isNull()
            && comMachine.GetUSBProxyAvailable())
        {
            /* Device Filters: */
            int iRowCount = 1;
            QString strItem;

            /* The USB controller may be unavailable (i.e. in VirtualBox OSE): */
            if (!comMachine.GetUSBControllers().isEmpty())
            {
                /* Acquire USB filters: */
                const CUSBDeviceFilterVector &filterVector = comFilters.GetDeviceFilters();
                /* Calculate the amount of active filters: */
                uint cActive = 0;
                foreach (const CUSBDeviceFilter &comFilter, filterVector)
                    if (comFilter.GetActive())
                        ++cActive;
                /* Here goes the record: */
                strItem = QString(sSectionItemTpl2).arg(tr("Device Filters", "details report (USB)"),
                                                        tr("%1 (%2 active)", "details report (USB)")
                                                           .arg(filterVector.size()).arg(cActive));
            }

            /* Handle side-case: */
            if (strItem.isNull())
                strItem = QString(sSectionItemTpl1).arg(tr("Disabled", "details report (USB)"));

            /* Append report: */
            strReport += strSectionTpl
                .arg(2 + iRowCount) /* rows */
                .arg("details://usb", /* icon */
                     "#usb", /* link */
                     tr("USB", "details report"), /* title */
                     strItem, /* items */
                     QString::number(iIndentMetric));
        }
    }

    /* Shared Folders */
    {
        /* Shared Folders: */
        int iRowCount = 1;
        QString strItem;

        /* Acquire shared folders count: */
        const ulong iCount = comMachine.GetSharedFolders().size();
        if (iCount > 0)
            strItem = QString(sSectionItemTpl2).arg(tr("Shared Folders", "details report (shared folders)"),
                                                    QString::number(iCount));
        else
            strItem = QString(sSectionItemTpl1).arg(tr("None", "details report (shared folders)"));

        /* Append report: */
        strReport += strSectionTpl
            .arg(2 + iRowCount) /* rows */
            .arg("details://sharedFolders", /* icon */
                 "#sfolders", /* link */
                 tr("Shared Folders", "details report"), /* title */
                 strItem, /* items */
                 QString::number(iIndentMetric));
    }

    /* Compose full report: */
    return QString(sTableTpl).arg(strReport);
}

#include "UISnapshotDetailsWidget.moc"

