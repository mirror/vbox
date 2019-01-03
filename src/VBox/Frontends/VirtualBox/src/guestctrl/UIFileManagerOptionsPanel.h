/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerOptionsPanel_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerOptionsPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIFileManagerPanel.h"

/* Forward declarations: */
class QCheckBox;
class QSpinBox;
class QLabel;
class QIToolButton;
class UIFileManagerOptions;

/** UIFileManagerPanel extension to change file manager options. It directly
 *  modifies the options through the passed UIFileManagerOptions instance. */
class UIFileManagerOptionsPanel : public UIFileManagerPanel
{
    Q_OBJECT;

public:

    UIFileManagerOptionsPanel(UIFileManager *pManagerWidget,
                                           QWidget *pParent, UIFileManagerOptions *pFileManagerOptions);
    virtual QString panelName() const /* override */;
    /** Reads the file manager options and updates the widget accordingly. This functions is typically called
     *  when file manager options have been changed by other means and this panel needs to adapt. */
    void update();

signals:

    void sigOptionsChanged();

protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;

    /** Handles the translation event. */
    void retranslateUi();

private slots:

    void sltListDirectoryCheckBoxToogled(bool bChecked);
    void sltDeleteConfirmationCheckBoxToogled(bool bChecked);
    void sltHumanReabableSizesCheckBoxToogled(bool bChecked);
    void sltShowHiddenObjectsCheckBoxToggled(bool bChecked);

private:

    QCheckBox  *m_pListDirectoriesOnTopCheckBox;
    QCheckBox  *m_pDeleteConfirmationCheckBox;
    QCheckBox  *m_pHumanReabableSizesCheckBox;
    QCheckBox  *m_pShowHiddenObjectsCheckBox;
    UIFileManagerOptions *m_pFileManagerOptions;
};

#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerOptionsPanel_h */
