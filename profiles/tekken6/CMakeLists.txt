# Tekken 6 (ULUS-10466).
#
# tekken6_generated  the recompiled game, as a check that it compiles
# tekken6_headless   a bring-up host with no window, audio or input: the shared
#                    mhp3rd kernel and firmware code plus Tekken's own calls.
#                    It runs the game far enough to show what it asks of the
#                    host. Graphics, audio and input are not written yet.

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

    # The kernel and the thread/memory/file/system firmware calls are shared
    # with the mhp3rd profile rather than copied; host/shims.cpp answers "off"
    # for the mhp3rd features Tekken does not have (mods, translation, fast
    # loading, overlays, the settings file).
    set(MHP3RD_HOST "${CMAKE_SOURCE_DIR}/profiles/mhp3rd/host")
    add_executable(tekken6_headless
        host/main.cpp
        host/tekken6_profile.cpp
        host/hle_tekken.cpp
        host/hle_tekken_system.cpp
        host/shims.cpp
        "${MHP3RD_HOST}/kernel/kernel.cpp"
        "${MHP3RD_HOST}/kernel/load_trace.cpp"
        "${MHP3RD_HOST}/kernel/iso_image.cpp"
        "${MHP3RD_HOST}/hle/hle_common.cpp"
        "${MHP3RD_HOST}/hle/hle_threadman.cpp"
        "${MHP3RD_HOST}/hle/hle_sysmem.cpp"
        "${MHP3RD_HOST}/hle/hle_io.cpp"
        "${MHP3RD_HOST}/hle/hle_system.cpp"
        "${MHP3RD_HOST}/platform/utf8_path.cpp"
        "${MHP3RD_HOST}/perf/frame_stats.cpp"
        $<TARGET_OBJECTS:tekken6_generated>)
    target_include_directories(tekken6_headless PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${MHP3RD_HOST}"
        "${CMAKE_CURRENT_SOURCE_DIR}/host")
    target_compile_definitions(tekken6_headless PRIVATE
        PSPRECOMP_DEFAULT_NIDS_CSV="${CMAKE_SOURCE_DIR}/configs/nids.csv")
    target_link_libraries(tekken6_headless PRIVATE psprecomp_core Threads::Threads ${CMAKE_DL_LIBS})
endif()
