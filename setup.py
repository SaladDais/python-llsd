import os
import sys

from setuptools import find_packages, setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.errors import PlatformError, CCompilerError, ExecError  # noqa: They exist, I swear.

root_dir = os.path.dirname(__file__)
with open(os.path.join(root_dir, "README.md")) as f:
    long_description = f.read()


class BuildFailed(Exception):
    pass


class OptionalBuildExt(build_ext):
    # This class allows C extension building to fail.

    def run(self):
        try:
            build_ext.run(self)
        except PlatformError as e:
            raise BuildFailed(e)

    def build_extension(self, ext):
        try:
            build_ext.build_extension(self, ext)
        except (CCompilerError, ExecError, PlatformError) as e:
            raise BuildFailed(e)


def run_setup(with_binary):
    kws = {}
    if with_binary:
        kws = {
            "ext_modules": [
                Extension("llsd._speedups", ["llsd/speedups.c"]),
            ],
            "cmdclass": {"build_ext": OptionalBuildExt},
        }
    setup(
        name="llsd",
        url="https://github.com/secondlife/python-llsd",
        license="MIT",
        author="Linden Research, Inc.",
        author_email="opensource-dev@lists.secondlife.com",
        description="Linden Lab Structured Data (LLSD) serialization library",
        long_description=long_description,
        long_description_content_type="text/markdown",
        packages=find_packages(exclude=("tests",)),
        setup_requires=["setuptools_scm<6"],
        use_scm_version={
            'local_scheme': 'no-local-version',  # disable local-version to allow uploads to test.pypi.org
        },
        extras_require={
            "dev": ["pytest", "pytest-cov<3"],
        },
        classifiers=[
            "Intended Audience :: Developers",
            "License :: OSI Approved :: MIT License",
            "Programming Language :: Python :: 2",
            "Programming Language :: Python :: 3",
            "Topic :: Software Development :: Libraries :: Python Modules",
            "Topic :: Software Development",
        ],
        **kws
    )


def main():
    try:
        run_setup(True)
    except BuildFailed:
        if os.environ.get("REQUIRE_LLSD_SPEEDUPS", "1") == "1":
            # Don't spam up the log with useless traceback, we're going
            # to print a C compiler error anyway.
            sys.exit(1)
        print("Failed to compile native extensions, falling back to pure Python", file=sys.stderr)
        run_setup(False)


main()
