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

private:

    QSize m_minimumSize;
};
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

struct SoftKeyboardKey
{
    /** Width and height might be inherited from the row and/or overwritten in row settings. */
    int       m_iWidth;
    int       m_iHeight;
    LONG      m_strScanCode;
    LONG      m_strScanCodePrefix;
    UIKeyType m_enmType;
    QString   m_strKeyCap;
    int       m_iSpaceWidthAfter;
    int       m_iCutoutWidth;
    int       m_iCutoutHeight;
    /** -1 is no cutout. 0 is the topleft corner. we go clockwise. */
    int       m_iCutoutCorner;
};

struct SoftKeyboardRow
{
    /** Default width and height might be inherited from the layout and overwritten in row settings. */
    int m_iDefaultWidth;
    int m_iDefaultHeight;
    int m_iStartingSpace;
    QList<SoftKeyboardKey> m_keys;
};

struct SoftKeyboardLayout
{
    int m_iDefaultWidth;
    int m_iDefaultHeight;
    QList<SoftKeyboardRow> m_rows;
};


/*********************************************************************************************************************************
*   UISoftKeyboardKey definition.                                                                                  *
*********************************************************************************************************************************/

class UISoftKeyboardKey : public QToolButton
{
    Q_OBJECT;

signals:

    void sigStateChanged();

public:

    UISoftKeyboardKey(QWidget *pParent = 0);

    void setWidth(int iWidth);
    int width() const;

    void setScanCode(LONG scanCode);
    LONG scanCode() const;

    void setScanCodePrefix(LONG scanCode);
    LONG scanCodePrefix() const;

    void setSpaceAfter(int iSpace);
    int spaceAfter() const;

    void setType(UIKeyType enmType);
    UIKeyType type() const;

    void setScaleMultiplier(float fMultiplier);

    UIKeyState state() const;
    QVector<LONG> scanCodeWithPrefix() const;
    void updateFontSize();

    void release();
    void setVertices(const QVector<QPoint> &vertices);

protected:

    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    virtual void paintEvent(QPaintEvent *pPaintEvent) /* override */;

private:

    void updateState(bool fPressed);
    void updateBackground();

    int   m_iWidth;
    int   m_iDefaultPixelSize;
    int   m_iDefaultPointSize;
    int   m_iSpaceAfter;
    LONG  m_scanCode;
    LONG  m_scanCodePrefix;
    UIKeyType m_enmType;
    UIKeyState m_enmState;
    QPalette     m_defaultPalette;
    float        m_fScaleMultiplier;
    QVector<QPoint> m_vertices;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader definition.                                                                                  *
*********************************************************************************************************************************/

class UIKeyboardLayoutReader
{

public:

    bool parseXMLFile(const QString &strFileName, SoftKeyboardLayout &layout);
    static QVector<QPoint> computeKeyVertices(const SoftKeyboardKey &key);
private:

    void  parseKey(SoftKeyboardRow &row);
    void  parseRow(SoftKeyboardLayout &layout);
    void  parseSpace(SoftKeyboardRow &row);
    void  parseCutout(SoftKeyboardKey &key);

    QXmlStreamReader m_xmlReader;
};

/*********************************************************************************************************************************
*   UIKeyboardLayoutReader implementation.                                                                                  *
*********************************************************************************************************************************/

bool UIKeyboardLayoutReader::parseXMLFile(const QString &strFileName, SoftKeyboardLayout &layout)
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
    layout.m_iDefaultWidth = attributes.value("defaultWidth").toInt();
    layout.m_iDefaultHeight = attributes.value("defaultHeight").toInt();

    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "row")
        {
            parseRow(layout);
        }
        else
            m_xmlReader.skipCurrentElement();
    }

    return true;
}

void UIKeyboardLayoutReader::parseRow(SoftKeyboardLayout &layout)
{
    SoftKeyboardRow row;

    row.m_iDefaultWidth = layout.m_iDefaultWidth;
    row.m_iDefaultHeight = layout.m_iDefaultHeight;

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
            parseSpace(row);
        else
            m_xmlReader.skipCurrentElement();
    }
    layout.m_rows.append(row);
}

