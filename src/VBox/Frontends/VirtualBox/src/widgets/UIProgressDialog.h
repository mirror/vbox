/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressDialog class declaration.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIProgressDialog_h___
#define ___UIProgressDialog_h___

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QProgressBar;
class QLabel;
class QILabel;
class UIMiniCancelButton;
class CProgress;


/** QProgressDialog enhancement that allows to:
  * 1) prevent closing the dialog when it has no cancel button;
  * 2) effectively track the IProgress object completion (w/o using
  *    IProgress::waitForCompletion() and w/o blocking the UI thread in any other way for too long).
  * @note The CProgress instance is passed as a non-const reference to the constructor (to memorize COM errors if they happen),
  *       and therefore must not be destroyed before the created UIProgressDialog instance is destroyed. */
class UIProgressDialog : public QIWithRetranslateUI2<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs progress-dialog passing @a pParent to the base-class.
      * @param  comProgress   Brings the progress reference.
      * @param  strTitle      Brings the progress-dialog title.
      * @param  pImage        Brings the progress-dialog image.
      * @param  cMinDuration  Brings the minimum duration before the progress-dialog is shown. */
    UIProgressDialog(CProgress &comProgress, const QString &strTitle,
                     QPixmap *pImage = 0, int cMinDuration = 2000, QWidget *pParent = 0);
    /** Destructs progress-dialog. */
    virtual ~UIProgressDialog() /* override */;

    /** Executes the progress-dialog within its loop with passed @a iRefreshInterval. */
    int run(int iRefreshInterval);

signals:

    /** Notifies listeners about wrapped CProgress change.
      * @param  iOperations   Brings the number of operations CProgress have.
      * @param  strOperation  Brings the description of the current CProgress operation.
      * @param  iOperation    Brings the index of the current CProgress operation.
      * @param  iPercent      Brings the percentage of the current CProgress operation. */
    void sigProgressChange(ulong iOperations, QString strOperation,
                           ulong iOperation, ulong iPercent);

public slots:

    /** Shows progress-dialog if it's not yet shown. */
    void show();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Rejects dialog. */
    virtual void reject() /* override */;

    /** Handles timer @a pEvent. */
    virtual void timerEvent(QTimerEvent *pEvent) /* override */;
    /** Handles close @a pEvent. */
    virtual void closeEvent(QCloseEvent *pEvent) /* override */;

private slots:

    /** Handles request to cancel operation. */
    void sltCancelOperation();

private:

    /** Performes timer event handling. */
    void handleTimerEvent();

    /** Holds the progress reference. */
    CProgress &m_comProgress;

    /** Holds the image label instance. */
    QLabel             *m_pLabelImage;
    /** Holds the description label instance. */
    QILabel            *m_pLabelDescription;
    /** Holds the progress-bar instance. */
    QProgressBar       *m_pProgressBar;
    /** Holds the cancel button instance. */
    UIMiniCancelButton *m_pButtonCancel;
    /** Holds the ETA label instance. */
    QILabel            *m_pLabelEta;

    /** Holds whether progress cancel is enabled. */
    bool         m_fCancelEnabled;
    /** Holds the amount of operations. */
    const ulong  m_cOperations;
    /** Holds the number of current operation. */
    ulong        m_uCurrentOperation;
    /** Holds whether the progress has ended. */
    bool         m_fEnded;

    /** Holds the operation description template. */
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
      * @param  iOperations   Brings the number of operations CProgress have.
      * @param  strOperation  Brings the description of the current CProgress operation.
      * @param  iOperation    Brings the index of the current CProgress operation.
      * @param  iPercent      Brings the percentage of the current CProgress operation. */
    void sigProgressChange(ulong iOperations, QString strOperation,
                           ulong iOperation, ulong iPercent);

    /** Notifies listeners about particular COM error.
      * @param strErrorInfo holds the details of the error happened. */
    void sigProgressError(QString strErrorInfo);

public:

    /** Constructs progress handler passing @a pParent to the base-class.
      * @param  comProgress  Brings the progress reference. */
    UIProgress(CProgress &comProgress, QObject *pParent = 0);

    /** Executes the progress-handler within its loop with passed @a iRefreshInterval. */
    void run(int iRefreshInterval);

private:

    /** Handles timer @a pEvent. */
    virtual void timerEvent(QTimerEvent *pEvent) /* override */;

    /** Holds the progress reference. */
    CProgress &m_comProgress;

    /** Holds the amount of operations. */
    const ulong  m_cOperations;
    /** Holds whether the progress has ended. */
    bool         m_fEnded;

    /** Holds the personal event-loop instance. */
    QPointer<QEventLoop> m_pEventLoop;
};

#endif /* !___UIProgressDialog_h___ */

