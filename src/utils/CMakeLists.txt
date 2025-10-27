set(project_name libutil)
project ("${project_name}")
add_definitions(-D_KRTL)

include_directories(${INJ_ROOT}/include ${RTL})
aux_source_directory(. SOURCES)
wdk_add_library(${project_name} ${SOURCES} ${RTL}/krtl/new.cc)
