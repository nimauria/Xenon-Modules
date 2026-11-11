cmake_minimum_required(VERSION 3.25)
project(ProjectGracemeria VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRACEMERIA_BUILD_TESTS "Build Project Gracemeria tests" ON)
set(XENON_RECOMP_ROOT "" CACHE PATH "Path to the authoritative Xenon-Recomp source tree")
if(NOT XENON_RECOMP_ROOT OR NOT EXISTS "${XENON_RECOMP_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set XENON_RECOMP_ROOT to the current Xenon-Recomp source tree")
endif()

set(XENON_DEPENDENCY_MODE AUTO CACHE STRING "" FORCE)
set(XENON_AUTO_BOOTSTRAP_DEPS ON CACHE BOOL "" FORCE)
set(XENON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(XENON_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(XENON_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
# NOTE: was forced OFF for a faster tooling-only build. session.cpp's GPU
# pump thread integration (Xenon-Recomp commits c281ed9/802b167/bfd3c09/
# 65b610e) now references xenon::gpu::GraphicsSystem unconditionally, with no
# XENON_ENABLE_GRAPHICS=OFF compile guard, so xenon_core no longer links
# without it - forcing it OFF here breaks gracemeria-import-coverage's link
# with unresolved GraphicsSystem externals. Forced ON until Xenon-Recomp adds
# real graphics-optional compile guards to session.cpp.
set(XENON_ENABLE_GRAPHICS ON CACHE BOOL "" FORCE)
set(XENON_ENABLE_INPUT ON CACHE BOOL "" FORCE)
# Was forced OFF to avoid the SDL2/FFmpeg dependency chain for a tooling-only
# build; switched to AUTO dependency mode (reusing the same managed SDL2/
# bundled-FFmpeg Xenon-Recomp's own gen10 build already resolved under
# Xenon-Recomp/.xenon/deps) so gracemeria-import-coverage registers the real
# XAudio*/XMA* exports instead of reporting every audio ordinal "missing"
# purely because this tool's own build never requested an audio backend to
# check them against.
set(XENON_ENABLE_AUDIO ON CACHE BOOL "" FORCE)
add_subdirectory("${XENON_RECOMP_ROOT}" "${CMAKE_BINARY_DIR}/xenon-recomp" EXCLUDE_FROM_ALL)

add_library(gracemeria_analysis STATIC
  src/analysis/hint_provider.cpp
  src/analysis/ac6_migration.cpp
)
target_include_directories(gracemeria_analysis PUBLIC include)
target_link_libraries(gracemeria_analysis PUBLIC Xenon::Recomp)
# Default source for Ac6HintProvider::default_revisions_root() (see
# include/gracemeria/hint_provider.hpp) - this source tree's own revisions/
# directory. Packaging Gracemeria as an installed module later replaces this
# compiled-in default with the installed package root; explicit constructor
# callers (tests) are unaffected either way.
target_compile_definitions(gracemeria_analysis PRIVATE
  GRACEMERIA_REVISIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/revisions")

add_executable(gracemeria-recomp src/tools/gracemeria_recomp.cpp)
target_link_libraries(gracemeria-recomp PRIVATE gracemeria_analysis Xenon::Recomp)

# Analysis-only tool (no code generation): always emits the JSON analysis
# report, even when entry-integrity would later fail generate_project() -
# feeds tools/reduce_legacy_hints.py so missing legacy function starts can be
# found in one batch rather than one entry-integrity failure at a time.
add_executable(gracemeria-analyze src/tools/gracemeria_analyze.cpp)
target_link_libraries(gracemeria-analyze PRIVATE gracemeria_analysis Xenon::Recomp)

# Title-specific AC6_recomp -> Xenon Analysis Hint Schema V2 migration tool
# (development tooling only; Xenon's own runtime/driver never reads ReXGlue
# TOML - see include/gracemeria/ac6_migration.hpp).
add_executable(migrate-ac6-recomp src/tools/migrate_ac6_recomp.cpp)
target_link_libraries(migrate-ac6-recomp PRIVATE gracemeria_analysis Xenon::Recomp)

# Import coverage intentionally uses the real XenonSession/ExportRegistry so
# its registered/stub/missing result follows the current runtime rather than a
# duplicated Gracemeria list.
add_executable(gracemeria-import-coverage src/tools/import_coverage.cpp)
target_link_libraries(gracemeria-import-coverage PRIVATE Xenon::Core)

# The Xenon module hint package (xenon-analysis/) the launcher's automatic
# preparation builds the native module from, one revision per disc. Written
# from revisions/ through Ac6HintProvider; `cmake --build . --target
# gracemeria-xenon-analysis` refreshes it, and gracemeria_xenon_analysis_tests
# fails while it is stale.
add_executable(gracemeria-export-analysis src/tools/export_xenon_analysis.cpp)
target_link_libraries(gracemeria-export-analysis PRIVATE gracemeria_analysis Xenon::Recomp Xenon::Graphics)
target_compile_definitions(gracemeria-export-analysis PRIVATE
  GRACEMERIA_REVISIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/revisions")
add_custom_target(gracemeria-xenon-analysis
  COMMAND gracemeria-export-analysis "${CMAKE_CURRENT_SOURCE_DIR}/xenon-analysis"
  DEPENDS gracemeria-export-analysis
  COMMENT "Exporting the Xenon module hint package to xenon-analysis/"
  VERBATIM)

if(GRACEMERIA_BUILD_TESTS)
  enable_testing()
  add_executable(gracemeria_hint_provider_tests tests/hint_provider_tests.cpp)
  target_link_libraries(gracemeria_hint_provider_tests PRIVATE gracemeria_analysis)
  target_compile_definitions(gracemeria_hint_provider_tests PRIVATE
    GRACEMERIA_REVISIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/revisions")
  add_test(NAME gracemeria_hint_provider_tests COMMAND gracemeria_hint_provider_tests)

  add_executable(gracemeria_ac6_migration_tests tests/ac6_migration_tests.cpp)
  target_link_libraries(gracemeria_ac6_migration_tests PRIVATE gracemeria_analysis)
  add_test(NAME gracemeria_ac6_migration_tests COMMAND gracemeria_ac6_migration_tests)

  add_library(gracemeria_abi_fixture SHARED tests/abi_fixture.cpp)
  target_include_directories(gracemeria_abi_fixture PRIVATE "${XENON_RECOMP_ROOT}/include")
  set_target_properties(gracemeria_abi_fixture PROPERTIES PREFIX "")

  add_executable(gracemeria_module_abi_tests tests/module_abi_tests.cpp)
  target_link_libraries(gracemeria_module_abi_tests PRIVATE Xenon::CPU ${CMAKE_DL_LIBS})
  add_dependencies(gracemeria_module_abi_tests gracemeria_abi_fixture)
  add_test(NAME gracemeria_module_abi_tests
           COMMAND gracemeria_module_abi_tests $<TARGET_FILE:gracemeria_abi_fixture>)

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME gracemeria_metadata_tests
           COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_metadata.py")
  add_test(NAME gracemeria_xenon_analysis_tests
           COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_xenon_analysis.py"
                   $<TARGET_FILE:gracemeria-export-analysis>)
  add_test(NAME gracemeria_rendering_export_tests
           COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_rendering_export.py"
                   $<TARGET_FILE:gracemeria-export-analysis>)

  # The US revision's host-timing hooks, run against a FlatMemory guest.
  set(GRACEMERIA_US_HOOKS "${CMAKE_CURRENT_SOURCE_DIR}/revisions/usa-retail-tu0/hooks")
  add_executable(gracemeria_hook_tests tests/hook_tests.cpp
    "${GRACEMERIA_US_HOOKS}/ac6_effect_mode_hooks.cpp"
    "${GRACEMERIA_US_HOOKS}/ac6_cutscene_resync_hooks.cpp"
    "${GRACEMERIA_US_HOOKS}/ac6_guest_call.cpp")
  target_link_libraries(gracemeria_hook_tests PRIVATE Xenon::CPU)
  add_test(NAME gracemeria_hook_tests COMMAND gracemeria_hook_tests)

  # The tests check with assert(), often around the call under test, so they
  # keep their asserts in every build type: Release and RelWithDebInfo define
  # NDEBUG, which would compile the checks - and those calls - away.
  foreach(test_target gracemeria_hint_provider_tests gracemeria_ac6_migration_tests
                      gracemeria_module_abi_tests gracemeria_hook_tests)
    target_compile_options(${test_target} PRIVATE
      $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
  endforeach()
endif()
