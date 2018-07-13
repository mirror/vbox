/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestOSTypeSelectionButton class declaration.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestOSTypeSelectionButton_h___
#define ___UIGuestOSTypeSelectionButton_h___

/* Qt includes: */
#include <QPushButton>

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QMenu;
class QSignalMapper;

/** QPushButton sub-class for choosing guest OS family/type inside appliance editor widget. */
class UIGuestOSTypeSelectionButton : public QIWithRetranslateUI<QPushButton>
{
    Q_OBJECT;

public:

    /** Constructs a button passing @a pParent to the base-class. */
    UIGuestOSTypeSelectionButton(QWidget *pParent);

    /** Returns whether the menu is shown. */
    bool isMenuShown() const;

    /** Returns current guest OS type ID. */
    QString osTypeId() const { return m_strOSTypeId; }

public slots:

    /** Defines current guest @a strOSTypeId. */
    void setOSTypeId(const QString &strOSTypeId);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** Populates menu. */
    void populateMenu();

    /** Holds the current guest OS type ID. */
    QString  m_strOSTypeId;

    /** Holds the menu instance. */
    QMenu         *m_pMainMenu;
    /** Holds the signal mapper instance. */
    QSignalMapper *m_pSignalMapper;
};

#endif /* !___UIGuestOSTypeSelectionButton_h___ */
