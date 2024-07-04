#include "dxvk_device_filter.h"

namespace dxvk {
  
  DxvkDeviceFilter::DxvkDeviceFilter(DxvkDeviceFilterFlags flags)
  : m_flags(flags) {
    m_matchDeviceName = env::getEnvVar("DXVK_FILTER_DEVICE_NAME");
    
    if (m_matchDeviceName.size() != 0)
      m_flags.set(DxvkDeviceFilterFlag::MatchDeviceName);
  }
  
  
  DxvkDeviceFilter::~DxvkDeviceFilter() {
    
  }
  
  
  bool DxvkDeviceFilter::testAdapter(const VkPhysicalDeviceProperties& properties) const {
#if defined(VBOX) && defined(RT_OS_DARWIN)
    /* MoltenVK only supports Vulkan 1.2 right now as there are some extensions missing for full 1.3 support. */
    if (properties.apiVersion < VK_MAKE_VERSION(1, 2, 0)) {
#else
    if (properties.apiVersion < VK_MAKE_VERSION(1, 3, 0)) {
#endif
      Logger::warn(str::format("Skipping Vulkan ",
        VK_VERSION_MAJOR(properties.apiVersion), ".",
        VK_VERSION_MINOR(properties.apiVersion), " adapter: ",
        properties.deviceName));
      return false;
    }

    if (m_flags.test(DxvkDeviceFilterFlag::MatchDeviceName)) {
      if (std::string(properties.deviceName).find(m_matchDeviceName) == std::string::npos)
        return false;
    }

    if (m_flags.test(DxvkDeviceFilterFlag::SkipCpuDevices)) {
      if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        Logger::warn(str::format("Skipping CPU adapter: ", properties.deviceName));
        return false;
      }
    }

    return true;
  }
  
}
