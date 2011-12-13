/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QITextEdit class declaration
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QITextEdit_h__
#define __QITextEdit_h__

/* Global includes: */
#include <QTextEdit>

/* QTextEdit class extension: */
class QITextEdit : public QTextEdit
{
    Q_OBJECT;

public:

    /* Constructor: */
    QITextEdit(QWidget *pParent = 0);

    /* Size hint: */
    QSize sizeHint() const;
};

#endif // __QITextEdit_h__
