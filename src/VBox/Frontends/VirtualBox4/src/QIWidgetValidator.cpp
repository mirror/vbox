/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIWidgetValidator class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "QIWidgetValidator.h"

/* Qt includes */
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>

#include <iprt/assert.h>

#include "VBoxGlobal.h"

/** @class QIWidgetValidator
 *
 *  The QIWidgetValidator class is a widget validator. Its purpose is to
 *  answer the question: whether all relevant (grand) children of the given
 *  widget are valid or not. Relevant children are those widgets which
 *  receive some user input that can be checked for validity (i.e. they
 *  have a validator() method that returns a pointer to a QValidator instance
 *  used to validate widget's input). The QLineEdit class is an example of
 *  such a widget.
 *
 *  When a QIWidgetValidator instance is created it receives a pointer to
 *  a widget whose children should be checked for validity. The instance
 *  connects itself to signals emitted by input widgets when their contents
 *  changes, and emits its own signal, validityChanged() (common for all
 *  child widgets being observed), whenever the change happens.
 *
 *  Objects that want to know when the validity state changes should connect
 *  themselves to the validityChanged() signal and then use the isValid()
 *  method to determine whether all children are valid or not.
 *
 *  It is assumed that all children that require validity checks have been
 *  already added to the widget when it is passed to the QIWidgetValidator
 *  constructor. If other children (that need to be validated) are added
 *  later, it's necessary to call the rescan() method to rescan the widget
 *  hierarchy again, otherwise the results of validity checks are undefined.
 *
 *  It's also necessary to call the revalidate() method every time the
 *  enabled state of the child widget is changed, because QIWidgetValidator
 *  skips disabled widgets when it calculates the combined validity state.
 *
 *  This class is useful for example for QWizard dialogs, where a separate
 *  instance validates every page of the wizard that has children to validate,
 *  and the validityChanged() signal of every instance is connected to some
 *  signal of the QWizard subclass, that enables or disables the Next button,
 *  depending on the result of the validity check.
 *
 *  Currently, only QLineEdit and QComboBox classes and their successors are
 *  recognized by QIWidgetValidator. It uses the QLineEdit::hasAcceptableInput()
 *  and QCombobox::validator() methods to determine the validity state o
 *  the corresponding widgets (note that the QComboBox widget must be editable,
 *  otherwise it will be skipped).
 */

/**
 *  Constructs a new instance that will check the validity of children
 *  of the given widget.
 *
 *  @param aWidget  Widget whose children should be checked.
 */
QIWidgetValidator::QIWidgetValidator (QWidget *aWidget, QObject *aParent)
    : QObject (aParent)
    , mWidget (aWidget)
    , mOtherValid (true)
{
    rescan();
}

/**
 *  Constructs a new instance that will check the validity of children
 *  of the given widget.
 *
 *  @param aCaption Caption to use for the warning message.
 *  @param aWidget  Widget whose children should be checked.
 */
QIWidgetValidator::QIWidgetValidator (const QString &aCaption,
                                      QWidget *aWidget, QObject *aParent)
    : QObject (aParent)
    , mCaption (aCaption)
    , mWidget (aWidget)
    , mOtherValid (true)
{
    rescan();
}

/**
 *  Destructs this validator instance.
 *  Before desctruction, the #validityChanged() signal is emitted; the
 *  value of #isValid() is always true at this time.
 */
QIWidgetValidator::~QIWidgetValidator()
{
    mWidget = 0;
    doRevalidate();
}

//
// Public members
/////////////////////////////////////////////////////////////////////////////

/** @fn QIWidgetValidator::widget() const
 *
 *  Returns a widget managed by this instance.
 */

/**
 *  Returns true if all relevant children of the widget managed by this
 *  instance are valid AND if #isOtherValid() returns true; otherwise returns
 *  false. Disabled children and children without validation
 *  are skipped and don't affect the result.
 *
 *  The method emits the #isValidRequested() signal before callig
 *  #isOtherValid(), thus giving someone an opportunity to affect its result by
 *  calling #setOtherValid() from the signal handler. Note that #isOtherValid()
 *  returns true by default, until #setOtherValid( false ) is called.
 *
 *  @note If #isOtherValid() returns true this method does a hierarchy scan, so
 *  it's a good idea to store the returned value in a local variable if needed
 *  more than once within a context of a single check.
 */
bool QIWidgetValidator::isValid() const
{
    // wgt is null, we assume we're valid
    if (!mWidget)
        return true;

    QIWidgetValidator *that = const_cast <QIWidgetValidator *> (this);
    emit that->isValidRequested (that);
    if (!isOtherValid())
        return false;

    QValidator::State state = QValidator::Acceptable;

    foreach (Watched watched, mWatched)
    {
        if (watched.widget->inherits ("QLineEdit"))
        {
            QLineEdit *le = ((QLineEdit *) watched.widget);
            Assert (le->validator());
            if (!le->validator() || !le->isEnabled())
                continue;
            QString text = le->text();
            int pos;
            state = le->validator()->validate (text, pos);
        }
        else if (watched.widget->inherits ("QComboBox"))
        {
            QComboBox *cb = ((QComboBox *) watched.widget);
            Assert (cb->validator());
            if (!cb->validator() || !cb->isEnabled())
                continue;
            QString text = cb->lineEdit()->text();
            int pos;
            state = cb->lineEdit()->validator()->validate (text, pos);
        }

        if (state != QValidator::Acceptable)
        {
            that->mLastInvalid = watched;
            that->mLastInvalid.state = state;
            return false;
        }
    }

    /* reset last invalid */
    that->mLastInvalid = Watched();
    return true;
}

