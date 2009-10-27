/* VBox includes */
#include "VBoxSpecialControls.h"

/* VBox includes */
#include "VBoxGlobal.h"

#ifdef VBOX_DARWIN_USE_NATIVE_CONTROLS

/********************************************************************************
 *
 * A mini cancel button in the native Cocoa version.
 *
 ********************************************************************************/
VBoxMiniCancelButton::VBoxMiniCancelButton (QWidget *aParent /* = 0 */)
  : QAbstractButton (aParent)
{
    setShortcut (QKeySequence (Qt::Key_Escape));
    mButton = new VBoxCocoaButton (VBoxCocoaButton::CancelButton, this);
    connect (mButton, SIGNAL (clicked()),
             this, SIGNAL (clicked()));
    setFixedSize (mButton->size());
}

/********************************************************************************
 *
 * A help button in the native Cocoa version.
 *
 ********************************************************************************/
VBoxHelpButton::VBoxHelpButton (QWidget *aParent /* = 0 */)
  : QPushButton (aParent)
{
    setShortcut (QKeySequence (QKeySequence::HelpContents));
    mButton = new VBoxCocoaButton (VBoxCocoaButton::HelpButton, this);
    connect (mButton, SIGNAL (clicked()),
             this, SIGNAL (clicked()));
    setFixedSize (mButton->size());
}

/********************************************************************************
 *
 * A segmented button in the native Cocoa version.
 *
 ********************************************************************************/
VBoxSegmentedButton::VBoxSegmentedButton (int aCount, QWidget *aParent /* = 0 */)
  : VBoxCocoaSegmentedButton (aCount, aParent)
{
}
/********************************************************************************
 *
 * A search field in the native Cocoa version.
 *
 ********************************************************************************/
VBoxSearchField::VBoxSearchField (QWidget *aParent /* = 0 */)
  : VBoxCocoaSearchField (aParent)
{
}

#else /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

/* Qt includes */
#include <QPainter>
#include <QBitmap>
#include <QMouseEvent>
#include <QSignalMapper>

/********************************************************************************
 *
 * A mini cancel button for the other OS's.
 *
 ********************************************************************************/
VBoxMiniCancelButton::VBoxMiniCancelButton (QWidget *aParent /* = 0 */)
  : QIWithRetranslateUI<QIToolButton> (aParent)
{
    setAutoRaise (true);
    setFocusPolicy (Qt::TabFocus);
    setShortcut (QKeySequence (Qt::Key_Escape));
    setIcon (VBoxGlobal::iconSet (":/delete_16px.png",
                                  ":/delete_dis_16px.png"));
}

/********************************************************************************
 *
 * A help button for the other OS's.
 *
 ********************************************************************************/
/* From: src/gui/styles/qmacstyle_mac.cpp */
static const int PushButtonLeftOffset = 6;
static const int PushButtonTopOffset = 4;
static const int PushButtonRightOffset = 12;
static const int PushButtonBottomOffset = 4;

VBoxHelpButton::VBoxHelpButton (QWidget *aParent /* = 0 */)
    : QIWithRetranslateUI<QPushButton> (aParent)
{
#ifdef Q_WS_MAC
    mButtonPressed = false;
    mNormalPixmap = new QPixmap (":/help_button_normal_mac_22px.png");
    mPressedPixmap = new QPixmap (":/help_button_pressed_mac_22px.png");
    mSize = mNormalPixmap->size();
    mMask = new QImage (mNormalPixmap->mask().toImage());
    mBRect = QRect (PushButtonLeftOffset,
                    PushButtonTopOffset,
                    mSize.width(),
                    mSize.height());
#endif /* Q_WS_MAC */
    /* Applying language settings */
    retranslateUi();
}

void VBoxHelpButton::initFrom (QPushButton *aOther)
{
    setIcon (aOther->icon());
    setText (aOther->text());
    setShortcut (aOther->shortcut());
    setFlat (aOther->isFlat());
    setAutoDefault (aOther->autoDefault());
    setDefault (aOther->isDefault());
    /* Applying language settings */
    retranslateUi();
}

void VBoxHelpButton::retranslateUi()
{
    QPushButton::setText (tr ("&Help"));
    if (QPushButton::shortcut().isEmpty())
        QPushButton::setShortcut (QKeySequence::HelpContents);
}

