 /*
   HTMLWindow.cpp

   Copyright (C) 2003-2004 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

#include "HTMLWindow.h"

#include <ExDispId.h>
#include <stdio.h>
#include <string>
#include <map>
#include <tchar.h>

#include "OleClientSite.h"
#include "Storage.h"
#include "OleInPlaceSite.h"
#include "OleInPlaceFrame.h"
#include "DocHostUiHandler.h"
#include "Tracer.h"

#include "VariantHelper.h"
#include "UrlHelper.h"

#include "resource.h"

HWND todo_hwnd;
bool todo_bool=false;

std::string IIDAsString (REFIID riid)
{
    char buf[_MAX_PATH] = { 0};

    for (int i=0;i<16; i++)
    {
        sprintf(buf + i*2, "%02x", *(((char*)&riid) + i));
    }

    return buf;
};

bool HTMLWindow::ole_is_initialized_ = false;

const IID IID_IDocHostUIHandler ={0xbd3f23c0,0xd43e,0x11CF,{0x89, 0x3b, 0x00, 0xaa, 0x00, 0xbd, 0xce, 0x1a}};

// TODO raus mit dem
static const SAFEARRAYBOUND ArrayBound = {1, 0};

HTMLWindow::HTMLWindow(EventReceiver* r, std::string const& title, HINSTANCE instance, std::string const& first_link, std::map<std::string, std::string> const& first_params)
: instance_(instance),
browserObject_(0),
event_receiver_(r)
{
    TraceFunc("HTMLWindow::HTMLWindow");

    InitOle_CreateWindow(title);

    std::string first_html_page; /* start page */
    Trace("Going to call event_receiver_->AppLink");

    if (event_receiver_) event_receiver_->AppLink(first_link, first_html_page, first_params);

    Trace("called event_receiver_->AppLink");

    DWORD todo_del_me_1;
    HANDLE todo_del_me = ::CreateFile("some.html", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    WriteFile(todo_del_me, first_html_page.c_str(), first_html_page.size(), &todo_del_me_1, 0);
    ::CloseHandle(todo_del_me);

    HTML(first_html_page);
}


HTMLWindow::HTMLWindow(std::string const& html_or_url,
                       std::string const& title,
                       HINSTANCE instance,
                       bool      is_url) :
instance_(instance),
browserObject_(0),
event_receiver_(0)
{

    InitOle_CreateWindow(title);

    if (is_url) URL (html_or_url);
    else        HTML(html_or_url);
}


void HTMLWindow::InitOle_CreateWindow(std::string const& title)
{
    DWORD dwErr = 0;

    if (!ole_is_initialized_)
    {
        /* An application, when initializing the WebBrowser Control, should use
           OleInitialize rather than CoInitialize to start COM. OleInitialize
           enables support for the clipboard, drag-and-drop, OLE, and in-place
           activation. Use OleUninitialize to close the COM library when your
           application shuts down.  */

        if (::OleInitialize(0) != S_OK)
        {
            return;
        }
        ole_is_initialized_ = true;
    }

    HICON hIconApp = (HICON)LoadImage(instance_, MAKEINTRESOURCE(IDI_VIRTUALBOX), IMAGE_ICON, 16, 16, LR_SHARED | LR_DEFAULTCOLOR);
    if (NULL == hIconApp) {
        dwErr = GetLastError();
        char s[1024];
        sprintf(s, "Could not load application icon! Error: %ld", dwErr);
        MessageBox (GetDesktopWindow(), s, "ERROR", MB_ICONERROR);
    }

    WNDCLASSEX wc;

    ::ZeroMemory (&wc, sizeof(WNDCLASSEX));
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.hInstance     = instance_;
    wc.lpfnWndProc   = WindowProc;
    wc.lpszClassName = "AutoStartMenu";
    wc.hIcon         = hIconApp;
    wc.hIconSm       = hIconApp;

    RegisterClassEx(&wc);

    int iSizeX = 700;
    int iSizeY = 525;

    hwnd_ = ::CreateWindowEx(0, "AutoStartMenu", title.c_str(), WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, 0,
                             iSizeX, iSizeY,
                             HWND_DESKTOP , 0,
                             instance_    , 0);

    todo_hwnd=hwnd_;
    if (hwnd_)
    {
        SetWindowLong(hwnd_, GWL_USERDATA, (LONG)this);
        SetWindowPos(hwnd_,HWND_TOPMOST, (GetSystemMetrics(SM_CXSCREEN) - iSizeX) / 2, (GetSystemMetrics(SM_CYSCREEN) - iSizeY) / 2, iSizeX, iSizeY, SWP_SHOWWINDOW | SWP_NOZORDER);
        if (EmbedBrowserObject()) return;
        AddSink();

        ShowWindow(hwnd_, SW_SHOWNORMAL);
        UpdateWindow(hwnd_);
    }
}

