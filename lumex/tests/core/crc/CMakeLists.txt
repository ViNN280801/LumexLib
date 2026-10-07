# lumex/tests/core/crc/CMakeLists.txt

# The tests follow the source tree (lumex/core/crc): catalog/ tests the
# runtime catalog, calculate_crc8 and the transport mode, parametric/ the
# parametric engine over every spec. Each has one suite per standard of the
# module (lumex/tests/LumexTestStandards.cmake).
add_subdirectory(catalog)
add_subdirectory(parametric)
