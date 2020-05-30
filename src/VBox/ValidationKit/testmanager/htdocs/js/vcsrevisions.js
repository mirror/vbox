/* $Id$ */
/** @file
 * Common JavaScript functions
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/** Called when we've got the revision data.   */
function vcsRevisionsRender(sTestMgr, oElmDst, sBugTracker, oRestReq, sUrl)
{
    console.log('vcsRevisionsRender: status=' + oRestReq.status + ' readyState=' + oRestReq.readyState + ' url=' + sUrl);
    if (oRestReq.readyState != oRestReq.DONE)
    {
        oElmDst.innerHTML = '<p>' + oRestReq.readyState + '</p>';
        return true;
    }


    /*
     * Check the result and translate it to a javascript object (oResp).
     */
    var oResp = null;
    var sHtml;
    if (oRestReq.status != 200)
    {
        /** @todo figure why this doesn't work (sPath to something random). */
        var sMsg = oRestReq.getResponseHeader('tm-error-message');
        console.log('vcsRevisionsRender: status=' + oRestReq.status + ' readyState=' + oRestReq.readyState + ' url=' + sUrl + ' msg=' + sMsg);
        sHtml  = '<p>error: status=' + oRestReq.status + 'readyState=' + oRestReq.readyState + ' url=' + sUrl;
        if (sMsg)
            sHtml += ' msg=' + sMsg;
        sHtml += '</p>';
    }
    else
    {
        try
        {
            oResp = JSON.parse(oRestReq.responseText);
        }
        catch (oEx)
        {
            console.log('JSON.parse threw: ' + oEx.toString());
            console.log(oRestReq.responseText);
            sHtml = '<p>error: JSON.parse threw: ' + oEx.toString() + '</p>';
        }
    }

    /*
     * Do the rendering.
     */
    if (oResp)
    {
        if (oResp.cCommits == 0)
        {
            sHtml = '<p>None.</p>';
        }
        else
        {
            sHtml = '<p>';

            var aoCommits = oResp.aoCommits;
            var cCommits  = oResp.aoCommits.length;
            var i;
            for (i = 0; i < cCommits; i++)
            {
                var oCommit = aoCommits[i];
                var sUrl    = oResp.sTracChangesetUrlFmt.replace('%(sRepository)s', oCommit.sRepository).replace('%(iRevision)s', oCommit.iRevision.toString());
                var sTitle  = oCommit.sAuthor + ': ' + oCommit.sMessage;
                sHtml += ' <a href="' + escapeElem(sUrl) + '" title="' + escapeElem(sTitle) + '">r' + oCommit.iRevision + '</a> \n';
            }

            sHtml += '</p>';

        }
    }

    oElmDst.innerHTML = sHtml;
}

/** Called by the xtracker bugdetails page. */
function VcsRevisionsLoad(sTestMgr, oElmDst, sBugTracker, lBugNo)
{
    oElmDst.innerHTML = '<p>Loading VCS revisions...</p>';

    var sUrl = sTestMgr + 'rest.py?sPath=vcs/bugreferences/' + sBugTracker + '/' + lBugNo;
    var oRestReq = new XMLHttpRequest();
    oRestReq.onreadystatechange = function() { vcsRevisionsRender(sTestMgr, oElmDst, sBugTracker, this, sUrl); }
    oRestReq.open('GET', sUrl);
    oRestReq.withCredentials = true;
    //oRestReq.setRequestHeader('Content-type', 'application/json');
    oRestReq.send();
}

