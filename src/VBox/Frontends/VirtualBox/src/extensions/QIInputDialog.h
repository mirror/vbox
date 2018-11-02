/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIInputDialog class declaration.
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

#ifndef ___QIInputDialog_h___
#define ___QIInputDialog_h___

/* Qt includes: */
#include <QDialog>
#include <QPointer>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QLabel;
class QLineEdit;
class QIDialogButtonBox;

/** QDialog extension providing the GUI with
  * the advanced input dialog capabilities. */
class SHARED_LIBRARY_STUFF QIInputDialog : public QDialog
{
    Q_OBJECT;

public:

    /** Constructs the dialog passing @a pParent and @a enmFlags to the base-class. */
    QIInputDialog(QWidget *pParent = 0, Qt::WindowFlags enmFlags = 0);

    /** Returns label text. */
    QString labelText() const;
    /** Undefines label text. */
    void resetLabelText();
    /** Defines label @a strText. */
    void setLabelText(const QString &strText);

    /** Returns text value. */
    QString textValue() const;
    /** Defines @a strText value. */
    void setTextValue(const QString &strText);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Handles text value change. */
    void sltTextChanged();

private:

    /** Prepared all. */
    void prepare();

    /** Holds whether label text redefined. */
    bool  m_fDefaultLabelTextRedefined;

    /** Holds the label instance. */
    QLabel            *m_pLabel;
    /** Holds the text value editor instance. */
    QLineEdit         *m_pTextValueEditor;
    /** Holds the button-box instance. */
    QIDialogButtonBox *m_pButtonBox;
};

/** Safe pointer to the QIInputDialog class. */
typedef QPointer<QIInputDialog> QISafePointerInputDialog;

#endif /* !___QIInputDialog_h___ */
