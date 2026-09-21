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

LUMEX_INLINE_VARIABLE std::unique_ptr<CoreDumpGenerator>
    CoreDumpGenerator::s_instance = nullptr;
LUMEX_INLINE_VARIABLE std::mutex CoreDumpGenerator::s_mutex;
LUMEX_INLINE_VARIABLE std::condition_variable
    CoreDumpGenerator::s_operationCondition;
LUMEX_INLINE_VARIABLE std::mutex CoreDumpGenerator::s_operationMutex;
LUMEX_INLINE_VARIABLE std::atomic<std::size_t>
    CoreDumpGenerator::s_activeOperations{};
#if CPP11_OR_GREATER
LUMEX_INLINE_VARIABLE std::once_flag CoreDumpGenerator::s_initFlag;
#endif
LUMEX_INLINE_VARIABLE std::string CoreDumpGenerator::s_dumpDirectory
    = "DumpCreatorCrashDump";
#if CPP11_OR_GREATER
LUMEX_INLINE_VARIABLE std::atomic_bool CoreDumpGenerator::s_initialized{};
#else
LUMEX_INLINE_VARIABLE bool CoreDumpGenerator::s_initialized = false;
#endif
LUMEX_INLINE_VARIABLE std::string CoreDumpGenerator::s_originalCorePattern;
LUMEX_INLINE_VARIABLE DumpConfiguration CoreDumpGenerator::s_currentConfig;

LUMEX_INLINE_VARIABLE std::map<int, void (*) (int)>
    CoreDumpGenerator::s_customSignalHandlers;
LUMEX_INLINE_VARIABLE std::mutex CoreDumpGenerator::s_customHandlersMutex;

#if DUMP_CREATOR_UNIX
LUMEX_INLINE_VARIABLE std::atomic_bool
    CoreDumpGenerator::s_monitorThreadShouldStop{ false };
LUMEX_INLINE_VARIABLE std::thread CoreDumpGenerator::s_monitorThread;
LUMEX_INLINE_VARIABLE pid_t CoreDumpGenerator::s_applicationPid = getpid ();
LUMEX_INLINE_VARIABLE std::string CoreDumpGenerator::s_adminGroupName;
LUMEX_INLINE_VARIABLE void (*CoreDumpGenerator::s_unixConsoleHandler) ()
    = nullptr;
LUMEX_INLINE_VARIABLE std::atomic_bool
    CoreDumpGenerator::s_posixSigThreadStarted{};
#endif
#if DUMP_CREATOR_WINDOWS
LUMEX_INLINE_VARIABLE
BOOL (WINAPI *CoreDumpGenerator::s_customConsoleHandler) (DWORD) = nullptr;
#endif

LUMEX_INLINE_VARIABLE std::map<DumpType, std::string> const
    DumpFactory::s_descriptions
    = { { DumpType::MINI_DUMP_NORMAL, "Basic mini-dump (64KB)" },
        { DumpType::MINI_DUMP_WITH_DATA_SEGS, "Mini-dump with data segments" },
        { DumpType::MINI_DUMP_WITH_FULL_MEMORY,
          "Full memory mini-dump (largest)" },
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
        { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS,
          "Mini-dump with code segments" },
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
        { DumpType::KERNEL_FULL_DUMP,
          "Full kernel dump - largest kernel dump" },
        { DumpType::KERNEL_KERNEL_DUMP,
          "Kernel memory dump - kernel memory only" },
        { DumpType::KERNEL_SMALL_DUMP, "Small kernel dump - 64KB" },
        { DumpType::KERNEL_AUTOMATIC_DUMP,
          "Automatic kernel dump - flexible size" },
        { DumpType::KERNEL_ACTIVE_DUMP,
          "Active kernel dump - similar to full but smaller" },
        { DumpType::CORE_DUMP_FULL, "Full core dump with all memory" },
        { DumpType::DEFAULT_WINDOWS, "Default Windows dump type" },
        { DumpType::DEFAULT_UNIX, "Default UNIX dump type" },
        { DumpType::DEFAULT_AUTO, "Auto-detect based on platform" } };

