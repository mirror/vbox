/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIDialogButtonBox class declaration.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIDialogButtonBox_h___
#define ___QIDialogButtonBox_h___

/* Qt includes: */
#include <QDialogButtonBox>
#include <QPointer>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QBoxLayout;
class QPushButton;
class UIHelpButton;

/** QDialogButtonBox subclass extending standard functionality. */
class SHARED_LIBRARY_STUFF QIDialogButtonBox : public QIWithRetranslateUI<QDialogButtonBox>
{
    Q_OBJECT;

public:

    /** Constructs dialog-button-box passing @a pParent to the base-class. */
    QIDialogButtonBox(QWidget *pParent = 0);
    /** Constructs dialog-button-box passing @a pParent to the base-class.
      * @param  enmOrientation  Brings the button-box orientation. */
    QIDialogButtonBox(Qt::Orientation enmOrientation, QWidget *pParent = 0);
    /** Constructs dialog-button-box passing @a pParent to the base-class.
      * @param  enmButtonTypes  Brings the set of button types.
      * @param  enmOrientation  Brings the button-box orientation. */
    QIDialogButtonBox(StandardButtons enmButtonTypes, Qt::Orientation enmOrientation = Qt::Horizontal, QWidget *pParent = 0);

    /** Returns the button of requested @a enmButtonType. */
    QPushButton *button(StandardButton enmButtonType) const;

    /** Adds button with passed @a strText for specified @a enmRole. */
    QPushButton *addButton(const QString &strText, ButtonRole enmRole);
    /** Adds standard button of passed @a enmButtonType. */
    QPushButton *addButton(StandardButton enmButtonType);

    /** Defines a set of standard @a enmButtonTypes. */
    void setStandardButtons(StandardButtons enmButtonTypes);

    /** Adds extra @a pWidget. */
    void addExtraWidget(QWidget *pWidget);
    /** Adds extra @a pLayout. */
    void addExtraLayout(QLayout *pLayout);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Returns button layout. */
    QBoxLayout *boxLayout() const;

    /** Searchs for empty @a pLayout space. */
    int findEmptySpace(QBoxLayout *pLayout) const;

private:

    /** Holds the Help button reference. */
    QPointer<UIHelpButton> m_pHelpButton;
};

#endif /* !___QIDialogButtonBox_h___ */
