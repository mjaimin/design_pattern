cmake_minimum_required(VERSION 3.20)
project(design_pattern LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

enable_testing()

# Every examples/NN-<pattern>/<name>.cpp becomes one executable and one ctest:
#   binary:  build/examples/NN-<pattern>/<name>
#   test:    NN-<pattern>/<name>
file(GLOB example_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS
     ${CMAKE_SOURCE_DIR}/examples/*)

foreach(dir ${example_dirs})
  if(NOT IS_DIRECTORY ${dir})
    continue()
  endif()
  get_filename_component(chapter ${dir} NAME)
  file(GLOB sources CONFIGURE_DEPENDS ${dir}/*.cpp)

  foreach(src ${sources})
    get_filename_component(name ${src} NAME_WE)
    set(target ${chapter}_${name})
    add_executable(${target} ${src})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/common)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    set_target_properties(${target} PROPERTIES
      OUTPUT_NAME ${name}
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/${chapter})
    add_test(NAME ${chapter}/${name} COMMAND ${target})
  endforeach()
endforeach()

# Capstone: one program built from several patterns.
add_executable(mini-gateway capstone/mini-gateway/main.cpp)
target_include_directories(mini-gateway PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_compile_options(mini-gateway PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(mini-gateway PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/capstone)
add_test(NAME capstone/mini-gateway COMMAND mini-gateway)
