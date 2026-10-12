"""PlatformIO extra script for [env:native_sanitize]: link with the sanitizer runtimes.

build_flags only reach the compiler; the sanitizer flags must also be passed to the linker.
"""

Import("env")  # type: ignore  # noqa: F821 - provided by PlatformIO/SCons

env.Append(LINKFLAGS=["-fsanitize=address,undefined", "-fno-sanitize-recover=all"])  # type: ignore  # noqa: F821
