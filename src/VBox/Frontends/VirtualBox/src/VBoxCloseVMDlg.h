/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxCloseVMDlg class declaration
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxCloseVMDlg_h__
#define __VBoxCloseVMDlg_h__

#include "VBoxCloseVMDlg.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

class VBoxCloseVMDlg : public QIWithRetranslateUI<QIDialog>,
                       public Ui::VBoxCloseVMDlg
{
    Q_OBJECT;

public:

    VBoxCloseVMDlg (QWidget *aParent);

protected:

    void retranslateUi();
};

#endif // __VBoxCloseVMDlg_h__

