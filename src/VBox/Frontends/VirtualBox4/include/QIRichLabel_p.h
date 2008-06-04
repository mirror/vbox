/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QILabelPrivate class declaration
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

/*
 * This class is based on the original QLabel implementation.
 */

#ifndef __QILabel_p_h__
#define __QILabel_p_h__

/* Qt includes */
#include <QLabel>

class QILabelPrivate: public QLabel
{
    Q_OBJECT;

public:

    QILabelPrivate (QWidget *aParent  = NULL, Qt::WindowFlags aFlags = 0)
        : QLabel (aParent, aFlags)
    {
        init();
    }

    QILabelPrivate (const QString &aText, QWidget *aParent = NULL, Qt::WindowFlags aFlags = 0)
        : QLabel (aParent, aFlags)
    {
        init();
        setFullText (aText);
    }

    void clearAll()
    {
        QLabel::clear();
        setFullText ("");
    }

    QString fullText () const
    {
        return mText;
    }

    void setFullText (const QString &aText)
    {
        mText = aText;
        updateText();
    }

    bool fullSizeSelection () const;
    void setFullSizeSelection (bool bOn);

protected:

    void resizeEvent (QResizeEvent *aEvent);
    void mousePressEvent (QMouseEvent *aEvent);
    void contextMenuEvent (QContextMenuEvent *aEvent);

protected slots:

    void copy();

private:

    void init();

    void updateText();

    QString removeHtmlTags (QString aText) const
    {
        /* Remove all HTML tags from the text and return it. */
        return QString(aText).remove (mCopyRegExp);
    }

    Qt::TextElideMode toTextElideMode (const QString& aStr) const
    {
        /* Converts a string to a Qt elide mode */
        Qt::TextElideMode mode = Qt::ElideNone;
        if (aStr == "start")
            mode = Qt::ElideLeft;
        else
            if (aStr == "middle")
                mode = Qt::ElideMiddle;
            else
                if (aStr == "end")
                    mode  = Qt::ElideRight;
        return mode;
    }


    QString compressText (const QString &aText) const;

    /* Private member vars */
    QString mText;
    bool mFullSizeSeclection;
    static const QRegExp mCopyRegExp;
    static QRegExp mElideRegExp;
};

#endif // __QILabel_p_h__

