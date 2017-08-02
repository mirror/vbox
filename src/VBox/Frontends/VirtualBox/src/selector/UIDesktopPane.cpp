/* $Id$ */
/** @file
 * VBox Qt GUI - UIDesktopPane class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QAction>
# include <QGridLayout>
# include <QHBoxLayout>
# include <QLabel>
# include <QPainter>
# include <QStackedWidget>
# include <QStyle>
# include <QToolButton>
# include <QUuid>
# include <QVBoxLayout>

/* GUI includes */
# include "QILabel.h"
# include "QIWithRetranslateUI.h"
# include "UIDesktopPane.h"
# include "UIIconPool.h"
# include "VBoxUtils.h"

/* Other VBox includes: */
# include <iprt/assert.h>

/* Forward declarations: */
class QEvent;
class QGridLayout;
class QHBoxLayout;
class QIcon;
class QLabel;
class QPaintEvent;
class QResizeEvent;
class QString;
class QUuid;
class QVBoxLayout;
class QWidget;

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Our own skinnable implementation of tool widget header for UIDesktopPane. */
class UIToolWidgetHeader : public QLabel
{
    Q_OBJECT;

public:

    /** Constructs tool widget header passing @a pParent to the base-class. */
    UIToolWidgetHeader(QWidget *pParent = 0);

protected:

    /** Handles resuze @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;
};


/** Our own skinnable implementation of tool widget for UIDesktopPane. */
class UIToolWidget : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs tool widget on the basis of passed @a pAction and @a strDescription. */
    UIToolWidget(QAction *pAction, const QString &strDescription);

protected:

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Holds the action reference. */
    QAction *m_pAction;
    /** Holds the widget description. */
    QString  m_strDescription;

    /** Holds whether the widget is hovered. */
    bool  m_fHovered;

    /** Holds the main layout instance. */
    QGridLayout *m_pLayout;
    /** Holds the icon label instance. */
    QLabel      *m_pLabelIcon;
    /** Holds the name label instance. */
    QLabel      *m_pLabelName;
    /** Holds the description label instance. */
    QILabel     *m_pLabelDescription;
};


/** QStackedWidget subclass representing container which have two panes:
  * 1. Text pane reflecting base information about VirtualBox,
  * 2. Error pane reflecting information about currently chosen
  *    inaccessible VM and allowing to operate over it. */
class UIDesktopPanePrivate : public QIWithRetranslateUI<QStackedWidget>
{
    Q_OBJECT;

public:

    /** Constructs private desktop pane passing @a pParent to the base-class.
      * @param  pRefreshAction  Brings the refresh action reference. */
    UIDesktopPanePrivate(QWidget *pParent, QAction *pRefreshAction);

    /** Assigns @a strText and switches to text pane. */
    void setText(const QString &strText);
    /** Assigns @a strError and switches to error pane. */
    void setError(const QString &strError);

    /** Defines a tools pane welcome @a strText. */
    void setToolsPaneText(const QString &strText);
    /** Add a tool element.
      * @param  pAction         Brings tool action reference.
      * @param  strDescription  Brings the tool description. */
    void addToolDescription(QAction *pAction, const QString &strDescription);
    /** Removes all tool elements. */
    void removeToolDescriptions();

protected:

    /** Handles translation event. */
    void retranslateUi();

    /** Prepares text pane. */
    void prepareTextPane();
    /** Prepares error pane. */
    void prepareErrorPane();

    /** Prepares tools pane. */
    void prepareToolsPane();

private:

    /** Holds the text pane instance. */
    QRichTextBrowser *m_pText;

    /** Holds the error pane container. */
    QWidget *m_pErrBox;
    /** Holds the error label instance. */
    QLabel *m_pErrLabel;
    /** Holds the error text-browser instance. */
    QTextBrowser *m_pErrText;
    /** Holds the VM refresh button instance. */
    QToolButton *m_pRefreshButton;
    /** Holds the VM refresh action reference. */
    QAction *m_pRefreshAction;

    /** Holds the tools pane instance. */
    QWidget     *m_pToolsPane;
    /** Holds the tools pane widget layout instance. */
    QVBoxLayout *m_pLayoutWidget;
    /** Holds the tools pane text label instance. */
    QILabel     *m_pLabelToolsPaneText;
};


/*********************************************************************************************************************************
*   Class UIToolWidgetHeader implementation.                                                                                     *
*********************************************************************************************************************************/

UIToolWidgetHeader::UIToolWidgetHeader(QWidget *pParent /* = 0 */)
    : QLabel(pParent)
{
}

