#define LUMEX_IMPLEMENTATION
#define NOMINMAX
#include "LumexHardwareCapabilities.hpp"
#include "lumex/applied/logging/LumexLogging"
#include "lumex/core/utility/LumexUtility"

#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

#if LUMEX_OS_WINDOWS
  #include <windows.h>

  #include <intrin.h>

  #include <d3d11.h>
  #include <dxgi.h>
  #pragma comment(lib, "d3d11.lib")
  #pragma comment(lib, "dxgi.lib")
#elif LUMEX_OS_UNIX
  #include <fstream>
  #include <regex>
  #include <sstream>
  #include <unistd.h>
#endif

LUMEX_PUBLIC_API
HardwareInfo
HardwareCapabilities::detectHardware()
{
  HardwareInfo info{};

  // Get CPU core count (cross-platform)
  info.cpuCoreCount = std::thread::hardware_concurrency();
  if(info.cpuCoreCount == 0) info.cpuCoreCount = 1; // Fallback

#if LUMEX_OS_WINDOWS
  // Get CPU name on Windows
  int cpuInfo[4]                                        = {-1};
  char cpuBrandString[Constants::KCPU_INFO_BUFFER_SIZE] = {0};

  __cpuid(cpuInfo, Constants::KCPUID_EXTENDED_FEATURES);
  int nExIds = cpuInfo[0];

  for(auto i = Constants::KCPUID_EXTENDED_FEATURES;
      i <= static_cast<uint32_t>(nExIds) && i <= Constants::KCPUID_BRAND_STRING_3; ++i)
  {
    __cpuid(cpuInfo, i);
    if(i == Constants::KCPUID_BRAND_STRING_1)
      memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
    else if(i == Constants::KCPUID_BRAND_STRING_2)
      memcpy(cpuBrandString + Constants::KCPU_BRAND_OFFSET_16, cpuInfo, sizeof(cpuInfo));
    else if(i == Constants::KCPUID_BRAND_STRING_3)
      memcpy(cpuBrandString + Constants::KCPU_BRAND_OFFSET_32, cpuInfo, sizeof(cpuInfo));
  }
  info.cpuName = std::string(cpuBrandString);

  // Get CPU frequency
  HKEY hKey;
  DWORD dwMHz  = 0;
  DWORD dwSize = sizeof(DWORD);
  if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)", 0, KEY_READ, &hKey)
     == ERROR_SUCCESS)
  {
    RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, (LPBYTE)&dwMHz, &dwSize);
    RegCloseKey(hKey);
  }
  info.cpuFrequencyMHz = dwMHz;

  // Get total memory
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  if(GlobalMemoryStatusEx(&memInfo) != 0)
    info.totalMemoryMB = static_cast<uint64_t>(memInfo.ullTotalPhys / Constants::KBYTES_TO_MB_FACTOR);

  // Check for discrete GPU using DXGI
  info.hasDiscreteGPU    = false;
  IDXGIFactory *pFactory = nullptr;
  if(SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)&pFactory)))
  {
    IDXGIAdapter *pAdapter = nullptr;
    UINT adapterIndex      = 0;
    while(pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND)
    {
      DXGI_ADAPTER_DESC desc;
      if(SUCCEEDED(pAdapter->GetDesc(&desc)))
      {
        // Convert wide string to narrow string properly
        std::string gpuName;
        int len = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
        if(len > 0)
        {
          std::vector<char> buffer(len);
          WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buffer.data(), len, nullptr, nullptr);
          gpuName = std::string(buffer.data());
        }

        // Check if it's not Microsoft Basic Render Driver or similar
        if(gpuName.find(Constants::KMICROSOFT_GPU_IDENTIFIER) == std::string::npos
           && gpuName.find(Constants::KBASIC_GPU_IDENTIFIER) == std::string::npos && desc.DedicatedVideoMemory > 0)
        {
          info.hasDiscreteGPU = true;
          info.gpuName        = gpuName;
        }
      }
      pAdapter->Release();
      adapterIndex++;
    }
    pFactory->Release();
  }