void UIKeyboardLayoutReader::parseKey(SoftKeyboardRow &row)
{
    SoftKeyboardKey key;
    key.m_iWidth = row.m_iDefaultWidth;
    key.m_iHeight = row.m_iDefaultHeight;
    key.m_iSpaceWidthAfter = 0;
    key.m_iCutoutCorner = -1;
    while (m_xmlReader.readNextStartElement())
    {
        if (m_xmlReader.name() == "width")
            key.m_iWidth = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "height")
            key.m_iHeight = m_xmlReader.readElementText().toInt();
        else if (m_xmlReader.name() == "scancode")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.m_strScanCode = strCode.toInt(&fOk, 16);
        }
        else if (m_xmlReader.name() == "scancodeprefix")
        {
            QString strCode = m_xmlReader.readElementText();
            bool fOk = false;
            key.m_strScanCodePrefix = strCode.toInt(&fOk, 16);
        }
        else if (m_xmlReader.name() == "keycap")
        {
            if (key.m_strKeyCap.isEmpty())
                key.m_strKeyCap = m_xmlReader.readElementText();
            else
                key.m_strKeyCap += "\n" + m_xmlReader.readElementText();
        }
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
    row.m_keys.append(key);
}

void UIKeyboardLayoutReader::parseSpace(SoftKeyboardRow &row)
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
    row.m_keys.back().m_iSpaceWidthAfter = iWidth;
}

void UIKeyboardLayoutReader::parseCutout(SoftKeyboardKey &key)
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

QVector<QPoint> UIKeyboardLayoutReader::computeKeyVertices(const SoftKeyboardKey &key)
{
    QVector<QPoint> vertices;

    if (key.m_iCutoutCorner == -1 || key.m_iWidth <= key.m_iCutoutWidth || key.m_iHeight <= key.m_iCutoutHeight)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.m_iWidth, 0));
        vertices.append(QPoint(key.m_iWidth, key.m_iHeight));
        vertices.append(QPoint(0, key.m_iHeight));
        return vertices;
    }
    if (key.m_iCutoutCorner == 0)
    {
        vertices.append(QPoint(key.m_iCutoutWidth, 0));
        vertices.append(QPoint(key.m_iWidth, 0));
        vertices.append(QPoint(key.m_iWidth, key.m_iHeight));
        vertices.append(QPoint(0, key.m_iHeight));
        vertices.append(QPoint(0, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iCutoutWidth, key.m_iCutoutHeight));
    }
    else if (key.m_iCutoutCorner == 1)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.m_iWidth - key.m_iCutoutWidth, 0));
        vertices.append(QPoint(key.m_iWidth - key.m_iCutoutWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iWidth, key.m_iHeight));
        vertices.append(QPoint(0, key.m_iHeight));
    }
    else if (key.m_iCutoutCorner == 2)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.m_iWidth, 0));
        vertices.append(QPoint(key.m_iWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iWidth - key.m_iCutoutWidth, key.m_iCutoutHeight));
        vertices.append(QPoint(key.m_iWidth - key.m_iCutoutWidth, key.m_iHeight));
        vertices.append(QPoint(0, key.m_iHeight));
    }
    else if (key.m_iCutoutCorner == 3)
    {
        vertices.append(QPoint(0, 0));
        vertices.append(QPoint(key.m_iWidth, 0));
        vertices.append(QPoint(key.m_iWidth, key.m_iHeight));
        vertices.append(QPoint(key.m_iCutoutWidth, key.m_iHeight));
        vertices.append(QPoint(key.m_iCutoutWidth, key.m_iHeight - key.m_iCutoutHeight));
        vertices.append(QPoint(0, key.m_iHeight - key.m_iCutoutHeight));
    }
    return vertices;
}


/*********************************************************************************************************************************
*   UISoftKeyboardKey implementation.                                                                                  *
*********************************************************************************************************************************/

UISoftKeyboardKey::UISoftKeyboardKey(QWidget *pParent /* = 0 */)
    :QToolButton(pParent)
    , m_iWidth(1)
    , m_iSpaceAfter(0)
    , m_scanCode(0)
    , m_scanCodePrefix(0)
    , m_enmType(UIKeyType_Ordinary)
    , m_enmState(UIKeyState_NotPressed)
    , m_fScaleMultiplier(1.f)
{
    m_iDefaultPointSize = font().pointSize();
    m_iDefaultPixelSize = font().pixelSize();
    m_defaultPalette = palette();
}