/**
 *  Rescans all (grand) children of the managed widget and:
 *
 *  1) remembers all supported widgets with validators to speed up further
 *     validation;
 *
 *  2) connects itself to those that can be validated, in order to emit the
 *     validityChanged() signal to give its receiver an oportunity to do
 *     useful actions.
 *
 *  Must be called every time a child widget is added or removed.
 */
void QIWidgetValidator::rescan()
{
    if (!mWidget)
        return;

    mWatched.clear();

    Watched watched;

    QList<QWidget *> list = mWidget->findChildren<QWidget *>();
    QWidget *wgt;

    /* detect all widgets that support validation */
    QListIterator<QWidget *> it (list);
    while (it.hasNext())
    {
        wgt = it.next();

        if (QLineEdit *le = qobject_cast<QLineEdit *> (wgt))
        {
            if (!le->validator())
                continue;
            /* disconnect to avoid duplicate connections */
            disconnect (le, SIGNAL (textChanged (const QString &)),
                        this, SLOT (doRevalidate()));
            connect (le, SIGNAL (textChanged (const QString &)),
                     this, SLOT (doRevalidate()));
        }
        else if (QComboBox *cb = qobject_cast<QComboBox *> (wgt))
        {
            
            if (!cb->validator() || !cb->lineEdit())
                continue;
            /* disconnect to avoid duplicate connections */
            disconnect (cb, SIGNAL (textChanged (const QString &)),
                        this, SLOT (doRevalidate()));
            connect (cb, SIGNAL (textChanged (const QString &)),
                     this, SLOT (doRevalidate()));
        }

        watched.widget = wgt;

        /* try to find a buddy widget in order to determine the title for
         * the watched widget which is used in the warning text */
        QListIterator<QWidget *> it2 (list);
        while (it2.hasNext())
        {
            wgt = it2.next();
            if (QLabel *label = qobject_cast<QLabel *> (wgt))
                if (label->buddy() == watched.widget)
                {
                    watched.buddy = label;
                    break;
                }
        }

        /* memorize */
        mWatched << watched;
    }
}

/** 
 *  Returns a message that describes the last detected error (invalid or
 *  incomplete input).
 *
 *  This message uses the caption text passed to the constructor as a page
 *  name to refer to. If the caption is NULL, this funciton will return a null
 *  string.
 *
 *  Also, if the failed widget has a buddy widget, this buddy widget's text
 *  will be used as a field name to refer to.
 */
QString QIWidgetValidator::warningText() const
{
    /* cannot generate an informative message if no caption provided */
    if (mCaption.isEmpty())
        return QString::null;

    if (mLastInvalid.state == QValidator::Acceptable)
        return QString::null;

    AssertReturn (mLastInvalid.widget, QString::null);

    QString title;
    if (mLastInvalid.buddy != NULL)
    {
        if (mLastInvalid.buddy->inherits ("QLabel"))
            title = VBoxGlobal::
                removeAccelMark (((QLabel *) mLastInvalid.buddy)->text());
    }

    QString state;
    if (mLastInvalid.state == QValidator::Intermediate)
        state = tr ("not complete", "value state");
    else
        state = tr ("invalid", "value state");

    if (!title.isEmpty())
        return tr ("<qt>Value of the <b>%1</b> field "
                   "on the <b>%2</b> page is %3.</qt>")
            .arg (title, mCaption, state);

    return tr ("<qt>One of the values "
               "on the <b>%1</b> page is %2.</qt>")
        .arg (mCaption, state);
}

/** @fn QIWidgetValidator::setOtherValid()
 *
 *  Sets the generic validity flag to true or false depending on the
 *  argument.
 *
 *  @see #isOtherValid()
 */

/** @fn QIWidgetValidator::isOtherValid()
 *
 *  Returns the current value of the generic validity flag.
 *  Thie generic validity flag is used by #isValid() to determine
 *  the overall validity of the managed widget. This flag is true by default,
 *  until #setOtherValid( false ) is called.
 */

/** @fn QIWidgetValidator::validityChanged( const QIWidgetValidator * ) const
 *
 *  Emitted when any of the relevant children of the widget managed by this
 *  instance changes its validity state. The argument is this instance.
 */

/** @fn QIWidgetValidator::isValidRequested( const QIWidgetValidator * ) const
 *
 *  Emitted whenever #sValid() is called, right before querying the generic
 *  validity value using the #isOtherValid() method.
 *  The argument is this instance.
 */

/** @fn QIWidgetValidator::revalidate()
 *
 *  Emits the validityChanged() signal, from where receivers can use the
 *  isValid() method to check for validity.
 */


/** @class QIULongValidator
 *
 *  The QIULongValidator class is a QIntValidator-like class to validate
 *  unsigned integer numbers. As opposed to QIntValidator, this class accepts
 *  all three integer number formats: decimal, hexadecimal (the string starts
 *  with "0x") and octal (the string starts with "0").
 */

QValidator::State QIULongValidator::validate (QString &aInput, int &aPos) const
{
    Q_UNUSED (aPos);

    QString stripped = aInput.trimmed();

    if (stripped.isEmpty() ||
        stripped.toUpper() == QString ("0x").toUpper())
        return Intermediate;

    bool ok;
    ulong entered = aInput.toULong (&ok, 0);

    if (!ok)
        return Invalid;

    if (entered >= mBottom && entered <= mTop)
        return Acceptable;

    return (entered > mTop ) ? Invalid : Intermediate;
}
