cmake_minimum_required(VERSION 3.25)
project(ProjectGears2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project Gears 2 ships data (revisions/ -> xenon-analysis/) and hook
# sources; Xenon's game preparation compiles the hooks into the native
# module. This build only checks the data and, once hooks exist, unit-tests
# them against Xenon's CPU library.
option(GEARS2_BUILD_TESTS "Build Project Gears 2 tests" ON)
set(XENON_RECOMP_ROOT "" CACHE PATH "Path to the authoritative Xenon-Recomp source tree")

if(GEARS2_BUILD_TESTS)
  enable_testing()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME gears2_metadata_tests
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_metadata.py")
  # Fails while xenon-analysis/ differs from what revisions/ exports to.
  add_test(NAME gears2_xenon_analysis_tests
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/export_xenon_analysis.py" --check)

  # Hook unit tests link Xenon's CPU library; they need XENON_RECOMP_ROOT.
  file(GLOB GEARS2_HOOK_SOURCES CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/revisions/*/hooks/*.cpp")
  file(GLOB GEARS2_HOOK_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*_hook_tests.cpp")
  if(GEARS2_HOOK_SOURCES OR GEARS2_HOOK_TESTS)
    if(NOT XENON_RECOMP_ROOT OR NOT EXISTS "${XENON_RECOMP_ROOT}/CMakeLists.txt")
      message(FATAL_ERROR "Hook tests need XENON_RECOMP_ROOT set to the Xenon-Recomp source tree")
    endif()
    set(XENON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(XENON_BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
    add_subdirectory("${XENON_RECOMP_ROOT}" "${CMAKE_BINARY_DIR}/xenon-recomp" EXCLUDE_FROM_ALL)
    foreach(test_source ${GEARS2_HOOK_TESTS})
      get_filename_component(test_name "${test_source}" NAME_WE)
      add_executable(gears2_${test_name} "${test_source}" ${GEARS2_HOOK_SOURCES})
      target_link_libraries(gears2_${test_name} PRIVATE Xenon::CPU)
      # The tests check with assert(); keep them in every build type.
      target_compile_options(gears2_${test_name} PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
      add_test(NAME gears2_${test_name} COMMAND gears2_${test_name})
    endforeach()
  endif()
endif()
