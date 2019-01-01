/* $Id$ */
/** @file
 * VBox Qt GUI - UIFileManagerDialog class declaration.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_guestctrl_UIFileManagerDialog_h
#define FEQT_INCLUDED_SRC_guestctrl_UIFileManagerDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QString>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"

/* Forward declarations: */
class QDialogButtonBox;
class QVBoxLayout;
class UIActionPool;
class UIFileManagerDialog;
class CGuest;


/** QIManagerDialogFactory extension used as a factory for the file manager dialog. */
class UIFileManagerDialogFactory : public QIManagerDialogFactory
{
public:

    UIFileManagerDialogFactory(UIActionPool *pActionPool = 0, const CGuest &comGuest = CGuest(), const QString &strMachineName = QString());

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Passes the widget to center wrt. pCenterWidget. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

    UIActionPool *m_pActionPool;
    CGuest        m_comGuest;
    QString       m_strMachineName;
};


/** QIManagerDialog extension providing GUI with the dialog displaying file manager releated logs. */
class UIFileManagerDialog : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

public:

    /** Constructs File Manager dialog.
      * @param  pCenterWidget  Passes the widget reference to center according to.
      * @param  pActionPool    Passes the action-pool reference.
      * @param  comGuest       Passes the com-guest reference. */
    UIFileManagerDialog(QWidget *pCenterWidget, UIActionPool *pActionPool, const CGuest &comGuest, const QString &strMachineName = QString());

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
     * @{ */
        /** Configures all. */
        virtual void configure() /* override */;
        /** Configures central-widget. */
        virtual void configureCentralWidget() /* override */;
        /** Perform final preparations. */
        virtual void finalize() /* override */;
        /** Loads dialog setting such as geometry from extradata. */
        virtual void loadSettings() /* override */;

        /** Saves dialog setting into extradata. */
        virtual void saveSettings() const /* override */;
    /** @} */

    /** @name Functions related to geometry restoration.
     * @{ */
        /** Returns whether the window should be maximized when geometry being restored. */
        virtual bool shouldBeMaximized() const /* override */;
    /** @} */

private slots:

    void sltSetCloseButtonShortCut(QKeySequence shortcut);

private:

    void manageEscapeShortCut();
    UIActionPool *m_pActionPool;
    CGuest      m_comGuest;
    QString     m_strMachineName;
};


#endif /* !FEQT_INCLUDED_SRC_guestctrl_UIFileManagerDialog_h */
