/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIRichLabel class declaration
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

/*
 * This class is based on the original QLabel implementation.
 */

#ifndef __QIRichLabel_h__
#define __QIRichLabel_h__

#include "qframe.h"

class QSimpleRichText;
class QLabelPrivate;
class QAction;
class QPopupMenu;

class QIRichLabel : public QFrame
{
   Q_OBJECT
      Q_PROPERTY( QString text READ text WRITE setText )
      Q_PROPERTY( TextFormat textFormat READ textFormat WRITE setTextFormat )
      Q_PROPERTY( QPixmap pixmap READ pixmap WRITE setPixmap )
      Q_PROPERTY( bool scaledContents READ hasScaledContents WRITE setScaledContents )
      Q_PROPERTY( Alignment alignment READ alignment WRITE setAlignment )
      Q_PROPERTY( int indent READ indent WRITE setIndent )
      Q_OVERRIDE( BackgroundMode backgroundMode DESIGNABLE true )

public:
   QIRichLabel (QWidget *parent, const char* name=0, WFlags f=0);
   QIRichLabel (const QString &text, QWidget *parent, const char* name=0,
              WFlags f=0);
   QIRichLabel (QWidget *buddy, const QString &,
              QWidget *parent, const char* name=0, WFlags f=0 );
   ~QIRichLabel();

   QString      text()    const   { return ltext; }
   QPixmap     *pixmap()  const   { return lpixmap; }
   QPicture    *picture() const   { return lpicture; }
   QMovie      *movie()   const;

   TextFormat   textFormat() const;
   void         setTextFormat( TextFormat );

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
   virtual void setPicture( const QPicture & );
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
   QPicture   *lpicture;
   QMovie     *lmovie;
   QPopupMenu *popupMenu;
   QString     popupBuffer;
   QWidget    *lbuddy;
   ushort      align;
   short       extraMargin;
   uint        autoresize:1;
   uint        scaledcontents :1;
   uint        baseheight;
   TextFormat  textformat;
   QAccel     *accel;
   QLabelPrivate *d;
   QSimpleRichText *doc;
   bool        mMaxHeightMode;

   friend class QTipLabel;

   QIRichLabel( const QIRichLabel & );
   QIRichLabel &operator=( const QIRichLabel & );
};

#endif // __QIRichLabel_h__
