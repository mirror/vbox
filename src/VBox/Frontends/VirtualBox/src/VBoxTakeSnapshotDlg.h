/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxTakeSnapshotDlg class declaration.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxTakeSnapshotDlg_h__
#define __VBoxTakeSnapshotDlg_h__

/* GUI includes: */
#include "VBoxTakeSnapshotDlg.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

/* Forward declarations: */
class CMachine;

class VBoxTakeSnapshotDlg : public QIWithRetranslateUI<QIDialog>, public Ui::VBoxTakeSnapshotDlg
{
    Q_OBJECT;

public:

    VBoxTakeSnapshotDlg(QWidget *pParent, const CMachine &machine);

protected:

    void retranslateUi();

private slots:

    void nameChanged(const QString &strName);
};

#endif // __VBoxTakeSnapshotDlg_h__

