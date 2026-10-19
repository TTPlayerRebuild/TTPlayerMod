include(FetchContent)
FetchContent_Declare(openmpt
 URL https://lib.openmpt.org/files/libopenmpt/src/libopenmpt-0.8.9+release.autotools.tar.gz
 URL_HASH SHA256=d7ce84fd05d686c4bcf66af40eae857afa371442db60eeda3f874bd6cf6fc318
 SOURCE_SUBDIR no-upstream-cmake)
FetchContent_MakeAvailable(openmpt)
# The official source distribution ships Autotools, not CMake. Use its exact
# library source manifest, excluding command-line programs and tests.
file(STRINGS "${openmpt_SOURCE_DIR}/Makefile.am" source_lines
 REGEX "^(MPT_FILES_(SRC_MPT|SRC_OPENMPT|COMMON|SOUNDBASE|SOUNDLIB|SOUNDDSP)|libopenmpt_la_SOURCES) \\+= [^$]+\\.cpp$")
set(codec_sources)
foreach(line IN LISTS source_lines)
 string(REGEX REPLACE "^[^+]+\\+= " "" relative "${line}")
 list(APPEND codec_sources "${openmpt_SOURCE_DIR}/${relative}")
endforeach()
if(NOT codec_sources)
 message(FATAL_ERROR "The pinned libopenmpt source manifest could not be read")
endif()
add_library(codec STATIC ${codec_sources})
target_include_directories(codec PUBLIC "${openmpt_SOURCE_DIR}"
 PRIVATE "${openmpt_SOURCE_DIR}/src" "${openmpt_SOURCE_DIR}/common")
target_compile_definitions(codec PUBLIC LIBOPENMPT_USE_STATIC
 PRIVATE LIBOPENMPT_BUILD MPT_BUILD_NO_DATE MPT_BUILD_NO_TIME
 UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0501 WINVER=0x0501
 _CRT_SECURE_NO_WARNINGS)
target_compile_features(codec PRIVATE cxx_std_17)
target_compile_options(codec PRIVATE /Os /Gy /Gw /GF /EHsc /GR /utf-8 /fp:precise /arch:SSE2 /Zc:preprocessor /MP4)
target_link_libraries(codec PUBLIC ole32 uuid)
