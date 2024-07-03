#pragma once

#include "../../vulkan/vulkan_loader.h"

#include "../wsi_monitor.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
  };
  
  inline bool isDisplayValid(int32_t displayId) {
    return displayId == 0;
  }

}