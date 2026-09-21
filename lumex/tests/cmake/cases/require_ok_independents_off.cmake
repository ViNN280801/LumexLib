# Modules with no Lumex-module CMake link can be OFF while the rest stay ON.

include("${LUMEX_SOURCE_DIR}/lumex/tests/cmake/setup_all_on.cmake")
set(LUMEX_BUILD_CIRCULAR_BUFFER OFF)
set(LUMEX_BUILD_EXPECTED OFF)
set(LUMEX_BUILD_OPTIONAL OFF)
set(LUMEX_BUILD_GENERATORS OFF)
set(LUMEX_BUILD_LOGGER OFF)
include("${LUMEX_SOURCE_DIR}/cmake/LumexModules.cmake")
lumex_check_module_dependencies()
