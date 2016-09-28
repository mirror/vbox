/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsSelector class declaration.
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

#ifndef ___UISettingsSelector_h___
#define ___UISettingsSelector_h___

/* Qt includes: */
#include <QObject>

/* Forward declarations: */
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


/** QObject subclass providing settings dialog
  * with the means to switch between settings pages. */
class UISettingsSelector: public QObject
{
    Q_OBJECT;

public:

    /** Constructs settings selector passing @a aParent to the base-class. */
    UISettingsSelector (QWidget *aParent = NULL);
    /** Destructs settings selector. */
    ~UISettingsSelector();

    /** Returns the widget selector operates on. */
    virtual QWidget *widget() const = 0;

    /** Adds a new selector item.
      * @param  strBigIcon     Brings the big icon reference.
      * @param  strMediumIcon  Brings the medium icon reference.
      * @param  strSmallIcon   Brings the small icon reference.
      * @param  aId            Brings the selector section ID.
      * @param  aLink          Brings the selector section link.
      * @param  aPage          Brings the selector section page reference.
      * @param  aParentId      Brings the parent section ID or -1 if there is no parent. */
    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1) = 0;

    /** Defines the @a aText for section with @a aId. */
    virtual void setItemText (int aId, const QString &aText);
    /** Returns the text for section with @a aId. */
    virtual QString itemText (int aId) const = 0;
    /** Returns the text for section with @a aPage. */
    virtual QString itemTextByPage (UISettingsPage *aPage) const;

    /** Returns the current selector ID. */
    virtual int currentId () const = 0;

    /** Returns the section ID for passed @a aLink. */
    virtual int linkToId (const QString &aLink) const = 0;

    /** Returns the section page for passed @a aId. */
    virtual QWidget *idToPage (int aId) const;
    /** Returns the section root-page for passed @a aId. */
    virtual QWidget *rootPage (int aId) const { return idToPage (aId); }

    /** Make the section with @a aId current. */
    virtual void selectById (int aId) = 0;
    /** Make the section with @a aLink current. */
    virtual void selectByLink (const QString &aLink) { selectById (linkToId (aLink)); }

    /** Make the section with @a aId @a aShow. */
    virtual void setVisibleById (int aId, bool aShow) = 0;

    /** Returns the list of all selector pages. */
    virtual QList<UISettingsPage*> settingPages() const;
    /** Returns the list of all root pages. */
    virtual QList<QWidget*> rootPages() const;

    /** Performs selector polishing. */
    virtual void polish() {};

    /** Returns minimum selector width. */
    virtual int minWidth () const { return 0; }

signals:

    /** Notifies listeners about selector @a iCategory changed. */
    void categoryChanged (int iCategory);

protected:

    /** Clears selector of all the items. */
    virtual void clear() = 0;

    /** Searches for item with passed @a aId. */
    SelectorItem* findItem (int aId) const;
    /** Searches for item with passed @a aLink. */
    SelectorItem* findItemByLink (const QString &aLink) const;
    /** Searches for item with passed @a aPage. */
    SelectorItem* findItemByPage (UISettingsPage* aPage) const;

    /** Holds the selector item instances. */
    QList<SelectorItem*> mItemList;
};


/** UISettingsSelector subclass providing settings dialog
  * with the means to switch between settings pages.
  * This one represented as tree-widget. */
class UISettingsSelectorTreeView: public UISettingsSelector
{
    Q_OBJECT;

public:

    /** Constructs settings selector passing @a aParent to the base-class. */
    UISettingsSelectorTreeView (QWidget *aParent = NULL);

    /** Returns the widget selector operates on. */
    virtual QWidget *widget() const;

    /** Adds a new selector item.
      * @param  strBigIcon     Brings the big icon reference.
      * @param  strMediumIcon  Brings the medium icon reference.
      * @param  strSmallIcon   Brings the small icon reference.
      * @param  aId            Brings the selector section ID.
      * @param  aLink          Brings the selector section link.
      * @param  aPage          Brings the selector section page reference.
      * @param  aParentId      Brings the parent section ID or -1 if there is no parent. */
    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1);

    /** Defines the @a aText for section with @a aId. */
    virtual void setItemText (int aId, const QString &aText);
    /** Returns the text for section with @a aId. */
    virtual QString itemText (int aId) const;

    /** Returns the current selector ID. */
    virtual int currentId() const;

    /** Returns the section ID for passed @a aLink. */
    virtual int linkToId (const QString &aLink) const;

    /** Make the section with @a aId current. */
    virtual void selectById (int aId);

    /** Make the section with @a aId @a aShow. */
    virtual void setVisibleById (int aId, bool aShow);

    /** Performs selector polishing. */
    virtual void polish();

