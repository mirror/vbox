/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * innotek Qt extensions: QIRichLabel class implementation
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

#include "QIRichLabel.h"

#include <qpainter.h>
#include <qaccel.h>
#include <qmovie.h>
#include <qimage.h>
#include <qpicture.h>
#include <qapplication.h>
#include <qsimplerichtext.h>
#include <qstylesheet.h>
#include <qstyle.h>
#include <qregexp.h>
#include <qfocusdata.h>
#include <qtooltip.h>
#include <qpopupmenu.h>
#include <qaction.h>
#include <qclipboard.h>
#include <qcursor.h>

class QLabelPrivate
{
public:
   QLabelPrivate()
      :img (0), pix (0), valid_hints (-1)
   {}
   QImage* img; // for scaled contents
   QPixmap* pix; // for scaled contents
   QSize sh;
   QSize msh;
   int valid_hints; // stores the frameWidth() for the stored size hint, -1 otherwise
};


QIRichLabel::QIRichLabel (QWidget *parent, const char *name, WFlags f)
: QFrame (parent, name, f | WMouseNoMask)
{
   init();
}


QIRichLabel::QIRichLabel (const QString &text, QWidget *parent, const char *name,
                          WFlags f)
                          : QFrame (parent, name, f | WMouseNoMask)
{
   init();
   setText (text);
}


QIRichLabel::QIRichLabel (QWidget *buddy,  const QString &text,
                          QWidget *parent, const char *name, WFlags f)
                          : QFrame (parent, name, f | WMouseNoMask)
{
   init();
   setBuddy (buddy);
   setText (text);
}


QIRichLabel::~QIRichLabel()
{
   clearContents();
   delete d;
}


void QIRichLabel::init()
{
   mMaxHeightMode = false;
   mIsMainTip = true;
   baseheight = 0;
   lpixmap = 0;
   lmovie = 0;
   lbuddy = 0;
   accel = 0;
   lpixmap = 0;
   lpicture = 0;
   align = AlignAuto | AlignVCenter | ExpandTabs;
   extraMargin = -1;
   autoresize = FALSE;
   scaledcontents = FALSE;
   textformat = Qt::AutoText;
   doc = 0;

   d = new QLabelPrivate;

   QAction *copyAction = new QAction (this, "copyAction");
   connect (copyAction, SIGNAL (activated()),
      this, SLOT (putToClipBoard()));
   copyAction->setMenuText (tr ("Copy to clipboard"));

   popupMenu = new QPopupMenu (this, "contextMenu");
   copyAction->addTo (popupMenu);

   setMouseTracking (true);
}


void QIRichLabel::setFixedHeight (int aHeight)
{
    baseheight = aHeight;
    QFrame::setFixedHeight (baseheight);
}


void QIRichLabel::setText (const QString &text)
{
   if (ltext == text)
      return;
   QSize osh = sizeHint();
   bool hadRichtext = doc != 0;

   clearContents();
   ltext = text;

   bool useRichText = (textformat == RichText ||
      ((textformat == AutoText) && QStyleSheet::mightBeRichText (ltext)));

   // ### Setting accelerators for rich text labels will not work.
   // Eg. <b>&gt;Hello</b> will return ALT+G which is clearly
   // not intended.
   if (!useRichText) {
      int p = QAccel::shortcutKey (ltext);
      if (p) {
         if (!accel)
            accel = new QAccel (this, "accel label accel");
         accel->connectItem (accel->insertItem (p),
            this, SLOT (acceleratorSlot()));
      }
   }

   if (useRichText) {
      if (!hadRichtext)
         align |= WordBreak;
      QString t = ltext;
      if (align & AlignRight)
         t.prepend ("<div align=\"right\">");
      else if (align & AlignHCenter)
         t.prepend ("<div align=\"center\">");
      if ((align & WordBreak) == 0)
         t.prepend ("<nobr>");
      doc = new QSimpleRichText (compressText(0), font());
   }

   updateLabel (osh);

   if (mMaxHeightMode && (int)baseheight < heightForWidth (width()))
   {
       baseheight = heightForWidth (width());
       QFrame::setFixedHeight (baseheight);
   }
}


void QIRichLabel::clear()
{
   setText (QString::fromLatin1 (""));
}


