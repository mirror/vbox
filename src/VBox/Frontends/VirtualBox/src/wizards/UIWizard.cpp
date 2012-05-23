/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizard class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QAbstractButton>
#include <QLayout>
#include <qmath.h>

/* Local includes: */
#include "UIWizard.h"
#include "UIWizardPage.h"
#include "VBoxGlobal.h"
#include "QIRichTextLabel.h"

int	UIWizard::exec()
{
    /* Prepare wizard: */
    prepare();
    /* Call to base-class: */
    return QWizard::exec();
}

void UIWizard::sltCurrentIdChanged(int iId)
{
    /* Hide/show description button disabled by default: */
    bool fIsHideShowDescriptionButtonAvailable = false;
    /* Enable hide/show description button for 1st page: */
    if (iId == 0)
        fIsHideShowDescriptionButtonAvailable = true;
    /* But first-run wizard has no such button anyway: */
    if (m_type == UIWizardType_FirstRun)
        fIsHideShowDescriptionButtonAvailable = false;
    /* Set a flag for hide/show description button finally: */
    setOption(QWizard::HaveCustomButton1, fIsHideShowDescriptionButtonAvailable);
}

void UIWizard::sltCustomButtonClicked(int iId)
{
    /* Handle 1st button: */
    if (iId == CustomButton1)
    {
        /* Cleanup: */
        cleanup();

        /* Compose wizard's name: */
        QString strWizardName = nameForType(m_type);
        /* Load mode settings: */
        QStringList wizards = vboxGlobal().virtualBox().GetExtraDataStringList(VBoxDefs::GUI_HideDescriptionForWizards);

        /* Switch mode: */
        switch (m_mode)
        {
            case UIWizardMode_Basic:
            {
                m_mode = UIWizardMode_Expert;
                if (!wizards.contains(strWizardName))
                    wizards << strWizardName;
                break;
            }
            case UIWizardMode_Expert:
            {
                m_mode = UIWizardMode_Basic;
                if (wizards.contains(strWizardName))
                    wizards.removeAll(strWizardName);
                break;
            }
            default:
            {
                AssertMsgFailed(("Invalid mode: %d", m_mode));
                break;
            }
        }

        /* Save mode settings: */
        vboxGlobal().virtualBox().SetExtraDataStringList(VBoxDefs::GUI_HideDescriptionForWizards, wizards);

        /* Prepare: */
        prepare();
    }
}

UIWizard::UIWizard(QWidget *pParent, UIWizardType type, UIWizardMode mode)
    : QIWithRetranslateUI<QWizard>(pParent)
    , m_type(type)
    , m_mode(mode == UIWizardMode_Auto ? loadModeForType(m_type) : mode)
{
#ifdef Q_WS_WIN
    /* Hide window icon: */
    setWindowIcon(QIcon());
#endif /* Q_WS_WIN */

#ifdef Q_WS_MAC
    /* I'm really not sure why there shouldn't be any default button on Mac OS X.
     * This prevents the using of Enter to jump to the next page. */
    setOptions(options() ^ QWizard::NoDefaultButton);
#endif /* Q_WS_MAC */

    /* Setup connections: */
    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(sltCurrentIdChanged(int)));
    connect(this, SIGNAL(customButtonClicked(int)), this, SLOT(sltCustomButtonClicked(int)));
}

void UIWizard::retranslateUi()
{
    /* Translate basic/expert button: */
    switch (m_mode)
    {
        case UIWizardMode_Basic: setButtonText(QWizard::CustomButton1, tr("Hide Description")); break;
        case UIWizardMode_Expert: setButtonText(QWizard::CustomButton1, tr("Show Description")); break;
        default: AssertMsgFailed(("Invalid mode: %d", m_mode)); break;
    }
}

void UIWizard::retranslatePages()
{
    /* Translate all the pages: */
    QList<int> ids = pageIds();
    for (int i = 0; i < ids.size(); ++i)
        qobject_cast<UIWizardPage*>(page(ids[i]))->retranslate();
}

void UIWizard::setPage(int iId, UIWizardPage *pPage)
{
    /* Configure page first: */
    configurePage(pPage);
    /* Add page finally: */
    QWizard::setPage(iId, pPage);
}

