/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialogButtonBox class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "QIDialogButtonBox.h"

#include <iprt/assert.h>

/* Qt includes */
#include <QPushButton>
#include <QEvent>
#include <QBoxLayout>

QIDialogButtonBox::QIDialogButtonBox (StandardButtons aButtons, Qt::Orientation aOrientation, QWidget *aParent)
   : QIWithRetranslateUI<QDialogButtonBox> (aParent)
{
    setOrientation (aOrientation);
    setStandardButtons (aButtons);

    retranslateUi();
}

QPushButton *QIDialogButtonBox::addButton (const QString &aText, ButtonRole aRole)
{
    QPushButton *btn = QDialogButtonBox::addButton (aText, aRole);
    retranslateUi();
    return btn;
}

QPushButton *QIDialogButtonBox::addButton (StandardButton aButton)
{
    QPushButton *btn = QDialogButtonBox::addButton (aButton);
    retranslateUi();
    return btn;
}

void QIDialogButtonBox::setStandardButtons (StandardButtons aButtons)
{
    QDialogButtonBox::setStandardButtons (aButtons);
    retranslateUi();
}

void QIDialogButtonBox::addExtraWidget (QWidget* aWidget)
{
    QBoxLayout *layout = boxLayout();
    int index = findEmptySpace (layout);
    layout->insertWidget (index + 1, aWidget);
    layout->insertStretch(index + 2);
}

void QIDialogButtonBox::addExtraLayout (QLayout* aLayout)
{
    QBoxLayout *layout = boxLayout();
    int index = findEmptySpace (layout);
    layout->insertLayout (index + 1, aLayout);
    layout->insertStretch(index + 2);
}

QBoxLayout *QIDialogButtonBox::boxLayout() const
{
  QBoxLayout *boxlayout = qobject_cast<QBoxLayout*> (layout());
  AssertMsg (VALID_PTR (boxlayout), ("Layout of the QDialogButtonBox isn't a box layout."));
  return boxlayout;
}

int QIDialogButtonBox::findEmptySpace (QBoxLayout *aLayout) const
{
  /* Search for the first occurrence of QSpacerItem and return the index.
   * Please note that this is Qt internal, so it may change at any time. */
  int i=0;
  for (; i < aLayout->count(); ++i)
  {
      QLayoutItem *item = aLayout->itemAt(i);
      if (QSpacerItem *sitem = item->spacerItem())
          break;
  }
  return i;
}

void QIDialogButtonBox::retranslateUi()
{
    QPushButton *btn = button (QDialogButtonBox::Help);
    if (btn)
    {
        btn->setText (tr ("&Help"));
        if (btn->shortcut().isEmpty())
            btn->setShortcut (QKeySequence (tr ("F1"))); 
    }
}
