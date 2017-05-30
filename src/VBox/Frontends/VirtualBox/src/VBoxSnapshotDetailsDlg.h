/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotDetailsDlg class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxSnapshotDetailsDlg_h__
#define __VBoxSnapshotDetailsDlg_h__

/* GUI includes: */
#include "VBoxSnapshotDetailsDlg.gen.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "CSnapshot.h"


class VBoxSnapshotDetailsDlg : public QIWithRetranslateUI <QDialog>, public Ui::VBoxSnapshotDetailsDlg
{
    Q_OBJECT;

public:

    VBoxSnapshotDetailsDlg (QWidget *aParent);

    void getFromSnapshot (const CSnapshot &aSnapshot);
    void putBackToSnapshot();

protected:

    void retranslateUi();

    bool eventFilter (QObject *aObject, QEvent *aEvent);
    void showEvent (QShowEvent *aEvent);

private slots:

    void onNameChanged (const QString &aText);

private:

    CSnapshot mSnapshot;

    QPixmap mThumbnail;
    QPixmap mScreenshot;
};

#endif // __VBoxSnapshotDetailsDlg_h__