void UIWizard::prepare()
{
    /* Translate wizard: */
    retranslateUi();
    /* Translate wizard pages: */
    retranslatePages();

    /* Resize wizard to 'golden ratio': */
    resizeToGoldenRatio();

    /* Notify pages they are ready: */
    QList<int> ids = pageIds();
    for (int i = 0; i < ids.size(); ++i)
        qobject_cast<UIWizardPage*>(page(ids[i]))->markReady();

    /* Make sure custom buttons shown even if final page is first to show: */
    sltCurrentIdChanged(startId());
}

void UIWizard::cleanup()
{
    /* Remove all the pages: */
    QList<int> ids = pageIds();
    for (int i = ids.size() - 1; i >= 0 ; --i)
    {
        /* Get enumerated page ID: */
        int iId = ids[i];
        /* Get corresponding page: */
        QWizardPage *pWizardPage = page(iId);

        /* Remove page from the wizard: */
        removePage(iId);
        /* Delete page finally: */
        delete pWizardPage;
    }

#ifndef Q_WS_MAC
    /* Cleanup watermark: */
    if (!m_strWatermarkName.isEmpty())
        setPixmap(QWizard::WatermarkPixmap, QPixmap());
#endif /* !Q_WS_MAC */
}

void UIWizard::resizeToGoldenRatio()
{
    /* Check if wizard is in basic or expert mode: */
    if (m_mode == UIWizardMode_Expert)
    {
        /* Unfortunately QWizard hides some of useful API in private part,
         * and also have few layouting bugs which could be easy fixed
         * by that API, so we will use QWizard::restart() method
         * to call the same functionality indirectly...
         * Early call restart() which is usually goes on show()! */
        restart();

        /* Now we have correct label size-hint(s) for all the pages.
         * We have to make sure all the pages uses maximum available size-hint. */
        QSize maxOfSizeHints;
        QList<UIWizardPage*> pages = findChildren<UIWizardPage*>();
        /* Search for the maximum available size-hint: */
        foreach (UIWizardPage *pPage, pages)
        {
            maxOfSizeHints.rwidth() = pPage->sizeHint().width() > maxOfSizeHints.width() ?
                                      pPage->sizeHint().width() : maxOfSizeHints.width();
            maxOfSizeHints.rheight() = pPage->sizeHint().height() > maxOfSizeHints.height() ?
                                       pPage->sizeHint().height() : maxOfSizeHints.height();
        }
        /* Feat corresponding height: */
        maxOfSizeHints.setWidth(qMax((int)(1.5 * maxOfSizeHints.height()), maxOfSizeHints.width()));
        /* Use that size-hint for all the pages: */
        foreach (UIWizardPage *pPage, pages)
            pPage->setMinimumSize(maxOfSizeHints);

        /* Relayout widgets: */
        QList<QLayout*> layouts = findChildren<QLayout*>();
        foreach(QLayout *pLayout, layouts)
            pLayout->activate();

        /* Unfortunately QWizard hides some of useful API in private part,
         * BUT it also have few layouting bugs which could be easy fixed
         * by that API, so we will use QWizard::restart() method
         * to call the same functionality indirectly...
         * And now we call restart() after layout activation procedure! */
        restart();

        /* Resize it to minimum size: */
        resize(QSize(0, 0));
    }
    else
    {
        /* Use some small (!) initial QIRichTextLabel width: */
        int iInitialLabelWidth = 200;

        /* Resize wizard according that initial width,
         * actually there could be other content
         * which wants to be wider than that initial width. */
        resizeAccordingLabelWidth(iInitialLabelWidth);

        /* Get some (first) of those pages: */
        UIWizardPage *pPage = qobject_cast<UIWizardPage*>(page(0));
        /* Calculate actual label width: */
        int iPageWidth = pPage->minimumWidth();
        int iLeft, iTop, iRight, iBottom;
        pPage->layout()->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
        int iCurrentLabelWidth = iPageWidth - iLeft - iRight;
        /* Calculate summary margin length,
         * including margins of the page and the wizard: */
        int iMarginsLength = width() - iCurrentLabelWidth;

        /* Get current wizard width and height: */
        int iCurrentWizardWidth = width();
        int iCurrentWizardHeight = height();
#ifndef Q_WS_MAC
        /* We should take into account watermark like its assigned already: */
        QPixmap watermarkPixmap(m_strWatermarkName);
        int iWatermarkWidth = watermarkPixmap.width();
        iCurrentWizardWidth += iWatermarkWidth;
#endif /* !Q_WS_MAC */
        /* Calculating nearest to 'golden ratio' label width: */
        int iGoldenRatioWidth = (int)qSqrt(ratio() * iCurrentWizardWidth * iCurrentWizardHeight);
        int iProposedLabelWidth = iGoldenRatioWidth - iMarginsLength;
#ifndef Q_WS_MAC
        /* We should take into account watermark like its assigned already: */
        iProposedLabelWidth -= iWatermarkWidth;
#endif /* !Q_WS_MAC */

        /* Choose maximum between current and proposed label width: */
        int iNewLabelWidth = qMax(iCurrentLabelWidth, iProposedLabelWidth);

        /* Finally resize wizard according new label width,
         * taking into account all the content and 'golden ratio' rule: */
        resizeAccordingLabelWidth(iNewLabelWidth);
    }

#ifndef Q_WS_MAC
    /* Really assign watermark: */
    if (!m_strWatermarkName.isEmpty())
        assignWatermarkHelper();
#endif /* !Q_WS_MAC */
}

