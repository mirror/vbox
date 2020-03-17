/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMResourceMonitor class declaration.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_UIVMResourceMonitor_h
#define FEQT_INCLUDED_SRC_manager_UIVMResourceMonitor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAbstractButton;
class QTableView;
class QTreeWidgetItem;
class QIDialogButtonBox;
class UIActionPool;
class UIToolBar;
class UIVMResourceMonitorProxyModel;
class UIVMResourceMonitorModel;


/** QWidget extension to display a Linux top like utility that sort running vm wrt. resource allocations. */
class UIVMResourceMonitorWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:


public:

    UIVMResourceMonitorWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                               bool fShowToolbar = true, QWidget *pParent = 0);

    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    /** @name Event-handling stuff.
      * @{ */
        virtual void retranslateUi() /* override */;
        virtual void resizeEvent(QResizeEvent *pEvent) /* override */;
        virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** @} */

public slots:

private slots:


private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        void prepare();
        void prepareActions();
        void prepareWidgets();
        void prepareToolBar();
        void loadSettings();
    /** @} */

    /** @name General variables.
      * @{ */
        const EmbedTo m_enmEmbedding;
        UIActionPool *m_pActionPool;
        const bool    m_fShowToolbar;
    /** @} */

    /** @name Misc members.
      * @{ */
        UIToolBar *m_pToolBar;
        QTableView *m_pTableWidget;
        UIVMResourceMonitorProxyModel *m_pProxyModel;
        UIVMResourceMonitorModel      *m_pModel;
    /** @} */

};

class UIVMResourceMonitorFactory : public QIManagerDialogFactory
{
public:

    UIVMResourceMonitorFactory(UIActionPool *pActionPool = 0);

protected:

    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;
    UIActionPool *m_pActionPool;
};

class UIVMResourceMonitor : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

private:

    UIVMResourceMonitor(QWidget *pCenterWidget, UIActionPool *pActionPool);

    virtual void retranslateUi() /* override */;

    /** @name Prepare/cleanup cascade.
      * @{ */
        virtual void configure() /* override */;
        virtual void configureCentralWidget() /* override */;
        virtual void configureButtonBox() /* override */;
        virtual void finalize() /* override */;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        virtual UIVMResourceMonitorWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        UIActionPool *m_pActionPool;
    /** @} */

    friend class UIVMResourceMonitorFactory;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIVMResourceMonitor_h */
