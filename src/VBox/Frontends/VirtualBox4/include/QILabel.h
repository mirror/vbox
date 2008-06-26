/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QILabel class declaration
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

#ifndef __QILabel_h__
#define __QILabel_h__

/* Qt includes */
#include <QFrame>

class QILabelPrivate;

class QILabel: public QWidget
{
    Q_OBJECT;

public:

    QILabel (QWidget *aParent = NULL, Qt::WindowFlags aFlags = 0);
    QILabel (const QString &aText, QWidget *aParent = NULL, Qt::WindowFlags aFlags = 0);

    /* QLabel extensions */
    bool fullSizeSelection () const;
    void setFullSizeSelection (bool bOn);

    void updateSizeHint();

    /* Default QLabel methods */
    Qt::Alignment alignment() const;
    QWidget * buddy() const;
    int frameWidth() const;
    bool hasScaledContents() const;
    int indent() const;
    int margin() const;
    QMovie *movie() const;
    bool openExternalLinks() const;
    const QPicture *picture() const;
    const QPixmap *pixmap() const;
    void setAlignment (Qt::Alignment aAlignment);
    void setBuddy (QWidget *aBuddy);
    void setFrameShadow (QFrame::Shadow aShadow);
    void setFrameShape (QFrame::Shape aShape);
    void setIndent (int aIndent);
    void setMargin (int aMargin);
    void setOpenExternalLinks (bool aOpen);
    void setScaledContents (bool aOn);
    void setTextFormat (Qt::TextFormat aFormat);
    void setTextInteractionFlags (Qt::TextInteractionFlags aFlags);
    void setWordWrap (bool aOn);
    QString text() const;
    Qt::TextFormat textFormat() const;
    Qt::TextInteractionFlags textInteractionFlags() const;
    bool wordWrap() const;

    /* Default QWidget methods */
    void setSizePolicy (QSizePolicy aPolicy);
    void setMinimumSize (const QSize &aSize);

public slots:

    void clear();
    void setMovie (QMovie *aMovie);
    void setNum (int aNum);
    void setNum (double aNum);
    void setPicture (const QPicture &aPicture);
    void setPixmap (const QPixmap &aPixmap);
    void setText (const QString &aText);

signals:

      void linkActivated (const QString &);
      void linkHovered (const QString &);

protected:

    virtual void init();

    /* Protected member vars */
    QILabelPrivate *mLabel;
};

#endif // __QILabel_h__