void UIToolWidgetHeader::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QLabel::resizeEvent(pEvent);

#if 0
    /* Get basic palette: */
    QPalette pal = qApp->palette();

    /* Prepare new foreground: */
    const QColor color0 = pal.color(QPalette::Active, QPalette::HighlightedText);
    const QColor color1 = pal.color(QPalette::Active, QPalette::WindowText);
    QLinearGradient grad(QPointF(0, 0), QPointF(width(), height()));
    {
        grad.setColorAt(0, color0);
        grad.setColorAt(1, color1);
    }

    /* Apply palette changes: */
    pal.setBrush(QPalette::Active, QPalette::WindowText, grad);
    setPalette(pal);
#endif
}

void UIToolWidgetHeader::paintEvent(QPaintEvent *pEvent)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
#ifdef VBOX_WS_MAC
    const QColor color0 = pal.color(QPalette::Highlight).lighter(105);
#else /* !VBOX_WS_MAC */
    const QColor color0 = pal.color(QPalette::Highlight).lighter(130);
#endif /* !VBOX_WS_MAC */
    QColor color1 = color0;
    color1.setAlpha(0);

    /* Prepare background: */
    QLinearGradient grad(QPointF(0, 0), QPointF(width(), height()));
    {
        grad.setColorAt(0,   color1);
        grad.setColorAt(0.2, color0);
        grad.setColorAt(1,   color1);
    }

    /* Paint background: */
    painter.fillRect(QRect(0, 0, width(), height()), grad);

    /* Call to base-class: */
    QLabel::paintEvent(pEvent);
}


/*********************************************************************************************************************************
*   Class UIToolWidget implementation.                                                                                           *
*********************************************************************************************************************************/

UIToolWidget::UIToolWidget(QAction *pAction, const QString &strDescription)
    : m_pAction(pAction)
    , m_strDescription(strDescription)
    , m_fHovered(false)
    , m_pLayout(0)
    , m_pLabelIcon(0)
    , m_pLabelName(0)
    , m_pLabelDescription(0)
{
    /* Prepare: */
    prepare();
}

bool UIToolWidget::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        /* Update the hovered state on/off: */
        case QEvent::Enter:
        {
            m_fHovered = true;
            update();
            break;
        }
        case QEvent::Leave:
        {
            m_fHovered = false;
            update();
            break;
        }

        /* Notify listeners about the item was clicked: */
        case QEvent::MouseButtonRelease:
        {
            m_pAction->trigger();
            break;
        }

        default:
            break;
    }
    /* Call to base-class: */
    return QWidget::event(pEvent);
}

void UIToolWidget::paintEvent(QPaintEvent * /* pEvent */)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
#ifdef VBOX_WS_MAC
    const QColor color0 = m_fHovered
                        ? pal.color(QPalette::Highlight).lighter(120)
                        : pal.color(QPalette::Base);
    const QColor color1 = color0.lighter(110);
#else /* !VBOX_WS_MAC */
    const QColor color0 = m_fHovered
                        ? pal.color(QPalette::Highlight).lighter(160)
                        : pal.color(QPalette::Base);
    const QColor color1 = color0.lighter(120);
