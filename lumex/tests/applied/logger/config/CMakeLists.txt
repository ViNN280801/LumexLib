# lumex/tests/applied/logger/config/CMakeLists.txt

# One suite per standard of the module (lumex/tests/LumexTestStandards.cmake):
# LumexLoggerConfigCxx11Tests, LumexLoggerConfigCxx14Tests,
# LumexLoggerConfigCxx17Tests and LumexLoggerConfigCxx20Tests, all built from
# LumexLoggerConfigFormat.cxx11.tests.cpp. The format the logger reads its
# config file in (LUMEX_LOGGER_CONFIG_FORMAT_*) comes with lumex::logger.
# STANDARDS narrows the module's row to 11 17 20 for this directory. Nothing in
# its tests or in the sources they test differs at C++14 from the next lower
# standard (the text of the directory's own files after preprocessing is the
# same, no __cplusplus threshold or LUMEX_HAS_* feature of those standards is
# used there; "Narrowing the row of one directory" in
# lumex/tests/LumexTestStandards.cmake), so the dropped suites would run the
# same code again; the suite names above for them do not exist.
lumex_add_standard_suites(LoggerConfig
    MODULE logger
    LINK lumex::logger
    STANDARDS 11 17 20
)
