/* $Id$ */
/** @file
 * VBox Qt GUI - UISoftKeyboard class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QXmlStreamReader>
#include <QVBoxLayout>


/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "QIFileDialog.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UISession.h"
#include "UISoftKeyboard.h"
#include "QIToolButton.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CGuest.h"
#include "CEventSource.h"

/* Forward declarations: */
class UISoftKeyboardWidget;

enum UIKeyState
{
    UIKeyState_NotPressed,
    UIKeyState_Pressed,
    UIKeyState_Locked,
    UIKeyState_Max
};

enum UIKeyType
{
    /** Can be in UIKeyState_NotPressed and UIKeyState_Pressed states. */
    UIKeyType_Ordinary,
    /** e.g. CapsLock. Can be only in UIKeyState_NotPressed, UIKeyState_Locked */
    UIKeyType_Toggleable,
    /** e.g. Shift Can be in all 3 states*/
    UIKeyType_Modifier,
    UIKeyType_Max
};

/*********************************************************************************************************************************
*   UISoftKeyEditor definition.                                                                                  *
*********************************************************************************************************************************/

class UISoftKeyEditor : public QIWithRetranslateUI<QMainWindow>
{
    Q_OBJECT;

public:

    UISoftKeyEditor(QWidget *pParent = 0);

protected:

    virtual void retranslateUi() /* override */;

private:

    void prepareObjects();
    void prepareConnections();

    QGridLayout *m_pEditorLayout;

    QLabel *m_pBaseCaptionLabel;
    QLabel *m_pShiftCaptionLabel;
    QLabel *m_pAltGrCaptionLabel;

    QLineEdit *m_pBaseCaptionEdit;
    QLineEdit *m_pShiftCaptionEdit;
    QLineEdit *m_pAltGrCaptionEdit;

    QPushButton *m_pOkButton;
    QPushButton *m_pCancelButton;
};

/*********************************************************************************************************************************
*   UISoftKeyboardRow definition.                                                                                  *
*********************************************************************************************************************************/

/** UISoftKeyboardRow represents a row in the physical keyboard. */
class UISoftKeyboardRow
{
public:

    UISoftKeyboardRow();

    void setDefaultWidth(int iWidth);
    int defaultWidth() const;

    void setDefaultHeight(int iHeight);
    int defaultHeight() const;

    QVector<UISoftKeyboardKey> &keys();
    const QVector<UISoftKeyboardKey> &keys() const;

    void setSpaceHeightAfter(int iSpace);
    int spaceHeightAfter() const;

private:

    /** Default width and height might be inherited from the layout and overwritten in row settings. */
    int m_iDefaultWidth;
    int m_iDefaultHeight;
    QVector<UISoftKeyboardKey> m_keys;
    int m_iSpaceHeightAfter;
};

/*********************************************************************************************************************************
*   UISoftKeyboardKey definition.                                                                                  *
*********************************************************************************************************************************/

/** UISoftKeyboardKey is a place holder for a keyboard key. Graphical key represantations are drawn according to this class. */
class UISoftKeyboardKey
{
public:

    UISoftKeyboardKey();

    const QRect keyGeometry() const;
    void setKeyGeometry(const QRect &rect);

    const QString &staticKeyCap() const;
    void setStaticKeyCap(const QString &strKeyCap);

    const QString &keyCap() const;
    void setKeyCap(const QString &strKeyCap);

    void setWidth(int iWidth);
    int width() const;

    void setHeight(int iHeight);
    int height() const;

    void setScanCode(LONG scanCode);
    LONG scanCode() const;

    void setScanCodePrefix(LONG scanCode);
    LONG scanCodePrefix() const;

    void setSpaceWidthAfter(int iSpace);
    int spaceWidthAfter() const;

    void setPosition(int iPosition);
    int position() const;

    void setType(UIKeyType enmType);
    UIKeyType type() const;

    void setCutout(int iCorner, int iWidth, int iHeight);

    void setParentWidget(UISoftKeyboardWidget* pParent);
    UIKeyState state() const;
    QVector<LONG> scanCodeWithPrefix() const;

    void release();
    void press();

    void setPolygon(const QPolygon &polygon);
    const QPolygon &polygon() const;

    QPolygon polygonInGlobal() const;

    int cutoutCorner() const;
    int cutoutWidth() const;
    int cutoutHeight() const;

private:

    void updateState(bool fPressed);

    QRect      m_keyGeometry;
    /** Static keycaps are what is obtained from the layout file, as opposed to the keycaps file.
      * normally they are the captions of modifier keys (Shift, etc) whose meaning stays the same across the layouts. */
    QString    m_strStaticKeyCap;
    /** Key caps are loaded from a keycap file and preferred over the static keycaps if they are not empty. */
    QString    m_strKeyCap;

    /** Stores the key polygon in local coordinates. */
    QPolygon   m_polygon;
    UIKeyType  m_enmType;
    UIKeyState m_enmState;
    /** Key width as it is read from the xml file. */
    int        m_iWidth;
    /** Key height as it is read from the xml file. */
    int        m_iHeight;
    int        m_iSpaceWidthAfter;
    LONG       m_scanCode;
    LONG       m_scanCodePrefix;
    int        m_iCutoutWidth;
    int        m_iCutoutHeight;
    /** -1 is for no cutout. 0 is the topleft, 2 is the top right and so on. */
    int        m_iCutoutCorner;
    /** Key's position in the layout. */
    int        m_iPosition;
    UISoftKeyboardWidget  *m_pParentWidget;
};

/*********************************************************************************************************************************
*   UISoftKeyboardWidget definition.                                                                                  *
*********************************************************************************************************************************/

/** The container widget for keyboard keys. It also handles all the keyboard related events. */
class UISoftKeyboardWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    enum Mode
    {
        Mode_KeyCapEdit,
        Mode_Keyboard,
        Mode_Max
    };

signals:

    void sigPutKeyboardSequence(QVector<LONG> sequence);
    void sigLayoutChange(const QString &strLayoutName);
    void sigKeyCapChange(const QString &strKepCapFileName);

public:

    UISoftKeyboardWidget(QWidget *pParent = 0);

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;
    void keyStateChange(UISoftKeyboardKey* pKey);
    void loadDefaultLayout();
    void showContextMenu(const QPoint &globalPoint);

protected:

    void paintEvent(QPaintEvent *pEvent) /* override */;
    void mousePressEvent(QMouseEvent *pEvent) /* override */;
    void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    void mouseMoveEvent(QMouseEvent *pEvent) /* override */;
    bool eventFilter(QObject *pWatched, QEvent *pEvent)/* override */;

    virtual void retranslateUi() /* override */;

