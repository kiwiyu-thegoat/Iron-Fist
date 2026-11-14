# Tekken 6 (ULUS-10466): first stage of the profile.
#
# This builds only the generated AOT corpus, as a check that the recompiled
# code compiles against the runtime. The host (kernel, graphics, audio, input)
# is not written yet; see README.md.

set(TEKKEN6_PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
file(GLOB TEKKEN6_GENERATED CONFIGURE_DEPENDS "${TEKKEN6_PROFILE_DIR}/generated/*.cpp")
list(LENGTH TEKKEN6_GENERATED TEKKEN6_GENERATED_COUNT)

if(TEKKEN6_GENERATED_COUNT EQUAL 0)
    message(STATUS "tekken6: no generated AOT units; run profiles/tekken6/scripts/generate.sh")
else()
    message(STATUS "tekken6: ${TEKKEN6_GENERATED_COUNT} generated AOT units")
    add_library(tekken6_generated OBJECT ${TEKKEN6_GENERATED})
    target_include_directories(tekken6_generated PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${TEKKEN6_PROFILE_DIR}/generated")
endif()
