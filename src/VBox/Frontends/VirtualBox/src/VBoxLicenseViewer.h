/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxLicenseViewer class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxLicenseViewer___
#define ___VBoxLicenseViewer___

/* Qt includes: */
#include <QDialog>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QTextBrowser;
class QPushButton;

/** QDialog subclass used to show a user license under linux. */
class SHARED_LIBRARY_STUFF VBoxLicenseViewer : public QIWithRetranslateUI2<QDialog>
{
    Q_OBJECT;

public:

    /** Constructs license viewer passing @a pParent to the base-class. */
    VBoxLicenseViewer(QWidget *pParent = 0);

    /** Shows license from passed @a strLicenseText. */
    int showLicenseFromString(const QString &strLicenseText);
    /** Shows license from file with passed @a strLicenseFileName. */
    int showLicenseFromFile(const QString &strLicenseFileName);

protected:

    /** Preprocesses Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles Qt show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Executes the dialog. */
    int exec();

    /** Handles scroll-bar moving by a certain @a iValue. */
    void sltHandleScrollBarMoved(int iValue);

    /** Uplocks buttons. */
    void sltUnlockButtons();

private:

    /** Holds the licence text browser instance. */
    QTextBrowser *m_pLicenseBrowser;

    /** Holds the licence agree button instance. */
    QPushButton *m_pButtonAgree;
    /** Holds the licence disagree button instance. */
    QPushButton *m_pButtonDisagree;
};

#endif /* !___VBoxLicenseViewer___ */

