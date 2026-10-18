"""PlatformIO extra script for [env:native_coverage]: link with gcov.

build_flags only reach the compiler; --coverage must also be passed to the linker.
"""

Import("env")  # type: ignore  # noqa: F821 - provided by PlatformIO/SCons

env.Append(LINKFLAGS=["--coverage"])  # type: ignore  # noqa: F821
