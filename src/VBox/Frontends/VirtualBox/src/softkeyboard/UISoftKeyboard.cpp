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
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QXmlStreamReader>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UISoftKeyboard.h"
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
*   UISoftKeyboardRow definition.                                                                                  *
*********************************************************************************************************************************/

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

class UISoftKeyboardKey
{
public:

    UISoftKeyboardKey();

    const QRect keyGeometry() const;
    void setKeyGeometry(const QRect &rect);

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
    /** -1 is no cutout. 0 is the topleft corner. we go clockwise. */
    int        m_iCutoutCorner;
    /** Key's position in the layout. */
    int        m_iPosition;
    UISoftKeyboardWidget  *m_pParentWidget;
};

/*********************************************************************************************************************************
*   UISoftKeyboardWidget definition.                                                                                  *
*********************************************************************************************************************************/
class UISoftKeyboardWidget : public QWidget
{
    Q_OBJECT;

signals:

    void sigPutKeyboardSequence(QVector<LONG> sequence);

public:

    UISoftKeyboardWidget(QWidget *pParent = 0);

    virtual QSize minimumSizeHint() const;
    virtual QSize sizeHint() const;
    void setNewMinimumSize(const QSize &size);

    QVector<UISoftKeyboardRow> &rows();
    const QVector<UISoftKeyboardRow> &rows() const;

    void setInitialSize(int iWidth, int iHeight);
    void keyStateChange(UISoftKeyboardKey* pKey);

protected:

    void paintEvent(QPaintEvent *pEvent) /* override */;
    void mousePressEvent(QMouseEvent *pEvent) /* override */;
    void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;
    void mouseMoveEvent(QMouseEvent *pEvent);

private:

    UISoftKeyboardKey *keyUnderMouse(QMouseEvent *pEvent);
    void               handleKeyPress(UISoftKeyboardKey *pKey);
    void               handleKeyRelease(UISoftKeyboardKey *pKey);
    QSize m_minimumSize;
    int m_iInitialHeight;
    int m_iInitialWidth;
    float m_fMultiplierX;
    float m_fMultiplierY;
    UISoftKeyboardKey *m_pKeyUnderMouse;
    UISoftKeyboardKey *m_pKeyPressed;
    QColor m_keyDefaultColor;
    QColor m_keyHoverColor;
    QColor m_textDefaultColor;
    QColor m_textPressedColor;
    QVector<UISoftKeyboardKey*> m_pressedModifiers;
    QVector<UISoftKeyboardRow> m_rows;
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

const QString &UISoftKeyboardKey::keyCap() const
{
    return m_strKeyCap;
}

void UISoftKeyboardKey::setKeyCap(const QString &strKeyCap)
{
    if (m_strKeyCap.isEmpty())
        m_strKeyCap = strKeyCap;
    else
        m_strKeyCap += "\n" + strKeyCap;
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
    :QWidget(pParent)
    , m_pKeyUnderMouse(0)
    , m_pKeyPressed(0)
    , m_keyDefaultColor(QColor(103, 128, 159))
    , m_keyHoverColor(QColor(108, 122, 137))
    , m_textDefaultColor(QColor(46, 49, 49))
    , m_textPressedColor(QColor(149, 165, 166))
{
    setMouseTracking(true);
}

QSize UISoftKeyboardWidget::minimumSizeHint() const
{
    return m_minimumSize;
}

QSize UISoftKeyboardWidget::sizeHint() const
{
    return m_minimumSize;
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

QVector<UISoftKeyboardRow> &UISoftKeyboardWidget::rows()
{
    return m_rows;
}

const QVector<UISoftKeyboardRow> &UISoftKeyboardWidget::rows() const
{
    return m_rows;
}

void UISoftKeyboardWidget::paintEvent(QPaintEvent *pEvent) /* override */
{
    Q_UNUSED(pEvent);
    if (m_iInitialWidth == 0 || m_iInitialHeight == 0)
        return;

    m_fMultiplierX = width() / (float) m_iInitialWidth;
    m_fMultiplierY = height() / (float) m_iInitialHeight;

    QPainter painter(this);
    QFont painterFont(font());
    painterFont.setPixelSize(15);
    painterFont.setBold(false);
    painter.setFont(painterFont);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(m_fMultiplierX, m_fMultiplierY);
    int unitSize = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    float fLedRadius =  0.8 * unitSize;
    float fLedMargin =  0.6 * unitSize;

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

            if (&key  == m_pKeyPressed)
                painter.setPen(QPen(QColor(m_textPressedColor), 2));
            else
                painter.setPen(QPen(QColor(m_textDefaultColor), 2));

            painter.drawPolygon(key.polygon());

            QRect textRect(0.8 * unitSize, 1 * unitSize
                           , key.keyGeometry().width(), key.keyGeometry().height());
            painter.drawText(textRect, Qt::TextWordWrap, key.keyCap());

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
    m_pKeyPressed = keyUnderMouse(pEvent);
    handleKeyPress(m_pKeyPressed);
    update();
}

void UISoftKeyboardWidget::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_pKeyPressed)
        return;
    handleKeyRelease(m_pKeyPressed);
    m_pKeyPressed = 0;
    update();
}

void UISoftKeyboardWidget::mouseMoveEvent(QMouseEvent *pEvent)
{
    keyUnderMouse(pEvent);
}

UISoftKeyboardKey *UISoftKeyboardWidget::keyUnderMouse(QMouseEvent *pEvent)
{
    QPoint eventPosition(pEvent->pos().x() / m_fMultiplierX, pEvent->pos().y() / m_fMultiplierY);
    QWidget::mousePressEvent(pEvent);
    for (int i = 0; i < m_rows.size(); ++i)
    {
        QVector<UISoftKeyboardKey> &keys = m_rows[i].keys();
        for (int j = 0; j < keys.size(); ++j)
        {
            UISoftKeyboardKey &key = keys[j];
            if (key.polygonInGlobal().containsPoint(eventPosition, Qt::OddEvenFill))
            {
                if (&key != m_pKeyUnderMouse)
                {
                    m_pKeyUnderMouse = &key;
                    update();
                }
                return &key;
                break;
            }
        }
    }
    return 0;
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
            key.setKeyCap(m_xmlReader.readElementText());
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
*   UISoftKeyboard implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboard::UISoftKeyboard(QWidget *pParent,
                               UISession *pSession, QString strMachineName /* = QString()*/)
    :QIWithRetranslateUI<QMainWindow>(pParent)
    , m_pSession(pSession)
    , m_pMainLayout(0)
    , m_pContainerWidget(0)
    , m_strMachineName(strMachineName)
    , m_iXSpacing(5)
    , m_iYSpacing(5)
    , m_iLeftMargin(10)
    , m_iTopMargin(10)
    , m_iRightMargin(10)
    , m_iBottomMargin(10)
{
    setAttribute(Qt::WA_DeleteOnClose);
    prepareObjects();
    createKeyboard();
    prepareConnections();
    prepareToolBar();
    loadSettings();
    retranslateUi();
}

UISoftKeyboard::~UISoftKeyboard()
{
    saveSettings();
}

void UISoftKeyboard::retranslateUi()
{
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

void UISoftKeyboard::prepareObjects()
{
    m_pContainerWidget = new UISoftKeyboardWidget;
    if (!m_pContainerWidget)
        return;
    m_pContainerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setCentralWidget(m_pContainerWidget);
    m_pContainerWidget->updateGeometry();
}

void UISoftKeyboard::prepareConnections()
{
    connect(m_pSession, &UISession::sigKeyboardLedsChange, this, &UISoftKeyboard::sltHandleKeyboardLedsChange);
    connect(m_pContainerWidget, &UISoftKeyboardWidget::sigPutKeyboardSequence, this, &UISoftKeyboard::sltHandlePutKeyboardSequence);
}

void UISoftKeyboard::prepareToolBar()
{
}

void UISoftKeyboard::saveSettings()
{
}

void UISoftKeyboard::loadSettings()
{
}

void UISoftKeyboard::createKeyboard()
{
    if (!m_pContainerWidget)
        return;
    UIKeyboardLayoutReader reader;
    QVector<UISoftKeyboardRow> &rows = m_pContainerWidget->rows();
    if (!reader.parseXMLFile(":/102_iso.xml", rows))
        return;
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
            key.setParentWidget(m_pContainerWidget);
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
    m_pContainerWidget->setNewMinimumSize(QSize(iInitialWidth, iInitialHeight));
    m_pContainerWidget->setInitialSize(iInitialWidth, iInitialHeight);
}

CKeyboard& UISoftKeyboard::keyboard() const
{
    return m_pSession->keyboard();
}

#include "UISoftKeyboard.moc"