#endif /* !VBOX_WS_MAC */
    QColor color2 = pal.color(QPalette::Window).lighter(110);
    color2.setAlpha(0);
    QColor color3 = pal.color(QPalette::Window).darker(200);

    /* Invent pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Background gradient: */
    QLinearGradient grad(QPointF(0, height()), QPointF(0, 0));
    {
        grad.setColorAt(0, color0);
        grad.setColorAt(1, color1);
    }

    /* Top-left corner: */
    QRadialGradient grad1(QPointF(iMetric, iMetric), iMetric);
    {
        grad1.setColorAt(0, color3);
        grad1.setColorAt(1, color2);
    }
    /* Top-right corner: */
    QRadialGradient grad2(QPointF(width() - iMetric, iMetric), iMetric);
    {
        grad2.setColorAt(0, color3);
        grad2.setColorAt(1, color2);
    }
    /* Bottom-left corner: */
    QRadialGradient grad3(QPointF(iMetric, height() - iMetric), iMetric);
    {
        grad3.setColorAt(0, color3);
        grad3.setColorAt(1, color2);
    }
    /* Botom-right corner: */
    QRadialGradient grad4(QPointF(width() - iMetric, height() - iMetric), iMetric);
    {
        grad4.setColorAt(0, color3);
        grad4.setColorAt(1, color2);
    }

    /* Top line: */
    QLinearGradient grad5(QPointF(iMetric, 0), QPointF(iMetric, iMetric));
    {
        grad5.setColorAt(0, color2);
        grad5.setColorAt(1, color3);
    }
    /* Bottom line: */
    QLinearGradient grad6(QPointF(iMetric, height()), QPointF(iMetric, height() - iMetric));
    {
        grad6.setColorAt(0, color2);
        grad6.setColorAt(1, color3);
    }
    /* Left line: */
    QLinearGradient grad7(QPointF(0, height() - iMetric), QPointF(iMetric, height() - iMetric));
    {
        grad7.setColorAt(0, color2);
        grad7.setColorAt(1, color3);
    }
    /* Right line: */
    QLinearGradient grad8(QPointF(width(), height() - iMetric), QPointF(width() - iMetric, height() - iMetric));
    {
        grad8.setColorAt(0, color2);
        grad8.setColorAt(1, color3);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(iMetric,           iMetric,            width() - iMetric * 2, height() - iMetric * 2), grad);
    painter.fillRect(QRect(0,                 0,                  iMetric,               iMetric),                grad1);
    painter.fillRect(QRect(width() - iMetric, 0,                  iMetric,               iMetric),                grad2);
    painter.fillRect(QRect(0,                 height() - iMetric, iMetric,               iMetric),                grad3);
    painter.fillRect(QRect(width() - iMetric, height() - iMetric, iMetric,               iMetric),                grad4);
    painter.fillRect(QRect(iMetric,           0,                  width() - iMetric * 2, iMetric),                grad5);
    painter.fillRect(QRect(iMetric,           height() - iMetric, width() - iMetric * 2, iMetric),                grad6);
    painter.fillRect(QRect(0,                 iMetric,            iMetric,               height() - iMetric * 2), grad7);
    painter.fillRect(QRect(width() - iMetric, iMetric,            iMetric,               height() - iMetric * 2), grad8);
}

void UIToolWidget::prepare()
{
    /* Configure self: */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    /* Create main layout: */
    m_pLayout = new QGridLayout(this);
    AssertPtrReturnVoid(m_pLayout);
    {
        /* Invent pixel metric: */
        const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.375;
        const int iMargin = iMetric / 2;
        const int iSpacing = iMargin / 2;

        /* Configure layout: */
        m_pLayout->setContentsMargins(iMargin + iSpacing, iMargin, iMargin, iMargin);
        m_pLayout->setSpacing(iSpacing);

        /* Create name label: */
        m_pLabelName = new UIToolWidgetHeader;
        AssertPtrReturnVoid(m_pLabelName);
        {
            /* Configure label: */
            QFont fontLabel = m_pLabelName->font();
            fontLabel.setBold(true);
            m_pLabelName->setFont(fontLabel);
            m_pLabelName->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_pLabelName->setText(QString("%1     ").arg(m_pAction->text().remove('&')));

            /* Add into layout: */
            m_pLayout->addWidget(m_pLabelName, 0, 0);
        }

        /* Create description label: */
        m_pLabelDescription = new QILabel;
        AssertPtrReturnVoid(m_pLabelDescription);
        {
            /* Configure label: */
            m_pLabelDescription->setAttribute(Qt::WA_TransparentForMouseEvents);
            m_pLabelDescription->setWordWrap(true);
            m_pLabelDescription->useSizeHintForWidth(400);
            m_pLabelDescription->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
            m_pLabelDescription->setText(m_strDescription);

            /* Add into layout: */
            m_pLayout->addWidget(m_pLabelDescription, 1, 0);
        }

        /* Create icon label: */
        m_pLabelIcon = new QLabel;
        AssertPtrReturnVoid(m_pLabelIcon);
        {
            /* Configure label: */
            m_pLabelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_pLabelIcon->setPixmap(m_pAction->icon().pixmap(iMetric));

            /* Add into layout: */
            m_pLayout->addWidget(m_pLabelIcon, 0, 1, 2, 1);
            m_pLayout->setAlignment(m_pLabelIcon, Qt::AlignHCenter | Qt::AlignTop);
        }
    }
}


/*********************************************************************************************************************************
*   Class UIDesktopPanePrivate implementation.                                                                                   *
*********************************************************************************************************************************/

UIDesktopPanePrivate::UIDesktopPanePrivate(QWidget *pParent, QAction *pRefreshAction)
    : QIWithRetranslateUI<QStackedWidget>(pParent)
    , m_pText(0)
    , m_pErrBox(0), m_pErrLabel(0), m_pErrText(0)
    , m_pRefreshButton(0), m_pRefreshAction(pRefreshAction)
    , m_pToolsPane(0), m_pLayoutWidget(0), m_pLabelToolsPaneText(0)
{
    /* Translate finally: */
    retranslateUi();
}

