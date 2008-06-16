/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSnapshotDetailsDlg class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxSnapshotDetailsDlg_h__
#define __VBoxSnapshotDetailsDlg_h__

#include "VBoxSnapshotDetailsDlg.gen.h"
#include "QIWithRetranslateUI.h"
#include "COMDefs.h"

class VBoxSnapshotDetailsDlg : public QIWithRetranslateUI<QDialog>,
                               public Ui::VBoxSnapshotDetailsDlg
{
    Q_OBJECT

public:

    VBoxSnapshotDetailsDlg (QWidget *aParent);

    void getFromSnapshot (const CSnapshot &aSnapshot);
    void putBackToSnapshot();

protected:

    void retranslateUi();

private slots:

    void onNameChanged (const QString &aText);

private:

    CSnapshot mSnapshot;
};

#endif // __VBoxSnapshotDetailsDlg_h__

