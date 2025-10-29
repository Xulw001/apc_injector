set(project_name injdrv)
project ("${project_name}")

set(DLL32 "${TARGET_DIRECTORY}\\bin\\x86\\Demo.dll")
set(DLL64 "${TARGET_DIRECTORY}\\bin\\x64\\Demo.dll")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/inject_define.h.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/inject_define.h)

include_directories(${INJ_ROOT}/include ${RTL})
aux_source_directory(. SOURCES)
wdk_add_driver(${project_name} ${SOURCES})
target_link_libraries(${project_name} PRIVATE libutil)