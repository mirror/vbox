/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIWidgetValidator class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIWidgetValidator_h___
#define ___QIWidgetValidator_h___

/* Qt includes: */
#include <QMap>
#include <QPixmap>
#include <QValidator>

/* Forward declarations: */
class QPixmap;
class QString;
class UISettingsPage;


/** QObject extension,
  * providing passed QObject with validation routine. */
class QObjectValidator : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listener(s) about validity changed to @a enmState. */
    void sigValidityChange(QValidator::State enmState);

public:

    /** Constructs object validator passing @a pParent to the base-class.
      * @param  pValidator  Brings the validator passed on to the OObject
      *                     children and used to perform validation itself. */
    QObjectValidator(QValidator *pValidator, QObject *pParent = 0);

    /** Returns last validation state. */
    QValidator::State state() const { return m_enmState; }

public slots:

    /** Performs validation: */
    void sltValidate(QString strInput = QString());

private:

    /** Prepare routine. */
    void prepare();

    /** Holds the validator reference. */
    QValidator *m_pValidator;

    /** Holds the validation state. */
    QValidator::State m_enmState;
};


/** QObject extension,
  * which can group various QObjectValidator instances to operate on. */
class QObjectValidatorGroup : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listener(s) about validity changed to @a fValid. */
    void sigValidityChange(bool fValid);

public:

    /** Constructs validation group passing @a pParent to the base-class. */
    QObjectValidatorGroup(QObject *pParent)
        : QObject(pParent)
        , m_fResult(false)
    {}

    /** Adds @a pObjectValidator.
      * @note The ownership of @a pObjectValidator is transferred to the group,
      *       and it's the group's responsibility to delete it. */
    void addObjectValidator(QObjectValidator *pObjectValidator);

    /** Returns last validation result. */
    bool result() const { return m_fResult; }

private slots:

    /** Performs validation for a passed @a enmState. */
    void sltValidate(QValidator::State enmState);

private:

    /** Converts QValidator::State to bool result. */
    static bool toResult(QValidator::State enmState);

    /** Holds object-validators and their states. */
    QMap<QObjectValidator*, bool> m_group;

    /** Holds validation result. */
    bool m_fResult;
};


/** Page validator prototype. */
class UIPageValidator : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about validity change for @a pValidator. */
    void sigValidityChanged(UIPageValidator *pValidator);

    /** Asks listener to show warning icon. */
    void sigShowWarningIcon();
    /** Asks listener to hide warning icon. */
    void sigHideWarningIcon();

public:

    /** Constructs page validator for a certain @a pPage,
      * passing @a pParent to the base-class. */
    UIPageValidator(QObject *pParent, UISettingsPage *pPage)
        : QObject(pParent)
        , m_pPage(pPage)
        , m_fIsValid(true)
    {}

    /** Returns page. */
    UISettingsPage *page() const { return m_pPage; }
    /** Returns warning pixmap. */
    QPixmap warningPixmap() const;
    /** Returns internal name. */
    QString internalName() const;

    /** Returns whether validator is valid. */
    bool isValid() const { return m_fIsValid; }
    /** Defines whether validator @a fIsValid. */
    void setValid(bool fIsValid) { m_fIsValid = fIsValid; }

    /** Returns last message. */
    QString lastMessage() const { return m_strLastMessage; }
    /** Defines @a strLastMessage. */
    void setLastMessage(const QString &strLastMessage);

public slots:

    /** Performs revalidation. */
    void revalidate();

private:

    /** Holds the validated page. */
    UISettingsPage *m_pPage;

    /** Holds whether the page is valid. */
    bool m_fIsValid;

    /** Holds the last message. */
    QString m_strLastMessage;
};


/** QValidator extension,
  * for long number validations. */
class QIULongValidator : public QValidator
{
public:

    /** Constructs long validator passing @a pParent to the base-class. */
    QIULongValidator(QObject *pParent)
        : QValidator(pParent)
        , m_uBottom(0), m_uTop(ULONG_MAX)
    {}

    /** Constructs long validator passing @a pParent to the base-class.
      * @param  uMinimum  Holds the minimum valid border.
      * @param  uMaximum  Holds the maximum valid border. */
    QIULongValidator(ulong uMinimum, ulong uMaximum,
                     QObject *pParent)
        : QValidator(pParent)
        , m_uBottom(uMinimum), m_uTop(uMaximum)
    {}

    /** Destructs long validator. */
    virtual ~QIULongValidator() {}

    /** Performs validation for @a strInput at @a iPosition. */
    State validate(QString &strInput, int &iPosition) const;

    /** Defines @a uBottom. */
    void setBottom(ulong uBottom) { setRange(uBottom, m_uTop); }
    /** Defines @a uTop. */
    void setTop(ulong uTop) { setRange(m_uBottom, uTop); }
    /** Defines range based on passed @a uBottom and @a uTop. */
    void setRange(ulong uBottom, ulong uTop) { m_uBottom = uBottom; m_uTop = uTop; }
    /** Returns bottom. */
    ulong bottom() const { return m_uBottom; }
    /** Returns top. */
    ulong top() const { return m_uTop; }

private:

    /** Holds the bottom. */
    ulong m_uBottom;
    /** Holds the top. */
    ulong m_uTop;
};


#endif /* !___QIWidgetValidator_h___ */

