/* $Id$ */
/** @file
 * VBox Qt GUI - UIHelpBrowserDialog class declaration.
 */

/*
 * Copyright (C) 2010-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h
#define FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QString>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QDialogButtonBox;
class QVBoxLayout;
class UIHelpBrowserDialog;


/** QIManagerDialogFactory extension used as a factory for Log Viewer dialog. */
class SHARED_LIBRARY_STUFF UIHelpBrowserDialogFactory : public QIManagerDialogFactory
{

public:

    /** @param strHelpFilePath: the full path of the qHelp archive file.
      * @param strKeyword: optional keyword string. Used in context sensitive help. */
    UIHelpBrowserDialogFactory(const QString &strHelpFilePath, const QString &strKeyword = QString());
    UIHelpBrowserDialogFactory();

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget to center wrt. pCenterWidget. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

private:

    QString m_strHelpFilePath;
    QString    m_strKeyword;
};

class SHARED_LIBRARY_STUFF UIHelpBrowserDialog : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

public:

    UIHelpBrowserDialog(QWidget *pCenterWidget, const QString &strHelpFilePath,
                        const QString &strKeyword = QString());

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
        /** Loads dialog setting from extradata. */
        virtual void loadSettings() /* override */;

        /** Saves dialog setting into extradata. */
        virtual void saveSettings() /* override */;
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

    QString m_strHelpFilePath;
    QString m_strKeyword;
};


#endif /* !FEQT_INCLUDED_SRC_helpbrowser_UIHelpBrowserDialog_h */
