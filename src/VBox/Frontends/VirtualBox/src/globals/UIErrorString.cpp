/* $Id$ */
/** @file
 * VBox Qt GUI - UIErrorString class implementation.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <QObject>

/* GUI includes: */
#include "UICommon.h"
#include "UIErrorString.h"

/* COM includes: */
#include "COMDefs.h"
#include "CProgress.h"
#include "CVirtualBoxErrorInfo.h"


/* static */
QString UIErrorString::formatRC(HRESULT rc)
{
    /** @todo r=bird: Not sure why we set the sign bit 31 bit for warnings.
     *  Maybe to try get the error variant?  It won't really work for S_FALSE and
     *  probably a bunch of others too.  I've modified it on windows to try get
     *  the exact one, the one with the top bit set, or just the value. */
#if 0//def RT_OS_WINDOWS
    char szDefine[80];
    if (!SUCCEEDED_WARNING(rc))
        RTErrWinQueryDefine(rc, szDefine, sizeof(szDefine), false /*fFailIfUnknown*/);
    else
    {
        if (   RTErrWinQueryDefine(rc, szDefine, sizeof(szDefine), true /*fFailIfUnknown*/) < 0
            || RTErrWinQueryDefine(rc | 0x80000000, szDefine, sizeof(szDefine), true /*fFailIfUnknown*/) < 0)
            RTErrWinQueryDefine(rc, szDefine, sizeof(szDefine), false /*fFailIfUnknown*/);
    }

    QString str;
    str.sprintf("%s", szDefine);
    return str;
#else
    const char *pszDefine = RTErrCOMGet(SUCCEEDED_WARNING(rc) ? rc | 0x80000000 : rc)->pszDefine;
    Assert(pszDefine);

    QString str;
    str.sprintf("%s", pszDefine);
    return str;
#endif
}

/* static */
QString UIErrorString::formatRCFull(HRESULT rc)
{
    /** @todo r=bird: See UIErrorString::formatRC for 31th bit discussion. */
#if 0//def RT_OS_WINDOWS
    char szDefine[80];
    ssize_t cchRet = RTErrWinQueryDefine(rc, szDefine, sizeof(szDefine), true /*fFailIfUnknown*/);
    if (RT_FAILURE(cchRet) && SUCCEEDED_WARNING(rc)))
        cchRet = RTErrWinQueryDefine(rc | 0x80000000, szDefine, sizeof(szDefine), true /*fFailIfUnknown*/);

    QString str;
    if (RT_SUCCESS(cchRet))
        str.sprintf("%s (0x%08x)", szDefine, rc);
    else
        str.sprintf("0x%08x", rc);
    return str;
#else
    const char *pszDefine = RTErrCOMGet(SUCCEEDED_WARNING(rc) ? rc | 0x80000000 : rc)->pszDefine;
    Assert(pszDefine);

    QString str;
    if (strncmp(pszDefine, RT_STR_TUPLE("Unknown ")))
        str.sprintf("%s (0x%08X)", pszDefine, rc);
    else
        str.sprintf("0x%08X", rc);
    return str;
#endif
}

/* static */
QString UIErrorString::formatErrorInfo(const CProgress &comProgress)
{
    /* Check for API errors first: */
    if (!comProgress.isOk())
        return formatErrorInfo(static_cast<COMBaseWithEI>(comProgress));

    /* For progress errors otherwise: */
    CVirtualBoxErrorInfo comErrorInfo = comProgress.GetErrorInfo();
    /* Handle valid error-info first: */
    if (!comErrorInfo.isNull())
        return formatErrorInfo(comErrorInfo);
    /* Handle NULL error-info otherwise: */
    return QString("<table bgcolor=#EEEEEE border=0 cellspacing=5 cellpadding=0 width=100%>"
                   "<tr><td>%1</td><td><tt>%2</tt></td></tr></table>")
                   .arg(QApplication::translate("UIErrorString", "Result&nbsp;Code: ", "error info"))
                   .arg(formatRCFull(comProgress.GetResultCode()))
                   .prepend("<!--EOM-->") /* move to details */;
}

