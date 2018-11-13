/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
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

#ifndef ___UIGuestControlFileManagerOperationsPanel_h___
#define ___UIGuestControlFileManagerOperationsPanel_h___

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "UIGuestControlFileManagerPanel.h"

/* Forward declarations: */
class CProgress;
class QTableWidget;
class UIFileOperationProgressWidget;
class UIGuestControlFileManager;


/** UIVMLogViewerPanel extension hosting a QTableWidget which in turn has a special QWidget extension
  * to manage multiple CProgress instances. This is particulary used in monitoring file operations. */
class UIGuestControlFileManagerOperationsPanel : public UIGuestControlFileManagerPanel
{
    Q_OBJECT;

public:

    UIGuestControlFileManagerOperationsPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent);
    virtual QString panelName() const /* override */;
    void addNewProgress(const CProgress &comProgress);

protected:

    /** @name Preparation specific functions.
      * @{ */
        virtual void prepareWidgets() /* override */;
        virtual void prepareConnections() /* override */;
    /** @} */

    /** Handles the translation event. */
    void retranslateUi();

private slots:


private:

    /** @name Member variables.
      * @{ */
        QTableWidget *m_pTableWidget;
        UIFileOperationProgressWidget *m_pOperationsWidget;
    /** @} */

};

#endif /* !___UIGuestControlFileManagerOperationsPanel_h___ */
