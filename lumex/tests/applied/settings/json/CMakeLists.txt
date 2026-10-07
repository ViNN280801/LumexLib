# lumex/tests/applied/settings/json/CMakeLists.txt

# settings has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSettingsJsonCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsJson
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
