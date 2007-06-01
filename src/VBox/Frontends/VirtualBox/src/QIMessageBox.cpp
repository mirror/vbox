/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIMessageBox class implementation
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

#include "QIMessageBox.h"
#include "VBoxDefs.h"

#include <qpixmap.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qhbox.h>
#include <qlayout.h>

#include <qfontmetrics.h>

/** @class QIMessageBox
 *
 *  The QIMessageBox class is a message box similar to QMessageBox.
 *  It partly implements the QMessageBox interface and adds some enhanced
 *  functionality.
 */

/**
 *  This constructor is equivalent to
 *  QMessageBox::QMessageBox (const QString &, const QString &, Icon,
 *      int, int, int, QWidget *, const char *, bool, WFlags).
 *  See QMessageBox for details.
 */
QIMessageBox::QIMessageBox (
    const QString &caption, const QString &text,
    Icon icon, int button0, int button1, int button2,
    QWidget *parent, const char *name, bool modal,
    WFlags f
) :
    QDialog (
        parent, name, modal,
        f | WStyle_Customize | WStyle_NormalBorder | WStyle_Title | WStyle_SysMenu
    )
{
    setCaption (caption);

    b0 = button0;
    b1 = button1;
    b2 = button2;

    QVBoxLayout *layout = new QVBoxLayout (this);
    /* setAutoAdd() behavior is really poor (it messes up with the order
     * of widgets), never use it: layout->setAutoAdd (true); */
    layout->setMargin (11);
    layout->setSpacing (10);
    layout->setResizeMode (QLayout::Minimum);

    QHBox *main = new QHBox (this);
    main->setMargin (0);
    main->setSpacing (10);
    layout->addWidget (main);

    licon = new QLabel (main);
    licon->setPixmap (QMessageBox::standardIcon ((QMessageBox::Icon) icon));
    licon->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Minimum);
    licon->setAlignment (AlignHCenter | AlignTop);

    message = new QVBox (main);
    message->setMargin (0);
    message->setSpacing (10);

    ltext = new QLabel (text, message);
    ltext->setAlignment (AlignAuto | AlignTop | ExpandTabs | WordBreak);
    ltext->setSizePolicy (QSizePolicy::Minimum, QSizePolicy::Fixed, true);

    dbox = new QVBox (this);
    dbox->setMargin (0);
    dbox->setSpacing (10);
    layout->addWidget (dbox);

    dtext = new QTextEdit (dbox);
    {
        /* calculate the minimum size dynamically, approx. for 40 chars and
         * 6 lines */
        QFontMetrics fm = dtext->fontMetrics();
        dtext->setMinimumSize (40 * fm.width ('m'), fm.lineSpacing() * 6);
    }
    dtext->setReadOnly (true);
    dtext->setWrapPolicy (QTextEdit::AtWordOrDocumentBoundary);
    dtext->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

    /* cbflag has no parent initially, setDetailsShown() will do that */
    cbflag = new QCheckBox (0);
    cbflag->hide();

    spacer = new QSpacerItem (0, 0);
    layout->addItem (spacer);

    QHBoxLayout *buttons = new QHBoxLayout (new QWidget (this));
    layout->addWidget (buttons->mainWidget());
    buttons->setAutoAdd (true);
    buttons->setSpacing (5);

    bescape = 0;

    pb0 = createButton (buttons->mainWidget(), button0);
    if (pb0) connect (pb0, SIGNAL( clicked() ), SLOT( done0() ));
    pb1 = createButton (buttons->mainWidget(), button1);
    if (pb1) connect (pb1, SIGNAL( clicked() ), SLOT( done1() ));
    pb2 = createButton (buttons->mainWidget(), button2);
    if (pb2) connect (pb2, SIGNAL( clicked() ), SLOT( done2() ));
    buttons->setAlignment (AlignHCenter);

    setDetailsShown (false);
}

/** @fn QIMessageBox::flagText() const
 *
 *  Returns the text of the optional message box flag. If the flag is hidden
 *  (by default) a null string is returned.
 *
 *  @see #setFlagText()
 */

/**
 *  Sets the text for the optional message box flag (check box) that is
 *  displayed under the message text. Passing the null string as the argument
 *  will hide the flag. By default, the flag is hidden.
 */
void QIMessageBox::setFlagText (const QString &text)
{
    if (text.isNull()) {
        cbflag->hide();
    } else {
        cbflag->setText (text);
        cbflag->show();
        cbflag->setFocus();
    }
}

/** @fn QIMessageBox::isFlagChecked() const
 *
 *  Returns true if the optional message box flag is checked and false
 *  otherwise. By default, the flag is not checked.
 *
 *  @see #setFlagChecked()
 *  @see #setFlagText()
 */

/** @fn QIMessageBox::setFlagChecked (bool)
 *
 *  Sets the state of the optional message box flag to a value of the argument.
 *
 *  @see #isFlagChecked()
 *  @see #setFlagText()
 */

QPushButton *QIMessageBox::createButton (QWidget *parent, int button)
{
    if (button == 0)
        return 0;

    QString text;
    switch (button & ButtonMask)
    {
        case Ok:        text = tr ("OK"); break;
        case Yes:       text = tr ("Yes"); break;
        case No:        text = tr ("No"); break;
        case Cancel:    text = tr ("Cancel"); break;
        case Ignore:    text = tr ("Ignore"); break;
        default:
            AssertMsgFailed (("QIMessageBox::createButton(): type %d is not implemented",
                              button));
            return 0;
    }

    QPushButton *b = new QPushButton (text, parent);

    if (button & Default)
    {
        b->setDefault (true);
        b->setFocus();
    }
    else
    if (button & Escape)
        bescape = button & ButtonMask;

    return b;
}

/** @fn QIMessageBox::detailsText() const
 *
 *  Returns the text of the optional details box. The details box is empty
 *  by default, so QString::null will be returned.
 *
 *  @see #setDetailsText()
 */

/**
 *  Sets the text for the optional details box. Note that the details box
 *  is hidden by default, call #setDetailsShown(true) to make it visible.
 *
 *  @see #detailsText()
 *  @see #setDetailsShown()
 */
void QIMessageBox::setDetailsText (const QString &text)
{
    dtext->setText (text);
}

/** @fn QIMessageBox::isDetailsShown() const
 *
 *  Returns true if the optional details box is shown and false otherwise.
 *  By default, the details box is not shown.
 *
 *  @see #setDetailsShown()
 *  @see #setDetailsText()
 */

/**
 *  Sets the visibility state of the optional details box
 *  to a value of the argument.
 *
 *  @see #isDetailsShown()
 *  @see #setDetailsText()
 */
void QIMessageBox::setDetailsShown (bool shown)
{

    bool cbflagShown = cbflag->isShown();

    if (shown)
    {
        cbflag->reparent (dbox, QPoint());
        cbflag->setShown (cbflagShown);
        spacer->changeSize (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    }

    dbox->setShown (shown);

    if (!shown)
    {
        cbflag->reparent (message, QPoint(), cbflagShown);
        cbflag->setShown (cbflagShown);
        spacer->changeSize (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    }
}

