/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UINetworkManager stuff implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QWidget>
#include <QTimer>
#include <QGridLayout>
#include <QProgressBar>
#include <QTextEdit>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QKeyEvent>
#include <QNetworkReply>

/* Local includes: */
#include "UINetworkManager.h"
#include "UINetworkCustomer.h"
#include "QIWithRetranslateUI.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UIIconPool.h"
#include "QIDialogButtonBox.h"
#include "UIPopupBox.h"
#include "QIToolButton.h"

/* Network-request widget: */
class UINetworkRequestWidget : public QIWithRetranslateUI<UIPopupBox>
{
    Q_OBJECT;

signals:

    /* Signal to retry network-request: */
    void sigRetry();
    /* Signal to cancel network-request: */
    void sigCancel();

public:

    /* Constructor: */
    UINetworkRequestWidget(QWidget *pParent, UINetworkRequest *pNetworkRequest)
        : QIWithRetranslateUI<UIPopupBox>(pParent)
        , m_pNetworkRequest(pNetworkRequest)
        , m_pTimer(new QTimer(this))
        , m_pContentWidget(new QWidget(this))
        , m_pMainLayout(new QGridLayout(m_pContentWidget))
        , m_pProgressBar(new QProgressBar(m_pContentWidget))
        , m_pRetryButton(new QIToolButton(m_pContentWidget))
        , m_pCancelButton(new QIToolButton(m_pContentWidget))
        , m_pErrorPane(new QTextEdit(m_pContentWidget))
    {
        /* Setup self: */
        setTitleIcon(UIIconPool::iconSet(":/nw_16px.png"));
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setContentWidget(m_pContentWidget);
        setOpen(true);

        /* Prepare listeners for m_pNetworkRequest: */
        connect(m_pNetworkRequest, SIGNAL(sigProgress(qint64, qint64)), this, SLOT(sltSetProgress(qint64, qint64)));
        connect(m_pNetworkRequest, SIGNAL(sigStarted()), this, SLOT(sltSetProgressToStarted()));
        connect(m_pNetworkRequest, SIGNAL(sigFinished()), this, SLOT(sltSetProgressToFinished()));
        connect(m_pNetworkRequest, SIGNAL(sigFailed(const QString&)), this, SLOT(sltSetProgressToFailed(const QString&)));

        /* Setup timer: */
        m_pTimer->setInterval(5000);
        connect(m_pTimer, SIGNAL(timeout()), this, SLOT(sltTimeIsOut()));

        /* Setup progress-bar: */
        m_pProgressBar->setRange(0, 0);
        m_pProgressBar->setMaximumHeight(16);

        /* Setup retry-button: */
        m_pRetryButton->setHidden(true);
        m_pRetryButton->removeBorder();
        m_pRetryButton->setFocusPolicy(Qt::NoFocus);
        m_pRetryButton->setIcon(UIIconPool::iconSet(":/refresh_16px.png"));
        connect(m_pRetryButton, SIGNAL(clicked(bool)), this, SIGNAL(sigRetry()));

        /* Setup cancel-button: */
        m_pCancelButton->removeBorder();
        m_pCancelButton->setFocusPolicy(Qt::NoFocus);
        m_pCancelButton->setIcon(UIIconPool::iconSet(":/delete_16px.png"));
        connect(m_pCancelButton, SIGNAL(clicked(bool)), this, SIGNAL(sigCancel()));

        /* Setup error-label: */
        m_pErrorPane->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        m_pErrorPane->viewport()->setAutoFillBackground(false);
        m_pErrorPane->setFrameShape(QFrame::NoFrame);
        m_pErrorPane->setReadOnly(true);

        /* Layout content: */
        m_pMainLayout->addWidget(m_pProgressBar, 0, 0);
        m_pMainLayout->addWidget(m_pRetryButton, 0, 1);
        m_pMainLayout->addWidget(m_pCancelButton, 0, 2);
        m_pMainLayout->addWidget(m_pErrorPane, 1, 0, 1, 3);

        /* Retranslate UI: */
        retranslateUi();
    }

private slots:

    /* Retranslate UI: */
    void retranslateUi()
    {
        /* Get corresponding title: */
        const QString &strTitle = m_pNetworkRequest->description();

        /* Set popup title (default if missed): */
        setTitle(strTitle.isEmpty() ? UINetworkManager::tr("Network Operation") : strTitle);

        /* Translate retry button: */
        m_pRetryButton->setStatusTip(UINetworkManager::tr("Restart network operation"));

        /* Translate cancel button: */
        m_pCancelButton->setStatusTip(UINetworkManager::tr("Cancel network operation"));
    }

