from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
import os


class FtxuiConan(ConanFile):
    name = "ftxui"
    version = "6.2.0"
    description = "Functional Terminal (X) User Interface"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/ArthurSonzogni/FTXUI"
    topics = ("tui", "terminal", "ui", "cpp20")
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(
            self,
            f"https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v{self.version}.tar.gz",
            strip_root=True,
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["FTXUI_BUILD_EXAMPLES"] = False
        tc.variables["FTXUI_BUILD_TESTS"] = False
        tc.variables["FTXUI_BUILD_DOCS"] = False
        tc.variables["FTXUI_ENABLE_INSTALL"] = True
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "ftxui")

        # screen (lowest level)
        self.cpp_info.components["screen"].set_property(
            "cmake_target_name", "ftxui::screen"
        )
        self.cpp_info.components["screen"].libs = ["ftxui-screen"]

        # dom depends on screen
        self.cpp_info.components["dom"].set_property(
            "cmake_target_name", "ftxui::dom"
        )
        self.cpp_info.components["dom"].libs = ["ftxui-dom"]
        self.cpp_info.components["dom"].requires = ["screen"]

        # component depends on dom + screen
        self.cpp_info.components["component"].set_property(
            "cmake_target_name", "ftxui::component"
        )
        self.cpp_info.components["component"].libs = ["ftxui-component"]
        self.cpp_info.components["component"].requires = ["dom", "screen"]

        # ui (high-level layer) depends on component
        self.cpp_info.components["ui"].set_property(
            "cmake_target_name", "ftxui::ui"
        )
        self.cpp_info.components["ui"].libs = ["ftxui-ui"]
        self.cpp_info.components["ui"].requires = ["component", "dom", "screen"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["component"].system_libs = ["pthread"]
