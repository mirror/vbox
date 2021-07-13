/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMEditors class declaration.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h
#define FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QGroupBox>

/* Local includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QGridLayout;
class QLabel;
class QILineEdit;
class UIPasswordLineEdit;
class UIUserNamePasswordEditor;

class UIUserNamePasswordGroupBox : public QGroupBox
{
    Q_OBJECT;

signals:

    void sigUserNameChanged(const QString &strUserName);
    void sigPasswordChanged(const QString &strPassword);

public:

    UIUserNamePasswordGroupBox(QWidget *pParent = 0);

    /** @name Wrappers for UIUserNamePasswordEditor
     * @{ */
       QString userName() const;
       void setUserName(const QString &strUserName);

       QString password() const;
       void setPassword(const QString &strPassword);
       bool isComplete();
       void setLabelsVisible(bool fVisible);
    /** @} */

private:

    void prepare();

    UIUserNamePasswordEditor *m_pUserNamePasswordEditor;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_editors_UIWizardNewVMEditors_h */