    /* Updates current network-request progess: */
    void sltSetProgress(qint64 iReceived, qint64 iTotal)
    {
        /* Restart timer: */
        m_pTimer->start();

        /* Set current progress to passed: */
        m_pProgressBar->setRange(0, iTotal);
        m_pProgressBar->setValue(iReceived);
    }

    /* Set current network-request progress to 'started': */
    void sltSetProgressToStarted()
    {
        /* Start timer: */
        m_pTimer->start();

        /* Set current progress to 'started': */
        m_pProgressBar->setRange(0, 1);
        m_pProgressBar->setValue(0);

        /* Hide 'retry' button: */
        m_pRetryButton->setHidden(true);

        /* Hide error label: */
        m_pErrorPane->setPlainText(QString());
        m_pErrorPane->setMinimumHeight(0);
    }

    /* Set current network-request progress to 'finished': */
    void sltSetProgressToFinished()
    {
        /* Stop timer: */
        m_pTimer->stop();

        /* Set current progress to 'started': */
        m_pProgressBar->setRange(0, 1);
        m_pProgressBar->setValue(1);
    }

    /* Set current network-request progress to 'failed': */
    void sltSetProgressToFailed(const QString &strError)
    {
        /* Stop timer: */
        m_pTimer->stop();

        /* Set current progress to 'failed': */
        m_pProgressBar->setRange(0, 1);
        m_pProgressBar->setValue(1);

        /* Show 'retry' button: */
        m_pRetryButton->setHidden(false);

        /* Try to find all the links in the error-message,
         * replace them with %increment if present: */
        QString strErrorText(strError);
        QRegExp linkRegExp("[\\S]+[\\./][\\S]+");
        QStringList links;
        for (int i = 1; linkRegExp.indexIn(strErrorText) != -1; ++i)
        {
            links << linkRegExp.cap();
            strErrorText.replace(linkRegExp.cap(), QString("%%1").arg(i));
        }
        /* Return back all the links, just in bold: */
        if (!links.isEmpty())
            for (int i = 0; i < links.size(); ++i)
                strErrorText = strErrorText.arg(QString("<b>%1</b>").arg(links[i]));

        /* Show error label: */
        m_pErrorPane->setText(UINetworkManager::tr("Error: %1.").arg(strErrorText));
        m_pErrorPane->setMinimumHeight(m_pErrorPane->document()->size().toSize().height());
    }

    /* Handle frozen progress: */
    void sltTimeIsOut()
    {
        /* Stop timer: */
        m_pTimer->stop();

        /* Set current progress to unknown: */
        m_pProgressBar->setRange(0, 0);
    }

private:

    /* Objects: */
    UINetworkRequest *m_pNetworkRequest;
    QTimer *m_pTimer;

    /* Widgets: */
    QWidget *m_pContentWidget;
    QGridLayout *m_pMainLayout;
    QProgressBar *m_pProgressBar;
    QIToolButton *m_pRetryButton;
    QIToolButton *m_pCancelButton;
    QTextEdit *m_pErrorPane;
};

/* Network requests dialog: */
class UINetworkManagerDialog : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

signals:

    /* Signal to cancel all network-requests: */
    void sigCancelNetworkRequests();

public slots:

    /* Show the dialog, make sure its visible: */
    void showNormal()
    {
        /* Show (restore if neessary): */
        QMainWindow::showNormal();

        /* Raise above the others: */
        raise();

        /* Activate: */
        activateWindow();
    }