void QIRichLabel::setPixmap (const QPixmap &pixmap)
{
   QSize osh = sizeHint();

   if (!lpixmap || lpixmap->serialNumber() != pixmap.serialNumber()) {
      clearContents();
      lpixmap = new QPixmap (pixmap);
   }

   if (lpixmap->depth() == 1 && !lpixmap->mask())
      lpixmap->setMask (*((QBitmap *)lpixmap));

   updateLabel (osh);
}


void QIRichLabel::setPicture (const QPicture &picture)
{
   QSize osh = sizeHint();
   clearContents();
   lpicture = new QPicture (picture);

   updateLabel (osh);
}


void QIRichLabel::setNum (int num)
{
   QString str;
   str.setNum (num);
   setText (str);
}


void QIRichLabel::setNum (double num)
{
   QString str;
   str.setNum (num);
   setText (str);
}


void QIRichLabel::setAlignment (int alignment)
{
   if (alignment == align)
      return;
   QSize osh = sizeHint();

   if (lbuddy)
      align = alignment | ShowPrefix;
   else
      align = alignment;

   QString t = ltext;
   if (!t.isNull()) {
      ltext = QString::null;
      setText (t);
   }

   updateLabel (osh);
}


void QIRichLabel::setIndent (int indent)
{
   extraMargin = indent;
   updateLabel (QSize (-1, -1));
}


void QIRichLabel::setAutoResize (bool enable)
{
   if ((bool)autoresize != enable) {
      autoresize = enable;
      if (autoresize)
         adjustSize();           // calls resize which repaints
   }
}


void QIRichLabel::setMaxHeightMode (bool aEnabled)
{
    mMaxHeightMode = aEnabled;
}


QSize QIRichLabel::sizeForWidth (int w) const
{
   QRect br;
   QPixmap *pix = pixmap();
   QPicture *pic = picture();
   QMovie *mov = movie();

   int hextra = 2 * frameWidth();
   int vextra = hextra;
   QFontMetrics fm (fontMetrics());
   int xw = fm.width ('x');
   if (!mov && !pix && !pic) {
      int m = indent();
      if (m < 0 && hextra) // no indent, but we do have a frame
         m = xw / 2 - margin();
      if (m >= 0) {
         int horizAlign = QApplication::horizontalAlignment( align );
         if ((horizAlign & AlignLeft) || (horizAlign & AlignRight))
            hextra += m;
         if ((align & AlignTop) || (align & AlignBottom))
            vextra += m;
      }
   }

   if (pix)
      br = pix->rect();
   else if (pic)
      br = pic->boundingRect();
   else if ( mov )
      br = mov->framePixmap().rect();
   else if (doc) {
      int oldW = doc->width();
      if ( align & WordBreak ) {
         if (w < 0)
            doc->adjustSize();
         else
            doc->setWidth (w-hextra - 2*3);
      }
      br = QRect (0, 0, doc->widthUsed(), doc->height());
      doc->setWidth (oldW);
   }
   else {
      bool tryWidth = (w < 0) && (align & WordBreak);
      if (tryWidth)
         w = xw * 80;
      else if (w < 0)
         w = 2000;
      w -= hextra;
      br = fm.boundingRect (0, 0, w ,2000, alignment(), text());
      if (tryWidth && br.height() < 4*fm.lineSpacing() && br.width() > w/2)
         br = fm.boundingRect (0, 0, w/2, 2000, alignment(), text());
      if (tryWidth && br.height() < 2*fm.lineSpacing() && br.width() > w/4)
         br = fm.boundingRect (0, 0, w/4, 2000, alignment(), text());
   }
   int wid = br.width() + hextra;
   int hei = br.height() + vextra;

   return QSize (wid, hei);
}


int QIRichLabel::heightForWidth (int w) const
{
   if (
      doc ||
      (align & WordBreak))
      return sizeForWidth (w).height();
   return QWidget::heightForWidth(w);
}


QSize QIRichLabel::sizeHint() const
{
   if ( d->valid_hints != frameWidth() )
      (void) QIRichLabel::minimumSizeHint();
   return d->sh;
}