#ifndef Q_WS_MAC
void UIWizard::assignWatermark(const QString &strWatermark)
{
    if (wizardStyle() != QWizard::AeroStyle
# ifdef Q_WS_WIN
        /* There is a Qt bug about Windows7 do NOT match conditions for 'aero' wizard-style,
         * so its silently fallbacks to 'modern' one without any notification,
         * so QWizard::wizardStyle() returns QWizard::ModernStyle, while using aero, at least partially. */
        && QSysInfo::windowsVersion() != QSysInfo::WV_WINDOWS7
# endif /* Q_WS_WIN */
        )
        m_strWatermarkName = strWatermark;
}
#else
void UIWizard::assignBackground(const QString &strBackground)
{
    setPixmap(QWizard::BackgroundPixmap, strBackground);
}
#endif

void UIWizard::showEvent(QShowEvent *pShowEvent)
{
    /* Resize to minimum possible size: */
    resize(0, 0);

    /* Call to base-class: */
    QWizard::showEvent(pShowEvent);
}

void UIWizard::configurePage(UIWizardPage *pPage)
{
    /* Page margins: */
    switch (wizardStyle())
    {
        case QWizard::ClassicStyle:
        {
            int iLeft, iTop, iRight, iBottom;
            pPage->layout()->getContentsMargins(&iLeft, &iTop, &iRight, &iBottom);
            pPage->layout()->setContentsMargins(iLeft, iTop, 0, 0);
            break;
        }
        default:
            break;
    }
}

void UIWizard::resizeAccordingLabelWidth(int iLabelsWidth)
{
    /* Unfortunately QWizard hides some of useful API in private part,
     * and also have few layouting bugs which could be easy fixed
     * by that API, so we will use QWizard::restart() method
     * to call the same functionality indirectly...
     * Early call restart() which is usually goes on show()! */
    restart();

    /* Update QIRichTextLabel(s) text-width(s): */
    QList<QIRichTextLabel*> labels = findChildren<QIRichTextLabel*>();
    foreach (QIRichTextLabel *pLabel, labels)
        pLabel->setMinimumTextWidth(iLabelsWidth);

    /* Now we have correct label size-hint(s) for all the pages.
     * We have to make sure all the pages uses maximum available size-hint. */
    QSize maxOfSizeHints;
    QList<UIWizardPage*> pages = findChildren<UIWizardPage*>();
    /* Search for the maximum available size-hint: */
    foreach (UIWizardPage *pPage, pages)
    {
        maxOfSizeHints.rwidth() = pPage->sizeHint().width() > maxOfSizeHints.width() ?
                                  pPage->sizeHint().width() : maxOfSizeHints.width();
        maxOfSizeHints.rheight() = pPage->sizeHint().height() > maxOfSizeHints.height() ?
                                   pPage->sizeHint().height() : maxOfSizeHints.height();
    }
    /* Use that size-hint for all the pages: */
    foreach (UIWizardPage *pPage, pages)
        pPage->setMinimumSize(maxOfSizeHints);

    /* Relayout widgets: */
    QList<QLayout*> layouts = findChildren<QLayout*>();
    foreach(QLayout *pLayout, layouts)
        pLayout->activate();

    /* Unfortunately QWizard hides some of useful API in private part,
     * BUT it also have few layouting bugs which could be easy fixed
     * by that API, so we will use QWizard::restart() method
     * to call the same functionality indirectly...
     * And now we call restart() after layout activation procedure! */
    restart();

    /* Resize it to minimum size: */
    resize(QSize(0, 0));
}

