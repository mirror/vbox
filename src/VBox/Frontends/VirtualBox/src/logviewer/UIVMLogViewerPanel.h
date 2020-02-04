/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QKeySequence>

/* GUI includes: */
#include "UIDialogPanel.h"

/* Forward declarations: */
class QPlainTextEdit;
class QTextDocument;
class UIVMLogViewerWidget;


/** UIDialonPanel extension acting as the base class for UIVMLogViewerXXXPanel widgets. */
class UIVMLogViewerPanel : public UIDialogPanel
{
    Q_OBJECT;

public:

    UIVMLogViewerPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

protected:

    virtual void retranslateUi() /* override */;

    /* Access functions for children classes. */
    UIVMLogViewerWidget        *viewer();
    const UIVMLogViewerWidget  *viewer() const;
    QTextDocument  *textDocument();
    QPlainTextEdit *textEdit();
    /* Return the unmodified log. */
    const QString *logString() const;

private:

    /** Holds the reference to VM Log-Viewer this panel belongs to. */
    UIVMLogViewerWidget *m_pViewer;
};

#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerPanel_h */