public:

    /* Constructor: */
    UINetworkManagerDialog(QWidget *pParent = 0)
        : QIWithRetranslateUI<QMainWindow>(pParent)
    {
        /* Do not count that window as important for application,
         * it will NOT be taken into account when other top-level windows will be closed: */
        setAttribute(Qt::WA_QuitOnClose, false);

        /* Set minimum width: */
        setMinimumWidth(500);

        /* Prepare central-widget: */
        setCentralWidget(new QWidget);

        /* Create main-layout: */
        m_pMainLayout = new QVBoxLayout(centralWidget());
        m_pMainLayout->setMargin(6);

        /* Create description-label: */
        m_pLabel = new QLabel(centralWidget());
        m_pLabel->setAlignment(Qt::AlignCenter);
        m_pLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

        /* Create button-box: */
        m_pButtonBox = new QIDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, centralWidget());
        connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(sltHandleCancelAllButtonPress()));
        m_pButtonBox->setHidden(true);

        /* Layout content: */
        m_pMainLayout->addWidget(m_pLabel);
        m_pMainLayout->addStretch();
        m_pMainLayout->addWidget(m_pButtonBox);

        /* Create status-bar: */
        setStatusBar(new QStatusBar);

        /* Translate dialog: */
        retranslateUi();
    }

    /* Add network-request widget: */
    UINetworkRequestWidget* addNetworkRequestWidget(UINetworkRequest *pNetworkRequest)
    {
        /* Make sure network-request is really exists: */
        AssertMsg(pNetworkRequest, ("Network-request doesn't exists!\n"));

        /* Create new network-request widget: */
        UINetworkRequestWidget *pNetworkRequestWidget = new UINetworkRequestWidget(this, pNetworkRequest);
        m_pMainLayout->insertWidget(m_pMainLayout->count() - 2 /* before button-box and stretch */, pNetworkRequestWidget);
        m_widgets.insert(pNetworkRequest->uuid(), pNetworkRequestWidget);

        /* Hide label: */
        m_pLabel->setHidden(true);
        /* Show button-box: */
        m_pButtonBox->setHidden(false);
        /* Show dialog: */
        showNormal();

        /* Return network-request widget: */
        return pNetworkRequestWidget;
    }

    /* Remove network-request widget: */
    void removeNetworkRequestWidget(const QUuid &uuid)
    {
        /* Make sure network-request widget still present: */
        AssertMsg(m_widgets.contains(uuid), ("Network-request widget already removed!\n"));

        /* Delete corresponding network-request widget: */
        delete m_widgets.value(uuid);
        m_widgets.remove(uuid);

        /* Check if dialog is empty: */
        if (m_widgets.isEmpty())
        {
            /* Show label: */
            m_pLabel->setHidden(false);
            /* Hide button-box: */
            m_pButtonBox->setHidden(true);
            /* Hide dialog: */
            hide();
        }
    }

private slots:

    /* Handler for 'Cancel All' button press: */
    void sltHandleCancelAllButtonPress()
    {
        /* Ask if user wants to cancel all current network-requests: */
        if (msgCenter().askAboutCancelAllNetworkRequest(this))
            emit sigCancelNetworkRequests();
    }

private:

    /* Translate whole dialog: */
    void retranslateUi()
    {
        /* Set window caption: */
        setWindowTitle(UINetworkManager::tr("Network Operations Manager"));

        /* Set description-label text: */
        m_pLabel->setText(UINetworkManager::tr("There are no active network operations."));

        /* Set buttons-box text: */
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(UINetworkManager::tr("&Cancel All"));
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setStatusTip(UINetworkManager::tr("Cancel all active network operations"));
    }

    /* Overloaded show-event: */
    void showEvent(QShowEvent *pShowEvent)
    {
        /* Resize to minimum size: */
        resize(minimumSize());

        /* Center according current main application window: */
        vboxGlobal().centerWidget(this, vboxGlobal().mainWindow(), false);

        /* Pass event to the base-class: */
        QMainWindow::showEvent(pShowEvent);
    }

    /* Overloaded close-event: */
    void closeEvent(QCloseEvent *pCloseEvent)
    {
        /* If there are network-requests present
         * ask if user wants to cancel all current network-requests
         * or leave them working at the background: */
        if (!m_widgets.isEmpty() &&
            msgCenter().askAboutCancelOrLeaveAllNetworkRequest(this))
            emit sigCancelNetworkRequests();

        /* Pass event to the base-class: */
        QMainWindow::closeEvent(pCloseEvent);
    }

    /* Overloaded keypress-event: */
    void keyPressEvent(QKeyEvent *pKeyPressEvent)
    {
        /* 'Escape' key used to close the dialog: */
        if (pKeyPressEvent->key() == Qt::Key_Escape)
        {
            close();
            return;
        }

        /* Pass event to the base-class: */
        QMainWindow::keyPressEvent(pKeyPressEvent);
    }

    /* Main layout: */
    QVBoxLayout *m_pMainLayout;
    QLabel *m_pLabel;
    QIDialogButtonBox *m_pButtonBox;

    /* Popup-widget map: */
    QMap<QUuid, UINetworkRequestWidget*> m_widgets;
};

