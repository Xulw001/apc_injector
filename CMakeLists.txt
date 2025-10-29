cmake_minimum_required (VERSION 3.20)
project (InjDemo LANGUAGES C CXX)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH "x64")
else()
    set(ARCH "x86")
endif()

if(NOT TARGET_DIRECTORY)
    set(TARGET_DIRECTORY [==[C:\Inject]==])
endif()

# Set Target Output Path
set(BIN_OUTPUT_DIR ${TARGET_DIRECTORY}\\bin\\${ARCH})
set(LIB_OUTPUT_DIR ${TARGET_DIRECTORY}\\lib\\${ARCH})

# Set Default Target Output Path
foreach(config DEBUG RELEASE RELWITHDEBINFO)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} ${BIN_OUTPUT_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} ${LIB_OUTPUT_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config} ${LIB_OUTPUT_DIR})
endforeach()

link_directories(${LIB_OUTPUT_DIR})

set(INJ_ROOT ${CMAKE_SOURCE_DIR})
set(RTL ${CMAKE_SOURCE_DIR})

include(${RTL}/krtl/wdk.cmake)
wdk_clean_default_options()
add_subdirectory(src/utils libutil)
add_subdirectory(src/inj injdrv)
wdk_restore_default_options()

add_subdirectory(src/dll)
add_subdirectory(src/cli)

configure_file("deployment.bat" "${TARGET_DIRECTORY}\\deployment.bat")
