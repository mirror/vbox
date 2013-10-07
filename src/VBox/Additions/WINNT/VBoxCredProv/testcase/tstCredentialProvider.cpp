#include <windows.h>
#include <stdio.h>
#include <WinCred.h>

int main(int argc, TCHAR* argv[])
{
    BOOL save = false;
    DWORD authPackage = 0;
    LPVOID authBuffer;
    ULONG authBufferSize = 0;
    CREDUI_INFO credUiInfo;

    credUiInfo.pszCaptionText = TEXT("VBoxCaption");
    credUiInfo.pszMessageText = TEXT("VBoxMessage");
    credUiInfo.cbSize = sizeof(credUiInfo);
    credUiInfo.hbmBanner = NULL;
    credUiInfo.hwndParent = NULL;

    DWORD dwErr = CredUIPromptForWindowsCredentials(&(credUiInfo), 0, &(authPackage),
                                                    NULL, 0, &authBuffer, &authBufferSize, &(save), 0);
    printf("Test returned %ld\n", dwErr);

    return dwErr == ERROR_SUCCESS ? 0 : 1;
}
