/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMainDialog class implementation
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

#ifndef __QIMainDialog_h__
#define __QIMainDialog_h__

/* Qt includes */
#include <QMainWindow>
#include <QDialog>
#include <QPointer>

class QEventLoop;
class QSizeGrip;

class QIMainDialog: public QMainWindow
{
    Q_OBJECT;

public:

    QIMainDialog (QWidget *aParent = NULL, Qt::WindowFlags aFlags = Qt::Dialog);

    QDialog::DialogCode exec();
    QDialog::DialogCode result() const;

    void setFileForProxyIcon (const QString& aFile);
    QString fileForProxyIcon () const;

    void setSizeGripEnabled (bool aEnabled);
    bool isSizeGripEnabled () const;

public slots:

    virtual void setVisible (bool aVisible);

protected:

    virtual bool event (QEvent *aEvent);
    virtual void resizeEvent (QResizeEvent *aEvent);
    virtual void keyPressEvent (QKeyEvent *aEvent);

protected slots:

    void accept();
    void reject();
    void done (QDialog::DialogCode aRescode);

    void setResult (QDialog::DialogCode aRescode);

    void openAction (QAction *aAction);

private:

    /* Private member vars */
    QDialog::DialogCode mRescode;
    QPointer<QEventLoop> mEventLoop;

    QString mFileForProxyIcon;

    QPointer<QSizeGrip> mSizeGrip;
};

#endif /* __QIMainDialog_h__ */