void UIDesktopPanePrivate::setText(const QString &strText)
{
    /* Prepare text pane if necessary: */
    prepareTextPane();

    /* Assign corresponding text: */
    m_pText->setText(strText);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pText));
}

void UIDesktopPanePrivate::setError(const QString &strError)
{
    /* Prepare error pane if necessary: */
    prepareErrorPane();

    /* Assign corresponding text: */
    m_pErrText->setText(strError);

    /* Raise corresponding widget: */
    setCurrentIndex(indexOf(m_pErrBox));
}

void UIDesktopPanePrivate::setToolsPaneText(const QString &strText)
{
    /* Prepare tools pane if necessary: */
    prepareToolsPane();

    /* Assign corresponding text: */
    m_pLabelToolsPaneText->setText(strText);

    /* Raise corresponding widget: */
    setCurrentWidget(m_pToolsPane);
}

void UIDesktopPanePrivate::addToolDescription(QAction *pAction, const QString &strDescription)
{
    /* Prepare tools pane if necessary: */
    prepareToolsPane();

    /* Add tool widget on the basis of passed description: */
    UIToolWidget *pWidget = new UIToolWidget(pAction, strDescription);
    AssertPtrReturnVoid(pWidget);
    {
        /* Add into layout: */
        m_pLayoutWidget->addWidget(pWidget);
    }

    /* Raise corresponding widget: */
    setCurrentWidget(m_pToolsPane);
}

void UIDesktopPanePrivate::removeToolDescriptions()
{
    /* Clear the layout: */
    QLayoutItem *pChild = 0;
    while ((pChild = m_pLayoutWidget->takeAt(0)) != 0)
    {
        /* Delete widget wrapped by the item first: */
        if (pChild->widget())
            delete pChild->widget();
        /* Then the item itself: */
        delete pChild;
    }
}