HTMLWindow::~HTMLWindow()
{
    if (ole_is_initialized_)
    {
        ::OleUninitialize();
        ole_is_initialized_ = false;
    }
}

HRESULT HTMLWindow::QueryInterface(REFIID riid, void ** ppvObject)
{
    TraceFunc("HTMLWindow::QueryInterface");
    std::string s = IIDAsString(riid);
    if (!memcmp((const void*) &riid, (const void*)&IID_IUnknown,          sizeof(GUID)) ||
        !memcmp((const void*) &riid, (const void*)&IID_IDispatch,         sizeof(GUID)) ||
        !memcmp((const void*) &riid, (const void*)&IID_IDocHostUIHandler, sizeof(GUID)))
    {
        TraceFunc("HTMLWindow::QueryInterface");
        *ppvObject = doc_host_ui_handler_;
        return S_OK;
    }

    ppvObject = 0;
    return E_NOINTERFACE;
}

void HTMLWindow::BeforeNavigate2(
                                IDispatch*    pDisp,
                                VARIANT*      url,
                                VARIANT*      Flags,
                                VARIANT*      TargetFrameName,
                                VARIANT*      PostData,
                                VARIANT*      Headers,
                                VARIANT_BOOL* Cancel)
{
    if (NULL != wcsstr(url->bstrVal, L".exe") ||
        NULL != wcsstr(url->bstrVal, L".com"))
    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        DWORD dwRes = 0;

        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );

        dwRes = CreateProcessW (url->bstrVal, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        if (!dwRes && GetLastError() == 740)   // 740 = ERROR_ELEVATION_REQUIRED -> Only for Windows Vista
        {
            SHELLEXECUTEINFOW TempInfo = { 0 };

            WCHAR szCurDir [_MAX_PATH+1] = { 0} ;
            GetCurrentDirectoryW( _MAX_PATH, szCurDir);

            TempInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
            TempInfo.fMask = 0;
            TempInfo.hwnd = NULL;
            TempInfo.lpVerb = L"runas";
            TempInfo.lpFile = url->bstrVal;
            TempInfo.lpParameters = L"";
            TempInfo.lpDirectory = szCurDir;
            TempInfo.nShow = SW_NORMAL;

            ::ShellExecuteExW (&TempInfo);
        }

        *Cancel = TRUE;
    }
    else if ((NULL != wcsstr(url->bstrVal, L".msi")) ||
             (NULL != wcsstr(url->bstrVal, L".pdf")))
    {
        ShellExecuteW (GetDesktopWindow(), L"open", url->bstrVal, NULL, NULL, SW_SHOW);
        *Cancel = TRUE;
    }
    else if (NULL != wcsstr(url->bstrVal, L"app:exit"))
    {
        PostQuitMessage(0);
        *Cancel = TRUE;
    }
    else
    {
        *Cancel = FALSE;
    }
}

