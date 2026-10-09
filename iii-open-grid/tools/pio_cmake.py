"""PlatformIO glue for the iii firmware.

PlatformIO installs the tools (pico-sdk, Arm GCC, CMake, Ninja, picotool);
the firmware itself is built by iii-open-grid/CMakeLists.txt, the same way
as a plain pico-sdk checkout. This script replaces PlatformIO's own compile
step with a CMake + Ninja build and hands the resulting ELF back, so the
regular PlatformIO targets (UF2 generation, size, upload) keep working.

Used as `extra_scripts = pre:iii-open-grid/tools/pio_cmake.py`.
"""

import shutil
import subprocess
from os.path import isabs, join

Import("env")  # noqa: F821  (provided by PlatformIO)

platform = env.PioPlatform()
project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")

firmware_dir = join(project_dir, "iii-open-grid")
cmake_dir = join(build_dir, "cmake")

og_board = env.GetProjectOption("custom_og_board")
og_i2c_hz = env.GetProjectOption("custom_og_i2c_hz", "100000")
# Optional: use a pico-sdk checkout instead of the framework-picosdk package.
sdk_override = env.GetProjectOption("custom_pico_sdk_path", "")


def package_dir(name):
    path = platform.get_package_dir(name)
    if not path:
        env.Exit("Package %s is not installed; check platform_packages in platformio.ini" % name)
    return path


def find_tool(package, relpath, exe):
    """Prefer the PlatformIO package; fall back to the tool on PATH."""
    path = platform.get_package_dir(package)
    if path:
        return join(path, relpath)
    return shutil.which(exe)


def pico_sdk_path():
    if sdk_override:
        return sdk_override if isabs(sdk_override) else join(project_dir, sdk_override)
    return package_dir("framework-picosdk")


def generator_args():
    ninja = find_tool("tool-ninja", "ninja", "ninja")
    if ninja:
        return ["-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + ninja]
    make = shutil.which("make") or shutil.which("mingw32-make")
    if not make:
        env.Exit("Neither ninja nor make found; add tool-ninja to platform_packages")
    # MinGW Makefiles is for cmd.exe-style shells; CMake refuses it when sh.exe is on PATH.
    gen = "Unix Makefiles" if shutil.which("sh") or not env.subst("$PLATFORM").startswith("win") else "MinGW Makefiles"
    return ["-G", gen, "-DCMAKE_MAKE_PROGRAM=" + make]


def run(cmd):
    print(" ".join('"%s"' % c if " " in c else c for c in cmd))
    if subprocess.call(cmd) != 0:
        env.Exit(1)


def cmake_build(target, source, env):
    cmake = find_tool("tool-cmake", join("bin", "cmake"), "cmake")
    if not cmake:
        env.Exit("CMake not found; add tool-cmake to platform_packages")
    run([
        cmake, "-S", firmware_dir, "-B", cmake_dir,
        *generator_args(),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DPICO_SDK_PATH=" + pico_sdk_path(),
        "-DPICO_TOOLCHAIN_PATH=" + package_dir("toolchain-gccarmnoneeabi"),
        "-DPython3_EXECUTABLE=" + env.subst("$PYTHONEXE"),
        # PlatformIO makes the UF2 itself; don't let the SDK fetch and build picotool.
        "-Dpicotool_FOUND=0",
        "-DOG_BOARD=" + og_board,
        "-DOG_I2C_HZ=" + og_i2c_hz,
    ])
    run([cmake, "--build", cmake_dir])
    shutil.copyfile(join(cmake_dir, "iii-open-grid.elf"), str(target[0]))


def build_program(env):
    elf = env.Command(
        join("$BUILD_DIR", "${PROGNAME}.elf"),
        [],
        env.VerboseAction(cmake_build, "Building iii-open-grid (%s) with CMake" % og_board),
    )
    env.AlwaysBuild(elf)  # Ninja decides what is out of date
    # What PlatformIO's own BuildProgram also sets up; other targets depend on it.
    env.Replace(PIOMAINPROG=elf)
    env.AlwaysBuild(env.Alias(
        "checkprogsize", elf, env.VerboseAction(env.CheckUploadSize, "Checking size $PIOMAINPROG")))
    return elf


env.AddMethod(build_program, "BuildProgram")

# The platform's UF2 step calls a bare `picotool`; the Arduino builder normally
# puts it on PATH, so do the same here.
picotool_dir = platform.get_package_dir("tool-picotool-rp2040-earlephilhower")
if picotool_dir:
    env.PrependENVPath("PATH", picotool_dir)

# Let upload-port autodetection find the device in either firmware mode.
env.BoardConfig().update("build.hwids", [["0xCAFE", "0x1101"], ["0xCAFE", "0x1110"]])
