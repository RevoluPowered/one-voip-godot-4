#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

sources = []


# Opus (REQUIRES PRECOMPILE)

env.Append(CPPPATH=["thirdparty/opus/include"], LIBS=["thirdparty/opus/build/Release/opus.lib"])
env['LINKFLAGS'] = ['/WX:NO']


# Opus Tools (speex resampler)

env.Append(CPPPATH=["thirdparty/opus-tools/src"], CPPDEFINES={"OUTSIDE_SPEEX": None, "FLOATING_POINT": None, "RANDOM_PREFIX": "X3_EPIC_VOIP_PLUGIN"})
sources += ["thirdparty/opus-tools/src/resample.c"]


# OneVOIP Extension

env.Append(CPPPATH=["include/"])
env['CCPDBFLAGS'] = '/Zi /Fd${TARGET}.pdb'
# env.Append(CPPDEFINES={"NDEBUG": None}) # For release builds
sources += Glob("src/*.cpp")

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "demo_rtc/bin/libonevoip.{}.{}.framework/libonevoip.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "demo_rtc/bin/libonevoip{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