private slots:

    void sltHandleContextMenuRequest(const QPoint &point);
    void sltHandleLoadLayout(QAction *pSenderAction);
    void sltHandleSaveKeyCapFile();
    void sltHandleLoadKeyCapFile();
    void sltHandleUnloadKeyCaps();
    void sltHandleKepCapEditModeToggle(bool fToggle);

private:

    void               setNewMinimumSize(const QSize &size);
    void               setInitialSize(int iWidth, int iHeight);
    /** Searches for the key which contains the position of the @p pEvent and returns it if found. */
    UISoftKeyboardKey *keyUnderMouse(QMouseEvent *pEvent);
    UISoftKeyboardKey *keyUnderMouse(const QPoint &point);
    void               handleKeyPress(UISoftKeyboardKey *pKey);
    void               handleKeyRelease(UISoftKeyboardKey *pKey);
    bool               createKeyboard(const QString &strLayoutFileName);
    void               reset();
    void               configure();
    void               prepareObjects();
    void               enableDisableKeyCapEditMode(bool fEnable);

    UISoftKeyboardKey *m_pKeyUnderMouse;
    UISoftKeyboardKey *m_pKeyBeingEdited;

    UISoftKeyboardKey *m_pKeyPressed;
    QColor m_keyDefaultColor;
    QColor m_keyHoverColor;
    QColor m_textDefaultColor;
    QColor m_textPressedColor;
    QVector<UISoftKeyboardKey*> m_pressedModifiers;
    QVector<UISoftKeyboardRow>  m_rows;
    QStringList defaultLayouts;

    QSize m_minimumSize;
    float m_fScaleFactorX;
    float m_fScaleFactorY;
    int   m_iInitialHeight;
    int   m_iInitialWidth;
    int   m_iXSpacing;
    int   m_iYSpacing;
    int   m_iLeftMargin;
    int   m_iTopMargin;
    int   m_iRightMargin;
    int   m_iBottomMargin;
    bool  m_fInKeyCapEditMode;

    QMenu   *m_pContextMenu;
    QActionGroup *m_pLayoutActionGroup;
    QAction *m_pShowPositionsAction;
    QAction *m_pLoadLayoutFileAction;
    QAction *m_pLastSelectedLayout;
    QAction *m_pKeyCapEditModeToggleAction;
    QLineEdit *m_pKeyCapEditor;
    Mode       m_enmMode;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIKeyboardLayoutReader
{

public:

    bool parseXMLFile(const QString &strFileName, QVector<UISoftKeyboardRow> &rows);
    static QVector<QPoint> computeKeyVertices(const UISoftKeyboardKey &key);
private:

    void  parseKey(UISoftKeyboardRow &row);
    void  parseRow(int iDefaultWidth, int iDefaultHeight, QVector<UISoftKeyboardRow> &rows);
    /**  Parses the verticel space between the rows. */
    void  parseRowSpace(QVector<UISoftKeyboardRow> &rows);
    /** Parses the horizontal space between keys. */
    void  parseKeySpace(UISoftKeyboardRow &row);
    void  parseCutout(UISoftKeyboardKey &key);

    QXmlStreamReader m_xmlReader;
};

/*********************************************************************************************************************************
*   UIKeyboardKeyCapReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIKeyboardKeyCapReader
{

public:

    UIKeyboardKeyCapReader(const QString &strFileName);
    const QMap<int, QString> &keyCapMap() const;

private:

    bool  parseKeyCapFile(const QString &strFileName);
    void  parseKey();
    QXmlStreamReader m_xmlReader;
    /** Keys are positions and values are keycaps. */
    QMap<int, QString> m_keyCapMap;
};

