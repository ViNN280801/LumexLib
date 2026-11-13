# lumex/tests/applied/settings/CMakeLists.txt

# The tests follow the source tree (lumex/applied/settings): guard/, ini/,
# json/ and xml/; factory/ and interface/ have no tests of their own.
# settings has two standards in lumex/tests/LumexTestStandards.cmake, C++11 and
# C++17: LumexSettingsCxx11Tests builds LumexSettingsSave.cxx11.tests.cpp (saves
# through the three backends) and LumexSettingsPath.cxx11.tests.cpp (the
# lumex::path overloads); LumexSettingsCxx17Tests builds both again and adds
# LumexSettingsStdPath.cxx17.tests.cpp (a std::filesystem::path is not a
# lumex::path). None of them belongs to a directory.
lumex_add_standard_suites(Settings
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)

add_subdirectory(guard)
add_subdirectory(ini)
add_subdirectory(json)
add_subdirectory(xml)
