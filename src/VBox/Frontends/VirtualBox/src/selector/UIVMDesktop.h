/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIVMDesktop class declarations
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMDesktop_h__
#define __UIVMDesktop_h__

/* Local includes */
#include "QIWithRetranslateUI.h"

/* Global includes */
#include <QWidget>

/* Local forward declarations */
class CMachine;
class UIDescriptionPagePrivate;
class UIDetailsPagePrivate;
class UITexturedSegmentedButton;
class UIVMItem;
class VBoxSnapshotsWgt;
class UIToolBar;

class UIVMDesktop: public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIVMDesktop(UIToolBar *pToolBar, QAction *pRefreshAction, QWidget *pParent = 0);

    void updateDetails(UIVMItem *pVMItem, const QList<CMachine> &machines);
    void updateDetailsText(const QString &strText);
    void updateDetailsErrorText(const QString &strText);

    void updateSnapshots(UIVMItem *pVMItem, const CMachine& machine);
//    void updateDescription(UIVMItem *pVMItem, const CMachine& machine);
//    void updateDescriptionState();

signals:

    void linkClicked(const QString &strURL);

protected:

    void retranslateUi();

private:

    /* Private member vars */
    UITexturedSegmentedButton *m_pHeaderBtn;
    UIDetailsPagePrivate *m_pDetails;
    VBoxSnapshotsWgt *m_pSnapshotsPage;
//    UIDescriptionPagePrivate *m_pDescription;
};

#endif /* !__UIVMDesktop_h__ */