/*********************************************************************************************************************************
*   UISoftKeyEditor implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyEditor::UISoftKeyEditor(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QMainWindow>(pParent)
    , m_pEditorLayout(0)
    , m_pBaseCaptionLabel(0)
    , m_pShiftCaptionLabel(0)
    , m_pAltGrCaptionLabel(0)
    , m_pBaseCaptionEdit(0)
    , m_pShiftCaptionEdit(0)
    , m_pAltGrCaptionEdit(0)
    , m_pOkButton(0)
    , m_pCancelButton(0)
{
    prepareObjects();
}

void UISoftKeyEditor::retranslateUi()
{
    if (m_pBaseCaptionLabel)
        m_pBaseCaptionLabel->setText(UISoftKeyboard::tr("Base Caption"));


    if (m_pShiftCaptionLabel)
        m_pShiftCaptionLabel->setText(UISoftKeyboard::tr("Shift Caption"));

    if (m_pAltGrCaptionLabel)
        m_pAltGrCaptionLabel->setText(UISoftKeyboard::tr("AltGr Caption"));
}

void UISoftKeyEditor::prepareObjects()
{
    m_pEditorLayout = new QGridLayout;

    if (!m_pEditorLayout)
        return;

    m_pBaseCaptionLabel = new QLabel;
    m_pBaseCaptionEdit = new QLineEdit;
    m_pBaseCaptionLabel->setBuddy(m_pBaseCaptionEdit);

    m_pShiftCaptionLabel = new QLabel;
    m_pShiftCaptionEdit = new QLineEdit;
    m_pShiftCaptionLabel->setBuddy(m_pShiftCaptionEdit);

    m_pAltGrCaptionLabel = new QLabel;
    m_pAltGrCaptionEdit = new QLineEdit;
    m_pAltGrCaptionLabel->setBuddy(m_pAltGrCaptionEdit);

    setLayout(m_pEditorLayout);

    retranslateUi();
}

/*********************************************************************************************************************************
*   UISoftKeyboardRow implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardRow::UISoftKeyboardRow()
    : m_iDefaultWidth(0)
    , m_iDefaultHeight(0)
    , m_iSpaceHeightAfter(0)
{
}

void UISoftKeyboardRow::setDefaultWidth(int iWidth)
{
    m_iDefaultWidth = iWidth;
}

int UISoftKeyboardRow::defaultWidth() const
{
    return m_iDefaultWidth;
}

void UISoftKeyboardRow::setDefaultHeight(int iHeight)
{
    m_iDefaultHeight = iHeight;
}

int UISoftKeyboardRow::defaultHeight() const
{
    return m_iDefaultHeight;
}

QVector<UISoftKeyboardKey> &UISoftKeyboardRow::keys()
{
    return m_keys;
}

const QVector<UISoftKeyboardKey> &UISoftKeyboardRow::keys() const
{
    return m_keys;
}

void UISoftKeyboardRow::setSpaceHeightAfter(int iSpace)
{
    m_iSpaceHeightAfter = iSpace;
}

int UISoftKeyboardRow::spaceHeightAfter() const
{
    return m_iSpaceHeightAfter;
}

/*********************************************************************************************************************************
*   UISoftKeyboardKey implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardKey::UISoftKeyboardKey()
    : m_enmType(UIKeyType_Ordinary)
    , m_enmState(UIKeyState_NotPressed)
    , m_iWidth(0)
    , m_iHeight(0)
    , m_iSpaceWidthAfter(0)
    , m_scanCode(0)
    , m_scanCodePrefix(0)
    , m_iCutoutWidth(0)
    , m_iCutoutHeight(0)
    , m_iCutoutCorner(-1)
    , m_iPosition(0)
    , m_pParentWidget(0)
{
}

const QRect UISoftKeyboardKey::keyGeometry() const
{
    return m_keyGeometry;
}

void UISoftKeyboardKey::setKeyGeometry(const QRect &rect)
{
    m_keyGeometry = rect;
}

const QString &UISoftKeyboardKey::staticKeyCap() const
{
    return m_strStaticKeyCap;
}

void UISoftKeyboardKey::setStaticKeyCap(const QString &strStaticKeyCap)
{
    m_strStaticKeyCap = strStaticKeyCap;
}

const QString &UISoftKeyboardKey::keyCap() const
{
    return m_strKeyCap;
}

void UISoftKeyboardKey::setKeyCap(const QString &strKeyCap)
{
    m_strKeyCap = strKeyCap;
}

void UISoftKeyboardKey::setWidth(int iWidth)
{
    m_iWidth = iWidth;
}

int UISoftKeyboardKey::width() const
{
    return m_iWidth;
}

void UISoftKeyboardKey::setHeight(int iHeight)
{
    m_iHeight = iHeight;
}

int UISoftKeyboardKey::height() const
{
    return m_iHeight;
}

void UISoftKeyboardKey::setScanCode(LONG scanCode)
{
    m_scanCode = scanCode;
}

LONG UISoftKeyboardKey::scanCode() const
{
    return m_scanCode;
}

void UISoftKeyboardKey::setScanCodePrefix(LONG scanCodePrefix)
{
    m_scanCodePrefix = scanCodePrefix;
}

LONG UISoftKeyboardKey::scanCodePrefix() const
{
    return m_scanCodePrefix;
}

void UISoftKeyboardKey::setSpaceWidthAfter(int iSpace)
{
    m_iSpaceWidthAfter = iSpace;
}

int UISoftKeyboardKey::spaceWidthAfter() const
{
    return m_iSpaceWidthAfter;
}

void UISoftKeyboardKey::setPosition(int iPosition)
{
    m_iPosition = iPosition;
}

int UISoftKeyboardKey::position() const
{
    return m_iPosition;
}

void UISoftKeyboardKey::setType(UIKeyType enmType)
{
    m_enmType = enmType;
}

UIKeyType UISoftKeyboardKey::type() const
{
    return m_enmType;
}

void UISoftKeyboardKey::setCutout(int iCorner, int iWidth, int iHeight)
{
    m_iCutoutCorner = iCorner;
    m_iCutoutWidth = iWidth;
    m_iCutoutHeight = iHeight;
}

UIKeyState UISoftKeyboardKey::state() const
{
    return m_enmState;
}

void UISoftKeyboardKey::setParentWidget(UISoftKeyboardWidget* pParent)
{
    m_pParentWidget = pParent;
}

void UISoftKeyboardKey::release()
{
    updateState(false);
}

void UISoftKeyboardKey::press()
{
    updateState(true);
}

void UISoftKeyboardKey::setPolygon(const QPolygon &polygon)
{
    m_polygon = polygon;
}

const QPolygon &UISoftKeyboardKey::polygon() const
{
    return m_polygon;
}

QPolygon UISoftKeyboardKey::polygonInGlobal() const
{
    QPolygon globalPolygon(m_polygon);
    globalPolygon.translate(m_keyGeometry.x(), m_keyGeometry.y());
    return globalPolygon;
}

int UISoftKeyboardKey::cutoutCorner() const
{
    return m_iCutoutCorner;
}

int UISoftKeyboardKey::cutoutWidth() const
{
    return m_iCutoutWidth;
}

int UISoftKeyboardKey::cutoutHeight() const
{
    return m_iCutoutHeight;
}

void UISoftKeyboardKey::updateState(bool fPressed)
{
    UIKeyState enmPreviousState = state();
    if (m_enmType == UIKeyType_Modifier)
    {
        if (fPressed)
        {
            if (m_enmState == UIKeyState_NotPressed)
                m_enmState = UIKeyState_Pressed;
            else if(m_enmState == UIKeyState_Pressed)
                m_enmState = UIKeyState_Locked;
            else
                m_enmState = UIKeyState_NotPressed;
        }
        else
        {
            if(m_enmState == UIKeyState_Pressed)
                m_enmState = UIKeyState_NotPressed;
        }
    }
    else if (m_enmType == UIKeyType_Toggleable)
    {
        if (fPressed)
        {
            if (m_enmState == UIKeyState_NotPressed)
                 m_enmState = UIKeyState_Locked;
            else
                m_enmState = UIKeyState_NotPressed;
        }
    }
    else if (m_enmType == UIKeyType_Ordinary)
    {
        if (m_enmState == UIKeyState_NotPressed)
            m_enmState = UIKeyState_Pressed;
        else
            m_enmState = UIKeyState_NotPressed;
    }
    if (enmPreviousState != state() && m_pParentWidget)
        m_pParentWidget->keyStateChange(this);
}

/*********************************************************************************************************************************
*   UISoftKeyboardWidget implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardWidget::UISoftKeyboardWidget(QWidget *pParent /* = 0 */)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pKeyUnderMouse(0)
    , m_pKeyBeingEdited(0)
    , m_pKeyPressed(0)
    , m_keyDefaultColor(QColor(103, 128, 159))
    , m_keyHoverColor(QColor(108, 122, 137))
    , m_textDefaultColor(QColor(46, 49, 49))
    , m_textPressedColor(QColor(149, 165, 166))
    , m_iInitialHeight(0)
    , m_iInitialWidth(0)
    , m_iXSpacing(5)
    , m_iYSpacing(5)
    , m_iLeftMargin(10)
    , m_iTopMargin(10)
    , m_iRightMargin(10)
    , m_iBottomMargin(10)
    , m_fInKeyCapEditMode(false)
    , m_pContextMenu(0)
    , m_pShowPositionsAction(0)
    , m_pLoadLayoutFileAction(0)
    , m_pLastSelectedLayout(0)
    , m_pKeyCapEditModeToggleAction(0)
    , m_enmMode(Mode_Keyboard)
{

    configure();
    prepareObjects();
}

