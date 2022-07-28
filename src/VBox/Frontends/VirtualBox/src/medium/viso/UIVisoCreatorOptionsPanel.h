/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoCreatorOptionsPanel class declaration.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsPanel_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsPanel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Forward declarations: */
class QCheckBox;
class QILabel;

/* GUI includes: */
#include "UIDialogPanel.h"

class UIVisoCreatorOptionsPanel : public UIDialogPanel
{
    Q_OBJECT;

signals:

    void sigShowHiddenObjects(bool fShow);

public:

    UIVisoCreatorOptionsPanel(QWidget *pParent = 0);
    ~UIVisoCreatorOptionsPanel();
    virtual QString panelName() const RT_OVERRIDE;
    void setShowHiddenbjects(bool fShow);

protected:

    void retranslateUi() RT_OVERRIDE;


private slots:

    void sltHandlShowHiddenObjectsChange(int iState);

private:

    void prepareObjects();
    void prepareConnections();

    QCheckBox *m_pShowHiddenObjectsCheckBox;
    QILabel *m_pShowHiddenObjectsLabel;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsPanel_h */
