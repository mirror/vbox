/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkRequestWidget stuff declaration.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UINetworkRequestWidget_h___
#define ___UINetworkRequestWidget_h___

/* GUI inludes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"
#include "UIPopupBox.h"

/* Forward declarations: */
class UINetworkManagerDialog;
class QWidget;
class QGridLayout;
class QProgressBar;
class QIToolButton;
class QIRichTextLabel;
class UINetworkRequest;
class QTimer;

/** UIPopupBox reimplementation to reflect network-request status. */
class SHARED_LIBRARY_STUFF UINetworkRequestWidget : public QIWithRetranslateUI<UIPopupBox>
{
    Q_OBJECT;

signals:

    /** Asks to retry network-request. */
    void sigRetry();
    /** Asks to cancel network-request. */
    void sigCancel();

protected:

    /** Allows creation of UINetworkRequestWidget to UINetworkManagerDialog only. */
    friend class UINetworkManagerDialog;
    /** Constructs @a pNetworkRequest widget passing @a pParent to the base-class. */
    UINetworkRequestWidget(UINetworkManagerDialog *pParent, UINetworkRequest *pNetworkRequest);

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Updates current network-request progess as @a iReceived of @a iTotal. */
    void sltSetProgress(qint64 iReceived, qint64 iTotal);

    /** Sets current network-request progress to 'started'. */
    void sltSetProgressToStarted();
    /** Sets current network-request progress to 'finished'. */
    void sltSetProgressToFinished();
    /** Sets current network-request progress to 'failed' because of @a strError. */
    void sltSetProgressToFailed(const QString &strError);

    /** Handles frozen progress. */
    void sltTimeIsOut();

private:

    /** Composes error text on the basis of the passed @a strErrorText. */
    static const QString composeErrorText(QString strErrorText);

    /** Holds the contents widget instance. */
    QWidget         *m_pContentWidget;
    /** Holds the main layout instance. */
    QGridLayout     *m_pMainLayout;
    /** Holds the progress-bar instance. */
    QProgressBar    *m_pProgressBar;
    /** Holds the Retry button instance. */
    QIToolButton    *m_pRetryButton;
    /** Holds the Cancel button instance. */
    QIToolButton    *m_pCancelButton;
    /** Holds the error-pane instance. */
    QIRichTextLabel *m_pErrorPane;

    /** Holds the network request reference. */
    UINetworkRequest *m_pNetworkRequest;

    /** Holds the timeout timer instance. */
    QTimer *m_pTimer;
};

#endif /* !___UINetworkRequestWidget_h___ */

