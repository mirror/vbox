/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIMainDialog class declaration.
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

#ifndef ___QIMainDialog_h___
#define ___QIMainDialog_h___

/* Qt includes: */
#include <QDialog>
#include <QMainWindow>
#include <QPointer>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QPushButton;
class QEventLoop;
class QSizeGrip;

/** QDialog analog based on QMainWindow. */
class SHARED_LIBRARY_STUFF QIMainDialog : public QMainWindow
{
    Q_OBJECT;

public:

    /** Constructs main-dialog passing @a pParent and @a fFlags to the base-class.
      * @param  fIsAutoCentering  Brigs whether this dialog should be centered according it's parent. */
    QIMainDialog(QWidget *pParent = 0,
                 Qt::WindowFlags fFlags = Qt::Dialog,
                 bool fIsAutoCentering = true);

    /** Returns the dialog's result code. */
    QDialog::DialogCode result() const { return m_enmResult; }

    /** Executes the dialog, launching local event-loop.
      * @param fApplicationModal defines whether this dialog should be modal to application or window. */
    QDialog::DialogCode exec(bool fApplicationModal = true);

    /** Returns dialog's default button. */
    QPushButton *defaultButton() const;
    /** Defines dialog's default @a pButton. */
    void setDefaultButton(QPushButton *pButton);

    /** Returns whether size-grip was enabled for that dialog. */
    bool isSizeGripEnabled() const;
    /** Defines whether size-grip should be @a fEnabled for that dialog. */
    void setSizeGripEnabled(bool fEnabled);

public slots:

    /** Defines whether the dialog is @a fVisible. */
    virtual void setVisible(bool fVisible);

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles first show @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

    /** Searches for dialog's default button. */
    QPushButton *searchDefaultButton() const;

protected slots:

    /** Sets the modal dialog's result code to @a enmResult. */
    void setResult(QDialog::DialogCode enmResult) { m_enmResult = enmResult; }

    /** Closes the modal dialog and sets its result code to @a enmResult.
      * If this dialog is shown with exec(), done() causes the local
      * event-loop to finish, and exec() to return @a enmResult. */
    virtual void done(QDialog::DialogCode enmResult);
    /** Hides the modal dialog and sets the result code to Accepted. */
    virtual void accept() { done(QDialog::Accepted); }
    /** Hides the modal dialog and sets the result code to Rejected. */
    virtual void reject() { done(QDialog::Rejected); }

private:

    /** Holds whether this dialog should be centered according it's parent. */
    const bool  m_fIsAutoCentering;
    /** Holds whether this dialog is polished. */
    bool        m_fPolished;

    /** Holds modal dialog's result code. */
    QDialog::DialogCode   m_enmResult;
    /** Holds modal dialog's event-loop. */
    QPointer<QEventLoop>  m_pEventLoop;

    /** Holds dialog's default button. */
    QPointer<QPushButton>  m_pDefaultButton;
    /** Holds dialog's size-grip. */
    QPointer<QSizeGrip>    m_pSizeGrip;
};

#endif /* !___QIMainDialog_h___ */