QSize QIRichLabel::minimumSizeHint() const
{
   if ( d->valid_hints == frameWidth() )
      return d->msh;

   constPolish();
   d->valid_hints = frameWidth();
   d->sh = sizeForWidth (-1);
   QSize sz (-1, -1);

   if (
      !doc &&
      (align & WordBreak) == 0) {
      sz = d->sh;
   } else {
      // think about caching these for performance
      sz.rwidth() = sizeForWidth (0).width();
      sz.rheight() = sizeForWidth (QWIDGETSIZE_MAX).height();
      if (d->sh.height() < sz.height())
         sz.rheight() = d->sh.height();
   }
   if (sizePolicy().horData() == QSizePolicy::Ignored)
      sz.rwidth() = -1;
   if (sizePolicy().verData() == QSizePolicy::Ignored)
      sz.rheight() = -1;
   d->msh = sz;
   return sz;
}


void QIRichLabel::mouseMoveEvent (QMouseEvent *aEvent)
{
    if (!doc) return;

    QString link = doc->anchorAt (aEvent->pos());
    if (!link.isEmpty()) /* Mouse cursor above link */
    {
        if (mIsMainTip)
        {
            mTipText = QToolTip::textFor (this);
            QToolTip::remove (this);
            QToolTip::add (this, link);
            mIsMainTip = false;
        }
        setCursor (QCursor (Qt::PointingHandCursor));
    }
    else /* Mouse cursor above non-link */
    {
        if (!mIsMainTip)
        {
            QToolTip::remove (this);
            QToolTip::add (this, mTipText);
            mIsMainTip = true;
        }
        setCursor (QCursor (Qt::ArrowCursor));
    }
}


void QIRichLabel::mousePressEvent (QMouseEvent *aEvent)
{
    if (!doc) return;

    QString link = doc->anchorAt (aEvent->pos());
    /* Check for mouse left button clicked on the link */
    if (!link.isEmpty() && aEvent->button() == LeftButton)
        emit clickedOnLink (link);
}


void QIRichLabel::resizeEvent (QResizeEvent *e)
{
   QFrame::resizeEvent (e);

   static const bool doc = FALSE;

   // optimize for standard labels
   if (frameShape() == NoFrame && (align & WordBreak) == 0 && !doc &&
       (e->oldSize().width() >= e->size().width() && (align & AlignLeft) == AlignLeft)
       && (e->oldSize().height() >= e->size().height() && (align & AlignTop) == AlignTop)) {
      setWFlags (WResizeNoErase);
      return;
   }

   clearWFlags (WResizeNoErase);
   QRect cr = contentsRect();
   if ( !lpixmap ||  !cr.isValid() ||
      // masked pixmaps can only reduce flicker when being top/left
      // aligned and when we do not perform scaled contents
      (lpixmap->hasAlpha() && (scaledcontents || ((align & (AlignLeft|AlignTop)) != (AlignLeft|AlignTop)))))
      return;

   setWFlags (WResizeNoErase);

   if (!scaledcontents) {
      // don't we all love QFrame? Reduce pixmap flicker
      QRegion reg = QRect (QPoint(0, 0), e->size());
      reg = reg.subtract (cr);
      int x = cr.x();
      int y = cr.y();
      int w = lpixmap->width();
      int h = lpixmap->height();
      if ((align & Qt::AlignVCenter) == Qt::AlignVCenter)
         y += cr.height()/2 - h/2;
      else if ((align & Qt::AlignBottom) == Qt::AlignBottom)
         y += cr.height() - h;
      if ((align & Qt::AlignRight) == Qt::AlignRight )
         x += cr.width() - w;
      else if ((align & Qt::AlignHCenter) == Qt::AlignHCenter )
         x += cr.width()/2 - w/2;
      if (x > cr.x())
         reg = reg.unite (QRect (cr.x(), cr.y(), x - cr.x(), cr.height()));
      if (y > cr.y())
         reg = reg.unite (QRect (cr.x(), cr.y(), cr.width(), y - cr.y()));

      if (x + w < cr.right())
         reg = reg.unite (QRect (x + w, cr.y(),  cr.right() - x - w, cr.height()));
      if (y + h < cr.bottom())
         reg = reg.unite (QRect (cr.x(), y +  h, cr.width(), cr.bottom() - y - h));

      erase (reg);
   }
}


