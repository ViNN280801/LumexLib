# lumex/tests/applied/settings/guard/CMakeLists.txt

# settings has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSettingsGuardCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsGuard
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