/*
  This method displays the HTML that is passed as a string.
*/
long HTMLWindow::HTML(std::string const& html_txt)
{
    IWebBrowser2    *webBrowser2;
    LPDISPATCH       lpDispatch;
    IHTMLDocument2  *htmlDoc2;
    SAFEARRAY       *sfArray;
    VARIANT         *pVar;
    BSTR             bstr;

    static bool todo_about_blank_written_once = false;

    // Assume an error.
    bstr = 0;

    TraceFunc("HTMLWindow::HTML");

    Trace("going to DisplayHTMLPage with about:blank");

    if (!todo_about_blank_written_once)
    {
        URL("about:blank");
        todo_about_blank_written_once = true;
    }

    if (!browserObject_->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2))
    {
        if (!webBrowser2->get_Document( &lpDispatch))
        {
            if (!lpDispatch->QueryInterface(IID_IHTMLDocument2, (void**)&htmlDoc2))
            {

                /* The HTML passed to IHTMLDocument2::write must be a BSTR within an array of VARIANTs....  */
                if ((sfArray = SafeArrayCreate(VT_VARIANT, 1, (SAFEARRAYBOUND *)&ArrayBound)))
                {
                    if (!SafeArrayAccessData(sfArray, (void**)&pVar))
                    {
                        pVar->vt = VT_BSTR;
#ifndef UNICODE
                        {
                            wchar_t  *buffer;
                            DWORD    size;

                            size = MultiByteToWideChar(CP_ACP, 0, html_txt.c_str(), -1, 0, 0);
                            if (!(buffer = (wchar_t *)GlobalAlloc(GMEM_FIXED, sizeof(wchar_t) * size))) goto todo_bad;
                            MultiByteToWideChar(CP_ACP, 0, html_txt.c_str(), -1, buffer, size);
                            bstr = SysAllocString(buffer);
                            GlobalFree(buffer);
                        }
#else
                        bstr = SysAllocString(html_txt);
#endif
                        if ((pVar->bstrVal = bstr))
                        {
                            Trace("Going to actually write sfArray into html document");
                            if (htmlDoc2->write(sfArray) != S_OK)
                            {
                                throw ("Could not write sfArray");
                            }

                            /*
                               If the document were not closed, subsequent calls to DisplayHTMLStr would append to the contents of the page.
                            */
                            htmlDoc2->close();
                            Trace("Closing document");
                        }
                    }

                    /*
                      Freeing the array along with the VARIANT that SafeArrayAccessData created and the BSTR that was allocated with SysAllocString
                    */
                    SafeArrayDestroy(sfArray);
                }

                todo_bad:      htmlDoc2->Release();
            }

            lpDispatch->Release();
        }

        webBrowser2->Release();
    }


    // bstr <> 0 indicates no error
    if (bstr) return 0;

    return(-1);
}

long HTMLWindow::EmbedBrowserObject()
{
    IWebBrowser2    *webBrowser2;
    RECT             rect;

    // RENE
#define OLERENDER_DRAW 1


    storage_ = NULL;
    storage_ = new Storage;

    ole_in_place_frame_ = NULL;
    ole_in_place_frame_ = new OleInPlaceFrame;

    ole_in_place_site_ = NULL;
    ole_in_place_site_ = new OleInPlaceSite(ole_in_place_frame_, hwnd_);;

    doc_host_ui_handler_ = NULL;
    doc_host_ui_handler_ = new DocHostUiHandler(this);

    ole_client_site_ = NULL;
    ole_client_site_ = new OleClientSite(
                                        ole_in_place_site_,
                                        doc_host_ui_handler_,
                                        static_cast<DWebBrowserEvents2*>(this));

    if ((storage_ == NULL) ||
        (ole_in_place_frame_ == NULL) ||
        (ole_in_place_site_ == NULL) ||
        (doc_host_ui_handler_ == NULL) ||
        (ole_client_site_ == NULL))
    {
        ::MessageBox(0, "Could not create objects!", 0, 0);
    }

    // todo
    /*dynamic_cast<DocHostUiHandler*>(doc_host_ui_handler_)->ClientSite(ole_client_site_);
    dynamic_cast<OleInPlaceSite*  >(ole_in_place_site_  )->ClientSite(ole_client_site_);*/
    doc_host_ui_handler_->ClientSite(ole_client_site_);
    ole_in_place_site_->ClientSite(ole_client_site_);

    if (! ::OleCreate(
                     CLSID_WebBrowser,
                     IID_IOleObject,
                     OLERENDER_DRAW,
                     0,
                     ole_client_site_,
                     storage_,
                     (void**)&browserObject_))
    {

        //todo
        dynamic_cast<OleInPlaceSite*>(ole_in_place_site_)->BrowserObject(browserObject_);

        //browser_object_todo = (void*) browserObject;

        browserObject_->SetHostNames(L"Some_host_name", 0);

        GetClientRect(hwnd_, &rect);

        // Let browser object know that it is embedded in an OLE container.
        if (!OleSetContainedObject(static_cast<IUnknown*>(browserObject_), TRUE) &&
            // Set the display area of our browser control the same as our window's size and actually put the browser object into our window.
            !browserObject_->DoVerb(OLEIVERB_SHOW, NULL,
                                    // (IOleClientSite *)_iOleClientSiteEx,
                                    ole_client_site_,
                                    -1, hwnd_, &rect) &&

            !browserObject_->QueryInterface(IID_IWebBrowser2, reinterpret_cast<void**> (&webBrowser2) ))
        {

            webBrowser2->put_Left  (0);
            webBrowser2->put_Top   (0);
            webBrowser2->put_Width (rect.right);
            webBrowser2->put_Height(rect.bottom);

            webBrowser2->Release();

            return 0;
        }

        ::MessageBox(0, "Something went wrong -3", 0, 0);
        /* Something went wrong!
         TODO  UnEmbedBrowserObject(hwnd_); */
        return(-3);
    }
    ::MessageBox(0, "Something went wrong -2", 0, 0);

    return -2 ;
}

