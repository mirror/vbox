#ifndef HTMLWINDOW_H__
#define HTMLWINDOW_H__

#include <windows.h>
#include <exdisp.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <oaidl.h>

#include <string>
#include <map>

#include "DocHostUiHandler.h"
#include "OleInPlaceFrame.h"
#include "OleInPlaceSite.h"

// TODO
#define MUST_BE_IMPLEMENTED(s) ::MessageBox(0, s, "Not Implemented", 0);  return E_NOTIMPL;

/*
  Thanks to http://www.codeproject.com/com/cwebpage.asp for a great example
*/


extern bool todo_bool;

class EventReceiver
{
    friend class HTMLWindow;
protected:
    virtual void AppLink(std::string const&, std::string& out_html, const std::map<std::string,std::string>& param_map) = 0;
};

class HTMLWindow :
public virtual DWebBrowserEvents2
{

    friend class DocHostUiHandler;

public:
    typedef std::map<std::string, std::string> param_list;

    HTMLWindow(std::string const& html_or_url,
               std::string const& title,
               HINSTANCE,
               bool is_url);

    HTMLWindow(EventReceiver*,
               std::string const& title,
               HINSTANCE,
               std::string const& first_link = "start_app",
               param_list  const& first_params = param_list());

    virtual ~HTMLWindow();

    long HTML(std::string const&);
    long URL (std::string const& url);
    void ResizeBrowser (HWND hwnd, DWORD width, DWORD height);
    void AddSink();

    HWND               hwnd_;

private:
    static bool ole_is_initialized_;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    long EmbedBrowserObject();

    void InitOle_CreateWindow(std::string const& title);

    HINSTANCE          instance_;
    IOleObject*        browserObject_;

    IOleClientSite*    ole_client_site_;
    IStorage*          storage_;
    OleInPlaceSite*   ole_in_place_site_;
    OleInPlaceFrame*  ole_in_place_frame_;
    DocHostUiHandler* doc_host_ui_handler_;

    EventReceiver*     event_receiver_;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return 1;
    }

    void STDMETHODCALLTYPE BeforeNavigate2(
                                          IDispatch*    pDisp,
                                          VARIANT*      url,
                                          VARIANT*      Flags,
                                          VARIANT*      TargetFrameName,
                                          VARIANT*      PostData,
                                          VARIANT*      Headers,
                                          VARIANT_BOOL* Cancel);

    void ClientToHostWindow(
                           long* CX,
                           long* CY
                           )
    {
    }

    void CommandStateChange(          long Command,
                                      VARIANT_BOOL Enable
                           )
    {
    }

    void DocumentComplete(          IDispatch* pDisp,
                                    VARIANT* URL
                         )
    {
    }

    void DownloadBegin(VOID)
    {
    }

    void DownloadComplete(VOID)
    {
    }

    void FileDownload(VARIANT_BOOL* Cancel);

    void NavigateComplete(IDispatch* pDisp, VARIANT* URL)
    {
    }

    void NavigateError(          IDispatch* pDisp,
                                 VARIANT* URL,
                                 VARIANT* TargetFrameName,
                                 VARIANT* StatusCode,
                                 VARIANT_BOOL* Cancel
                      )
    {
    }

    void NewWindow2(
                   IDispatch**   ppDisp,
                   VARIANT_BOOL* Cancel
                   )
    {
    }

    void OnFullScreen(VARIANT_BOOL FullScreen)
    {
    }

    void OnMenuBar(VARIANT_BOOL MenuBar)
    {
    }

    void OnQuit(VOID)
    {
    }

    void OnStatusBar(VARIANT_BOOL StatusBar)
    {
    }

    void OnTheaterMode(VARIANT_BOOL TheaterMode)
    {
    }

    void OnToolBar(VARIANT_BOOL ToolBar)
    {
    }

    void OnVisible(VARIANT_BOOL Visible)
    {
        ::MessageBox(0, "OnVisible", 0, 0);
    }

    void PrintTemplateInstantiation(IDispatch* pDisp)
    {
    }

    void PrintTemplateTeardown(IDispatch* pDisp)
    {
    }

    void PrivacyImpactedStateChange(VARIANT_BOOL PrivacyImpacted)
    {
    }

    void ProgressChange(long Progress, long ProgressMax)
    {
    }

    void PropertyChange(BSTR szProperty)
    {
    }

    void SetSecureLockIcon(long SecureLockIcon)
    {
    }

    void StatusTextChange(BSTR Text)
    {
    }

    void TitleChange(BSTR Text)
    {
    }

    void WindowClosing(VARIANT_BOOL IsChildWindow, VARIANT_BOOL* Cancel)
    {
    }

    void WindowSetHeight(long Height)
    {
    }

    void WindowSetLeft(long Left)
    {
    }

    void WindowSetResizable(VARIANT_BOOL Resizable)
    {
    }

    void WindowSetTop(long Top)
    {
    }

    void WindowSetWidth(long Width)
    {
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount( unsigned int FAR*  pctinfo)
    {
        MUST_BE_IMPLEMENTED("GetTypeInfoCount")
        *pctinfo = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(
                                         unsigned int  iTInfo,
                                         LCID  lcid,
                                         ITypeInfo FAR* FAR*  ppTInfo
                                         )
    {
        MUST_BE_IMPLEMENTED("GetTypeInfo")
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(
                                           REFIID             riid,
                                           OLECHAR FAR* FAR*  rgszNames,
                                           unsigned int       cNames,
                                           LCID               lcid,
                                           DISPID       FAR*  rgDispId
                                           )
    {
        MUST_BE_IMPLEMENTED("GetIDsOfNames")
    }

    HRESULT STDMETHODCALLTYPE Invoke(
                                    DISPID             dispIdMember,
                                    REFIID             riid,
                                    LCID               lcid,
                                    WORD               wFlags,
                                    DISPPARAMS   FAR*  pDispParams,
                                    VARIANT      FAR*  pVarResult,
                                    EXCEPINFO    FAR*  pExcepInfo,
                                    unsigned int FAR*  puArgErr);
};

#endif
