/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIRichLabel class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef __QIRichLabel_h__
#define __QIRichLabel_h__

#include "q3frame.h"
//Added by qt3to4:
#include <Q3Accel>
#include <Q3Picture>
#include <QPixmap>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <Q3PopupMenu>
#include <QFocusEvent>

class Q3SimpleRichText;
class QLabelPrivate;
class QAction;
class Q3PopupMenu;

class QIRichLabel : public Q3Frame
{
   Q_OBJECT
      Q_PROPERTY( QString text READ text WRITE setText )
      Q_PROPERTY( Qt::TextFormat textFormat READ textFormat WRITE setTextFormat )
      Q_PROPERTY( QPixmap pixmap READ pixmap WRITE setPixmap )
      Q_PROPERTY( bool scaledContents READ hasScaledContents WRITE setScaledContents )
//      Q_PROPERTY( Qt::Alignment alignment READ alignment WRITE setAlignment )
      Q_PROPERTY( int indent READ indent WRITE setIndent )
      Q_OVERRIDE( Qt::BackgroundMode backgroundMode DESIGNABLE true )

public:
   QIRichLabel (QWidget *parent, const char* name=0, Qt::WFlags f=0);
   QIRichLabel (const QString &text, QWidget *parent, const char* name=0,
              Qt::WFlags f=0);
   QIRichLabel (QWidget *buddy, const QString &,
              QWidget *parent, const char* name=0, Qt::WFlags f=0 );
   ~QIRichLabel();

   QString      text()    const   { return ltext; }
   QPixmap     *pixmap()  const   { return lpixmap; }
   Q3Picture    *picture() const   { return lpicture; }
   QMovie      *movie()   const;

   Qt::TextFormat   textFormat() const;
   void         setTextFormat( Qt::TextFormat );

   int          alignment() const  { return align; }
   virtual void setAlignment( int );
   int          indent() const     { return extraMargin; }
   void         setIndent( int );

   bool         autoResize() const { return autoresize; }
   virtual void setAutoResize( bool );
   bool         hasScaledContents() const;
   void         setScaledContents( bool );
   QSize        sizeHint() const;
   QSize        minimumSizeHint() const;
   virtual void setBuddy( QWidget * );
   QWidget     *buddy() const;
   int          heightForWidth(int) const;

   void setFont( const QFont &f );
   void setFixedHeight (int);

   void setMaxHeightMode (bool);

public slots:
   virtual void setText( const QString &);
   virtual void setPixmap( const QPixmap & );
   virtual void setPicture( const Q3Picture & );
   virtual void setMovie( const QMovie & );
   virtual void setNum( int );
   virtual void setNum( double );
   void         clear();

protected slots:
   void         putToClipBoard();

protected:
   void         drawContents ( QPainter * );
   void         fontChange ( const QFont & );
   void         mouseMoveEvent (QMouseEvent *);
   void         mousePressEvent (QMouseEvent *);
   void         resizeEvent ( QResizeEvent* );
   void         focusInEvent ( QFocusEvent* );
   void         keyPressEvent ( QKeyEvent* );
   void         contextMenuEvent (QContextMenuEvent*);

signals:
   void         clickedOnLink (const QString&);

private slots:
   void         acceleratorSlot();
   void         buddyDied();
   void         movieUpdated(const QRect&);
   void         movieResized(const QSize&);

private:
   void        init();
   void        clearContents();
   void        updateLabel (QSize oldSizeHint);
   QString     compressText (int paneWidth = -1) const;
   QSize       sizeForWidth (int w) const;

   QString     ltext;
   QString     mTipText;
   bool        mIsMainTip;
   QPixmap    *lpixmap;
   Q3Picture   *lpicture;
   QMovie     *lmovie;
   Q3PopupMenu *popupMenu;
   QString     popupBuffer;
   QWidget    *lbuddy;
   ushort      align;
   short       extraMargin;
   uint        autoresize:1;
   uint        scaledcontents :1;
   uint        baseheight;
   Qt::TextFormat  textformat;
   Q3Accel     *accel;
   QLabelPrivate *d;
   Q3SimpleRichText *doc;
   bool        mMaxHeightMode;

   friend class QTipLabel;

   QIRichLabel( const QIRichLabel & );
   QIRichLabel &operator=( const QIRichLabel & );
};

#endif // __QIRichLabel_h__