/* static */
QString UIErrorString::formatErrorInfo(const COMErrorInfo &comInfo, HRESULT wrapperRC /* = S_OK */)
{
    return QString("<qt>%1</qt>").arg(UIErrorString::errorInfoToString(comInfo, wrapperRC));
}

/* static */
QString UIErrorString::formatErrorInfo(const CVirtualBoxErrorInfo &comInfo)
{
    return formatErrorInfo(COMErrorInfo(comInfo));
}

/* static */
QString UIErrorString::formatErrorInfo(const COMBaseWithEI &comWrapper)
{
    Assert(comWrapper.lastRC() != S_OK);
    return formatErrorInfo(comWrapper.errorInfo(), comWrapper.lastRC());
}

/* static */
QString UIErrorString::formatErrorInfo(const COMResult &comRc)
{
    Assert(comRc.rc() != S_OK);
    return formatErrorInfo(comRc.errorInfo(), comRc.rc());
}

/* static */
QString UIErrorString::errorInfoToString(const COMErrorInfo &comInfo, HRESULT wrapperRC)
{
    /* Compose complex details string with internal <!--EOM--> delimiter to
     * make it possible to split string into info & details parts which will
     * be used separately in QIMessageBox. */
    QString strFormatted;

    /* Check if details text is NOT empty: */
    const QString strDetailsInfo = comInfo.text();
    if (!strDetailsInfo.isEmpty())
    {
        /* Check if details text written in English (latin1) and translated: */
        if (   strDetailsInfo == QString::fromLatin1(strDetailsInfo.toLatin1())
            && strDetailsInfo != QObject::tr(strDetailsInfo.toLatin1().constData()))
            strFormatted += QString("<p>%1.</p>").arg(uiCommon().emphasize(QObject::tr(strDetailsInfo.toLatin1().constData())));
        else
            strFormatted += QString("<p>%1.</p>").arg(uiCommon().emphasize(strDetailsInfo));
    }

    strFormatted += "<!--EOM--><table bgcolor=#EEEEEE border=0 cellspacing=5 "
                    "cellpadding=0 width=100%>";

    bool fHaveResultCode = false;

    if (comInfo.isBasicAvailable())
    {
#ifdef VBOX_WS_WIN
        fHaveResultCode = comInfo.isFullAvailable();
        bool fHaveComponent = true;
        bool fHaveInterfaceID = true;
#else /* !VBOX_WS_WIN */
        fHaveResultCode = true;
        bool fHaveComponent = comInfo.isFullAvailable();
        bool fHaveInterfaceID = comInfo.isFullAvailable();
#endif

        if (fHaveResultCode)
        {
            strFormatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
                .arg(QApplication::translate("UIErrorString", "Result&nbsp;Code: ", "error info"))
                .arg(formatRCFull(comInfo.resultCode()));
        }

        if (fHaveComponent)
            strFormatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(QApplication::translate("UIErrorString", "Component: ", "error info"), comInfo.component());

        if (fHaveInterfaceID)
        {
            QString s = comInfo.interfaceID().toString();
            if (!comInfo.interfaceName().isEmpty())
                s = comInfo.interfaceName() + ' ' + s;
            strFormatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(QApplication::translate("UIErrorString", "Interface: ", "error info"), s);
        }

        if (!comInfo.calleeIID().isNull() && comInfo.calleeIID() != comInfo.interfaceID())
        {
            QString s = comInfo.calleeIID().toString();
            if (!comInfo.calleeName().isEmpty())
                s = comInfo.calleeName() + ' ' + s;
            strFormatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(QApplication::translate("UIErrorString", "Callee: ", "error info"), s);
        }
    }

    if (   FAILED(wrapperRC)
        && (!fHaveResultCode || wrapperRC != comInfo.resultCode()))
    {
        strFormatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
            .arg(QApplication::translate("UIErrorString", "Callee&nbsp;RC: ", "error info"))
            .arg(formatRCFull(wrapperRC));
    }

    strFormatted += "</table>";

    if (comInfo.next())
        strFormatted = strFormatted + "<!--EOP-->" + errorInfoToString(*comInfo.next());

    return strFormatted;
}