double UIWizard::ratio()
{
    /* Default value: */
    double dRatio = 1.6;

#ifdef Q_WS_WIN
    switch (wizardStyle())
    {
        case QWizard::ClassicStyle:
        case QWizard::ModernStyle:
            /* There is a Qt bug about Windows7 do NOT match conditions for 'aero' wizard-style,
             * so its silently fallbacks to 'modern' one without any notification,
             * so QWizard::wizardStyle() returns QWizard::ModernStyle, while using aero, at least partially. */
            if (QSysInfo::windowsVersion() != QSysInfo::WV_WINDOWS7)
            {
                dRatio = 2;
                break;
            }
        case QWizard::AeroStyle:
            dRatio = 2.2;
            break;
        default:
            break;
    }
#endif /* Q_WS_WIN */

    switch (m_type)
    {
        case UIWizardType_CloneVM:
            dRatio -= 0.4;
            break;
        case UIWizardType_NewVD:
        case UIWizardType_CloneVD:
            dRatio += 0.1;
            break;
        case UIWizardType_ExportAppliance:
            dRatio += 0.3;
            break;
        case UIWizardType_FirstRun:
            dRatio += 0.3;
            break;
        default:
            break;
    }

    /* Return final result: */
    return dRatio;
}

#ifndef Q_WS_MAC
int UIWizard::proposedWatermarkHeight()
{
    /* We should calculate suitable height for watermark pixmap,
     * for that we have to take into account:
     * 1. wizard-layout top-margin (for modern style),
     * 2. wizard-header height,
     * 3. spacing between wizard-header and wizard-page,
     * 4. wizard-page height,
     * 5. wizard-layout bottom-margin (for modern style). */

    /* Get current application style: */
    QStyle *pStyle = QApplication::style();

    /* Acquire wizard-layout top-margin: */
    int iTopMargin = 0;
    if (m_mode == UIWizardMode_Basic)
    {
        if (wizardStyle() == QWizard::ModernStyle)
            iTopMargin = pStyle->pixelMetric(QStyle::PM_LayoutTopMargin);
    }

    /* Acquire wizard-header height: */
    int iTitleHeight = 0;
    if (m_mode == UIWizardMode_Basic)
    {
        /* We have no direct access to QWizardHeader inside QWizard private data...
         * From Qt sources it seems title font is hardcoded as current font point-size + 4: */
        QFont titleFont(QApplication::font());
        titleFont.setPointSize(titleFont.pointSize() + 4);
        QFontMetrics titleFontMetrics(titleFont);
        iTitleHeight = titleFontMetrics.height();
    }

    /* Acquire spacing between wizard-header and wizard-page: */
    int iMarginBetweenTitleAndPage = 0;
    if (m_mode == UIWizardMode_Basic)
    {
        /* We have no direct access to margin between QWizardHeader and wizard-pages...
         * From Qt sources it seems its hardcoded as just 7 pixels: */
        iMarginBetweenTitleAndPage = 7;
    }

    /* Acquire wizard-page height: */
    int iPageHeight = 0;
    if (page(0))
    {
        iPageHeight = page(0)->minimumSize().height();
    }

    /* Acquire wizard-layout bottom-margin: */
    int iBottomMargin = 0;
    if (wizardStyle() == QWizard::ModernStyle)
        iBottomMargin = pStyle->pixelMetric(QStyle::PM_LayoutBottomMargin);

    /* Finally, calculate summary height: */
    return iTopMargin + iTitleHeight + iMarginBetweenTitleAndPage + iPageHeight + iBottomMargin;
}

