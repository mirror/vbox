/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIMainDialog class implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIMainDialog.h"
#include "VBoxUtils.h"
#include "VBoxGlobal.h"

#include <iprt/assert.h>

/* Qt includes */
#include <QProcess>
#include <QEventLoop>
#include <QApplication>
#include <QDir>
#include <QUrl>
#include <QMenu>
#include <QSizeGrip>
#include <QPushButton>
#include <QDialogButtonBox>

QIMainDialog::QIMainDialog (QWidget *aParent /* = 0 */,
                            Qt::WindowFlags aFlags /* = Qt::Dialog */)
    : QMainWindow (aParent, aFlags)
    , mRescode (QDialog::Rejected)
    , mPolished (false)
    , mIsAutoCentering (true)
    , mCenterWidget (aParent)
{
    qApp->installEventFilter (this);
}

QDialog::DialogCode QIMainDialog::exec()
{
    /* Check for the recursive run: */
    AssertMsg(!mEventLoop, ("QIMainDialog::exec() is called recursively!\n"));

    /* Reset the result code: */
    setResult(QDialog::Rejected);

    /* Tune some attributes: */
    bool fDeleteOnClose = testAttribute(Qt::WA_DeleteOnClose); NOREF(fDeleteOnClose);
    AssertMsg(!fDeleteOnClose, ("QIMainDialog is NOT supposed to be run in 'delete-on-close' mode!"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    bool fWasShowModal = testAttribute(Qt::WA_ShowModal);
    setAttribute(Qt::WA_ShowModal, true);

    /* Create a local event-loop: */
    QEventLoop eventLoop;
    mEventLoop = &eventLoop;
    /* Show the window: */
    show();
    /* A guard to ourself for the case we destroy ourself: */
    QPointer<QIMainDialog> guard = this;
    /* Start the event-loop: */
    eventLoop.exec();
    /* Check if dialog is still valid: */
    if (guard.isNull())
        return QDialog::Rejected;
    mEventLoop = 0;
    /* Prepare result: */
    QDialog::DialogCode res = result();
    /* Restore old show-modal attribute: */
    setAttribute(Qt::WA_ShowModal, fWasShowModal);
    /* Return the final result: */
    return res;
}

QDialog::DialogCode QIMainDialog::result() const
{
    return mRescode;
}

void QIMainDialog::setFileForProxyIcon (const QString& aFile)
{
    mFileForProxyIcon = aFile;
}

QString QIMainDialog::fileForProxyIcon() const
{
    return mFileForProxyIcon;
}

void QIMainDialog::setSizeGripEnabled (bool aEnabled)
{
    if (!mSizeGrip && aEnabled)
    {
        mSizeGrip = new QSizeGrip (this);
        mSizeGrip->resize (mSizeGrip->sizeHint());
        mSizeGrip->show();
    }
    else if (mSizeGrip && !aEnabled)
        delete mSizeGrip;
}

bool QIMainDialog::isSizeGripEnabled() const
{
    return mSizeGrip;
}

void QIMainDialog::setDefaultButton (QPushButton* aButton)
{
    mDefaultButton = aButton;
}

QPushButton* QIMainDialog::defaultButton() const
{
    return mDefaultButton;
}

void QIMainDialog::setAutoCenteringEnabled(bool fIsAutoCentering)
{
    mIsAutoCentering = fIsAutoCentering;
}

void QIMainDialog::setVisible (bool aVisible)
{
    QMainWindow::setVisible (aVisible);
    /* Exit from the event loop if there is any and we are changing our state
     * from visible to invisible. */
    if (mEventLoop && !aVisible)
        mEventLoop->exit();
}

bool QIMainDialog::event (QEvent *aEvent)
{
     switch (aEvent->type())
     {
#ifdef Q_WS_MAC
          case QEvent::IconDrag:
          {
              Qt::KeyboardModifiers currentModifiers = qApp->keyboardModifiers();

              if (currentModifiers == Qt::NoModifier)
              {
                  if (!mFileForProxyIcon.isEmpty())
                  {
                      aEvent->accept();
                      /* Create a drag object we can use */
                      QDrag *drag = new QDrag (this);
                      QMimeData *data = new QMimeData();
                      /* Set the appropriate url data */
                      data->setUrls (QList<QUrl>() << QUrl::fromLocalFile (mFileForProxyIcon));
                      drag->setMimeData (data);
                      /* Make a nice looking DnD icon */
                      QFileInfo fi (mFileForProxyIcon);
                      QPixmap cursorPixmap (::darwinCreateDragPixmap (QPixmap (windowIcon().pixmap (16, 16)), fi.fileName()));
                      drag->setPixmap (cursorPixmap);
                      drag->setHotSpot (QPoint (5, cursorPixmap.height() - 5));
                      /* Start the DnD action */
                      drag->start (Qt::LinkAction | Qt::CopyAction);
                      return true;
                  }
              }
#if QT_VERSION < 0x040400
              else if (currentModifiers == Qt::ShiftModifier)
#else
              else if (currentModifiers == Qt::ControlModifier)
#endif
              {
                  if (!mFileForProxyIcon.isEmpty())
                  {
                      aEvent->accept();
                      /* Create the proxy icon menu */
                      QMenu menu (this);
                      connect (&menu, SIGNAL (triggered (QAction*)),
                               this, SLOT (openAction (QAction*)));
                      /* Add the file with the disk icon to the menu */
                      QFileInfo fi (mFileForProxyIcon);
                      QAction *action = menu.addAction (fi.fileName());
                      action->setIcon (windowIcon());
                      /* Create some nice looking menu out of the other
                       * directory parts. */
                      QDir dir (fi.absolutePath());
                      do
                      {
                          if (dir.isRoot())
                              action = menu.addAction ("/");
                          else
                              action = menu.addAction (dir.dirName());
                          action->setIcon (vboxGlobal().icon(QFileInfo (dir, "")));
                      }
                      while (dir.cdUp());
                      /* Show the menu */
                      menu.exec (QPoint (QCursor::pos().x() - 20, frameGeometry().y() - 5));
                      return true;
                  }
              }
              break;
          }
#endif /* Q_WS_MAC */
          case QEvent::Polish:
          {
              /* Initially search for the default button. */
              mDefaultButton = searchDefaultButton();
              break;
          }
          default:
              break;
     }
     return QMainWindow::event (aEvent);
}

void QIMainDialog::showEvent (QShowEvent *aEvent)
{
    QMainWindow::showEvent (aEvent);

    /* Polishing border */
    if (mPolished)
        return;
    mPolished = true;

    /* Explicit widget centering relatively to it's centering
     * widget if any or desktop if centering widget is missed. */
    if (mIsAutoCentering)
        VBoxGlobal::centerWidget (this, mCenterWidget, false);
}

void QIMainDialog::resizeEvent (QResizeEvent *aEvent)
{
    QMainWindow::resizeEvent (aEvent);

    /* Adjust the size-grip location for the current resize event */
    if (mSizeGrip)
    {
        if (isRightToLeft())
            mSizeGrip->move (rect().bottomLeft() - mSizeGrip->rect().bottomLeft());
        else
            mSizeGrip->move (rect().bottomRight() - mSizeGrip->rect().bottomRight());
        aEvent->accept();
    }
}

bool QIMainDialog::eventFilter (QObject *aObject, QEvent *aEvent)
{
    if (!isActiveWindow())
        return QMainWindow::eventFilter (aObject, aEvent);

    if (qobject_cast<QWidget*> (aObject) &&
        qobject_cast<QWidget*> (aObject)->window() != this)
        return QMainWindow::eventFilter (aObject, aEvent);

    switch (aEvent->type())
    {
        /* Auto-default button focus-in processor used to move the "default"
         * button property into the currently focused button. */
        case QEvent::FocusIn:
        {
            if (qobject_cast<QPushButton*> (aObject) &&
                (aObject->parent() == centralWidget() ||
                 qobject_cast<QDialogButtonBox*> (aObject->parent())))
            {
                qobject_cast<QPushButton*> (aObject)->setDefault (aObject != mDefaultButton);
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject == mDefaultButton);
            }
            break;
        }
        /* Auto-default button focus-out processor used to remove the "default"
         * button property from the previously focused button. */
        case QEvent::FocusOut:
        {
            if (qobject_cast<QPushButton*> (aObject) &&
                (aObject->parent() == centralWidget() ||
                 qobject_cast<QDialogButtonBox*> (aObject->parent())))
            {
                if (mDefaultButton)
                    mDefaultButton->setDefault (aObject != mDefaultButton);
                qobject_cast<QPushButton*> (aObject)->setDefault (aObject == mDefaultButton);
            }
            break;
        }
        case QEvent::KeyPress:
        {
#if defined (Q_WS_MAC) && (QT_VERSION < 0x040402)
            /* Bug in Qt below 4.4.2. The key events are send to the current
             * window even if a menu is shown & has the focus. See
             * http://trolltech.com/developer/task-tracker/index_html?method=entry&id=214681. */
            if (::darwinIsMenuOpen())
                return true;
#endif /* defined (Q_WS_MAC) && (QT_VERSION < 0x040402) */
            /* Make sure that we only proceed if no
             * popup or other modal widgets are open. */
            if (qApp->activePopupWidget() ||
                (qApp->activeModalWidget() && qApp->activeModalWidget() != this))
                break;

            QKeyEvent *event = static_cast<QKeyEvent*> (aEvent);
#ifdef Q_WS_MAC
            if (event->modifiers() == Qt::ControlModifier &&
                event->key() == Qt::Key_Period)
                reject();
            else
#endif
                if (event->modifiers() == Qt::NoModifier ||
                    (event->modifiers() & Qt::KeypadModifier && event->key() == Qt::Key_Enter))
                {
                    switch (event->key())
                    {
                        case Qt::Key_Enter:
                        case Qt::Key_Return:
                        {
                            QPushButton *currentDefault = searchDefaultButton();
                            if (currentDefault)
                            {
                                /* We handle this, so return true after
                                 * that. */
                                currentDefault->animateClick();
                                return true;
                            }
                            break;
                        }
                        case Qt::Key_Escape:
                        {
                            reject();
                            return true;
                        }
                        default:
                            break;
                    }
                }
        }
        default:
            break;
    }
    return QMainWindow::eventFilter (aObject, aEvent);
}

QPushButton* QIMainDialog::searchDefaultButton() const
{
    /* Search for the first default button in the dialog. */
    QList<QPushButton*> list = qFindChildren<QPushButton*> (this);
    foreach (QPushButton *button, list)
        if (button->isDefault() &&
            (button->parent() == centralWidget() ||
             qobject_cast<QDialogButtonBox*> (button->parent())))
            return button;
    return NULL;
}


void QIMainDialog::accept()
{
    done (QDialog::Accepted);
}

void QIMainDialog::reject()
{
    done (QDialog::Rejected);
}

void QIMainDialog::done (QDialog::DialogCode aResult)
{
    /* Set the final result */
    setResult (aResult);
    /* Hide this window */
    hide();
    /* And close the window */
    close();
}

void QIMainDialog::setResult (QDialog::DialogCode aRescode)
{
    mRescode = aRescode;
}

void QIMainDialog::openAction (QAction *aAction)
{
#ifdef Q_WS_MAC
    if (!mFileForProxyIcon.isEmpty())
    {
        QString path = mFileForProxyIcon.left (mFileForProxyIcon.indexOf (aAction->text())) + aAction->text();
        /* Check for the first item */
        if (mFileForProxyIcon != path)
        {
            /* @todo: vboxGlobal().openURL (path); should be able to open paths */
            QProcess process;
            process.start ("/usr/bin/open", QStringList() << path, QIODevice::ReadOnly);
            process.waitForFinished();
        }
    }
#else /* Q_WS_MAC */
    NOREF (aAction);
#endif /* Q_WS_MAC */
}

