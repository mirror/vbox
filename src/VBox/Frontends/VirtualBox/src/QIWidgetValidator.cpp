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
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "QIWidgetValidator.h"

#include <qobjectlist.h>
#include <qlineedit.h>
#include <qcombobox.h>

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
 *  @param widget the widget whose children should be checked
 */
QIWidgetValidator::QIWidgetValidator(
    QWidget *widget, QObject *parent, const char *name
) :
    QObject( parent, name ),
    wgt( widget ),
    otherValid( true )
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
    wgt = 0;
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
    if (!wgt)
        return true;

    QIWidgetValidator *that = const_cast <QIWidgetValidator *> (this);
    emit that->isValidRequested( that );
    if (!isOtherValid())
        return false;

    bool valid = true;

    QObjectList *list = wgt->queryList ("QLineEdit");
    QObjectListIterator it (*list);
    QObject *obj;
    while (valid && (obj = it.current()) != 0)
    {
        ++it;
        if (obj->inherits ("QLineEdit"))
        {
            QLineEdit *le = ((QLineEdit *) obj);
            if (!le->validator() || !le->isEnabled())
                continue;
            valid = le->hasAcceptableInput();
        }
        else if (obj->inherits ("QComboBox"))
        {
            QComboBox *cb = ((QComboBox *) obj);
            if (!cb->validator() || !cb->lineEdit() || !cb->isEnabled())
                continue;
            valid = cb->lineEdit()->hasAcceptableInput();
        }
    }
    delete list;

    return valid;
}

/**
 *  Rescans all (grand) children of the managed widget and connects itself to
 *  those that can be validated, in order to emit the validityChanged()
 *  signal to give its receiver an oportunity to do useful actions.
 */
void QIWidgetValidator::rescan()
{
    if (!wgt)
        return;

    QObjectList *list = wgt->queryList();
    QObjectListIterator it (*list);
    QObject *obj;
    while ((obj = it.current()) != 0)
    {
        ++ it;
        if (obj->inherits ("QLineEdit"))
        {
            QLineEdit *le = ((QLineEdit *) obj);
            if (!le->validator())
                continue;
            // disconnect to avoid duplicate connections
            disconnect (le, SIGNAL (textChanged (const QString &)),
                        this, SLOT (doRevalidate()));
            connect (le, SIGNAL (textChanged (const QString &)),
                     this, SLOT (doRevalidate()));
        }
        else if (obj->inherits ("QComboBox"))
        {
            QComboBox *cb = ((QComboBox *) obj);
            if (!cb->validator() || !cb->lineEdit())
                continue;
            // disconnect to avoid duplicate connections
            disconnect (cb, SIGNAL (textChanged (const QString &)),
                        this, SLOT (doRevalidate()));
            connect (cb, SIGNAL (textChanged (const QString &)),
                     this, SLOT (doRevalidate()));
        }
    }
    delete list;
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

