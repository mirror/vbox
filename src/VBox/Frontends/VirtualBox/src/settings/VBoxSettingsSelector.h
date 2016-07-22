/* $Id$ */
/** @file
 * VBox Qt GUI - VBoxSettingsSelector class declaration.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxSettingsSelector_h__
#define __VBoxSettingsSelector_h__

/* Qt includes */
#include <QObject>

class QITreeWidget;
class UIToolBar;
class UISettingsPage;
class SelectorItem;
class SelectorActionItem;

class QTreeWidget;
class QTreeWidgetItem;
class QIcon;
class QAction;
class QActionGroup;
template <class Key, class T> class QMap;
class QTabWidget;

class VBoxSettingsSelector: public QObject
{
    Q_OBJECT;

public:

    VBoxSettingsSelector (QWidget *aParent = NULL);
    ~VBoxSettingsSelector();

    virtual QWidget *widget() const = 0;

    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1) = 0;

    virtual void setItemText (int aId, const QString &aText);
    virtual QString itemText (int aId) const = 0;
    virtual QString itemTextByPage (UISettingsPage *aPage) const;

    virtual int currentId () const = 0;
    virtual int linkToId (const QString &aLink) const = 0;

    virtual QWidget *idToPage (int aId) const;
    virtual QWidget *rootPage (int aId) const { return idToPage (aId); }

    virtual void selectById (int aId) = 0;
    virtual void selectByLink (const QString &aLink) { selectById (linkToId (aLink)); }

    virtual void setVisibleById (int aId, bool aShow) = 0;

    virtual QList<UISettingsPage*> settingPages() const;
    virtual QList<QWidget*> rootPages() const;

    virtual void polish() {};

    virtual int minWidth () const { return 0; }

signals:

    void categoryChanged (int);

protected:

    virtual void clear() = 0;

    SelectorItem* findItem (int aId) const;
    SelectorItem* findItemByLink (const QString &aLink) const;
    SelectorItem* findItemByPage (UISettingsPage* aPage) const;

    QList<SelectorItem*> mItemList;
};

class VBoxSettingsTreeViewSelector: public VBoxSettingsSelector
{
    Q_OBJECT;

public:

    VBoxSettingsTreeViewSelector (QWidget *aParent = NULL);

    virtual QWidget *widget() const;

    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1);
    virtual void setItemText (int aId, const QString &aText);
    virtual QString itemText (int aId) const;

    virtual int currentId() const;
    virtual int linkToId (const QString &aLink) const;

    virtual void selectById (int aId);

    virtual void setVisibleById (int aId, bool aShow);

    virtual void polish();

private slots:

    void settingsGroupChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem);

private:

    virtual void clear();

    QString pagePath (const QString &aMatch) const;
    QTreeWidgetItem* findItem (QTreeWidget *aView, const QString &aMatch, int aColumn) const;
    QString idToString (int aId) const;

    /* Private member vars */
    QITreeWidget *mTwSelector;
};

class VBoxSettingsToolBarSelector: public VBoxSettingsSelector
{
    Q_OBJECT;

public:

    VBoxSettingsToolBarSelector (QWidget *aParent = NULL);
    ~VBoxSettingsToolBarSelector();

    virtual QWidget *widget() const;

    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1);
    virtual void setItemText (int aId, const QString &aText);
    virtual QString itemText (int aId) const;

    virtual int currentId() const;
    virtual int linkToId (const QString &aLink) const;

    virtual QWidget *idToPage (int aId) const;
    virtual QWidget *rootPage (int aId) const;

    virtual void selectById (int aId);

    virtual void setVisibleById (int aId, bool aShow);

    virtual int minWidth() const;

    virtual QList<QWidget*> rootPages() const;
private slots:

    void settingsGroupChanged (QAction *aAction);
    void settingsGroupChanged (int aIndex);

private:

    virtual void clear();

    SelectorActionItem *findActionItem (int aId) const;
    SelectorActionItem *findActionItemByAction (QAction *aAction) const;
    SelectorActionItem *findActionItemByTabWidget (QTabWidget* aTabWidget, int aIndex) const;

    /* Private member vars */
    UIToolBar *mTbSelector;
    QActionGroup *mActionGroup;
};

#endif /* __VBoxSettingsSelector_h__ */