QSize UISoftKeyboardWidget::minimumSizeHint() const
{
    return m_minimumSize;
}

QSize UISoftKeyboardWidget::sizeHint() const
{
    return m_minimumSize;
}

void UISoftKeyboardWidget::paintEvent(QPaintEvent *pEvent) /* override */
{
    Q_UNUSED(pEvent);
    if (m_iInitialWidth == 0 || m_iInitialHeight == 0)
        return;

    m_fScaleFactorX = width() / (float) m_iInitialWidth;
    m_fScaleFactorY = height() / (float) m_iInitialHeight;

    QPainter painter(this);
    QFont painterFont(font());
    painterFont.setPixelSize(15);
    painterFont.setBold(false);
    painter.setFont(painterFont);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(m_fScaleFactorX, m_fScaleFactorY);
    int unitSize = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    float fLedRadius =  0.8 * unitSize;
    float fLedMargin =  0.6 * unitSize;
    if (!m_pKeyBeingEdited && m_pKeyCapEditor)
        m_pKeyCapEditor->hide();
    for (int i = 0; i < m_rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            painter.translate(key.keyGeometry().x(), key.keyGeometry().y());
            if (&key  == m_pKeyUnderMouse)
                painter.setBrush(QBrush(m_keyHoverColor));
            else
                painter.setBrush(QBrush(m_keyDefaultColor));

            if (m_enmMode == Mode_KeyCapEdit && &key == m_pKeyBeingEdited)
            {
                m_pKeyCapEditor->setFont(painterFont);
                m_pKeyCapEditor->setGeometry(m_fScaleFactorX * key.keyGeometry().x(), m_fScaleFactorY * key.keyGeometry().y(),
                                             m_fScaleFactorX * key.keyGeometry().width(), m_fScaleFactorY * key.keyGeometry().height());
                m_pKeyCapEditor->show();
                m_pKeyCapEditor->setFocus();
            }

            if (&key  == m_pKeyPressed)
                painter.setPen(QPen(QColor(m_textPressedColor), 2));
            else
                painter.setPen(QPen(QColor(m_textDefaultColor), 2));

            painter.drawPolygon(key.polygon());


            QRect textRect(0.55 * unitSize, 1 * unitSize
                           , key.keyGeometry().width(), key.keyGeometry().height());
            if (m_pShowPositionsAction && m_pShowPositionsAction->isChecked())
                painter.drawText(textRect, Qt::TextWordWrap, QString::number(key.position()));
            else
                painter.drawText(textRect, Qt::TextWordWrap,
                                 !key.keyCap().isEmpty() ? key.keyCap() : key.staticKeyCap());

            if (key.type() != UIKeyType_Ordinary)
            {
                QColor ledColor;
                if (key.state() == UIKeyState_NotPressed)
                    ledColor = m_textDefaultColor;
                else if (key.state() == UIKeyState_Pressed)
                    ledColor = QColor(0, 255, 0);
                else
                    ledColor = QColor(255, 0, 0);
                painter.setBrush(ledColor);
                painter.setPen(ledColor);
                QRectF rectangle(key.keyGeometry().width() - 2 * fLedMargin, fLedMargin, fLedRadius, fLedRadius);
                painter.drawEllipse(rectangle);
            }
            painter.translate(-key.keyGeometry().x(), -key.keyGeometry().y());

        }
    }
}

void UISoftKeyboardWidget::mousePressEvent(QMouseEvent *pEvent)
{
    QWidget::mousePressEvent(pEvent);
    if (pEvent->button() != Qt::LeftButton)
        return;
    m_pKeyPressed = keyUnderMouse(pEvent);

    if (m_enmMode == Mode_Keyboard)
        handleKeyPress(m_pKeyPressed);
    else if (m_enmMode == Mode_KeyCapEdit)
    {
        /* If the editor is shown already for another key clicking away accepts the entered text: */
        if (m_pKeyBeingEdited && m_pKeyBeingEdited != m_pKeyUnderMouse)
        {
            printf("farkli %p\n", m_pKeyBeingEdited);
        }
        m_pKeyBeingEdited = m_pKeyUnderMouse;
        if (m_pKeyBeingEdited && m_pKeyCapEditor)
            m_pKeyCapEditor->setText(m_pKeyBeingEdited->keyCap());
    }
    update();
}

void UISoftKeyboardWidget::mouseReleaseEvent(QMouseEvent *pEvent)
{
    QWidget::mouseReleaseEvent(pEvent);
    if (pEvent->button() != Qt::LeftButton)
        return;
    if (!m_pKeyPressed)
        return;

    if (m_enmMode == Mode_Keyboard)
        handleKeyRelease(m_pKeyPressed);

    update();
    m_pKeyPressed = 0;
}

void UISoftKeyboardWidget::mouseMoveEvent(QMouseEvent *pEvent)
{
    QWidget::mouseMoveEvent(pEvent);
    keyUnderMouse(pEvent);
}

bool UISoftKeyboardWidget::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched != m_pKeyCapEditor)
        return QIWithRetranslateUI<QWidget>::eventFilter(pWatched, pEvent);

    switch (pEvent->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *pKeyEvent = dynamic_cast<QKeyEvent*>(pEvent);
            if (pKeyEvent && pKeyEvent->key() == Qt::Key_Escape)
            {
                m_pKeyBeingEdited = 0;
                update();
                return true;
            }
            else if (pKeyEvent && (pKeyEvent->key() == Qt::Key_Enter || pKeyEvent->key() == Qt::Key_Return))
            {
                if (m_pKeyBeingEdited)
                    m_pKeyBeingEdited->setStaticKeyCap(m_pKeyCapEditor->text());
                m_pKeyBeingEdited = 0;
                update();
                return true;
            }
            break;
        }
        default:
            break;
    }

    return QIWithRetranslateUI<QWidget>::eventFilter(pWatched, pEvent);
}


void UISoftKeyboardWidget::retranslateUi()
{
}

void UISoftKeyboardWidget::sltHandleContextMenuRequest(const QPoint &point)
{
    showContextMenu(mapToGlobal(point));
}

