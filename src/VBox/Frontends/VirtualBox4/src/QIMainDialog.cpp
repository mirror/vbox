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

#include "QIMainDialog.h"
#include "VBoxUtils.h"

#include <iprt/assert.h>

/* Qt includes */
#include <QProcess>
#include <QEventLoop>
#include <QApplication>
#include <QFileIconProvider>
#include <QDir>
#include <QUrl>
#include <QMenu>
#include <QSizeGrip>
#include <QPushButton>

QIMainDialog::QIMainDialog (QWidget *aParent /* = NULL */, Qt::WindowFlags aFlags /* = Qt::Dialog */)
    : QMainWindow (aParent, aFlags) 
    , mRescode (QDialog::Rejected)
{
}

QDialog::DialogCode QIMainDialog::exec()
{
    AssertMsg (!mEventLoop, ("exec is called recursively!\n"));

    /* Reset the result code */
    setResult (QDialog::Rejected);
    bool deleteOnClose = testAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_DeleteOnClose, false);
    /* Create a local event loop */
    mEventLoop = new QEventLoop();
    /* Show the window */
    show();
    /* A guard to ourself for the case we destroy ourself. */
    QPointer<QIMainDialog> guard = this;
    /* Start the event loop. This blocks. */
    mEventLoop->exec();
    /* Delete the event loop */
    delete mEventLoop;
    /* Are we valid anymore? */
    if (guard.isNull())
        return QDialog::Rejected;
    QDialog::DialogCode res = result();
    if (deleteOnClose)
        delete this;
    /* Return the final result */
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

QString QIMainDialog::fileForProxyIcon () const
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

bool QIMainDialog::isSizeGripEnabled () const
{
    return mSizeGrip;
}

void QIMainDialog::setVisible (bool aVisible)
{
    QMainWindow::setVisible (aVisible);
    /* Exit from the event loop if there is any and we are changing our state
     * from visible to invisible. */
    if(mEventLoop && !aVisible)
        mEventLoop->exit();
}

bool QIMainDialog::event (QEvent *aEvent)
{
#ifdef Q_WS_MAC
     switch (aEvent->type()) 
     {
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
                          data->setUrls(QList<QUrl>() << QUrl::fromLocalFile (mFileForProxyIcon));
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
                          QMenu menu(this);
                          connect(&menu, SIGNAL (triggered (QAction *)), 
                                  this, SLOT (openAction (QAction *)));
                          /* Add the file with the disk icon to the menu */
                          QFileInfo fi (mFileForProxyIcon);
                          QAction *action = menu.addAction (fi.fileName());
                          action->setIcon (windowIcon());
                          /* Create some nice looking menu out of the other
                           * directory parts. */
                          QFileIconProvider fip;
                          QDir dir (fi.absolutePath());
                          do
                          {
                              if (dir.isRoot())
                                  action = menu.addAction ("/");
                              else
                                  action = menu.addAction (dir.dirName());
                              action->setIcon (fip.icon (QFileInfo (dir, "")));
                          }
                          while (dir.cdUp());
                          /* Show the menu */
                          menu.exec (QPoint (QCursor::pos().x() - 20, frameGeometry().y() - 5));
                          return true;
                      }
                  }
                  break;
              }
          default: 
              break;
     }
#endif /* Q_WS_MAC */
     return QMainWindow::event (aEvent);
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

void QIMainDialog::keyPressEvent (QKeyEvent *aEvent)
{
#ifdef Q_WS_MAC
    if (aEvent->modifiers() == Qt::ControlModifier && 
        aEvent->key() == Qt::Key_Period) 
        reject();
    else
#endif
    if (aEvent->modifiers() == Qt::NoModifier ||
        (aEvent->modifiers() & Qt::KeypadModifier && aEvent->key() == Qt::Key_Enter))
    {
        switch (aEvent->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
                {
                    QList<QPushButton*> list = qFindChildren<QPushButton*> (this);
                    for (int i=0; i < list.size(); ++i)
                    {
                        QPushButton *pb = list.at (i);
                        if (pb->isDefault() && pb->isVisible()) 
                        {
                            if (pb->isEnabled())
                                pb->click();
                            return;
                        }
                    }
                    break;
                }
            case Qt::Key_Escape:
                {
                    reject();
                    break;
                }
        }
    }
    else
        aEvent->ignore();
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

