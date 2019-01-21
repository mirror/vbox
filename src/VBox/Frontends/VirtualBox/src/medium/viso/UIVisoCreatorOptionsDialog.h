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
class QGridLayout;
class QIDialogButtonBox;

class UIVisoCreatorOptionsDialog : public QIDialog
{
    Q_OBJECT;

public:

    UIVisoCreatorOptionsDialog(const BrowserOptions &browserOptions,
                               QWidget *pParent = 0);
    ~UIVisoCreatorOptionsDialog();
    const BrowserOptions &browserOptions() const;

private slots:

    void sltHandlShowHiddenObjectsChange(int iState);

private:

    void prepareObjects();
    void prepareConnections();
    QGridLayout          *m_pMainLayout;
    QIDialogButtonBox    *m_pButtonBox;
    BrowserOptions       m_browserOptions;
    friend class UIVisoCreator;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoCreatorOptionsDialog_h */
