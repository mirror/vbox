/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIProgressDialog class declaration
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
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

    /* Constructor/destructor: */
    UIProgressDialog(CProgress &progress, const QString &strTitle,
                     QPixmap *pImage = 0, int cMinDuration = 2000, QWidget *pParent = 0);

    /* API: Run stuff: */
    int run(int aRefreshInterval);

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

#endif /* __UIProgressDialog_h__ */

