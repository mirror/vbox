/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMainDialog class implementation
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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

    QIMainDialog (QWidget *aParent = 0, Qt::WindowFlags aFlags = Qt::Dialog);

    QDialog::DialogCode exec();
    QDialog::DialogCode result() const;

    void setFileForProxyIcon (const QString &aFile);
    QString fileForProxyIcon () const;

    void setSizeGripEnabled (bool aEnabled);
    bool isSizeGripEnabled () const;

    void setDefaultButton (QPushButton *aButton);
    QPushButton* defaultButton () const;

    void setAutoCenteringEnabled(bool fIsAutoCentering);

public slots:

    virtual void setVisible (bool aVisible);

protected:

    virtual bool event (QEvent *aEvent);
    virtual void showEvent (QShowEvent *aEvent);
    virtual void resizeEvent (QResizeEvent *aEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual bool eventFilter (QObject *aObject, QEvent *aEvent);

    QPushButton* searchDefaultButton() const;

    void centerAccording (QWidget *aWidget) { mCenterWidget = aWidget; }

protected slots:

    virtual void accept();
    virtual void reject();

    void done (QDialog::DialogCode aRescode);
    void setResult (QDialog::DialogCode aRescode);

    void openAction (QAction *aAction);

private:

    /* Private member vars */
    QDialog::DialogCode mRescode;
    QPointer<QEventLoop> mEventLoop;

    QString mFileForProxyIcon;

    QPointer<QSizeGrip> mSizeGrip;
    QPointer<QPushButton> mDefaultButton;

    bool mPolished;
    bool mIsAutoCentering;
    QWidget *mCenterWidget;
};

#endif /* __QIMainDialog_h__ */

