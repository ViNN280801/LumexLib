# lumex/tests/applied/settings/CMakeLists.txt

# The tests follow the source tree (lumex/applied/settings): guard/, ini/,
# json/ and xml/; factory/ and interface/ have no tests of their own.
# settings has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSettingsCxx11Tests builds LumexSettingsSave.cxx11.tests.cpp, which
# saves through the three backends and belongs to none of the directories.
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
