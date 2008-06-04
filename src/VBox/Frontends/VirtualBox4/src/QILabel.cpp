/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QILabel class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

/*
 * This class is based on the original QLabel implementation.
 */

#include "QILabel.h"
#include "QILabel_p.h"
#include "VBoxGlobal.h"

/* Qt includes */
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>

/* @todo: Compare the minimal size behavior in the qt3 & qt4 version. */

QILabel::QILabel (QWidget *aParent /* = NULL */, Qt::WindowFlags aFlags /* = 0 */)
    : QWidget (aParent, aFlags)
    , mLabel (NULL)
{
    init();
}

QILabel::QILabel (const QString &aText, QWidget *aParent /* = NULL */, Qt::WindowFlags aFlags /* = 0 */)
    : QWidget (aParent, aFlags)
    , mLabel (NULL)
{
    init();
    mLabel->setFullText (aText);
}

bool QILabel::fullSizeSelection () const
{
    return mLabel->fullSizeSelection();
}

void QILabel::setFullSizeSelection (bool bOn)
{
    mLabel->setFullSizeSelection (bOn);
}

void QILabel::updateSizeHint()
{
    mLabel->updateSizeHint();
}

void QILabel::init()
{
    QHBoxLayout *layout = new QHBoxLayout (this);
    VBoxGlobal::setLayoutMargin (layout, 0);
    layout->setSpacing (0);
    mLabel = new QILabelPrivate (this);
    layout->addWidget (mLabel);
    connect (mLabel, SIGNAL (linkActivated (const QString &)),
             this, SIGNAL (linkActivated (const QString &)));
    connect (mLabel, SIGNAL (linkHovered (const QString &)),
             this, SIGNAL (linkHovered (const QString &)));
}

/* Default QLabel methods */

Qt::Alignment QILabel::alignment () const
{
    return mLabel->alignment();
}

QWidget *QILabel::buddy () const
{
    return mLabel->buddy();
}

bool QILabel::hasScaledContents () const
{
    return mLabel->hasScaledContents();
}

int QILabel::indent () const
{
    return mLabel->indent();
}

int QILabel::margin () const
{
    return mLabel->margin();
}

QMovie *QILabel::movie () const
{
    return mLabel->movie();
}

bool QILabel::openExternalLinks () const
{
    return mLabel->openExternalLinks();
}

const QPicture *QILabel::picture () const
{
    return mLabel->picture();
}

const QPixmap *QILabel::pixmap () const
{
    return mLabel->pixmap();
}

void QILabel::setAlignment (Qt::Alignment aAlignment) 
{ 
    mLabel->setAlignment (aAlignment); 
}

void QILabel::setBuddy (QWidget *aBuddy)
{
    mLabel->setBuddy (aBuddy);
}

void QILabel::setIndent (int aIndent)
{
    mLabel->setIndent (aIndent);
}

void QILabel::setMargin (int aMargin)
{
    mLabel->setMargin (aMargin);
}

void QILabel::setOpenExternalLinks (bool aOpen)
{ 
    mLabel->setOpenExternalLinks (aOpen); 
}

void QILabel::setScaledContents (bool aOn)
{
    mLabel->setScaledContents (aOn);
}

void QILabel::setTextFormat (Qt::TextFormat aFormat)
{
    mLabel->setTextFormat (aFormat);
}

void QILabel::setTextInteractionFlags (Qt::TextInteractionFlags aFlags) 
{ 
    mLabel->setTextInteractionFlags (aFlags); 
}

void QILabel::setWordWrap (bool aOn) 
{ 
    mLabel->setWordWrap (aOn); 
}

QString QILabel::text () const
{
    return mLabel->fullText();
}

Qt::TextFormat QILabel::textFormat () const
{
    return mLabel->textFormat();
}

Qt::TextInteractionFlags QILabel::textInteractionFlags () const
{
    return mLabel->textInteractionFlags();
}

bool QILabel::wordWrap () const
{
    return mLabel->wordWrap();
}

void QILabel::setSizePolicy (QSizePolicy aPolicy)
{
    mLabel->setSizePolicy (aPolicy);
    QWidget::setSizePolicy (aPolicy);
}

void QILabel::setMinimumSize (const QSize &aSize)
{
    mLabel->setMinimumSize (aSize);
    QWidget::setMinimumSize (aSize);
}

void QILabel::clear()
{
    mLabel->clearAll();
}

void QILabel::setMovie (QMovie *aMovie)
{
    mLabel->setMovie (aMovie);
}

void QILabel::setNum (int aNum)
{
    mLabel->setNum (aNum);
}

void QILabel::setNum (double aNum)
{
    mLabel->setNum (aNum);
}

void QILabel::setPicture (const QPicture &aPicture)
{
    mLabel->setPicture (aPicture);
}

void QILabel::setPixmap (const QPixmap &aPixmap)
{
    mLabel->setPixmap (aPixmap);
}

void QILabel::setText (const QString &aText)
{
    mLabel->setFullText (aText);
}

/* QILabelPrivate implementation: */

