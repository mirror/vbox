/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewerDialog class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMLogViewerDialog_h___
#define ___UIVMLogViewerDialog_h___

/* Qt includes: */
#include <QMap>
#include <QString>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QDialogButtonBox;
class QVBoxLayout;
class UIActionPool;
class UIVMLogViewerDialog;
class CMachine;


/** QIManagerDialogFactory extension used as a factory for Log Viewer dialog. */
class SHARED_LIBRARY_STUFF UIVMLogViewerDialogFactory : public QIManagerDialogFactory
{
public:

    /** Constructs Log Viewer factory acquiring additional arguments.
      * @param  pActionPool  Brings the action-pool reference.
      * @param  comMachine   Brings the machine for which VM Log-Viewer is requested. */
    UIVMLogViewerDialogFactory(UIActionPool *pActionPool = 0, const CMachine &comMachine = CMachine());

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget to center wrt. pCenterWidget. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;
    /** Holds the machine reference. */
    CMachine      m_comMachine;
};


/** QIManagerDialog extension providing GUI with the dialog displaying machine logs. */
class SHARED_LIBRARY_STUFF UIVMLogViewerDialog : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

public:

    /** Constructs Log Viewer dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to.
      * @param  pActionPool    Brings the action-pool reference.
      * @param  comMachine     Brings the machine reference. */
    UIVMLogViewerDialog(QWidget *pCenterWidget, UIActionPool *pActionPool, const CMachine &comMachine);

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

    /** Must be handles soemthing related to close @a shortcut. */
    void sltSetCloseButtonShortCut(QKeySequence shortcut);

private:

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;
    /** Holds the machine reference. */
    CMachine      m_comMachine;
};


#endif /* !___UIVMLogViewerDialog_h___ */
