/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressDialog class declaration.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIProgressDialog_h__
#define __UIProgressDialog_h__

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QProgressBar;
class QLabel;
class QILabel;
class UIMiniCancelButton;
class CProgress;

/**
 * A QProgressDialog enhancement that allows to:
 *
 * 1) prevent closing the dialog when it has no cancel button;
 * 2) effectively track the IProgress object completion (w/o using
 *    IProgress::waitForCompletion() and w/o blocking the UI thread in any other
 *    way for too long).
 *
 * @note The CProgress instance is passed as a non-const reference to the
 *       constructor (to memorize COM errors if they happen), and therefore must
 *       not be destroyed before the created UIProgressDialog instance is
 *       destroyed.
 */
class UIProgressDialog: public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructor. */
    UIProgressDialog(CProgress &progress, const QString &strTitle,
                     QPixmap *pImage = 0, int cMinDuration = 2000, QWidget *pParent = 0);
    /** Destructor. */
    ~UIProgressDialog();

    /* API: Run stuff: */
    int run(int aRefreshInterval);

signals:

    /** Notifies listeners about wrapped CProgress change.
      * @param iOperations  holds the number of operations CProgress have,
      * @param strOperation holds the description of the current CProgress operation,
      * @param iOperation   holds the index of the current CProgress operation,
      * @param iPercent     holds the percentage of the current CProgress operation. */
    void sigProgressChange(ulong iOperations, QString strOperation,
                           ulong iOperation, ulong iPercent);

public slots:

    /* Handler: Show stuff: */
    void show();

protected:

    /* Helper: Translate stuff: */
    void retranslateUi();

    /* Helper: Cancel stuff: */
    void reject();

    /* Handlers: Event processing stuff: */
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void closeEvent(QCloseEvent *pEvent);

private slots:

    /* Handler: Cancel stuff: */
    void sltCancelOperation();

private:

    /** Timer event handling delegate. */
    void handleTimerEvent();

    /* Variables: */
    CProgress &m_progress;
    QLabel *m_pImageLbl;
    QILabel *m_pDescriptionLbl;
    QProgressBar *m_pProgressBar;
    UIMiniCancelButton *m_pCancelBtn;
    QILabel *m_pEtaLbl;
    bool m_fCancelEnabled;
    QString m_strCancel;
    const ulong m_cOperations;
    ulong m_iCurrentOperation;
    bool m_fEnded;

    static const char *m_spcszOpDescTpl;
};

/** QObject reimplementation allowing to effectively track the CProgress object completion
  * (w/o using CProgress::waitForCompletion() and w/o blocking the calling thread in any other way for too long).
  * @note The CProgress instance is passed as a non-const reference to the constructor
  *       (to memorize COM errors if they happen), and therefore must not be destroyed
  *       before the created UIProgress instance is destroyed.
  * @todo To be moved to separate files. */
class UIProgress : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about wrapped CProgress change.
      * @param iOperations  holds the number of operations CProgress have,
      * @param strOperation holds the description of the current CProgress operation,
      * @param iOperation   holds the index of the current CProgress operation,
      * @param iPercent     holds the percentage of the current CProgress operation. */
    void sigProgressChange(ulong iOperations, QString strOperation,
                           ulong iOperation, ulong iPercent);

    /** Notifies listeners about particular COM error.
      * @param strErrorInfo holds the details of the error happened. */
    void sigProgressError(QString strErrorInfo);

public:

    /** Constructor passing @a pParent to the base-class.
      * @param progress holds the CProgress to be wrapped. */
    UIProgress(CProgress &progress, QObject *pParent = 0);

    /** Starts the UIProgress by entering the personal event-loop.
      * @param iRefreshInterval holds the refresh interval to check
      *                         for the CProgress updates with. */
    void run(int iRefreshInterval);

private:

    /** Timer @a pEvent reimplementation. */
    virtual void timerEvent(QTimerEvent *pEvent);

    /** Holds the wrapped CProgress reference. */
    CProgress &m_progress;
    /** Holds the number of operations wrapped CProgress have. */
    const ulong m_cOperations;
    /** Holds whether the UIProgress ended. */
    bool m_fEnded;

    /** Holds the personal event-loop. */
    QPointer<QEventLoop> m_pEventLoop;
};

#endif /* __UIProgressDialog_h__ */

