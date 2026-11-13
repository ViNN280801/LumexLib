# lumex/tests/applied/settings/json/CMakeLists.txt

# settings has two standards in lumex/tests/LumexTestStandards.cmake, C++11 and
# C++17: LumexSettingsJsonCxx11Tests and LumexSettingsJsonCxx17Tests build every
# *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsJson
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
