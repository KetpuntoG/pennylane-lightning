# Copyright 2018-2024 Xanadu Quantum Technologies Inc.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import os
import platform
import subprocess
import shutil
import sys
import toml

from pathlib import Path
from setuptools import setup, Extension, find_namespace_packages
from setuptools.command.build_ext import build_ext

project_name = toml.load("pyproject.toml")['project']['name']
backend = project_name.replace("PennyLane_", "").lower()
if (backend == "lightning"): backend = "lightning_qubit"

class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = Path(sourcedir).absolute()


class CMakeBuild(build_ext):
    """
    This class is built upon https://github.com/diegoferigo/cmake-build-extension/blob/master/src/cmake_build_extension/build_extension.py and https://github.com/pybind/cmake_example/blob/master/setup.py
    """

    user_options = build_ext.user_options + [("define=", "D", "Define variables for CMake")]

    def initialize_options(self):
        super().initialize_options()
        self.define = None
        self.verbosity = ""

    def finalize_options(self):
        # Parse the custom CMake options and store them in a new attribute
        defines = [] if self.define is None else self.define.split(";")
        self.cmake_defines = [f"-D{define}" for define in defines]
        if self.verbosity != "":
            self.verbosity = "--verbose"

        super().finalize_options()

    def build_extension(self, ext: CMakeExtension):
        self.build_temp = f"build_{backend}"
        extdir = str(Path(self.get_ext_fullpath(ext.name)).parent.absolute())
        debug = int(os.environ.get("DEBUG", 0)) if self.debug is None else self.debug
        build_type = "Debug" if debug else "RelWithDebInfo"
        ninja_path = str(shutil.which("ninja"))

        build_args = ["--config", "Debug"] if debug else ["--config", "RelWithDebInfo"]
        configure_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DCMAKE_BUILD_TYPE={build_type}",  # not used on MSVC, but no harm
            "-DENABLE_WARNINGS=OFF",  # Ignore warnings
        ]
        configure_args += (
            [f"-DPYTHON_EXECUTABLE={sys.executable}"]
            if platform.system() == "Linux"
            else [f"-DPython_EXECUTABLE={sys.executable}"]
        )

        if platform.system() == "Windows":
            # As Ninja does not support long path for windows yet:
            #  (https://github.com/ninja-build/ninja/pull/2056)
            configure_args += [
                "-T clangcl",
            ]
        elif ninja_path:
            configure_args += [
                "-GNinja",
                f"-DCMAKE_MAKE_PROGRAM={ninja_path}",
            ]

        configure_args += [f"-DPL_BACKEND={backend}"]
        configure_args += self.cmake_defines

        # Add more platform dependent options
        if platform.system() == "Darwin":
            clang_path = Path(shutil.which("clang++")).parent.parent
            configure_args += [
                f"-DCMAKE_CXX_COMPILER={clang_path}/bin/clang++",
                f"-DCMAKE_LINKER={clang_path}/bin/lld",
                f"-DENABLE_GATE_DISPATCHER=OFF",
            ]
            if shutil.which("brew"):
                libomp_path = subprocess.run(
                    "brew --prefix libomp".split(" "),
                    check=False,
                    capture_output=True,
                    text=True,
                ).stdout.strip()
                if not Path(libomp_path).exists():
                    libomp_path = ""
                configure_args += (
                    [f"-DOpenMP_ROOT={libomp_path}/"] if libomp_path else ["-DENABLE_OPENMP=OFF"]
                )
        elif platform.system() == "Windows":
            configure_args += ["-DENABLE_OPENMP=OFF", "-DENABLE_BLAS=OFF"]
        elif platform.system() not in ["Linux"]:
            raise RuntimeError(f"Unsupported '{platform.system()}' platform")

        if not Path(self.build_temp).exists():
            os.makedirs(self.build_temp)

        if "CMAKE_ARGS" in os.environ:
            configure_args += os.environ["CMAKE_ARGS"].split(" ")

        subprocess.check_call(
            ["cmake", str(ext.sourcedir)] + configure_args,
            cwd=self.build_temp,
            env=os.environ,
        )
        subprocess.check_call(
            ["cmake", "--build", ".", "--verbose"] + build_args,
            cwd=self.build_temp,
            env=os.environ,
        )

with open(os.path.join("pennylane_lightning", "core", "_version.py"), encoding="utf-8") as f:
    version = f.readlines()[-1].split()[-1].strip("\"'")

packages_list = ["pennylane_lightning." + backend]

if backend == "lightning_qubit":
    packages_list += ["pennylane_lightning.core"]

info = {
    "version": version,
    "packages": find_namespace_packages(include=packages_list),
    "include_package_data": True,
    "ext_modules": (
        [] if os.environ.get("SKIP_COMPILATION", False) else [CMakeExtension(f"{backend}_ops")]
    ),
    "cmdclass": {"build_ext": CMakeBuild},
    "ext_package": "pennylane_lightning",
}

if backend == "lightning_qubit":
    info.update(
        {
            "package_data": {
                "pennylane_lightning.core": [
                    os.path.join("src", "*"),
                    os.path.join("src", "**", "*"),
                ]
            },
        }
    )

setup(**(info))