#elif LUMEX_OS_UNIX
  // Get CPU info from /proc/cpuinfo
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while(std::getline(cpuinfo, line))
  {
    if(line.find("model name") != std::string::npos)
    {
      size_t pos = line.find(':');
      if(pos != std::string::npos)
      {
        info.cpuName = line.substr(pos + 2);
        break;
      }
    }
  }
  cpuinfo.close();

  // Extract frequency from CPU name (e.g., "2.93GHz")
  std::regex freqRegex(R"((\d+\.?\d*)\s*[MG]Hz)", std::regex::icase);
  std::smatch match;
  if(std::regex_search(info.cpuName, match, freqRegex))
  {
    float freq       = std::stof(match[1]);
    std::string unit = match[0];
    if(unit.find("GHz") != std::string::npos || unit.find("ghz") != std::string::npos)
      freq *= Constants::KGHZ_TO_MHZ_FACTOR;
    info.cpuFrequencyMHz = static_cast<uint32_t>(freq);
  }

  // Get total memory from /proc/meminfo
  std::ifstream meminfo("/proc/meminfo");
  while(std::getline(meminfo, line))
  {
    if(line.find("MemTotal:") != std::string::npos)
    {
      std::istringstream iss(line);
      std::string label;
      uint64_t memKB;
      iss >> label >> memKB;
      info.totalMemoryMB = memKB / Constants::KKB_TO_MB_FACTOR;
      break;
    }
  }
  meminfo.close();

  // Check for discrete GPU by looking for common GPU vendors in lspci
  FILE *pipe = popen("lspci 2>/dev/null | grep -E 'VGA|3D|Display' | grep -v 'Intel'", "r");
  if(pipe != nullptr)
  {
    char buffer[Constants::KLSPCI_BUFFER_SIZE];
    if(fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
      info.hasDiscreteGPU = true;
      info.gpuName        = std::string(buffer);
      // Remove newline
      info.gpuName.erase(info.gpuName.find_last_not_of("\n\r") + 1);
    }
    pclose(pipe);
  }
#endif

  // Estimate CPU generation
  info.cpuGeneration = estimateCPUGeneration(info.cpuName);

  // Log detected hardware
  LumexLogging::info(KMODULE_NAME, "Detected hardware - CPU: ", info.cpuName, " (", std::to_string(info.cpuCoreCount),
                     " cores, ", std::to_string(info.cpuFrequencyMHz), " MHz), ",
                     "RAM: ", std::to_string(info.totalMemoryMB), " MB, ",
                     "GPU: ", info.hasDiscreteGPU ? info.gpuName : "Integrated/None", ", CPU Generation: ~",
                     std::to_string(info.cpuGeneration));

  return info;
}

