/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxSettingsSelector class declaration
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

#ifndef __VBoxSettingsSelector_h__
#define __VBoxSettingsSelector_h__

/* Qt includes */
#include <QObject>

class QITreeWidget;

class QTreeWidget;
class QTreeWidgetItem;
class QIcon;

class VBoxSettingsSelector: public QObject
{
    Q_OBJECT;

public:

    VBoxSettingsSelector (QWidget *aParent = NULL);

    virtual QWidget *widget() const = 0;

    virtual void addItem (const QIcon &aIcon, const QString &aText, int aId, const QString &aLink) = 0;
    virtual QString itemText (int aId) const = 0;
    virtual QString itemTextByIndex (int aIndex) const { return itemText (indexToId (aIndex)); }

    virtual int currentId () const = 0;
    virtual int idToIndex (int aId) const = 0;
    virtual int indexToId (int aIndex) const = 0;
    virtual int linkToId (const QString &aLink) const = 0;
    virtual void selectById (int aId) = 0;
    virtual void selectByLink (const QString &aLink) { selectById (linkToId (aLink)); }
    virtual void setVisibleById (int aId, bool aShow) = 0;

    virtual void clear() = 0;

    virtual void retranslateUi() = 0;

signals:

    void categoryChanged (int);

};

class VBoxSettingsTreeSelector: public VBoxSettingsSelector
{
    Q_OBJECT;

public:

    VBoxSettingsTreeSelector (QWidget *aParent = NULL);

    virtual QWidget *widget() const;

    virtual void addItem (const QIcon &aIcon, const QString &aText, int aIndex, const QString &aLink);
    virtual QString itemText (int aId) const;

    virtual int currentId() const;
    virtual int idToIndex (int aId) const;
    virtual int indexToId (int aIndex) const;
    virtual int linkToId (const QString &aLink) const;
    virtual void selectById (int aId);
    virtual void setVisibleById (int aId, bool aShow);

    virtual void clear();

    virtual void retranslateUi();

private slots:

    void settingsGroupChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem);

private:

    QString pagePath (const QString &aMatch) const;
    QTreeWidgetItem* findItem (QTreeWidget *aView, const QString &aMatch, int aColumn) const;
    QString idToString (int aId) const;

    /* Private member vars */
    QITreeWidget *mTwSelector;
};

#endif /* __VBoxSettingsSelector_h__ */