#ifdef Q_WS_MAC
VBoxHelpButton::~VBoxHelpButton()
{
    delete mNormalPixmap;
    delete mPressedPixmap;
    delete mMask;
}

QSize VBoxHelpButton::sizeHint() const
{
    return QSize (mSize.width() + PushButtonLeftOffset + PushButtonRightOffset,
                  mSize.height() + PushButtonTopOffset + PushButtonBottomOffset);
}

void VBoxHelpButton::paintEvent (QPaintEvent * /* aEvent */)
{
    QPainter painter (this);
    painter.drawPixmap (PushButtonLeftOffset, PushButtonTopOffset, mButtonPressed ? *mPressedPixmap: *mNormalPixmap);
}

bool VBoxHelpButton::hitButton (const QPoint &pos) const
{
    if (mBRect.contains (pos))
        return  mMask->pixel (pos.x() - PushButtonLeftOffset,
                              pos.y() - PushButtonTopOffset) == 0xff000000;
    else
        return false;
}

void VBoxHelpButton::mousePressEvent (QMouseEvent *aEvent)
{
    if (hitButton (aEvent->pos()))
        mButtonPressed = true;
    QPushButton::mousePressEvent (aEvent);
    update();
}

void VBoxHelpButton::mouseReleaseEvent (QMouseEvent *aEvent)
{
    QPushButton::mouseReleaseEvent (aEvent);
    mButtonPressed = false;
    update();
}

void VBoxHelpButton::leaveEvent (QEvent * aEvent)
{
    QPushButton::leaveEvent (aEvent);
    mButtonPressed = false;
    update();
}
#endif /* Q_WS_MAC */

/********************************************************************************
 *
 * A segmented button for the other OS's.
 *
 ********************************************************************************/
VBoxSegmentedButton::VBoxSegmentedButton (int aCount, QWidget *aParent /* = 0 */)
  : QWidget (aParent)
{
    mSignalMapper = new QSignalMapper (this);

    QHBoxLayout *layout = new QHBoxLayout (this);
    for (int i=0; i < aCount; ++i)
    {
        QIToolButton *button = new QIToolButton (this);
        button->setAutoRaise (true);
        button->setFocusPolicy (Qt::TabFocus);
        button->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
        mButtons.append (button);
        layout->addWidget (button);
        connect (button, SIGNAL(clicked()),
                 mSignalMapper, SLOT(map()));
        mSignalMapper->setMapping (button, i);
    }
    connect (mSignalMapper, SIGNAL(mapped(int)),
             this, SIGNAL(clicked(int)));

}

VBoxSegmentedButton::~VBoxSegmentedButton()
{
    delete mSignalMapper;
    qDeleteAll (mButtons);
}

void VBoxSegmentedButton::setTitle (int aSegment, const QString &aTitle)
{
    mButtons.at (aSegment)->setText (aTitle);
}

void VBoxSegmentedButton::setToolTip (int aSegment, const QString &aTip)
{
    mButtons.at (aSegment)->setToolTip (aTip);
}

void VBoxSegmentedButton::setIcon (int aSegment, const QIcon &aIcon)
{
    mButtons.at (aSegment)->setIcon (aIcon);
}

void VBoxSegmentedButton::setEnabled (int aSegment, bool fEnabled)
{
    mButtons.at (aSegment)->setEnabled (fEnabled);
}

void VBoxSegmentedButton::animateClick (int aSegment)
{
    mButtons.at (aSegment)->animateClick();
}

/********************************************************************************
 *
 * A search field  for the other OS's.
 *
 ********************************************************************************/
VBoxSearchField::VBoxSearchField (QWidget *aParent /* = 0 */)
  : QLineEdit (aParent)
{
    mBaseBrush = palette().base();
}

void VBoxSearchField::markError()
{
    QPalette pal = palette();
    QColor c (Qt::red);
    c.setAlphaF (0.3);
    pal.setBrush (QPalette::Base, c);
    setPalette (pal);
}

void VBoxSearchField::unmarkError()
{
    QPalette pal = palette();
    pal.setBrush (QPalette::Base, mBaseBrush);
    setPalette (pal);
}

#endif /* VBOX_DARWIN_USE_NATIVE_CONTROLS */