LUMEX_PUBLIC_API
uint32_t
HardwareCapabilities::estimateCPUGeneration(std::string const &cpuName)
{
  // Order of checks: more specific (unique older identifiers) first, then general patterns (newer generations)

  // Intel CPU generation detection

  // Intel Core 2 (e.g., Q6600, E8400)
  if(cpuName.find(Constants::KINTEL_CORE2_IDENTIFIER) != std::string::npos) return Constants::KCPU_CORE2_GENERATION;

  // Intel 1st Gen Core i-series (Nehalem) (e.g., i5-750, i7-9xx)
  if(cpuName.find(Constants::KINTEL_I5_750_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_9_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_NEHALEM_GENERATION;

  // Intel Pentium G-series (often Ivy Bridge based, like G2020)
  if(cpuName.find(Constants::KINTEL_PENTIUM_G_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_IVY_BRIDGE_GENERATION;

  // IMPORTANT: Check longer identifiers FIRST to avoid false matches
  // (e.g., "i3-12" must be checked before "i3-2" to avoid matching "i3-2" inside "i3-12")

  // Intel 14th Gen (Meteor Lake) (e.g., i3-14xxx, i5-14xxx, i7-14xxx, i9-14xxx)
  if(cpuName.find(Constants::KINTEL_I3_14_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_14_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_14_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I9_14_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_METEOR_LAKE_GENERATION;

  // Intel 13th Gen (Raptor Lake) (e.g., i3-13xxx, i5-13xxx, i7-13xxx, i9-13xxx)
  if(cpuName.find(Constants::KINTEL_I3_13_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_13_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_13_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I9_13_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_RAPTOR_LAKE_GENERATION;

  // Intel 12th Gen (Alder Lake) (e.g., i3-12xxx, i5-12xxx, i7-12xxx, i9-12xxx)
  if(cpuName.find(Constants::KINTEL_I3_12_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_12_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_12_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I9_12_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_ALDER_LAKE_GENERATION;

  // Intel 11th Gen (Rocket Lake) (e.g., i3-11xxx, i5-11xxx, i7-11xxx)
  if(cpuName.find(Constants::KINTEL_I3_11_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_11_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_11_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_ROCKET_LAKE_GENERATION;

  // Intel 10th Gen (Comet Lake) (e.g., i3-10xxx, i5-10xxx, i7-10xxx)
  if(cpuName.find(Constants::KINTEL_I3_10_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_10_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_10_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_COMET_LAKE_GENERATION;

  // Intel 8th Gen (Coffee Lake) (e.g., i3-8xxx, i5-8xxx, i7-8xxx)
  if(cpuName.find(Constants::KINTEL_I3_8_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_8_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_8_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_COFFEE_LAKE_GENERATION;

  // Intel 7th Gen (Kaby Lake) (e.g., i3-7xxx, i5-7xxx, i7-7xxx)
  if(cpuName.find(Constants::KINTEL_I3_7_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_7_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_7_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_KABY_LAKE_GENERATION;

  // Intel 6th Gen (Skylake) (e.g., i3-6xxx, i5-6xxx, i7-6xxx)
  if(cpuName.find(Constants::KINTEL_I3_6_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_6_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_6_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_SKYLAKE_GENERATION;

  // Intel 4th Gen (Haswell) (e.g., i3-4xxx, i5-4xxx, i7-4xxx)
  if(cpuName.find(Constants::KINTEL_I3_4_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_4_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_4_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_HASWELL_GENERATION;

  // Intel 3rd Gen (Ivy Bridge) (e.g., i3-3xxx, i5-3xxx, i7-3xxx)
  if(cpuName.find(Constants::KINTEL_I3_3_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_3_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_3_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_IVY_BRIDGE_GENERATION;

  // Intel 2nd Gen (Sandy Bridge) (e.g., i3-2xxx, i5-2xxx, i7-2xxx)
  if(cpuName.find(Constants::KINTEL_I3_2_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I5_2_IDENTIFIER) != std::string::npos
     || cpuName.find(Constants::KINTEL_I7_2_IDENTIFIER) != std::string::npos)
    return Constants::KCPU_SANDY_BRIDGE_GENERATION;

  // AMD CPU detection (order matters here too, specific Ryzen series first)
  if(cpuName.find(Constants::KAMD_RYZEN_IDENTIFIER) != std::string::npos)
  {
    if(cpuName.find(Constants::KAMD_RYZEN_7000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_7000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_6000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_6000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_5000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_5000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_4000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_4000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_3000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_3000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_2000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_2000_GENERATION;
    if(cpuName.find(Constants::KAMD_RYZEN_1000_IDENTIFIER) != std::string::npos)
      return Constants::KCPU_RYZEN_1000_GENERATION;
  }

  // Default to 2015 if unknown (conservative estimate)
  return Constants::KCPU_DEFAULT_GENERATION;
}

LUMEX_PUBLIC_API
bool
HardwareCapabilities::isOldCPU(std::string const &cpuName)
{
  // List of known old CPU families
  std::vector<std::string> const oldCPUs = {Constants::KOLD_CPU_PENTIUM,
                                            Constants::KOLD_CPU_CELERON,
                                            Constants::KINTEL_CORE2_IDENTIFIER,
                                            Constants::KOLD_CPU_ATOM,
                                            Constants::KOLD_CPU_ATHLON64,
                                            Constants::KOLD_CPU_PHENOM,
                                            Constants::KOLD_CPU_SEMPRON,
                                            Constants::KINTEL_I3_2_IDENTIFIER,      // Sandy Bridge is old enough
                                            Constants::KINTEL_PENTIUM_G_IDENTIFIER, // Ivy Bridge based Pentium
                                            Constants::KAMD_FX_IDENTIFIER,          // AMD FX series
                                            Constants::KAMD_A_SERIES_IDENTIFIER};   // AMD A-series APUs

  std::string lowerCpu                   = cpuName;
  std::transform(lowerCpu.begin(), lowerCpu.end(), lowerCpu.begin(), ::tolower);

  for(auto const &oldCpu : oldCPUs)
  {
    std::string lowerOldCpu = oldCpu;
    std::transform(lowerOldCpu.begin(), lowerOldCpu.end(), lowerOldCpu.begin(), ::tolower);
    if(lowerCpu.find(lowerOldCpu) != std::string::npos) return true;
  }

  return false;
}

LUMEX_PUBLIC_API
bool
HardwareCapabilities::shouldUseSoftwareRendering()
{
  HardwareInfo info = detectHardware();

  bool useSoftware  = false;
  std::string reason;

  // Check each criterion
  if(info.totalMemoryMB < Constants::KMIN_MEMORY_MB)
  {
    useSoftware = true;
    reason += "Low RAM (" + std::to_string(info.totalMemoryMB) + " MB < " + std::to_string(Constants::KMIN_MEMORY_MB)
              + " MB), ";
  }

  if(info.cpuCoreCount < Constants::KMIN_CPU_CORES)
  {
    useSoftware = true;
    reason += "Few CPU cores (" + std::to_string(info.cpuCoreCount) + " < " + std::to_string(Constants::KMIN_CPU_CORES)
              + "), ";
  }

  if(info.cpuGeneration < Constants::KMIN_CPU_GENERATION)
  {
    useSoftware = true;
    reason += "Old CPU generation (~" + std::to_string(info.cpuGeneration) + "), ";
  }

  if(isOldCPU(info.cpuName))
  {
    useSoftware = true;
    reason += "Known old CPU family, ";
  }

  if(!info.hasDiscreteGPU)
  {
    // Only consider this if other factors also indicate weak hardware
    if(info.totalMemoryMB < Constants::KHIGH_MEMORY_THRESHOLD_MB || info.cpuCoreCount < Constants::KHIGH_CPU_CORES)
    {
      useSoftware = true;
      reason += "No discrete GPU + weak specs, ";
    }
  }

  // Remove trailing comma and space
  if(!reason.empty() && reason.size() >= 2) reason = reason.substr(0, reason.size() - 2);

  if(useSoftware)
  {
    LumexLogging::warning(KMODULE_NAME,
                          "Hardware is below recommended specs. "
                          "Enabling software rendering. Reasons: ",
                          reason);
  }
  else
  {
    LumexLogging::info(KMODULE_NAME, "Hardware meets recommended specs. Using "
                                     "hardware-accelerated rendering.");
  }

  return useSoftware;
}

LUMEX_PUBLIC_API
void
HardwareCapabilities::applyOptimalRenderingSettings()
{
  if(shouldUseSoftwareRendering())
  {
    LumexLogging::info(KMODULE_NAME, "Applying software rendering settings for "
                                     "optimal performance on this hardware");

#if LUMEX_OS_WINDOWS
    _putenv_s("QT_QUICK_BACKEND", "software");
    _putenv_s("QSG_RENDER_LOOP", "basic");
#else
    setenv("QT_QUICK_BACKEND", "software", 1);
    setenv("QSG_RENDER_LOOP", "basic", 1);
#endif

    LumexLogging::success(KMODULE_NAME, "Software rendering settings applied successfully");
  }
  else { LumexLogging::info(KMODULE_NAME, "Hardware rendering will be used (default Qt settings)"); }
}

LUMEX_PUBLIC_API
std::string
Lumex::Applied::Hardware::getMacAddress()
{
#if LUMEX_OS_WINDOWS
  // Windows implementation using GetAdaptersInfo
  ULONG buffer_size = 0;
  DWORD result      = GetAdaptersInfo(nullptr, std::addressof(buffer_size));
  if(result != ERROR_BUFFER_OVERFLOW) return {};

  std::vector<BYTE> buffer(buffer_size);
  PIP_ADAPTER_INFO adapter_info
    = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  result = GetAdaptersInfo(adapter_info, std::addressof(buffer_size));
  if(result != NO_ERROR) return {};

  // Find first active adapter
  constexpr DWORD ETHERNET_MAC_LENGTH = 6;
  for(PIP_ADAPTER_INFO adapter = adapter_info; adapter != nullptr; adapter = adapter->Next)
  {
    if(adapter->Type == MIB_IF_TYPE_ETHERNET && adapter->AddressLength == ETHERNET_MAC_LENGTH)
    {
      std::ostringstream mac_stream;
      for(DWORD i = 0; i < adapter->AddressLength; ++i)
      {
        if(i > 0) mac_stream << ":";
        mac_stream << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<int>(
                        adapter->Address[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      }
      return mac_stream.str();
    }
  }
  return {};
#else
  // Linux implementation using getifaddrs
  struct ifaddrs *ifaddrs_ptr = nullptr;
  if(getifaddrs(&ifaddrs_ptr) != 0) return {};

  std::string mac_address;
  for(struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next)
  {
    if(ifa->ifa_addr == nullptr) continue;

    // Для Linux используем AF_PACKET вместо AF_LINK
    if(ifa->ifa_addr->sa_family != AF_PACKET) continue;

    auto *sll                         = reinterpret_cast<struct sockaddr_ll *>(ifa->ifa_addr);
    constexpr int ETHERNET_MAC_LENGTH = 6;

    if(sll->sll_halen == ETHERNET_MAC_LENGTH)
    {
      std::ostringstream mac_stream;
      unsigned char *mac = sll->sll_addr; // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
      for(int i = 0; i < ETHERNET_MAC_LENGTH; ++i)
      {
        if(i > 0) mac_stream << ":";
        mac_stream << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<int>(mac[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      }
      mac_address = mac_stream.str();
      break;
    }
  }
  freeifaddrs(ifaddrs_ptr);
  return mac_address;
#endif
}

LUMEX_PUBLIC_API
std::uint64_t
Lumex::Applied::Hardware::generateCryptographicSeed()
{
#if LUMEX_OS_WINDOWS
  // Windows: Try CryptGenRandom first
  HCRYPTPROV h_prov = 0;
  if(CryptAcquireContext(&h_prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
  {
    std::uint64_t seed = 0;
    if(CryptGenRandom(h_prov, sizeof(seed),
                      reinterpret_cast< // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                        BYTE *>(std::addressof(seed)))
       != 0)
    {
      CryptReleaseContext(h_prov, 0);
      return seed;
    }
    CryptReleaseContext(h_prov, 0);
  }
#else
  // POSIX: Try getentropy() first (POSIX.1-2024, Issue 8)
  std::uint64_t seed = 0;
  if(getentropy(std::addressof(seed), sizeof(seed)) == 0) return seed;

  // Fallback: Try /dev/urandom
  std::ifstream urandom("/dev/urandom", std::ios::binary);
  if(urandom.is_open())
  {
    urandom.read(reinterpret_cast< // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                   char *>(std::addressof(seed)),
                 sizeof(seed));
    if(urandom.good()) return seed;
  }
#endif

  // Final fallback: Use std::random_device (uses hardware RNG where available)
  std::random_device random_device;
  std::uniform_int_distribution<std::uint64_t> distribution;
  return distribution(random_device);
}
