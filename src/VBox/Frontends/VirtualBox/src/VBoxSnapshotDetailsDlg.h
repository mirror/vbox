/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSnapshotDetailsDlg class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxSnapshotDetailsDlg_h___
#define ___VBoxSnapshotDetailsDlg_h___

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "VBoxSnapshotDetailsDlg.gen.h"

/* COM includes: */
#include "CSnapshot.h"


/** QDialog extension providing GUI with snapshot details dialog. */
class VBoxSnapshotDetailsDlg : public QIWithRetranslateUI<QDialog>, public Ui::VBoxSnapshotDetailsDlg
{
    Q_OBJECT;

public:

    /** Constructs snapshot details dialog passing @a pParent to the base-class. */
    VBoxSnapshotDetailsDlg(QWidget *pParent = 0);

    /** Defines the snapshot @a data. */
    void setData(const CSnapshot &comSnapshot);
    /** Saves the snapshot data. */
    void saveData();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

private slots:

    /** Handles snapshot @a strName change. */
    void sltHandleNameChange(const QString &strName);

private:

    /** Prepares all. */
    void prepare();

    /** Holds whether this widget was polished. */
    bool m_fPolished;

    /** Holds the snapshot object to load data from. */
    CSnapshot m_comSnapshot;

    /** Holds the cached thumbnail. */
    QPixmap m_pixmapThumbnail;
    /** Holds the cached screenshot. */
    QPixmap m_pixmapScreenshot;
};

#endif /* !___VBoxSnapshotDetailsDlg_h___ */

