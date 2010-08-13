
#include <windows.h>
#include "HTMLWindow.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE unused__, LPSTR lpszCmdLine, int nCmdShow)
{
    TCHAR szCurDir [_MAX_PATH+1] ={ 0};
    TCHAR szFile [_MAX_PATH+1] ={ 0};
    GetCurrentDirectory( _MAX_PATH, szCurDir);

    strcat (szFile, szCurDir);

    if ((szCurDir[strlen(szFile)-1] != '\\') &&
        (szCurDir[strlen(szFile)-1] != '/'))
    {
        strcat (szFile, "\\");
    }

    strcat (szFile, "Menu\\index.html");

    HTMLWindow* pWindow = new HTMLWindow(szFile, "Sun VirtualBox", hInstance, true);
    if (NULL == pWindow)
        return 1;

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        if (msg.message >= WM_KEYFIRST &&
            msg.message <= WM_KEYLAST)
        {
            ::SendMessage(pWindow->hwnd_, msg.message, msg.wParam, msg.lParam);
        }

        if (msg.message==WM_QUIT)
            break;

        DispatchMessage(&msg);
    }

    delete pWindow;
    pWindow = NULL;

    return 0;
}