/* Constructor: */
UINetworkRequest::UINetworkRequest(UINetworkManager *pNetworkManager,
                                   UINetworkManagerDialog *pNetworkManagerDialog,
                                   const QNetworkRequest &request, UINetworkRequestType type,
                                   const QString &strDescription, UINetworkCustomer *pCustomer)
    : QObject(pNetworkManager)
    , m_pNetworkManagerDialog(pNetworkManagerDialog)
    , m_pNetworkRequestWidget(0)
    , m_uuid(QUuid::createUuid())
    , m_requests(QList<QNetworkRequest>() << request)
    , m_iCurrentRequestIndex(0)
    , m_type(type)
    , m_strDescription(strDescription)
    , m_pCustomer(pCustomer)
    , m_fRunning(false)
{
    /* Initialize: */
    initialize();
}

UINetworkRequest::UINetworkRequest(UINetworkManager *pNetworkManager,
                                   UINetworkManagerDialog *pNetworkManagerDialog,
                                   const QList<QNetworkRequest> &requests, UINetworkRequestType type,
                                   const QString &strDescription, UINetworkCustomer *pCustomer)
    : QObject(pNetworkManager)
    , m_pNetworkManagerDialog(pNetworkManagerDialog)
    , m_pNetworkRequestWidget(0)
    , m_uuid(QUuid::createUuid())
    , m_requests(requests)
    , m_iCurrentRequestIndex(0)
    , m_type(type)
    , m_strDescription(strDescription)
    , m_pCustomer(pCustomer)
    , m_fRunning(false)
{
    /* Initialize: */
    initialize();
}

/* Destructor: */
UINetworkRequest::~UINetworkRequest()
{
    /* Destroy network-reply: */
    cleanupNetworkReply();

    /* Destroy network-request widget: */
    m_pNetworkManagerDialog->removeNetworkRequestWidget(m_uuid);
}

/* Network-reply progress handler: */
void UINetworkRequest::sltHandleNetworkReplyProgress(qint64 iReceived, qint64 iTotal)
{
    /* Notify UINetworkManager: */
    emit sigProgress(m_uuid, iReceived, iTotal);
    /* Notify UINetworkRequestWidget: */
    emit sigProgress(iReceived, iTotal);
}

/* Network-reply finish handler: */
void UINetworkRequest::sltHandleNetworkReplyFinish()
{
    /* Set as non-running: */
    m_fRunning = false;

    /* Get sender network reply: */
    QNetworkReply *pNetworkReply = static_cast<QNetworkReply*>(sender());

    /* If network-request was canceled: */
    if (pNetworkReply->error() == QNetworkReply::OperationCanceledError)
    {
        /* Notify UINetworkManager: */
        emit sigCanceled(m_uuid);
    }
    /* If network-reply has no errors: */
    else if (pNetworkReply->error() == QNetworkReply::NoError)
    {
        /* Check if redirection required: */
        QUrl redirect = pNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (redirect.isValid())
        {
            /* Cleanup current network-reply first: */
            cleanupNetworkReply();

            /* Choose redirect-source as current: */
            m_request.setUrl(redirect);

            /* Create new network-reply finally: */
            prepareNetworkReply();
        }
        else
        {
            /* Notify UINetworkRequestWidget: */
            emit sigFinished();
            /* Notify UINetworkManager: */
            emit sigFinished(m_uuid);
        }
    }
    /* If some error occured: */
    else
    {
        /* Check if we have other requests in set: */
        if (m_iCurrentRequestIndex < m_requests.size() - 1)
        {
            /* Cleanup current network-reply first: */
            cleanupNetworkReply();

            /* Choose next network-request as current: */
            ++m_iCurrentRequestIndex;
            m_request = m_requests[m_iCurrentRequestIndex];

            /* Create new network-reply finally: */
            prepareNetworkReply();
        }
        else
        {
            /* Notify UINetworkRequestWidget: */
            emit sigFailed(pNetworkReply->errorString());
            /* Notify UINetworkManager: */
            emit sigFailed(m_uuid, pNetworkReply->errorString());
        }
    }
}

/* Slot to retry network-request: */
void UINetworkRequest::sltRetry()
{
    /* Cleanup current network-reply first: */
    cleanupNetworkReply();

    /* Choose first network-request as current: */
    m_iCurrentRequestIndex = 0;
    m_request = m_requests[m_iCurrentRequestIndex];

    /* Create new network-reply finally: */
    prepareNetworkReply();
}

