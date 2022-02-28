/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
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

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerLogPanel_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerLogPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "UIDialogPanel.h"

/* Forward declarations: */
class QTextEdit;
class UIFileManager;

/** UIDialogPanel extension to display file manager logs. */
class UIFileManagerLogPanel : public UIDialogPanel
{
    Q_OBJECT;

public:

    UIFileManagerLogPanel(QWidget *pParent = 0);
    void appendLog(const QString &str, const QString &strMachineName, FileManagerLogType eLogType);
    virtual QString panelName() const RT_OVERRIDE;

protected:

    virtual void prepareWidgets() RT_OVERRIDE;
    virtual void prepareConnections() RT_OVERRIDE;

    /** Handles the translation event. */
    void retranslateUi();

private slots:


private:

    QTextEdit *m_pLogTextEdit;
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerLogPanel_h */