void UISoftKeyboardKey::setWidth(int iWidth)
{
    m_iWidth = iWidth;
}

int UISoftKeyboardKey::width() const
{
    return m_iWidth;
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

void UISoftKeyboardKey::setSpaceAfter(int iSpace)
{
    m_iSpaceAfter = iSpace;
}

int UISoftKeyboardKey::spaceAfter() const
{
    return m_iSpaceAfter;
}

void UISoftKeyboardKey::setType(UIKeyType enmType)
{
    m_enmType = enmType;
}

UIKeyType UISoftKeyboardKey::type() const
{
    return m_enmType;
}

void UISoftKeyboardKey::setScaleMultiplier(float fMultiplier)
{
    m_fScaleMultiplier = fMultiplier;
}

UIKeyState UISoftKeyboardKey::state() const
{
    return m_enmState;
}

void UISoftKeyboardKey::updateFontSize()
{
    QFont newFont = font();
    if (m_iDefaultPointSize != -1)
        newFont.setPointSize(m_fScaleMultiplier * m_iDefaultPointSize);
    else
        newFont.setPixelSize(m_fScaleMultiplier * m_iDefaultPixelSize);
    setFont(newFont);
}

void UISoftKeyboardKey::release()
{
    updateState(false);
}

void UISoftKeyboardKey::setVertices(const QVector<QPoint> &vertices)
{
    m_vertices = vertices;
}

void UISoftKeyboardKey::mousePressEvent(QMouseEvent *pEvent)
{
    QToolButton::mousePressEvent(pEvent);
    updateState(true);
}

void UISoftKeyboardKey::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    // QToolButton::paintEvent(pEvent);
    // return;
    QPainter painter(this);
    QFont painterFont(font());
    painterFont.setPixelSize(16);
    painter.setFont(painterFont);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(0, 0,0), 2));
    painter.setBrush(QBrush(QColor(255, 0,0)));
    painter.drawPolygon(&(m_vertices[0]), m_vertices.size());
    QRect textRect(0, 0, width(), height());
    painter.drawText(textRect, Qt::TextWordWrap, text());
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
    emit sigStateChanged();
    updateBackground();
}

 void UISoftKeyboardKey::updateBackground()
 {
     if (m_enmState == UIKeyState_NotPressed)
     {
         setPalette(m_defaultPalette);
         return;
     }
     QColor newColor;

     if (m_enmState == UIKeyState_Pressed)
         newColor = QColor(20, 255, 42);
     else
         newColor = QColor(255, 7, 58);

     setAutoFillBackground(true);
     QPalette currentPalette = palette();
     currentPalette.setColor(QPalette::Button, newColor);

     setPalette(currentPalette);
     update();
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
    , m_iTotalRowHeight(0)
    , m_iMaxRowWidth(0)
    , m_fKeepAspectRatio(false)
    , m_iXSpacing(5)
    , m_iYSpacing(5)
    , m_iLeftMargin(5)
    , m_iTopMargin(5)
    , m_iRightMargin(5)
    , m_iBottomMargin(5)
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

void UISoftKeyboard::resizeEvent(QResizeEvent *pEvent)
{
    QIWithRetranslateUI<QMainWindow>::resizeEvent(pEvent);
    //updateLayout();
}

void UISoftKeyboard::sltHandleKeyPress()
{
    UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
    if (!pKey)
        return;
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
    keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltHandleKeyRelease()
{
    UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
    if (!pKey)
        return;
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
    keyboard().PutScancodes(sequence);
}

void UISoftKeyboard::sltHandleModifierStateChange()
{
    UISoftKeyboardKey *pKey = qobject_cast<UISoftKeyboardKey*>(sender());
    if (!pKey)
        return;
    if (pKey->type() != UIKeyType_Modifier)
        return;
    if (pKey->state() == UIKeyState_NotPressed)
    {
        m_pressedModifiers.removeOne(pKey);
    }
    else
    {
        if (!m_pressedModifiers.contains(pKey))
            m_pressedModifiers.append(pKey);
    }
}

void UISoftKeyboard::prepareObjects()
{

    /* Create layout: */
    // m_pMainLayout = new QHBoxLayout(this);
    // if (!m_pMainLayout)
    //     return;
    m_pContainerWidget = new UISoftKeyboardWidget;
    if (!m_pContainerWidget)
        return;
    m_pContainerWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    m_pContainerWidget->setStyleSheet("background-color:red;");
    //m_pMainLayout->addWidget(m_pContainerWidget);
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
    UIKeyboardLayoutReader reader;
    SoftKeyboardLayout layout;
    layout.m_iDefaultWidth = 0;
    layout.m_iDefaultHeight = 0;
    if (!reader.parseXMLFile(":/102_iso.xml", layout))
        return;
    int iY = m_iTopMargin;
    int iMaxWidth = 0;
    for (int i = 0; i < layout.m_rows.size(); ++i)
    {
        m_keys.append(QVector<UISoftKeyboardKey*>());
        const SoftKeyboardRow &row = layout.m_rows[i];
        int iX = m_iLeftMargin;
        QVector<UISoftKeyboardKey*> &rowKeyVector = m_keys.back();
        int iRowHeight = row.m_iDefaultHeight;
        for (int j = 0; j < row.m_keys.size(); ++j)
        {
            const SoftKeyboardKey &key = row.m_keys[j];
            UISoftKeyboardKey *pKey = new UISoftKeyboardKey(m_pContainerWidget);
            rowKeyVector.append(pKey);
            pKey->setText(key.m_strKeyCap);
            pKey->setGeometry(iX, iY, key.m_iWidth, key.m_iHeight);
            pKey->setWidth(key.m_iWidth);
            // printf("key.m_iWidth %d\n", pKey->width());
            pKey->setVertices(UIKeyboardLayoutReader::computeKeyVertices(key));
            iX += key.m_iWidth;
            if (j < row.m_keys.size() - 1)
                iX += m_iXSpacing;
            if (key.m_iSpaceWidthAfter != 0)
                iX += (m_iXSpacing + key.m_iSpaceWidthAfter);
            iRowHeight = qMax(iRowHeight, key.m_iHeight);
        }
        iMaxWidth = qMax(iMaxWidth, iX);
        iY += iRowHeight;
        if (i < layout.m_rows.size() - 1)
            iY += m_iYSpacing;
    }
    m_pContainerWidget->setNewMinimumSize(QSize(iMaxWidth + m_iRightMargin, iY + m_iBottomMargin));
}

void UISoftKeyboard::updateLayout()
{
    if (!m_pContainerWidget)
        return;

    QSize containerSize(m_pContainerWidget->size());
    if (containerSize.width() == 0 || containerSize.height() == 0)
        return;

    float fMultiplierX = containerSize.width() / (float) m_iMaxRowWidth;
    float fMultiplierY = containerSize.height() / (float) m_iTotalRowHeight;
    float fMultiplier = fMultiplierX;

    if (fMultiplier * m_iTotalRowHeight > containerSize.height())
        fMultiplier = fMultiplierY;

    // int y = 0;
    // int totalHeight = 0;
    // int totalWidth = 0;
    //for (int i = 0; i < layout.m_rows.size(); ++i)
    {
    //     UISoftKeyboardRow *pRow = m_rows[i];
    //     if (!pRow)
    //         continue;
    //     if(m_fKeepAspectRatio)
    //     {
    //         pRow->setGeometry(0, y, fMultiplier * pRow->unscaledWidth(), fMultiplier * pRow->unscaledHeight());
    //         pRow->setVisible(true);
    //         y += fMultiplier * pRow->unscaledHeight();
    //         totalWidth += fMultiplier * pRow->unscaledWidth();
    //         totalHeight += fMultiplier * pRow->unscaledHeight();
    //     }
    //     else
    //     {
    //         pRow->setGeometry(0, y, fMultiplierX * pRow->unscaledWidth(), fMultiplierY * pRow->unscaledHeight());
    //         pRow->setVisible(true);
    //         y += fMultiplierY * pRow->unscaledHeight();
    //         totalWidth += fMultiplierX * pRow->unscaledWidth();
    //         totalHeight += fMultiplierY * pRow->unscaledHeight();
    //     }
    //     pRow->updateLayout();
    }
}

CKeyboard& UISoftKeyboard::keyboard() const
{
    return m_pSession->keyboard();
}

#include "UISoftKeyboard.moc"