void UISoftKeyboardWidget::sltHandleLoadLayout(QAction *pSenderAction)
{
    if (!pSenderAction)
        return;
    if (pSenderAction == m_pLoadLayoutFileAction)
    {
        const QString strFileName = QIFileDialog::getOpenFileName(QString(), UISoftKeyboard::tr("XML files (*.xml)"), this,
                                                                  UISoftKeyboard::tr("Choose file to load physical keyboard layout.."));
        if (!strFileName.isEmpty() && createKeyboard(strFileName))
        {
            m_pLastSelectedLayout = pSenderAction;
            m_pLoadLayoutFileAction->setData(strFileName);
            emit sigLayoutChange(strFileName);
            return;
        }
    }
    else
    {
        QString strLayout = pSenderAction->data().toString();
        if (!strLayout.isEmpty() && createKeyboard(strLayout))
        {
            m_pLastSelectedLayout = pSenderAction;
            emit sigLayoutChange(strLayout);
            return;
        }
    }
    /* In case something went wrong try to restore the previous layout: */
    if (m_pLastSelectedLayout)
    {
        QString strLayout = m_pLastSelectedLayout->data().toString();
        createKeyboard(strLayout);
        emit sigLayoutChange(strLayout);
        m_pLastSelectedLayout->setChecked(true);
    }
}

void UISoftKeyboardWidget::sltHandleSaveKeyCapFile()
{
    QString strFileName = QIFileDialog::getSaveFileName(vboxGlobal().documentsPath(), UISoftKeyboard::tr("XML files (*.xml)"),
                                                        this, tr("Save keycaps to a file..."));
    if (strFileName.isEmpty())
        return;

    QFileInfo fileInfo(strFileName);
    if (fileInfo.suffix().compare("xml", Qt::CaseInsensitive) != 0)
        strFileName += ".xml";

    QFile xmlFile(strFileName);
    if (!xmlFile.open(QIODevice::WriteOnly))
        return;
    QXmlStreamWriter xmlWriter;
    xmlWriter.setDevice(&xmlFile);

    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument("1.0");
    xmlWriter.writeStartElement("keycaps");

   for (int i = 0; i < m_rows.size(); ++i)
   {
       QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
       for (int j = 0; j < keys.size(); ++j)
       {
           xmlWriter.writeStartElement("key");

           UISoftKeyboardKey &key = keys[j];
           xmlWriter.writeTextElement("position", QString::number(key.position()));
           if (!key.keyCap().isEmpty())
               xmlWriter.writeTextElement("keycap", key.keyCap());
           else
               xmlWriter.writeTextElement("keycap", key.staticKeyCap());
           xmlWriter.writeEndElement();
       }
   }
   xmlWriter.writeEndElement();
   xmlWriter.writeEndDocument();

   xmlFile.close();
}

void UISoftKeyboardWidget::sltHandleLoadKeyCapFile()
{
    const QString strFileName = QIFileDialog::getOpenFileName(QString(), UISoftKeyboard::tr("XML files (*.xml)"), this,
                                                              UISoftKeyboard::tr("Choose file to load key captions.."));
    if (strFileName.isEmpty())
        return;
    UIKeyboardKeyCapReader keyCapReader(strFileName);
    const QMap<int, QString> &keyCapMap = keyCapReader.keyCapMap();

    for (int i = 0; i < m_rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (keyCapMap.contains(key.position()))
                key.setKeyCap(keyCapMap[key.position()]);
        }
    }
    emit sigKeyCapChange(strFileName);
}

void UISoftKeyboardWidget::sltHandleUnloadKeyCaps()
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
            keys[j].setKeyCap(QString());
    }
    emit sigKeyCapChange(QString());
}

void UISoftKeyboardWidget::sltHandleKepCapEditModeToggle(bool fToggle)
{
    if (fToggle)
        m_enmMode = Mode_KeyCapEdit;
    else
        m_enmMode = Mode_Keyboard;
}

void UISoftKeyboardWidget::setNewMinimumSize(const QSize &size)
{
    m_minimumSize = size;
    updateGeometry();
}

void UISoftKeyboardWidget::setInitialSize(int iWidth, int iHeight)
{
    m_iInitialWidth = iWidth;
    m_iInitialHeight = iHeight;
}

UISoftKeyboardKey *UISoftKeyboardWidget::keyUnderMouse(QMouseEvent *pEvent)
{
    QPoint eventPosition(pEvent->pos().x() / m_fScaleFactorX, pEvent->pos().y() / m_fScaleFactorY);
    return keyUnderMouse(eventPosition);
}

UISoftKeyboardKey *UISoftKeyboardWidget::keyUnderMouse(const QPoint &eventPosition)
{
    UISoftKeyboardKey *pKey = 0;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (key.polygonInGlobal().containsPoint(eventPosition, Qt::OddEvenFill))
            {
                pKey = &key;
                break;
            }
        }
    }
    if (m_pKeyUnderMouse != pKey)
    {
        m_pKeyUnderMouse = pKey;
        update();
    }
    return pKey;
}

void UISoftKeyboardWidget::handleKeyPress(UISoftKeyboardKey *pKey)
{
    if (!pKey)
        return;
    pKey->press();

    if (pKey->type() == UIKeyType_Modifier)
        return;

    QVector<LONG> sequence;
     /* Add the pressed modifiers first: */
    for (int i = 0; i < m_pressedModifiers.size(); ++i)
    {
        UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
        if (pModifier->scanCodePrefix() != 0)
            sequence << pModifier->scanCodePrefix();
        sequence << pModifier->scanCode();
    }

    if (pKey->scanCodePrefix() != 0)
        sequence << pKey->scanCodePrefix();
    sequence << pKey->scanCode();
    emit sigPutKeyboardSequence(sequence);
}

void UISoftKeyboardWidget::keyStateChange(UISoftKeyboardKey* pKey)
{
    if (!pKey)
        return;
    if (pKey->type() == UIKeyType_Modifier)
    {
        if (pKey->state() == UIKeyState_NotPressed)
            m_pressedModifiers.removeOne(pKey);
        else
            if (!m_pressedModifiers.contains(pKey))
                m_pressedModifiers.append(pKey);
    }
}

void UISoftKeyboardWidget::loadDefaultLayout()
{
    /* Choose the first layout action's data as the default layout: */
    if (!m_pLayoutActionGroup->actions().empty())
    {
        QAction *pAction = m_pLayoutActionGroup->actions().at(0);
        if (pAction)
        {
            QString strLayout = pAction->data().toString();
            if (!strLayout.isEmpty())
            {
                if (createKeyboard(strLayout))
                {
                    emit sigLayoutChange(strLayout);
                    m_pLastSelectedLayout = pAction;
                    pAction->setChecked(true);
                }
            }
        }
    }
}

