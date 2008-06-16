/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxAboutDlg class declaration
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __VBoxAboutDlg_h__
#define __VBoxAboutDlg_h__

#include "VBoxAboutDlg.gen.h"
#include "QIWithRetranslateUI.h"

class VBoxAboutDlg : public QIWithRetranslateUI<QDialog>,
                     public Ui::VBoxAboutDlg
{
    Q_OBJECT

public:

    VBoxAboutDlg (QWidget *aParent, const QString &aVersion);

protected:

    void retranslateUi();

private:

    void mouseReleaseEvent (QMouseEvent *aEvent);
};

#endif // __VBoxAboutDlg_h__

