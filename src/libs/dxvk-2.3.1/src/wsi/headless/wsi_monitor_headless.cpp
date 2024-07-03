#include "../wsi_monitor.h"

#include <windows.h>

#include "wsi/native_wsi.h"
#include "wsi_platform_headless.h"

#include "../../util/log/log.h"

#include <string>
#include <sstream>

namespace dxvk::wsi {

  HMONITOR getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR enumMonitors(uint32_t index) {
    return isDisplayValid(int32_t(index))
      ? toHmonitor(index)
      : nullptr;
  }


  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }


  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    const int32_t displayId    = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    std::wstringstream nameStream;
    nameStream << LR"(\\.\DISPLAY)" << (displayId + 1);

    std::wstring name = nameStream.str();

    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);

    return true;
  }


  bool getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    const int32_t displayId    = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = 1024;
    pRect->bottom = 1024;

    return true;
  }


  bool getDisplayMode(
      HMONITOR hMonitor,
      uint32_t ModeNumber,
      WsiMode* pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);
    if (!isDisplayValid(displayId))
      return false;

    pMode->width        = 1024;
    pMode->height       = 1024;
    pMode->refreshRate  = WsiRational{60 * 1000, 1000};
    pMode->bitsPerPixel = 32;
    pMode->interlaced   = false;
    return true;
  }


  bool getCurrentDisplayMode(
      HMONITOR hMonitor,
      WsiMode* pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    pMode->width        = 1024;
    pMode->height       = 1024;
    pMode->refreshRate  = WsiRational{60 * 1000, 1000};
    pMode->bitsPerPixel = 32;
    pMode->interlaced   = false;
    return true;
  }


  std::vector<uint8_t> getMonitorEdid(HMONITOR hMonitor) {
    Logger::err("getMonitorEdid not implemented on this platform.");
    return {};
  }

}