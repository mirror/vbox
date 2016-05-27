/* $Id$ */
/** @file
 * VBox Qt GUI - VBox Qt extensions: QIDialog class declaration.
 */

/*
 * Copyright (C) 2008-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __QIDialog_h__
#define __QIDialog_h__

/* Qt includes: */
#include <QDialog>
#include <QPointer>

/* Forward declarations: */
class QEventLoop;

/* Qt dialog extension: */
class QIDialog: public QDialog
{
    Q_OBJECT;

public:

    /* Constructor/destructor: */
    QIDialog(QWidget *pParent = 0, Qt::WindowFlags flags = 0);
    ~QIDialog();

    /* API: Visibility stuff: */
    void setVisible(bool fVisible);

public slots:

    /** Shows the dialog as a modal dialog, blocking until the user closes it.
      * @param  fShow              detemines whether the dialog should be shown instantly.
      * @param  fApplicationModal  determine whether the dialog should be application-modal. */
    virtual int execute(bool fShow = true, bool fApplicationModal = false);

    /** Shows the dialog as a modal dialog, blocking until the user closes it.
      * @note  Provided for compatibility with various Qt versions. */
    virtual int exec() /* overload for Qt4, override for Qt5 */ { return execute(); }

protected:

    /* Handler: Show-event stuff: */
    void showEvent(QShowEvent *pEvent);

    /* Handler: Polish-event stuff: */
    virtual void polishEvent(QShowEvent *pEvent);

private:

    /* Variables: */
    bool m_fPolished;
    QPointer<QEventLoop> m_pEventLoop;
};

typedef QPointer<QIDialog> UISafePointerDialog;

#endif /* __QIDialog_h__ */