/* Slot to cancel network-request: */
void UINetworkRequest::sltCancel()
{
    /* Abort network-reply if present: */
    abortNetworkReply();
}

/* Initialize: */
void UINetworkRequest::initialize()
{
    /* Prepare listeners for parent(): */
    connect(parent(), SIGNAL(sigCancelNetworkRequests()), this, SLOT(sltCancel()));

    /* Create network-request widget: */
    m_pNetworkRequestWidget = m_pNetworkManagerDialog->addNetworkRequestWidget(this);

    /* Prepare listeners for m_pNetworkRequestWidget: */
    connect(m_pNetworkRequestWidget, SIGNAL(sigRetry()), this, SLOT(sltRetry()));
    connect(m_pNetworkRequestWidget, SIGNAL(sigCancel()), this, SLOT(sltCancel()));

    /* Choose first network-request as current: */
    m_iCurrentRequestIndex = 0;
    m_request = m_requests[m_iCurrentRequestIndex];

    /* Create network-reply: */
    prepareNetworkReply();
}

/* Prepare network-reply: */
void UINetworkRequest::prepareNetworkReply()
{
    /* Make network-request: */
    switch (m_type)
    {
        case UINetworkRequestType_HEAD:
        {
            m_pReply = gNetworkManager->head(m_request);
            break;
        }
        case UINetworkRequestType_GET:
        {
            m_pReply = gNetworkManager->get(m_request);
            break;
        }
        default:
            break;
    }

    /* Prepare listeners for m_pReply: */
    AssertMsg(m_pReply, ("Unable to make network-request!\n"));
    connect(m_pReply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(sltHandleNetworkReplyProgress(qint64, qint64)));
    connect(m_pReply, SIGNAL(finished()), this, SLOT(sltHandleNetworkReplyFinish()));

    /* Set as running: */
    m_fRunning = true;

    /* Notify UINetworkRequestWidget: */
    emit sigStarted();
}

/* Cleanup network-reply: */
void UINetworkRequest::cleanupNetworkReply()
{
    /* Destroy current reply: */
    AssertMsg(m_pReply, ("Network-reply already destroyed!\n"));
    m_pReply->disconnect();
    m_pReply->deleteLater();
    m_pReply = 0;
}

/* Abort network-reply: */
void UINetworkRequest::abortNetworkReply()
{
    /* Abort network-reply if present: */
    if (m_pReply)
    {
        if (m_fRunning)
            m_pReply->abort();
        else
            emit sigCanceled(m_uuid);
    }
}

/* Instance: */
UINetworkManager* UINetworkManager::m_pInstance = 0;

/* Create singleton: */
void UINetworkManager::create()
{
    /* Check that instance do NOT exist: */
    if (m_pInstance)
        return;

    /* Create instance: */
    new UINetworkManager;
}

/* Destroy singleton: */
void UINetworkManager::destroy()
{
    /* Check that instance exists: */
    if (!m_pInstance)
        return;

    /* Destroy instance: */
    delete m_pInstance;
}

/* Network Access Manager GUI window: */
QWidget* UINetworkManager::window() const
{
    return m_pNetworkProgressDialog;
}

/* Show Network Access Manager GUI: */
void UINetworkManager::show()
{
    /* Just show the dialog: */
    m_pNetworkProgressDialog->showNormal();
}

/* Network-request creation wrapper for UINetworkCustomer: */
void UINetworkManager::createNetworkRequest(const QNetworkRequest &request, UINetworkRequestType type,
                                            const QString &strDescription, UINetworkCustomer *pCustomer)
{
    /* Create new network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(this, m_pNetworkProgressDialog,
                                                             request, type, strDescription, pCustomer);
    /* Prepare created network-request: */
    prepareNetworkRequest(pNetworkRequest);
}

/* Network request (set) creation wrapper for UINetworkCustomer: */
void UINetworkManager::createNetworkRequest(const QList<QNetworkRequest> &requests, UINetworkRequestType type,
                                            const QString &strDescription, UINetworkCustomer *pCustomer)
{
    /* Create new network-request: */
    UINetworkRequest *pNetworkRequest = new UINetworkRequest(this, m_pNetworkProgressDialog,
                                                             requests, type, strDescription, pCustomer);
    /* Prepare created network-request: */
    prepareNetworkRequest(pNetworkRequest);
}