void UIWizard::assignWatermarkHelper()
{
    /* Create initial watermark: */
    QPixmap pixWaterMark(m_strWatermarkName);
    /* Convert watermark to image which
     * allows to manage pixel data directly: */
    QImage imgWatermark = pixWaterMark.toImage();
    /* Use the right-top watermark pixel as frame color: */
    QRgb rgbFrame = imgWatermark.pixel(imgWatermark.width() - 1, 0);
    /* Create final image on the basis of incoming, applying the rules: */
    QImage imgWatermarkNew(imgWatermark.width(), qMax(imgWatermark.height(), proposedWatermarkHeight()), imgWatermark.format());
    for (int y = 0; y < imgWatermarkNew.height(); ++y)
    {
        for (int x = 0; x < imgWatermarkNew.width(); ++x)
        {
            /* Border rule 1 - draw border for ClassicStyle */
            if (wizardStyle() == QWizard::ClassicStyle &&
                (x == 0 || y == 0 || x == imgWatermarkNew.width() - 1 || y == imgWatermarkNew.height() - 1))
                imgWatermarkNew.setPixel(x, y, rgbFrame);
            /* Border rule 2 - draw border for ModernStyle */
            else if (wizardStyle() == QWizard::ModernStyle && x == imgWatermarkNew.width() - 1)
                imgWatermarkNew.setPixel(x, y, rgbFrame);
            /* Horizontal extension rule - use last used color */
            else if (x >= imgWatermark.width() && y < imgWatermark.height())
                imgWatermarkNew.setPixel(x, y, imgWatermark.pixel(imgWatermark.width() - 1, y));
            /* Vertical extension rule - use last used color */
            else if (y >= imgWatermark.height() && x < imgWatermark.width())
                imgWatermarkNew.setPixel(x, y, imgWatermark.pixel(x, imgWatermark.height() - 1));
            /* Common extension rule - use last used color */
            else if (x >= imgWatermark.width() && y >= imgWatermark.height())
                imgWatermarkNew.setPixel(x, y, imgWatermark.pixel(imgWatermark.width() - 1, imgWatermark.height() - 1));
            /* Else just copy color */
            else
                imgWatermarkNew.setPixel(x, y, imgWatermark.pixel(x, y));
        }
    }
    /* Convert processed image to pixmap and assign it to wizard's watermark. */
    QPixmap pixWatermarkNew = QPixmap::fromImage(imgWatermarkNew);
    setPixmap(QWizard::WatermarkPixmap, pixWatermarkNew);
}
#endif /* !Q_WS_MAC */

/* static */
QString UIWizard::nameForType(UIWizardType type)
{
    QString strName;
    switch (type)
    {
        case UIWizardType_NewVM: strName = "NewVM"; break;
        case UIWizardType_CloneVM: strName = "CloneVM"; break;
        case UIWizardType_ExportAppliance: strName = "ExportAppliance"; break;
        case UIWizardType_ImportAppliance: strName = "ImportAppliance"; break;
        case UIWizardType_FirstRun: strName = "FirstRun"; break;
        case UIWizardType_NewVD: strName = "NewVD"; break;
        case UIWizardType_CloneVD: strName = "CloneVD"; break;
    }
    return strName;
}

/* static */
UIWizardMode UIWizard::loadModeForType(UIWizardType type)
{
    /* Some wizard use only basic mode: */
    if (type == UIWizardType_FirstRun)
        return UIWizardMode_Basic;
    /* Get mode from extra-data: */
    QStringList wizards = vboxGlobal().virtualBox().GetExtraDataStringList(VBoxDefs::GUI_HideDescriptionForWizards);
    if (wizards.contains(nameForType(type)))
        return UIWizardMode_Expert;
    /* Return mode: */
    return UIWizardMode_Basic;
}

