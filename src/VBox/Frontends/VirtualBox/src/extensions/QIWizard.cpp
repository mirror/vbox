/* $Id$ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * QIWizard class implementation
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QAbstractButton>
#include <QLayout>
#include <QTextEdit>

/* Local includes */
#include "QIWizard.h"
#include "QILabel.h"
#include "VBoxGlobal.h"

/* System includes */
#include <math.h>

QIWizard::QIWizard(QWidget *pParent)
    : QIWithRetranslateUI<QWizard>(pParent)
    , m_iMinimumContentWidth(0)
{
#ifdef Q_WS_MAC
    /* I'm really not sure why there shouldn't be any default button on Mac OS
     * X. This prevents the using of Enter to jump to the next page. */
    setOptions(options() ^ QWizard::NoDefaultButton);
#endif /* Q_WS_MAC */
}

void QIWizard::resizeToGoldenRatio()
{
    /* Random initial QILabel width() to be adjusted! */
    int iLabelsWidth = 400;
    resizeAccordingLabelWidth(iLabelsWidth);

    /* Label delta for 'golden ratio' calculation. */
    int iLabelDelta = width() - iLabelsWidth;

    /* Calculating nearest 'golden ratio' width. */
    int iGoldRatioWidth = (int)sqrt(1.61 * width() * height());
    int iNewLabelWidth = iGoldRatioWidth - iLabelDelta;
    resizeAccordingLabelWidth(iNewLabelWidth);
    m_iMinimumContentWidth = iNewLabelWidth;
}

void QIWizard::assignWatermark(const QString &strWatermark)
{
    /* Create initial watermark. */
    QPixmap pixWaterMark(strWatermark);

    /* Convert watermark to image which
     * allows to manage pixel data directly. */
    QImage imgWatermark = pixWaterMark.toImage();

    /* Use the right-top watermark pixel as frame color */
    QRgb rgbFrame = imgWatermark.pixel(imgWatermark.width() - 1, 0);

    /* Take into account button's height */
    int iPageHeight = height() - button(QWizard::CancelButton)->height();

    /* Create final image on the basis of incoming, applying the rules. */
    QImage imgWatermarkNew(imgWatermark.width(), qMax(imgWatermark.height(), iPageHeight), imgWatermark.format());
    for (int y = 0; y < imgWatermarkNew.height(); ++ y)
    {
        for (int x = 0; x < imgWatermarkNew.width(); ++ x)
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

void QIWizard::assignBackground(const QString &strBg)
{
    setPixmap(QWizard::BackgroundPixmap, strBg);
}

void QIWizard::resizeAccordingLabelWidth(int iLabelsWidth)
{
    /* Update QILabels size-hints */
    QList<QILabel*> labels = findChildren<QILabel*>();
    foreach (QILabel *pLabel, labels)
    {
        pLabel->useSizeHintForWidth(iLabelsWidth);
        pLabel->updateGeometry();
    }

    /* Unfortunately QWizard hides some of useful API in private part,
     * BUT it also have few layouting bugs which could be easy fixed
     * by that API, so we will use QWizard::restart() method
     * to call the same functionality indirectly...
     * Early call restart() which is usually goes on show()! */
    restart();

    /* Now we have correct size-hints calculated for all the pages.
     * We have to make sure all the pages uses maximum size-hint
     * of all the available. */
    QSize maxOfSizeHints;
    QList<QIWizardPage*> pages = findChildren<QIWizardPage*>();
    /* Search for the maximum available size-hint */
    foreach (QIWizardPage *pPage, pages)
    {
        maxOfSizeHints.rwidth() = pPage->sizeHint().width() > maxOfSizeHints.width() ?
                                  pPage->sizeHint().width() : maxOfSizeHints.width();
        maxOfSizeHints.rheight() = pPage->sizeHint().height() > maxOfSizeHints.height() ?
                                   pPage->sizeHint().height() : maxOfSizeHints.height();
    }
    /* Use that size-hint for all the pages */
    foreach (QIWizardPage *pPage, pages)
    {
        pPage->setMinimumSizeHint(maxOfSizeHints);
        pPage->updateGeometry();
    }

    /* Reactivate layouts tree */
    QList<QLayout*> layouts = findChildren<QLayout*>();
    foreach (QLayout *pLayout, layouts)
        pLayout->activate();

    /* Unfortunately QWizard hides some of useful API in private part,
     * BUT it also have few layouting bugs which could be easy fixed
     * by that API, so we will use QWizard::restart() method
     * to call the same functionality indirectly...
     * And now we call restart() after layout update procedure! */
    restart();

    /* Resize to minimum possible size */
    resize(minimumSizeHint());
}

void QIWizard::retranslateAllPages()
{
    QList<QIWizardPage*> pages = findChildren<QIWizardPage*>();
    for(int i=0; i < pages.size(); ++i)
        static_cast<QIWizardPage*>(pages.at((i)))->retranslateUi();
}

QIWizardPage::QIWizardPage()
{
}

QSize QIWizardPage::minimumSizeHint() const
{
    return m_MinimumSizeHint.isValid() ? m_MinimumSizeHint : QWizardPage::minimumSizeHint();
}

void QIWizardPage::setMinimumSizeHint(const QSize &minimumSizeHint)
{
    m_MinimumSizeHint = minimumSizeHint;
}

QString QIWizardPage::standardHelpText() const
{
    return tr("Use the <b>%1</b> button to go to the next page of the wizard and the "
              "<b>%2</b> button to return to the previous page. "
              "You can also press <b>%3</b> if you want to cancel the execution "
              "of this wizard.</p>")
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::NextButton).remove(" >"))))
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::BackButton).remove("< "))))
#ifdef Q_WS_MAC
        .arg(QKeySequence("ESC").toString()); /* There is no button shown on Mac OS X, so just say the key sequence. */
#else /* Q_WS_MAC */
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::CancelButton))));
#endif /* Q_WS_MAC */
}

void QIWizardPage::startProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(false);
}

void QIWizardPage::endProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(true);
}

QIWizard* QIWizardPage::wizard() const
{
    return qobject_cast<QIWizard*>(QWizardPage::wizard());
}

