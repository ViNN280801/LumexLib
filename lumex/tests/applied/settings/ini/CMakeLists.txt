# lumex/tests/applied/settings/ini/CMakeLists.txt

# settings has two standards in lumex/tests/LumexTestStandards.cmake, C++11 and
# C++17: LumexSettingsIniCxx11Tests and LumexSettingsIniCxx17Tests build every
# *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsIni
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