void UISoftKeyboardWidget::showContextMenu(const QPoint &globalPoint)
{
    m_pContextMenu->exec(globalPoint);
    update();
}

void UISoftKeyboardWidget::handleKeyRelease(UISoftKeyboardKey *pKey)
{
    if (!pKey)
        return;
    if (pKey->type() == UIKeyType_Ordinary)
        pKey->release();
    /* We only send the scan codes of Ordinary keys: */
    if (pKey->type() == UIKeyType_Modifier)
        return;

    QVector<LONG> sequence;
    if (pKey->scanCodePrefix() != 0)
        sequence <<  pKey->scanCodePrefix();
    sequence << (pKey->scanCode() | 0x80);

    /* Add the pressed modifiers in the reverse order: */
    for (int i = m_pressedModifiers.size() - 1; i >= 0; --i)
    {
        UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
        if (pModifier->scanCodePrefix() != 0)
            sequence << pModifier->scanCodePrefix();
        sequence << (pModifier->scanCode() | 0x80);
        /* Release the pressed modifiers (if there are not locked): */
        pModifier->release();
    }
    emit sigPutKeyboardSequence(sequence);
}

bool UISoftKeyboardWidget::createKeyboard(const QString &strLayoutFileName)
{
    UIKeyboardLayoutReader reader;
    QVector<UISoftKeyboardRow> &rows = m_rows;
    reset();
    bool fParseSuccess = false;
    if (!strLayoutFileName.isEmpty())
        fParseSuccess = reader.parseXMLFile(strLayoutFileName, rows);

    if (!fParseSuccess)
        return false;

    int iY = m_iTopMargin;
    int iMaxWidth = 0;

    for (int i = 0; i < rows.size(); ++i)
    {
        UISoftKeyboardRow &row = rows[i];
        int iX = m_iLeftMargin;
        int iRowHeight = row.defaultHeight();
        for (int j = 0; j < row.keys().size(); ++j)
        {
            UISoftKeyboardKey &key = (row.keys())[j];
            key.setKeyGeometry(QRect(iX, iY, key.width(), key.height()));
            key.setPolygon(QPolygon(UIKeyboardLayoutReader::computeKeyVertices(key)));
            key.setParentWidget(this);
            iX += key.width();
            if (j < row.keys().size() - 1)
                iX += m_iXSpacing;
            if (key.spaceWidthAfter() != 0)
                iX += (m_iXSpacing + key.spaceWidthAfter());
        }
        if (row.spaceHeightAfter() != 0)
            iY += row.spaceHeightAfter() + m_iYSpacing;
        iMaxWidth = qMax(iMaxWidth, iX);
        iY += iRowHeight;
        if (i < rows.size() - 1)
            iY += m_iYSpacing;
    }
    int iInitialWidth = iMaxWidth + m_iRightMargin;
    int iInitialHeight = iY + m_iBottomMargin;
    float fScale = 1.0f;
    setNewMinimumSize(QSize(fScale * iInitialWidth, fScale * iInitialHeight));
    setInitialSize(fScale * iInitialWidth, fScale * iInitialHeight);
    update();
    return true;
}

void UISoftKeyboardWidget::reset()
{
    m_pressedModifiers.clear();
    m_rows.clear();
}

void UISoftKeyboardWidget::configure()
{
    defaultLayouts << ":/101_ansi.xml" << ":/102_iso.xml";
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &UISoftKeyboardWidget::customContextMenuRequested,
            this, &UISoftKeyboardWidget::sltHandleContextMenuRequest);
}

void UISoftKeyboardWidget::prepareObjects()
{
    m_pKeyCapEditor = new QLineEdit(this);
    if (m_pKeyCapEditor)
    {
        m_pKeyCapEditor->hide();
        m_pKeyCapEditor->installEventFilter(this);
    }
    m_pContextMenu = new QMenu(this);
    /* Layout menu: */
    m_pLayoutActionGroup = new QActionGroup(this);
    m_pLayoutActionGroup->setExclusive(true);
    connect(m_pLayoutActionGroup, &QActionGroup::triggered, this, &UISoftKeyboardWidget::sltHandleLoadLayout);

    foreach (const QString &strLayout, defaultLayouts)
    {
        QString strName = strLayout.left(strLayout.indexOf('.'));
        QAction *pAction = m_pLayoutActionGroup->addAction(QString("%1 %2").arg(UISoftKeyboard::tr("Load Layout ")).arg(strName.remove(":/")));
        pAction->setData(strLayout);
        pAction->setCheckable(true);
    }
    m_pLoadLayoutFileAction = m_pLayoutActionGroup->addAction(UISoftKeyboard::tr("Load Layout File..."));
    m_pLoadLayoutFileAction->setCheckable(true);
    m_pContextMenu->addActions(m_pLayoutActionGroup->actions());

    /* Keycaps menu: */
    m_pKeyCapEditModeToggleAction = m_pContextMenu->addAction(UISoftKeyboard::tr("Key captions editing mode"));
    connect(m_pKeyCapEditModeToggleAction, &QAction::toggled, this, &UISoftKeyboardWidget::sltHandleKepCapEditModeToggle);
    m_pKeyCapEditModeToggleAction->setCheckable(true);
    m_pKeyCapEditModeToggleAction->setChecked(false);

#ifdef DEBUG
    m_pShowPositionsAction = m_pContextMenu->addAction(UISoftKeyboard::tr("Show key positions instead of key caps"));
    m_pShowPositionsAction->setCheckable(true);
    m_pShowPositionsAction->setChecked(false);
#endif

    QAction *pSaveKeyCapFile = m_pContextMenu->addAction(UISoftKeyboard::tr("Save key caps to file..."));
    connect(pSaveKeyCapFile, &QAction::triggered, this, &UISoftKeyboardWidget::sltHandleSaveKeyCapFile);
    QAction *pLoadKeyCapFile = m_pContextMenu->addAction(UISoftKeyboard::tr("Load key caps from file..."));
    connect(pLoadKeyCapFile, &QAction::triggered, this, &UISoftKeyboardWidget::sltHandleLoadKeyCapFile);
    QAction *pUnloadKeyCaps = m_pContextMenu->addAction(UISoftKeyboard::tr("Unload key caps"));
    connect(pUnloadKeyCaps, &QAction::triggered, this, &UISoftKeyboardWidget::sltHandleUnloadKeyCaps);
}

