# LumexLib/conanfile.py
#
# Conan 2 recipe. Consumers use `find_package(LumexLib)` and link either one
# module target (`lumex::fmt`, `lumex::settings`, ...) or the umbrella
# `lumex::Lumex`. The components below mirror the CMake targets: keep them in
# step with `cmake/LumexModules.cmake` and every module's CMakeLists.txt
# (quality-gates section 10).

import os
import re

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class LumexLibConan(ConanFile):
    name = "lumex-core"
    license = "MIT"
    author = "Vladislav Semykin <vladislav.semykin@gmail.com>"
    url = "https://github.com/ViNN280801/LumexCore"
    description = (
        "Modular C++ utility library (C++11 floor; math, to_json and "
        "std::format-style compile-time checks need C++20, resource_monitor "
        "needs C++17): formatting, strings, logging, settings, JSON, XML, "
        "filesystem, time, diagnostics"
    )
    topics = ("c++", "library", "lumex", "utilities", "format", "xml", "logging")
    package_type = "library"

    # Configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
    }
    # Shared by default, as LUMEX_BUILD_SHARED_LIBS: LumexXml is always a
    # shared library, and on Windows LUMEX_API imports the other modules.
    default_options = {
        "shared": True,
        "fPIC": True,
        "with_tests": False,
    }
    # CMakeRoutines holds the build helpers the root CMakeLists.txt includes;
    # 3rdparty holds the vendored nlohmann/json (settings, logger) and
    # GoogleTest (with_tests).
    exports_sources = (
        "CMakeLists.txt",
        "LICENSE",
        "cmake/*",
        "CMakeRoutines/*",
        "3rdparty/*",
        "lumex/*",
    )

    def set_version(self):
        """The version of `project(LumexLib VERSION ...)`."""
        content = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        match = re.search(r"project\(\s*LumexLib\s+VERSION\s+([0-9.]+)", content)
        if match is None:
            raise ValueError("CMakeLists.txt: no project(LumexLib VERSION ...)")
        self.version = match.group(1)

    def config_options(self):
        if self.settings.os == "Windows":  # type: ignore
            self.options.rm_safe("fPIC")  # type: ignore

    def configure(self):
        if self.options.shared:  # type: ignore
            self.options.rm_safe("fPIC")  # type: ignore

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["LUMEX_BUILD_SHARED_LIBS"] = self.options.shared  # type: ignore
        tc.variables["LUMEX_BUILD_TESTS"] = self.options.with_tests  # type: ignore
        tc.variables["LUMEX_BUILD_EXAMPLES"] = False
        tc.variables["LUMEX_BUILD_BENCHMARKS"] = False
        tc.variables["LUMEX_BUILD_DOCUMENTATION"] = False
        tc.variables["LUMEX_INSTALL"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        # with_tests compiles the library suite into the build tree. ctest
        # runs here, before package(); the binaries are not installed.
        if self.options.with_tests:  # type: ignore
            cmake.test()

    def package(self):
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()

    def _component(self, name, target, libs=None, requires=None):
        """One component per CMake target `lumex::<target>`."""
        component = self.cpp_info.components[name]
        component.set_property("cmake_target_name", "lumex::" + target)
        component.libs = libs or []
        component.requires = requires or []
        # As the CMake export (cmake/LumexBuild.cmake): Lumex headers test
        # __cplusplus, which MSVC reports as 199711L without this flag.
        if self.settings.compiler == "msvc":  # type: ignore
            component.cxxflags = ["/Zc:__cplusplus"]
        return component

    def package_info(self):
        # Consumers call find_package(LumexLib) and link lumex::<module> or
        # the umbrella lumex::Lumex.
        self.cpp_info.set_property("cmake_file_name", "LumexLib")
        self.cpp_info.set_property("cmake_target_name", "lumex::Lumex")
        windows = self.settings.os == "Windows"  # type: ignore

        # ================= Core components =================
        # Header-only (CMake INTERFACE targets): no libs.
        self._component("core_math", "math")
        self._component("core_optional", "optional")
        self._component("core_string", "string")
        self._component("core_generators_number", "number_generator")
        self._component(
            "core_utility", "utility", ["LumexCore_utility"], ["core_math"]
        )
        self._component("core_circular_buffer", "circular_buffer",
                        requires=["core_utility"])
        self._component("core_expected", "expected", requires=["core_utility"])
        self._component("core_fmt", "fmt", requires=["core_utility"])
        self._component("core_reflection", "reflection",
                        requires=["core_utility"])

        self._component(
            "core_base64", "base64", ["LumexCore_base64"], ["core_utility"]
        )
        self._component("core_crc", "crc", ["LumexCore_crc"], ["core_utility"])
        self._component(
            "core_environment", "environment", ["LumexCore_environment"],
            ["core_utility"],
        )
        filesystem = self._component(
            "core_filesystem", "filesystem", ["LumexCore_filesystem"],
            ["core_utility"],
        )
        if windows:
            filesystem.system_libs.append("shlwapi")
        self._component(
            "core_string_view", "string_view", ["LumexCore_string_view"],
            ["core_utility"],
        )
        self._component(
            "core_time", "time", ["LumexCore_time"], ["core_environment"]
        )
        self._component(
            "core_temporary", "temporary", ["LumexCore_temporary"],
            ["core_environment", "core_filesystem"],
        )
        exceptions = self._component(
            "core_exceptions", "exceptions", ["LumexCore_exceptions"],
            ["core_environment", "core_filesystem", "core_string", "core_time",
             "core_utility"],
        )
        if windows:
            exceptions.system_libs.append("dbghelp")
        elif self.settings.os == "FreeBSD":  # type: ignore
            # glibc has backtrace () built in; FreeBSD needs libexecinfo.
            exceptions.system_libs.append("execinfo")

        # ================= XML =================
        xml = self._component("xml", "xml", ["LumexXml"], ["core_utility"])
        xml.includedirs = ["include", os.path.join("include", "lumex", "xml")]

        # ================= Applied components =================
        self._component(
            "applied_logging", "logging", ["LumexApplied_logging"],
            ["core_environment", "core_filesystem", "core_string", "core_time"],
        )
        self._component(
            "applied_hardware", "hardware", ["LumexApplied_hardware"],
            ["applied_logging", "core_utility"],
        )
        self._component(
            "applied_resource_monitor", "resource_monitor",
            ["LumexApplied_resource_monitor"], ["applied_logging", "core_time"],
        )
        serial = self._component(
            "applied_serial", "serial", ["LumexApplied_serial"],
            ["core_utility"],
        )
        if windows:
            serial.system_libs.extend(["setupapi", "advapi32"])
        settings = self._component(
            "applied_settings", "settings", ["LumexApplied_settings"],
            ["core_filesystem", "applied_logging", "core_time", "xml"],
        )
        # The package is built with XML and the vendored nlohmann/json.
        settings.defines = ["LUMEX_SETTINGS_WITH_XML", "LUMEX_SETTINGS_WITH_JSON"]
        logger = self._component(
            "applied_logger", "logger", ["LumexApplied_logger"]
        )
        # Default LUMEX_LOGGER_CONFIG_FORMAT (PLAIN_TEXT): no reader library.
        logger.defines = ["LUMEX_LOGGER_CONFIG_FORMAT_PLAIN_TEXT"]
        if windows:
            logger.system_libs.append("dbghelp")
        # Header-only; compiles against the consumer's own nlohmann/json.
        self._component(
            "applied_json", "json",
            requires=["core_reflection", "core_string_view", "core_utility"],
        )
