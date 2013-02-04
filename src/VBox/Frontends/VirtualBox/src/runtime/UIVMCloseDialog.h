/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIVMCloseDialog class declaration
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __UIVMCloseDialog_h__
#define __UIVMCloseDialog_h__

#include "UIVMCloseDialog.gen.h"
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

class UIVMCloseDialog : public QIWithRetranslateUI<QIDialog>,
                        public Ui::UIVMCloseDialog
{
    Q_OBJECT;

public:

    UIVMCloseDialog(QWidget *pParent);
    ~UIVMCloseDialog();

protected:

    void retranslateUi();
};

#endif // __UIVMCloseDialog_h__

