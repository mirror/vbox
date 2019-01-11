/* $Id$ */
/** @file
 * VBox Qt GUI - UIVisoConfigurationDialog class declaration.
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

#ifndef FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationDialog_h
#define FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIVisoCreatorDefs.h"

/* Forward declarations: */
class QGridLayout;
class QTextEdit;
class QIDialogButtonBox;
class QILineEdit;
class QITabWidget;

class SHARED_LIBRARY_STUFF UIVisoConfigurationDialog : public QIDialog
{
    Q_OBJECT;

public:
    UIVisoConfigurationDialog(const VisoOptions &visoOptions,
                              QWidget *pParent = 0);
    ~UIVisoConfigurationDialog();
    const VisoOptions &visoOptions() const;

public slots:

    void accept() /* override */;


private:

    void prepareObjects();
    void prepareConnections();

    QGridLayout          *m_pMainLayout;
    QIDialogButtonBox    *m_pButtonBox;
    QILineEdit           *m_pVisoNameLineEdit;
    QTextEdit           *m_pCustomOptionsTextEdit;
    VisoOptions          m_visoOptions;

    friend class UIVisoCreator;
};

#endif /* !FEQT_INCLUDED_SRC_medium_viso_UIVisoConfigurationDialog_h */
