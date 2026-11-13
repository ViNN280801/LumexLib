# lumex/tests/applied/settings/xml/CMakeLists.txt

# settings has two standards in lumex/tests/LumexTestStandards.cmake, C++11 and
# C++17: LumexSettingsXmlCxx11Tests and LumexSettingsXmlCxx17Tests build every
# *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsXml
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
