/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumDetailsWidget class declaration.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMediumDetailsWidget_h___
#define ___UIMediumDetailsWidget_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMediumDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QFrame;
class QLabel;
class QStackedLayout;
class QWidget;
class QILabel;


/** Virtual Media Manager: Medium details data structure. */
struct UIDataMediumDetails
{
    /** Constructs data. */
    UIDataMediumDetails()
        : m_aLabels(QStringList())
        , m_aFields(QStringList())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataMediumDetails &other) const
    {
        return true
               && (m_aLabels == other.m_aLabels)
               && (m_aFields == other.m_aFields)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataMediumDetails &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataMediumDetails &other) const { return !equal(other); }

    /** Holds the labels list. */
    QStringList m_aLabels;
    /** Holds the fields list. */
    QStringList m_aFields;
};


/** Virtual Media Manager: Medium data structure. */
struct UIDataMedium
{
    /** Constructs data. */
    UIDataMedium()
        : m_enmType(UIMediumType_Invalid)
        , m_details(UIDataMediumDetails())
    {}

    /** Constructs data with passed @enmType. */
    UIDataMedium(UIMediumType enmType)
        : m_enmType(enmType)
        , m_details(UIDataMediumDetails())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataMedium &other) const
    {
        return true
               && (m_details == other.m_details)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataMedium &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataMedium &other) const { return !equal(other); }

    /** Holds the medium type. */
    UIMediumType m_enmType;

    /** Holds the details data. */
    UIDataMediumDetails m_details;
};


/** Virtual Media Manager: Virtual Media Manager details-widget. */
class UIMediumDetailsWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs medium details dialog passing @a pParent to the base-class.
      * @param  enmEmbedding  Brings embedding type. */
    UIMediumDetailsWidget(EmbedTo enmEmbedding, QWidget *pParent = 0);

    /** Defines the raised details @a enmType. */
    void setCurrentType(UIMediumType enmType);

    /** Returns the medium data. */
    const UIDataMedium &data() const { return m_newData; }
    /** Defines the @a data for passed @a enmType. */
    void setData(const UIDataMedium &data);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares this. */
        void prepareThis();
        /** Prepares frame. */
        void prepareFrame();
        /** Prepares 'Details' tab. */
        void prepareTabDetails();
        /** Prepares information-container. */
        void prepareInformationContainer(UIMediumType enmType, int cFields);
    /** @} */

    /** @name Loading stuff.
      * @{ */
        /** Loads data. */
        void loadData();
        /** Load details. */
        void loadDetails();
    /** @} */

    /** @name Details stuff.
      * @{ */
        /** Returns information-container for passed medium @a enmType. */
        QWidget *infoContainer(UIMediumType enmType) const;
        /** Returns information-label for passed medium @a enmType and @a iIndex. */
        QLabel *infoLabel(UIMediumType enmType, int iIndex) const;
        /** Returns information-field for passed medium @a enmType and @a iIndex. */
        QILabel *infoField(UIMediumType enmType, int iIndex) const;
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the parent widget embedding type. */
        const EmbedTo m_enmEmbedding;

        /** Holds the old data copy. */
        UIDataMedium  m_oldData;
        /** Holds the new data copy. */
        UIDataMedium  m_newData;
        /** Holds the frame. */
        QFrame       *m_pFrame;
    /** @} */

    /** @name Details variables.
      * @{ */
        /** Holds the details layout: */
        QStackedLayout *m_pLayoutDetails;

        /** Holds the map of information-container instances. */
        QMap<UIMediumType, QWidget*>          m_aContainers;
        /** Holds the map of information-container label instances. */
        QMap<UIMediumType, QList<QLabel*> >   m_aLabels;
        /** Holds the information-container field instances. */
        QMap<UIMediumType, QList<QILabel*> >  m_aFields;
    /** @} */
};

#endif /* !___UIMediumDetailsWidget_h___ */