void UISoftKeyboardWidget::enableDisableKeyCapEditMode(bool fEnable)
{
    if (m_fInKeyCapEditMode == fEnable)
        return;
    m_enmMode = Mode_KeyCapEdit;
}

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader implementation.                                                                                  *
*********************************************************************************************************************************/

bool UIKeyboardLayoutReader::parseXMLFile(const QString &strFileName, QVector<UISoftKeyboardRow> &rows)
{
    QFile xmlFile(strFileName);
    if (!xmlFile.exists())
        return false;

    if (!xmlFile.open(QIODevice::ReadOnly))
        return false;

    m_xmlReader.setDevice(&xmlFile);

    if (!m_xmlReader.readNextStartElement() || m_xmlReader.name() != "layout")
        return false;

    QXmlStreamAttributes attributes = m_xmlReader.attributes();
    int iDefaultWidth = attributes.value("defaultWidth").toInt();
    int iDefaultHeight = attributes.value("defaultHeight").toInt();

    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "row")
            parseRow(iDefaultWidth, iDefaultHeight, rows);
        else if (m_xmlReader.name() == "space")
            parseRowSpace(rows);
        else
            m_xmlReader.skipCurrentElement();
    }

    return true;
}

void UIKeyboardLayoutReader::parseRow(int iDefaultWidth, int iDefaultHeight, QVector<UISoftKeyboardRow> &rows)
{
    rows.append(UISoftKeyboardRow());
    UISoftKeyboardRow &row = rows.back();

    row.setDefaultWidth(iDefaultWidth);
    row.setDefaultHeight(iDefaultHeight);
    row.setSpaceHeightAfter(0);

    /* Override the layout attributes if the row also has them: */
    QXmlStreamAttributes attributes = m_xmlReader.attributes();
    if (attributes.hasAttribute("defaultWidth"))
        row.setDefaultWidth(attributes.value("defaultWidth").toInt());
    if (attributes.hasAttribute("defaultHeight"))
        row.setDefaultHeight(attributes.value("defaultHeight").toInt());
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "key")
            parseKey(row);
        else if (m_xmlReader.name() == "space")
            parseKeySpace(row);
        else
            m_xmlReader.skipCurrentElement();
    }
}

void UIKeyboardLayoutReader::parseRowSpace(QVector<UISoftKeyboardRow> &rows)
{
    int iSpace = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "height")
            iSpace = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    if (!rows.empty())
        rows.back().setSpaceHeightAfter(iSpace);
}

void UIKeyboardLayoutReader::parseKey(UISoftKeyboardRow &row)
{
    row.keys().append(UISoftKeyboardKey());
    UISoftKeyboardKey &key = row.keys().back();
    key.setWidth(row.defaultWidth());
    key.setHeight(row.defaultHeight());
    QString strKeyCap;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            key.setWidth(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "height")
            key.setHeight(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "scancode")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.setScanCode(strCode.toInt(&fOk, 16));
        }
        else if (m_xmlReader.name() == "scancodeprefix")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.setScanCodePrefix(strCode.toInt(&fOk, 16));
        }
        else if (m_xmlReader.name() == "keycap")
        {
            QString strCap = m_xmlReader.readElementText();
            if (strKeyCap.isEmpty())
                strKeyCap = strCap;
            else
                strKeyCap += "\n" + strCap;
        }
        else if (m_xmlReader.name() == "cutout")
            parseCutout(key);
        else if (m_xmlReader.name() == "position")
            key.setPosition(m_xmlReader.readElementText().toInt());
        else if (m_xmlReader.name() == "type")
        {
            QString strType = m_xmlReader.readElementText();
            if (strType == "modifier")
                key.setType(UIKeyType_Modifier);
            else if (strType == "toggleable")
                key.setType(UIKeyType_Toggleable);
        }
        else
            m_xmlReader.skipCurrentElement();
    }
    key.setStaticKeyCap(strKeyCap);
}

void UIKeyboardLayoutReader::parseKeySpace(UISoftKeyboardRow &row)
{
    int iWidth = row.defaultWidth();
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            iWidth = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    if (row.keys().size() <= 0)
        return;
    row.keys().back().setSpaceWidthAfter(iWidth);
}

void UIKeyboardLayoutReader::parseCutout(UISoftKeyboardKey &key)
{
    int iWidth = 0;
    int iHeight = 0;
    int iCorner = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            iWidth = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "height")
            iHeight = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "corner")
        {
            QString strCorner = m_xmlReader.readElementText();
            if (strCorner == "topLeft")
                    iCorner = 0;
            else if(strCorner == "topRight")
                    iCorner = 1;
            else if(strCorner == "bottomRight")
                    iCorner = 2;
            else if(strCorner == "bottomLeft")
                    iCorner = 3;
        }
        else
            m_xmlReader.skipCurrentElement();
    }
    key.setCutout(iCorner, iWidth, iHeight);
}

QVector<QPoint> UIKeyboardLayoutReader::computeKeyVertices(const UISoftKeyboardKey &key)
{
    QVector<QPoint> vertices;

    if (key.cutoutCorner() == -1 || key.width() <= key.cutoutWidth() || key.height() <= key.cutoutHeight())
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        return vertices;
    }
    if (key.cutoutCorner() == 0)
    {
        vertices.append(QPoint(key.cutoutWidth(), 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        vertices.append(QPoint(0, key.cutoutHeight()));
        vertices.append(QPoint(key.cutoutWidth(), key.cutoutHeight()));
    }
    else if (key.cutoutCorner() == 1)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), 0));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.cutoutHeight()));
        vertices.append(QPoint(key.width(), key.cutoutHeight()));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.cutoutCorner() == 2)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.cutoutHeight()));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.cutoutHeight()));
        vertices.append(QPoint(key.width() - key.cutoutWidth(), key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.cutoutCorner() == 3)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(key.cutoutWidth(), key.height()));
        vertices.append(QPoint(key.cutoutWidth(), key.height() - key.cutoutHeight()));
        vertices.append(QPoint(0, key.height() - key.cutoutHeight()));
    }
    return vertices;
}

/*********************************************************************************************************************************
*   UIKeyboardKeyCapReader implementation.                                                                                  *
*********************************************************************************************************************************/

UIKeyboardKeyCapReader::UIKeyboardKeyCapReader(const QString &strFileName)
{
    parseKeyCapFile(strFileName);
}

const QMap<int, QString> &UIKeyboardKeyCapReader::keyCapMap() const
{
    return m_keyCapMap;
}

