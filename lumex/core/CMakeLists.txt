# lumex/core/CMakeLists.txt
#
# Order is dependency-first so ALIAS targets exist at
# target_link_libraries() time (CMake requires that).

lumex_add_subdirectory_if(LUMEX_BUILD_MATH math)
lumex_add_subdirectory_if(LUMEX_BUILD_SPAN span)
lumex_add_subdirectory_if(LUMEX_BUILD_OPTIONAL optional)
lumex_add_subdirectory_if(LUMEX_BUILD_UTILITY utility)
lumex_add_subdirectory_if(LUMEX_BUILD_UNICODE unicode)
# string_view is linked by base64, crc, exceptions and xml: add it before them.
lumex_add_subdirectory_if(LUMEX_BUILD_STRING_VIEW string_view)
# hazard_pointer links utility and span: add it after them.
lumex_add_subdirectory_if(LUMEX_BUILD_HAZARD_POINTER hazard_pointer)
# contracts links utility: add it after it.
lumex_add_subdirectory_if(LUMEX_BUILD_CONTRACTS contracts)
# smart_ptr links utility; the atomic module (phase 2) links it softly: add it before atomic.
lumex_add_subdirectory_if(LUMEX_BUILD_SMART_PTR smart_ptr)
lumex_add_subdirectory_if(LUMEX_BUILD_ATOMIC atomic)
lumex_add_subdirectory_if(LUMEX_BUILD_CIRCULAR_BUFFER circular_buffer)
lumex_add_subdirectory_if(LUMEX_BUILD_EXPECTED expected)
lumex_add_subdirectory_if(LUMEX_BUILD_GENERATORS generators)
lumex_add_subdirectory_if(LUMEX_BUILD_STRING string)
lumex_add_subdirectory_if(LUMEX_BUILD_FMT fmt)
lumex_add_subdirectory_if(LUMEX_BUILD_BASE64 base64)
lumex_add_subdirectory_if(LUMEX_BUILD_CRC crc)
lumex_add_subdirectory_if(LUMEX_BUILD_ENVIRONMENT environment)
# time PUBLIC-links environment (measure_time env gate); add after it.
lumex_add_subdirectory_if(LUMEX_BUILD_TIME time)
lumex_add_subdirectory_if(LUMEX_BUILD_FILESYSTEM filesystem)
lumex_add_subdirectory_if(LUMEX_BUILD_REFLECTION reflection)
lumex_add_subdirectory_if(LUMEX_BUILD_TEMPORARY temporary)
lumex_add_subdirectory_if(LUMEX_BUILD_EXCEPTIONS exceptions)
