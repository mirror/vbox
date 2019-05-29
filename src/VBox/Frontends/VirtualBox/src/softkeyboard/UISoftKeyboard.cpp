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



enum UIKeyState
{
    UIKeyState_NotPressed,
    UIKeyState_Pressed,
    UIKeyState_Locked,
    UIKeyState_Max
};

enum UIKeyType
{
    UIKeyType_Ordinary,
    /** e.g. CapsLock. Can be only in UIKeyState_NotPressed, UIKeyState_Locked, */
    UIKeyType_Toggleable,
    /** e.g. Shift Can be in all 3 states*/
    UIKeyType_Modifier,
    UIKeyType_Max
};


class UISoftKeyboardRow
{
public:
    /** Default width and height might be inherited from the layout and overwritten in row settings. */
    int m_iDefaultWidth;
    int m_iDefaultHeight;
    int m_iStartingSpace;
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

    void setType(UIKeyType enmType);
    UIKeyType type() const;

    UIKeyState state() const;
    QVector<LONG> scanCodeWithPrefix() const;

    void release();

    void setPolygon(const QPolygon &polygon);
    const QPolygon &polygon() const;

    QPolygon polygonInGlobal() const;

    int   m_iDefaultPixelSize;
    int   m_iDefaultPointSize;
    LONG  m_scanCode;
    LONG  m_scanCodePrefix;

    int       m_iCutoutWidth;
    int       m_iCutoutHeight;
    /** -1 is no cutout. 0 is the topleft corner. we go clockwise. */
    int       m_iCutoutCorner;


protected:

    //virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    virtual void paintEvent(QPaintEvent *pPaintEvent) /* override */;

private:

    void updateState(bool fPressed);

    QRect     m_keyGeometry;
    QString   m_strKeyCap;
    /** Stores the key polygon in local coordinates. */
    QPolygon  m_polygon;

    UIKeyType  m_enmType;
    UIKeyState m_enmState;

    /** Key width as it is read from the xml file. */
    int       m_iWidth;
    /** Key height as it is read from the xml file. */
    int       m_iHeight;
    int       m_iSpaceWidthAfter;
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

    row.m_iDefaultWidth = iDefaultWidth;
    row.m_iDefaultHeight = iDefaultHeight;
    row.m_iSpaceHeightAfter = 0;

    /* Override the layout attributes if the row also has them: */
    QXmlStreamAttributes attributes = m_xmlReader.attributes();
    if (attributes.hasAttribute("defaultWidth"))
        row.m_iDefaultWidth = attributes.value("defaultWidth").toInt();
    if (attributes.hasAttribute("defaultHeight"))
        row.m_iDefaultHeight = attributes.value("defaultHeight").toInt();
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
        rows.back().m_iSpaceHeightAfter = iSpace;
}

void UIKeyboardLayoutReader::parseKey(UISoftKeyboardRow &row)
{
    row.m_keys.append(UISoftKeyboardKey());
    UISoftKeyboardKey &key = row.m_keys.back();
    key.setWidth(row.m_iDefaultWidth);
    key.setHeight(row.m_iDefaultHeight);
    key.m_iCutoutCorner = -1;
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
            key.m_scanCode = strCode.toInt(&fOk, 16);
        }
        else if (m_xmlReader.name() == "scancodeprefix")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.m_scanCodePrefix = strCode.toInt(&fOk, 16);
        }
        else if (m_xmlReader.name() == "keycap")
            key.setKeyCap(m_xmlReader.readElementText());
        else if (m_xmlReader.name() == "cutout")
            parseCutout(key);
        // else if (m_xmlReader.name() == "type")
        // {
        //     QString strType = m_xmlReader.readElementText();
        //     if (strType == "modifier")
        //         pKey->setType(UIKeyType_Modifier);
        //     else if (strType == "toggleable")
        //         pKey->setType(UIKeyType_Toggleable);
        // }
        else
            m_xmlReader.skipCurrentElement();
    }
}

void UIKeyboardLayoutReader::parseKeySpace(UISoftKeyboardRow &row)
{
    int iWidth = row.m_iDefaultWidth;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            iWidth = m_xmlReader.readElementText().toInt();
        else
            m_xmlReader.skipCurrentElement();
    }
    if (row.m_keys.size() <= 0)
        return;
    row.m_keys.back().setSpaceWidthAfter(iWidth);
}

