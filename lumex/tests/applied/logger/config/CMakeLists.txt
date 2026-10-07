# lumex/tests/applied/logger/config/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexLoggerConfigCxx11Tests, LumexLoggerConfigCxx14Tests,
# LumexLoggerConfigCxx17Tests and LumexLoggerConfigCxx20Tests, all built from
# LumexLoggerConfigFormat.cxx11.tests.cpp. The format the logger reads its
# config file in (LUMEX_LOGGER_CONFIG_FORMAT_*) comes with lumex::logger.
lumex_add_standard_suites(LoggerConfig
    MODULE logger
    LINK lumex::logger
)
