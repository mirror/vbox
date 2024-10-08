#include <windows.h>

namespace dxvk::wsi {

  inline void* fromHwnd(HWND hWindow) {
    return reinterpret_cast<void*>(hWindow);
  }

  inline HWND toHwnd(void* pWindow) {
    return reinterpret_cast<HWND>(pWindow);
  }

  // Offset so null HMONITORs go to -1
  inline int32_t fromHmonitor(HMONITOR hMonitor) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(hMonitor)) - 1;
  }

  // Offset so -1 display id goes to 0 == NULL
  inline HMONITOR toHmonitor(int32_t displayId) {
    return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(displayId + 1));
  }

}