void QIRichLabel::focusInEvent (QFocusEvent *aEvent)
{
   QFrame::focusInEvent (aEvent);
   repaint();
}


void QIRichLabel::keyPressEvent (QKeyEvent *aEvent)
{
   switch (aEvent->key())
   {
   case Qt::Key_Up:
      focusData()->home();
      focusData()->prev()->setFocus();
      break;
   case Qt::Key_Down:
      focusData()->home();
      focusData()->next()->setFocus();
      break;
   default:
      aEvent->ignore();
   }
}


void QIRichLabel::contextMenuEvent (QContextMenuEvent *aEvent)
{
    popupBuffer = doc->anchorAt (aEvent->pos());
    if (hasFocus() || !popupBuffer.isEmpty())
        popupMenu->popup (aEvent->globalPos());
}


void QIRichLabel::putToClipBoard()
{
    QString toClipBoard = ltext;

    if (popupBuffer.isEmpty())
        toClipBoard.remove (QRegExp ("<[^>]*>"));
    else
        toClipBoard = popupBuffer;

    QApplication::clipboard()->setText (toClipBoard);
}


QString QIRichLabel::compressText (int aPaneWidth) const
{
    QString allText = ltext;

    if (aPaneWidth == -1) aPaneWidth = width();
    int indentSize = fontMetrics().width ("x...x") + frameWidth() * 2;

    QStringList strList = QStringList::split ("<br>", allText);
    for (QStringList::Iterator it = strList.begin(); it != strList.end(); ++it)
    {
        QString oneString = *it;
        int oldSize = fontMetrics().width (oneString);

        int start = 0;
        int finish = 0;
        int position = 0;
        int textWidth = 0;
        do {
            QString filteredString = oneString;
            filteredString.remove (QRegExp (QString ("<[^>]+>")));
            textWidth = fontMetrics().width (filteredString);
            if (textWidth + indentSize > aPaneWidth)
            {
                QRegExp regStart ("(<compact(\\s+elipsis=\"(start|middle|end)\")?>)");
                QRegExp regFinish ("</compact>");
                start  = regStart.search (oneString);
                finish = regFinish.search (oneString);
                if (start == -1 || finish == -1 || finish < start)
                   break;

                if (regStart.cap(3) == "start")
                    position = start + regStart.cap(1).length();
                else if (regStart.cap(3) == "middle" || regStart.cap(3).isEmpty())
                    position = start + regStart.cap(1).length() +
                               (finish-start-regStart.cap(1).length())/2;
                else if (regStart.cap(3) == "end")
                    position = finish - 1;
                else
                    break;

                if (position == finish ||
                    position == start + (int) regStart.cap(1).length() - 1)
                   break;
                oneString.remove (position, 1);
            }
        } while (textWidth + indentSize > aPaneWidth);
        if (position) oneString.insert (position, "...");

        int newSize = fontMetrics().width (oneString);
        if (newSize < oldSize) *it = oneString;
    }
    QString result = strList.join ("<br>");
    return result;
}


