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

#ifndef FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerOptionsPanel_h
#define FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerOptionsPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIVMLogViewerPanel.h"

/* Forward declarations: */
class QCheckBox;
class QSpinBox;
class QLabel;
class QIToolButton;
class UIVMLogViewerWidget;

/** UIVMLogViewerPanel extension providing GUI to manage logviewer options. */
class UIVMLogViewerOptionsPanel : public UIVMLogViewerPanel
{
    Q_OBJECT;

signals:

    void sigShowLineNumbers(bool show);
    void sigWrapLines(bool show);
    void sigChangeFontSizeInPoints(int size);
    void sigChangeFont(QFont font);
    void sigResetToDefaults();

public:

    UIVMLogViewerOptionsPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

    void setShowLineNumbers(bool bShowLineNumbers);
    void setWrapLines(bool bWrapLines);
    void setFontSizeInPoints(int fontSizeInPoints);
    virtual QString panelName() const /* override */;

public slots:


protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;

    /** Handles the translation event. */
    void retranslateUi();

private slots:

    void sltOpenFontDialog();

private:

    QCheckBox    *m_pLineNumberCheckBox;
    QCheckBox    *m_pWrapLinesCheckBox;
    QSpinBox     *m_pFontSizeSpinBox;
    QLabel       *m_pFontSizeLabel;
    QIToolButton *m_pOpenFontDialogButton;
    QIToolButton *m_pResetToDefaultsButton;

    /** Default font size in points. */
    const int    m_iDefaultFontSize;

};

#endif /* !FEQT_INCLUDED_SRC_logviewer_UIVMLogViewerOptionsPanel_h */
