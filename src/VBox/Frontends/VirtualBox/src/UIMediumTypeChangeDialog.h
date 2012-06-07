/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMediumTypeChangeDialog class declaration
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIMediumTypeChangeDialog_h__
#define __UIMediumTypeChangeDialog_h__

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMedium.h"

/* Forward declarations: */
class QVBoxLayout;
class QILabel;
class QGroupBox;
class QRadioButton;
class QIDialogButtonBox;

/* Dialog providing user with possibility to change medium type: */
class UIMediumTypeChangeDialog : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIMediumTypeChangeDialog(QWidget *pParent, const QString &strMediumId);

protected:

    /* Translation stuff: */
    void retranslateUi();

protected slots:

    /* Finisher(s): */
    void sltAccept();
    void sltReject();

    /* Validation stuff: */
    void sltValidate();

private:

    /* Various helping stuff: */
    void createMediumTypeButtons();
    void createMediumTypeButton(KMediumType mediumType);

    /* Widgets: */
    QILabel *m_pLabel;
    QGroupBox *m_pGroupBox;
    QVBoxLayout *m_pGroupBoxLayout;
    QIDialogButtonBox *m_pButtonBox;

    /* Variables: */
    CMedium m_medium;
    KMediumType m_oldMediumType;
    KMediumType m_newMediumType;
};

#endif // __UIMediumTypeChangeDialog_h__