void QIRichLabel::drawContents (QPainter *p)
{
   QRect cr = contentsRect();

   QPixmap *pix = pixmap();
   QPicture *pic = picture();
   QMovie *mov = movie();

   if (!mov && !pix && !pic) {
      int m = indent();
      if (m < 0 && frameWidth()) // no indent, but we do have a frame
         m = fontMetrics().width ('x') / 2 - margin();
      if (m > 0) {
         int hAlign = QApplication::horizontalAlignment (align);
         if (hAlign & AlignLeft)
            cr.setLeft (cr.left() + m);
         if (hAlign & AlignRight)
            cr.setRight (cr.right() - m);
         if (align & AlignTop)
            cr.setTop (cr.top() + m);
         if (align & AlignBottom)
            cr.setBottom (cr.bottom() - m);
      }
   }

   if (mov) {
      // ### should add movie to qDrawItem
      QRect r = style().itemRect (p, cr, align, isEnabled(), &(mov->framePixmap()),
         QString::null);
      // ### could resize movie frame at this point
      p->drawPixmap (r.x(), r.y(), mov->framePixmap());
   }
   else
      if (doc) {
         delete doc;
         QToolTip::remove (this);
         QString filteredText = compressText();
         doc = new QSimpleRichText (filteredText, font());
         /* focus indent */
         doc->setWidth (p, cr.width() - 2*3);
         int rh = doc->height();
         int yo = 0;
         if (align & AlignVCenter)
            yo = (cr.height()-rh)/2;
         else if (align & AlignBottom)
            yo = cr.height()-rh;
         if (! isEnabled() &&
            style().styleHint (QStyle::SH_EtchDisabledText, this)) {
            QColorGroup cg = colorGroup();
            cg.setColor (QColorGroup::Text, cg.light());
            doc->draw (p, cr.x()+1, cr.y()+yo+1, cr, cg, 0);
         }

         // QSimpleRichText always draws with QColorGroup::Text as with
         // background mode PaletteBase. QIRichLabel typically has
         // background mode PaletteBackground, so we create a temporary
         // color group with the text color adjusted.
         QColorGroup cg = colorGroup();
         QBrush paper;
         if (isEnabled())
         {
            if (hasFocus())
            {
               const QColorGroup &standartGroup = QApplication::palette().active();
               cg.setColor (QColorGroup::Text,
                  standartGroup.color (QColorGroup::HighlightedText));
               paper.setColor (standartGroup.color (QColorGroup::Highlight));
               paper.setStyle (QBrush::SolidPattern);
            }
            else
               cg.setColor (QColorGroup::Text, paletteForegroundColor());
         }

         doc->draw (p, cr.x()+3, cr.y()+yo, cr, cg, &paper);
         if (hasFocus())
            style().drawPrimitive (QStyle::PE_FocusRect, p, cr, cg,
            QStyle::Style_FocusAtBorder,
            cg.highlight());

         if (filteredText != ltext)
            QToolTip::add (this, QString ("&nbsp;&nbsp;%1").arg (ltext));
      } else
         if ( pic ) {
            QRect br = pic->boundingRect();
            int rw = br.width();
            int rh = br.height();
            if ( scaledcontents ) {
               p->save();
               p->translate (cr.x(), cr.y());
               p->scale ((double)cr.width()/rw, (double)cr.height()/rh);
               p->drawPicture (-br.x(), -br.y(), *pic);
               p->restore();
            } else {
               int xo = 0;
               int yo = 0;
               if (align & AlignVCenter)
                  yo = (cr.height()-rh)/2;
               else if (align & AlignBottom)
                  yo = cr.height()-rh;
               if (align & AlignRight)
                  xo = cr.width()-rw;
               else if (align & AlignHCenter)
                  xo = (cr.width()-rw)/2;
               p->drawPicture (cr.x()+xo-br.x(), cr.y()+yo-br.y(), *pic);
            }
         } else
         {
            if (scaledcontents && pix) {
               if (!d->img)
                  d->img = new QImage (lpixmap->convertToImage());

               if (!d->pix)
                  d->pix = new QPixmap;
               if (d->pix->size() != cr.size())
                  d->pix->convertFromImage (d->img->smoothScale (cr.width(), cr.height()));
               pix = d->pix;
            }
            int alignment = align;
            if ((align & ShowPrefix) && !style().styleHint(QStyle::SH_UnderlineAccelerator, this))
               alignment |= NoAccel;
            // ordinary text or pixmap label
            style().drawItem ( p, cr, alignment, colorGroup(), isEnabled(),
               pix, ltext );
         }
}


void QIRichLabel::updateLabel (QSize oldSizeHint)
{
   d->valid_hints = -1;
   QSizePolicy policy = sizePolicy();
   bool wordBreak = align & WordBreak;
   policy.setHeightForWidth (wordBreak);
   if (policy != sizePolicy())
      setSizePolicy (policy);
   if (sizeHint() != oldSizeHint)
      updateGeometry();
   if (autoresize) {
      adjustSize();
      update (contentsRect());
   } else {
      update (contentsRect());
   }
}