/* Some constant predefines */
const QRegExp QILabelPrivate::mCopyRegExp = QRegExp ("<[^>]*>");
QRegExp QILabelPrivate::mElideRegExp = QRegExp ("(<compact\\s+elipsis=\"(start|middle|end)\"?>([^<]+)</compact>)");

bool QILabelPrivate::fullSizeSelection () const
{
    return mFullSizeSeclection;
}

void QILabelPrivate::setFullSizeSelection (bool bOn)
{
    mFullSizeSeclection = bOn;
    if (mFullSizeSeclection)
    {
        /* Enable mouse interaction only */
        setTextInteractionFlags (Qt::LinksAccessibleByMouse);
        /* The label should be able to get the focus */
        setFocusPolicy (Qt::StrongFocus);
        /* Change the appearance in focus state a little bit */
        setStyleSheet (QString("QLabel::focus {\
                               color: palette(highlighted-text);\
                               background-color: palette(highlight);\
                               }"));
//                                 border: 1px dotted black;
    }
    else
    {
        /* Text should be selectable/copyable */
        setTextInteractionFlags (Qt::TextBrowserInteraction);
        /* No Focus an the label */
        setFocusPolicy (Qt::NoFocus);
        /* No focus style change */
        setStyleSheet ("");
    }
}

void QILabelPrivate::resizeEvent (QResizeEvent *aEvent)
{
    QLabel::resizeEvent (aEvent);
    /* Recalculate the elipsis of the text after every resize. */
    updateText();
}

void QILabelPrivate::mousePressEvent (QMouseEvent *aEvent)
{
    if (aEvent->button() == Qt::LeftButton &&
        geometry().contains (aEvent->pos()) &&
        mFullSizeSeclection) 
        mStartDraging = true;
    else
        QLabel::mousePressEvent (aEvent);
}

void QILabelPrivate::mouseReleaseEvent (QMouseEvent *aEvent)
{
    mStartDraging = false;
    QLabel::mouseReleaseEvent (aEvent);
}

void QILabelPrivate::mouseMoveEvent (QMouseEvent *aEvent)
{
    if (mStartDraging)
    {
        mStartDraging = false;
        /* Create a drag object out of the given data. */
        QDrag *drag = new QDrag (this);
        QMimeData *mimeData = new QMimeData;
        mimeData->setText (removeHtmlTags (mText));
        drag->setMimeData (mimeData);
        /* Start the dragging */
        drag->exec();
    }
    else
        QLabel::mouseMoveEvent (aEvent);
}

void QILabelPrivate::contextMenuEvent (QContextMenuEvent *aEvent)
{
    if (mFullSizeSeclection)
    {
        /* Create a context menu for the copy to clipboard action. */
        QMenu *menu = new QMenu();
        menu->addAction (tr ("&Copy") + "\t" + QString (QKeySequence (QKeySequence::Copy)), this, SLOT (copy()));
        menu->exec (aEvent->globalPos());
    }else
        QLabel::contextMenuEvent (aEvent);
}

void QILabelPrivate::copy()
{
    QString text = removeHtmlTags (mText);
    /* Copy the current text to the global and selection clipboard. */
    QApplication::clipboard()->setText (text, QClipboard::Clipboard);
    QApplication::clipboard()->setText (text, QClipboard::Selection);
}

void QILabelPrivate::init()
{
    mStartDraging = false;
    setFullSizeSelection (false);
    /* Open links with the QDesktopService */
    setOpenExternalLinks (true);
}

void QILabelPrivate::updateText()
{
    QString comp = compressText (mText);

    QLabel::setText (comp);
    /* Only set the tooltip if the text is shortened in any way. */
    if (removeHtmlTags (comp) != removeHtmlTags (mText))
        setToolTip (mText);
    else
        setToolTip ("");
}

QString QILabelPrivate::compressText (const QString &aText) const
{
    QStringList strResult;
    QFontMetrics fm = fontMetrics();
    /* Split up any multi line text */
    QStringList strList = aText.split (QRegExp ("<br */?>"));
    foreach (QString text, strList)
    {
        /* Search for the compact tag */
        if (mElideRegExp.indexIn (text) > -1)
        {
            QString workStr = text;
            /* Grep out the necessary info of the regexp */
            QString compactStr = mElideRegExp.cap (1);
            QString elideModeStr = mElideRegExp.cap (2);
            QString elideStr = mElideRegExp.cap (3);
            /* Remove the whole compact tag (also the text) */
            QString flatStr = removeHtmlTags (QString (workStr).remove (compactStr));
            /* What size will the text have without the compact text */
            int flatWidth = fm.width (flatStr);
            /* Create the shortened text */
            QString newStr = fm.elidedText (elideStr, toTextElideMode (elideModeStr), width() - flatWidth);
            /* Replace the compact part with the shortened text in the initial
             * string */
            text = QString (workStr).replace (compactStr, newStr);
            /* For debug */
//            printf ("%d, %s # %s # %s ## %s\n", pos, 
//                    qPrintable (compactStr),
//                    qPrintable (elideModeStr),
//                    qPrintable (elideStr), 
//                    qPrintable (newStr));
        }
        strResult << text;
    }
    return strResult.join ("<br />");
}

