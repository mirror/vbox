/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserDialog class declaration.
 */

/*
 * Copyright (C) 2010-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h
#define FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRestorableGeometry.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QLabel;
class UIHelpBrowserWidget;

class SHARED_LIBRARY_STUFF UIHelpBrowserDialog : public QIWithRetranslateUI<QIWithRestorableGeometry<QMainWindow> >
{
    Q_OBJECT;

public:

    UIHelpBrowserDialog(QWidget *pParent, QWidget *pCenterWidget, const QString &strHelpFilePath);
    /** A passthru function for QHelpIndexWidget::showHelpForKeyword. */
    void showHelpForKeyword(const QString &strKeyword);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

    /** @name Prepare/cleanup cascade.
     * @{ */
    virtual void prepareCentralWidget();
    virtual void loadSettings();
    virtual void saveDialogGeometry();
    /** @} */

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const RT_OVERRIDE;

private slots:

    void sltStatusBarMessage(const QString& strLink, int iTimeOut);
    void sltStatusBarVisibilityChange(bool fVisible);
    void sltZoomPercentageChanged(int iPercentage);

private:

    QString m_strHelpFilePath;
    UIHelpBrowserWidget *m_pWidget;
    QWidget *m_pCenterWidget;
    int m_iGeometrySaveTimerId;
    QLabel *m_pZoomLabel;
};


#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h */
