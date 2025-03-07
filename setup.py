import os
import sys
from os.path import dirname
from setuptools import setup, Extension, find_packages

__lib_name__ = "simsimd"
__version__ = "5.0.1"

compile_args = []
link_args = []
macros_args = [
    ("SIMSIMD_NATIVE_F16", "0"),
    ("SIMSIMD_NATIVE_BF16", "0"),
    ("SIMSIMD_DYNAMIC_DISPATCH", "1"),
]


def get_bool_env(name: str, preference: bool) -> bool:
    return os.environ.get(name, "1" if preference else "0") == "1"


def get_bool_env_w_name(name: str, preference: bool) -> tuple:
    return name, "1" if get_bool_env(name, preference) else "0"


if sys.platform == "linux":
    compile_args.append("-std=c11")
    compile_args.append("-O3")
    compile_args.append("-ffast-math")
    compile_args.append("-fdiagnostics-color=always")

    # Disable warnings
    compile_args.append("-w")

    # Enable OpenMP for Linux
    compile_args.append("-fopenmp")
    link_args.append("-fopenmp")

    # Add vectorized `logf` implementation from the `glibc`
    link_args.append("-lm")

    # SIMD all the way on Linux!
    macros_args.extend(
        [
            get_bool_env_w_name("SIMSIMD_TARGET_NEON", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SVE", True),
            get_bool_env_w_name("SIMSIMD_TARGET_HASWELL", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SKYLAKE", True),
            get_bool_env_w_name("SIMSIMD_TARGET_ICE", True),
            get_bool_env_w_name("SIMSIMD_TARGET_GENOA", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SAPPHIRE", True),
        ]
    )

if sys.platform == "darwin":
    compile_args.append("-std=c11")
    compile_args.append("-O3")
    compile_args.append("-ffast-math")

    # Disable warnings
    compile_args.append("-w")

    # We can't SIMD all the way on MacOS :(
    macros_args.extend(
        [
            get_bool_env_w_name("SIMSIMD_TARGET_NEON", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SVE", False),
            get_bool_env_w_name("SIMSIMD_TARGET_HASWELL", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SKYLAKE", False),
            get_bool_env_w_name("SIMSIMD_TARGET_ICE", False),
            get_bool_env_w_name("SIMSIMD_TARGET_GENOA", False),
            get_bool_env_w_name("SIMSIMD_TARGET_SAPPHIRE", False),
        ]
    )

if sys.platform == "win32":
    compile_args.append("/std:c11")
    compile_args.append("/O2")
    compile_args.append("/fp:fast")

    # Dealing with MinGW linking errors
    # https://cibuildwheel.readthedocs.io/en/stable/faq/#windows-importerror-dll-load-failed-the-specific-module-could-not-be-found
    compile_args.append("/d2FH4-")

    # We can't SIMD all the way on Windows :(
    macros_args.extend(
        [
            get_bool_env_w_name("SIMSIMD_TARGET_NEON", True),
            get_bool_env_w_name("SIMSIMD_TARGET_NEON_BF16", False),
            get_bool_env_w_name("SIMSIMD_TARGET_SVE", False),
            get_bool_env_w_name("SIMSIMD_TARGET_HASWELL", True),
            get_bool_env_w_name("SIMSIMD_TARGET_SKYLAKE", True),
            get_bool_env_w_name("SIMSIMD_TARGET_ICE", True),
            get_bool_env_w_name("SIMSIMD_TARGET_GENOA", False),
            get_bool_env_w_name("SIMSIMD_TARGET_SAPPHIRE", False),
        ]
    )

ext_modules = [
    Extension(
        "simsimd",
        sources=["python/lib.c", "c/lib.c"],
        include_dirs=["include"],
        extra_compile_args=compile_args,
        extra_link_args=link_args,
        define_macros=macros_args,
    ),
]

this_directory = os.path.abspath(dirname(__file__))
with open(os.path.join(this_directory, "README.md"), "r", encoding="utf8") as f:
    long_description = f.read()

setup(
    name=__lib_name__,
    version=__version__,
    author="Ash Vardanian",
    author_email="1983160+ashvardanian@users.noreply.github.com",
    url="https://github.com/ashvardanian/simsimd",
    description="Fastest SIMD-Accelerated Vector Similarity Functions for x86 and Arm",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="Apache-2.0",
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: MacOS",
        "Development Status :: 5 - Production/Stable",
        "Natural Language :: English",
        "Intended Audience :: Developers",
        "Intended Audience :: Information Technology",
        "Programming Language :: C",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Scientific/Engineering :: Mathematics",
        "Topic :: Scientific/Engineering :: Information Analysis",
        "Topic :: Scientific/Engineering :: Bio-Informatics",
        "Topic :: Scientific/Engineering :: Chemistry",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
    ],
    ext_modules=ext_modules,
    zip_safe=False,
    packages=["simsimd"],
    package_dir={"simsimd": "python/simsimd"},
    package_data={"simsimd": ["__init__.pyi", "py.typed"]},
    include_package_data=True,
)
