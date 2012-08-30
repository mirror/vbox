/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIProgressDialog class declaration
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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

/* Qt includes: */
#include "QIDialog.h"

/* Forward declarations: */
class QProgressBar;
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
class UIProgressDialog: protected QIDialog
{
    Q_OBJECT;

public:

    UIProgressDialog(CProgress &progress, const QString &strTitle,
                     QPixmap *pImage = 0, bool fSheetOnDarwin = false,
                     int cMinDuration = 2000, QWidget *pParent = 0);
    ~UIProgressDialog();

    int run(int aRefreshInterval);
    bool cancelEnabled() const { return m_fCancelEnabled; }

protected:

    virtual void retranslateUi();

    virtual void reject();

    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void closeEvent(QCloseEvent *pEvent);

private slots:

    void showDialog();
    void cancelOperation();

private:

    /* Private member vars */
    CProgress &m_progress;
    QILabel *m_pImageLbl;
    QILabel *m_pDescriptionLbl;
    QILabel *m_pEtaLbl;
    QString m_strCancel;
    QProgressBar *m_progressBar;
    UIMiniCancelButton *m_pCancelBtn;
    bool m_fCancelEnabled;
    const ulong m_cOperations;
    ulong m_iCurrentOperation;
    bool m_fEnded;

    static const char *m_spcszOpDescTpl;
};

#endif /* __UIProgressDialog_h__ */

