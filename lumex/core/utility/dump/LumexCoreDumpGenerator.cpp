#define LUMEX_IMPLEMENTATION
#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"

namespace lumex
{
namespace core
{
namespace utility
{
namespace dump
{

// The single definition of every static data member. The header declares
// them without `inline`, so these definitions must not be `inline` either:
// a C++17 inline variable defined in one translation unit only is emitted
// there only when that unit odr-uses it, and ELF shared libraries then
// export no symbol for the constant-initialized members.
std::unique_ptr<CoreDumpGenerator> CoreDumpGenerator::s_instance = nullptr;
std::mutex CoreDumpGenerator::s_mutex;
std::condition_variable CoreDumpGenerator::s_operationCondition;
std::mutex CoreDumpGenerator::s_operationMutex;
std::atomic<std::size_t> CoreDumpGenerator::s_activeOperations{};
#if __cplusplus >= 201103L
std::once_flag CoreDumpGenerator::s_initFlag;
#endif
std::string CoreDumpGenerator::s_dumpDirectory = "DumpCreatorCrashDump";
#if __cplusplus >= 201103L
std::atomic_bool CoreDumpGenerator::s_initialized{};
#else
bool CoreDumpGenerator::s_initialized = false;
#endif
std::string CoreDumpGenerator::s_originalCorePattern;
DumpConfiguration CoreDumpGenerator::s_currentConfig;

std::map<int, void (*) (int)> CoreDumpGenerator::s_customSignalHandlers;
std::mutex CoreDumpGenerator::s_customHandlersMutex;

#if (LUMEX_OS_IS_UNIX() || LUMEX_OS_IS_ANDROID())
std::atomic_bool CoreDumpGenerator::s_monitorThreadShouldStop{ false };
std::thread CoreDumpGenerator::s_monitorThread;
pid_t CoreDumpGenerator::s_applicationPid = getpid ();
std::string CoreDumpGenerator::s_adminGroupName;
void (*CoreDumpGenerator::s_unixConsoleHandler) () = nullptr;
std::atomic_bool CoreDumpGenerator::s_posixSigThreadStarted{};
#endif
#if LUMEX_OS_IS_WINDOWS()
BOOL (WINAPI *CoreDumpGenerator::s_customConsoleHandler) (DWORD) = nullptr;
#endif

std::map<DumpType, std::string> const DumpFactory::s_descriptions = {
  { DumpType::MINI_DUMP_NORMAL, "Basic mini-dump (64KB)" },
  { DumpType::MINI_DUMP_WITH_DATA_SEGS, "Mini-dump with data segments" },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY, "Full memory mini-dump (largest)" },
  { DumpType::MINI_DUMP_WITH_HANDLE_DATA, "Mini-dump with handle data" },
  { DumpType::MINI_DUMP_FILTER_MEMORY, "Filtered memory mini-dump" },
  { DumpType::MINI_DUMP_SCAN_MEMORY, "Scanned memory mini-dump" },
  { DumpType::MINI_DUMP_WITH_UNLOADED_MODULES,
    "Mini-dump with unloaded modules" },
  { DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY,
    "Mini-dump with indirectly referenced memory" },
  { DumpType::MINI_DUMP_FILTER_MODULE_PATHS,
    "Mini-dump with filtered module paths" },
  { DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA,
    "Mini-dump with process/thread data" },
  { DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY,
    "Mini-dump with private read/write memory" },
  { DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA,
    "Mini-dump without optional data" },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO,
    "Mini-dump with full memory info" },
  { DumpType::MINI_DUMP_WITH_THREAD_INFO, "Mini-dump with thread info" },
  { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS, "Mini-dump with code segments" },
  { DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE,
    "Mini-dump without auxiliary state" },
  { DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE,
    "Mini-dump with full auxiliary state" },
  { DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY,
    "Mini-dump with private write-copy memory" },
  { DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY,
    "Mini-dump ignoring inaccessible memory" },
  { DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION,
    "Mini-dump with token information" },
  { DumpType::KERNEL_FULL_DUMP, "Full kernel dump - largest kernel dump" },
  { DumpType::KERNEL_KERNEL_DUMP, "Kernel memory dump - kernel memory only" },
  { DumpType::KERNEL_SMALL_DUMP, "Small kernel dump - 64KB" },
  { DumpType::KERNEL_AUTOMATIC_DUMP, "Automatic kernel dump - flexible size" },
  { DumpType::KERNEL_ACTIVE_DUMP,
    "Active kernel dump - similar to full but smaller" },
  { DumpType::CORE_DUMP_FULL, "Full core dump with all memory" },
  { DumpType::DEFAULT_WINDOWS, "Default Windows dump type" },
  { DumpType::DEFAULT_UNIX, "Default UNIX dump type" },
  { DumpType::DEFAULT_AUTO, "Auto-detect based on platform" }
};

std::map<DumpType, bool> const DumpFactory::s_platformSupport = {
  { DumpType::MINI_DUMP_NORMAL, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_DATA_SEGS, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_HANDLE_DATA, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_FILTER_MEMORY, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_SCAN_MEMORY, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_UNLOADED_MODULES, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY,
    LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_FILTER_MODULE_PATHS, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY,
    LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_THREAD_INFO, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY,
    LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY, LUMEX_OS_IS_WINDOWS () },
  { DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION, LUMEX_OS_IS_WINDOWS () },
  { DumpType::KERNEL_FULL_DUMP, LUMEX_OS_IS_WINDOWS () },
  { DumpType::KERNEL_KERNEL_DUMP, LUMEX_OS_IS_WINDOWS () },
  { DumpType::KERNEL_SMALL_DUMP, LUMEX_OS_IS_WINDOWS () },
  { DumpType::KERNEL_AUTOMATIC_DUMP, LUMEX_OS_IS_WINDOWS () },
  { DumpType::KERNEL_ACTIVE_DUMP, LUMEX_OS_IS_WINDOWS () },
  { DumpType::CORE_DUMP_FULL, !LUMEX_OS_IS_WINDOWS () },
  { DumpType::DEFAULT_WINDOWS, LUMEX_OS_IS_WINDOWS () },
  { DumpType::DEFAULT_UNIX, !LUMEX_OS_IS_WINDOWS () },
  { DumpType::DEFAULT_AUTO, true }
};

std::map<DumpType, std::size_t> const DumpFactory::s_estimatedSizes = {
  { DumpType::MINI_DUMP_NORMAL, CoreDumpGenerator::KB_64 },
  { DumpType::MINI_DUMP_WITH_DATA_SEGS, CoreDumpGenerator::KB_128 },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY, 0 },
  { DumpType::MINI_DUMP_WITH_HANDLE_DATA, CoreDumpGenerator::KB_256 },
  { DumpType::MINI_DUMP_FILTER_MEMORY, CoreDumpGenerator::KB_64 },
  { DumpType::MINI_DUMP_SCAN_MEMORY, CoreDumpGenerator::KB_128 },
  { DumpType::MINI_DUMP_WITH_UNLOADED_MODULES, CoreDumpGenerator::KB_512 },
  { DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY, 0 },
  { DumpType::MINI_DUMP_FILTER_MODULE_PATHS, CoreDumpGenerator::KB_64 },
  { DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA, CoreDumpGenerator::MB_1 },
  { DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY, 0 },
  { DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA, CoreDumpGenerator::KB_32 },
  { DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO, 0 },
  { DumpType::MINI_DUMP_WITH_THREAD_INFO, CoreDumpGenerator::KB_256 },
  { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS, CoreDumpGenerator::KB_512 },
  { DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE, CoreDumpGenerator::KB_64 },
  { DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE, CoreDumpGenerator::MB_1 },
  { DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY, 0 },
  { DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY, CoreDumpGenerator::KB_64 },
  { DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION, CoreDumpGenerator::KB_128 },
  { DumpType::KERNEL_FULL_DUMP, 0 },
  { DumpType::KERNEL_KERNEL_DUMP, 0 },
  { DumpType::KERNEL_SMALL_DUMP, CoreDumpGenerator::KB_64 },
  { DumpType::KERNEL_AUTOMATIC_DUMP, 0 },
  { DumpType::KERNEL_ACTIVE_DUMP, 0 },
  { DumpType::CORE_DUMP_FULL, 0 },
  { DumpType::DEFAULT_WINDOWS, 0 },
  { DumpType::DEFAULT_UNIX, 0 },
  { DumpType::DEFAULT_AUTO, 0 }
};

} // namespace dump
} // namespace utility
} // namespace core
} // namespace lumex
