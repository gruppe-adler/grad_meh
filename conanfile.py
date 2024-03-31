from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps

class MehRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("fmt/10.2.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("pcl/1.13.1")
        self.requires("clipper2/1.3.0")
        self.requires("plog/1.1.10")
        self.requires("gdal/3.8.3")
        self.requires("openimageio/2.5.9.0")
        self.requires("libpng/1.6.43", override=True)
        self.requires("boost/1.84.0", override=True)

    def build_requirements(self):
        self.tool_requires("cmake/3.29.0")