bool UIKeyboardKeyCapReader::parseKeyCapFile(const QString &strFileName)
{
    QFile xmlFile(strFileName);
    if (!xmlFile.exists())
        return false;

    if (!xmlFile.open(QIODevice::ReadOnly))
        return false;

    m_xmlReader.setDevice(&xmlFile);

    if (!m_xmlReader.readNextStartElement() || m_xmlReader.name() != "keycaps")
        return false;

    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "key")
            parseKey();
        else
            m_xmlReader.skipCurrentElement();
    }

    return true;
}

void  UIKeyboardKeyCapReader::parseKey()
{
    QString strKeyCap;
    int iKeyPosition = 0;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "keycap")
            strKeyCap = m_xmlReader.readElementText();
        else if (m_xmlReader.name() == "position")
            iKeyPosition = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    m_keyCapMap.insert(iKeyPosition, strKeyCap);
}

/*********************************************************************************************************************************
*   UISoftKeyboard implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboard::UISoftKeyboard(QWidget *pParent,
                               UISession *pSession, QString strMachineName /* = QString()*/)
    :QIWithRetranslateUI<QMainWindow>(pParent)
    , m_pSession(pSession)
    , m_pMainLayout(0)
    , m_pContainerWidget(0)
    , m_strMachineName(strMachineName)
    , m_pSettingsButton(0)
{
    setWindowTitle(QString("%1 - %2").arg(m_strMachineName).arg(tr("Soft Keyboard")));
    setAttribute(Qt::WA_DeleteOnClose);
    prepareObjects();
    prepareConnections();

    if (m_pContainerWidget)
        m_pContainerWidget->loadDefaultLayout();

    loadSettings();
    retranslateUi();
    updateStatusBarMessage();
}

UISoftKeyboard::~UISoftKeyboard()
{
    saveSettings();
}

void UISoftKeyboard::retranslateUi()
{
    if (m_pSettingsButton)
        m_pSettingsButton->setToolTip(tr("Settings Menu"));
}

bool UISoftKeyboard::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched != m_pSettingsButton)
        return QIWithRetranslateUI<QMainWindow>::eventFilter(pWatched, pEvent);

    if (pEvent->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *pMouseEvent = dynamic_cast<QMouseEvent*>(pEvent);
        if (pMouseEvent && pMouseEvent->button() == Qt::LeftButton)
        {
            if (m_pContainerWidget)
                m_pContainerWidget->showContextMenu(m_pSettingsButton->mapToGlobal(pMouseEvent->pos()));
            return true;
        }
    }

    return QIWithRetranslateUI<QMainWindow>::eventFilter(pWatched, pEvent);
}

void UISoftKeyboard::sltHandleKeyboardLedsChange()
{
    // bool fNumLockLed = m_pSession->isNumLock();
    // bool fCapsLockLed = m_pSession->isCapsLock();
    // bool fScrollLockLed = m_pSession->isScrollLock();
}

void UISoftKeyboard::sltHandlePutKeyboardSequence(QVector<LONG> sequence)
{
    keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltHandleLayoutChange(const QString &strLayoutName)
{
    if (m_strLayoutName == strLayoutName)
        return;
    m_strLayoutName = strLayoutName;
    updateStatusBarMessage();
}

void UISoftKeyboard::sltHandleKeyCapFileChange(const QString &strKeyCapFileName)
{
    if (strKeyCapFileName == m_strKeyCapFileName)
        return;
    m_strKeyCapFileName = strKeyCapFileName;
    updateStatusBarMessage();
}

void UISoftKeyboard::sltHandleStatusBarContextMenuRequest(const QPoint &point)
{
    if (m_pContainerWidget)
        m_pContainerWidget->showContextMenu(statusBar()->mapToGlobal(point));
}

void UISoftKeyboard::prepareObjects()
{
    m_pContainerWidget = new UISoftKeyboardWidget;
    if (!m_pContainerWidget)
        return;
    m_pContainerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setCentralWidget(m_pContainerWidget);
    m_pContainerWidget->updateGeometry();


    statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(statusBar(), &QStatusBar::customContextMenuRequested,
            this, &UISoftKeyboard::sltHandleStatusBarContextMenuRequest);

    statusBar()->setStyleSheet( "QStatusBar::item { border: 0px}" );
    // QWidget *pStatusBarWidget = new QWidget;
    // QHBoxLayout *pStatusBarLayout = new QHBoxLayout(pStatusBarWidget);
    // pStatusBarLayout->setSpacing(0);
    // pStatusBarLayout->setContentsMargins(0, 0, 0, 0);

    m_pSettingsButton = new QToolButton;
    m_pSettingsButton->setIcon(UIIconPool::iconSet(":/keyboard_settings_16px.png"));
    m_pSettingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_pSettingsButton->resize(QSize(iIconMetric, iIconMetric));
    m_pSettingsButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
    statusBar()->addPermanentWidget(m_pSettingsButton);

    QToolButton *pButton = new QToolButton;
    pButton->setIcon(UIIconPool::iconSet(":/hd_modify_16px.png"));
    pButton->setStyleSheet("QToolButton { border: 0px none black; margin: 0px 0px 0px 0px; } QToolButton::menu-indicator {image: none;}");
    statusBar()->addPermanentWidget(pButton);



    retranslateUi();
}

void UISoftKeyboard::prepareConnections()
{
    connect(m_pSession, &UISession::sigKeyboardLedsChange, this, &UISoftKeyboard::sltHandleKeyboardLedsChange);
    connect(m_pContainerWidget, &UISoftKeyboardWidget::sigPutKeyboardSequence, this, &UISoftKeyboard::sltHandlePutKeyboardSequence);
    connect(m_pContainerWidget, &UISoftKeyboardWidget::sigLayoutChange, this, &UISoftKeyboard::sltHandleLayoutChange);
    connect(m_pContainerWidget, &UISoftKeyboardWidget::sigKeyCapChange, this, &UISoftKeyboard::sltHandleKeyCapFileChange);
}

void UISoftKeyboard::saveSettings()
{
}

void UISoftKeyboard::loadSettings()
{
}

void UISoftKeyboard::updateStatusBarMessage()
{
    QString strMessage;
    if (!m_strLayoutName.isEmpty())
        strMessage += QString("%1: %2").arg(tr("Layout")).arg(m_strLayoutName);
    if (!m_strKeyCapFileName.isEmpty())
        strMessage += QString("\t/\t %1: %2").arg(tr("Key Captions File")).arg(m_strKeyCapFileName);
    statusBar()->showMessage(strMessage);
}

CKeyboard& UISoftKeyboard::keyboard() const
{
    return m_pSession->keyboard();
}

#include "UISoftKeyboard.moc"
