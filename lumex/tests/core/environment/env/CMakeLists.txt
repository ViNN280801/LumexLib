# lumex/tests/core/environment/env/CMakeLists.txt

# environment has one standard in lumex/tests/LumexTestStandards.cmake, C++11:
# LumexEnvironmentEnvCxx11Tests builds every *.cxx11.tests.cpp of this
# directory.
lumex_add_standard_suites(EnvironmentEnv
    MODULE environment
    LINK
        lumex::environment
)
