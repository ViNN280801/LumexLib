import os

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout


class LumexLibTestConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        # The package carries the sources of the examples and the helper they
        # include: share/examples/<module>/CMakeLists.txt builds against it.
        share = os.path.join(self.dependencies["lumex-core"].package_folder, "share")
        for relative in (("examples", "cmake", "LumexExampleHelpers.cmake"),
                         ("examples", "span", "CMakeLists.txt"),
                         ("benchmarks", "README.md"),
                         ("LumexLib", "LICENSE")):
            path = os.path.join(share, *relative)
            if not os.path.isfile(path):
                raise ConanException("the package has no " + path)
        if can_run(self):
            cmd = os.path.join(self.cpp.build.bindir, "lumex_consumer")
            self.run(cmd, env="conanrun")
