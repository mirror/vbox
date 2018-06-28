/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
class UIVMLogViewerDialog;
class CMachine;


/** QIManagerDialogFactory  used as a factory for Virtual Media Manager dialog. */
class SHARED_LIBRARY_STUFF UIVMLogViewerDialogFactory : public QIManagerDialogFactory
{
public:
    UIVMLogViewerDialogFactory(const CMachine &machine);

protected:
    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget to center wrt. pCenterWidget. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;
    CMachine m_comMachine;
};


/** A QIDialog to display machine logs. */
class SHARED_LIBRARY_STUFF UIVMLogViewerDialog : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

public:

    UIVMLogViewerDialog(QWidget *pCenterWidget, const CMachine &machine);

protected:

    /** @name Prepare/cleanup cascade.
     * @{ */
        virtual void configure() /* override */;
        virtual void configureCentralWidget() /* override */;
        virtual void finalize() /* override */;
        virtual void saveSettings() const /* override */;
        virtual void loadSettings() /* override */;
    /** @} */
    /* Reads the related extradata to determine if the dialog should be maximized. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    void sltSetCloseButtonShortCut(QKeySequence shortCut);

private:

    void retranslateUi();
    CMachine m_comMachine;
};


#endif /* !___UIVMLogViewerDialog_h___ */
