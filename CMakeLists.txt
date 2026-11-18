cmake_minimum_required(VERSION 3.10)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SKRED_VERSION)
string(STRIP "${SKRED_VERSION}" SKRED_VERSION)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" SKRED_VERSION_MATCH "${SKRED_VERSION}")
if(NOT SKRED_VERSION_MATCH)
    message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH, got '${SKRED_VERSION}'")
endif()
set(SKRED_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(SKRED_VERSION_MINOR "${CMAKE_MATCH_2}")
set(SKRED_VERSION_PATCH "${CMAKE_MATCH_3}")

set(PULP_VERSION "${SKRED_VERSION}")
set(PULP_VERSION_MAJOR "${SKRED_VERSION_MAJOR}")
set(PULP_VERSION_MINOR "${SKRED_VERSION_MINOR}")
set(PULP_VERSION_PATCH "${SKRED_VERSION_PATCH}")

project(skred VERSION "${SKRED_VERSION}" LANGUAGES C)
include(GNUInstallDirs)

option(SKRED_KSYNTH "Enable KSynth integration" ON)
option(SKRED_SCOPE "Enable SCOPE IPC" ON)
option(SKRED_UDP "Enable UDP networking" ON)

if(SKRED_KSYNTH)
    add_compile_definitions(KSYNTH=1)
endif()
if(SKRED_SCOPE)
    add_compile_definitions(SCOPE=1)
endif()
if(SKRED_UDP)
    add_compile_definitions(UDP=1)
endif()

if(SKRED_SCOPE AND WIN32)
    message(FATAL_ERROR "SCOPE=1 currently requires POSIX shared memory")
endif()

set(SKRED_VERSION_H "${CMAKE_CURRENT_BINARY_DIR}/skred-version.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/skred-version.h.in"
    "${SKRED_VERSION_H}"
    @ONLY
)

set(API_SOURCES
    src/ands.c
    src/miniaudio.c
    src/midi.c
    src/util.c
    src/exp-vfs/miniz.c
    src/exp-vfs/miniz_tdef.c
    src/exp-vfs/miniz_tinfl.c
    src/exp-vfs/miniz_zip.c
    src/exp-vfs/skred_vfs.c
    src/skode-event.c
    src/skode-dict.c
    src/gen/skred_opcodes_gen.c
    src/skqueue.c
    src/udp.c
    src/udp-events.c
    src/api.c
    src/synth.c
    src/synth-delay.c
    src/synth-osc.c
    src/synth-filter.c
    src/synth-env.c
    src/synth-voice.c
    src/synth-wave.c
    src/skode.c
    src/skode-session.c
    src/skode-ksynth-bridge.c
    src/skode-loader.c
    src/skode-opcode-exec.c
    src/skode-display-wave.c
    src/skode-diagnostics.c
    src/skode-audio-export.c
    src/skode-words-dsp.c
    src/skode-words-seq.c
    src/skode-words-data.c
    src/skode-words-system.c
    src/polyphony.c
    src/synth-alloc.c
    src/seq.c
    src/recorder.c
)

if(SKRED_SCOPE)
    list(APPEND API_SOURCES src/scope-ipc.c)
endif()

if(SKRED_KSYNTH)
    list(APPEND API_SOURCES src/vendor/ksynth/ksynth.c)
endif()

# Static library
add_library(api STATIC ${API_SOURCES})
set_target_properties(api PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shared library
add_library(api_shared SHARED ${API_SOURCES})
set_target_properties(api_shared PROPERTIES
    OUTPUT_NAME api
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

foreach(tgt api api_shared)
    target_include_directories(${tgt} PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/exp-vfs"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
    )
    target_compile_definitions(${tgt} PUBLIC SKRED_MIDI_ENABLED=1)
    if(EMSCRIPTEN)
        target_link_libraries(${tgt} PUBLIC "-sASYNCIFY")
    elseif(APPLE)
        find_library(COREMIDI_FRAMEWORK CoreMIDI REQUIRED)
        find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
        target_link_libraries(${tgt} PUBLIC ${COREMIDI_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
    elseif(UNIX)
        find_package(ALSA REQUIRED)
        target_include_directories(${tgt} PRIVATE ${ALSA_INCLUDE_DIRS})
        target_link_libraries(${tgt} PUBLIC ${ALSA_LIBRARIES} m pthread dl)
    endif()
endforeach()

# sk3 CLI and legacy compatibility aliases
add_executable(sk3 src/main.c)
target_link_libraries(sk3 PRIVATE api)
target_include_directories(sk3 PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
)

add_executable(mini-skred src/main.c)
target_link_libraries(mini-skred PRIVATE api)
target_include_directories(mini-skred PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
)

add_executable(skred src/main.c)
target_link_libraries(skred PRIVATE api)
target_include_directories(skred PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
)

if(SKRED_SCOPE)
    add_executable(scope_reader src/scope-reader.c)
    target_link_libraries(scope_reader PRIVATE api)
    target_include_directories(scope_reader PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gen"
    )
endif()

# Installation headers
install(FILES
    src/api.h
    src/polyphony.h
    src/synth-config.h
    src/synth-types.h
    src/portable_atomic.h
    src/skode.h
    src/ands.h
    src/seq.h
    src/exp-vfs/skred_vfs.h
    "${SKRED_VERSION_H}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skred"
)

install(TARGETS api api_shared sk3 mini-skred skred
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Test Suite
enable_testing()

add_executable(ands_parser_tests tests/ands_parser_tests.c)
target_link_libraries(ands_parser_tests PRIVATE api)
target_include_directories(ands_parser_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME ands.parser-contract COMMAND ands_parser_tests)

add_executable(skode_state_tests tests/skode_state_tests.c)
target_link_libraries(skode_state_tests PRIVATE api)
target_include_directories(skode_state_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
target_compile_definitions(skode_state_tests PRIVATE
    SKRED_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    SKRED_TEST_TRACKS=1
    SKRED_TEST_RECORD_SCOPE=1
    SKRED_TEST_PD=1
    SKRED_TEST_FM=1
    SKRED_TEST_KSYNTH=1
    SKRED_TEST_MIDI=1
)
add_test(NAME skode.command-state COMMAND skode_state_tests)
add_test(NAME skode.session-archive COMMAND skode_state_tests --session-only)

add_executable(skqueue_tests tests/skqueue_tests.c)
target_link_libraries(skqueue_tests PRIVATE api)
target_include_directories(skqueue_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME skqueue.concurrent-publication COMMAND skqueue_tests)

add_executable(audio_command_tests tests/audio_command_tests.c)
target_link_libraries(audio_command_tests PRIVATE api)
target_include_directories(audio_command_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME audio.command-routing COMMAND audio_command_tests)

add_executable(hard_sync_tests tests/hard_sync_tests.c)
target_link_libraries(hard_sync_tests PRIVATE api)
target_include_directories(hard_sync_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME synth.hard-sync COMMAND hard_sync_tests)

add_executable(midi_tests tests/midi_tests.c)
target_link_libraries(midi_tests PRIVATE api)
target_include_directories(midi_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME midi.control-event-bridge COMMAND midi_tests)

add_executable(polyphony_tests tests/polyphony_tests.c)
target_link_libraries(polyphony_tests PRIVATE api)
target_include_directories(polyphony_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
target_compile_definitions(polyphony_tests PRIVATE SKRED_TEST_TRACKS=1)
add_test(NAME polyphony.groups-pools-and-graphs COMMAND polyphony_tests)

add_executable(mksynth_equivalence_tests src/gen/test_synth_equivalence_gen.c)
target_link_libraries(mksynth_equivalence_tests PRIVATE api)
target_include_directories(mksynth_equivalence_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME synth.mksynth-equivalence COMMAND mksynth_equivalence_tests)

add_executable(ksynth_bridge_tests tests/ksynth_bridge_tests.c)
target_link_libraries(ksynth_bridge_tests PRIVATE api)
target_include_directories(ksynth_bridge_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
target_compile_definitions(ksynth_bridge_tests PRIVATE SKRED_TEST_KSYNTH=1)
add_test(NAME ksynth.array-bridge COMMAND ksynth_bridge_tests)

add_executable(recording_tests tests/recording_tests.c)
target_link_libraries(recording_tests PRIVATE api)
target_include_directories(recording_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
add_test(NAME recording.track-routing COMMAND recording_tests)

if(SKRED_SCOPE)
    add_executable(scope_ipc_tests tests/scope_ipc_tests.c)
    target_link_libraries(scope_ipc_tests PRIVATE api)
    target_include_directories(scope_ipc_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
    add_test(NAME scope.shared-memory-publication COMMAND scope_ipc_tests)
endif()

add_executable(render_equivalence_probe EXCLUDE_FROM_ALL tests/render_equivalence_probe.c)
target_link_libraries(render_equivalence_probe PRIVATE api)
target_include_directories(render_equivalence_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")

add_executable(synth_callback_bench EXCLUDE_FROM_ALL tests/synth_callback_bench.c)
target_link_libraries(synth_callback_bench PRIVATE api)
target_include_directories(synth_callback_bench PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/src/gen")
target_compile_definitions(synth_callback_bench PRIVATE SKRED_BENCH_PD=1 SKRED_BENCH_FM=1)

set_source_files_properties(
    src/skode.c
    src/skode-words-dsp.c
    src/skode-words-data.c
    src/skode-words-seq.c
    src/skode-words-system.c
    PROPERTIES COMPILE_FLAGS "-Wno-multichar"
)
