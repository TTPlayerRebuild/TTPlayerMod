if(TTP_MOD_MODERN_ENGINE)
 include("${CMAKE_CURRENT_LIST_DIR}/modern_codec.cmake")
 return()
endif()
include(FetchContent)
FetchContent_Declare(modplug
 URL https://downloads.sourceforge.net/project/modplug-xmms/libmodplug/0.8.9.0/libmodplug-0.8.9.0.tar.gz
 URL_HASH SHA256=457ca5a6c179656d66c01505c0d95fafaead4329b9dbaa0f997d00a3508ad9de
 SOURCE_SUBDIR no-upstream-cmake)
FetchContent_MakeAvailable(modplug)
set(src "${modplug_SOURCE_DIR}/src")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/codec-generated")
file(READ "${src}/snd_fx.cpp" effects)
string(FIND "${effects}" "return (dwElapsedTime+500) / 1000;" milliseconds_at)
if(milliseconds_at LESS 0)
 message(FATAL_ERROR "Pinned ModPlug millisecond compatibility patch no longer matches")
endif()
# Original 604562bd returns the accumulated millisecond value. The public C
# wrapper rounds this to seconds; do not use that wrapper for the player ABI.
string(REPLACE "return (dwElapsedTime+500) / 1000;" "return dwElapsedTime;" effects "${effects}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/snd_fx.cpp" "${effects}")
file(READ "${src}/sndfile.cpp" loader)
# Restrict dispatch to the eight advertised module/container types. In
# particular no MIDI/PAT loader may open external sound-bank files.
foreach(format Wav ABC MID PAT Med MDL DBM 669 AMS OKT PTM Ult DMF DSM AMF PSM MT2)
 string(REPLACE " && (!Read${format}(lpStream, dwMemLength))" "" loader "${loader}")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/sndfile.cpp" "${loader}")
file(GLOB core_sources CONFIGURE_DEPENDS "${src}/*.cpp")
list(REMOVE_ITEM core_sources "${src}/snd_fx.cpp" "${src}/sndfile.cpp" "${src}/modplug.cpp")
add_library(codec STATIC ${core_sources} "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/snd_fx.cpp" "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/sndfile.cpp")
target_include_directories(codec PUBLIC "${src}" "${src}/libmodplug")
target_compile_definitions(codec PUBLIC MODPLUG_STATIC PRIVATE MODPLUG_BUILD MSC_VER=_MSC_VER _CRT_SECURE_NO_WARNINGS)
target_compile_options(codec PRIVATE /Os /Gy /Gw /GF /EHsc /fp:precise /arch:SSE2 /w)
target_link_libraries(codec PUBLIC winmm)