private slots:

    /** Handles selector section change from @a aPrevItem to @a aItem. */
    void settingsGroupChanged (QTreeWidgetItem *aItem, QTreeWidgetItem *aPrevItem);

private:

    /** Clears selector of all the items. */
    virtual void clear();

    /** Returns page path for passed @a aMatch. */
    QString pagePath (const QString &aMatch) const;
    /** Find item within the passed @a aView and @a aColumn matching @a aMatch. */
    QTreeWidgetItem* findItem (QTreeWidget *aView, const QString &aMatch, int aColumn) const;
    /** Performs @a aId to QString serialization. */
    QString idToString (int aId) const;

    /** Holds the tree-widget instance. */
    QITreeWidget *mTwSelector;
};


/** UISettingsSelector subclass providing settings dialog
  * with the means to switch between settings pages.
  * This one represented as tab-widget. */
class UISettingsSelectorToolBar: public UISettingsSelector
{
    Q_OBJECT;

public:

    /** Constructs settings selector passing @a aParent to the base-class. */
    UISettingsSelectorToolBar (QWidget *aParent = NULL);
    ~UISettingsSelectorToolBar();

    /** Returns the widget selector operates on. */
    virtual QWidget *widget() const;

    /** Adds a new selector item.
      * @param  strBigIcon     Brings the big icon reference.
      * @param  strMediumIcon  Brings the medium icon reference.
      * @param  strSmallIcon   Brings the small icon reference.
      * @param  aId            Brings the selector section ID.
      * @param  aLink          Brings the selector section link.
      * @param  aPage          Brings the selector section page reference.
      * @param  aParentId      Brings the parent section ID or -1 if there is no parent. */
    virtual QWidget *addItem (const QString &strBigIcon, const QString &strMediumIcon, const QString &strSmallIcon,
                              int aId, const QString &aLink, UISettingsPage* aPage = NULL, int aParentId = -1);

    /** Defines the @a aText for section with @a aId. */
    virtual void setItemText (int aId, const QString &aText);
    /** Returns the text for section with @a aId. */
    virtual QString itemText (int aId) const;

    /** Returns the current selector ID. */
    virtual int currentId() const;

    /** Returns the section ID for passed @a aLink. */
    virtual int linkToId (const QString &aLink) const;

    /** Returns the section page for passed @a aId. */
    virtual QWidget *idToPage (int aId) const;
    /** Returns the section root-page for passed @a aId. */
    virtual QWidget *rootPage (int aId) const;

    /** Make the section with @a aId current. */
    virtual void selectById (int aId);

    /** Make the section with @a aId @a aShow. */
    virtual void setVisibleById (int aId, bool aShow);

    /** Returns minimum selector width. */
    virtual int minWidth() const;

    /** Returns the list of all root pages. */
    virtual QList<QWidget*> rootPages() const;

private slots:

    /** Handles selector section change to @a aAction . */
    void settingsGroupChanged (QAction *aAction);
    /** Handles selector section change to @a aIndex . */
    void settingsGroupChanged (int aIndex);

private:

    /** Clears selector of all the items. */
    virtual void clear();

    /** Searches for action item with passed @a aId. */
    SelectorActionItem *findActionItem (int aId) const;
    /** Searches for action item with passed @a aAction. */
    SelectorActionItem *findActionItemByAction (QAction *aAction) const;
    /** Searches for action item with passed @a aTabWidget and @a aIndex. */
    SelectorActionItem *findActionItemByTabWidget (QTabWidget* aTabWidget, int aIndex) const;

    /** Holds the toolbar instance. */
    UIToolBar *mTbSelector;
    /** Holds the action group instance. */
    QActionGroup *mActionGroup;
};

#endif /* !___UISettingsSelector_h___ */

