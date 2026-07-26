cmake_minimum_required(VERSION 3.24)

project(
  brownout-replay-lab
  VERSION 0.1.0
  DESCRIPTION "Deterministic NOR flash power-loss replay laboratory"
  LANGUAGES CXX
)

option(BRLAB_BUILD_TESTS "Build Brownout Replay Lab tests" ON)
option(BRLAB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/Warnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(brlab_core STATIC src/core/types.cpp src/core/step_runner.cpp)
add_library(brlab::core ALIAS brlab_core)
target_include_directories(brlab_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
brlab_enable_warnings(brlab_core)
brlab_enable_sanitizers(brlab_core)

add_library(brlab_power INTERFACE)
add_library(brlab::power ALIAS brlab_power)
target_link_libraries(brlab_power INTERFACE brlab_core)

add_library(brlab_flash STATIC src/flash/byte_flash.cpp)
add_library(brlab::flash ALIAS brlab_flash)
target_link_libraries(brlab_flash PUBLIC brlab_core)
brlab_enable_warnings(brlab_flash)
brlab_enable_sanitizers(brlab_flash)

add_library(brlab_persistence STATIC src/persistence/record_codec.cpp)
add_library(brlab::persistence ALIAS brlab_persistence)
target_link_libraries(brlab_persistence PUBLIC brlab_core brlab_flash)
brlab_enable_warnings(brlab_persistence)
brlab_enable_sanitizers(brlab_persistence)

add_library(brlab_faults INTERFACE)
add_library(brlab::faults ALIAS brlab_faults)
target_link_libraries(brlab_faults INTERFACE brlab_core brlab_persistence)

add_library(brlab_oracle INTERFACE)
add_library(brlab::oracle ALIAS brlab_oracle)
target_link_libraries(brlab_oracle INTERFACE brlab_core brlab_persistence)

add_executable(brlab app/brlab.cpp)
target_link_libraries(brlab PRIVATE brlab_persistence)
brlab_enable_warnings(brlab)
brlab_enable_sanitizers(brlab)

if(BRLAB_BUILD_TESTS)
  include(CTest)
  add_executable(
    brlab_tests
    tests/test_main.cpp
    tests/unit/smoke_test.cpp
    tests/unit/core_tests.cpp
    tests/unit/flash_tests.cpp
    tests/unit/record_codec_tests.cpp
    tests/integration/torn_write_recovery_test.cpp
  )
  target_include_directories(brlab_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(brlab_tests PRIVATE brlab_persistence)
  brlab_enable_warnings(brlab_tests)
  brlab_enable_sanitizers(brlab_tests)
  add_test(NAME brlab_tests COMMAND brlab_tests)
endif()