void UIKeyboardLayoutReader::parseCutout(UISoftKeyboardKey &key)
{
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            key.m_iCutoutWidth = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "height")
            key.m_iCutoutHeight = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "corner")
        {
            QString strCorner = m_xmlReader.readElementText();
            if (strCorner == "topLeft")
                    key.m_iCutoutCorner = 0;
            else if(strCorner == "topRight")
                    key.m_iCutoutCorner = 1;
            else if(strCorner == "bottomRight")
                    key.m_iCutoutCorner = 2;
            else if(strCorner == "bottomLeft")
                    key.m_iCutoutCorner = 3;
        }
        else
            m_xmlReader.skipCurrentElement();
    }
}

QVector<QPoint> UIKeyboardLayoutReader::computeKeyVertices(const UISoftKeyboardKey &key)
{
    QVector<QPoint> vertices;

    if (key.m_iCutoutCorner == -1 || key.width() <= key.m_iCutoutWidth || key.height() <= key.m_iCutoutHeight)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        return vertices;
    }
    if (key.m_iCutoutCorner == 0)
    {
        vertices.append(QPoint(key.m_iCutoutWidth, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
        vertices.append(QPoint(0, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iCutoutWidth, key.m_iCutoutHeight));
    }
    else if (key.m_iCutoutCorner == 1)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width() - key.m_iCutoutWidth, 0));
        vertices.append(QPoint(key.width() - key.m_iCutoutWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.width(), key.m_iCutoutHeight));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.m_iCutoutCorner == 2)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.m_iCutoutHeight));
        vertices.append(QPoint(key.width() - key.m_iCutoutWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.width() - key.m_iCutoutWidth, key.height()));
        vertices.append(QPoint(0, key.height()));
    }
    else if (key.m_iCutoutCorner == 3)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.width(), 0));
        vertices.append(QPoint(key.width(), key.height()));
        vertices.append(QPoint(key.m_iCutoutWidth, key.height()));
        vertices.append(QPoint(key.m_iCutoutWidth, key.height() - key.m_iCutoutHeight));
        vertices.append(QPoint(0, key.height() - key.m_iCutoutHeight));
    }
    return vertices;
}

class UISoftKeyboardWidget : public QWidget
{
    Q_OBJECT;
 public:

    virtual QSize minimumSizeHint() const
    {
        return m_minimumSize;
    }

    virtual QSize sizeHint() const
    {
        return m_minimumSize;
    }

    void setNewMinimumSize(const QSize &size)
    {
        m_minimumSize = size;
        updateGeometry();
    }
    QVector<UISoftKeyboardRow> m_rows;

    void setInitialSize(int iWidth, int iHeight)
    {
        m_iInitialWidth = iWidth;
        m_iInitialHeight = iHeight;
    }

protected:

    void paintEvent(QPaintEvent *pEvent) /* override */
    {
        Q_UNUSED(pEvent);
        if (m_iInitialWidth == 0 || m_iInitialHeight == 0)
            return;

        m_fMultiplierX = width() / (float) m_iInitialWidth;
        m_fMultiplierY = height() / (float) m_iInitialHeight;

        QPainter painter(this);
        QFont painterFont(font());
        painterFont.setPixelSize(16);
        painter.setFont(painterFont);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(0, 0,0), 2));
        painter.setBrush(QBrush(QColor(255, 0,0)));
        painter.scale(m_fMultiplierX, m_fMultiplierY);
        for (int i = 0; i < m_rows.size(); ++i)
        {
            QVector<UISoftKeyboardKey> &keys = m_rows[i].m_keys;
            for (int j = 0; j < keys.size(); ++j)
            {
                UISoftKeyboardKey &key = keys[j];
                painter.translate(key.keyGeometry().x(), key.keyGeometry().y());
                painter.drawPolygon(key.polygon());
                QRect textRect(0, 0, key.keyGeometry().width(), key.keyGeometry().height());
                painter.drawText(textRect, Qt::TextWordWrap, key.keyCap());
                painter.translate(-key.keyGeometry().x(), -key.keyGeometry().y());
            }
        }
    }

    void mousePressEvent(QMouseEvent *pEvent)
    {
        QPoint eventPosition(pEvent->pos().x() / m_fMultiplierX, pEvent->pos().y() / m_fMultiplierY);
        QWidget::mousePressEvent(pEvent);
        for (int i = 0; i < m_rows.size(); ++i)
        {
            QVector<UISoftKeyboardKey> &keys = m_rows[i].m_keys;
            for (int j = 0; j < keys.size(); ++j)
            {
                UISoftKeyboardKey &key = keys[j];
                if (key.polygonInGlobal().containsPoint(eventPosition, Qt::OddEvenFill))
                {
                    //printf ("%s\n", qPrintable(key.keyCap()));
                    break;
                }
            }
        }
    }

private:

    QSize m_minimumSize;
    int m_iInitialHeight;
    int m_iInitialWidth;
    float m_fMultiplierX;
    float m_fMultiplierY;

};