/* Constructor: */
UINetworkManager::UINetworkManager()
{
    /* Prepare instance: */
    m_pInstance = this;

    /* Prepare finally: */
    prepare();
}

/* Destructor: */
UINetworkManager::~UINetworkManager()
{
    /* Cleanup first: */
    cleanup();

    /* Cleanup instance: */
    m_pInstance = 0;
}

/* Prepare: */
void UINetworkManager::prepare()
{
    /* Prepare network manager GUI: */
    m_pNetworkProgressDialog = new UINetworkManagerDialog;

    /* Prepare listeners for m_pNetworkProgressDialog: */
    connect(m_pNetworkProgressDialog, SIGNAL(sigCancelNetworkRequests()), this, SIGNAL(sigCancelNetworkRequests()));
}

/* Cleanup: */
void UINetworkManager::cleanup()
{
    /* Cleanup network-requests first: */
    cleanupNetworkRequests();

    /* Cleanup network manager GUI: */
    delete m_pNetworkProgressDialog;
}

/* Prepare network-request: */
void UINetworkManager::prepareNetworkRequest(UINetworkRequest *pNetworkRequest)
{
    /* Prepare listeners for pNetworkRequest: */
    connect(pNetworkRequest, SIGNAL(sigProgress(const QUuid&, qint64, qint64)), this, SLOT(sltHandleNetworkRequestProgress(const QUuid&, qint64, qint64)));
    connect(pNetworkRequest, SIGNAL(sigCanceled(const QUuid&)), this, SLOT(sltHandleNetworkRequestCancel(const QUuid&)));
    connect(pNetworkRequest, SIGNAL(sigFinished(const QUuid&)), this, SLOT(sltHandleNetworkRequestFinish(const QUuid&)));
    connect(pNetworkRequest, SIGNAL(sigFailed(const QUuid&, const QString&)), this, SLOT(sltHandleNetworkRequestFailure(const QUuid&, const QString&)));

    /* Add this request into internal map: */
    m_requests.insert(pNetworkRequest->uuid(), pNetworkRequest);
}

/* Cleanup network-request: */
void UINetworkManager::cleanupNetworkRequest(const QUuid &uuid)
{
    /* Cleanup network-request map: */
    delete m_requests[uuid];
    m_requests.remove(uuid);
}

/* Cleanup all network-requests: */
void UINetworkManager::cleanupNetworkRequests()
{
    /* Get all the request IDs: */
    const QList<QUuid> &uuids = m_requests.keys();
    /* Cleanup corresponding requests: */
    for (int i = 0; i < uuids.size(); ++i)
        cleanupNetworkRequest(uuids[i]);
}

#if 0
/* Downloader creation notificator: */
void UINetworkManager::notifyDownloaderCreated(UIDownloadType downloaderType)
{
    emit sigDownloaderCreated(downloaderType);
}
#endif

/* Slot to handle network-request progress: */
void UINetworkManager::sltHandleNetworkRequestProgress(const QUuid &uuid, qint64 iReceived, qint64 iTotal)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyProgress(iReceived, iTotal);
}

/* Slot to handle network-request cancel: */
void UINetworkManager::sltHandleNetworkRequestCancel(const QUuid &uuid)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyCanceled(pNetworkRequest->reply());

    /* Cleanup network-request: */
    cleanupNetworkRequest(uuid);
}

/* Slot to handle network-request finish: */
void UINetworkManager::sltHandleNetworkRequestFinish(const QUuid &uuid)
{
    /* Make sure corresponding map contains received ID: */
    AssertMsg(m_requests.contains(uuid), ("Network-request NOT found!\n"));

    /* Get corresponding network-request: */
    UINetworkRequest *pNetworkRequest = m_requests.value(uuid);

    /* Get corresponding customer: */
    UINetworkCustomer *pNetworkCustomer = pNetworkRequest->customer();

    /* Send to customer to process: */
    pNetworkCustomer->processNetworkReplyFinished(pNetworkRequest->reply());

    /* Cleanup network-request: */
    cleanupNetworkRequest(uuid);
}

/* Slot to handle network-request failure: */
void UINetworkManager::sltHandleNetworkRequestFailure(const QUuid &uuid, const QString &strError)
{
    /* Just show the dialog: */
    show();
    Q_UNUSED(uuid);
    Q_UNUSED(strError);
}

#include "UINetworkManager.moc"

