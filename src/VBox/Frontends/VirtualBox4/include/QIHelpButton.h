/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * QIHelpButton class declaration
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

#ifndef __QIHelpButton_h_
#define __QIHelpButton_h_

#include "QIWithRetranslateUI.h"

/* Qt includes */
#include <QPushButton>

class QIHelpButton: public QIWithRetranslateUI<QPushButton>
{
    Q_OBJECT;

public:

    QIHelpButton (QWidget *aParent = NULL);

    void initFrom (QPushButton *aOther);

#ifdef Q_WS_MAC
    ~QIHelpButton();

    QSize sizeHint() const;

protected:

    bool hitButton (const QPoint &pos) const;

    void mousePressEvent (QMouseEvent *aEvent);
    void mouseReleaseEvent (QMouseEvent *aEvent);
    void leaveEvent (QEvent *aEvent);

    void paintEvent (QPaintEvent *aEvent);
#endif /* Q_WS_MAC */

protected:

    void retranslateUi();

private:
    
#ifdef Q_WS_MAC
    /* Private member vars */
    bool mButtonPressed;

    QSize mSize;
    QPixmap *mNormalPixmap;
    QPixmap *mPressedPixmap;
    QImage *mMask;
    QRect mBRect;
#endif /* Q_WS_MAC */
};

#endif /* __QIHelpButton_h_ */

