# LumexCore/conanfile.py

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class LumexCoreConan(ConanFile):
    name = "lumex-core"
    version = "1.0.0"
    license = "Proprietary"
    author = "ViNN280801"
    url = "https://github.com/ViNN280801/LumexCore"
    description = "A core C++11 library for various utilities"
    topics = ("c++", "library", "lumex", "utilities")

    # Configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": False,
    }
    exports_sources = "CMakeLists.txt", "lumex/*", "cmake/*"

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
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # ================= Info =================
        # Consumers will use `find_package(lumex-core)`
        self.cpp_info.set_property("cmake_file_name", "LumexLib")
        self.cpp_info.set_property("cmake_target_name", "Lumex::Lumex")

        # Don't need to set global cmake_target_name, because the library is fully modular.
        # Consumers will connect specific targets, for example Lumex::settings.
        # Or they can use Lumex::Lumex to connect all components.

        # ================= Core Components =================

        # core_base64
        self.cpp_info.components["core_base64"].set_property(
            "cmake_target_name", "Lumex::base64"
        )
        self.cpp_info.components["core_base64"].libs = ["LumexCore_base64"]
        self.cpp_info.components["core_base64"].requires = [
            "core_utility",
        ]

        # core_environment
        self.cpp_info.components["core_environment"].set_property(
            "cmake_target_name", "Lumex::environment"
        )
        self.cpp_info.components["core_environment"].libs = ["LumexCore_environment"]
        self.cpp_info.components["core_environment"].requires = [
            "core_utility",
        ]

        # core_exceptions
        self.cpp_info.components["core_exceptions"].set_property(
            "cmake_target_name", "Lumex::exceptions"
        )
        self.cpp_info.components["core_exceptions"].libs = ["LumexCore_exceptions"]
        self.cpp_info.components["core_exceptions"].requires = [
            "core_environment",
            "core_filesystem",
            "core_string",
            "core_time",
            "core_utility",
        ]
        if self.settings.os == "Windows":  # type: ignore
            self.cpp_info.components["core_exceptions"].system_libs.append("dbghelp")
        elif self.settings.os in ["Linux", "FreeBSD"]:  # type: ignore
            self.cpp_info.components["core_exceptions"].system_libs.append("execinfo")

        # core_filesystem
        self.cpp_info.components["core_filesystem"].set_property(
            "cmake_target_name", "Lumex::filesystem"
        )
        self.cpp_info.components["core_filesystem"].libs = ["LumexCore_filesystem"]
        self.cpp_info.components["core_filesystem"].requires = ["core_utility"]
        if self.settings.os == "Windows":  # type: ignore
            self.cpp_info.components["core_filesystem"].system_libs.append("shlwapi")

        # core_generators_number (header-only)
        self.cpp_info.components["core_generators_number"].set_property(
            "cmake_target_name", "Lumex::number_generator"
        )

        # core_math (header-only)
        self.cpp_info.components["core_math"].set_property(
            "cmake_target_name", "Lumex::math"
        )

        # core_optional (header-only)
        self.cpp_info.components["core_optional"].set_property(
            "cmake_target_name", "Lumex::optional"
        )

        # core_string (header-only)
        self.cpp_info.components["core_string"].set_property(
            "cmake_target_name", "Lumex::string"
        )

        # core_string_view
        self.cpp_info.components["core_string_view"].set_property(
            "cmake_target_name", "Lumex::string_view"
        )
        self.cpp_info.components["core_string_view"].libs = ["LumexCore_string_view"]

        # core_temporary
        self.cpp_info.components["core_temporary"].set_property(
            "cmake_target_name", "Lumex::temporary"
        )
        self.cpp_info.components["core_temporary"].libs = ["LumexCore_temporary"]
        self.cpp_info.components["core_temporary"].requires = [
            "core_environment",
            "core_filesystem",
        ]

        # core_time
        self.cpp_info.components["core_time"].set_property(
            "cmake_target_name", "Lumex::time"
        )
        self.cpp_info.components["core_time"].libs = ["LumexCore_time"]
        self.cpp_info.components["core_time"].requires = ["core_utility"]

        # core_utility (header-only)
        self.cpp_info.components["core_utility"].set_property(
            "cmake_target_name", "Lumex::utility"
        )

        # ================= Applied Components =================

        # applied_hardware
        self.cpp_info.components["applied_hardware"].set_property(
            "cmake_target_name", "Lumex::hardware"
        )
        self.cpp_info.components["applied_hardware"].libs = ["LumexApplied_hardware"]
        self.cpp_info.components["applied_hardware"].requires = [
            "core_utility",
            "applied_logging",
        ]

        # applied_logging
        self.cpp_info.components["applied_logging"].set_property(
            "cmake_target_name", "Lumex::logging"
        )
        self.cpp_info.components["applied_logging"].libs = ["LumexApplied_logging"]
        self.cpp_info.components["applied_logging"].requires = [
            "core_environment",
            "core_filesystem",
            "core_string",
            "core_time",
        ]

        # applied_settings
        self.cpp_info.components["applied_settings"].set_property(
            "cmake_target_name", "Lumex::settings"
        )
        self.cpp_info.components["applied_settings"].libs = ["LumexApplied_settings"]
        self.cpp_info.components["applied_settings"].requires = ["core_filesystem"]
