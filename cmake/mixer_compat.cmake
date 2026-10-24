# Fail closed if the pinned upstream layout changes. Replacement ranges are
# complete macro definitions, not sample-specific playback adjustments.
function(mod_replace_range variable begin end replacement)
 set(value "${${variable}}")
 string(FIND "${value}" "${begin}" first)
 string(FIND "${value}" "${end}" last)
 if(first LESS 0 OR last LESS_EQUAL first)
  message(FATAL_ERROR "Pinned ModPlug mixer patch no longer matches: ${begin}")
 endif()
 string(SUBSTRING "${value}" 0 ${first} prefix)
 string(SUBSTRING "${value}" ${last} -1 suffix)
 set(${variable} "${prefix}${replacement}\n${suffix}" PARENT_SCOPE)
endfunction()

file(READ "${src}/fastmix.cpp" mixer)
mod_replace_range(mixer "// quantizer scale of window coefs" "// ----------------------------------------------------------------------------\n// MIXING MACROS" "")
mod_replace_range(mixer "#define SNDMIX_GETMONOVOL8FIRFILTER" "/////////////////////////////////////////////////////////////////////////////\n// Stereo" [=[
#define SNDMIX_GETMONOVOL8FIRFILTER \
    int vol = ttp_mod_legacy::Fir(p, nPos, pChn->nInc);
#define SNDMIX_GETMONOVOL16FIRFILTER \
    int vol = ttp_mod_legacy::Fir(p, nPos, pChn->nInc);
]=])
mod_replace_range(mixer "#define SNDMIX_GETSTEREOVOL8FIRFILTER" "#define SNDMIX_STOREMONOVOL" [=[
// Original FIR dispatch uses the 256-phase cubic path for stereo samples.
#define SNDMIX_GETSTEREOVOL8FIRFILTER \
    int vol_l = ttp_mod_legacy::StereoSpline(p, nPos); \
    int vol_r = ttp_mod_legacy::StereoSpline(p + 1, nPos);
#define SNDMIX_GETSTEREOVOL16FIRFILTER \
    int vol_l = ttp_mod_legacy::StereoSpline(p, nPos); \
    int vol_r = ttp_mod_legacy::StereoSpline(p + 1, nPos);
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/fastmix.cpp" "#include \"legacy_mixer.h\"\n${mixer}")

file(READ "${src}/sndmix.cpp" mixing)
string(FIND "${mixing}" "pCvt = X86_Convert32To16;" converter_at)
if(converter_at LESS 0)
 message(FATAL_ERROR "Pinned ModPlug PCM16 converter patch no longer matches")
endif()
string(REPLACE "pCvt = X86_Convert32To16;" "pCvt = ttp_mod_legacy::Convert16;" mixing "${mixing}")
set(conversion "lpBuffer += pCvt(lpBuffer, MixSoundBuffer, lTotalSampleCount, &nVUMeterMin, &nVUMeterMax);")
string(FIND "${mixing}" "${conversion}" conversion_at)
if(conversion_at LESS 0)
 message(FATAL_ERROR "Pinned ModPlug dither compatibility patch no longer matches")
endif()
string(REPLACE "${conversion}" "if (gnBitsPerSample <= 16 && (gdwSoundSetup & SNDMIX_HQRESAMPLER) &&\n            (gnCPUUsage < 25 || (gdwSoundSetup & SNDMIX_DIRECTTODISK)))\n            ttp_mod_legacy::Dither(MixSoundBuffer, lTotalSampleCount, gnBitsPerSample);\n        ${conversion}" mixing "${mixing}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/codec-generated/sndmix.cpp" "#include \"legacy_mixer.h\"\n${mixing}")
