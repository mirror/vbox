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

#ifndef ___UIGuestControlFileManagerSessionPanel_h___
#define ___UIGuestControlFileManagerSessionPanel_h___

/* GUI includes: */
#include "UIGuestControlFileManagerPanel.h"

/* Forward declarations: */
class QTextEdit;
class UIGuestControlFileManager;
class UIGuestSessionCreateWidget;

/** UIGuestControlFileManagerPanel extension providing GUI for creating/stopping a guest session. */
class UIGuestControlFileManagerSessionPanel : public UIGuestControlFileManagerPanel
{
    Q_OBJECT;

public:

    UIGuestControlFileManagerSessionPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent);
    /** @name Enable/disable member widget wrt. guest session status.
      * @{ */
        void switchSessionCloseMode();
        void switchSessionCreateMode();
    /** @} */
    virtual QString panelName() const /* override */;

signals:

    void sigCreateSession(QString strUserName, QString strPassword);
    void sigCloseSession();

protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;
    void retranslateUi();

private:

    UIGuestSessionCreateWidget *m_pSessionCreateWidget;
};

#endif /* !___UIGuestControlFileManagerSessionPanel_h___ */