/*********************************************************************************************************************************
*   UISoftKeyboardKey implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardKey::UISoftKeyboardKey()
    : m_scanCode(0)
    , m_scanCodePrefix(0)
    , m_enmType(UIKeyType_Ordinary)
    , m_enmState(UIKeyState_NotPressed)
    , m_iWidth(0)
    , m_iHeight(0)
    , m_iSpaceWidthAfter(0)
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

void UISoftKeyboardKey::setType(UIKeyType enmType)
{
    m_enmType = enmType;
}

UIKeyType UISoftKeyboardKey::type() const
{
    return m_enmType;
}

UIKeyState UISoftKeyboardKey::state() const
{
    return m_enmState;
}

void UISoftKeyboardKey::release()
{
    updateState(false);
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

void UISoftKeyboardKey::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    // QToolButton::paintEvent(pEvent);
    // return;
}

void UISoftKeyboardKey::updateState(bool fPressed)
{
    if (m_enmType == UIKeyType_Ordinary)
        return;
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

    , m_fKeepAspectRatio(false)
    , m_iXSpacing(5)
    , m_iYSpacing(5)
    , m_iLeftMargin(10)
    , m_iTopMargin(10)
    , m_iRightMargin(10)
    , m_iBottomMargin(10)
{
    setAttribute(Qt::WA_DeleteOnClose);
    prepareObjects();
    parseLayout();
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

void UISoftKeyboard::sltHandleKeyPress()
{
//     UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
//     if (!pKey)
//         return;
//     if (pKey->type() == UIKeyType_Modifier)
//         return;

//     QVector<LONG> sequence;

//     /* Add the pressed modifiers first: */
//     for (int i = 0; i < m_pressedModifiers.size(); ++i)
//     {
//         UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
//         if (pModifier->scanCodePrefix() != 0)
//             sequence << pModifier->scanCodePrefix();
//         sequence << pModifier->scanCode();
//     }

//     if (pKey->scanCodePrefix() != 0)
//         sequence << pKey->scanCodePrefix();
//     sequence << pKey->scanCode();
//     keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltHandleKeyRelease()
{
    //UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
//     if (!pKey)
//         return;
//     /* We only send the scan codes of Ordinary keys: */
//     if (pKey->type() == UIKeyType_Modifier)
//         return;

//     QVector<LONG> sequence;
//     if (pKey->scanCodePrefix() != 0)
//         sequence <<  pKey->scanCodePrefix();
//     sequence << (pKey->scanCode() | 0x80);

//     /* Add the pressed modifiers in the reverse order: */
//     for (int i = m_pressedModifiers.size() - 1; i >= 0; --i)
//     {
//         UISoftKeyboardKey *pModifier = m_pressedModifiers[i];
//         if (pModifier->scanCodePrefix() != 0)
//             sequence << pModifier->scanCodePrefix();
//         sequence << (pModifier->scanCode() | 0x80);
//         /* Release the pressed modifiers (if there are not locked): */
//         pModifier->release();
//     }
//     keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltHandleModifierStateChange()
{
    // UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
    // if (!pKey)
    //     return;
    // if (pKey->type() != UIKeyType_Modifier)
    //     return;
    // if (pKey->state() == UIKeyState_NotPressed)
    // {
    //     m_pressedModifiers.removeOne(pKey);
    // }
    // else
    // {
    //     if (!m_pressedModifiers.contains(pKey))
    //         m_pressedModifiers.append(pKey);
    // }
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

void UISoftKeyboard::parseLayout()
{
    if (!m_pContainerWidget)
        return;
    UIKeyboardLayoutReader reader;
    QVector<UISoftKeyboardRow> &rows = m_pContainerWidget->m_rows;
    if (!reader.parseXMLFile(":/102_iso.xml", rows))
        return;
    int iY = m_iTopMargin;
    int iMaxWidth = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        UISoftKeyboardRow &row = rows[i];
        int iX = m_iLeftMargin;
        int iRowHeight = row.m_iDefaultHeight;
        for (int j = 0; j < row.m_keys.size(); ++j)
        {
            UISoftKeyboardKey &key = row.m_keys[j];
            key.setKeyGeometry(QRect(iX, iY, key.width(), key.height()));
            key.setPolygon(QPolygon(UIKeyboardLayoutReader::computeKeyVertices(key)));
            iX += key.width();
            if (j < row.m_keys.size() - 1)
                iX += m_iXSpacing;
            if (key.spaceWidthAfter() != 0)
                iX += (m_iXSpacing + key.spaceWidthAfter());
        }
        if (row.m_iSpaceHeightAfter != 0)
            iY += row.m_iSpaceHeightAfter + m_iYSpacing;
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