void UIDesktopPanePrivate::retranslateUi()
{
    /* Translate error-label text: */
    if (m_pErrLabel)
        m_pErrLabel->setText(QApplication::translate("UIDetailsPagePrivate",
                                 "The selected virtual machine is <i>inaccessible</i>. "
                                 "Please inspect the error message shown below and press the "
                                 "<b>Refresh</b> button if you want to repeat the accessibility check:"));

    /* Translate refresh button & action text: */
    if (m_pRefreshAction && m_pRefreshButton)
    {
        m_pRefreshButton->setText(m_pRefreshAction->text());
        m_pRefreshButton->setIcon(m_pRefreshAction->icon());
        m_pRefreshButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
}

void UIDesktopPanePrivate::prepareTextPane()
{
    if (m_pText)
        return;

    /* Create text pane: */
    m_pText = new QRichTextBrowser(this);
    m_pText->setFocusPolicy(Qt::StrongFocus);
    m_pText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    /* Make text pane transparent: */
    m_pText->setFrameShape(QFrame::NoFrame);
    m_pText->viewport()->setAutoFillBackground(false);
    m_pText->setOpenLinks(false);

    /* Add into the stack: */
    addWidget(m_pText);

    /* Retranslate finally: */
    retranslateUi();
}

void UIDesktopPanePrivate::prepareErrorPane()
{
    if (m_pErrBox)
        return;

    /* Create error pane: */
    m_pErrBox = new QWidget;

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(m_pErrBox);
#if   defined(VBOX_WS_MAC)
    pMainLayout->setContentsMargins(4, 5, 5, 5);
#elif defined(VBOX_WS_WIN)
    pMainLayout->setContentsMargins(3, 5, 5, 0);
#elif defined(VBOX_WS_X11)
    pMainLayout->setContentsMargins(0, 5, 5, 5);
#endif
    pMainLayout->setSpacing(10);

    /* Create error label: */
    m_pErrLabel = new QLabel(m_pErrBox);
    m_pErrLabel->setWordWrap(true);
    m_pErrLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    pMainLayout->addWidget(m_pErrLabel);

    /* Create error text-browser: */
    m_pErrText = new QTextBrowser(m_pErrBox);
    m_pErrText->setFocusPolicy(Qt::StrongFocus);
    m_pErrText->document()->setDefaultStyleSheet("a { text-decoration: none; }");
    pMainLayout->addWidget(m_pErrText);

    /* If refresh action was set: */
    if (m_pRefreshAction)
    {
        /* Create refresh button: */
        m_pRefreshButton = new QToolButton(m_pErrBox);
        m_pRefreshButton->setFocusPolicy(Qt::StrongFocus);

        /* Create refresh button layout: */
        QHBoxLayout *pButtonLayout = new QHBoxLayout;
        pMainLayout->addLayout(pButtonLayout);
        pButtonLayout->addStretch();
        pButtonLayout->addWidget(m_pRefreshButton);

        /* Connect refresh button: */
        connect(m_pRefreshButton, SIGNAL(clicked()), m_pRefreshAction, SIGNAL(triggered()));
    }

    pMainLayout->addStretch();

    /* Add into the stack: */
    addWidget(m_pErrBox);

    /* Retranslate finally: */
    retranslateUi();
}

void UIDesktopPanePrivate::prepareToolsPane()
{
    /* Do nothing if already exists: */
    if (m_pToolsPane)
        return;

    /* Create tool pane: */
    m_pToolsPane = new QWidget;
    AssertPtrReturnVoid(m_pToolsPane);
    {
        /* Create main layout: */
        QVBoxLayout *pMainLayout = new QVBoxLayout(m_pToolsPane);
        AssertPtrReturnVoid(pMainLayout);
        {
            /* Create welcome layout: */
            QHBoxLayout *pLayoutWelcome = new QHBoxLayout;
            AssertPtrReturnVoid(pLayoutWelcome);
            {
                /* Invent pixel metric: */
                const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

                /* Configure layout: */
                pLayoutWelcome->setContentsMargins(iMetric, 0, 0, 0);
                pLayoutWelcome->setSpacing(10);

                /* Create welcome text label: */
                m_pLabelToolsPaneText = new QILabel;
                AssertPtrReturnVoid(m_pLabelToolsPaneText);
                {
                    /* Configure label: */
                    m_pLabelToolsPaneText->setWordWrap(true);
                    m_pLabelToolsPaneText->useSizeHintForWidth(200);
                    m_pLabelToolsPaneText->setAlignment(Qt::AlignLeading | Qt::AlignTop);
                    m_pLabelToolsPaneText->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

                    /* Add into layout: */
                    pLayoutWelcome->addWidget(m_pLabelToolsPaneText);
                }

                /* Create picture label: */
                QLabel *pLabelPicture = new QLabel;
                AssertPtrReturnVoid(pLabelPicture);
                {
                    /* Configure label: */
                    const QIcon icon = UIIconPool::iconSet(":/tools_200px.png");
                    pLabelPicture->setPixmap(icon.pixmap(QSize(200, 200)));
                    pLabelPicture->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

                    /* Add into layout: */
                    pLayoutWelcome->addWidget(pLabelPicture);
                    pLayoutWelcome->setAlignment(pLabelPicture, Qt::AlignHCenter | Qt::AlignTop);
                }

                /* Add into layout: */
                pMainLayout->addLayout(pLayoutWelcome);
            }

            /* Create widget layout: */
            m_pLayoutWidget = new QVBoxLayout;
            AssertPtrReturnVoid(m_pLayoutWidget);
            {
                /* Add into layout: */
                pMainLayout->addLayout(m_pLayoutWidget);
            }

            /* Add stretch: */
            pMainLayout->addStretch();
        }

        /* Add into the stack: */
        addWidget(m_pToolsPane);
    }
}


/*********************************************************************************************************************************
*   Class UIDesktopPane implementation.                                                                                          *
*********************************************************************************************************************************/

UIDesktopPane::UIDesktopPane(QAction *pRefreshAction /* = 0 */, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
{
    /* Prepare main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    pMainLayout->setContentsMargins(0, 0, 0, 0);

    /* Create desktop pane: */
    m_pDesktopPrivate = new UIDesktopPanePrivate(this, pRefreshAction);

    /* Add it to the layout: */
    pMainLayout->addWidget(m_pDesktopPrivate);
}

void UIDesktopPane::updateDetailsText(const QString &strText)
{
    m_pDesktopPrivate->setText(strText);
}

void UIDesktopPane::updateDetailsError(const QString &strError)
{
    m_pDesktopPrivate->setError(strError);
}

void UIDesktopPane::setToolsPaneText(const QString &strText)
{
    m_pDesktopPrivate->setToolsPaneText(strText);
}

void UIDesktopPane::addToolDescription(QAction *pAction, const QString &strDescription)
{
    m_pDesktopPrivate->addToolDescription(pAction, strDescription);
}

void UIDesktopPane::removeToolDescriptions()
{
    m_pDesktopPrivate->removeToolDescriptions();
}

#include "UIDesktopPane.moc"

