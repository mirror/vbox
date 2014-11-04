#ifndef __WINOVERRIDE_H__
#define __WINOVERRIDE_H__

#define GetProcessHeap  VBoxGetProcessHeap
#define HeapAlloc       VBoxHeapAlloc
#define HeapFree        VBoxHeapFree
#define HeapReAlloc     VBoxHeapReAlloc
#define DebugBreak      VBoxDebugBreak

HANDLE WINAPI VBoxGetProcessHeap(void);
LPVOID      WINAPI VBoxHeapAlloc(HANDLE hHeap, DWORD heaptype,SIZE_T size);
BOOL        WINAPI VBoxHeapFree(HANDLE hHeap, DWORD heaptype,LPVOID ptr);
LPVOID      WINAPI VBoxHeapReAlloc(HANDLE hHeap,DWORD heaptype,LPVOID ptr ,SIZE_T size);
void VBoxDebugBreak();

#endif /* __WINOVERRIDE_H__ */