LUMEX_INLINE_VARIABLE std::map<DumpType, bool> const
    DumpFactory::s_platformSupport
    = { { DumpType::MINI_DUMP_NORMAL, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_DATA_SEGS, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_FULL_MEMORY, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_HANDLE_DATA, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_FILTER_MEMORY, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_SCAN_MEMORY, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_UNLOADED_MODULES, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY,
          DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_FILTER_MODULE_PATHS, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY,
          DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_THREAD_INFO, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE, DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE,
          DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY,
          DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY,
          DUMP_CREATOR_WINDOWS },
        { DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION, DUMP_CREATOR_WINDOWS },
        { DumpType::KERNEL_FULL_DUMP, DUMP_CREATOR_WINDOWS },
        { DumpType::KERNEL_KERNEL_DUMP, DUMP_CREATOR_WINDOWS },
        { DumpType::KERNEL_SMALL_DUMP, DUMP_CREATOR_WINDOWS },
        { DumpType::KERNEL_AUTOMATIC_DUMP, DUMP_CREATOR_WINDOWS },
        { DumpType::KERNEL_ACTIVE_DUMP, DUMP_CREATOR_WINDOWS },
        { DumpType::CORE_DUMP_FULL, !DUMP_CREATOR_WINDOWS },
        { DumpType::DEFAULT_WINDOWS, DUMP_CREATOR_WINDOWS },
        { DumpType::DEFAULT_UNIX, !DUMP_CREATOR_WINDOWS },
        { DumpType::DEFAULT_AUTO, true } };

LUMEX_INLINE_VARIABLE std::map<DumpType, std::size_t> const
    DumpFactory::s_estimatedSizes
    = { { DumpType::MINI_DUMP_NORMAL, CoreDumpGenerator::KB_64 },
        { DumpType::MINI_DUMP_WITH_DATA_SEGS, CoreDumpGenerator::KB_128 },
        { DumpType::MINI_DUMP_WITH_FULL_MEMORY, 0 },
        { DumpType::MINI_DUMP_WITH_HANDLE_DATA, CoreDumpGenerator::KB_256 },
        { DumpType::MINI_DUMP_FILTER_MEMORY, CoreDumpGenerator::KB_64 },
        { DumpType::MINI_DUMP_SCAN_MEMORY, CoreDumpGenerator::KB_128 },
        { DumpType::MINI_DUMP_WITH_UNLOADED_MODULES,
          CoreDumpGenerator::KB_512 },
        { DumpType::MINI_DUMP_WITH_INDIRECTLY_REFERENCED_MEMORY, 0 },
        { DumpType::MINI_DUMP_FILTER_MODULE_PATHS, CoreDumpGenerator::KB_64 },
        { DumpType::MINI_DUMP_WITH_PROCESS_THREAD_DATA,
          CoreDumpGenerator::MB_1 },
        { DumpType::MINI_DUMP_WITH_PRIVATE_READ_WRITE_MEMORY, 0 },
        { DumpType::MINI_DUMP_WITHOUT_OPTIONAL_DATA,
          CoreDumpGenerator::KB_32 },
        { DumpType::MINI_DUMP_WITH_FULL_MEMORY_INFO, 0 },
        { DumpType::MINI_DUMP_WITH_THREAD_INFO, CoreDumpGenerator::KB_256 },
        { DumpType::MINI_DUMP_WITH_CODE_SEGMENTS, CoreDumpGenerator::KB_512 },
        { DumpType::MINI_DUMP_WITHOUT_AUXILIARY_STATE,
          CoreDumpGenerator::KB_64 },
        { DumpType::MINI_DUMP_WITH_FULL_AUXILIARY_STATE,
          CoreDumpGenerator::MB_1 },
        { DumpType::MINI_DUMP_WITH_PRIVATE_WRITE_COPY_MEMORY, 0 },
        { DumpType::MINI_DUMP_IGNORE_INACCESSIBLE_MEMORY,
          CoreDumpGenerator::KB_64 },
        { DumpType::MINI_DUMP_WITH_TOKEN_INFORMATION,
          CoreDumpGenerator::KB_128 },
        { DumpType::KERNEL_FULL_DUMP, 0 },
        { DumpType::KERNEL_KERNEL_DUMP, 0 },
        { DumpType::KERNEL_SMALL_DUMP, CoreDumpGenerator::KB_64 },
        { DumpType::KERNEL_AUTOMATIC_DUMP, 0 },
        { DumpType::KERNEL_ACTIVE_DUMP, 0 },
        { DumpType::CORE_DUMP_FULL, 0 },
        { DumpType::DEFAULT_WINDOWS, 0 },
        { DumpType::DEFAULT_UNIX, 0 },
        { DumpType::DEFAULT_AUTO, 0 } };

} // namespace dump
} // namespace utility
} // namespace core
} // namespace lumex
