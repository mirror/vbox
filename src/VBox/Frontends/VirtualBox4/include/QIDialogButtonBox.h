/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialogButtonBox class declaration
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __QIDialogButtonBox_h__
#define __QIDialogButtonBox_h__

#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QDialogButtonBox>

class QBoxLayout;

class QIDialogButtonBox: public QIWithRetranslateUI<QDialogButtonBox>
{
public:
    QIDialogButtonBox (QWidget *aParent = 0) :QIWithRetranslateUI<QDialogButtonBox> (aParent) {}
    QIDialogButtonBox (Qt::Orientation aOrientation, QWidget *aParent = 0) :QIWithRetranslateUI<QDialogButtonBox> (aParent) { setOrientation (aOrientation); }
    QIDialogButtonBox (StandardButtons aButtons, Qt::Orientation aOrientation = Qt::Horizontal, QWidget *aParent = 0);

    QPushButton *addButton (const QString &aText, ButtonRole aRole);
    QPushButton *addButton (StandardButton aButton);

    void setStandardButtons (StandardButtons aButtons);

    void addExtraWidget (QWidget *aWidget);
    void addExtraLayout (QLayout *aLayout);

protected:

    QBoxLayout *boxLayout() const;
    int findEmptySpace (QBoxLayout *aLayout) const;

    void retranslateUi();
};

#endif /* __QIDialogButtonBox_h__ */

