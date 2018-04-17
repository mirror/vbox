/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILineEdit class declaration.
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

#ifndef ___QILineEdit_h___
#define ___QILineEdit_h___

/* Qt includes */
#include <QLineEdit>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QLineEdit extension with advanced functionality. */
class SHARED_LIBRARY_STUFF QILineEdit : public QLineEdit
{
    Q_OBJECT;

public:

    /** Constructs label-separator passing @a pParent to the base-class. */
    QILineEdit(QWidget *pParent = 0)
        : QLineEdit(pParent) {}
    /** Constructs label-separator passing @a pParent to the base-class.
      * @param  strContents  Brings the line-edit text. */
    QILineEdit(const QString &strContents, QWidget *pParent = 0)
        : QLineEdit(strContents, pParent) {}

    /** Forces line-edit to adjust minimum width acording to passed @a strText. */
    void setMinimumWidthByText(const QString &strText);
    /** Forces line-edit to adjust fixed width acording to passed @a strText. */
    void setFixedWidthByText(const QString &strText);

private:

    /** Calculates suitable @a strText size. */
    QSize featTextWidth(const QString &strText) const;
};

#endif /* !___QILineEdit_h___ */
