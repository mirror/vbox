/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlFileManagerLogPanel_h___
#define ___UIGuestControlFileManagerLogPanel_h___

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "UIGuestControlFileManagerPanel.h"

/* Forward declarations: */
class QTextEdit;
class UIGuestControlFileManager;

/** UIVMLogViewerPanel extension providing GUI to manage logviewer settings. */
class UIGuestControlFileManagerLogPanel : public UIGuestControlFileManagerPanel
{
    Q_OBJECT;

public:

    UIGuestControlFileManagerLogPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent);
    void appendLog(const QString &str, FileManagerLogType);
    virtual QString panelName() const /* override */;

protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;

    /** Handles the translation event. */
    void retranslateUi();

private slots:


private:

    QTextEdit *m_pLogTextEdit;
};

#endif /* !___UIGuestControlFileManagerLogPanel_h___ */
