cmake_minimum_required (VERSION 3.20)
project (InjDemo LANGUAGES C CXX)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x64")
else()
    set(ARCH "x86")
endif()

set(BIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/../output/bin/${ARCH})
set(LIB_OUTPUT_DIR ${CMAKE_BINARY_DIR}/../output/lib/${ARCH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR})
link_directories(${LIB_OUTPUT_DIR})

set(INJ_ROOT ${CMAKE_SOURCE_DIR})
set(RTL ${CMAKE_SOURCE_DIR})

include(${RTL}/krtl/wdk.cmake)
wdk_clean_default_options()
add_subdirectory(src/utils libutil)
wdk_restore_default_options()

