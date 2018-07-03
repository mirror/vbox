/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSelector class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumSelector_h___
#define ___UIMediumSelector_h___

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMedium.h"
#include "UIMediumDefs.h"
#include "UIMediumDetailsWidget.h"

/* Forward declarations: */
class QAction;
class QTreeWidgetItem;
class QITreeWidget;
class QITreeWidgetItem;
class QVBoxLayout;
class QIDialogButtonBox;
class UIMediumItem;
class UIMediumSearchWidget;
class UIToolBar;


/** QIDialog extension providing GUI with a dialog to select an existing medium. */
class SHARED_LIBRARY_STUFF UIMediumSelector : public QIWithRetranslateUI<QIDialog>
{

    Q_OBJECT;

signals:

public:

    UIMediumSelector(UIMediumType enmMediumType, const QString &machineName = QString(),
                     const QString &machineSettigFilePath = QString(), QWidget *pParent = 0);
    QStringList selectedMediumIds() const;

protected:

    void showEvent(QShowEvent *pEvent);

private slots:

    void sltAddMedium();
    void sltCreateMedium();
    void sltHandleItemSelectionChanged();
    void sltHandleMediumEnumerationStart();
    void sltHandleMediumEnumerated();
    void sltHandleMediumEnumerationFinish();
    void sltHandleRefresh();
    void sltHandleSearchTypeChange(int type);
    void sltHandleSearchTermChange(QString searchTerm);

private:


    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
            void configure();
            void prepareWidgets();
            void prepareActions();
            void prepareConnections();
        /** Perform final preparations. */
        void finalize();
    /** @} */

    void          repopulateTreeWidget();
    /** Disable/enable 'ok' button on the basis of having a selected item */
    void          updateOkButton();
    UIMediumItem* addTreeItem(const UIMedium &medium, QITreeWidgetItem *pParent);
    void          restoreSelection(const QStringList &selectedMediums, QVector<UIMediumItem*> &mediumList);
    /** Recursively create the hard disk hierarchy under the tree widget */
    UIMediumItem* createHardDiskItem(const UIMedium &medium, QITreeWidgetItem *pParent);
    UIMediumItem* searchItem(const QTreeWidgetItem *pParent, const QString &mediumId);
    void          performMediumSearch();
    /** Remember the default foreground brush of the tree so that we can reset tree items' foreground later */
    void          saveDefaultForeground();


    QVBoxLayout          *m_pMainLayout;
    QITreeWidget         *m_pTreeWidget;
    UIMediumType          m_enmMediumType;
    QIDialogButtonBox    *m_pButtonBox;
    UIToolBar            *m_pToolBar;
    QAction              *m_pActionAdd;
    QAction              *m_pActionCreate;
    QAction              *m_pActionRefresh;
    /** All the known media that are already attached to some vm are added under the following top level tree item */
    QITreeWidgetItem     *m_pAttachedSubTreeRoot;
    /** All the known media that are not attached to any vm are added under the following top level tree item */
    QITreeWidgetItem     *m_pNotAttachedSubTreeRoot;
    QWidget              *m_pParent;
    UIMediumSearchWidget *m_pSearchWidget;
    QList<UIMediumItem*>  m_mediumItemList;
    QBrush                m_defaultItemForeground;
    QString               m_strMachineSettingsFilePath;
    QString               m_strMachineName;
};

#endif /* !___UIMediumSelector_h___ */
