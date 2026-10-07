# lumex/tests/applied/settings/xml/CMakeLists.txt

# settings has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexSettingsXmlCxx11Tests builds every *.cxx11.tests.cpp of this directory.
lumex_add_standard_suites(SettingsXml
    MODULE settings
    LINK
        lumex::filesystem
        lumex::settings
        lumex::utility
)
