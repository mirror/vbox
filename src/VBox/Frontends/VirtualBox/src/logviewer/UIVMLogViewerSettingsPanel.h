/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMLogViewerSettingssPanel_h___
#define ___UIVMLogViewerSettingssPanel_h___

/* GUI includes: */
#include "UIVMLogViewerPanel.h"

/* Forward declarations: */
class QCheckBox;
class QSpinBox;
class QLabel;
class QIToolButton;
class UIVMLogViewerWidget;

/** UIVMLogViewerPanel extension providing GUI to manage logviewer settings. */
class UIVMLogViewerSettingsPanel : public UIVMLogViewerPanel
{
    Q_OBJECT;

signals:

    void sigShowLineNumbers(bool show);
    void sigWrapLines(bool show);
    void sigChangeFontSizeInPoints(int size);
    void sigChangeFont(QFont font);
    void sigResetToDefaults();

public:

    UIVMLogViewerSettingsPanel(QWidget *pParent, UIVMLogViewerWidget *pViewer);

    void setShowLineNumbers(bool bShowLineNumbers);
    void setWrapLines(bool bWrapLines);
    void setFontSizeInPoints(int fontSizeInPoints);

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

#endif /* !___UIVMLogViewerSettingsPanel_h___ */
