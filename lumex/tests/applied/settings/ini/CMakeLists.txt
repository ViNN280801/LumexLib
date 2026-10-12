# lumex/tests/applied/settings/ini/CMakeLists.txt

# settings has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSettingsIniCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsIni
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