void HTMLWindow::ResizeBrowser(HWND hwnd, DWORD width, DWORD height)
{
    IWebBrowser2  *webBrowser2;

    if (!browserObject_->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2))
    {

        webBrowser2->put_Width(width);
        webBrowser2->put_Height(height);

        webBrowser2->Release();
    }
    else
    {
        ::MessageBox(0, "Couldn't query interface for IID_IWebBrowser2", 0, 0);
    }
}

void HTMLWindow::FileDownload(VARIANT_BOOL* Cancel)
{
    *Cancel = TRUE;
}

HRESULT HTMLWindow::Invoke(
                          DISPID             dispIdMember,
                          REFIID             riid,
                          LCID               lcid,
                          WORD               wFlags,
                          DISPPARAMS FAR*    pDispParams,
                          VARIANT FAR*       pVarResult,
                          EXCEPINFO FAR*     pExcepInfo,
                          unsigned int FAR*  puArgErr)
{

    switch (dispIdMember)
    {

    case DISPID_BEFORENAVIGATE2:
        // call BeforeNavigate
        // (parameters are on stack, thus in reverse order)
        BeforeNavigate2( pDispParams->rgvarg[6].pdispVal,    // pDisp
                         pDispParams->rgvarg[5].pvarVal,     // url
                         pDispParams->rgvarg[4].pvarVal,     // Flags
                         pDispParams->rgvarg[3].pvarVal,     // TargetFrameName
                         pDispParams->rgvarg[2].pvarVal,     // PostData
                         pDispParams->rgvarg[1].pvarVal,     // Headers
                         pDispParams->rgvarg[0].pboolVal);   // Cancel
        break;

    case DISPID_CLIENTTOHOSTWINDOW:
        ClientToHostWindow(pDispParams->rgvarg[1].plVal, pDispParams->rgvarg[0].plVal);
        break;
    case DISPID_COMMANDSTATECHANGE:
        CommandStateChange(pDispParams->rgvarg[1].lVal, pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_DOCUMENTCOMPLETE:
        DocumentComplete(pDispParams->rgvarg[1].pdispVal, pDispParams->rgvarg[0].pvarVal);
        break;
    case DISPID_DOWNLOADBEGIN:
        DownloadBegin();
        break;
    case DISPID_DOWNLOADCOMPLETE:
        DownloadComplete();
        break;
    case DISPID_FILEDOWNLOAD:
        FileDownload(pDispParams->rgvarg[0].pboolVal);
        break;
    case DISPID_NAVIGATECOMPLETE2:
//      NavigateComplete2(pDispParams->rgvarg[1].pdispVal, pDispParams->rgvarg[0].pvarVal);
        break;
    case DISPID_NAVIGATEERROR:
        NavigateError(pDispParams->rgvarg[4].pdispVal, pDispParams->rgvarg[3].pvarVal, pDispParams->rgvarg[2].pvarVal, pDispParams->rgvarg[1].pvarVal, pDispParams->rgvarg[0].pboolVal);
        break;
    case DISPID_NEWWINDOW2:
        NewWindow2(pDispParams->rgvarg[1].ppdispVal, pDispParams->rgvarg[0].pboolVal);
        break;
    case DISPID_ONFULLSCREEN:
        OnFullScreen(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_ONMENUBAR:
        OnMenuBar(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_ONQUIT:
        OnQuit();
        break;
    case DISPID_ONSTATUSBAR:
        OnStatusBar(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_ONTHEATERMODE:
        OnTheaterMode(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_ONTOOLBAR:
        OnToolBar(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_ONVISIBLE:
        OnVisible(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_PRINTTEMPLATEINSTANTIATION:
        PrintTemplateInstantiation(pDispParams->rgvarg[0].pdispVal);
        break;
    case DISPID_PRINTTEMPLATETEARDOWN:
        PrintTemplateTeardown(pDispParams->rgvarg[0].pdispVal);
        break;
    case DISPID_PRIVACYIMPACTEDSTATECHANGE:
        PrivacyImpactedStateChange(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_PROGRESSCHANGE:
        ProgressChange(pDispParams->rgvarg[1].lVal, pDispParams->rgvarg[0].lVal);
        break;
    case DISPID_PROPERTYCHANGE:
        PropertyChange(pDispParams->rgvarg[0].bstrVal);
        break;
    case DISPID_SETSECURELOCKICON:
        SetSecureLockIcon(pDispParams->rgvarg[0].lVal);
        break;
    case DISPID_STATUSTEXTCHANGE:
        StatusTextChange(pDispParams->rgvarg[0].bstrVal);
        break;
    case DISPID_TITLECHANGE:
        TitleChange(pDispParams->rgvarg[0].bstrVal);
        break;
    case DISPID_WINDOWCLOSING:
        WindowClosing(pDispParams->rgvarg[1].boolVal, pDispParams->rgvarg[0].pboolVal);
        break;
    case DISPID_WINDOWSETHEIGHT:
        WindowSetHeight(pDispParams->rgvarg[0].lVal);
        break;
    case DISPID_WINDOWSETLEFT:
        WindowSetLeft(pDispParams->rgvarg[0].lVal);
        break;
    case DISPID_WINDOWSETRESIZABLE:
        WindowSetResizable(pDispParams->rgvarg[0].boolVal);
        break;
    case DISPID_WINDOWSETTOP:
        WindowSetTop(pDispParams->rgvarg[0].lVal);
        break;
    case DISPID_WINDOWSETWIDTH:
        WindowSetWidth(pDispParams->rgvarg[0].lVal);
        break;

    default:
        return DISP_E_MEMBERNOTFOUND;
    }
#ifdef ASDF
    switch (dispIdMember)
    {
    case DISPID_BEFORENAVIGATE     :   // this is sent before navigation to give a chance to abort
        // ::MessageBox(0, "Before Navigate", 0, 0);

        return S_OK;
        break;
//  case DISPID_NAVIGATECOMPLETE   :   // in async, this is sent when we have enough to show
//
//    ::MessageBox(0, "Download complete", 0, 0);
//  break;
    case DISPID_STATUSTEXTCHANGE   :
        //case DISPID_QUIT               :
    case DISPID_DOWNLOADCOMPLETE:
        return S_OK;
    case DISPID_COMMANDSTATECHANGE :
        return S_OK;
        break;
    case DISPID_DOWNLOADBEGIN      :

        return S_OK;
        break;
        //case DISPID_NEWWINDOW          :   // sent when a new window should be created
    case DISPID_PROGRESSCHANGE     :   // sent when download progress is updated
        //case DISPID_WINDOWMOVE         :   // sent when main window has been moved
        //case DISPID_WINDOWRESIZE       :   // sent when main window has been sized
        //case DISPID_WINDOWACTIVATE     :   // sent when main window has been activated
    case DISPID_PROPERTYCHANGE: {   // sent when the PutProperty method is called
            VARIANT a = pDispParams->rgvarg[0];

            return S_OK;
        }
    case DISPID_TITLECHANGE        :   // sent when the document title changes
        //case DISPID_TITLEICONCHANGE    :   // sent when the top level window icon may have changed.
        //case DISPID_FRAMEBEFORENAVIGATE    :
        //case DISPID_FRAMENAVIGATECOMPLETE  :
        //case DISPID_FRAMENEWWINDOW         :
        return S_OK;
        break;
    case DISPID_BEFORENAVIGATE2: {   // hyperlink clicked on

            BeforeNavigate2( pDispParams->rgvarg[6].pdispVal,    // pDisp
                             pDispParams->rgvarg[5].pvarVal,     // url
                             pDispParams->rgvarg[4].pvarVal,     // Flags
                             pDispParams->rgvarg[3].pvarVal,     // TargetFrameName
                             pDispParams->rgvarg[2].pvarVal,     // PostData
                             pDispParams->rgvarg[1].pvarVal,     // Headers
                             pDispParams->rgvarg[0].pboolVal);   // Cancel

/*    char buf[200];
//
//    sprintf(buf, "cargs: %d", pDispParams->cArgs);
//    ::MessageBox(0, buf, "Before Navigate 2", 0);
//
//
//    VARIANT arg_0 = (pDispParams->rgvarg)[6];
//    if (arg_0.vt == VT_DISPATCH) {
//      ::MessageBox(0, "arg 0 is VT_DISPATCH", 0, 0);
//    }
//    else {
//      ::MessageBox(0, "arg 0 is NOT VT_DISPATCH", 0, 0);
//    }
//
//    VARIANT arg_1 = (pDispParams->rgvarg)[5];
//
//    std::string type = VariantTypeAsString(arg_1);
//
//    ::MessageBox(0, type.c_str(), "Variant type", 0);
//
//    VARIANT a;
//    if (::VariantChangeType(&a, &arg_1, 0, VT_BSTR) == S_OK) {
//      if (a.vt == VT_BSTR) {
//        ::MessageBox(0, "arg 1 is BSTR", 0, 0);
//        wcstombs(buf, a.bstrVal,199);
//        ::MessageBox(0, buf, "buf of arg_1", 0);
//      }
//      else {
//        ::MessageBox(0, "arg 1 is NOT VT_BSTR", 0, 0);
//      }
//    }
//    else {
//      ::MessageBox(0, "Couldn't convert arg 1", 0, 0);
//    }
//    VARIANT arg_2 = (pDispParams->rgvarg)[4];
//    if (arg_2.vt == (VT_BSTR | VT_BYREF)) {
//      ::MessageBox(0, "arg 4 is BSTR", 0, 0);
//    }
//    else {
//      ::MessageBox(0, "arg 4 is NOT VT_BSTR", 0, 0);
//    }
//
//    VARIANT arg_6 = pDispParams->rgvarg[0];
//    if (arg_6.vt == VT_BOOL) {
//      ::MessageBox(0, "arg 6 is VT_BOOL", 0, 0);
//    }
//    else {
//      ::MessageBox(0, "arg 6 is NOT VT_BOOL", 0, 0);
//    } */

            VARIANT arg_6 = pDispParams->rgvarg[0];
            std::string type = VariantTypeAsString(arg_6);


            return S_OK;
        }
        break;
    case DISPID_NEWWINDOW2:
        return S_OK;
    case DISPID_NAVIGATECOMPLETE2:       // UIActivate new document
        return S_OK;
        break;
        //case DISPID_ONQUIT               :
        //case DISPID_ONVISIBLE            :   // sent when the window goes visible/hidden
        //case DISPID_ONTOOLBAR            :   // sent when the toolbar should be shown/hidden
        //case DISPID_ONMENUBAR            :   // sent when the menubar should be shown/hidden
        //case DISPID_ONSTATUSBAR          :   // sent when the statusbar should be shown/hidden
        //case DISPID_ONFULLSCREEN         :   // sent when kiosk mode should be on/off
    case DISPID_DOCUMENTCOMPLETE     :   // new document goes ReadyState_Complete
        //AddSink();
        return S_OK;
        //case DISPID_ONTHEATERMODE        :   // sent when theater mode should be on/off
        //case DISPID_ONADDRESSBAR         :   // sent when the address bar should be shown/hidden
        //case DISPID_WINDOWSETRESIZABLE   :   // sent to set the style of the host window frame
        //case DISPID_WINDOWCLOSING        :   // sent before script window.close closes the window
        //case DISPID_WINDOWSETLEFT        :   // sent when the put_left method is called on the WebOC
        //case DISPID_WINDOWSETTOP         :   // sent when the put_top method is called on the WebOC
        //case DISPID_WINDOWSETWIDTH       :   // sent when the put_width method is called on the WebOC
        //case DISPID_WINDOWSETHEIGHT      :   // sent when the put_height method is called on the WebOC
        //case DISPID_CLIENTTOHOSTWINDOW   :   // sent during window.open to request conversion of dimensions
        //case DISPID_SETSECURELOCKICON    :   // sent to suggest the appropriate security icon to show
    case DISPID_FILEDOWNLOAD         :   // Fired to indicate the File Download dialog is opening
        FileDownload(pDispParams->rgvarg[0].pboolVal);
        return S_OK;

    case DISPID_NAVIGATEERROR: {   // Fired to indicate the a binding error has occurred
            char buf[200];
            //   std::string type = VariantTypeAsString(*((pDispParams->rgvarg[1]).pvarVal));
            VARIANT StatusCode=*((pDispParams->rgvarg[1]).pvarVal);
            sprintf(buf, "Navigate Error, code is: %d", static_cast<int>(StatusCode.lVal));
            MessageBox(0, buf, 0, 0);
            //case DISPID_PRIVACYIMPACTEDSTATECHANGE   :  // Fired when the user's browsing experience is impacted
            return S_OK;
        }
    default:
        char buf[50];
        sprintf(buf, "invoke, dispid: %d", static_cast<int>(dispIdMember));
        if (todo_bool) ::MessageBox(0, buf, 0, 0);
        return DISP_E_MEMBERNOTFOUND;
    }

#endif

    return S_OK;
}

void HTMLWindow::AddSink()
{
    TraceFunc("HTMLWindow::AddSink");
    IConnectionPointContainer* cpc;
    IConnectionPoint*          cp;

    if (!browserObject_)
    {
        ::MessageBox(0, "browserObject_ is null in AddSink", 0, 0);
        return;
    }
    std::string s = IIDAsString(IID_IUnknown);
    if (browserObject_->QueryInterface(IID_IConnectionPointContainer, reinterpret_cast<void**>(&cpc)) == S_OK)
    {
        if (cpc->FindConnectionPoint(DIID_DWebBrowserEvents2, &cp) == S_OK)
        {

            // TODO: Member Var
            unsigned long cookie = 1;
            if (! (SUCCEEDED(cp->Advise(static_cast<IDispatch*>(this), &cookie)) ))
            {
                ::MessageBox(0, "Advise failed", 0, 0);
            }
        }
        else
        {
            ::MessageBox(0, "FindConnectionPoint", 0, 0);
        }
    }
    else
    {
        ::MessageBox(0, "QueryInterface for IID_IConnectionPointContainer", 0, 0);
    }
}

LRESULT CALLBACK HTMLWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SIZE: {
            HTMLWindow* win = reinterpret_cast<HTMLWindow*> (::GetWindowLong(hwnd, GWL_USERDATA));
            if (win)
            {
                win->ResizeBrowser(hwnd, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        }

    case WM_CREATE: {
            return 0;
        }

    case WM_DESTROY: {
            PostQuitMessage(0);
            return TRUE;
        }

    case WM_KEYDOWN: {
            TraceFunc("WM_KEYDOWN in HTMLWindow");
            Trace2("wParam: ", wParam);
            if (wParam == VK_TAB)
            {
                /*
                   The following code is necessary to enable 'tabulator navigating' in forms.
                   See also http://www.microsoft.com/0499/faq/faq0499.asp
                   and the SendMessage part in the MessageLoop
                */
                IOleInPlaceActiveObject* ipao;
                IWebBrowser2  *webBrowser2;
                HTMLWindow* win = reinterpret_cast<HTMLWindow*> (::GetWindowLong(hwnd, GWL_USERDATA));
                if (win)
                {
                    if (!win->browserObject_->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2))
                    {
                        webBrowser2->QueryInterface(IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&ipao));
                        if (ipao)
                        {
                            MSG m;
                            m.message=WM_KEYDOWN;
                            m.wParam = wParam;
                            m.lParam = lParam;
                            m.hwnd   = hwnd;

                            ipao->TranslateAccelerator(&m);
                        }
                        else
                        {
                            ::MessageBox(0, "Failed to retrieve IOleInPlaceActiveObject in WM_KEYDOWN", 0, 0);
                        }
                    }
                    return 0;
                }
                else
                {
                    ::MessageBox(0, "Failed to retrieve webBrowser2 in WM_KEYDOWN", 0, 0);
                }
                return -1;
            }
            break;
        }

    case WM_APP: {

            TraceFunc("WM_APP called");
            HTMLWindow*  win               = reinterpret_cast<HTMLWindow* >(::GetWindowLong(hwnd, GWL_USERDATA));
            std::string* path_with_params  = reinterpret_cast<std::string*>(wParam);
            std::string path;
            std::map<std::string,std::string> params;

            Trace(std::string("path_with_params: ") + *path_with_params);

            SplitGetReq(*path_with_params, path, params);

            Trace(std::string("path: ") + path);

            std::string out_html;

            if (win->event_receiver_) win->event_receiver_->AppLink(path, out_html, params);

            /*
               DWORD todo_del_me_1;
               HANDLE todo_del_me = ::CreateFile("some.html", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
               WriteFile(todo_del_me, out_html.c_str(), out_html.size(), &todo_del_me_1, 0);
               ::CloseHandle(todo_del_me);
            */

            win->HTML(out_html);

            // url is allocated in DOCHostHandler.cpp
            Trace("going to delete url");
            //delete url;
            delete path_with_params;
            // param_map is allocated in DOCHostHandler.cpp
            Trace("going to delete param_map");

            //delete param_map;
            Trace("param_map deleted");
            return 0;
        }
    }

    return(DefWindowProc(hwnd, uMsg, wParam, lParam));
}


/*
   This method shows an URL
*/
long HTMLWindow::URL(std::string const& url)
{
    IWebBrowser2    *webBrowser2;
    VARIANT            myURL;

    TraceFunc("HTMLWindow::URL");
    Trace( (std::string("url is: ") + url).c_str());

    if (!browserObject_->QueryInterface(IID_IWebBrowser2, (void**)&webBrowser2))
    {
        VariantInit(&myURL);
        myURL.vt = VT_BSTR;

#ifndef UNICODE
        {
            wchar_t       *buffer;
            DWORD     size;

            size = MultiByteToWideChar(CP_ACP, 0, url.c_str(), -1, 0, 0);
            if (!(buffer = (wchar_t *)GlobalAlloc(GMEM_FIXED, sizeof(wchar_t) * size))) goto badalloc;
            MultiByteToWideChar(CP_ACP, 0, url.c_str(), -1, buffer, size);
            myURL.bstrVal = SysAllocString(buffer);
            GlobalFree(buffer);
        }
#else
        myURL.bstrVal = SysAllocString(url);
#endif
        if (!myURL.bstrVal)
        {
            badalloc:   webBrowser2->Release();
            return(-6);
        }

        // Navigate2 displays the page
        webBrowser2->Navigate2(&myURL, 0, 0, 0, 0);

        VariantClear(&myURL);

        webBrowser2->Release();

        return 0 ;
    }

    Trace("Something went wrong, returning -5");
    return(-5);
}
