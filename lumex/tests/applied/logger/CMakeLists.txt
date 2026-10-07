# lumex/tests/applied/logger/CMakeLists.txt

# The tests follow the source tree (lumex/applied/logger): config/ tests the
# config file formats, logger/ tests the logger. Each directory has one suite
# per standard of the module (lumex/tests/LumexTestStandards.cmake).
add_subdirectory(config)
add_subdirectory(logger)
