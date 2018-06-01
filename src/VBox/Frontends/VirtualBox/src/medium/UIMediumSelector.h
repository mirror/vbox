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
class UIToolBar;


/** QIDialog extension providing GUI with the dialog to select an existing media. */
class SHARED_LIBRARY_STUFF UIMediumSelector : public QIWithRetranslateUI<QIDialog>
{

    Q_OBJECT;

signals:

public:

    UIMediumSelector(UIMediumType enmMediumType, QWidget *pParent = 0);
    QStringList selectedMediumIds() const;

private slots:

    void sltAddMedium();
    //void sltHandleCurrentItemChanged();
    void sltHandleItemSelectionChanged();
    void sltHandleMediumEnumerationStart();
    void sltHandleMediumEnumerated();
    void sltHandleMediumEnumerationFinish();
    void sltHandleRefresh();

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

    void repopulateTreeWidget();
    /** Disable/enable 'ok' button on the basis of having a selected item */
    void updateOkButton();
    UIMediumItem* addTreeItem(const UIMedium &medium, QITreeWidgetItem *pParent);
    void restoreSelection(const QStringList &selectedMediums, QVector<UIMediumItem*> &mediumList);

    QVBoxLayout       *m_pMainLayout;
    QITreeWidget      *m_pTreeWidget;
    UIMediumType       m_enmMediumType;
    QIDialogButtonBox *m_pButtonBox;
    UIToolBar         *m_pToolBar;
    QAction           *m_pActionAdd;
    QAction           *m_pActionRefresh;
    /** All the known media that are already attached to some vm are added under the following top level tree item */
    QITreeWidgetItem  *m_pAttachedSubTreeRoot;
    /** All the known media that are not attached to any vm are added under the following top level tree item */
    QITreeWidgetItem  *m_pNotAttachedSubTreeRoot;
};

#endif /* !___UIMediumSelector_h___ */
