/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreatorOptionsDialog class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsDialog_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIVisoCreatorDefs.h"

/* Forward declarations: */
class QITabWidget;
class QVBoxLayout;
class QIDialogButtonBox;

class SHARED_LIBRARY_STUFF UIVisoCreatorOptionsDialog : public QIDialog
{
    Q_OBJECT;

public:

    UIVisoCreatorOptionsDialog(const VisoOptions &visoOptions,
                               const BrowserOptions &browserOptions,
                               QWidget *pParent = 0);
    ~UIVisoCreatorOptionsDialog();
    const BrowserOptions &browserOptions() const;
    const VisoOptions &visoOptions() const;

private slots:
    void sltHandlShowHiddenObjectsChange(int iState);
    void sltHandleVisoNameChange(const QString &strText);

private:
    enum TabPage
    {
        TabPage_VISO_Options,
        TabPage_Browser_Options,
        TabPage_Max
    };

    void prepareObjects();
    void prepareConnections();
    void prepareTabWidget();
    QVBoxLayout          *m_pMainLayout;
    QIDialogButtonBox    *m_pButtonBox;
    QITabWidget          *m_pTab;
    //    QILineEdit           *m_pVisoNameLineEdit;
    //QILabel              *m_pVISONameLabel;
    VisoOptions          m_visoOptions;
    BrowserOptions       m_browserOptions;
    friend class UIVisoCreator;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsDialog_h */
