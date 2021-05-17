/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityToolWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_activity_overview_UIVMActivityToolWidget_h
#define FEQT_INCLUDED_SRC_activity_overview_UIVMActivityToolWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QTabWidget>
#include <QUuid>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class UIActionPool;
class QIToolBar;
class UIVMActivityMonitor;
class UIVMActivityListWidget;
class UIVirtualMachineItem;
class CMachine;

/** QWidget extension to display a Linux top like utility that sort running vm wrt. resource allocations. */
class UIVMActivityToolWidget : public QIWithRetranslateUI<QTabWidget>
{
    Q_OBJECT;

signals:

    void sigSwitchToResourcesPane();

public:

    UIVMActivityToolWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool,
                               bool fShowToolbar = true, QWidget *pParent = 0);
    QMenu *menu() const;

    bool isCurrentTool() const;
    void setIsCurrentTool(bool fIsCurrentTool);

    void setSelectedVMListItems(const QList<UIVirtualMachineItem*> &items);

#ifdef VBOX_WS_MAC
    QIToolBar *toolbar() const { return m_pToolBar; }
#endif

protected:

    /** @name Event-handling stuff.
      * @{ */
        virtual void retranslateUi() /* override */;
        virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** @} */

private slots:

    void sltExportToFile();
    void sltCurrentTabChanged(int iIndex);

private:

    void setMachines(const QVector<QUuid> &machineIDs);

    /** @name Prepare/cleanup cascade.
      * @{ */
        void prepare();
        void prepareToolBar();
        void prepareActions();
        void updateColumnsMenu();
        void loadSettings();
    /** @} */

    /** Remove tabs conaining machine monitors with ids @machineIdsToRemove. */
    void removeTabs(const QVector<QUuid> &machineIdsToRemove);
    /** Add new tabs for each QUuid in @machineIdsToAdd. Does not check for duplicates. */
    void addTabs(const QVector<QUuid> &machineIdsToAdd);
    void setExportActionEnabled(bool fEnabled);

    /** @name General variables.
      * @{ */
        const EmbedTo m_enmEmbedding;
        UIActionPool *m_pActionPool;
        const bool    m_fShowToolbar;
    /** @} */

    QIToolBar *m_pToolBar;

    /** Indicates if this widget's host tool is current tool. */
    bool    m_fIsCurrentTool;
    QVector<QUuid> m_machineIds;
    QAction *m_pExportToFileAction;
};


#endif /* !FEQT_INCLUDED_SRC_activity_overview_UIVMActivityToolWidget_h */