void QIRichLabel::acceleratorSlot()
{
   if (!lbuddy)
      return;
   QWidget * w = lbuddy;
   while (w->focusProxy())
      w = w->focusProxy();
   if (!w->hasFocus() &&
       w->isEnabled() &&
       w->isVisible() &&
       w->focusPolicy() != NoFocus) {
      QFocusEvent::setReason (QFocusEvent::Shortcut);
      w->setFocus();
      QFocusEvent::resetReason();
   }
}


void QIRichLabel::buddyDied()
{
   lbuddy = 0;
}


void QIRichLabel::setBuddy (QWidget *buddy)
{
   if (buddy)
      setAlignment (alignment() | ShowPrefix);
   else
      setAlignment (alignment() & ~ShowPrefix);

   if (lbuddy)
      disconnect (lbuddy, SIGNAL (destroyed()), this, SLOT (buddyDied()));

   lbuddy = buddy;

   if (!lbuddy)
      return;

   if (!( textformat == RichText || (textformat == AutoText &&
      QStyleSheet::mightBeRichText(ltext))))
   {
      int p = QAccel::shortcutKey (ltext);
      if (p) {
         if (!accel)
            accel = new QAccel (this, "accel label accel");
         accel->connectItem (accel->insertItem (p),
            this, SLOT (acceleratorSlot()));
      }
   }

   connect (lbuddy, SIGNAL (destroyed()), this, SLOT (buddyDied()));
}


QWidget * QIRichLabel::buddy() const
{
   return lbuddy;
}


void QIRichLabel::movieUpdated (const QRect &rect)
{
   QMovie *mov = movie();
   if (mov && !mov->isNull()) {
      QRect r = contentsRect();
      r = style().itemRect (0, r, align, isEnabled(), &(mov->framePixmap()),
         QString::null);
      r.moveBy (rect.x(), rect.y());
      r.setWidth (QMIN (r.width(), rect.width()));
      r.setHeight (QMIN (r.height(), rect.height()));
      repaint (r, mov->framePixmap().mask() != 0);
   }
}


void QIRichLabel::movieResized (const QSize &size)
{
   d->valid_hints = -1;
   if (autoresize)
      adjustSize();
   movieUpdated (QRect (QPoint(0,0), size));
   updateGeometry();
}


void QIRichLabel::setMovie (const QMovie &movie)
{
   QSize osh = sizeHint();
   clearContents();

   lmovie = new QMovie (movie);
   lmovie->connectResize (this, SLOT (movieResized (const QSize&)));
   lmovie->connectUpdate (this, SLOT (movieUpdated (const QRect&)));

   if (!lmovie->running())   // Assume that if the movie is running,
      updateLabel (osh); // resize/update signals will come soon enough
}


void QIRichLabel::clearContents()
{
   delete doc;
   doc = 0;

   delete lpixmap;
   lpixmap = 0;

   delete lpicture;
   lpicture = 0;

   delete d->img;
   d->img = 0;

   delete d->pix;
   d->pix = 0;

   ltext = QString::null;

   if (accel)
      accel->clear();

   if (lmovie) {
      lmovie->disconnectResize (this, SLOT (movieResized (const QSize&)));
      lmovie->disconnectUpdate (this, SLOT (movieUpdated (const QRect&)));
      delete lmovie;
      lmovie = 0;
   }
}


QMovie* QIRichLabel::movie() const
{
   return lmovie;
}


Qt::TextFormat QIRichLabel::textFormat() const
{
   return textformat;
}

void QIRichLabel::setTextFormat( Qt::TextFormat format )
{
   if (format != textformat) {
      textformat = format;
      QString t = ltext;
      if (!t.isNull()) {
         ltext = QString::null;
         setText (t);
      }
   }
}


void QIRichLabel::fontChange (const QFont&)
{
   if (!ltext.isEmpty()) {
      if (doc)
         doc->setDefaultFont (font());

      updateLabel (QSize (-1, -1));
   }
}


bool QIRichLabel::hasScaledContents() const
{
   return scaledcontents;
}


void QIRichLabel::setScaledContents (bool enable)
{
   if ((bool)scaledcontents == enable)
      return;
   scaledcontents = enable;
   if (!enable) {
      delete d->img;
      d->img = 0;
      delete d->pix;
      d->pix = 0;
   }
   update (contentsRect());
}


void QIRichLabel::setFont (const QFont &f)
{
   QFrame::setFont (f);
}
